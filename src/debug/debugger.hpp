#ifndef VM_DEBUGGER_HPP
#define VM_DEBUGGER_HPP

#include <stdint.h>
#include "../unknown/state.hpp"
#include "registers.hpp"
#include "handles.hpp"
#include "kernel.hpp"
#include "syscall.hpp"
#include "memory.hpp"

struct vm_debug_report {
    int code = 100;
    vm_reason reason = vm_reason_incomplete;
    uint32_t native_error = 0;
};

#ifdef _WIN32
#include <windows.h>
#include <intrin.h>

static vm_debug_report vm_debugger_report(void) {
    if (IsDebuggerPresent()) return {10, vm_reason_debugger_api};
    const uint8_t *peb = NULL;
#if defined(_M_X64) || defined(__x86_64__)
    peb = (const uint8_t *) __readgsqword(0x60);
#elif defined(_M_IX86) || defined(__i386__)
    peb = (const uint8_t *) __readfsdword(0x30);
#else
    return {100, vm_reason_platform};
#endif
    if (peb == NULL) return {100, vm_reason_debugger_peb};
    if (peb[2] != 0) return {10, vm_reason_debugger_peb};
    BOOL remote = FALSE;
    if (!CheckRemoteDebuggerPresent(GetCurrentProcess(), &remote)) {
        return {100, vm_reason_debugger_remote, (uint32_t) GetLastError()};
    }
    if (remote) return {10, vm_reason_debugger_remote};
    HANDLE debug_object = NULL;
    LONG status = vm_native_query_process(GetCurrentProcess(), 7, &debug_object, sizeof(debug_object), NULL);
    if (status < 0) return {100, vm_reason_debugger_port, (uint32_t) status};
    if (debug_object != NULL) return {10, vm_reason_debugger_port};
    status = vm_native_query_process(GetCurrentProcess(), 30, &debug_object, sizeof(debug_object), NULL);
    if (status >= 0) {
        if (debug_object != NULL) CloseHandle(debug_object);
        return {10, vm_reason_debugger_object};
    }
    if ((ULONG) status != 0xc0000353u) return {100, vm_reason_debugger_object, (uint32_t) status};
    int handles = vm_debug_handles_error();
    if (handles != 0) return {handles, vm_reason_debugger_handles};
    int registers = vm_debug_registers_error();
    if (registers != 0) return {registers, vm_reason_debugger_registers};
    struct {
        BOOLEAN enabled;
        BOOLEAN not_present;
    } kernel_debugger = {};
    status = vm_native_query_system(35, &kernel_debugger, sizeof(kernel_debugger), NULL);
    if (status < 0) return {100, vm_reason_debugger_kernel, (uint32_t) status};
    if (kernel_debugger.enabled && !kernel_debugger.not_present) return {10, vm_reason_debugger_kernel};
    int modules = vm_kernel_modules_error();
    return {modules, modules == 0 ? vm_reason_none : vm_reason_debugger_modules};
}
#else
static vm_debug_report vm_debugger_report(void) {
    return {100, vm_reason_platform};
}
#endif

static int vm_debugger_error(void) {
    // eskı çağıranların düzenını rapor ekledık dıye dağıtma
    return vm_debugger_report().code;
}

#endif
