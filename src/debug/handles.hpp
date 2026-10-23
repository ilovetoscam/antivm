#ifndef VM_HANDLES_HPP
#define VM_HANDLES_HPP

#ifdef _WIN32
#include <windows.h>
#include <stdint.h>

static thread_local bool *vm_handle_exception;

static LONG CALLBACK vm_handle_handler(EXCEPTION_POINTERS *exception) {
    if (vm_handle_exception == NULL
            || exception->ExceptionRecord->ExceptionCode != EXCEPTION_INVALID_HANDLE) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    *vm_handle_exception = true;
    return EXCEPTION_CONTINUE_EXECUTION;
}

static int vm_debug_handles_error(void) {
    bool raised = false;
    PVOID handler = AddVectoredExceptionHandler(1, vm_handle_handler);
    if (handler == NULL) return 100;
    vm_handle_exception = &raised;
    // böyle bır handle yok amk başarı dönerse bır şey araya gırmış
    BOOL closed = CloseHandle((HANDLE) (uintptr_t) 0xdeadc0deu);
    DWORD error = GetLastError();
    vm_handle_exception = NULL;
    if (!RemoveVectoredExceptionHandler(handler)) return 100;
    if (raised || closed) return 10;
    return error == ERROR_INVALID_HANDLE ? 0 : 100;
}
#else
static int vm_debug_handles_error(void) {
    return 100;
}
#endif

#endif
