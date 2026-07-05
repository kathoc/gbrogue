#include <gb/gb.h>
#include "monsters.h"
#include "rng.h"
#include "farcopy.h"

/* Stats follow Rogue 5.4's monsters[] table (level, armor, exp, damage
   strings). Special on-hit abilities (rust, freeze, steal, drain) land
   in M7 — docs/status.md tracks which are live. */
/* mkind() and monster_roll_hp() live in bank0_monster.c (fixed bank) so
   banked item logic can reach them; the stat cache lives there too. */

/* Rogue 5.4's shallow-to-deep spawn ladder. */
static const char LVL_MONS[MKIND_COUNT + 1] = "KEBSHIROZLCQANYFTWPXUMVGJD";

uint8_t monster_pick(uint8_t depth) {
    /* index ~ depth-1 + rnd(5)-2, clamped to the ladder. */
    int8_t idx = (int8_t)(depth - 1) + (int8_t)rng_range(5) - 2;
    if (idx < 0) idx = 0;
    if (idx >= MKIND_COUNT) idx = MKIND_COUNT - 1;
    return (uint8_t)(LVL_MONS[idx] - 'A');
}

