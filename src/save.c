#include <gb/gb.h>
#include <string.h>
#include "save.h"
#include "world.h"
#include "map.h"
#include "actor.h"
#include "items.h"
#include "inventory.h"
#include "identify.h"
#include "traps.h"
#include "rng.h"
#include "worldview.h"

#define SRAM ((uint8_t *)0xA000)
#define SAVE_MAGIC 0x47u          /* 'G' */
#define SAVE_VER   3u

/* identify.c state (exposed for tests + save) */
extern uint8_t  g_id_alias[4][14];
extern uint16_t g_id_known[4];

/*
 * Header: magic, version, 16-bit checksum, 16-bit length.
 * Payload: fixed-order chunks below. Everything is plain bytes; the
 * same code runs both directions via OP_SAVE / OP_LOAD.
 */

static uint16_t cursor;
static uint8_t  op_load;

static void chunk(void *p, uint16_t len) {
    if (op_load) memcpy(p, SRAM + cursor, len);
    else memcpy(SRAM + cursor, p, len);
    cursor += len;
}

/* Chunk table: one row per saved field (much smaller than a call per
   field). Order is the wire format — append only. */
typedef struct { void *p; uint16_t len; } chunk_row_t;
static const chunk_row_t CHUNKS[] = {
    { &g_px, 1 }, { &g_py, 1 }, { &g_depth, 1 },
    { &g_hp, 1 }, { &g_maxhp, 1 },
    { &g_str, 1 }, { &g_maxstr, 1 },
    { &g_ac, 1 }, { &g_level, 1 },
    { &g_xp, 2 }, { &g_gold, 2 }, { &g_turns, 2 },
    { &g_food, 2 },
    { &g_hunger, 1 },
    { &g_conf_t, 1 }, { &g_blind_t, 1 }, { &g_haste_t, 1 },
    { &g_sleep_t, 1 }, { &g_levit_t, 1 }, { &g_seeinv_t, 1 },
    { &g_halluc_t, 1 }, { &g_mondet_t, 1 }, { &g_held_t, 1 },
    { &g_has_amulet, 1 },
    { g_map, MAP_H * MAP_W },
    { g_rooms, sizeof(g_rooms) },
    { &g_room_count, 1 },
    { g_traps, sizeof(g_traps) },
    { &g_trap_count, 1 },
    { g_floor, sizeof(g_floor) },
    { g_pack, sizeof(g_pack) },
    { &g_wield, 1 }, { &g_worn, 1 },
    { &g_ring_l, 1 }, { &g_ring_r, 1 },
    { g_mons, sizeof(g_mons) },
    { g_id_alias, sizeof(g_id_alias) },
    { g_id_known, sizeof(g_id_known) },
    { &g_repeat_speed, 1 },
    { &g_wander_t, 2 },
    { &g_lang, 1 },
};
#define N_CHUNKS (sizeof(CHUNKS) / sizeof(CHUNKS[0]))

static void serialize(void) {
    uint16_t rs;
    uint8_t i;

    cursor = 6;                    /* skip header */

    rs = rng_state();
    chunk(&rs, 2);
    if (op_load) rng_seed(rs);

    for (i = 0; i < N_CHUNKS; i++)
        chunk(CHUNKS[i].p, CHUNKS[i].len);
}

static uint16_t checksum(uint16_t len) {
    uint16_t sum = 0, i;
    for (i = 6; i < len; i++) sum = (uint16_t)(sum + SRAM[i]);
    return sum;
}

uint8_t save_exists(void) {
    uint8_t ok;
    ENABLE_RAM;
    if (SRAM[0] == SAVE_MAGIC && SRAM[1] == SAVE_VER) {
        uint16_t len = (uint16_t)(SRAM[4] | ((uint16_t)SRAM[5] << 8));
        uint16_t sum = (uint16_t)(SRAM[2] | ((uint16_t)SRAM[3] << 8));
        ok = (len > 6u && len < 0x2000u && checksum(len) == sum);
    } else {
        ok = 0;
    }
    DISABLE_RAM;
    return ok;
}

void save_write(void) {
    uint16_t sum;
    ENABLE_RAM;
    op_load = 0;
    serialize();
    SRAM[0] = SAVE_MAGIC;
    SRAM[1] = SAVE_VER;
    SRAM[4] = (uint8_t)cursor;
    SRAM[5] = (uint8_t)(cursor >> 8);
    sum = checksum(cursor);
    SRAM[2] = (uint8_t)sum;
    SRAM[3] = (uint8_t)(sum >> 8);
    DISABLE_RAM;
}

uint8_t save_load(void) {
    if (!save_exists()) return 0;
    ENABLE_RAM;
    op_load = 1;
    serialize();
    /* No consume-on-load anymore: the game autosaves every step, so
       there is never an older snapshot to scum back to. Death and
       victory still wipe the save. */
    DISABLE_RAM;
    view_player_moved();
    return 1;
}

void save_invalidate(void) {
    ENABLE_RAM;
    SRAM[0] = 0;
    DISABLE_RAM;
}

/* Best-run records were replaced by the persistent ranking (rank.c). */
