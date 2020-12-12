#pragma once

#include <stddef.h>
#include <stdint.h>

#include "defs.h"
#include "multiboot.h"

#define PT_PRESENT      (1 << 0)
#define PT_WRITEABLE    (1 << 1)
#define PT_USER         (1 << 2)
#define PT_PAGE_SIZE    (1 << 7)

#define PGDIR_IDX(addr) ((((uint32_t)addr) >> 22) & 1023)
#define PT_IDX(addr) ((((uint32_t)addr) >> 12) & 1023)

#define ROUNDUP(addr) ((void*)(((uint32_t)(addr) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1)))
#define ROUNDDOWN(addr) ((void*)((uint32_t)(addr) & ~(PAGE_SIZE - 1)))

extern char KERNEL_END[];
extern char KERNEL_START[];
extern char USERSPACE_START[];

extern const uint32_t MULTIBOOT_INFO_ADDR;

// simplified multiboot structure representing memory section
struct memory_map_section {
  uint32_t addr;
  uint32_t len;
  uint8_t type;
};

static inline void* virt2phys(void* addr) {
    return (void*)((uint32_t)addr - KERNEL_HIGH);
}

static inline void* phys2virt(void* addr) {
    return (void*)((uint32_t)addr + KERNEL_HIGH);
}

void init_kalloc_early();

void* kalloc();

void kfree(void* p);

void init_kernel_paging();

void identity_map(void* addr, size_t sz);

void fill_memory_map();

void print_memory_map();
