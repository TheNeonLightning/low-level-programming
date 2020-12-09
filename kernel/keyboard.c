#include "irq.h"
#include "vga.h"

void keyboard_irq(struct regs* regs) {
    (void)regs;
    printk("pepega");
    apic_eoi();
}
