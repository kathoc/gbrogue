#include <gb/gb.h>
#pragma bank 5
#include "save.h"
#include "world.h"
#include "map.h"
#include "actor.h"
#include "items.h"
#include "inventory.h"
#include "identify.h"
#include "traps.h"

/*
 * BANK5. Suspend save serialisation. Self-contained: SRAM is copied
 * byte-by-byte (no memcpy — it lives in another bank) and the rng /
 * view calls the old code made are done by the BANK0 shims instead, so
 * nothing here reaches outside BANK5 while it is mapped.
 */
#define SRAM ((uint8_t *)0xA000)
#define SAVE_MAGIC 0x47u          /* 'G' */
#define SAVE_VER   6u          /* 32-bit rng state (g_save_rng widened) */

/* Fixed SRAM layout (see plan_save_bcd PART 1):
 *   [0..5]     header: magic, ver, checksum_lo/hi, len_lo/hi
 *   [6..901]   STATIC:  g_map (896B)              — offset 6, len 896
 *   [902.. ]   DYNAMIC: g_save_rng(4) + chunks + g_explored(112B)
 * The two regions are contiguous with no gap, so the whole-buffer checksum
 * (SRAM[6..len)) equals g_static_sum + dyn_sum. */
#define STATIC_OFF   6u
#define STATIC_LEN   (MAP_H * MAP_W)          /* 896 */
#define DYNAMIC_OFF  (STATIC_OFF + STATIC_LEN) /* 902 */

/* identify.c state (exposed for tests + save) */
extern uint8_t  g_id_alias[4][14];
extern uint16_t g_id_known[4];

uint32_t g_save_rng;
uint8_t  g_save_ok;
uint8_t  g_save_static_dirty;    /* WRAM; set by save_mark_map_dirty() */

static uint16_t cursor;
static uint8_t  op_load;
static uint16_t s_acc;           /* running byte sum during a write pass */
static uint16_t g_static_sum;    /* cached checksum of the STATIC region */

/* byte copy to/from SRAM (no memcpy — that would be a cross-bank call).
   On write the copied bytes are folded into s_acc so the checksum comes for
   free (incremental (B)); the read path leaves s_acc untouched. */
static void chunk(void *p, uint16_t len) {
    uint8_t *pp = (uint8_t *)p;
    uint16_t i;
    if (op_load) {
        for (i = 0; i < len; i++) pp[i] = SRAM[cursor + i];
    } else {
        for (i = 0; i < len; i++) {
            uint8_t b = pp[i];
            SRAM[cursor + i] = b;
            s_acc = (uint16_t)(s_acc + b);
        }
    }
    cursor = (uint16_t)(cursor + len);
}

typedef struct { void *p; uint16_t len; } chunk_row_t;

/* DYNAMIC region: everything that is NOT the static terrain map. g_save_rng
   is written first (before this table) so it heads the DYNAMIC block. */
static const chunk_row_t DYNAMIC_CHUNKS[] = {
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
    { &g_play_frames, 4 },        /* carry the play timer across a suspend */
    { &g_run_seed, 4 },           /* keep the seed so it shows post-resume */
    { g_explored, sizeof(g_explored) },   /* 112B, tail of the DYNAMIC block */
};
#define N_DYNAMIC (sizeof(DYNAMIC_CHUNKS) / sizeof(DYNAMIC_CHUNKS[0]))

/* STATIC region: the terrain map only. Same routine for read and write so
   the layout can never drift between save and load. */
static void serialize_static(void) {
    cursor = STATIC_OFF;
    chunk(g_map, STATIC_LEN);
}

/* DYNAMIC region. rng state rides in g_save_rng (set by the BANK0 shim on
   save; read back there to reseed on load) so this stays out of the rng
   module. Same routine for read and write. */
static void serialize_dynamic(void) {
    uint8_t i;
    cursor = DYNAMIC_OFF;
    chunk(&g_save_rng, 4);
    for (i = 0; i < N_DYNAMIC; i++)
        chunk(DYNAMIC_CHUNKS[i].p, DYNAMIC_CHUNKS[i].len);
}

static uint16_t checksum(uint16_t len) {
    uint16_t sum = 0, i;
    for (i = 6; i < len; i++) sum = (uint16_t)(sum + SRAM[i]);
    return sum;
}

void bank_save_exists(void) {
    ENABLE_RAM;
    if (SRAM[0] == SAVE_MAGIC && SRAM[1] == SAVE_VER) {
        uint16_t len = (uint16_t)(SRAM[4] | ((uint16_t)SRAM[5] << 8));
        uint16_t sum = (uint16_t)(SRAM[2] | ((uint16_t)SRAM[3] << 8));
        g_save_ok = (uint8_t)(len > 6u && len < 0x2000u && checksum(len) == sum);
    } else {
        g_save_ok = 0;
    }
    DISABLE_RAM;
}

void bank_save_write(void) {
    uint16_t sum, dyn_sum, len;
    ENABLE_RAM;
    op_load = 0;
    /* STATIC: only re-flush the 896-byte terrain map when it actually
       changed (dig / trap reveal / new floor). Otherwise the bytes and the
       cached partial sum already in SRAM are reused. */
    if (g_save_static_dirty) {
        s_acc = 0;
        serialize_static();
        g_static_sum = s_acc;
        g_save_static_dirty = 0;
    }
    /* DYNAMIC: always rewritten. cursor ends at the total length. */
    s_acc = 0;
    serialize_dynamic();
    dyn_sum = s_acc;
    len = cursor;
    SRAM[0] = SAVE_MAGIC;
    SRAM[1] = SAVE_VER;
    SRAM[4] = (uint8_t)len;
    SRAM[5] = (uint8_t)(len >> 8);
    sum = (uint16_t)(g_static_sum + dyn_sum);
    SRAM[2] = (uint8_t)sum;
    SRAM[3] = (uint8_t)(sum >> 8);
    DISABLE_RAM;
}

void bank_save_load(void) {
    uint16_t i, s;
    ENABLE_RAM;
    op_load = 1;
    serialize_static();        /* g_map  <- SRAM[6..902) */
    serialize_dynamic();       /* rng + dynamic + g_explored <- SRAM[902..) */
    /* Rebuild the STATIC partial sum so the next incremental save can skip
       re-flushing an unchanged map, and mark the map clean. */
    s = 0;
    for (i = STATIC_OFF; i < DYNAMIC_OFF; i++) s = (uint16_t)(s + SRAM[i]);
    g_static_sum = s;
    g_save_static_dirty = 0;
    DISABLE_RAM;
}

void bank_save_invalidate(void) {
    ENABLE_RAM;
    SRAM[0] = 0;
    DISABLE_RAM;
}
