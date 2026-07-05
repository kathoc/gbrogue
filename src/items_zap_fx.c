#include <gb/gb.h>
#pragma bank 2
#include "items_zap.h"
#include "items.h"
#include "inventory.h"
#include "identify.h"
#include "world.h"
#include "map.h"
#include "actor.h"
#include "monsters.h"
#include "rng.h"
#include "msg.h"
#include "lang.h"
#include "render.h"

/*
 * BANK2. The post-aim wand effect, relocated out of the (full) HOME bank.
 * Reached only via call_bank(2, bank_zap_effect) from items_zap() in HOME,
 * which marshals the aim through the g_zap_* globals below.
 *
 * call_bank maps bank 2 over bank 1 (interrupts masked), so this code may
 * only call functions in the fixed bank 0. Every callee here was placed
 * there: rng (bank0_rng), map and mon helpers (already bank 0),
 * identify_learn, and the freshly relocated msgq (bank0_msgq), mkind /
 * monster_roll_hp (bank0_monster), render_flash_add (bank0_flash). It
 * never struct-assigns and never renders: messages/kills are enqueued via
 * msgq and replayed by msgq_flush() in the orchestrator after call_bank.
 */

/* Marshalling globals (WRAM, bank-independent). */
uint8_t g_zap_slot;
int8_t  g_zap_dx, g_zap_dy;
uint8_t g_zap_turns;

static monster_t *ray_hit(int8_t dx, int8_t dy) {
    uint8_t x = g_px, y = g_py;
    uint8_t range;
    for (range = 0; range < 24u; range++) {
        monster_t *m;
        x = (uint8_t)(x + dx);
        y = (uint8_t)(y + dy);
        if (!map_walkable(x, y) && map_terrain(x, y) != TI_DOOR) return 0;
        m = mon_at(x, y);
        if (m) return m;
    }
    return 0;
}

static void bolt_damage(monster_t *m, uint8_t dmg, const char *what) {
    uint8_t kind = m->kind;
    /* Queued in order: the bolt line, then (on a kill) the kill + level-up
       lines. msgq preserves insertion order and copies the literal now
       (while BANK2 is mapped), so nothing renders mid-processing. */
    msgq_str(what);
    render_flash_add(m->x, m->y, FLASH_HIT,
                     (uint8_t)(SPR_MON0 + (m - g_mons)));
    if (mon_damage(m, dmg)) {
        msgq_kill(kind);
    }
}

/* call_bank(2, ...) entry: reads the marshalled aim, applies the wand. */
void bank_zap_effect(void) {
    uint8_t slot = g_zap_slot;
    int8_t  dx = g_zap_dx, dy = g_zap_dy;
    item_t *it = &g_pack[slot];
    monster_t *m;

    it->qty--;                       /* charges live in qty */
    identify_learn(IDC_WAND, it->sub);

    m = ray_hit(dx, dy);

    switch (it->sub) {
    case 0:  /* light */
        msgq_id(SID_W_GLOW);
        break;
    case 1:  /* invisibility */
        if (m) { m->eff |= MEF_INVIS; msgq_id(SID_W_VANISH); }
        else msgq_id(SID_S_NOTHING);
        break;
    case 2:  /* lightning */
        if (m) bolt_damage(m, rng_dice(6, 6), "A bolt of lightning!");
        else msgq_id(SID_W_FIZZLE);
        break;
    case 3:  /* fire */
        if (m) bolt_damage(m, rng_dice(6, 6), "A burst of flame!");
        else msgq_id(SID_W_FIZZLE);
        break;
    case 4:  /* cold */
        if (m) bolt_damage(m, rng_dice(6, 6), "An icy blast!");
        else msgq_id(SID_W_FIZZLE);
        break;
    case 5:  /* polymorph */
        if (m) {
            m->kind = rng_range(MKIND_COUNT);
            m->hp = monster_roll_hp(m->kind);
            m->state |= MST_AWAKE;
            msgq_id(SID_W_POLY);
        } else msgq_id(SID_S_NOTHING);
        break;
    case 6:  /* magic missile — always hits */
        if (m) bolt_damage(m, rng_dice(1, 4), "A magic missile!");
        else msgq_id(SID_W_FIZZLE);
        break;
    case 7:  /* haste monster */
        if (m) { m->eff |= MEF_HASTE; m->state |= MST_AWAKE; msgq_id(SID_W_HASTE); }
        else msgq_id(SID_S_NOTHING);
        break;
    case 8:  /* slow monster */
        if (m) { m->eff |= MEF_SLOW; msgq_id(SID_W_SLOW); }
        else msgq_id(SID_S_NOTHING);
        break;
    case 9:  /* drain life: half your HP hits every visible monster */
        if (g_hp > 1u) {
            uint8_t d = (uint8_t)(g_hp / 2u);
            uint8_t i;
            g_hp = (uint8_t)(g_hp - d);
            for (i = 0; i < MAX_MONSTERS; i++) {
                monster_t *v = &g_mons[i];
                uint8_t kind;
                if (v->kind == MON_NONE) continue;
                kind = v->kind;
                if (mon_damage(v, d)) {
                    msgq_kill(kind);
                }
            }
            msgq_id(SID_W_DRAIN);
        } else msgq_id(SID_W_WEAK);
        break;
    case 10: /* nothing */
        msgq_id(SID_S_NOTHING);
        break;
    case 11: /* teleport away */
        if (m) {
            uint8_t t;
            for (t = 0; t < 100u; t++) {
                uint8_t x = rng_range(MAP_W), y = rng_range(MAP_H);
                if (!map_walkable(x, y) || mon_at(x, y)) continue;
                if (x == g_px && y == g_py) continue;
                m->x = x; m->y = y;
                break;
            }
            msgq_id(SID_W_AWAY);
        } else msgq_id(SID_S_NOTHING);
        break;
    case 12: /* teleport to */
        if (m) {
            uint8_t t;
            for (t = 0; t < 12u; t++) {
                uint8_t x = (uint8_t)(g_px - 1u + rng_range(3));
                uint8_t y = (uint8_t)(g_py - 1u + rng_range(3));
                if ((x == g_px && y == g_py) || !map_walkable(x, y)) continue;
                if (mon_at(x, y)) continue;
                m->x = x; m->y = y;
                m->state |= MST_AWAKE;
                break;
            }
            msgq_id(SID_W_TO);
        } else msgq_id(SID_S_NOTHING);
        break;
    default: /* cancellation */
        if (m) { m->eff = 0; msgq_id(SID_W_DULL); }
        else msgq_id(SID_S_NOTHING);
        break;
    }
    g_zap_turns = 1;
}
