/* pit.c -- 8253/8254 PIT channel 0 at 100 Hz. */

#include "pit.h"
#include "io.h"

#define PIT_CH0_DATA 0x40   /* channel 0 data port    */
#define PIT_COMMAND  0x43   /* mode/command register  */

/* The PIT is driven by a fixed 1.193182 MHz crystal; the output frequency is
 * that divided by a 16-bit divisor.  1193182 / 100 = 11931.82, i.e. 11932. */
#define PIT_DIVISOR 11932

/* Written by the interrupt handler, read by the main loop: volatile, otherwise
 * the compiler is entitled to hoist the read out of the wait loop and spin
 * forever on a stale value. */
static volatile uint64_t ticks;

void pit_init(void)
{
    /* Command byte 0x36 = 0b00_11_011_0:
     *   bits 6-7 = 00  -> channel 0
     *   bits 4-5 = 11  -> access mode: write low byte then high byte
     *   bits 1-3 = 011 -> mode 3, square wave generator (periodic)
     *   bit 0    = 0   -> 16-bit binary counting, not BCD
     */
    outb(PIT_COMMAND, 0x36);
    outb(PIT_CH0_DATA, (uint8_t)(PIT_DIVISOR & 0xFF));         /* low byte  */
    outb(PIT_CH0_DATA, (uint8_t)((PIT_DIVISOR >> 8) & 0xFF));  /* high byte */
}

void pit_tick(void)
{
    ticks++;
}

uint64_t pit_ticks(void)
{
    return ticks;
}
