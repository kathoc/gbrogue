#include <gb/gb.h>
#include "bgm.h"

/*
 * Melody: C4 B3 D#4 D4 G#3 G3 B3 A#3 F3 E3 D#3 E3 F3 A3 G#3,
 * all whole notes at BPM 60 (240 frames each), stored as semitones
 * above C2 so the depth shift (down to -12) stays in table range.
 *
 * The whole tune lives on CH3 (wave): its wave RAM holds a 12.5%
 * pulse shape, output level is fixed at 50%, and the same frequency
 * table plays ONE OCTAVE LOWER than a pulse channel by hardware
 * design — exactly the requested "no echo, an octave down, half
 * volume". A +-1 LSB wobble adds a light vibrato. SFX own channels
 * 1 and 4; CH2 stays free.
 */
static const uint8_t MELODY[15] = {
    24u, 23u, 27u, 26u, 20u, 19u, 23u, 22u,
    17u, 16u, 15u, 16u, 17u, 21u, 20u,
};
#define MELODY_LEN 15u

/* GB frequency register values for semitones C2 (0) .. D#4 (27)
   (pulse pitches; on CH3 they sound an octave lower). */
static const uint16_t FREQ[28] = {
    44u, 157u, 263u, 363u, 457u, 547u, 631u, 711u,
    786u, 856u, 923u, 986u, 1046u, 1102u, 1155u, 1205u,
    1253u, 1297u, 1339u, 1379u, 1417u, 1452u, 1486u, 1517u,
    1546u, 1575u, 1602u, 1627u,
};

volatile uint8_t  g_bgm_idx;
volatile uint16_t g_bgm_len = 240u;

static volatile uint8_t on;
static uint16_t t;              /* frame within the current note */
static uint8_t  shift;          /* semitones down (depth) */
static uint16_t cur_f;          /* base freq reg of the current note */
static uint8_t  vib;            /* vibrato phase */

static void melody_note(void) {
    uint8_t n = (uint8_t)(MELODY[g_bgm_idx] - shift);
    cur_f = FREQ[n];
    NR32_REG = 0x40u;                       /* output 50% */
    NR33_REG = (uint8_t)cur_f;
    NR34_REG = (uint8_t)(0x80u | (cur_f >> 8));
}

/* One tick per frame — called from sfx.c's single VBL handler
   (registering a second add_VBL handler corrupted the ISR chain on
   GBDK-2020 4.5.0). */
void bgm_tick(void) {
    uint8_t w;
    if (!on) return;
    t++;
    if (t >= g_bgm_len) {
        t = 0;
        g_bgm_idx++;
        if (g_bgm_idx >= MELODY_LEN) g_bgm_idx = 0;
        melody_note();
    }
    /* vibrato: wobble the frequency LSB on a ~0.5s square LFO */
    vib = (uint8_t)((vib + 1u) & 31u);
    w = (uint8_t)((vib & 16u) ? 1u : 0u);
    if (t > 8u) NR33_REG = (uint8_t)(cur_f + w);
}

void bgm_set_depth(uint8_t depth) {
    uint8_t d = (uint8_t)(depth ? depth - 1u : 0u);
    g_bgm_len = (uint16_t)(240u + (uint16_t)(d > 12u ? 12u : d) * 10u);
    shift = (uint8_t)(d / 2u);
    if (shift > 12u) shift = 12u;
}

void bgm_start(void) {
    static const uint8_t PULSE_WAVE[16] = {
        0xFFu, 0xFFu, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    };
    uint8_t i;
    NR30_REG = 0x00u;                       /* wave RAM writable */
    for (i = 0; i < 16u; i++)
        *((uint8_t *)0xFF30 + i) = PULSE_WAVE[i];
    NR30_REG = 0x80u;                       /* wave DAC on */
    t = 0;
    g_bgm_idx = 0;
    melody_note();
    on = 1;
}

void bgm_stop(void) {
    on = 0;
    NR30_REG = 0x00u;                       /* wave DAC off */
}
