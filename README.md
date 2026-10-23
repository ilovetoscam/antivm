# antivm

small header only c++ library for checking vms and debuggers on windows

checks cpu and firmware data including smbios and acpi
also reports tpm and boot state along with pci devices disks and driver details

results are physical virtual or unknown
failed checks stay unknown because broken shit proves nothing
tpm and secure boot do not decide whether a machine is virtual
some vms will get through so test it on your own setup

## compiling

use a c++17 compiler and add src to your include path
include the [vm header](src/vm.hpp) from your own program
tested with zig on windows x64 and x86
