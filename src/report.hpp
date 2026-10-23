#ifndef VM_DIAGNOSTIC_REPORT_HPP
#define VM_DIAGNOSTIC_REPORT_HPP

#include "virtual/virtual.hpp"
#include "debug/debugger.hpp"
#include "physical/tpm.hpp"
#include "physical/devices.hpp"
#include "physical/boot.hpp"

struct vm_diagnostic_report {
    vm_detection_details detection;
    vm_debug_report debugger;
    vm_tpm_report tpm;
    vm_device_report pci;
    vm_device_report disks;
    vm_boot_report boot;
};

static vm_diagnostic_report vm_collect_report(vm_device_info *pci, size_t pci_capacity,
        vm_device_info *disks, size_t disk_capacity) {
    vm_diagnostic_report report;
    report.detection = vm_detect_details();
    report.debugger = vm_debugger_report();
    report.tpm = vm_tpm_query();
    report.pci = vm_devices_query(vm_device_pci, pci, pci_capacity);
    report.disks = vm_devices_query(vm_device_disk, disks, disk_capacity);
    report.boot = vm_boot_query();
    return report;
}

#endif
