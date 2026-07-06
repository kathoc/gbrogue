#include <gb/gb.h>
#include "worldview.h"
#include "map.h"
#include "world.h"
#include "render.h"
#include "actor.h"
#include "monsters.h"
#include "items.h"
#include "input.h"

uint8_t g_cam_x, g_cam_y;
uint8_t g_cur_room;

/* ------------------------------------------------------------ explore */

static void mark_room_explored(uint8_t idx);
static void paint_rect(uint8_t x0, uint8_t y0, uint8_t w, uint8_t h);

static void mark_around_explored(void) {
    uint8_t x, y;
    for (y = (uint8_t)(g_py - 1u); y != (uint8_t)(g_py + 2u); y++)
        for (x = (uint8_t)(g_px - 1u); x != (uint8_t)(g_px + 2u); x++)
            map_set_flag(x, y, MF_EXPLORED);
}

/* ------------------------------------------------------------- camera */

uint8_t view_cam_px(void) {
    uint16_t p = (uint16_t)g_px * 8u;
    if (p <= 72u) return 0;
    p -= 72u;
    if (p > (WORLD_W * 8u - 160u)) p = WORLD_W * 8u - 160u;
    return (uint8_t)p;
}

uint8_t view_cam_py(void) {
    uint16_t p = (uint16_t)g_py * 8u;
    if (p <= 64u) return 0;
    p -= 64u;
    if (p > (WORLD_H * 8u - 128u)) p = WORLD_H * 8u - 128u;
    return (uint8_t)p;
}

static void update_camera(void) {
    g_cam_x = (uint8_t)(view_cam_px() / 8u);
    g_cam_y = (uint8_t)(view_cam_py() / 8u);
}

void view_player_moved(void) {
    g_cur_room = map_room_at(g_px, g_py);
    if (g_cur_room != 0xFFu) mark_room_explored(g_cur_room);
    /* Corridor steps — and door thresholds — always reveal the ring
       around the player. */
    mark_around_explored();
    if (render_world_on()) view_worldpaint_around();
    update_camera();
}

/* view_visible() and view_player_moved_ram() live in bank0_view.c (fixed
   bank) so banked scroll logic (BANK2 FOV/teleport) can reach them. */

/* -------------------------------------------------- world BG painting */

/* Displayable terrain of a cell (hidden traps pose as floor). */
static tile_id_t cell_terrain_shown(uint8_t cell) {
    tile_id_t t = (tile_id_t)(cell & MF_TERRAIN);
    if (t == TI_TRAP && (cell & MF_HIDDEN)) t = TI_FLOOR;
    return t;
}

static void paint_cell(uint8_t x, uint8_t y) {
    uint8_t cell = map_cell(x, y);
    if (cell & MF_EXPLORED) {
        const item_t *it = item_floor_at(x, y);
        if (it) {
            render_world_cell(x, y, (tile_id_t)item_tile(it->kind));
            return;
        }
        render_world_cell(x, y, cell_terrain_shown(cell));
    } else {
        render_world_cell(x, y, TI_BLANK);
    }
}

static void paint_rect(uint8_t x0, uint8_t y0, uint8_t w, uint8_t h) {
    uint8_t x, y;
    for (y = y0; y != (uint8_t)(y0 + h); y++)
        for (x = x0; x != (uint8_t)(x0 + w); x++)
            paint_cell(x, y);
}

static void mark_room_explored(uint8_t idx) {
    room_t *r = &g_rooms[idx];
    uint8_t x, y;
    if (r->flags & ROOM_EXPLORED) return;
    r->flags |= ROOM_EXPLORED;
    for (y = r->y; y < (uint8_t)(r->y + r->h); y++)
        for (x = r->x; x < (uint8_t)(r->x + r->w); x++)
            map_set_flag(x, y, MF_EXPLORED);
    if (render_world_on()) paint_rect(r->x, r->y, r->w, r->h);
}

void view_worldpaint_full(void) {
    uint8_t x, y, i;
    /* Terrain pass first (no per-cell item scan), then overlay the few
       floor items — much cheaper than paint_cell() per cell. */
    for (y = 0; y < MAP_H; y++) {
        input_tick();               /* stay responsive during the paint */
        for (x = 0; x < MAP_W; x++) {
            uint8_t cell = g_map[y][x];
            render_world_cell(x, y, (cell & MF_EXPLORED)
                                        ? cell_terrain_shown(cell)
                                        : TI_BLANK);
        }
    }
    for (i = 0; i < MAX_FLOOR_ITEMS; i++) {
        const item_t *it = &g_floor[i];
        if (it->kind == ITEM_NONE) continue;
        if (!(map_cell(it->x, it->y) & MF_EXPLORED)) continue;
        render_world_cell(it->x, it->y, (tile_id_t)item_tile(it->kind));
    }
}

void view_worldpaint_around(void) {
    uint8_t x, y;
    for (y = (uint8_t)(g_py - 1u); y != (uint8_t)(g_py + 2u); y++)
        for (x = (uint8_t)(g_px - 1u); x != (uint8_t)(g_px + 2u); x++)
            if (x < MAP_W && y < MAP_H) paint_cell(x, y);
}

/* -------------------------------------------------------------- sprites */

static char mon_glyph(const monster_t *m, uint8_t idx) {
    if (g_halluc_t)
        return (char)('A' + ((m->x ^ (uint8_t)g_turns ^ idx) % 26u));
    return (char)('A' + m->kind);
}

static uint8_t mon_shown(const monster_t *m) {
    if (m->kind == MON_NONE) return 0;
    if (((mkind(m->kind)->flags & MFL_INVIS) || (m->eff & MEF_INVIS)) &&
        !g_seeinv_t) return 0;
    if (!view_visible(m->x, m->y) && !g_mondet_t) return 0;
    return 1;
}

uint8_t view_mon_shown_idx(uint8_t i) {
    return mon_shown(&g_mons[i]);
}

/* Breathing: every actor sinks one pixel for half of its cycle,
   phase-shifted by its slot so the dungeon doesn't bob in lockstep.
   The offset only ever goes downward, so an actor never crosses into
   the tile row above (keeps the screen-to-cell math exact).

   The half-cycle length adapts to each actor's tempo, so the breath
   *reads* its state at a glance: a normal actor is 32 frames (~1s
   round-trip), a sleeping monster 96 (3x, slow drowsing), a hasted
   one 16 (twice as fast), a slowed one 64. All periods divide 16, so
   re-syncing every 16 frames catches every transition. */
static uint16_t breath_frames;

static uint8_t bob(uint8_t slot, uint8_t period) {
    return (uint8_t)(((breath_frames / period) + slot) & 1u);
}

static uint8_t player_breath_period(void) {
    return g_haste_t ? 16u : 32u;              /* hasted: breathe fast */
}

static uint8_t mon_breath_period(const monster_t *m) {
    if (!(m->state & MST_AWAKE)) return 96u;   /* asleep: slow drowsing */
    if (m->eff & MEF_HASTE)      return 16u;   /* sped up: fast */
    if (m->eff & MEF_SLOW)       return 64u;   /* slowed down: sluggish */
    return 32u;
}

void view_sync_sprites(void) {
    uint8_t i, scx = SCX_REG, scy = SCY_REG;

    if (g_render_mode) render_sprite_id(SPR_PLAYER, TI_PLAYER);
    else render_sprite_glyph(SPR_PLAYER, '@');
    render_sprite_pos(SPR_PLAYER,
                      (uint8_t)(g_px * 8u - scx),
                      (uint8_t)(g_py * 8u - scy + bob(0, player_breath_period())));

    for (i = 0; i < MAX_MONSTERS; i++) {
        const monster_t *m = &g_mons[i];
        uint8_t sx, sy;
        if (!mon_shown(m)) {
            render_sprite_hide((uint8_t)(SPR_MON0 + i));
            continue;
        }
        sx = (uint8_t)(m->x * 8u - scx);
        sy = (uint8_t)(m->y * 8u - scy);
        if (sx >= 160u || sy >= 128u) {
            render_sprite_hide((uint8_t)(SPR_MON0 + i));
            continue;
        }
        render_sprite_glyph((uint8_t)(SPR_MON0 + i), mon_glyph(m, i));
        render_sprite_pos((uint8_t)(SPR_MON0 + i), sx,
                          (uint8_t)(sy + bob((uint8_t)(1u + i),
                                             mon_breath_period(m))));
    }
}

void view_breathe(void) {
    breath_frames++;
    if ((breath_frames & 15u) == 0u) view_sync_sprites();
}

/*
 * Attack lunges: the attacker's sprite jabs a few pixels toward its
 * victim and springs back. Combat queues them (it runs before the
 * screen is repainted); finish_turn plays the queue after the move
 * glide, one attacker at a time.
 */
#define LUNGE_MAX 4u
static uint8_t lunge_spr[LUNGE_MAX];
static int8_t  lunge_dx[LUNGE_MAX], lunge_dy[LUNGE_MAX];
static uint8_t lunge_n;

void view_lunge_add(uint8_t spr, int8_t dx, int8_t dy) {
    if (lunge_n >= LUNGE_MAX) return;
    lunge_spr[lunge_n] = spr;
    lunge_dx[lunge_n] = dx;
    lunge_dy[lunge_n] = dy;
    lunge_n++;
}

void view_lunge_play(void) {
    static const int8_t JAB[5] = { 2, 4, 4, 2, 0 };
    uint8_t i, f;
    uint8_t scx, scy;

    if (!lunge_n) return;
    if (!render_world_on()) {
        lunge_n = 0;
        return;
    }
    scx = SCX_REG;
    scy = SCY_REG;
    for (i = 0; i < lunge_n; i++) {
        uint8_t spr = lunge_spr[i];
        uint8_t wx, wy;
        if (spr == SPR_PLAYER) {
            wx = g_px;
            wy = g_py;
        } else {
            const monster_t *m = &g_mons[spr - SPR_MON0];
            if (!mon_shown(m)) continue;       /* invisible: no tell */
            wx = m->x;
            wy = m->y;
        }
        for (f = 0; f < 5u; f++) {
            uint8_t sx = (uint8_t)(wx * 8u - scx + lunge_dx[i] * JAB[f]);
            uint8_t sy = (uint8_t)(wy * 8u - scy + lunge_dy[i] * JAB[f]);
            wait_vbl_done();
            input_tick();
            render_msg_tick();
            render_sprite_pos(spr, sx, sy);
        }
    }
    lunge_n = 0;
    view_sync_sprites();
}

void view_world_enter(void) {
    render_set_world(1);
    update_camera();
    view_worldpaint_full();
    render_scroll(view_cam_px(), view_cam_py());
    view_sync_sprites();
}

/* ------------------------------------------------- overlay snapshot */

void view_draw(void) {
    uint8_t sx, sy;
    for (sy = 0; sy < VIEW_H; sy++) {
        uint8_t wy = (uint8_t)(g_cam_y + sy);
        for (sx = 0; sx < SCREEN_W; sx++) {
            uint8_t wx = (uint8_t)(g_cam_x + sx);
            uint8_t cell = map_cell(wx, wy);
            if (cell & MF_EXPLORED) {
                render_tile(sx, sy, cell_terrain_shown(cell));
            } else {
                render_tile(sx, sy, TI_BLANK);
            }
        }
    }
    /* Floor items: they don't move, so explored is enough to show them. */
    {
        uint8_t i;
        for (i = 0; i < MAX_FLOOR_ITEMS; i++) {
            const item_t *it = &g_floor[i];
            if (it->kind == ITEM_NONE) continue;
            if (!(map_cell(it->x, it->y) & MF_EXPLORED)) continue;
            if (it->x < g_cam_x || it->y < g_cam_y) continue;
            if ((uint8_t)(it->x - g_cam_x) >= SCREEN_W) continue;
            if ((uint8_t)(it->y - g_cam_y) >= VIEW_H) continue;
            render_tile((uint8_t)(it->x - g_cam_x),
                        (uint8_t)(it->y - g_cam_y),
                        (tile_id_t)item_tile(it->kind));
        }
    }
    /* Monsters as glyphs (backdrop only; live play uses sprites). */
    {
        uint8_t i;
        for (i = 0; i < MAX_MONSTERS; i++) {
            const monster_t *m = &g_mons[i];
            if (!mon_shown(m)) continue;
            if (m->x < g_cam_x || m->y < g_cam_y) continue;
            if ((uint8_t)(m->x - g_cam_x) >= SCREEN_W) continue;
            if ((uint8_t)(m->y - g_cam_y) >= VIEW_H) continue;
            render_glyph((uint8_t)(m->x - g_cam_x),
                         (uint8_t)(m->y - g_cam_y), mon_glyph(m, i));
        }
    }
    render_tile((uint8_t)(g_px - g_cam_x), (uint8_t)(g_py - g_cam_y),
                TI_PLAYER);
}
