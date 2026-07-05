#include <gb/gb.h>
#include "text4.h"
#include "input.h"
#include "farcopy.h"

BANKREF_EXTERN(FONT4)
extern const uint8_t FONT4[95][8];         /* ROM bank 2 */
BANKREF_EXTERN(lang_bank)
extern const uint8_t MISAKI[][8];          /* ROM bank 3, 1bpp kana */

/* Pull one glyph's 8 rows out of the font bank via the WRAM far-copy
   trampoline (home code spans past 0x4000, so no direct switching). */
static void font4_fetch(char c, uint8_t *dst) {
    uint8_t u = (uint8_t)c;
    if (u < 0x20u || u > 0x7Eu) u = 0x20u;
    far_copy(BANK(FONT4), FONT4[u - 0x20u], dst, 8);
}

uint16_t g_t4_keys[T4_COUNT];
uint8_t  g_t4_used;
uint8_t  g_t4_flushed;

void t4_reset(void) {
    g_t4_used = 0;
}

uint8_t t4_pair(char a, char b) {
    uint16_t key = (uint16_t)(((uint16_t)(uint8_t)a << 8) | (uint8_t)b);
    uint8_t i;

    /* Text repaints are the longest CPU bursts in the game; latching
       edges here keeps short taps alive through any redraw. */
    input_tick();

    if (a == ' ' && b == ' ') return 0;      /* shared blank tile */

    for (i = 0; i < g_t4_used; i++)
        if (g_t4_keys[i] == key) return (uint8_t)(T4_BASE + i);

    if (g_t4_used >= T4_COUNT) {
        /* Pool exhausted mid-scene: recycle and flag it. render_present
           redraws the HUD rows from their stored strings so nothing on
           screen keeps pointing at recycled slots. */
        g_t4_used = 0;
        g_t4_flushed = 1;
    }

    {
        uint8_t buf[16];
        uint8_t l[8], r[8];
        uint8_t row, v;
        font4_fetch(a, l);
        font4_fetch(b, r);
        for (row = 0; row < 8u; row++) {
            v = (uint8_t)(l[row] | (r[row] >> 4));
            buf[row * 2u] = v;                /* both planes: color 3 */
            buf[row * 2u + 1u] = v;
        }
        i = g_t4_used;
        set_bkg_data((uint8_t)(T4_BASE + i), 1, buf);
        g_t4_keys[i] = key;
        g_t4_used++;
        return (uint8_t)(T4_BASE + i);
    }
}

/* Allocate a slot with the given key; returns 0xFF if a lookup hit
   instead (caller returns the cached tile). */
static uint8_t t4_alloc(uint16_t key, uint8_t *hit) {
    uint8_t i;
    for (i = 0; i < g_t4_used; i++) {
        if (g_t4_keys[i] == key) {
            *hit = (uint8_t)(T4_BASE + i);
            return 0xFFu;
        }
    }
    if (g_t4_used >= T4_COUNT) {
        g_t4_used = 0;
        g_t4_flushed = 1;
    }
    g_t4_keys[g_t4_used] = key;
    return g_t4_used++;
}

uint8_t t4_full(uint8_t code) {
    uint8_t hit, slot;
    input_tick();
    slot = t4_alloc((uint16_t)(0xFF00u | code), &hit);
    if (slot == 0xFFu) return hit;
    {
        uint8_t rows[8], buf[16], r;
        uint8_t idx = (code < 0x20u) ? (uint8_t)(code - 2u)
                                     : (uint8_t)(30u + code - 0x80u);
        far_copy(BANK(lang_bank), MISAKI[idx], rows, 8);
        for (r = 0; r < 8u; r++) {
            buf[r * 2u] = rows[r];        /* both planes: color 3 */
            buf[r * 2u + 1u] = rows[r];
        }
        set_bkg_data((uint8_t)(T4_BASE + slot), 1, buf);
        return (uint8_t)(T4_BASE + slot);
    }
}

void t4_line_bitmap(const char *s, uint8_t *dst) {
    uint8_t cell = 0, r;
    while (cell < MSGROW_TILES && s && *s) {
        uint8_t *rows = dst + (uint16_t)cell * 8u;
        uint8_t b0 = (uint8_t)*s;
        if (T4_IS_FULL(b0)) {
            uint8_t idx = (b0 < 0x20u) ? (uint8_t)(b0 - 2u)
                                       : (uint8_t)(30u + b0 - 0x80u);
            far_copy(BANK(lang_bank), MISAKI[idx], rows, 8);
            s++;
        } else {
            uint8_t l[8], rgt[8];
            char a = *s++;
            char b = ' ';
            if (*s && !T4_IS_FULL(*s)) b = *s++;
            font4_fetch(a, l);
            font4_fetch(b, rgt);
            for (r = 0; r < 8u; r++) rows[r] = (uint8_t)(l[r] | (rgt[r] >> 4));
        }
        cell++;
    }
    while (cell < MSGROW_TILES) {
        uint8_t *rows = dst + (uint16_t)cell * 8u;
        for (r = 0; r < 8u; r++) rows[r] = 0;
        cell++;
    }
}

uint8_t t4_raw(const uint8_t *tile16) {
    uint8_t hit, slot;
    slot = t4_alloc((uint16_t)(0xFD00u | g_t4_used), &hit);
    if (slot == 0xFFu) return hit;        /* unreachable: keys unique */
    set_bkg_data((uint8_t)(T4_BASE + slot), 1, tile16);
    return (uint8_t)(T4_BASE + slot);
}
