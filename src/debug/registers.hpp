#ifndef VM_REGISTERS_HPP
#define VM_REGISTERS_HPP

#include "../virtual/trap.hpp"

static int vm_debug_registers_error(void) {
#if defined(_WIN32) && (defined(__x86_64__) || defined(__i386__))
    uintptr_t registers = 0;
    vm_state state = vm_trap_probe(false, &registers);
    if (registers != 0) return 10;
    if (state == vm_unknown) return 100;
    return 0;
#else
    return 100;
#endif
}

#endif
