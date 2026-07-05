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

/* Food + potion effects (eat/heal/quaff) live in items_use_fx.c
   (#pragma bank 2); items_use() reaches them via call_bank. */

/* -------------------------------------------------------------- scrolls */

static void magic_map(void) {
    uint8_t x, y;
    for (y = 0; y < MAP_H; y++)
        for (x = 0; x < MAP_W; x++)
            if (map_terrain(x, y) != TI_BLANK)
                map_set_flag(x, y, MF_EXPLORED);
    msgq_id(SID_S_MAP);
}

static void identify_something(void) {
    uint8_t i;
    for (i = 0; i < PACK_SLOTS; i++) {
        const item_t *it = &g_pack[i];
        uint8_t cls;
        if (it->kind == IK_POTION) cls = IDC_POTION;
        else if (it->kind == IK_SCROLL) cls = IDC_SCROLL;
        else if (it->kind == IK_WAND) cls = IDC_WAND;
        else if (it->kind == IK_RING) cls = IDC_RING;
        else continue;
        if (identify_known(cls, it->sub)) continue;
        identify_learn(cls, it->sub);
        {
            char buf[32];
            char *p = item_name(buf, it);
            *p = 0;
            msgq_arg(SID_S_ID, buf);
        }
        return;
    }
    msgq_id(SID_S_SMART);
}

static void spawn_one_adjacent(void) {
    uint8_t i;
    for (i = 0; i < MAX_MONSTERS; i++) {
        if (g_mons[i].kind != MON_NONE) continue;
        {
            uint8_t t;
            for (t = 0; t < 12u; t++) {
                uint8_t x = (uint8_t)(g_px - 1u + rng_range(3));
                uint8_t y = (uint8_t)(g_py - 1u + rng_range(3));
                if ((x == g_px && y == g_py) || !map_walkable(x, y)) continue;
                if (mon_at(x, y)) continue;
                g_mons[i].kind = monster_pick(g_depth);
                g_mons[i].x = x;
                g_mons[i].y = y;
                g_mons[i].hp = monster_roll_hp(g_mons[i].kind);
                g_mons[i].state = MST_AWAKE;
                g_mons[i].eff = 0;
                g_mons[i].eff_t = 0;
                msgq_id(SID_S_MAKEMON);
                return;
            }
        }
        break;
    }
    msgq_id(SID_S_GROWL);
}

static void mons_apply_visible(uint8_t eff, uint8_t dur) {
    uint8_t i, n = 0;
    for (i = 0; i < MAX_MONSTERS; i++) {
        monster_t *m = &g_mons[i];
        if (m->kind == MON_NONE) continue;
        if (!view_visible(m->x, m->y)) continue;
        m->eff |= eff;
        if (dur > m->eff_t) m->eff_t = dur;
        n++;
    }
    if (!n) {
        msgq_id(SID_S_NOTHING);
    } else if (eff & MEF_HELD) {
        msgq_id(SID_S_HOLD);
    } else {
        msgq_id(SID_S_FLEE);
    }
}

static uint8_t read_scroll(uint8_t slot) {
    uint8_t sub = g_pack[slot].sub;
    identify_learn(IDC_SCROLL, sub);
    inv_consume(slot);

    switch (sub) {
    case 0:  /* confuse monster: your next hit confuses */
        combat_set_confuse_hit();
        msgq_id(SID_S_CONFHIT);
        break;
    case 1:
        magic_map();
        break;
    case 2:  /* hold monster */
        mons_apply_visible(MEF_HELD, 20u);
        break;
    case 3:  /* sleep */
        g_sleep_t = (uint8_t)(4u + rng_range(4));
        msgq_id(SID_S_SLEEP);
        break;
    case 4:  /* enchant armor */
        if (g_worn != SLOT_NONE) {
            g_pack[g_worn].ench++;
            g_pack[g_worn].flags &= (uint8_t)~IF_CURSED;
            g_ac = inv_player_ac();
            msgq_id(SID_S_ARMOR);
        } else {
            msgq_id(SID_S_SKIN);
        }
        break;
    case 5:
        identify_something();
        break;
    case 6:  /* scare monster */
        mons_apply_visible(MEF_FLEE, 15u);
        break;
    case 7:  /* food detection */
        msgq_id(SID_S_FOOD);
        break;
    case 8:  /* teleportation */
        teleport_player();
        msgq_id(SID_S_TELE);
        break;
    case 9:  /* enchant weapon */
        if (g_wield != SLOT_NONE) {
            g_pack[g_wield].ench++;
            g_pack[g_wield].flags &= (uint8_t)~IF_CURSED;
            msgq_id(SID_S_WEAPON);
        } else {
            msgq_id(SID_S_SKIN);
        }
        break;
    case 10:
        spawn_one_adjacent();
        break;
    case 11: /* remove curse */
        {
            uint8_t i;
            for (i = 0; i < PACK_SLOTS; i++)
                g_pack[i].flags &= (uint8_t)~IF_CURSED;
        }
        msgq_id(SID_S_PROTECT);
        break;
    default: /* aggravate monsters */
        {
            uint8_t i;
            for (i = 0; i < MAX_MONSTERS; i++)
                if (g_mons[i].kind != MON_NONE)
                    g_mons[i].state |= MST_AWAKE;
        }
        msgq_id(SID_S_ROAR);
        break;
    }
    return 1;
}

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
        return read_scroll(slot);
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
