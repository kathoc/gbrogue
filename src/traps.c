#include "traps.h"
#include "map.h"
#include "world.h"
#include "rng.h"
#include "msg.h"
#include "lang.h"
#include "render.h"
#include "sfx.h"

trap_t  g_traps[MAX_TRAPS];
uint8_t g_trap_count;

void traps_clear(void) {
    g_trap_count = 0;
}

void traps_add(uint8_t x, uint8_t y, uint8_t kind) {
    if (g_trap_count >= MAX_TRAPS) return;
    g_traps[g_trap_count].x = x;
    g_traps[g_trap_count].y = y;
    g_traps[g_trap_count].kind = kind;
    g_trap_count++;
}

static trap_t *trap_at(uint8_t x, uint8_t y) {
    uint8_t i;
    for (i = 0; i < g_trap_count; i++)
        if (g_traps[i].x == x && g_traps[i].y == y) return &g_traps[i];
    return 0;
}

uint8_t traps_step(void) {
    trap_t *t;
    if (map_terrain(g_px, g_py) != TI_TRAP) return 0;
    t = trap_at(g_px, g_py);
    map_clear_flag(g_px, g_py, MF_HIDDEN);
    if (!t) return 0;

    if (g_levit_t) {
        msg_post_id(SID_T_FLOAT);
        return 0;
    }

    sfx_play(SFX_TRAP);
    switch (t->kind) {
    case TR_TRAPDOOR:
        return 1;                      /* game.c descends + popup */
    case TR_BEAR:
        g_held_t = (uint8_t)(4u + rng_range(4));
        msg_post_id(SID_T_BEAR);
        break;
    case TR_SLEEP:
        g_sleep_t = (uint8_t)(2u + rng_range(3));
        msg_post_id(SID_T_GAS);
        break;
    case TR_ARROW:
        if (rng_byte() < 150u) {
            uint8_t d = rng_dice(1, 6);
            msg_post_id(SID_T_ARROW);
            render_flash_add(g_px, g_py, FLASH_HURT, SPR_PLAYER);
            if (d >= g_hp) {
                g_hp = 0;
                msg_death(SID_DEATH_ARROW, 0);
            } else g_hp -= d;
        } else {
            msg_post_id(SID_T_ARROW_MISS);
        }
        break;
    case TR_DART:
        if (rng_byte() < 150u) {
            uint8_t d = rng_dice(1, 4);
            msg_post_id(SID_T_DART);
            render_flash_add(g_px, g_py, FLASH_HURT, SPR_PLAYER);
            if (d >= g_hp) {
                g_hp = 0;
                msg_death(SID_DEATH_DART, 0);
            } else g_hp -= d;
            if (g_str > 3) g_str--;    /* sustain ring checked by caller? keep raw */
        } else {
            msg_post_id(SID_T_DART_MISS);
        }
        break;
    case TR_TELEPORT: {
        uint8_t tries;
        for (tries = 0; tries < 200u; tries++) {
            uint8_t x = rng_range(MAP_W);
            uint8_t y = rng_range(MAP_H);
            if (!map_walkable(x, y)) continue;
            g_px = x;
            g_py = y;
            break;
        }
        msg_post_id(SID_S_TELE);
        break;
    }
    default:
        break;
    }
    return 0;
}

void traps_search(void) {
    uint8_t i, found = 0;
    for (i = 0; i < g_trap_count; i++) {
        trap_t *t = &g_traps[i];
        if ((uint8_t)(t->x - g_px + 1u) <= 2u &&
            (uint8_t)(t->y - g_py + 1u) <= 2u &&
            (map_cell(t->x, t->y) & MF_HIDDEN)) {
            if (rng_byte() < 128u) {   /* 50% per search, like Rogue */
                map_clear_flag(t->x, t->y, MF_HIDDEN);
                found = 1;
            }
        }
    }
    if (found) msg_post_id(SID_T_FOUND);
}
