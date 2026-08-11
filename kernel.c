/* kernel.c -- KittyOS entry point and animation loop.
 *
 * By the time kernel_main runs, boot.asm has already put the CPU in 64-bit long
 * mode with the first 1 GiB identity-mapped and a 64-bit GDT loaded.  All this
 * file does is bring up interrupts and the timer, then draw frames forever.
 */

#include <stdint.h>
#include <stddef.h>
#include "vga.h"
#include "nyan.h"
#include "splash.h"
#include "speaker.h"
#include "idt.h"
#include "pit.h"

/* PIT ticks between animation frames.  At the programmed 100 Hz this is
 * 12.5 frames per second, which is about right for a 4-pose cycle. */
#define TICKS_PER_FRAME 8

/* Freestanding C still lets GCC emit calls to memset/memcpy for things like
 * struct assignment or array initialisation, so we must provide them ourselves;
 * there is no libc to link against. */
void *memset(void *dst, int c, size_t n)
{
    unsigned char *p = (unsigned char *)dst;
    while (n--)
        *p++ = (unsigned char)c;
    return dst;
}

void *memcpy(void *dst, const void *src, size_t n)
{
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    while (n--)
        *d++ = *s++;
    return dst;
}

void kernel_main(void)
{
    vga_hide_cursor();

    idt_init();     /* IDT built and loaded, PIC remapped, only IRQ0 unmasked */
    pit_init();     /* channel 0 free-running at 100 Hz -> IRQ0 */

    /* STI only now.  Enabling interrupts before the IDT is loaded means the
     * first timer tick vectors through a garbage descriptor: #GP with no
     * handler, then double fault with no handler, then triple fault -- which
     * looks like QEMU rebooting in a loop for no reason. */
    __asm__ volatile ("sti");

    /* Boot splash: one static frame, held for SPLASH_TICKS.  This is the only
     * place the timer is used for anything but the animation. */
    splash_draw();
    vga_present();
    {
        uint64_t until = pit_ticks() + SPLASH_TICKS;
        while (pit_ticks() < until) {
            __asm__ volatile ("hlt");
            speaker_update(pit_ticks());    /* the tune starts on the splash */
        }
    }

    uint32_t frame = 0;
    uint64_t next  = pit_ticks() + TICKS_PER_FRAME;

    for (;;) {
        /* Sleep until the timer says the next frame is due.  HLT parks the CPU
         * until the next interrupt instead of spinning; the loop re-checks
         * because any interrupt wakes it, not only the one we want.
         *
         * The melody is advanced inside this wait rather than once per frame:
         * a frame is 8 ticks, so note lengths would otherwise be quantised to
         * multiples of 80 ms and the tune would limp. */
        while (pit_ticks() < next) {
            __asm__ volatile ("hlt");
            speaker_update(pit_ticks());
        }

        next += TICKS_PER_FRAME;

        /* If we ever fall a whole frame behind (a very slow host, say), resync
         * rather than trying to render a backlog we can never catch up on. */
        if (pit_ticks() > next)
            next = pit_ticks() + TICKS_PER_FRAME;

        /* frame is uint32_t and every use of it inside the renderer is taken
         * modulo a power of two that divides 2^32, so wrapping after ~4 billion
         * frames is seamless -- there is no state that can overflow. */
        nyan_render(frame++);
        vga_present();
    }
}
