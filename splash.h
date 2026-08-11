/* splash.h -- boot splash: a big cat, converted from Braille art. */
#ifndef KITTYOS_SPLASH_H
#define KITTYOS_SPLASH_H

#include <stdint.h>

/* How many PIT ticks the splash stays on screen before the animation starts.
 * At the programmed 100 Hz that is 2.5 seconds. */
#define SPLASH_TICKS 250

/* Draw the splash into the VGA back buffer (does not present). */
void splash_draw(void);

#endif /* KITTYOS_SPLASH_H */
