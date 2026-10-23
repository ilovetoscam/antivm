#ifndef VM_DEBUG_MEMORY_HPP
#define VM_DEBUG_MEMORY_HPP

#include "../unknown/state.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

struct vm_page_report {
    vm_reason reason = vm_reason_platform;
    uint32_t native_error = 0;
    uint32_t state = 0;
    uint32_t protection = 0;
    bool guarded = false;
};

static vm_page_report vm_page_query(const void *address) {
    vm_page_report report;
#ifdef _WIN32
    MEMORY_BASIC_INFORMATION information = {};
    SIZE_T size = VirtualQuery(address, &information, sizeof(information));
    if (size != sizeof(information)) {
        report.reason = vm_reason_memory_query;
        if (size == 0) report.native_error = GetLastError();
        return report;
    }
    report.reason = vm_reason_none;
    report.state = information.State;
    if (information.State == MEM_COMMIT) {
        report.protection = information.Protect;
        // sayfaya dokunma lan guard bayrağı tek kullanımlık bılet gıbı
        report.guarded = (information.Protect & PAGE_GUARD) != 0;
    }
#else
    (void) address;
#endif
    return report;
}

#endif
