#ifndef MONSTERS_H
#define MONSTERS_H

#include <stdint.h>

/* The full Rogue 5.4 bestiary, A..Z. Kind index 0 = 'A' Aquator. */
#define MKIND_COUNT 26

#define MFL_MEAN   0x01u   /* chases as soon as it sees you */
#define MFL_FLY    0x02u
#define MFL_REGEN  0x04u
#define MFL_INVIS  0x08u
#define MFL_GREEDY 0x10u   /* heads for gold (flavor, M7) */

typedef struct {
    int8_t   lvl;          /* hit dice / attack level */
    int8_t   arm;          /* armor class, lower = harder to hit */
    uint16_t exp;
    uint8_t  flags;
    uint8_t  d[6];         /* up to 3 attacks: count,sides pairs, 0-padded */
} mkind_t;

/* Banked stat table accessor (cached far-copy). */
const mkind_t *mkind(uint8_t kind);

/* Pick a kind for the given depth (Rogue's lvl_mons ladder). */
uint8_t monster_pick(uint8_t depth);

/* Max HP roll for a kind: lvl d8, minimum 1. */
uint8_t monster_roll_hp(uint8_t kind);

#endif
