#include <gb/gb.h>
#pragma bank 2
#include "items_throw.h"
#include "items.h"
#include "inventory.h"
#include "world.h"
#include "map.h"
#include "actor.h"
#include "monsters.h"
#include "rng.h"
#include "msg.h"
#include "lang.h"
#include "render.h"

/*
 * BANK2. The post-aim projectile flight + hit resolution, out of the full
 * HOME bank. Reached only via call_bank(2, bank_throw_effect) from
 * items_throw() in HOME, which marshals the ammo slot / launcher bonus /
 * aim through the g_throw_* globals below. call_bank maps bank 2 over bank
 * 1, so every callee here is in the fixed bank 0 (map/mon, mkind, rng,
 * WEAPON_DICE, render_flash_add, inv_consume, msgq); it never renders,
 * only enqueues via msgq for the orchestrator to replay.
 */

/* Marshalling globals (WRAM, bank-independent). */
uint8_t g_throw_slot, g_throw_bonus;
int8_t  g_throw_dx, g_throw_dy;
uint8_t g_throw_turns;

/* Index of m within g_mons via pointer increments only: `m - g_mons` would
   divide by sizeof(monster_t) (7), and that divide helper is in bank 1 —
   unreachable from BANK2. Same value. */
static uint8_t mon_index(const monster_t *m) {
    uint8_t i;
    const monster_t *p = g_mons;
    for (i = 0; i < MAX_MONSTERS; i++, p++)
        if (p == m) return i;
    return 0;
}

/* call_bank(2, ...) entry: reads the marshalled aim, flies the shot. */
void bank_throw_effect(void) {
    uint8_t slot = g_throw_slot, bonus = g_throw_bonus;
    int8_t  dx = g_throw_dx, dy = g_throw_dy;
    /* fly until a monster or a wall */
    uint8_t x = g_px, y = g_py, range;
    monster_t *m = 0;
    for (range = 0; range < 10u; range++) {
        x = (uint8_t)(x + dx);
        y = (uint8_t)(y + dy);
        if (!map_walkable(x, y)) break;
        m = mon_at(x, y);
        if (m) break;
    }

    if (m) {
        const item_t *ammo = &g_pack[slot];
        uint8_t kind = m->kind;
        /* to-hit: level + launcher bonus vs monster armor */
        int8_t need = (int8_t)(20 - (int8_t)(g_level + bonus)
                               - mkind(kind)->arm);
        if ((int8_t)rng_range(20) >= need) {
            uint8_t dmg = (uint8_t)(rng_dice(WEAPON_DICE[ammo->sub][0],
                                             WEAPON_DICE[ammo->sub][1])
                                    + (ammo->ench > 0 ? ammo->ench : 0));
            m->state |= MST_AWAKE;
            render_flash_add(m->x, m->y, FLASH_HIT,
                             (uint8_t)(SPR_MON0 + mon_index(m)));
            if (mon_damage(m, dmg)) {
                msgq_kill(kind);
            } else {
                msgq_id(SID_TH_HIT);
            }
        } else {
            m->state |= MST_AWAKE;
            msgq_id(SID_TH_MISS);
        }
    } else {
        msgq_id(SID_TH_CLATTER);
    }
    inv_consume(slot);
    g_throw_turns = 1;
}
