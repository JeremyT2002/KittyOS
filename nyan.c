/* nyan.c -- flying cat with a wavy rainbow trail.
 *
 * The cat is not hand-drawn character art: it is a 30x20 PIXEL sprite (see
 * nyan_art.h, generated from Braille art by tools/braille2sprite.py) rendered
 * two pixels per character cell using CP437 half blocks.  So the whole scene is
 * composed in a half-row plane first -- one colour per pixel -- and only then
 * folded down into 80x25 cells:
 *
 *     starfield (whole cells)
 *        -> half-row plane: rainbow trail, then cat on top
 *        -> fold pairs of half-rows into cells via vga_put_half()
 *
 * Working in pixels rather than cells is what lets the trail wave in steps of
 * half a cell and the cat bob by half a cell, both of which look far smoother
 * than whole-cell motion.
 */

#include "nyan.h"
#include "vga.h"
#include "nyan_art.h"

/* --- geometry ------------------------------------------------------------- */

#define HALF_ROWS (VGA_HEIGHT * 2)      /* 50 pixel rows on a 25 row screen */

#define CAT_X          50               /* fixed left column of the sprite  */
#define CAT_TOP_HALF   14               /* top pixel row (cell row 7)       */

#define TRAIL_BANDS      6
#define BAND_HALF_ROWS   2              /* each band is one cell tall       */
#define TRAIL_TOP_HALF   (CAT_TOP_HALF + 2)
#define TRAIL_RIGHT      (CAT_X + 1)    /* first column the cat covers      */

#define BG_COLOR    VGA_BLUE            /* dark blue "space"                */
#define TRANSPARENT 0xFF                /* no pixel drawn here              */

/* Rainbow bands, top to bottom. */
static const uint8_t trail_colors[TRAIL_BANDS] = {
    VGA_RED,        /*  4 */
    VGA_BROWN,      /*  6 -- renders as orange-brown */
    VGA_YELLOW,     /* 14 */
    VGA_GREEN,      /*  2 */
    VGA_CYAN,       /*  3 */
    VGA_MAGENTA,    /*  5 */
};

/* Sawtooth: two columns flat, two columns one pixel lower, repeating. */
static const int saw[4] = { 0, 0, 1, 1 };

/* The pixel plane the scene is composed in.  One byte per pixel: a VGA colour
 * index, or TRANSPARENT.  80 * 50 = 4000 bytes of .bss. */
static uint8_t plane[HALF_ROWS][VGA_WIDTH];

/* --- starfield ------------------------------------------------------------ */

struct star { uint8_t x, y; unsigned char ch; };

/* Fixed positions -- the stars do not move, only their brightness changes. */
static const struct star stars[] = {
    {  3,  1, '*' }, { 11,  4, '.' }, { 19,  2, '*' }, { 27,  6, '.' },
    {  6, 20, '.' }, { 14, 22, '*' }, { 22, 19, '.' }, { 31, 23, '*' },
    { 38,  3, '*' }, { 45,  7, '.' }, { 52,  1, '.' }, { 60,  4, '*' },
    { 68,  2, '.' }, { 75,  5, '*' }, { 41, 21, '.' }, { 49, 23, '*' },
    { 57, 20, '.' }, { 66, 22, '*' }, { 73, 19, '.' }, { 35, 22, '.' },
};

#define STAR_COUNT ((int)(sizeof(stars) / sizeof(stars[0])))

/* --- helpers -------------------------------------------------------------- */

/* Sprite colour key -> VGA colour.  Kept here rather than in the generated
 * header so the palette can be retuned without regenerating the art. */
static uint8_t key_color(char key)
{
    switch (key) {
    case 'k': return VGA_BLACK;         /* outline                 */
    case 'p': return VGA_MAGENTA;       /* pop-tart filling        */
    case 'g': return VGA_LIGHT_GREY;    /* fur                     */
    case 's': return VGA_RED;           /* sprinkle                */
    default:  return TRANSPARENT;
    }
}

static void plane_clear(void)
{
    for (int y = 0; y < HALF_ROWS; y++)
        for (int x = 0; x < VGA_WIDTH; x++)
            plane[y][x] = TRANSPARENT;
}

static void plane_set(int x, int y, uint8_t color)
{
    if (x < 0 || x >= VGA_WIDTH || y < 0 || y >= HALF_ROWS)
        return;                                  /* silent clip */
    plane[y][x] = color;
}

static void draw_stars(uint32_t tick)
{
    /* Brightness flips every 4 frames; the +i staggers neighbouring stars so
     * they do not all pulse in lockstep. */
    for (int i = 0; i < STAR_COUNT; i++) {
        int bright = (int)(((tick >> 2) + (uint32_t)i) & 1u);
        uint8_t fg = bright ? VGA_WHITE : VGA_DARK_GREY;
        vga_put(stars[i].x, stars[i].y, stars[i].ch, vga_attr(fg, BG_COLOR));
    }
}

static void draw_trail(uint32_t tick)
{
    /* Sampling the sawtooth at (x + tick) slides the pattern one column left
     * per frame, so the trail streams away from the cat.  Unsigned arithmetic
     * and a mask of 3: tick wraps at 2^32, which is a multiple of 4, so the
     * pattern wraps without a seam. */
    for (int x = 0; x < TRAIL_RIGHT; x++) {
        int offset = saw[((uint32_t)x + tick) & 3u];

        for (int band = 0; band < TRAIL_BANDS; band++)
            for (int k = 0; k < BAND_HALF_ROWS; k++)
                plane_set(x, TRAIL_TOP_HALF + band * BAND_HALF_ROWS + k + offset,
                          trail_colors[band]);
    }
}

static void draw_cat(uint32_t tick)
{
    int index = (int)(tick % CAT_FRAMES);
    const char *const *rows = cat_frames[index];
    int top = CAT_TOP_HALF + cat_bob[index];

    for (int r = 0; r < CAT_HALF_ROWS; r++) {
        for (int c = 0; c < CAT_W; c++) {
            uint8_t color = key_color(rows[r][c]);
            if (color != TRANSPARENT)
                plane_set(CAT_X + c, top + r, color);
        }
    }
}

/* Fold the pixel plane down onto the character grid.  Cells where both pixels
 * are transparent are left alone, so the starfield drawn underneath survives. */
static void plane_blit(void)
{
    for (int y = 0; y < VGA_HEIGHT; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            uint8_t top    = plane[y * 2][x];
            uint8_t bottom = plane[y * 2 + 1][x];

            if (top == TRANSPARENT && bottom == TRANSPARENT)
                continue;

            vga_put_half(x, y,
                         top == TRANSPARENT ? BG_COLOR : top,
                         bottom == TRANSPARENT ? BG_COLOR : bottom);
        }
    }
}

/* --- public --------------------------------------------------------------- */

void nyan_render(uint32_t tick)
{
    vga_clear(' ', vga_attr(VGA_LIGHT_GREY, BG_COLOR));
    draw_stars(tick);

    plane_clear();
    draw_trail(tick);
    draw_cat(tick);
    plane_blit();
}
