#include <gb/gb.h>
#pragma bank 5
#include "rank.h"

/*
 * BANK5. SRAM block near the top of the 8 KB bank, clear of the suspend
 * save (0xA000, well under 1 KB). Layout: magic 'K', then RANK_N entries.
 * Everything here is self-contained (no calls into other banks): SRAM is
 * touched byte-by-byte and entries are copied with tiny manual loops so
 * no library routine (which might live in another bank) is reached while
 * BANK5 is mapped.
 */
#define RB       ((uint8_t *)0xBF80)
#define RB_MAGIC 0x4Bu
#define ESZ      ((uint8_t)sizeof(rank_entry_t))
#define HDR      1u
#define TOTAL    (uint8_t)(HDR + RANK_N * ESZ)

rank_entry_t g_rank_io[RANK_N];
rank_entry_t g_rank_new;
uint8_t      g_rank_n;

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
    copy((uint8_t *)g_rank_io, RB + HDR, (uint8_t)(RANK_N * ESZ));
    DISABLE_RAM;
    g_rank_n = 0;
    for (i = 0; i < RANK_N; i++)
        if (g_rank_io[i].deepest) g_rank_n++;
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
        for (i = (uint8_t)(RANK_N - 1u); i > pos; i--) t[i] = t[i - 1u];
        t[pos] = g_rank_new;
        copy(RB + HDR, (const uint8_t *)t, (uint8_t)(RANK_N * ESZ));
    }
    DISABLE_RAM;
}
