#include <gb/gb.h>
#include "monsters.h"
#include "rng.h"
#include "farcopy.h"

/*
 * BANK0 (deliberately early-sorting filename, like bank0_rng.c). mkind()
 * and monster_roll_hp() are reached from banked item logic (BANK2 wand
 * polymorph), and call_bank unmaps bank 1 — so these must live in the
 * fixed bank. mkind far-copies the stat record from bank 2 (mkind_data)
 * into this WRAM cache; far_copy itself is already in bank 0.
 * monster_pick lives here too — banked read_scroll (BANK2 create-monster)
 * calls it; it touches only rng (bank 0) and a local const ladder.
 */
BANKREF_EXTERN(mkind_data)
extern const mkind_t MKIND_ROM[MKIND_COUNT];

/* Rogue 5.4's shallow-to-deep spawn ladder. */
static const char LVL_MONS[MKIND_COUNT + 1] = "KEBSHIROZLCQANYFTWPXUMVGJD";

uint8_t monster_pick(uint8_t depth) {
    /* index ~ depth-1 + rnd(5)-2, clamped to the ladder. */
    int8_t idx = (int8_t)(depth - 1) + (int8_t)rng_range(5) - 2;
    if (idx < 0) idx = 0;
    if (idx >= MKIND_COUNT) idx = MKIND_COUNT - 1;
    return (uint8_t)(LVL_MONS[idx] - 'A');
}

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

uint8_t monster_roll_hp(uint8_t kind) {
    uint8_t lvl = (uint8_t)mkind(kind)->lvl;
    uint8_t hp = rng_dice(lvl ? lvl : 1, 8);
    return hp ? hp : 1;
}
