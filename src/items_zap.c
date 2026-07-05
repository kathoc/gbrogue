#include <gb/gb.h>
#include "items_zap.h"
#include "items.h"
#include "inventory.h"
#include "identify.h"
#include "world.h"
#include "map.h"
#include "actor.h"
#include "monsters.h"
#include "combat.h"
#include "effects.h"
#include "rng.h"
#include "msg.h"
#include "lang.h"
#include "render.h"
#include "input.h"
#include "worldview.h"
#include "status.h"

/* Wait for a D-pad direction (B cancels). The pack modal is dropped
   first so the player aims on the live world, with "Which way?" on the
   message band and "B:cancel" bottom-right. Holding two directions for
   a moment yields a diagonal: after the first edge we give the second
   button a 4-frame grace window, matching the walk input. */
uint8_t g_zap_prompted;

uint8_t items_prompt_dir(int8_t *dx, int8_t *dy) {
    uint8_t picked = 0;
    g_zap_prompted = 1;
    view_world_enter();
    msg_post_id(SID_W_WHICHWAY);
    g_status_hint = SID_HINT_CANCEL;
    status_update();
    msg_refresh();
    /* swallow BEFORE the multi-frame world flush: presses landing
       during the repaint stay latched and register as the aim */
    input_swallow_edges();
    render_present();
    for (;;) {
        uint8_t keys;
        wait_vbl_done();
        view_breathe();
        render_msg_tick();
        keys = input_pressed();
        if (keys & J_B) break;
        if (keys & (J_UP | J_DOWN | J_LEFT | J_RIGHT)) {
            uint8_t grace, held = (uint8_t)(keys | input_held());
            for (grace = 0; grace < 4u; grace++) {
                wait_vbl_done();
                held |= input_held();
            }
            *dx = 0;
            *dy = 0;
            if (held & J_LEFT)  *dx = -1;
            if (held & J_RIGHT) *dx = 1;
            if (held & J_UP)    *dy = -1;
            if (held & J_DOWN)  *dy = 1;
            picked = 1;
            break;
        }
    }
    g_status_hint = 0;
    status_update();
    render_present();
    return picked;
}

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
    msg_post(what);
    render_flash_add(m->x, m->y, FLASH_HIT,
                     (uint8_t)(SPR_MON0 + (m - g_mons)));
    if (mon_damage(m, dmg)) {
        combat_report_kill(kind);
        combat_gain_xp(mkind(kind)->exp);   /* level-up line lands after */
    }
}

uint8_t items_zap(uint8_t slot) {
    item_t *it = &g_pack[slot];
    int8_t dx, dy;
    monster_t *m;

    if (it->qty == 0) {
        msg_post_id(SID_S_NOTHING);
        identify_learn(IDC_WAND, it->sub);
        return 1;
    }
    if (!items_prompt_dir(&dx, &dy)) return 0;
    it->qty--;                       /* charges live in qty */
    identify_learn(IDC_WAND, it->sub);

    m = ray_hit(dx, dy);

    switch (it->sub) {
    case 0:  /* light */
        msg_post_id(SID_W_GLOW);
        break;
    case 1:  /* invisibility */
        if (m) { m->eff |= MEF_INVIS; msg_post_id(SID_W_VANISH); }
        else msg_post_id(SID_S_NOTHING);
        break;
    case 2:  /* lightning */
        if (m) bolt_damage(m, rng_dice(6, 6), "A bolt of lightning!");
        else msg_post_id(SID_W_FIZZLE);
        break;
    case 3:  /* fire */
        if (m) bolt_damage(m, rng_dice(6, 6), "A burst of flame!");
        else msg_post_id(SID_W_FIZZLE);
        break;
    case 4:  /* cold */
        if (m) bolt_damage(m, rng_dice(6, 6), "An icy blast!");
        else msg_post_id(SID_W_FIZZLE);
        break;
    case 5:  /* polymorph */
        if (m) {
            m->kind = rng_range(MKIND_COUNT);
            m->hp = monster_roll_hp(m->kind);
            m->state |= MST_AWAKE;
            msg_post_id(SID_W_POLY);
        } else msg_post_id(SID_S_NOTHING);
        break;
    case 6:  /* magic missile — always hits */
        if (m) bolt_damage(m, rng_dice(1, 4), "A magic missile!");
        else msg_post_id(SID_W_FIZZLE);
        break;
    case 7:  /* haste monster */
        if (m) { m->eff |= MEF_HASTE; m->state |= MST_AWAKE; msg_post_id(SID_W_HASTE); }
        else msg_post_id(SID_S_NOTHING);
        break;
    case 8:  /* slow monster */
        if (m) { m->eff |= MEF_SLOW; msg_post_id(SID_W_SLOW); }
        else msg_post_id(SID_S_NOTHING);
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
                    combat_report_kill(kind);
                    combat_gain_xp(mkind(kind)->exp);
                }
            }
            msg_post_id(SID_W_DRAIN);
        } else msg_post_id(SID_W_WEAK);
        break;
    case 10: /* nothing */
        msg_post_id(SID_S_NOTHING);
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
            msg_post_id(SID_W_AWAY);
        } else msg_post_id(SID_S_NOTHING);
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
            msg_post_id(SID_W_TO);
        } else msg_post_id(SID_S_NOTHING);
        break;
    default: /* cancellation */
        if (m) { m->eff = 0; msg_post_id(SID_W_DULL); }
        else msg_post_id(SID_S_NOTHING);
        break;
    }
    return 1;
}
