#include "items.h"
#include "items_data.h"
#include "identify.h"
#include "inventory.h"
#include "world.h"
#include "tiles.h"
#include "msg.h"
#include "lang.h"
#include "util.h"
#include "sfx.h"

item_t g_floor[MAX_FLOOR_ITEMS];

void items_clear_floor(void) {
    uint8_t i;
    for (i = 0; i < MAX_FLOOR_ITEMS; i++) g_floor[i].kind = ITEM_NONE;
}

item_t *item_floor_at(uint8_t x, uint8_t y) {
    uint8_t i;
    for (i = 0; i < MAX_FLOOR_ITEMS; i++) {
        if (g_floor[i].kind != ITEM_NONE &&
            g_floor[i].x == x && g_floor[i].y == y)
            return &g_floor[i];
    }
    return 0;
}

item_t *item_place(uint8_t kind, uint8_t sub, uint8_t x, uint8_t y) {
    uint8_t i;
    for (i = 0; i < MAX_FLOOR_ITEMS; i++) {
        item_t *it = &g_floor[i];
        if (it->kind != ITEM_NONE) continue;
        it->kind = kind;
        it->sub = sub;
        it->x = x;
        it->y = y;
        it->qty = 1;
        it->ench = 0;
        it->flags = 0;
        return it;
    }
    return 0;
}

uint8_t item_tile(uint8_t kind) {
    switch (kind) {
    case IK_FOOD:   return TI_FOOD;
    case IK_POTION: return TI_POTION;
    case IK_SCROLL: return TI_SCROLL;
    case IK_WAND:   return TI_WAND;
    case IK_RING:   return TI_RING;
    case IK_WEAPON: return TI_WEAPON;
    case IK_ARMOR:  return TI_ARMOR;
    case IK_GOLD:   return TI_GOLD;
    case IK_AMULET: return TI_AMULET;
    default:        return TI_BLANK;
    }
}

/* One identified-or-alias class name + its kind suffix. */
static char *class_name(char *p, uint8_t idc, uint8_t sub,
                        uint8_t alias_tbl, uint8_t name_tbl, uint8_t sfx) {
    if (identify_known(idc, sub))
        p = fmt_str(p, lang_name(name_tbl, sub));
    else
        p = fmt_str(p, lang_name(alias_tbl, identify_alias(idc, sub)));
    return fmt_str(p, lang_name(LT_SUFFIX, sfx));
}

/* "blue ptn" / "あおのくすり" / "mace +1" / "23 gold" ... */
char *item_name(char *dst, const item_t *it) {
    char *p = dst;
    switch (it->kind) {
    case IK_FOOD:
        p = fmt_str(p, lang_name(LT_FOOD, it->sub));
        break;
    case IK_POTION:
        p = class_name(p, IDC_POTION, it->sub, LT_P_ALIAS, LT_P_NAME, 0);
        break;
    case IK_SCROLL:
        p = class_name(p, IDC_SCROLL, it->sub, LT_S_ALIAS, LT_S_NAME, 1);
        break;
    case IK_WAND:
        p = class_name(p, IDC_WAND, it->sub, LT_W_ALIAS, LT_W_NAME, 2);
        break;
    case IK_RING:
        p = class_name(p, IDC_RING, it->sub, LT_R_ALIAS, LT_R_NAME, 3);
        break;
    case IK_WEAPON:
        p = fmt_str(p, lang_name(LT_WEAPON, it->sub));
        goto ench;
    case IK_ARMOR:
        p = fmt_str(p, lang_name(LT_ARMOR, it->sub));
ench:
        if ((it->flags & IF_WORN) && it->ench != 0) {
            p = fmt_str(p, it->ench > 0 ? " +" : " -");
            p = fmt_u16(p, (uint16_t)(it->ench > 0 ? it->ench : -it->ench));
        } else if (!(it->flags & IF_WORN)) {
            p = fmt_str(p, " ?");
        }
        if (it->flags & IF_KNOWN_CURSED) p = fmt_str(p, " c");
        break;
    case IK_GOLD:
        if (g_lang) {
            p = fmt_str(p, lang_name(LT_EXTRA, 0));
            p = fmt_u16(p, it->qty);
        } else {
            p = fmt_u16(p, it->qty);
            p = fmt_char(p, ' ');
            p = fmt_str(p, lang_name(LT_EXTRA, 0));
        }
        break;
    case IK_AMULET:
        p = fmt_str(p, lang_name(LT_EXTRA, 1));
        break;
    default:
        p = fmt_str(p, lang_name(LT_EXTRA, 2));
        break;
    }
    *p = 0;
    return p;
}

void item_pickup_here(void) {
    item_t *it = item_floor_at(g_px, g_py);
    char buf[32];
    char *p;

    if (!it) return;

    /* Mutate state BEFORE posting: the message slide takes ~8 frames
       and nothing should be observable mid-scroll. */
    if (it->kind == IK_GOLD) {
        uint16_t amount = (uint16_t)(it->qty * 4u);   /* stored /4 */
        sfx_play(SFX_GOLD);
        g_gold = (uint16_t)(g_gold + amount);
        it->kind = ITEM_NONE;
        p = fmt_u16(buf, amount);
        *p = 0;
        msg_postf(SID_FOUND_GOLD, buf);
        return;
    }

    if (it->kind == IK_AMULET) {
        sfx_play(SFX_ITEM);
        g_has_amulet = 1;
        it->kind = ITEM_NONE;
        msg_post_id(SID_AMULET_GET);
        return;
    }

    {
        uint8_t slot = inv_add(it);
        if (slot == SLOT_NONE) {
            msg_post_id(SID_PACK_FULL);
            return;
        }
        sfx_play(SFX_ITEM);
        p = item_name(buf, it);
        *p = 0;
        it->kind = ITEM_NONE;
        msg_postf(SID_GOT, buf);
    }
}
