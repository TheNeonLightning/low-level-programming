#pragma once

#include <stdint.h>
#include <stddef.h>

typedef enum state {
    TASK_NOT_ALLOCATED = 0,
    TASK_RUNNING       = 1,
    TASK_WAITING       = 2,
    TASK_DISK_READ     = 3,
} state_t;

struct context {
    uint32_t esp;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;
    uint32_t ebx;
};

struct mapped_mem {
  struct mapped_mem* next;
  uint32_t addr;
  size_t size;
};

struct task {
    int pid;
    state_t state;
    struct context context;
    struct regs* regs;
    void* kstack;

    uint32_t* pgdir;
    struct mapped_mem* mapped_mem_list;
    size_t list_size;

    int ticks_remaining;
    int exitcode;
};

extern struct task* current;

struct task* task_create();
void scheduler_start();
void scheduler_tick(struct regs* regs);
void reschedule();
