#ifndef VM_ACPI_HPP
#define VM_ACPI_HPP

#include "../unknown/state.hpp"
#include "../virtual/vendor.hpp"

static vm_report vm_acpi_report(const uint8_t *data, size_t size, uint32_t table = 0) {
    if (data == NULL || size < 8) return {vm_unknown, vm_reason_acpi_header};
    uint32_t length, signature;
    for (size_t i = 0; i < 4; i++) {
        if (data[i] < 0x21 || data[i] > 0x7e) return {vm_unknown, vm_reason_acpi_header};
    }
    memcpy(&signature, data, sizeof(signature));
    memcpy(&length, data + 4, sizeof(length));
    auto location = [&](vm_report report, uint32_t field = UINT32_MAX) {
        report.provider = 0x41435049u;
        report.table = signature;
        report.field_offset = field;
        return report;
    };
    if (length != size || (table != 0 && table != signature)) return {vm_unknown, vm_reason_acpi_header};
    if (memcmp(data, "FACS", 4) == 0) {
        // facs checksum taşımıyor olmayan borcu zorla ödetme lan
        return size >= 64 ? vm_report{vm_physical, vm_reason_none} : vm_report{vm_unknown, vm_reason_acpi_header};
    }
    if (size < 36) return {vm_unknown, vm_reason_acpi_header};
    uint8_t checksum = 0;
    for (size_t i = 0; i < size; i++) checksum = (uint8_t) (checksum + data[i]);
    if (checksum != 0) return location({vm_unknown, vm_reason_acpi_checksum}, 9);
    // aml gövdesınde kelıme avlayıp gerçek kasayı çöpe atma
    bool oem = vm_vendor_field(data + 10, 6);
    bool matched = oem || vm_vendor_field(data + 16, 8);
    uint32_t field = oem ? 10 : 16;
    if (!matched) field = 10;
    matched = matched || (vm_text_equal(data + 10, 6, "vbox") && vm_text_equal(data + 16, 4, "vbox"));
    matched = matched || (vm_text_equal(data + 10, 6, "bochs") && vm_text_equal(data + 16, 4, "bxpc"));
    return location(matched ? vm_report{vm_virtual, vm_reason_acpi_oem} : vm_report{vm_physical, vm_reason_none},
            matched ? field : UINT32_MAX);
}

#endif
