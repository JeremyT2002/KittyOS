/* io.h -- x86 port I/O primitives.
 *
 * The PIC, PIT and VGA CRTC registers all live in the I/O address space, which
 * is only reachable with the IN/OUT instructions -- there is no memory-mapped
 * alternative on a PC.  These are static inline so no .c file is needed.
 */
#ifndef IO_H
#define IO_H

#include <stdint.h>

/* "a" = AL holds the byte, "Nd" = the port goes in DX, or as an 8-bit
 * immediate when it is a constant < 256. */
static inline void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile ("outb %0, %1" :: "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* Write to an unused port to burn ~1 microsecond.  Some legacy chips (the PIC
 * in particular) need a moment between consecutive command writes. */
static inline void io_wait(void)
{
    outb(0x80, 0);
}

#endif /* IO_H */
