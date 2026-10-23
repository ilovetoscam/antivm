#ifndef VM_SYSCALL_HPP
#define VM_SYSCALL_HPP

#include "version.hpp"
#include "../physical/image.hpp"

#ifdef _WIN32
#include <windows.h>

static uint32_t vm_windows_build(void) {
    typedef LONG (WINAPI *GetVersion)(OSVERSIONINFOW *);
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    GetVersion get_version = ntdll == NULL ? NULL : (GetVersion) GetProcAddress(ntdll, "RtlGetVersion");
    OSVERSIONINFOW version = {};
    version.dwOSVersionInfoSize = sizeof(version);
    return get_version != NULL && get_version(&version) >= 0 ? version.dwBuildNumber : 0;
}

static uint32_t vm_resolve_syscall(const char *name) {
    wchar_t path[MAX_PATH];
    UINT length = GetSystemDirectoryW(path, MAX_PATH);
    const wchar_t suffix[] = L"\\ntdll.dll";
    if (length == 0 || length >= MAX_PATH || sizeof(suffix) / sizeof(wchar_t) > MAX_PATH - length) return 0;
    memcpy(path + length, suffix, sizeof(suffix));
    uint32_t number = vm_file_syscall(path, name);
    if (strcmp(name, "NtQueryInformationProcess") == 0) {
        vm_syscall_set known = vm_syscalls_for_build(vm_windows_build(), true);
        // eskı syscall numarasını yenı wındows ıçıne zar gıbı atma amk
        if (known.query_information_process != 0 && number != known.query_information_process) return 0;
    }
    return number;
}

#if defined(__x86_64__) && (defined(__clang__) || defined(__GNUC__))
__attribute__((naked)) static LONG WINAPI vm_direct_query_process(
        HANDLE, ULONG, PVOID, ULONG, PULONG, ULONG) {
    __asm__ volatile ("movq %rcx, %r10\n\tmovl 48(%rsp), %eax\n\tsyscall\n\tretq");
}

__attribute__((naked)) static LONG WINAPI vm_direct_query_system(
        ULONG, PVOID, ULONG, PULONG, ULONG) {
    __asm__ volatile ("movq %rcx, %r10\n\tmovl 40(%rsp), %eax\n\tsyscall\n\tretq");
}
#endif

static LONG vm_native_query_process(HANDLE process, ULONG kind, PVOID buffer, ULONG size, PULONG length) {
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    if (ntdll == NULL) return (LONG) 0xc00000bbu;
#if defined(__x86_64__) && (defined(__clang__) || defined(__GNUC__))
    if (GetProcAddress(ntdll, "wine_get_version") == NULL) {
        static const uint32_t number = vm_resolve_syscall("NtQueryInformationProcess");
        if (number == 0) return (LONG) 0xc00000bbu;
        return vm_direct_query_process(process, kind, buffer, size, length, number);
    }
#endif
    typedef LONG (WINAPI *Query)(HANDLE, ULONG, PVOID, ULONG, PULONG);
    Query query = (Query) GetProcAddress(ntdll, "NtQueryInformationProcess");
    return query != NULL ? query(process, kind, buffer, size, length) : (LONG) 0xc00000bbu;
}

static LONG vm_native_query_system(ULONG kind, PVOID buffer, ULONG size, PULONG length) {
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    if (ntdll == NULL) return (LONG) 0xc00000bbu;
#if defined(__x86_64__) && (defined(__clang__) || defined(__GNUC__))
    if (GetProcAddress(ntdll, "wine_get_version") == NULL) {
        static const uint32_t number = vm_resolve_syscall("NtQuerySystemInformation");
        if (number == 0) return (LONG) 0xc00000bbu;
        return vm_direct_query_system(kind, buffer, size, length, number);
    }
#endif
    typedef LONG (WINAPI *Query)(ULONG, PVOID, ULONG, PULONG);
    Query query = (Query) GetProcAddress(ntdll, "NtQuerySystemInformation");
    return query != NULL ? query(kind, buffer, size, length) : (LONG) 0xc00000bbu;
}
#endif

#endif
