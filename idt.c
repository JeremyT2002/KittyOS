/* idt.c -- 64-bit IDT construction and 8259A PIC remapping. */

#include <stdint.h>
#include "idt.h"
#include "io.h"

#define IDT_ENTRIES 256

/* Selector of the 64-bit code segment in the GDT built by boot.asm:
 * null descriptor (0x00), then code (0x08), then data (0x10). */
#define KERNEL_CS 0x08

/* Gate type byte: P=1 (present), DPL=00, S=0, type=0xE (64-bit interrupt gate).
 * An interrupt gate clears IF on entry, so handlers are not re-entered. */
#define GATE_INTERRUPT 0x8E

/* A 64-bit gate descriptor is 16 bytes and the handler address is split across
 * three separate fields -- a legacy of extending the old 8-byte 32-bit gate. */
struct idt_entry {
    uint16_t offset_low;    /* handler address bits  0..15 */
    uint16_t selector;      /* code segment selector       */
    uint8_t  ist;           /* interrupt stack table index, 0 = don't switch */
    uint8_t  type_attr;     /* gate type / DPL / present   */
    uint16_t offset_mid;    /* handler address bits 16..31 */
    uint32_t offset_high;   /* handler address bits 32..63 */
    uint32_t zero;          /* reserved, must be 0         */
} __attribute__((packed));

/* Operand for LIDT: 2-byte limit followed by the 8-byte base. */
struct idt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

static struct idt_entry idt[IDT_ENTRIES];
static struct idt_ptr   idtr;

/* Assembly stubs (boot.asm). */
extern void irq0_stub(void);
extern void isr_default_stub(void);

static void idt_set_gate(int vector, void (*handler)(void))
{
    uint64_t addr = (uint64_t)handler;

    idt[vector].offset_low  = (uint16_t)(addr & 0xFFFF);
    idt[vector].selector    = KERNEL_CS;
    idt[vector].ist         = 0;
    idt[vector].type_attr   = GATE_INTERRUPT;
    idt[vector].offset_mid  = (uint16_t)((addr >> 16) & 0xFFFF);
    idt[vector].offset_high = (uint32_t)(addr >> 32);
    idt[vector].zero        = 0;
}

/* --- 8259A PIC ------------------------------------------------------------ */

#define PIC1_CMD  0x20      /* master: command port */
#define PIC1_DATA 0x21      /* master: data / mask port */
#define PIC2_CMD  0xA0      /* slave:  command port */
#define PIC2_DATA 0xA1      /* slave:  data / mask port */

#define ICW1_INIT 0x11      /* start init sequence, expect ICW4 */
#define ICW4_8086 0x01      /* 8086/88 mode (not the ancient MCS-80 mode) */

/* On a bare PC the PICs deliver IRQ0..7 on vectors 0x08..0x0F, which collide
 * with the CPU's own exception vectors (0x08 = double fault, 0x0D = #GP ...).
 * In protected/long mode that is unusable, so the PICs must be told to use
 * vectors 0x20 (master) and 0x28 (slave) instead. */
static void pic_remap(void)
{
    outb(PIC1_CMD, ICW1_INIT);   io_wait();
    outb(PIC2_CMD, ICW1_INIT);   io_wait();

    outb(PIC1_DATA, 0x20);       io_wait();  /* ICW2: master vector offset */
    outb(PIC2_DATA, 0x28);       io_wait();  /* ICW2: slave vector offset  */

    outb(PIC1_DATA, 0x04);       io_wait();  /* ICW3: slave is on IRQ line 2
                                              *       (bitmask, bit 2)      */
    outb(PIC2_DATA, 0x02);       io_wait();  /* ICW3: slave's cascade id = 2 */

    outb(PIC1_DATA, ICW4_8086);  io_wait();
    outb(PIC2_DATA, ICW4_8086);  io_wait();

    /* Interrupt mask registers: a set bit MASKS that line.
     * 0xFE = 1111_1110 -> only IRQ0 (the PIT) is allowed through.
     * 0xFF = every slave line masked; nothing behind the cascade can fire. */
    outb(PIC1_DATA, 0xFE);
    outb(PIC2_DATA, 0xFF);
}

/* --- public --------------------------------------------------------------- */

void idt_init(void)
{
    /* Point every vector at the catch-all stub first.  A gate that is not
     * present turns any stray interrupt into #GP and then, with no #GP handler,
     * into a triple fault and a reboot loop -- much harder to diagnose than the
     * red '!' the default stub paints. */
    for (int i = 0; i < IDT_ENTRIES; i++)
        idt_set_gate(i, isr_default_stub);

    /* Vector 0x20 = IRQ0 = PIT, after the remap below. */
    idt_set_gate(0x20, irq0_stub);

    pic_remap();

    idtr.limit = (uint16_t)(sizeof(idt) - 1);
    idtr.base  = (uint64_t)&idt[0];
    __asm__ volatile ("lidt %0" :: "m"(idtr));
}
