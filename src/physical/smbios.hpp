#ifndef VM_SMBIOS_HPP
#define VM_SMBIOS_HPP

#include "../unknown/state.hpp"
#include "../virtual/vendor.hpp"

struct vm_smbios_text {
    const uint8_t *data = NULL;
    size_t size = 0;
};

static bool vm_smbios_string(const uint8_t *data, size_t first, size_t last,
        unsigned index, vm_smbios_text *text) {
    if (index == 0) return true;
    unsigned current = 1;
    for (size_t start = first; start < last;) {
        size_t end = start;
        while (end < last && data[end] != 0) end++;
        if (current == index) {
            text->data = data + start;
            text->size = end - start;
            return true;
        }
        current++;
        start = end + 1;
    }
    return false;
}

static vm_report vm_smbios_report(const uint8_t *data, size_t size) {
    if (data == NULL || size <= 8 || data[1] < 2) return {vm_unknown, vm_reason_smbios_header};
    uint32_t length;
    memcpy(&length, data + 4, sizeof(length));
    if (length != size - 8) return {vm_unknown, vm_reason_smbios_header};
    vm_result result;
    bool identity = false;
    for (size_t position = 8; position < size;) {
        if (size - position < 4) return {vm_unknown, vm_reason_smbios_structure};
        unsigned type = data[position];
        unsigned formatted = data[position + 1];
        uint16_t handle;
        memcpy(&handle, data + position + 2, sizeof(handle));
        auto location = [&](vm_report report, uint32_t field = UINT32_MAX) {
            report.record_type = type;
            report.record_handle = handle;
            report.record_offset = position;
            report.field_offset = field;
            return report;
        };
        if (formatted < 4 || formatted > size - position) return location({vm_unknown, vm_reason_smbios_structure}, 1);
        size_t first = position + formatted;
        if (first + 1 < size && data[first] == 0 && data[first + 1] != 0) return location({vm_unknown, vm_reason_smbios_structure});
        size_t last = first;
        while (last + 1 < size && (data[last] != 0 || data[last + 1] != 0)) last++;
        if (last + 1 >= size) return location({vm_unknown, vm_reason_smbios_structure});
        if (type == 127) {
            if (formatted != 4 || first != last) return location({vm_unknown, vm_reason_smbios_structure});
            for (size_t i = last + 2; i < size; i++) {
                if (data[i] != 0 && data[i] != 0xff) return location({vm_unknown, vm_reason_smbios_structure});
            }
            break;
        }
        unsigned count = 0;
        // serı numarasında vmware yazdı dıye kasayı sürgüne yollama lan
        vm_reason reasons[3] = {};
        if (type == 0) {
            if (formatted < 0x12) return location({vm_unknown, vm_reason_smbios_structure}, 1);
            count = 2;
            reasons[0] = vm_reason_bios_vendor;
            reasons[1] = vm_reason_bios_version;
        } else if (type == 1 || type == 2) {
            if (formatted < 8) return location({vm_unknown, vm_reason_smbios_structure}, 1);
            count = 3;
            reasons[0] = type == 1 ? vm_reason_system_manufacturer : vm_reason_board_manufacturer;
            reasons[1] = type == 1 ? vm_reason_system_product : vm_reason_board_product;
            reasons[2] = type == 1 ? vm_reason_system_version : vm_reason_board_version;
        } else if (type == 3) {
            if (formatted < 9) return location({vm_unknown, vm_reason_smbios_structure}, 1);
            count = 1;
            reasons[0] = vm_reason_chassis_manufacturer;
        }
        vm_smbios_text fields[3];
        for (unsigned i = 0; i < count; i++) {
            if (!vm_smbios_string(data, first, last, data[position + 4 + i], &fields[i])) {
                return location({vm_unknown, vm_reason_smbios_string}, 4 + i);
            }
            if (fields[i].size == 0) continue;
            identity = true;
            if (vm_vendor_field(fields[i].data, fields[i].size)) {
                result.observe(location({vm_virtual, reasons[i]}, 4 + i));
            }
        }
        if (type == 1 && (data[1] > 2 || (data[1] == 2 && data[2] >= 3))
                && (fields[0].size == 0 || fields[1].size == 0)) {
            return location({vm_unknown, vm_reason_smbios_string}, fields[0].size == 0 ? 4 : 5);
        }
        // mıcrosoft tek başına suç değıl ürün adı da aynı boku söylemelı
        if (type == 1 && vm_text_token(fields[0].data, fields[0].size, "microsoft")
                && vm_text_equal(fields[1].data, fields[1].size, "virtual machine")) {
            result.observe(location({vm_virtual, vm_reason_system_product}, 5));
        }
        if (type == 1 && vm_text_equal(fields[0].data, fields[0].size, "xen")
                && vm_text_equal(fields[1].data, fields[1].size, "hvm domu")) {
            result.observe(location({vm_virtual, vm_reason_system_product}, 5));
        }
        if ((type == 1 || type == 2) && vm_text_equal(fields[0].data, fields[0].size, "freebsd")
                && vm_text_equal(fields[1].data, fields[1].size, "bhyve")) {
            result.observe(location({vm_virtual, type == 1 ? vm_reason_system_product : vm_reason_board_product}, 5));
        }
        // bayrak yok dıye fızıksel damgası basma yokluğu kanıt değıl lan
        if (type == 0 && formatted >= 0x14 && (data[1] > 2 || data[2] >= 3)
                && (data[position + 0x13] & 0x10u) != 0) {
            result.observe(location({vm_virtual, vm_reason_smbios_virtual}, 0x13));
        }
        position = last + 2;
    }
    if (!identity && !result.detected) return {vm_unknown, vm_reason_smbios_structure};
    result.observe(vm_physical);
    return result.report();
}

#endif
