/* splash.c -- boot splash drawn from Braille art.
 *
 * The original art was Unicode Braille (U+2800..U+28FF), which VGA text mode
 * cannot display at all: the hardware font is code page 437 and has no Braille
 * glyphs.  But a Braille character is nothing more than a 2x4 dot bitmap, so the
 * art was decoded into a plain bitmap and re-encoded for the glyphs we DO have.
 *
 * Each text cell here carries two bitmap rows, using the CP437 half blocks:
 *
 *     '#'  both rows set     -> 0xDB full block
 *     '^'  top row set       -> 0xDF upper half block
 *     '_'  bottom row set    -> 0xDC lower half block
 *     ' '  neither           -> not drawn
 *
 * The 30x11 Braille grid is a 60x44 dot bitmap, which after trimming the empty
 * border becomes the 60x20 cell picture below -- comfortably inside 80x25.
 */

#include "splash.h"
#include "vga.h"

#define SPLASH_W 60
#define SPLASH_H 20

/* Centred: (80 - 60) / 2 and (25 - 20) / 2. */
#define SPLASH_X 10
#define SPLASH_Y 2

static const char *const splash_art[SPLASH_H] = {
    "             _______##########################____          ",
    "            _#^^                                 ^#         ",
    "            ##                    __         _    #         ",
    "            ##  ^    #^                      ^    #         ",
    "            ##                    ___             #         ",
    "            ##                 # _#^^^#_   _     _#___###   ",
    "______      ##          ^      ^ ##    ^#_ ^     ###^^  ##  ",
    "##   ^^#__  ##                   #       ^#_____##^     ##  ",
    " #_      ^# ##               ^   #                      ##  ",
    "  ##      ^###       ^^        _##     _                 ## ",
    "   ^#_      ##             #^ _#^    _#  #        _# ^#   ^#",
    "     ^##_   ##      _      ^  #^     ^####    _   ^####    #",
    "        ^#__##     ##         ##             ^^#^          #",
    "          ^^##                ^#_       _     _#          _#",
    "            ##       #     #   ^#_       ^^# ^^ ^^_^^    _#^",
    "            ##_                  ##___              ____#^  ",
    "           _#^^^^################^##^^^^^######^^##^^^^     ",
    "           #   _#^ ##    #^       ^#    _#  _#    #         ",
    "           ^###^   ^#__#^^         ^#__#^   ^#_ _#          ",
    "                                              ^^^           ",
};

static const char title[] = "K I T T Y O S";

void splash_draw(void)
{
    uint8_t bg   = VGA_BLUE;
    uint8_t ink  = vga_attr(VGA_WHITE, bg);
    uint8_t name = vga_attr(VGA_LIGHT_MAGENTA, bg);

    vga_clear(' ', vga_attr(VGA_LIGHT_GREY, bg));

    for (int r = 0; r < SPLASH_H; r++) {
        for (int c = 0; c < SPLASH_W; c++) {
            unsigned char ch;

            switch (splash_art[r][c]) {
            case '#': ch = 0xDB; break;     /* full block        */
            case '^': ch = 0xDF; break;     /* upper half block  */
            case '_': ch = 0xDC; break;     /* lower half block  */
            default:  continue;             /* transparent       */
            }
            vga_put(SPLASH_X + c, SPLASH_Y + r, ch, ink);
        }
    }

    /* Caption, centred under the cat. */
    int len = (int)(sizeof(title) - 1);
    int x   = (VGA_WIDTH - len) / 2;
    for (int i = 0; i < len; i++)
        vga_put(x + i, SPLASH_Y + SPLASH_H + 1, (unsigned char)title[i], name);
}
