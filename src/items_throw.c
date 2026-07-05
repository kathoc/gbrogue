#include "items_throw.h"
#include "items.h"
#include "items_zap.h"
#include "inventory.h"
#include "world.h"
#include "map.h"
#include "actor.h"
#include "monsters.h"
#include "combat.h"
#include "rng.h"
#include "msg.h"
#include "lang.h"
#include "render.h"

/* WEAPON subtypes */
#define WS_BOW     2u
#define WS_ARROW   3u
#define WS_DART    6u
#define WS_SHURIKEN 7u

static uint8_t find_ammo(uint8_t *bonus_hit) {
    uint8_t i;
    uint8_t have_bow = (g_wield != SLOT_NONE &&
                        g_pack[g_wield].kind == IK_WEAPON &&
                        g_pack[g_wield].sub == WS_BOW);
    *bonus_hit = 0;
    if (have_bow) {
        for (i = 0; i < PACK_SLOTS; i++)
            if (g_pack[i].kind == IK_WEAPON && g_pack[i].sub == WS_ARROW) {
                *bonus_hit = 2;        /* launcher match, like Rogue */
                return i;
            }
    }
    for (i = 0; i < PACK_SLOTS; i++)
        if (g_pack[i].kind == IK_WEAPON &&
            (g_pack[i].sub == WS_DART || g_pack[i].sub == WS_SHURIKEN))
            return i;
    return SLOT_NONE;
}

uint8_t items_throw(void) {
    int8_t dx, dy;
    uint8_t bonus, slot = find_ammo(&bonus);

    if (slot == SLOT_NONE) {
        msg_post_id(SID_TH_NOTHING);
        return 0;
    }
    if (!items_prompt_dir(&dx, &dy)) return 0;

    {
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
                                 (uint8_t)(SPR_MON0 + (m - g_mons)));
                if (mon_damage(m, dmg)) {
                    combat_report_kill(kind);
                    combat_gain_xp(mkind(kind)->exp);
                } else {
                    msg_post_id(SID_TH_HIT);
                }
            } else {
                m->state |= MST_AWAKE;
                msg_post_id(SID_TH_MISS);
            }
        } else {
            msg_post_id(SID_TH_CLATTER);
        }
    }
    inv_consume(slot);
    return 1;
}
