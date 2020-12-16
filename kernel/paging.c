#include "paging.h"
#include "common.h"
#include "panic.h"
#include "irq.h"

#include <stdint.h>

uint32_t early_pgdir[1024] __attribute__((aligned(4096), section(".boot.bss")));

struct fl_entry {
    struct fl_entry* next;
};

struct kalloc_head {
    struct fl_entry* freelist_head;
};

struct kalloc_head kalloc_head;

__attribute__((section(".boot.text"))) void setup_paging_early() {
    // 0x0...4Mb identity mapped
    // KERNEL_HIGH...KERNEL_HIGH+4Mb mapped to 0x0...4Mb
    early_pgdir[0] = PT_PRESENT | PT_WRITEABLE | PT_PAGE_SIZE; // (0) | ...
    early_pgdir[PGDIR_IDX(KERNEL_HIGH)] = PT_PRESENT | PT_WRITEABLE | PT_PAGE_SIZE;
}

void init_kalloc_early() {
    void* addr = (void*)ROUNDUP((uint32_t)&KERNEL_END);
    kalloc_head.freelist_head = NULL;
    void* end = KERNEL_HIGH + 4 * (1 << 20);
    while (addr < end) {
        struct fl_entry* entry = (struct fl_entry*)addr;
        entry->next = kalloc_head.freelist_head;
        kalloc_head.freelist_head = entry;
        addr += PAGE_SIZE;
    }
}

void* kalloc() {
    // TODO: spinlock needed here.

    if (!kalloc_head.freelist_head) {
        return NULL;
    }

    void* ptr = kalloc_head.freelist_head;
    kalloc_head.freelist_head = kalloc_head.freelist_head->next;
    return ptr;
}

void kfree(void* p) {
    // ...
}

uint32_t* alloc_page(uint32_t* pgdir, void* addr, /*int user,*/ int fl) {
    uint32_t* page_table = NULL;
    if (pgdir[PGDIR_IDX(addr)] & PT_PRESENT) {
        page_table = phys2virt((void*)ROUNDDOWN(pgdir[PGDIR_IDX(addr)]));
    } else {
        page_table = kalloc();
        if (!page_table) {
            return NULL;
        }
        for (int i = 0; i < 1024; i++) {
            page_table[i] = 0;
        }
    }

    int flags = PT_PRESENT;
    if (fl) {
        flags |= fl;
    }
    pgdir[PGDIR_IDX(addr)] = ((uint32_t)virt2phys(page_table)) | flags;
    return &page_table[PT_IDX(addr)];
}

void map_continuous(uint32_t* pgdir, void* addr, size_t size, void* phys_addr, int flags) {
    addr = (void*)ROUNDDOWN((uint32_t)addr);
    phys_addr = (void*)ROUNDDOWN((uint32_t)phys_addr);
    size = ROUNDUP(size);

    while (size > 0) {
        uint32_t* pte = alloc_page(pgdir, addr, flags & PT_USER);
        *pte = ((uint32_t)phys_addr) | PT_PRESENT;
        *pte |= flags;
        addr += PAGE_SIZE;
        phys_addr += PAGE_SIZE;
        size -= PAGE_SIZE;
    }
}

uint32_t* kernel_pgdir = NULL;

void load_cr3(uint32_t* pgdir) {
    asm volatile (
        "mov %0, %%cr3\n"
        :
        : "r"(pgdir)
        : "memory"
    );
}

void init_kernel_paging() {
  kernel_pgdir = kalloc();
  memset(kernel_pgdir, '\0', PAGE_SIZE);
  extend_phys_memory_mapping();
  //map_continuous(kernel_pgdir, (void*)KERNEL_HIGH, 4 * (1 << 20), 0x0, PT_WRITEABLE);
  //map_continuous(kernel_pgdir, &USERSPACE_START, 4096, virt2phys(&USERSPACE_START), PT_USER | PT_WRITEABLE);
  //load_cr3(virt2phys(kernel_pgdir));
}

void identity_map(void* addr, size_t sz) {
    map_continuous(kernel_pgdir, addr, sz, addr, PT_WRITEABLE);
}

uint32_t read_cr2() {
    uint32_t val = 0;
    asm volatile (
        "mov %%cr2, %0\n"
        : "=r"(val)
    );
    return val;
}

void pagefault_irq(struct regs* regs) {
    (void)regs;

    void* addr = (void*)read_cr2();

    if (!is_userspace(regs)) {
        panic("pagefault in kernel space\n    cr2=0x%x, eip=0x%x, err_code=%d", addr, regs->eip, regs->error_code);
    }
    printk("%u ", addr);
}

static struct memory_map_section MEMORY_MAP[32];
uint32_t memory_map_section_counter = 0;

void fill_memory_map() {

  multiboot_info_t* mboot_header = (multiboot_info_t *)MULTIBOOT_INFO_ADDR;

  unsigned long cur_addr = mboot_header->mmap_addr;
  unsigned long end_addr = cur_addr + mboot_header->mmap_length;

  while (cur_addr < end_addr && memory_map_section_counter < 32) {
    multiboot_memory_map_t* cur_entry = (multiboot_memory_map_t*)cur_addr;

    struct memory_map_section cur_section;
    cur_section.addr = cur_entry->addr;
    cur_section.len  = cur_entry->len;
    cur_section.type = cur_entry->type;

    MEMORY_MAP[memory_map_section_counter] = cur_section;
    ++memory_map_section_counter;

    // it seems like multiboot_memory_map_t.size does not represent the actual
    // size of the structure, so will use sizeof instead
    cur_addr += sizeof(multiboot_memory_map_t);
  }
}

void print_memory_map() {

  printk("____________________________\n"
         "BASE ADDRESS | LENGTH | TYPE\n"
         "----------------------------\n");

  for (int index = 0; index < memory_map_section_counter; ++index) {
    printk("0x%x | %u | ", MEMORY_MAP[index].addr, MEMORY_MAP[index].len);

    switch(MEMORY_MAP[index].type) {
    case 1:
      printk("AVAILABLE\n");
      break;
    case 2:
      printk("RESERVED\n");
      break;
    case 3:
      printk("ACPI RECLAIMABLE\n");
      break;
    case 4:
      printk("NVS\n");
      break;
    case 5:
      printk("BADRAM\n");
      break;
    }
  }

  printk("----------------------------\n");
}

void map_continuous_large_page(uint32_t* pgdir, void* addr, size_t size,
                               void* phys_addr, int flags) {

  addr = (void*)LARGE_ROUNDDOWN((uint32_t)addr);
  phys_addr = (void*)LARGE_ROUNDDOWN((uint32_t)phys_addr);
  size = LARGE_ROUNDUP(size);

  while (size > 0) {
    pgdir[PGDIR_IDX(addr)] = ((uint32_t )phys_addr) | PT_PRESENT | PT_PAGE_SIZE | flags;

    addr += LARGE_PAGE_SIZE;
    phys_addr += LARGE_PAGE_SIZE;
    size -= LARGE_PAGE_SIZE;
  }
}

void extend_phys_memory_mapping() {

  if (memory_map_section_counter == 0) {
    return;
  }

  uint32_t section_addr = 0, section_len = 0;

  for (int index = 0; index < memory_map_section_counter; ++index) {
    if (MEMORY_MAP[index].type == MULTIBOOT_MEMORY_AVAILABLE &&
        MEMORY_MAP[index].addr != 0) {

      section_addr = MEMORY_MAP[index].addr;
      section_len  = MEMORY_MAP[index].len;
      break;

    }
  }

  uint32_t map_size = (uint32_t)LARGE_ROUNDDOWN(section_addr + section_len);
  uint32_t available_virtual_memory = (uint32_t)LARGE_ROUNDDOWN(UINT32_MAX - KERNEL_HIGH);
  available_virtual_memory -= 4 * LARGE_PAGE_SIZE;
  if (available_virtual_memory < map_size) {
    map_size = available_virtual_memory;
  }

  map_continuous_large_page(kernel_pgdir, (void*)KERNEL_HIGH, map_size, 0x0, PT_WRITEABLE | PT_USER);

  load_cr3(virt2phys(kernel_pgdir));

  uint32_t kalloc_size = map_size - LARGE_PAGE_SIZE;
  uint32_t available_kalloc_space = (uint32_t)LARGE_ROUNDDOWN(UINT32_MAX - (uint32_t)&KERNEL_END);
  available_kalloc_space -= 4 * LARGE_PAGE_SIZE;
  if (available_kalloc_space < map_size) {
    kalloc_size = available_kalloc_space;
  }

  void* addr = (void*)LARGE_ROUNDUP((uint32_t)&KERNEL_END);
  void* end = addr + kalloc_size;
  while (addr < end) {
    struct fl_entry *entry = (struct fl_entry *)addr;
    entry->next = kalloc_head.freelist_head;
    kalloc_head.freelist_head = entry;
    addr += PAGE_SIZE;
  }
}

void extend_mapping_test() {

  printk("Testing extended mapping of available physical memory:\n");
  printk("First kalloc(), value 11:\n");
  uint32_t* test = kalloc();
  test[5] = 11;
  printk("value - %d, ", test[5]);
  printk("virt. addr. - %u, ", test);
  printk("phys. addr. - %u.\n", virt2phys(test));

  printk("Next kalloc(), value 10:\n");
  test = kalloc();
  test[5] = 10;
  printk("value - %d, ", test[5]);
  printk("virt. addr. - %u, ", test);
  printk("phys. addr. - %u.\n", virt2phys(test));

}

void copy_kernel_section(uint32_t* pgdir) {
  memset(pgdir, 0, 512 * sizeof(uint32_t));
  for (uint32_t i = 512; i < 1024; ++i) {
    pgdir[i] = kernel_pgdir[i];
  }
}

void* kalloc_s(size_t size) {
  size = ROUNDUP(size);
  struct fl_entry* entry = kalloc_head.freelist_head;
  for (uint32_t index = 1; index < size / PAGE_SIZE; ++index) {
    entry = entry->next;
  }
  void* ptr = (void*)kalloc_head.freelist_head;
  kalloc_head.freelist_head = entry->next;
  return ptr;
}

void* mmap(void* addr, size_t size, int flags, uint32_t* pgdir) {
  void* k_addr = kalloc_s(size);
  map_continuous(pgdir, addr, size, virt2phys(k_addr), flags);
  return addr;
}
