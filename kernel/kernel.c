#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "idt.h"
#include "gdt.h"
#include "acpi.h"
#include "apic.h"
#include "irq.h"
#include "vga.h"
#include "panic.h"
#include "paging.h"
#include "sched.h"
#include "pci.h"
#include "ata.h"

void kernel_main() {
  init_gdt();
  init_idt();

  fill_memory_map();

  init_kalloc_early();
  init_kernel_paging();

  terminal_initialize();
  print_memory_map();

  struct acpi_sdt *rsdt = acpi_find_rsdt();
  if (!rsdt) {
    panic("RSDT not found!");
  }

  apic_init(rsdt);
  // ata_init();

  extend_mapping_test();

  printk("\nHell OS is loaded\n");

  scheduler_start();
}
