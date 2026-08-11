/* pit.h -- 8253/8254 programmable interval timer, channel 0. */
#ifndef KITTYOS_PIT_H
#define KITTYOS_PIT_H

#include <stdint.h>

#define PIT_HZ 100      /* interrupts per second */

/* Program channel 0 for a periodic PIT_HZ interrupt on IRQ0. */
void pit_init(void);

/* Called from the assembly IRQ0 stub in boot.asm. */
void pit_tick(void);

/* Monotonic tick count since pit_init(). */
uint64_t pit_ticks(void);

#endif /* KITTYOS_PIT_H */
