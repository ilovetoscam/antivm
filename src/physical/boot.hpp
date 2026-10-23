#ifndef VM_BOOT_HPP
#define VM_BOOT_HPP

#include "../unknown/state.hpp"

enum vm_boot_mode {
    vm_boot_unknown,
    vm_boot_bios,
    vm_boot_uefi
};

enum vm_secure_boot_state {
    vm_secure_boot_unknown,
    vm_secure_boot_disabled,
    vm_secure_boot_enabled,
    vm_secure_boot_not_applicable
};

struct vm_boot_report {
    vm_boot_mode mode = vm_boot_unknown;
    vm_reason firmware_reason = vm_reason_platform;
    uint32_t firmware_error = 0;
    vm_secure_boot_state secure_boot = vm_secure_boot_unknown;
    vm_reason secure_boot_reason = vm_reason_platform;
    uint32_t secure_boot_error = 0;
};

#ifdef _WIN32
#include <windows.h>
#endif

static vm_boot_report vm_boot_query(void) {
    vm_boot_report report;
#ifdef _WIN32
    report.firmware_reason = vm_reason_boot_api;
    HMODULE kernel = GetModuleHandleW(L"kernel32.dll");
    auto firmware = kernel == NULL ? NULL
            : (decltype(&GetFirmwareType)) GetProcAddress(kernel, "GetFirmwareType");
    if (firmware == NULL) {
        report.firmware_error = GetLastError();
    } else {
        FIRMWARE_TYPE type = FirmwareTypeUnknown;
        if (!firmware(&type)) {
            report.firmware_reason = vm_reason_boot_query;
            report.firmware_error = GetLastError();
        } else if (type == FirmwareTypeBios || type == FirmwareTypeUefi) {
            report.mode = type == FirmwareTypeBios ? vm_boot_bios : vm_boot_uefi;
            report.firmware_reason = vm_reason_none;
        } else {
            report.firmware_reason = vm_reason_boot_query;
            report.firmware_error = ERROR_INVALID_DATA;
        }
    }
    if (report.mode == vm_boot_bios) {
        report.secure_boot = vm_secure_boot_not_applicable;
        report.secure_boot_reason = vm_reason_none;
        return report;
    }
    report.secure_boot_reason = vm_reason_secure_boot;
    HMODULE library = LoadLibraryExW(L"advapi32.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (library == NULL) {
        report.secure_boot_error = GetLastError();
        return report;
    }
    auto read = (decltype(&RegGetValueW)) GetProcAddress(library, "RegGetValueW");
    if (read == NULL) {
        report.secure_boot_error = GetLastError();
    } else {
        DWORD value = 0, type = 0, size = sizeof(value);
        LSTATUS status = read(HKEY_LOCAL_MACHINE,
                L"SYSTEM\\CurrentControlSet\\Control\\SecureBoot\\State", L"UEFISecureBootEnabled",
                RRF_RT_REG_DWORD | RRF_SUBKEY_WOW6464KEY, &type, &value, &size);
        if (status != ERROR_SUCCESS) {
            report.secure_boot_error = (uint32_t) status;
        } else if (type != REG_DWORD || size != sizeof(value) || value > 1) {
            report.secure_boot_error = ERROR_INVALID_DATA;
        } else {
            report.secure_boot_reason = vm_reason_none;
            report.secure_boot = value == 1 ? vm_secure_boot_enabled : vm_secure_boot_disabled;
        }
    }
    FreeLibrary(library);
#endif
    // secure boot açık dıye vm ıhtımalını çöpe atma amk
    return report;
}

#endif
