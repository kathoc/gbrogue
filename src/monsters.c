#include <gb/gb.h>
#include "monsters.h"
#include "rng.h"
#include "farcopy.h"

/* Stats follow Rogue 5.4's monsters[] table (level, armor, exp, damage
   strings). Special on-hit abilities (rust, freeze, steal, drain) land
   in M7 — docs/status.md tracks which are live. */
/* The stat table lives in bank 2 (assets/mkind_data.c); this WRAM
   cache holds the record last asked for. */
BANKREF_EXTERN(mkind_data)
extern const mkind_t MKIND_ROM[MKIND_COUNT];

static mkind_t mk_cache;
static uint8_t mk_kind = 0xFFu;

const mkind_t *mkind(uint8_t kind) {
    if (kind != mk_kind) {
        far_copy(BANK(mkind_data), (const uint8_t *)&MKIND_ROM[kind],
                 &mk_cache, sizeof(mkind_t));
        mk_kind = kind;
    }
    return &mk_cache;
}

/* Rogue 5.4's shallow-to-deep spawn ladder. */
static const char LVL_MONS[MKIND_COUNT + 1] = "KEBSHIROZLCQANYFTWPXUMVGJD";

uint8_t monster_pick(uint8_t depth) {
    /* index ~ depth-1 + rnd(5)-2, clamped to the ladder. */
    int8_t idx = (int8_t)(depth - 1) + (int8_t)rng_range(5) - 2;
    if (idx < 0) idx = 0;
    if (idx >= MKIND_COUNT) idx = MKIND_COUNT - 1;
    return (uint8_t)(LVL_MONS[idx] - 'A');
}

uint8_t monster_roll_hp(uint8_t kind) {
    uint8_t lvl = (uint8_t)mkind(kind)->lvl;
    uint8_t hp = rng_dice(lvl ? lvl : 1, 8);
    return hp ? hp : 1;
}
