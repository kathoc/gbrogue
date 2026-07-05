#include "items.h"
#include "items_data.h"

/*
 * ROM stat tables. The EN/JA display-name tables were moved to bank 3
 * (assets/lang_data.c, via gen_lang.py) — HOME has no room for string
 * data. Every Rogue 5.4 subtype stays present (docs/non-negotiables).
 */

/* damage dice: count, sides (melee; bows/ammo shine in the M9 ranged pass) */
const uint8_t WEAPON_DICE[N_WEAPONS][2] = {
    {2, 4}, {3, 4}, {1, 1}, {1, 1}, {1, 6}, {4, 4}, {1, 3}, {2, 4}, {2, 3},
};

/* Rogue AC: lower = better. No armor = 10. */
const uint8_t ARMOR_AC[N_ARMORS] = { 8, 7, 7, 6, 5, 4, 4, 3 };
