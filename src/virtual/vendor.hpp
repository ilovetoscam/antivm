#ifndef VM_VENDOR_HPP
#define VM_VENDOR_HPP

#include <stdint.h>
#include <stddef.h>
#include <string.h>

static unsigned char vm_ascii_lower(unsigned char value) {
    return value >= 'A' && value <= 'Z' ? value + ('a' - 'A') : value;
}

static bool vm_text_equal(const uint8_t *data, size_t size, const char *text) {
    while (size != 0 && (data[size - 1] == ' ' || data[size - 1] == 0)) size--;
    if (size != strlen(text)) return false;
    for (size_t i = 0; i < size; i++) {
        if (vm_ascii_lower(data[i]) != vm_ascii_lower((unsigned char) text[i])) return false;
    }
    return true;
}

static bool vm_text_token(const uint8_t *data, size_t size, const char *text) {
    size_t length = strlen(text);
    if (length == 0 || length > size) return false;
    for (size_t i = 0; i <= size - length; i++) {
        bool equal = true;
        for (size_t j = 0; j < length; j++) {
            if (vm_ascii_lower(data[i + j]) != vm_ascii_lower((unsigned char) text[j])) {
                equal = false;
                break;
            }
        }
        if (!equal) continue;
        unsigned char before = i == 0 ? ' ' : vm_ascii_lower(data[i - 1]);
        unsigned char after = i + length == size ? ' ' : vm_ascii_lower(data[i + length]);
        bool left = (before >= 'a' && before <= 'z') || (before >= '0' && before <= '9');
        bool right = (after >= 'a' && after <= 'z') || (after >= '0' && after <= '9');
        if (!left && !right) return true;
    }
    return false;
}

static bool vm_vendor_field(const uint8_t *data, size_t size) {
    const char *names[] = {"vmware", "virtualbox", "qemu", "innotek", "parallels"};
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        if (vm_text_token(data, size, names[i])) return true;
    }
    return false;
}

static bool vm_firmware_vendor(const uint8_t *data, size_t data_size)
{
    // mıcrosoft görünce vm dıye atlama lan surface de aynı soyadını taşıyor
    bool microsoft_vm = false;
    for (size_t i = 0; i + 15 <= data_size; i++) {
        if (memcmp(data + i, "Virtual Machine", 15) == 0) { microsoft_vm = true; break; }
    }
    for (size_t i = 0; i < data_size; i++) {
        if (i + 3 < data_size && data[i + 0] == 'Q' && data[i + 1] == 'E' && data[i + 2] == 'M' && data[i + 3] == 'U')
            return true;
        if (microsoft_vm && i + 8 < data_size && data[i + 0] == 'M' && data[i + 1] == 'i' && data[i + 2] == 'c' && data[i + 3] == 'r' && data[i + 4] == 'o' && data[i + 5] == 's' && data[i + 6] == 'o' && data[i + 7] == 'f' && data[i + 8] == 't')
            return true;
        if (i + 6 < data_size && data[i + 0] == 'i' && data[i + 1] == 'n' && data[i + 2] == 'n' && data[i + 3] == 'o' && data[i + 4] == 't' && data[i + 5] == 'e' && data[i + 6] == 'k')
            return true;
        if (i + 9 < data_size && data[i + 0] == 'V' && data[i + 1] == 'i' && data[i + 2] == 'r' && data[i + 3] == 't' && data[i + 4] == 'u' && data[i + 5] == 'a' && data[i + 6] == 'l' && data[i + 7] == 'B' && data[i + 8] == 'o' && data[i + 9] == 'x')
            return true;
        if (i + 5 < data_size && data[i + 0] == 'V' && data[i + 1] == 'M' && data[i + 2] == 'w' && data[i + 3] == 'a' && data[i + 4] == 'r' && data[i + 5] == 'e')
            return true;
        if (i + 8 < data_size && data[i + 0] == 'P' && data[i + 1] == 'a' && data[i + 2] == 'r' && data[i + 3] == 'a' && data[i + 4] == 'l' && data[i + 5] == 'l' && data[i + 6] == 'e' && data[i + 7] == 'l' && data[i + 8] == 's')
            return true;
    }
    return false;
}

#endif
