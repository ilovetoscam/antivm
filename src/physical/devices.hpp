#ifndef VM_DEVICES_HPP
#define VM_DEVICES_HPP

#include "../unknown/state.hpp"
#include <stddef.h>
#include <wchar.h>
#include <string.h>

enum vm_device_kind {
    vm_device_pci,
    vm_device_disk
};

struct vm_device_info {
    wchar_t name[256] = {};
    wchar_t hardware_ids[1024] = {};
    wchar_t service[128] = {};
    uint32_t name_error = 0;
    uint32_t hardware_error = 0;
    uint32_t service_error = 0;
    wchar_t driver_provider[128] = {};
    wchar_t driver_version[64] = {};
    wchar_t driver_inf[260] = {};
    uint64_t driver_date = 0;
    uint32_t provider_error = 0;
    uint32_t version_error = 0;
    uint32_t inf_error = 0;
    uint32_t date_error = 0;
};

struct vm_device_report {
    vm_reason reason = vm_reason_platform;
    uint32_t native_error = 0;
    size_t count = 0;
    size_t property_failures = 0;
    bool truncated = false;
};

#ifdef _WIN32
#include <windows.h>
#include <setupapi.h>
#endif

enum vm_property_state {
    vm_property_available,
    vm_property_missing,
    vm_property_denied,
    vm_property_truncated,
    vm_property_invalid,
    vm_property_unsupported,
    vm_property_failed
};

static vm_property_state vm_property_status(uint32_t error) {
    if (error == 0) return vm_property_available;
#ifdef _WIN32
    switch (error) {
        case ERROR_NOT_FOUND:
        case ERROR_FILE_NOT_FOUND:
        case ERROR_PATH_NOT_FOUND: return vm_property_missing;
        case ERROR_ACCESS_DENIED: return vm_property_denied;
        case ERROR_INSUFFICIENT_BUFFER:
        case ERROR_MORE_DATA: return vm_property_truncated;
        case ERROR_INVALID_DATA:
        case ERROR_DATATYPE_MISMATCH: return vm_property_invalid;
        case ERROR_PROC_NOT_FOUND:
        case ERROR_CALL_NOT_IMPLEMENTED:
        case ERROR_NOT_SUPPORTED: return vm_property_unsupported;
    }
#endif
    return vm_property_failed;
}

#ifdef _WIN32

static DWORD vm_device_property(decltype(&SetupDiGetDeviceRegistryPropertyW) query,
        HDEVINFO set, SP_DEVINFO_DATA *device, DWORD property, wchar_t *output, DWORD bytes) {
    DWORD type = 0, required = 0;
    SetLastError(ERROR_SUCCESS);
    if (!query(set, device, property, &type, (PBYTE) output, bytes, &required)) {
        DWORD error = GetLastError();
        memset(output, 0, bytes);
        return error == ERROR_SUCCESS ? ERROR_GEN_FAILURE : error;
    }
    bool multiple = property == SPDRP_HARDWAREID;
    if (type != (multiple ? REG_MULTI_SZ : REG_SZ) || required > bytes
            || required < (multiple ? 2u : 1u) * sizeof(wchar_t)
            || required % sizeof(wchar_t) != 0
            || output[required / sizeof(wchar_t) - 1] != 0
            || (multiple && output[required / sizeof(wchar_t) - 2] != 0)) {
        memset(output, 0, bytes);
        return ERROR_INVALID_DATA;
    }
    return 0;
}

static DWORD vm_driver_property(decltype(&SetupDiGetDevicePropertyW) query,
        HDEVINFO set, SP_DEVINFO_DATA *device, ULONG id, void *output, DWORD bytes) {
    if (query == NULL) {
        memset(output, 0, bytes);
        return ERROR_PROC_NOT_FOUND;
    }
    const DEVPROPKEY key = {{0xa8b865dd, 0x2e3d, 0x4094,
            {0xad, 0x97, 0xe5, 0x93, 0xa7, 0x0c, 0x75, 0xd6}}, id};
    DEVPROPTYPE type = 0;
    DWORD required = 0;
    SetLastError(ERROR_SUCCESS);
    if (!query(set, device, &key, &type, (PBYTE) output, bytes, &required, 0)) {
        DWORD error = GetLastError();
        memset(output, 0, bytes);
        return error == ERROR_SUCCESS ? ERROR_GEN_FAILURE : error;
    }
    bool valid = id == 2 ? type == DEVPROP_TYPE_FILETIME && required == sizeof(uint64_t)
            : type == DEVPROP_TYPE_STRING && required >= sizeof(wchar_t) && required <= bytes
                && required % sizeof(wchar_t) == 0 && ((wchar_t *) output)[required / sizeof(wchar_t) - 1] == 0;
    if (!valid || required > bytes) {
        memset(output, 0, bytes);
        return ERROR_INVALID_DATA;
    }
    return 0;
}
#endif

static vm_device_report vm_devices_query(vm_device_kind kind, vm_device_info *output, size_t capacity) {
    vm_device_report report;
    if ((output == NULL && capacity != 0) || (kind != vm_device_pci && kind != vm_device_disk)) {
        report.reason = vm_reason_argument;
        return report;
    }
#ifdef _WIN32
    HMODULE library = LoadLibraryExW(L"setupapi.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (library == NULL) return {vm_reason_device_api, GetLastError()};
    auto create = (decltype(&SetupDiGetClassDevsW)) GetProcAddress(library, "SetupDiGetClassDevsW");
    auto enumerate = (decltype(&SetupDiEnumDeviceInfo)) GetProcAddress(library, "SetupDiEnumDeviceInfo");
    auto property = (decltype(&SetupDiGetDeviceRegistryPropertyW)) GetProcAddress(library, "SetupDiGetDeviceRegistryPropertyW");
    auto destroy = (decltype(&SetupDiDestroyDeviceInfoList)) GetProcAddress(library, "SetupDiDestroyDeviceInfoList");
    if (create == NULL || enumerate == NULL || property == NULL || destroy == NULL) {
        report = {vm_reason_device_api, GetLastError()};
        FreeLibrary(library);
        return report;
    }
    auto driver = (decltype(&SetupDiGetDevicePropertyW)) GetProcAddress(library, "SetupDiGetDevicePropertyW");
    const GUID disk_class = {0x4d36e967, 0xe325, 0x11ce, {0xbf, 0xc1, 0x08, 0x00, 0x2b, 0xe1, 0x03, 0x18}};
    HDEVINFO set = kind == vm_device_pci
            ? create(NULL, L"PCI", NULL, DIGCF_PRESENT | DIGCF_ALLCLASSES)
            : create(&disk_class, NULL, NULL, DIGCF_PRESENT);
    report.reason = vm_reason_device_enumeration;
    if (set == INVALID_HANDLE_VALUE) {
        report.native_error = GetLastError();
        FreeLibrary(library);
        return report;
    }
    for (DWORD index = 0;; index++) {
        SP_DEVINFO_DATA device = {};
        device.cbSize = sizeof(device);
        if (!enumerate(set, index, &device)) {
            DWORD error = GetLastError();
            report.reason = error == ERROR_NO_MORE_ITEMS ? vm_reason_none : vm_reason_device_enumeration;
            report.native_error = error == ERROR_NO_MORE_ITEMS ? 0 : error;
            break;
        }
        // tampon dolduysa tamamını gördüm dıye sallama amk
        if (report.count == capacity || index == MAXDWORD) {
            report.reason = vm_reason_capacity;
            report.truncated = true;
            break;
        }
        vm_device_info &entry = output[report.count++];
        entry = {};
        entry.name_error = vm_device_property(property, set, &device, SPDRP_DEVICEDESC, entry.name, sizeof(entry.name));
        entry.hardware_error = vm_device_property(property, set, &device, SPDRP_HARDWAREID, entry.hardware_ids, sizeof(entry.hardware_ids));
        entry.service_error = vm_device_property(property, set, &device, SPDRP_SERVICE, entry.service, sizeof(entry.service));
        entry.provider_error = vm_driver_property(driver, set, &device, 9, entry.driver_provider, sizeof(entry.driver_provider));
        entry.version_error = vm_driver_property(driver, set, &device, 3, entry.driver_version, sizeof(entry.driver_version));
        entry.inf_error = vm_driver_property(driver, set, &device, 5, entry.driver_inf, sizeof(entry.driver_inf));
        entry.date_error = vm_driver_property(driver, set, &device, 2, &entry.driver_date, sizeof(entry.driver_date));
        report.property_failures += (entry.name_error != 0) + (entry.hardware_error != 0) + (entry.service_error != 0)
                + (entry.provider_error != 0) + (entry.version_error != 0) + (entry.inf_error != 0) + (entry.date_error != 0);
    }
    if (!destroy(set)) {
        DWORD error = GetLastError();
        if (report.reason == vm_reason_none) {
            report.reason = vm_reason_device_enumeration;
            report.native_error = error;
        }
    }
    FreeLibrary(library);
#endif
    return report;
}

#endif
