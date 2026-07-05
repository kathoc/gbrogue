#include "inventory.h"
#include "world.h"
#include "msg.h"
#include "lang.h"
#include "util.h"

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
            g_pack[w] = g_pack[r];
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
    p->flags |= IF_WORN;
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
        g_ac = inv_player_ac();
        msg_post_id(SID_RING_ON);
        return 1;

    default:
        return 0;
    }
}

void inv_starting_kit(void) {
    item_t it;
    uint8_t s;

    it.x = it.y = 0;
    it.flags = 0;
    it.ench = 0;

    it.kind = IK_FOOD;   it.sub = 0; it.qty = 1; inv_add(&it);
    it.kind = IK_WEAPON; it.sub = 0; it.qty = 1; it.ench = 1;   /* mace +1 */
    s = inv_add(&it);
    g_wield = s;
    g_pack[s].flags |= IF_WORN;
    it.kind = IK_ARMOR;  it.sub = 1; it.qty = 1; it.ench = 1;   /* ring mail +1 */
    s = inv_add(&it);
    g_worn = s;
    g_pack[s].flags |= IF_WORN;
    it.kind = IK_WEAPON; it.sub = 2; it.qty = 1; it.ench = 0;   /* short bow */
    inv_add(&it);
    it.kind = IK_WEAPON; it.sub = 3; it.qty = 25;               /* arrows */
    inv_add(&it);

#ifdef GBR_DEBUG_KIT
    /* Short hunger fuse for the M7 hunger check. The deterministic test
       items themselves are no longer built here — verify_m7 injects them
       straight into g_pack after boot (keeps this out of the packed HOME
       bank; the on-cart debug build otherwise overflows into VRAM). */
    g_food = 340u;
#endif

    g_ac = inv_player_ac();
}
