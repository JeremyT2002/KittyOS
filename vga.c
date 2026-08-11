/* vga.c -- 80x25 text mode driver with a back buffer. */

#include "vga.h"
#include "io.h"

/* The framebuffer is a hardware window, not ordinary RAM: the compiler must not
 * cache, reorder away or elide these stores, hence volatile. */
static volatile uint16_t *const vga_fb = (volatile uint16_t *)0xB8000;

/* One frame's worth of cells.  Rendering happens here; only vga_present()
 * touches the hardware, so the display never shows a partial frame. */
static uint16_t back[VGA_CELLS];

void vga_clear(char ch, uint8_t attr)
{
    uint16_t cell = (uint16_t)((unsigned char)ch | ((uint16_t)attr << 8));
    for (int i = 0; i < VGA_CELLS; i++)
        back[i] = cell;
}

void vga_put(int x, int y, unsigned char ch, uint8_t attr)
{
    if (x < 0 || x >= VGA_WIDTH || y < 0 || y >= VGA_HEIGHT)
        return;                                  /* silent clip */
    back[y * VGA_WIDTH + x] = (uint16_t)(ch | ((uint16_t)attr << 8));
}

void vga_put_half(int x, int y, uint8_t top, uint8_t bottom)
{
    if (top == bottom) {
        /* One solid colour: a full block, background never shows. */
        vga_put(x, y, 0xDB, vga_attr(top, 0));
    } else if (bottom < 8) {
        /* Upper half block: foreground paints the top, background the bottom. */
        vga_put(x, y, 0xDF, vga_attr(top, bottom));
    } else if (top < 8) {
        /* Bottom half needs a bright colour, so flip it: lower half block puts
         * the bottom colour in the foreground instead. */
        vga_put(x, y, 0xDC, vga_attr(bottom, top));
    } else {
        /* Both halves bright.  Unrepresentable exactly -- keep the top exact
         * and dim the bottom to its dark twin. */
        vga_put(x, y, 0xDF, vga_attr(top, (uint8_t)(bottom & 0x07)));
    }
}

void vga_present(void)
{
    for (int i = 0; i < VGA_CELLS; i++)
        vga_fb[i] = back[i];
}

void vga_hide_cursor(void)
{
    /* The VGA CRT controller is accessed through an index/data port pair:
     * write the register number to 0x3D4, then read/write it at 0x3D5.
     * Register 0x0A is "cursor start"; bit 5 disables cursor display. */
    outb(0x3D4, 0x0A);
    outb(0x3D5, 0x20);
}
