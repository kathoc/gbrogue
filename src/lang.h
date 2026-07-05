#ifndef LANG_H
#define LANG_H

#include <stdint.h>
#include "lang_ids.h"

#define LANG_EN 0u
#define LANG_JA 1u

/* Name-table selectors for lang_name(). */
enum {
    LT_MNAME, LT_P_ALIAS, LT_P_NAME, LT_S_ALIAS, LT_S_NAME,
    LT_W_ALIAS, LT_W_NAME, LT_R_ALIAS, LT_R_NAME,
    LT_WEAPON, LT_ARMOR, LT_FOOD, LT_SUFFIX, LT_EXTRA,
};
/* LT_SUFFIX rows: 0 potion, 1 scroll, 2 wand, 3 ring.
   LT_EXTRA rows: 0 "gold", 1 "the Amulet", 2 "???". */

/* Fetch a message by SID into one of two rotating buffers (so one
   composition may hold a pattern and a name at once). */
const char *lang_str(uint8_t id);
/* Fetch a name-table entry in the active language. */
const char *lang_name(uint8_t table, uint8_t idx);

#endif
