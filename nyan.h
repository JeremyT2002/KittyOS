/* nyan.h -- the flying cat animation. */
#ifndef KITTYOS_NYAN_H
#define KITTYOS_NYAN_H

#include <stdint.h>

/* Render one whole frame (background, stars, trail, cat) into the VGA back
 * buffer.  Does not present -- the caller decides when to flip.
 * `tick` may increase forever; every use of it is taken modulo something, so
 * there is no state that can overflow into a glitch. */
void nyan_render(uint32_t tick);

#endif /* KITTYOS_NYAN_H */
