#include "inventory.h"
#include "world.h"
#include "msg.h"
#include "lang.h"
#include "util.h"
#include "identify.h"

item_t  g_pack[PACK_SLOTS];
uint8_t g_wield, g_worn, g_ring_l, g_ring_r;

void inv_clear(void) {
    uint8_t i;
    for (i = 0; i < PACK_SLOTS; i++) g_pack[i].kind = ITEM_NONE;
    g_wield = g_worn = g_ring_l = g_ring_r = SLOT_NONE;
}

static uint8_t stackable(uint8_t kind) {
    return kind == IK_FOOD || kind == IK_POTION || kind == IK_SCROLL;
}

uint8_t inv_add(const item_t *it) {
    uint8_t i;
    if (stackable(it->kind) || it->kind == IK_WEAPON) {
        for (i = 0; i < PACK_SLOTS; i++) {
            item_t *p = &g_pack[i];
            if (p->kind == it->kind && p->sub == it->sub &&
                p->ench == it->ench &&
                (stackable(it->kind) ||
                 /* ammo stacks: arrows, darts, shuriken */
                 it->sub == 3u || it->sub == 6u || it->sub == 7u)) {
                p->qty = (uint8_t)(p->qty + (it->qty ? it->qty : 1u));
                return i;
            }
        }
    }
    for (i = 0; i < PACK_SLOTS; i++) {
        if (g_pack[i].kind == ITEM_NONE) {
            g_pack[i] = *it;
            if (g_pack[i].qty == 0) g_pack[i].qty = 1;
            g_pack[i].flags &= (uint8_t)~IF_WORN;
            return i;
        }
    }
    return SLOT_NONE;
}

static void unequip_slot(uint8_t slot) {
    if (g_wield == slot) g_wield = SLOT_NONE;
    if (g_worn == slot) g_worn = SLOT_NONE;
    if (g_ring_l == slot) g_ring_l = SLOT_NONE;
    if (g_ring_r == slot) g_ring_r = SLOT_NONE;
}

/* Close gaps so the pack always lists top-down with no holes; the
   equip indices follow their items. */
void inv_compact(void) {
    uint8_t r, w = 0;
    for (r = 0; r < PACK_SLOTS; r++) {
        if (g_pack[r].kind == ITEM_NONE) continue;
        if (r != w) {
            /* Byte copy, NOT struct '=': a struct assignment compiles to a
               memcpy call, and memcpy sits in a switchable bank — once this
               code moves to BANK2 that call would crash (same reason as
               rank.c). inv_consume() reaches here from banked item use. */
            uint8_t bi;
            uint8_t *d = (uint8_t *)&g_pack[w];
            const uint8_t *s = (const uint8_t *)&g_pack[r];
            for (bi = 0; bi < sizeof(item_t); bi++) d[bi] = s[bi];
            g_pack[r].kind = ITEM_NONE;
            g_pack[r].flags = 0;
            if (g_wield == r) g_wield = w;
            if (g_worn == r) g_worn = w;
            if (g_ring_l == r) g_ring_l = w;
            if (g_ring_r == r) g_ring_r = w;
        }
        w++;
    }
}

void inv_consume(uint8_t slot) {
    item_t *p = &g_pack[slot];
    if (p->kind == ITEM_NONE) return;
    if (p->qty > 1u) {
        p->qty--;
        return;
    }
    unequip_slot(slot);
    p->kind = ITEM_NONE;
    p->flags = 0;
    inv_compact();
}

uint8_t inv_count(void) {
    uint8_t i, n = 0;
    for (i = 0; i < PACK_SLOTS; i++)
        if (g_pack[i].kind != ITEM_NONE) n++;
    return n;
}

uint8_t inv_player_ac(void) {
    int8_t ac = 10;
    if (g_worn != SLOT_NONE) {
        const item_t *a = &g_pack[g_worn];
        ac = (int8_t)(ARMOR_AC[a->sub] - a->ench);
    }
    /* protection ring(s): each +1 ench lowers AC by 1 */
    if (g_ring_l != SLOT_NONE && g_pack[g_ring_l].sub == 0u)
        ac = (int8_t)(ac - g_pack[g_ring_l].ench);
    if (g_ring_r != SLOT_NONE && g_pack[g_ring_r].sub == 0u)
        ac = (int8_t)(ac - g_pack[g_ring_r].ench);
    if (ac < 0) ac = 0;
    return (uint8_t)ac;
}

static void post_cursed(void) {
    msg_post_id(SID_CURSED);
}

/* Single-slot equip toggle (weapon / armor) with modern auto-swap:
   pressing equip on a new item takes off whatever occupies the slot
   first — no manual unequip. A cursed item stuck in the slot blocks
   both taking off and swapping (its curse is revealed instead). */
static uint8_t equip_swap(uint8_t *slotp, uint8_t slot,
                          uint8_t on_sid, uint8_t off_sid) {
    item_t *p = &g_pack[slot];
    if (*slotp == slot) {                 /* already equipped: take off */
        if (p->flags & IF_CURSED) {
            p->flags |= IF_KNOWN_CURSED;
            post_cursed();
            return 1;
        }
        p->flags &= (uint8_t)~IF_WORN;
        *slotp = SLOT_NONE;
        g_ac = inv_player_ac();
        msg_post_id(off_sid);
        return 1;
    }
    if (*slotp != SLOT_NONE) {            /* swap the old one out */
        if (g_pack[*slotp].flags & IF_CURSED) {
            g_pack[*slotp].flags |= IF_KNOWN_CURSED;
            post_cursed();
            return 1;
        }
        g_pack[*slotp].flags &= (uint8_t)~IF_WORN;
    }
    *slotp = slot;
    p->flags |= IF_WORN | IF_IDENT;       /* equipping reveals the enchant for good */
    g_ac = inv_player_ac();               /* weapon leaves AC unchanged */
    msg_post_id(on_sid);
    return 1;
}

uint8_t inv_equip(uint8_t slot) {
    item_t *p = &g_pack[slot];

    switch (p->kind) {
    case IK_WEAPON:
        return equip_swap(&g_wield, slot, SID_WIELD, SID_UNWIELD);

    case IK_ARMOR:
        return equip_swap(&g_worn, slot, SID_WEAR, SID_TAKEOFF);

    case IK_RING:
        if (g_ring_l == slot || g_ring_r == slot) {
            if (p->flags & IF_CURSED) {
                p->flags |= IF_KNOWN_CURSED;
                post_cursed();
                return 1;
            }
            p->flags &= (uint8_t)~IF_WORN;
            unequip_slot(slot);
            g_ac = inv_player_ac();
            msg_post_id(SID_RING_OFF);
            return 1;
        }
        if (g_ring_l == SLOT_NONE) g_ring_l = slot;
        else if (g_ring_r == SLOT_NONE) g_ring_r = slot;
        else {
            msg_post_id(SID_HANDS_FULL);
            return 0;
        }
        p->flags |= IF_WORN;
        /* Putting a ring on reveals what kind it is (its curse, if any,
           still only surfaces when you try to take it off). */
        identify_learn(IDC_RING, p->sub);
        g_ac = inv_player_ac();
        msg_post_id(SID_RING_ON);
        return 1;

    default:
        return 0;
    }
}

void inv_starting_kit(void) {
    item_t it;

    it.x = it.y = 0;
    it.flags = 0;
    it.ench = 0;
    it.sench = 0;

    it.kind = IK_FOOD;   it.sub = 0; it.qty = 1; inv_add(&it);
    /* Nothing else: the hero starts with a single ration only. Weapons,
       armor and ammo (including a bow + arrows) must be found in the
       dungeon; g_wield/g_worn stay SLOT_NONE (bare-handed) from
       inv_clear(). */

    /* The debug ROM used to build a test kit + short hunger fuse here;
       both are now injected by verify_m7/m9 after boot (gbtest.py
       inject_debug_kit), so the debug build is byte-identical to the
       release build and no longer nudges HOME over 0x8000. */

    g_ac = inv_player_ac();
}
