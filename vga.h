/* vga.h -- 80x25 VGA text mode driver.
 *
 * Cell layout at 0xB8000 (2 bytes per cell, 80 columns x 25 rows):
 *   byte 0 : character code, code page 437
 *   byte 1 : attribute -- bits 0-3 foreground, bits 4-6 background,
 *                         bit 7 blink (or bright background, depending on how
 *                         the attribute controller is programmed)
 *
 * Everything is drawn into a back buffer and pushed to the real framebuffer in
 * one pass, so a frame is never seen half-drawn.
 */
#ifndef VGA_H
#define VGA_H

#include <stdint.h>

#define VGA_WIDTH  80
#define VGA_HEIGHT 25
#define VGA_CELLS  (VGA_WIDTH * VGA_HEIGHT)

/* Standard CGA/VGA 16-colour palette indices. */
enum vga_color {
    VGA_BLACK         = 0,
    VGA_BLUE          = 1,
    VGA_GREEN         = 2,
    VGA_CYAN          = 3,
    VGA_RED           = 4,
    VGA_MAGENTA       = 5,
    VGA_BROWN         = 6,
    VGA_LIGHT_GREY    = 7,
    VGA_DARK_GREY     = 8,
    VGA_LIGHT_BLUE    = 9,
    VGA_LIGHT_GREEN   = 10,
    VGA_LIGHT_CYAN    = 11,
    VGA_LIGHT_RED     = 12,
    VGA_LIGHT_MAGENTA = 13,
    VGA_YELLOW        = 14,
    VGA_WHITE         = 15,
};

/* Background occupies bits 4-6; bit 7 is left clear so nothing blinks. */
static inline uint8_t vga_attr(uint8_t fg, uint8_t bg)
{
    return (uint8_t)((fg & 0x0F) | ((bg & 0x07) << 4));
}

/* Fill the whole back buffer with `ch`/`attr`. */
void vga_clear(char ch, uint8_t attr);

/* Write one cell into the back buffer.  Coordinates outside the screen are
 * ignored, which lets the animation code draw partially off-screen sprites
 * without doing its own clipping. */
void vga_put(int x, int y, unsigned char ch, uint8_t attr);

/* Write one cell as TWO stacked pixels, doubling the vertical resolution.
 *
 * A cell is 9x16 pixels on screen, and CP437 has half blocks: 0xDF fills the
 * top half, 0xDC the bottom, 0xDB both.  Since the foreground and background
 * of a cell are coloured independently, one cell can therefore show two
 * differently coloured square-ish pixels.
 *
 * The catch is that the background field is only 3 bits, so colours 8..15 are
 * unavailable there.  This picks whichever half block puts the unrepresentable
 * colour in the foreground; only if BOTH halves need a bright colour does it
 * have to approximate, by dimming the bottom one.
 */
void vga_put_half(int x, int y, uint8_t top, uint8_t bottom);

/* Copy the back buffer to 0xB8000 in one pass. */
void vga_present(void);

/* Turn off the blinking hardware text cursor. */
void vga_hide_cursor(void);

#endif /* VGA_H */
