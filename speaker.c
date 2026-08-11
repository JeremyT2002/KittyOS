/* speaker.c -- PC speaker square waves, and the melody that loops over them.
 *
 * How the speaker works on a PC: PIT channel 2 is wired to the speaker instead
 * of to the interrupt controller.  Programming it in mode 3 makes it emit a
 * square wave at 1193182 / divisor Hz, and port 0x61 decides whether that wave
 * reaches the speaker cone:
 *
 *     bit 0  gate    -- lets channel 2 count at all
 *     bit 1  data    -- connects channel 2's output to the speaker
 *
 * Both bits must be set for sound.  Bits 2-7 of port 0x61 are chipset status
 * that we must not disturb, so the port is always read-modify-written.
 *
 * Channel 2 is independent of channel 0, so none of this perturbs the 100 Hz
 * tick the animation is paced by.
 */

#include "speaker.h"
#include "io.h"

#define PIT_CH2_DATA  0x42
#define PIT_COMMAND   0x43
#define SPEAKER_PORT  0x61

/* The PIT's fixed input clock, 1.193182 MHz. */
#define PIT_BASE_HZ   1193182u

void speaker_tone(uint32_t hz)
{
    if (hz == 0) {
        speaker_off();
        return;
    }

    uint32_t divisor = PIT_BASE_HZ / hz;
    if (divisor > 0xFFFF)       /* below ~18 Hz the divisor no longer fits */
        divisor = 0xFFFF;
    if (divisor < 2)
        divisor = 2;

    /* Command 0xB6 = 0b10_11_011_0:
     *   bits 6-7 = 10  -> channel 2
     *   bits 4-5 = 11  -> access mode: low byte then high byte
     *   bits 1-3 = 011 -> mode 3, square wave
     *   bit 0    = 0   -> binary, not BCD
     */
    outb(PIT_COMMAND, 0xB6);
    outb(PIT_CH2_DATA, (uint8_t)(divisor & 0xFF));
    outb(PIT_CH2_DATA, (uint8_t)((divisor >> 8) & 0xFF));

    uint8_t v = inb(SPEAKER_PORT);
    if ((v & 0x03) != 0x03)
        outb(SPEAKER_PORT, (uint8_t)(v | 0x03));   /* gate + data on */
}

void speaker_off(void)
{
    uint8_t v = inb(SPEAKER_PORT);
    outb(SPEAKER_PORT, (uint8_t)(v & ~0x03));
}

/* --- notes ---------------------------------------------------------------- */
/* Equal temperament, A4 = 440 Hz, rounded to whole Hz.  The PIT divisor is
 * integer anyway, so fractions of a Hz would be lost regardless. */
#define REST    0
#define B4      494
#define CS5     554
#define D5      587
#define DS5     622
#define E5      659
#define FS5     740
#define GS5     831
#define AS5     932
#define B5      988
#define CS6    1109
#define DS6    1245
#define E6     1319
#define FS6    1480

/* The main riff, hand-tuned by ear rather than transcribed from a score -- it
 * is a recognisable approximation, not an exact quotation.  Edit freely: the
 * sequencer just walks this table.
 *
 * At 11 ticks per note (110 ms at 100 Hz) the loop runs about five seconds. */
static const uint16_t melody[] = {
    /* phrase A -- the climb */
    DS5, E5,  FS5, B5,   DS5, E5,  FS5, B5,
    CS6, DS6, CS6, AS5,  B5,  CS6, B5,  FS5,
    /* phrase B -- the peak */
    FS5, GS5, B5,  FS5,  GS5, B5,  CS6, DS6,
    B5,  E6,  DS6, E6,   FS6, B5,  AS5, B5,
    /* phrase C -- the fall, ending in a breath */
    B4,  FS5, GS5, B5,   FS5, B5,  CS6, DS6,
    CS6, AS5, B5,  FS5,  DS5, FS5, B4,  REST,
};

#define MELODY_LEN   ((int)(sizeof(melody) / sizeof(melody[0])))
#define NOTE_TICKS   11     /* tone length  */
#define GAP_TICKS    1      /* silence after each note, so repeated notes
                             * are audible as two separate notes */

/* Sequencer state.  `index` counts notes and is taken modulo MELODY_LEN on
 * every use, so it cannot run away even after days of playing. */
static uint32_t index;
static uint64_t next_change;
static int      in_gap = 1;     /* start in a gap so the first update plays
                                 * note 0 immediately */

void speaker_update(uint64_t ticks)
{
    if (ticks < next_change)
        return;

    if (in_gap) {
        /* Gap is over: start the next note. */
        uint16_t hz = melody[index % (uint32_t)MELODY_LEN];
        index++;
        speaker_tone(hz);
        in_gap = 0;
        next_change = ticks + NOTE_TICKS;
    } else {
        /* Note is over: brief silence before the next one. */
        speaker_off();
        in_gap = 1;
        next_change = ticks + GAP_TICKS;
    }
}
