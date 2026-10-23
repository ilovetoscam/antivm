#ifndef VM_FIRMWARE_HPP
#define VM_FIRMWARE_HPP

#include "../unknown/state.hpp"
#include "../virtual/vendor.hpp"
#include "smbios.hpp"
#include "acpi.hpp"
#include <stdlib.h>
#include <wchar.h>

struct vm_firmware_details {
    vm_report smbios = {vm_unknown, vm_reason_incomplete, 0, 0, 0, false};
    vm_report firm = {vm_unknown, vm_reason_incomplete, 0, 0, 0, false};
    vm_report acpi = {vm_unknown, vm_reason_incomplete, 0, 0, 0, false};
    vm_report legacy = {vm_unknown, vm_reason_incomplete, 0, 0, 0, false};
};

#ifdef _WIN32
#include <windows.h>
#include <winternl.h>

typedef UINT (WINAPI *vm_table_query)(DWORD, DWORD, PVOID, DWORD);

static vm_report vm_memory_report(void) {
    typedef LONG (NTAPI *OpenSection)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES);
    typedef LONG (NTAPI *MapSection)(HANDLE, HANDLE, PVOID *, ULONG_PTR, SIZE_T,
            PLARGE_INTEGER, PSIZE_T, ULONG, ULONG, ULONG);
    typedef LONG (NTAPI *UnmapSection)(HANDLE, PVOID);
    typedef LONG (NTAPI *CloseSection)(HANDLE);
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    if (ntdll == NULL) return {vm_unknown, vm_reason_firmware_api, GetLastError()};
    OpenSection open = (OpenSection) GetProcAddress(ntdll, "NtOpenSection");
    MapSection map = (MapSection) GetProcAddress(ntdll, "NtMapViewOfSection");
    UnmapSection unmap = (UnmapSection) GetProcAddress(ntdll, "NtUnmapViewOfSection");
    CloseSection close = (CloseSection) GetProcAddress(ntdll, "NtClose");
    if (open == NULL || map == NULL || unmap == NULL || close == NULL) {
        return {vm_unknown, vm_reason_firmware_api, GetLastError()};
    }
    const wchar_t *path = L"\\Device\\PhysicalMemory";
    UNICODE_STRING name;
    name.Buffer = const_cast<wchar_t *>(path);
    name.Length = (USHORT) (wcslen(path) * sizeof(wchar_t));
    name.MaximumLength = name.Length + sizeof(wchar_t);
    OBJECT_ATTRIBUTES attributes = {};
    attributes.Length = sizeof(attributes);
    attributes.ObjectName = &name;
    attributes.Attributes = 0x40u;
    HANDLE section = NULL;
    LONG status = open(&section, SECTION_MAP_READ, &attributes);
    if (status < 0 || section == NULL) return {vm_unknown, vm_reason_memory_access, (uint32_t) status};
    LARGE_INTEGER offset;
    offset.QuadPart = 0xc0000;
    SIZE_T size = 0x10000;
    PVOID data = NULL;
    HANDLE process = GetCurrentProcess();
    status = map(section, process, &data, 0, size, &offset, &size, 1, 0, PAGE_READONLY);
    vm_report report = {vm_unknown, vm_reason_memory_access, (uint32_t) status};
    if (status >= 0 && data != NULL) {
        if (size != 0 && size <= 0x10000) {
            bool matched = vm_firmware_vendor((const uint8_t *) data, size);
            report = {matched ? vm_virtual : vm_physical, matched ? vm_reason_firmware_vendor : vm_reason_none};
        } else {
            report = {vm_unknown, vm_reason_firmware_size};
        }
        status = unmap(process, data);
        if (status < 0) report = {vm_unknown, vm_reason_memory_release, (uint32_t) status};
    }
    status = close(section);
    if (status < 0) report = {vm_unknown, vm_reason_memory_release, (uint32_t) status};
    return report;
}

static vm_state vm_memory_state(void) {
    return vm_memory_report().state;
}
static vm_report vm_firmware_table_report(vm_table_query query, DWORD provider, DWORD table) {
    if (query == NULL) return {vm_unknown, vm_reason_firmware_api, 0, provider, table};
    SetLastError(ERROR_SUCCESS);
    UINT size = query(provider, table, NULL, 0);
    if (size == 0 || size > 16u * 1024u * 1024u) {
        return {vm_unknown, size == 0 ? vm_reason_firmware_read : vm_reason_firmware_size,
                size == 0 ? GetLastError() : 0, provider, table};
    }
    uint8_t *data = (uint8_t *) malloc(size);
    if (data == NULL) return {vm_unknown, vm_reason_allocation, 0, provider, table};
    SetLastError(ERROR_SUCCESS);
    UINT read = query(provider, table, data, size);
    vm_report report;
    if (read != size) {
        report = {vm_unknown, vm_reason_firmware_read, GetLastError()};
    } else if (provider == 0x52534d42u) {
        report = vm_smbios_report(data, size);
    } else if (provider == 0x41435049u) {
        report = vm_acpi_report(data, size, table);
    } else {
        bool matched = vm_firmware_vendor(data, size);
        report = {matched ? vm_virtual : vm_physical, matched ? vm_reason_firmware_vendor : vm_reason_none};
    }
    free(data);
    report.provider = provider;
    report.table = table;
    return report;
}

static vm_state vm_firmware_state(vm_table_query query, DWORD provider, DWORD table) {
    return vm_firmware_table_report(query, provider, table).state;
}

typedef UINT (WINAPI *vm_table_enum)(DWORD, PVOID, DWORD);

static vm_report vm_firmware_tables_report(vm_table_enum enumerate, vm_table_query query, DWORD provider) {
    if (enumerate == NULL || query == NULL) return {vm_unknown, vm_reason_firmware_api, 0, provider};
    SetLastError(ERROR_SUCCESS);
    UINT size = enumerate(provider, NULL, 0);
    if (size == 0) {
        DWORD error = GetLastError();
        vm_report report = {vm_unknown, vm_reason_firmware_enumeration, error, provider};
        if (error == ERROR_INVALID_FUNCTION || error == ERROR_NOT_SUPPORTED) {
            report.reason = vm_reason_firmware_unsupported;
            report.applicable = false;
        }
        return report;
    }
    // tablo lıstesı dıye kamyonla çöp gelırse belleğı peşkeş çekme
    if (size > 1024u * 1024u || size % sizeof(DWORD) != 0) {
        return {vm_unknown, vm_reason_firmware_enumeration, 0, provider};
    }
    DWORD *tables = (DWORD *) malloc(size);
    if (tables == NULL) return {vm_unknown, vm_reason_allocation, 0, provider};
    SetLastError(ERROR_SUCCESS);
    if (enumerate(provider, tables, size) != size) {
        DWORD error = GetLastError();
        free(tables);
        return {vm_unknown, vm_reason_firmware_enumeration, error, provider};
    }
    vm_result result;
    for (size_t i = 0; i < size / sizeof(DWORD); i++) {
        result.observe(vm_firmware_table_report(query, provider, tables[i]));
        if (result.detected) break;
    }
    free(tables);
    return result.report();
}

static vm_report vm_firmware_collect(vm_firmware_details *details) {
    HMODULE kernel32 = GetModuleHandleA("kernel32.dll");
    if (kernel32 == NULL) return {vm_unknown, vm_reason_firmware_api, GetLastError()};
    vm_table_enum enumerate = (vm_table_enum) GetProcAddress(kernel32, "EnumSystemFirmwareTables");
    vm_table_query query = (vm_table_query) GetProcAddress(kernel32, "GetSystemFirmwareTable");
    if (query == NULL) {
        vm_report report = vm_memory_report();
        if (details != NULL) details->legacy = report;
        return report;
    }
    vm_result result;
    // bıos temız dıye smbıos ıçın kefıl olma amk ayrı ayrı bak
    vm_report report = vm_firmware_table_report(query, 0x52534d42u, 0);
    if (details != NULL) details->smbios = report;
    result.observe(report);
    if (result.detected && details == NULL) return result.report();
    if (enumerate == NULL) {
        report = vm_memory_report();
        if (details != NULL) details->legacy = report;
        result.observe(report);
        return result.report();
    }
    report = vm_firmware_tables_report(enumerate, query, 0x4649524du);
    if (details != NULL) details->firm = report;
    result.observe(report);
    if (result.detected && details == NULL) return result.report();
    report = vm_firmware_tables_report(enumerate, query, 0x41435049u);
    if (details != NULL) details->acpi = report;
    result.observe(report);
    return result.report();
}
#else
static vm_report vm_firmware_collect(vm_firmware_details *) {
    return {vm_unknown, vm_reason_platform};
}
#endif

static vm_report vm_firmware_report(void) {
    return vm_firmware_collect(NULL);
}

static vm_state vm_firmware_detect(void) {
    return vm_firmware_report().state;
}

#endif
