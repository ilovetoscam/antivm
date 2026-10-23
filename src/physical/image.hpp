#ifndef VM_IMAGE_HPP
#define VM_IMAGE_HPP

#ifdef _WIN32
#include <windows.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

struct vm_image {
    const uint8_t *data;
    size_t size;
    IMAGE_OPTIONAL_HEADER64 optional = {};
    size_t section_offset = 0;
    unsigned section_count = 0;

    bool contains(size_t offset, size_t length) const {
        return offset <= size && length <= size - offset;
    }

    bool parse() {
        if (data == NULL || !contains(0, sizeof(IMAGE_DOS_HEADER))) return false;
        IMAGE_DOS_HEADER dos;
        memcpy(&dos, data, sizeof(dos));
        if (dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew < 0) return false;
        size_t offset = (size_t) dos.e_lfanew;
        if (!contains(offset, sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER))) return false;
        DWORD signature;
        IMAGE_FILE_HEADER header;
        memcpy(&signature, data + offset, sizeof(signature));
        memcpy(&header, data + offset + sizeof(signature), sizeof(header));
        if (signature != IMAGE_NT_SIGNATURE || header.Machine != IMAGE_FILE_MACHINE_AMD64
                || header.NumberOfSections == 0 || header.NumberOfSections > 96) return false;
        offset += sizeof(signature) + sizeof(header);
        if (header.SizeOfOptionalHeader < sizeof(optional)
                || !contains(offset, header.SizeOfOptionalHeader)) return false;
        memcpy(&optional, data + offset, sizeof(optional));
        if (optional.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC
                || optional.NumberOfRvaAndSizes <= IMAGE_DIRECTORY_ENTRY_EXPORT) return false;
        section_offset = offset + header.SizeOfOptionalHeader;
        section_count = header.NumberOfSections;
        return contains(section_offset, section_count * sizeof(IMAGE_SECTION_HEADER));
    }

    const uint8_t *rva(DWORD address, size_t length) const {
        if (address < optional.SizeOfHeaders) {
            if (length <= optional.SizeOfHeaders - address && contains(address, length)) return data + address;
            return NULL;
        }
        for (unsigned i = 0; i < section_count; i++) {
            IMAGE_SECTION_HEADER section;
            memcpy(&section, data + section_offset + i * sizeof(section), sizeof(section));
            if (address < section.VirtualAddress) continue;
            DWORD delta = address - section.VirtualAddress;
            if (delta >= section.SizeOfRawData || length > section.SizeOfRawData - delta) continue;
            if (section.PointerToRawData > size || delta > size - section.PointerToRawData) return NULL;
            size_t offset = (size_t) section.PointerToRawData + delta;
            return contains(offset, length) ? data + offset : NULL;
        }
        return NULL;
    }

    const uint8_t *find_export(const char *wanted, size_t length) const {
        IMAGE_DATA_DIRECTORY directory = optional.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
        if (directory.Size < sizeof(IMAGE_EXPORT_DIRECTORY)) return NULL;
        const uint8_t *entry = rva(directory.VirtualAddress, sizeof(IMAGE_EXPORT_DIRECTORY));
        if (entry == NULL) return NULL;
        IMAGE_EXPORT_DIRECTORY exports;
        memcpy(&exports, entry, sizeof(exports));
        if (exports.NumberOfNames > 1048576 || exports.NumberOfFunctions > 1048576) return NULL;
        const uint8_t *names = rva(exports.AddressOfNames, exports.NumberOfNames * sizeof(DWORD));
        const uint8_t *ordinals = rva(exports.AddressOfNameOrdinals, exports.NumberOfNames * sizeof(WORD));
        const uint8_t *functions = rva(exports.AddressOfFunctions, exports.NumberOfFunctions * sizeof(DWORD));
        if (names == NULL || ordinals == NULL || functions == NULL) return NULL;
        size_t name_length = strlen(wanted) + 1;
        for (DWORD i = 0; i < exports.NumberOfNames; i++) {
            DWORD name_address;
            memcpy(&name_address, names + i * sizeof(DWORD), sizeof(name_address));
            const uint8_t *name = rva(name_address, name_length);
            if (name == NULL || memcmp(name, wanted, name_length) != 0) continue;
            WORD ordinal;
            memcpy(&ordinal, ordinals + i * sizeof(WORD), sizeof(ordinal));
            if (ordinal >= exports.NumberOfFunctions) return NULL;
            DWORD address;
            memcpy(&address, functions + ordinal * sizeof(DWORD), sizeof(address));
            if (address >= directory.VirtualAddress && address - directory.VirtualAddress < directory.Size) return NULL;
            return rva(address, length);
        }
        return NULL;
    }
};

static uint32_t vm_image_syscall(const uint8_t *code, size_t size) {
    if (code == NULL || size < 11 || memcmp(code, "\x4c\x8b\xd1\xb8", 4) != 0) return 0;
    uint32_t number;
    memcpy(&number, code + 4, sizeof(number));
    if (number == 0 || number > 0xffff) return 0;
    for (size_t i = 8; i + 3 <= size; i++) {
        if (code[i] == 0x0f && code[i + 1] == 0x05 && code[i + 2] == 0xc3) return number;
    }
    return 0;
}

static uint32_t vm_file_syscall(const wchar_t *path, const char *name) {
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return 0;
    LARGE_INTEGER length;
    if (!GetFileSizeEx(file, &length) || length.QuadPart <= 0 || length.QuadPart > 64 * 1024 * 1024) {
        CloseHandle(file);
        return 0;
    }
    HANDLE mapping = CreateFileMappingW(file, NULL, PAGE_READONLY, 0, 0, NULL);
    const uint8_t *data = mapping == NULL ? NULL
            : (const uint8_t *) MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0);
    uint32_t number = 0;
    if (data != NULL) {
        vm_image image = {data, (size_t) length.QuadPart};
        if (image.parse()) number = vm_image_syscall(image.find_export(name, 32), 32);
        if (!UnmapViewOfFile(data)) number = 0;
    }
    if (mapping != NULL && !CloseHandle(mapping)) number = 0;
    if (!CloseHandle(file)) number = 0;
    return number;
}
#endif

#endif
