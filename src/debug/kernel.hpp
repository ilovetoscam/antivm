#ifndef VM_KERNEL_HPP
#define VM_KERNEL_HPP

#ifdef _WIN32
#include <windows.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "syscall.hpp"

struct vm_kernel_module {
    void *section;
    void *mapped_base;
    void *image_base;
    ULONG image_size;
    ULONG flags;
    USHORT load_order;
    USHORT init_order;
    USHORT load_count;
    USHORT name_offset;
    unsigned char path[256];
};

struct vm_kernel_module_list {
    ULONG count;
    vm_kernel_module modules[1];
};

static int vm_kernel_module_buffer_error(const void *data, size_t size) {
    size_t offset = offsetof(vm_kernel_module_list, modules);
    if (data == NULL || size < offset) return 100;
    ULONG count;
    memcpy(&count, data, sizeof(count));
    if (count == 0 || count > (size - offset) / sizeof(vm_kernel_module)) return 100;
    const char *names[] = {"sice.sys", "siwvid.sys", "ntice.sys", "iceext.sys", "syser.sys"};
    for (ULONG i = 0; i < count; i++) {
        vm_kernel_module module;
        memcpy(&module, (const uint8_t *) data + offset + i * sizeof(module), sizeof(module));
        if (module.name_offset >= sizeof(module.path)) return 100;
        const char *name = (const char *) module.path + module.name_offset;
        if (*name == 0 || memchr(name, 0, sizeof(module.path) - module.name_offset) == NULL) return 100;
        for (size_t j = 0; j < sizeof(names) / sizeof(names[0]); j++) {
            if (_stricmp(name, names[j]) == 0) return 10;
        }
    }
    return 0;
}

static int vm_kernel_modules_error(void) {
    ULONG size = 0;
    LONG status = vm_native_query_system(11, NULL, 0, &size);
    for (unsigned attempt = 0; attempt < 3; attempt++) {
        if ((ULONG) status != 0xc0000004u && (ULONG) status != 0xc0000023u) return 100;
        if (size < offsetof(vm_kernel_module_list, modules) || size > 16u * 1024u * 1024u) return 100;
        void *buffer = malloc(size);
        if (buffer == NULL) return 100;
        ULONG capacity = size;
        status = vm_native_query_system(11, buffer, capacity, &size);
        if (status >= 0) {
            int code = size <= capacity ? vm_kernel_module_buffer_error(buffer, size) : 100;
            free(buffer);
            return code;
        }
        free(buffer);
    }
    // yetkı yoksa temız dıye sallama lan göremedığın şeye kefıl olma
    return 100;
}
#else
static int vm_kernel_modules_error(void) {
    return 100;
}
#endif

#endif
