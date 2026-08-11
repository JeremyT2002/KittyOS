/* idt.h -- 64-bit Interrupt Descriptor Table and 8259 PIC remap. */
#ifndef KITTYOS_IDT_H
#define KITTYOS_IDT_H

/* Build the IDT, remap the PIC so IRQ0..15 land on vectors 0x20..0x2F, mask
 * every IRQ except IRQ0 (the timer), and load the table with LIDT.
 * Interrupts are still disabled on return -- the caller does STI. */
void idt_init(void);

#endif /* KITTYOS_IDT_H */
