# antivm

Small header-only C++ library for Windows VM and debugger checks. CPU, SMBIOS, ACPI and firmware checks, plus TPM, boot, device and driver reports.

Returns `physical`, `virtual` or `unknown`. Failed checks stay unknown. Broken shit is not proof of a physical machine.

## compiling

Use C++17. Tested on Windows x64 and x86 with Zig 0.16.0. Copy `src` into your project and include `vm.hpp`.

Save this as `main.cpp`:

```cpp
#include <cstdio>
#include "vm.hpp"

int main() {
    vm_report report = vm_detect_report();
    const char *state = report.state == vm_virtual ? "virtual"
            : report.state == vm_physical ? "physical" : "unknown";
    std::printf("%s -> %s\n", state, vm_reason_name(report.reason));
    int debugger = vm_debugger_error();
    std::printf("debugger -> %d\n", debugger);
    return debugger != 0 ? debugger : vm_error(report.state);
}
```

Build an x64 executable:

```sh
zig c++ -target x86_64-windows-gnu -std=c++17 -O2 -fno-exceptions -fno-rtti -Isrc main.cpp -o probe.exe
```

`vm_detect_details()` keeps separate CPU, sandbox, trap and firmware results. `vm_collect_report()` adds debugger, TPM, boot and device reports; pass separate PCI and disk buffers with their capacities.

TPM and Secure Boot are diagnostic data, not VM verdicts. This will not catch every damn VM. Test it on your target machines.

## credits

Parts of the VM and debugger checks are adapted from [VMProtect3.5.1](https://github.com/morvayo/VMProtect3.5.1).

Firmware parsing follows the [DMTF SMBIOS specification](https://www.dmtf.org/standards/smbios) and [ACPI specification](https://uefi.org/specifications).
