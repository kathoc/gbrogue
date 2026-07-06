#include "bankcall.h"
#include "items_use.h"
#include "items.h"
#include "items_data.h"
#include "items_zap.h"
#include "inventory.h"
#include "identify.h"
#include "world.h"
#include "map.h"
#include "worldview.h"
#include "actor.h"
#include "monsters.h"
#include "combat.h"
#include "effects.h"
#include "rng.h"
#include "msg.h"
#include "lang.h"
#include "util.h"

/* Food + potion + scroll effects (eat/heal/quaff/read_scroll) live in
   items_use_fx.c (#pragma bank 2); items_use() reaches them via call_bank. */

/* ------------------------------------------------------------- dispatch */

uint8_t items_use(uint8_t slot) {
    item_t *it = &g_pack[slot];

    switch (it->kind) {
    case IK_FOOD:
    case IK_POTION:
        /* eat/quaff live in BANK2 (items_use_fx.c); marshal + hop in.
           ui_inv's msgq_flush replays the queued messages afterward. */
        g_use_slot = slot;
        call_bank(2u, bank_consume_effect);
        return g_use_turns;
    case IK_SCROLL:
        /* read_scroll lives in BANK2 too; same marshalling path. The
           teleport case moves the player in RAM only — view_world_enter()
           on pack close repaints. */
        g_use_slot = slot;
        call_bank(2u, bank_read_scroll);
        return g_use_turns;
    case IK_WAND:
        return items_zap(slot);
    case IK_WEAPON:
    case IK_ARMOR:
    case IK_RING:
        return inv_equip(slot);
    case IK_AMULET:
        msgq_id(SID_AMULET_GLOW);
        return 0;
    default:
        return 0;
    }
}
