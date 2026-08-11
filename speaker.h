/* speaker.h -- PC speaker tone generator and melody sequencer.
 *
 * The PC speaker is driven by PIT channel 2, which is a completely separate
 * counter from channel 0 (our 100 Hz timer), so playing notes cannot disturb
 * the tick that paces the animation.
 */
#ifndef KITTYOS_SPEAKER_H
#define KITTYOS_SPEAKER_H

#include <stdint.h>

/* Start a square wave at `hz`, or silence the speaker if hz == 0. */
void speaker_tone(uint32_t hz);

/* Silence the speaker (also disconnects the timer gate). */
void speaker_off(void);

/* Advance the melody.  Call often -- once per timer tick is ideal; it returns
 * immediately unless the current note has run out. */
void speaker_update(uint64_t ticks);

#endif /* KITTYOS_SPEAKER_H */
