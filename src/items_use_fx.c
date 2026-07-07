#include <gb/gb.h>
#pragma bank 2
#include "items_use.h"
#include "items.h"
#include "inventory.h"
#include "identify.h"
#include "world.h"
#include "map.h"
#include "actor.h"
#include "monsters.h"
#include "combat.h"
#include "worldview.h"
#include "effects.h"
#include "rng.h"
#include "msg.h"
#include "lang.h"

/*
 * BANK2. The food + potion + scroll effect logic, out of the full HOME bank.
 * Reached via call_bank(2, bank_consume_effect / bank_read_scroll) from
 * items_use() in HOME. call_bank maps bank 2 over bank 1, so every callee
 * here is in the fixed bank 0 (rng, identify_*, inv_consume, map_*, mon_at,
 * monster_pick/roll_hp, view_visible, view_player_moved_ram, item_name ->
 * class_name -> fmt_str/lang_*) and no line renders: messages/xp are
 * enqueued via msgq for the orchestrator to replay. The teleport case moves
 * the player in RAM only (view_player_moved_ram); the world repaint is
 * deferred to view_world_enter() when the pack UI closes.
 */

/* Marshalling globals (WRAM, bank-independent). */
uint8_t g_use_slot, g_use_turns;

/* ---------------------------------------------------------------- food */

static uint8_t eat(uint8_t slot) {
    uint16_t add = (uint16_t)(750u + (uint16_t)rng_byte() * 3u);
    g_food = (uint16_t)(g_food + add);
    if (g_food > 2000u) g_food = 2000u;
    if (g_food > 300u) g_hunger = 0;
    msgq_id(SID_YUM);
    inv_consume(slot);
    return 1;
}

/* -------------------------------------------------------------- potions */

static void heal(uint8_t dice_sides) {
    uint8_t h = rng_dice(g_level, dice_sides);
    if ((uint16_t)g_hp + h >= g_maxhp) {
        if (g_hp == g_maxhp) g_maxhp++;   /* overheal nudges max, like Rogue */
        g_hp = g_maxhp;
    } else {
        g_hp = (uint8_t)(g_hp + h);
    }
    if (g_blind_t) g_blind_t = 0;
    msgq_id(SID_P_HEAL);
}

static uint8_t quaff(uint8_t slot) {
    uint8_t sub = g_pack[slot].sub;
    identify_learn(IDC_POTION, sub);
    inv_consume(slot);

    switch (sub) {
    case 0:  /* confusion */
        g_conf_t = (uint8_t)(12u + rng_range(8));
        msgq_id(SID_P_CONF);
        break;
    case 1:  /* hallucination */
        g_halluc_t = 120u;
        msgq_id(SID_P_HALLU);
        break;
    case 2:  /* poison */
        if (effects_ring_worn(2u)) {          /* sustain strength */
            msgq_id(SID_P_SICK_M);
        } else {
            uint8_t loss = (uint8_t)(1u + rng_range(3));
            g_str = (g_str > (uint8_t)(loss + 3u)) ? (uint8_t)(g_str - loss) : 3u;
            msgq_id(SID_P_SICK);
        }
        break;
    case 3:  /* gain strength */
        if (g_str < 31u) g_str++;
        if (g_str > g_maxstr) g_maxstr = g_str;
        msgq_id(SID_P_STR);
        break;
    case 4:  /* see invisible */
        g_seeinv_t = 200u;
        msgq_id(SID_P_SEEINV);
        break;
    case 5:  /* healing */
        heal(4);
        break;
    case 6:  /* monster detection */
        g_mondet_t = 60u;
        msgq_id(SID_P_SEEMON);
        break;
    case 7:  /* magic detection */
        msgq_id(SID_P_SEEMAGIC);
        break;
    case 8:  /* raise level */
        /* Both deferred so the pair stays ordered (P_RAISE then the
           welcome-to-level line combat_gain_xp posts) and neither renders
           mid-processing here in BANK2. */
        msgq_id(SID_P_RAISE);
        /* g_level*12 via shifts (12 = 8+4) so it never calls __mulint,
           which lives in bank 1 (unreachable from BANK2). */
        msgq_xp((uint16_t)(((uint16_t)g_level << 3) + ((uint16_t)g_level << 2) + 10u));
        break;
    case 9:  /* extra healing */
        heal(8);
        break;
    case 10: /* haste self */
        g_haste_t = (uint8_t)(10u + rng_range(10));
        msgq_id(SID_P_HASTE);
        break;
    case 11: /* restore strength */
        g_str = g_maxstr;
        msgq_id(SID_P_FIXSTR);
        break;
    case 12: /* blindness */
        g_blind_t = 120u;
        msgq_id(SID_P_BLIND);
        break;
    default: /* levitation */
        g_levit_t = 30u;
        msgq_id(SID_P_LEVIT);
        break;
    }
    return 1;
}

/* -------------------------------------------------------------- scrolls */

static void magic_map(void) {
    uint8_t x, y;
    for (y = 0; y < MAP_H; y++)
        for (x = 0; x < MAP_W; x++)
            if (map_terrain(x, y) != TI_BLANK)
                map_set_explored(x, y);
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
    case 8:  /* teleportation — move in RAM only; view_world_enter() on pack
                close does the full repaint (view_player_moved renders, so it
                can't run here in BANK2). Mirrors teleport_player()'s loop. */
        {
            uint8_t tries;
            for (tries = 0; tries < 200u; tries++) {
                uint8_t x = rng_range(MAP_W);
                uint8_t y = rng_range(MAP_H);
                if (!map_walkable(x, y)) continue;
                g_px = x;
                g_py = y;
                view_player_moved_ram();
                break;
            }
        }
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
            for (i = 0; i < PACK_SLOTS; i++) {
                /* lifting the curse doesn't just free the item — it flips
                   the penalty into an equal bonus (-5 -> +5), so a strong
                   piece dragged negative by a combine pays off once cleansed */
                if ((g_pack[i].flags & IF_CURSED) && g_pack[i].ench < 0)
                    g_pack[i].ench = (int8_t)(-g_pack[i].ench);
                g_pack[i].flags &= (uint8_t)~IF_CURSED;
            }
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

/* call_bank(2, ...) entry: eat or quaff the marshalled pack slot. */
void bank_consume_effect(void) {
    uint8_t slot = g_use_slot;
    if (g_pack[slot].kind == IK_FOOD) g_use_turns = eat(slot);
    else                              g_use_turns = quaff(slot);
}

/* call_bank(2, ...) entry: read the marshalled scroll slot. */
void bank_read_scroll(void) {
    g_use_turns = read_scroll(g_use_slot);
}

/* ------------------------------------------------------- combine (#3) */
/* Kept in BANK2 (with the other bulky item logic) so HOME/bank0 stays
   under 0x8000; ui_inv reaches both entries via call_bank(2, ...). */

/* Another pack slot holding the same combinable weapon/armor, else
   SLOT_NONE. Ammo (arrows/darts/shuriken) stacks instead; rings and
   consumables never merge. */
static uint8_t find_dup(uint8_t slot) {
    uint8_t k = g_pack[slot].kind, s = g_pack[slot].sub, i;
    if (k == IK_ARMOR || (k == IK_WEAPON && !WS_THROWABLE(s))) {
        for (i = 0; i < PACK_SLOTS; i++)
            if (i != slot && g_pack[i].kind == k && g_pack[i].sub == s)
                return i;
    }
    return SLOT_NONE;
}

/* call_bank(2, ...) query: g_use_turns <- 1 if g_use_slot has a mergeable
   twin (drives the "combine" verb / exec branch), else 0. */
void bank_dup_query(void) {
    g_use_turns = (uint8_t)(find_dup(g_use_slot) != SLOT_NONE);
}

/* call_bank(2, ...) action: fuse g_use_slot's twin into it. Effective value
   is the enchant when the piece is identified (worn or ever-equipped), else
   -1 (a small, cursed nub); magnitudes add and the sum is cursed if either
   piece was unidentified. Identification propagates: if either input was
   worn the result is worn (and thus identified); if both were identified the
   result is identified; if only one was, the result is partially identified
   (its known value survives in sench, the true value hides behind "?"); if
   neither was, the result stays unidentified ("?"). Two unworn pieces fuse
   without being equipped. g_use_turns<-1 on merge, 0 if there was no twin
   (caller then falls back to the normal action). */
void bank_combine(void) {
    uint8_t slot = g_use_slot;
    uint8_t other = find_dup(slot);
    item_t *a, *b;
    uint8_t *slotp;
    int8_t ea, eb, res, known;
    uint8_t mag, cursed, idA, idB, resultWorn, newflags;

    g_use_turns = 0;
    if (other == SLOT_NONE) return;
    a = &g_pack[slot];
    b = &g_pack[other];

    idA = (uint8_t)((a->flags & (IF_WORN | IF_IDENT)) != 0);
    idB = (uint8_t)((b->flags & (IF_WORN | IF_IDENT)) != 0);
    ea = idA ? a->ench : (int8_t)-1;
    eb = idB ? b->ench : (int8_t)-1;
    cursed = (uint8_t)(ea < 0 || eb < 0);
    if (ea < 0) ea = (int8_t)-ea;
    if (eb < 0) eb = (int8_t)-eb;
    mag = (uint8_t)((uint8_t)ea + (uint8_t)eb);
    res = cursed ? (int8_t)-(int8_t)mag : (int8_t)mag;

    /* stash the known value before a->ench is overwritten (partial-ident) */
    known = idA ? a->ench : b->ench;
    resultWorn = (uint8_t)(((a->flags | b->flags) & IF_WORN) != 0);

    /* drop the donor slot; strip any equip ref to it */
    if (g_wield == other) g_wield = SLOT_NONE;
    if (g_worn == other) g_worn = SLOT_NONE;
    b->kind = ITEM_NONE;
    b->flags = 0;

    /* survivor: merged enchant, curse per rule, ident state per propagation */
    a->ench = res;
    newflags = (uint8_t)(a->flags &
        ~(IF_CURSED | IF_KNOWN_CURSED | IF_WORN | IF_IDENT | IF_PARTIAL));
    if (cursed) newflags |= IF_CURSED;
    if (resultWorn)      newflags |= IF_WORN | IF_IDENT;   /* worn => identified */
    else if (idA && idB) newflags |= IF_IDENT;             /* both known */
    else if (idA || idB) { newflags |= IF_PARTIAL; a->sench = known; }
    a->flags = newflags;

    /* only touch the equip pointers when the result stays worn */
    if (resultWorn) {
        slotp = (a->kind == IK_WEAPON) ? &g_wield : &g_worn;
        if (*slotp != SLOT_NONE && *slotp != slot)
            g_pack[*slotp].flags &= (uint8_t)~IF_WORN;
        *slotp = slot;
    }

    inv_compact();                 /* closes the donor gap; g_* follow */
    g_ac = inv_player_ac();
    msgq_id(SID_COMBINED);
    g_use_turns = 1;
}
