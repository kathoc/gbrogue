#include "effects.h"
#include "world.h"
#include "inventory.h"
#include "map.h"
#include "worldview.h"
#include "rng.h"
#include "msg.h"
#include "lang.h"
#include "traps.h"

/* ring subtypes (items_data.c order) */
#define RS_PROTECT   0u
#define RS_ADDSTR    1u
#define RS_SUSTSTR   2u
#define RS_SEARCH    3u
#define RS_SEEINV    4u
#define RS_AGGRAVATE 6u
#define RS_DEX       7u
#define RS_DAMAGE    8u
#define RS_REGEN     9u
#define RS_SLOWDIG   10u
#define RS_TELEPORT  11u

static const item_t *ring(uint8_t which) {
    uint8_t slot = which ? g_ring_r : g_ring_l;
    if (slot == SLOT_NONE) return 0;
    return &g_pack[slot];
}

uint8_t effects_ring_worn(uint8_t sub) {
    const item_t *r;
    r = ring(0);
    if (r && r->sub == sub) return 1;
    r = ring(1);
    if (r && r->sub == sub) return 1;
    return 0;
}

int8_t effects_ring_ench_sum(uint8_t sub) {
    int8_t total = 0;
    const item_t *r;
    r = ring(0);
    if (r && r->sub == sub) total = (int8_t)(total + r->ench);
    r = ring(1);
    if (r && r->sub == sub) total = (int8_t)(total + r->ench);
    return total;
}

void teleport_player(void) {
    uint8_t tries;
    for (tries = 0; tries < 200u; tries++) {
        uint8_t x = rng_range(MAP_W);
        uint8_t y = rng_range(MAP_H);
        if (!map_walkable(x, y)) continue;
        g_px = x;
        g_py = y;
        view_player_moved();
        return;
    }
}

static void tick(uint8_t *t) {
    if (*t) (*t)--;
}

static void hunger_turn(void) {
    uint8_t drain, extra = 0, old_state = g_hunger;
    const item_t *r;

    r = ring(0);
    if (r && r->sub != RS_SLOWDIG) extra++;
    r = ring(1);
    if (r && r->sub != RS_SLOWDIG) extra++;

    /* slow digestion: base drain only every other turn */
    if (effects_ring_worn(RS_SLOWDIG) && (g_turns & 1u)) drain = extra;
    else drain = (uint8_t)(1u + extra);

    if (drain >= g_food) g_food = 0;
    else g_food = (uint16_t)(g_food - drain);

    if (g_food > 300u) g_hunger = 0;
    else if (g_food > 150u) g_hunger = 1;
    else if (g_food > 20u) g_hunger = 2;
    else g_hunger = 3;

    if (g_hunger != old_state) {
        if (g_hunger == 1u) msg_post_id(SID_H_HUNGRY);
        else if (g_hunger == 2u) msg_post_id(SID_H_WEAK);
        else if (g_hunger == 3u) msg_post_id(SID_H_FAINT);
    }
    if (g_food == 0) {
        msg_post_id(SID_H_STARVE);
        g_hp = 0;
        msg_death(SID_DEATH_STARVE, 0);
    }
}

void effects_turn(void) {
    tick(&g_conf_t);
    tick(&g_blind_t);
    tick(&g_haste_t);
    tick(&g_levit_t);
    tick(&g_seeinv_t);
    tick(&g_halluc_t);
    tick(&g_mondet_t);
    tick(&g_held_t);
    /* g_sleep_t is consumed by the game loop itself */

    hunger_turn();
    if (!g_hp) return;

    if (effects_ring_worn(RS_REGEN) && g_hp < g_maxhp) g_hp++;
    if (effects_ring_worn(RS_SEARCH)) traps_search();
    if (effects_ring_worn(RS_TELEPORT) && rng_byte() < 5u) {
        msg_post_id(SID_S_TELE);
        teleport_player();
    }
    /* add strength ring: applied live to the stat while worn */
}
