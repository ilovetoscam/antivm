#ifndef VM_TPM_HPP
#define VM_TPM_HPP

#include "../unknown/state.hpp"

enum vm_tpm_state {
    vm_tpm_unknown,
    vm_tpm_absent,
    vm_tpm_present
};

struct vm_tpm_report {
    vm_tpm_state state = vm_tpm_unknown;
    vm_reason reason = vm_reason_platform;
    uint32_t native_error = 0;
    uint32_t version = 0;
};

#ifdef _WIN32
#include <windows.h>
#include <tbs.h>
#endif

static vm_tpm_report vm_tpm_query(void) {
    vm_tpm_report report;
#ifdef _WIN32
    HMODULE library = LoadLibraryExW(L"tbs.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (library == NULL) return {vm_tpm_unknown, vm_reason_tpm_api, GetLastError()};
    auto query = (decltype(&Tbsi_GetDeviceInfo)) GetProcAddress(library, "Tbsi_GetDeviceInfo");
    if (query == NULL) {
        report = {vm_tpm_unknown, vm_reason_tpm_api, GetLastError()};
    } else {
        TPM_DEVICE_INFO information = {};
        information.structVersion = TPM_VERSION_20;
        TBS_RESULT status = query(sizeof(information), &information);
        if (status == TBS_SUCCESS && (information.tpmVersion == TPM_VERSION_12
                || information.tpmVersion == TPM_VERSION_20)) {
            report = {vm_tpm_present, vm_reason_none, 0, information.tpmVersion};
        } else if (status == TBS_E_TPM_NOT_FOUND) {
            report = {vm_tpm_absent, vm_reason_none, status};
        } else {
            report = {vm_tpm_unknown, vm_reason_tpm_query, status};
        }
    }
    FreeLibrary(library);
#endif
    // tpm var dıye kasaya fızıksel kımlığı basma sanalı da var lan
    return report;
}

#endif
