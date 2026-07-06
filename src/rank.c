#include <gb/gb.h>
#pragma bank 5
#include "rank.h"

/*
 * BANK5. SRAM block near the top of the 8 KB bank, clear of the suspend
 * save (0xA000, well under 1 KB). Layout: magic, then a 2-byte lifetime
 * games-played counter, then RANK_N entries. Everything here is
 * self-contained (no calls into other banks): SRAM is touched byte-by-byte
 * and entries are copied with tiny manual loops so no library routine
 * (which might live in another bank) is reached while BANK5 is mapped.
 */
#define RB       ((uint8_t *)0xBF80)
/* Magic bumped on every SRAM-layout change so a stale block is cleared
   (a one-time ranking reset on upgrade) rather than misread:
   'K'(0x4B) original -> 'L'(0x4C) death-cause fields
   -> 'M'(0x4D) games-played counter + per-entry play_no. */
#define RB_MAGIC 0x4Du
#define ESZ      ((uint8_t)sizeof(rank_entry_t))
#define HDR      3u                    /* magic(1) + games_played(2) */
#define TOTAL    (uint8_t)(HDR + RANK_N * ESZ)

rank_entry_t g_rank_io[RANK_N];
rank_entry_t g_rank_new;
uint8_t      g_rank_n;
uint16_t     g_games_played;

/* raw byte copy (avoids memcpy, which may sit in another bank) */
static void copy(uint8_t *dst, const uint8_t *src, uint8_t n) {
    while (n--) *dst++ = *src++;
}

static void rb_ensure(void) {
    if (RB[0] != RB_MAGIC) {
        uint8_t i;
        for (i = 1u; i < TOTAL; i++) RB[i] = 0;
        RB[0] = RB_MAGIC;
    }
}

void bank_rank_read(void) {
    uint8_t i;
    ENABLE_RAM;
    rb_ensure();
    g_games_played = (uint16_t)(RB[1] | ((uint16_t)RB[2] << 8));
    copy((uint8_t *)g_rank_io, RB + HDR, (uint8_t)(RANK_N * ESZ));
    DISABLE_RAM;
    g_rank_n = 0;
    for (i = 0; i < RANK_N; i++)
        if (g_rank_io[i].deepest) g_rank_n++;
}

/* Lifetime games-played counter (RB[1..2], little-endian). */
void bank_games_load(void) {
    ENABLE_RAM;
    rb_ensure();
    g_games_played = (uint16_t)(RB[1] | ((uint16_t)RB[2] << 8));
    DISABLE_RAM;
}

void bank_games_inc(void) {
    uint16_t v;
    ENABLE_RAM;
    rb_ensure();
    v = (uint16_t)(RB[1] | ((uint16_t)RB[2] << 8));
    v++;
    RB[1] = (uint8_t)v;
    RB[2] = (uint8_t)(v >> 8);
    g_games_played = v;
    DISABLE_RAM;
}

void bank_rank_insert(void) {
    rank_entry_t t[RANK_N];
    uint8_t i, pos = RANK_N;
    ENABLE_RAM;
    rb_ensure();
    copy((uint8_t *)t, RB + HDR, (uint8_t)(RANK_N * ESZ));
    for (i = 0; i < RANK_N; i++)
        if (!t[i].deepest || g_rank_new.gold > t[i].gold) { pos = i; break; }
    if (pos < RANK_N) {
        /* Shift down + insert with byte copies, NOT struct assignment:
           a struct '=' compiles to a memcpy call, and memcpy lives in
           another bank — calling it here (BANK5 mapped) would crash. */
        for (i = (uint8_t)(RANK_N - 1u); i > pos; i--)
            copy((uint8_t *)&t[i], (const uint8_t *)&t[i - 1u], ESZ);
        copy((uint8_t *)&t[pos], (const uint8_t *)&g_rank_new, ESZ);
        copy(RB + HDR, (const uint8_t *)t, (uint8_t)(RANK_N * ESZ));
    }
    DISABLE_RAM;
}
