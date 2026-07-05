#include <gb/gb.h>
#include "lang.h"
#include "world.h"
#include "farcopy.h"
#include "monsters.h"
#include "items_data.h"

/* Bank-3 blobs (assets/lang_data.c, generated). */
BANKREF_EXTERN(lang_bank)
extern const uint8_t  STR_BLOB_EN[];
extern const uint8_t  STR_BLOB_JA[];
extern const uint16_t STR_OFS_EN[];
extern const uint16_t STR_OFS_JA[];
extern const uint8_t MNAME_JA[][LANG_NAME_STRIDE];
extern const uint8_t POTION_ALIAS_JA[][LANG_NAME_STRIDE];
extern const uint8_t POTION_NAME_JA[][LANG_NAME_STRIDE];
extern const uint8_t SCROLL_ALIAS_JA[][LANG_NAME_STRIDE];
extern const uint8_t SCROLL_NAME_JA[][LANG_NAME_STRIDE];
extern const uint8_t WAND_ALIAS_JA[][LANG_NAME_STRIDE];
extern const uint8_t WAND_NAME_JA[][LANG_NAME_STRIDE];
extern const uint8_t RING_ALIAS_JA[][LANG_NAME_STRIDE];
extern const uint8_t RING_NAME_JA[][LANG_NAME_STRIDE];
extern const uint8_t WEAPON_NAME_JA[][LANG_NAME_STRIDE];
extern const uint8_t ARMOR_NAME_JA[][LANG_NAME_STRIDE];
extern const uint8_t FOOD_NAME_JA[][LANG_NAME_STRIDE];
extern const uint8_t KIND_SUFFIX_JA[][LANG_NAME_STRIDE];
extern const uint8_t EXTRA_JA[][LANG_NAME_STRIDE];
extern const uint8_t MNAME_EN[][LANG_NAME_STRIDE];
extern const uint8_t POTION_ALIAS_EN[][LANG_NAME_STRIDE];
extern const uint8_t POTION_NAME_EN[][LANG_NAME_STRIDE];
extern const uint8_t SCROLL_ALIAS_EN[][LANG_NAME_STRIDE];
extern const uint8_t SCROLL_NAME_EN[][LANG_NAME_STRIDE];
extern const uint8_t WAND_ALIAS_EN[][LANG_NAME_STRIDE];
extern const uint8_t WAND_NAME_EN[][LANG_NAME_STRIDE];
extern const uint8_t RING_ALIAS_EN[][LANG_NAME_STRIDE];
extern const uint8_t RING_NAME_EN[][LANG_NAME_STRIDE];
extern const uint8_t WEAPON_NAME_EN[][LANG_NAME_STRIDE];
extern const uint8_t ARMOR_NAME_EN[][LANG_NAME_STRIDE];
extern const uint8_t FOOD_NAME_EN[][LANG_NAME_STRIDE];
extern const uint8_t KIND_SUFFIX_EN[][LANG_NAME_STRIDE];
extern const uint8_t EXTRA_EN[][LANG_NAME_STRIDE];

#define LBUF 44u
static char bufs[2][LBUF];
static uint8_t which;

static char *next_buf(void) {
    which ^= 1u;
    return bufs[which];
}

const char *lang_str(uint8_t id) {
    char *b = next_buf();
    uint16_t ofs;
    if (g_lang == LANG_JA) {
        far_copy(BANK(lang_bank), (const uint8_t *)&STR_OFS_JA[id],
                 (uint8_t *)&ofs, 2);
        far_copy(BANK(lang_bank), STR_BLOB_JA + ofs, (uint8_t *)b, LBUF - 1u);
    } else {
        far_copy(BANK(lang_bank), (const uint8_t *)&STR_OFS_EN[id],
                 (uint8_t *)&ofs, 2);
        far_copy(BANK(lang_bank), STR_BLOB_EN + ofs, (uint8_t *)b, LBUF - 1u);
    }
    b[LBUF - 1u] = 0;
    return b;
}

/* Pointer table indexed [lang][LT_*] — much smaller than switches. */
static const uint8_t *const NAME_TBLS[2][14] = {
    { (const uint8_t *)MNAME_EN,
      (const uint8_t *)POTION_ALIAS_EN, (const uint8_t *)POTION_NAME_EN,
      (const uint8_t *)SCROLL_ALIAS_EN, (const uint8_t *)SCROLL_NAME_EN,
      (const uint8_t *)WAND_ALIAS_EN, (const uint8_t *)WAND_NAME_EN,
      (const uint8_t *)RING_ALIAS_EN, (const uint8_t *)RING_NAME_EN,
      (const uint8_t *)WEAPON_NAME_EN, (const uint8_t *)ARMOR_NAME_EN,
      (const uint8_t *)FOOD_NAME_EN, (const uint8_t *)KIND_SUFFIX_EN,
      (const uint8_t *)EXTRA_EN },
    { (const uint8_t *)MNAME_JA,
      (const uint8_t *)POTION_ALIAS_JA, (const uint8_t *)POTION_NAME_JA,
      (const uint8_t *)SCROLL_ALIAS_JA, (const uint8_t *)SCROLL_NAME_JA,
      (const uint8_t *)WAND_ALIAS_JA, (const uint8_t *)WAND_NAME_JA,
      (const uint8_t *)RING_ALIAS_JA, (const uint8_t *)RING_NAME_JA,
      (const uint8_t *)WEAPON_NAME_JA, (const uint8_t *)ARMOR_NAME_JA,
      (const uint8_t *)FOOD_NAME_JA, (const uint8_t *)KIND_SUFFIX_JA,
      (const uint8_t *)EXTRA_JA },
};

static const uint8_t *name_table(uint8_t table) {
    /* callers only pass LT_* values (0..13) */
    return NAME_TBLS[g_lang & 1u][table];
}

const char *lang_name(uint8_t table, uint8_t idx) {
    char *b = next_buf();
    far_copy(BANK(lang_bank),
             name_table(table) + (uint16_t)idx * LANG_NAME_STRIDE,
             (uint8_t *)b, LANG_NAME_STRIDE);
    b[LANG_NAME_STRIDE - 1u] = 0;
    return b;
}
