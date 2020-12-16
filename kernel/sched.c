#include "sched.h"
#include "errno.h"
#include "irq.h"
#include "common.h"
#include "paging.h"
#include "panic.h"
#include "gdt.h"
#include "bug.h"
#include "init.h"

#define MAX_TASKS          256
#define KERNEL_STACK_SIZE  4096

static struct task tasks[MAX_TASKS];

// First function in task, will return to __jump_userspace.
static void task_init() {
    /*if (current->pid == 1) {
        init_late();
    }*/
}

extern void userspace_fn();
extern void __jump_userspace();

static int task_setup(int pid) {
  struct task* task = &tasks[pid - 1];
  task->pid = pid;
  task->state = TASK_RUNNING;
  task->kstack = kalloc();
  if (!task->kstack) {
    return -ENOMEM;
  }

  // Allocate fake interrupt frame.
  void* esp = task->kstack + KERNEL_STACK_SIZE;
  esp -= sizeof(struct regs);

  // Prepare registers.
  task->regs = esp;
  memset(task->regs, '\0', sizeof(*task->regs));
  memset(&task->context, '\0', sizeof(task->context));
  task->regs->cs = 0x1b;
  task->regs->ds = 0x23;
  task->regs->es = 0x23;
  task->regs->fs = 0x23;
  task->regs->gs = 0x23;
  task->regs->ss = 0x23;
  task->regs->eflags = 1 << 9; // Set IF flag.

  // Prepare kernel context.
  esp -= 4;
  *(uint32_t*)esp = (uint32_t)__jump_userspace;
  esp -= 4;
  *(uint32_t*)esp = (uint32_t)task_init;
  task->context.esp = (uint32_t)esp;
  task->context.ebp = (uint32_t)esp;

  //////////////////////////

  task->regs->esp = 0;

  task->pgdir = kalloc();
  copy_kernel_section(task->pgdir);

  task->mapped_mem_list = kalloc();
  task->mapped_mem_list->next = kalloc();
  task->mapped_mem_list->next->next = NULL;

  task->mapped_mem_list->addr = 0;
  task->mapped_mem_list->next->addr = PAGE_SIZE;

  task->mapped_mem_list->size = PAGE_SIZE;
  task->mapped_mem_list->next->size = PAGE_SIZE;

  uint32_t* stack = kalloc();

  map_continuous(task->pgdir, 0, PAGE_SIZE, virt2phys(stack),
                PT_USER | PT_WRITEABLE);
  map_continuous(task->pgdir, PAGE_SIZE, PAGE_SIZE, virt2phys(task->kstack),
                PT_WRITEABLE);

  //////////////////////////

  return 0;
}

int task_allocate(struct task** res) {
    for (size_t i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_NOT_ALLOCATED) {
            int err = task_setup(i + 1);
            if (err != 0) {
                return err;
            }
            *res = &tasks[i];
            return 0;
        }
    }
    return -EAGAIN;
}

extern void __switch_to(void* from, void* to);

static struct context scheduler_context;
struct task* current = NULL;

static void switch_to(struct task* task) {
  TSS.ss0 = 0x10;
  TSS.esp0 = task->kstack + KERNEL_STACK_SIZE;
  TSS.iopb = 0xffff;
  reload_tss();
  current = task;
  load_cr3(virt2phys(task->pgdir));
  __switch_to(&scheduler_context, &task->context);
}

void do_idle() {
    asm volatile ("hlt");
}

void scheduler_start() {
    // Interrupts are still disabled.
    struct task* init = NULL;
    if (task_allocate(&init) != 0) {
        panic("cannot allocate init task");
    }
    init->regs->eip = (uint32_t)userspace_fn;

    for (;;) {
        int found = 0;
        for (size_t i = 0; i < MAX_TASKS; i++) {
            if (tasks[i].state == TASK_RUNNING) {
                // We found running task, switch to it.
                //printk("%u ", i);
                switch_to(&tasks[i]);
                found = 1;
                // We've returned to the scheduler.
            }
        }

        if (!found) {
            // If we didn't found a runnable task, wait for next interrupt and retry scheduling.
            enable_irq();
            do_idle();
            disable_irq();
        }
    }
}

void scheduler_tick(struct regs* regs) {
    for (size_t i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_WAITING) {
            tasks[i].ticks_remaining--;
            if (tasks[i].ticks_remaining == 0) {
                tasks[i].state = TASK_RUNNING;
            }
        }
    }

    if (current) {
        reschedule();
    }
}

void reschedule() {
    BUG_ON_NULL(current);

    struct task* old = current;
    current = NULL;
    __switch_to(&old->context, &scheduler_context);
}
