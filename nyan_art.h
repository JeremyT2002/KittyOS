/* nyan_art.h -- GENERATED, do not edit by hand.
 *
 * Produced by tools/braille2sprite.py from tools/cat.txt:
 *   Braille art -> dot bitmap -> region fill -> 2x downscale -> colour keys
 *
 * Each string is ONE HALF-ROW of pixels, not a text row: two consecutive
 * strings share a character cell and are drawn with a half block.  Keys:
 *
 *   ' '  transparent      'k'  outline        'p'  pop-tart filling
 *   'g'  fur              's'  sprinkle
 *
 * Regenerate with:
 *   python3 tools/braille2sprite.py --header nyan_art.h
 */
#ifndef KITTYOS_NYAN_ART_H
#define KITTYOS_NYAN_ART_H

#define CAT_W          30   /* pixels across, one per cell column */
#define CAT_HALF_ROWS  20   /* pixels down, two per cell row      */
#define CAT_FRAMES     4

static const char *const cat_frame0[CAT_HALF_ROWS] = {
    "      kkkkkkkkkkkkkkkkkkk     ",
    "      kkpppppppppppppppppk    ",
    "      kppppppppppsppppsppk    ",
    "      kpspssppppppppppsppk    ",
    "      kppppppppppkpppppppk    ",
    "      kppppppppspkkkpspppkkkk ",
    "kkk   kpppppsppskkggksppkkkgk ",
    "kggkk kpppppppppkggggkkkkgggk ",
    "kggggkkpppppppspkgggggggggggk ",
    " kgggkkpppssppppkgggggggggggkk",
    " kkgggkppppppsskgggkkggggkgkgk",
    "  kkggkpppsppspkgggkkggggkkkgk",
    "    kkkppssppppkgggggggkgggggk",
    "     kkppppppppkgggggggkgggggk",
    "      kpppsppsppkggggkggkkgggk",
    "      kpppppppppkkkgggggggkkk ",
    "     kkkkkkkkkkkkkkkkkkkkkkk  ",
    "     kggkkkgkk   kggk kggk    ",
    "     kkk kkkk    kkkk kgk     ",
    "                       kk     ",
};

static const char *const cat_frame1[CAT_HALF_ROWS] = {
    "      kkkkkkkkkkkkkkkkkkk     ",
    "      kkpppppppppppppppppk    ",
    "      kppppppppppsppppsppk    ",
    "      kpspssppppppppppsppk    ",
    "      kppppppppppkpppppppk    ",
    "kkk   kppppppppspkkkpspppkkkk ",
    "kggkk kpppppsppskkggksppkkkgk ",
    "kggggkkpppppppppkggggkkkkgggk ",
    " kgggkkpppppppspkgggggggggggk ",
    "  kgggkpppssppppkgggggggggggkk",
    " k kggkppppppsskgggkkggggkgkgk",
    "  k kkkpppsppspkgggkkggggkkkgk",
    "       ppssppppkgggggggkgggggk",
    "     kkppppppppkgggggggkgggggk",
    "      kpppsppsppkggggkggkkgggk",
    "      kpppppppppkkkgggggggkkk ",
    "     kkkkkkgkkkkkkkkkkkggkkk  ",
    "     kkkkkkkkkkk kkkkkkgk     ",
    "     kggkk       kggk k       ",
    "                  kk   kk     ",
};

static const char *const cat_frame2[CAT_HALF_ROWS] = {
    "      kkkkkkkkkkkkkkkkkkk     ",
    "      kkpppppppppppppppppk    ",
    "      kppppppppppsppppsppk    ",
    "      kpspssppppppppppsppk    ",
    "      kppppppppppkpppppppk    ",
    "      kppppppppspkkkpspppkkkk ",
    "kkk   kpppppsppskkggksppkkkgk ",
    "kkkkk kpppppppppkggggkkkkgggk ",
    "kggkkkkpppppppspkgggggggggggk ",
    "kggggkkpppssppppkgggggggggggkk",
    " kgggkkppppppsskgggkkggggkgkgk",
    "  kgggkpppsppspkgggkkggggkkkgk",
    "   kggkppssppppkgggggggkgggggk",
    "    kkkppppppppkgggggggkgggggk",
    "      kpppsppsppkggggkggkkgggk",
    "      kpppppppppkkkgggggggkkk ",
    "     kkkkkkkkkkkkkkkkkkkkkkk  ",
    "     kggkkkgkk   kggk kggk    ",
    "     kkk kkkk    kkkk kgk     ",
    "                       kk     ",
};

static const char *const cat_frame3[CAT_HALF_ROWS] = {
    "      kkkkkkkkkkkkkkkkkkk     ",
    "      kkpppppppppppppppppk    ",
    "      kppppppppppsppppsppk    ",
    "      kpspssppppppppppsppk    ",
    "      kppppppppppkpppppppk    ",
    "      kppppppppspkkkpspppkkkk ",
    "kkk   kpppppsppskkggksppkkkgk ",
    "kggkk kpppppppppkggggkkkkgggk ",
    "kggggkkpppppppspkgggggggggggk ",
    " kgggkkpppssppppkgggggggggggkk",
    " kkgggkppppppsskgggkkggggkgkgk",
    "  kkggkpppsppspkgggkkggggkkkgk",
    "    kkkppssppppkgggggggkgggggk",
    "     kkppppppppkgggggggkgggggk",
    "      kpppsppsppkggggkggkkgggk",
    "      kkkkkkkkkkkkkkkkkgggkkk ",
    "     kggkk      kkggk kkkkkk  ",
    "          kgkk    kk   ggk    ",
    "     kkk kkgk    k  k kggk    ",
    "          kkk         kgk     ",
};

static const char *const *const cat_frames[CAT_FRAMES] = {
    cat_frame0, cat_frame1, cat_frame2, cat_frame3,
};

/* Vertical bob per frame, in half-rows. */
static const int cat_bob[CAT_FRAMES] = { 0, 1, 0, 1 };

#endif /* KITTYOS_NYAN_ART_H */
