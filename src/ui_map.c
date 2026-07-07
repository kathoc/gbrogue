#include <gb/gb.h>
#include "ui_map.h"
#include "render.h"
#include "input.h"
#include "text4.h"
#include "map.h"
#include "world.h"
#include "worldview.h"
#include "status.h"
#include "msg.h"
#include "lang.h"

/*
 * Minimap: the full 32x28 level at 2x2 px per cell = 8x7 composed
 * tiles. Shades: 0 unexplored, 1 walkable, 2 wall, 3 stairs/player.
 */

#define MAP_ORG_X 6u
#define MAP_ORG_Y 4u

static uint8_t s_blink_off;      /* player dot blink phase */

static uint8_t cell_shade(uint8_t x, uint8_t y) {
    uint8_t cell = map_cell(x, y);
    tile_id_t t = (tile_id_t)(cell & MF_TERRAIN);
    if (x == g_px && y == g_py && !s_blink_off) return 3;
    if (!map_is_explored(x, y)) return 0;
    switch (t) {
    case TI_BLANK:
        return 0;
    case TI_WALL_H:
    case TI_WALL_V:
        return 2;
    case TI_STAIRS_DOWN:
    case TI_STAIRS_UP:
        return 3;
    default:
        return 1;
    }
}

static void compose_chunk(uint8_t tx, uint8_t ty, uint8_t *buf) {
    uint8_t cy, cx;
    for (cy = 0; cy < 4u; cy++) {
        uint8_t lo = 0, hi = 0;
        for (cx = 0; cx < 4u; cx++) {
            uint8_t v = cell_shade((uint8_t)(tx * 4u + cx),
                                   (uint8_t)(ty * 4u + cy));
            uint8_t mask = (uint8_t)(0xC0u >> (cx * 2u));
            if (v & 1u) lo |= mask;
            if (v & 2u) hi |= mask;
        }
        buf[cy * 4u] = lo;          /* two identical px rows per cell */
        buf[cy * 4u + 1u] = hi;
        buf[cy * 4u + 2u] = lo;
        buf[cy * 4u + 3u] = hi;
    }
}

void ui_map_show(void) {
    uint8_t tx, ty;
    uint8_t buf[16];
    uint8_t me_tile = 0;
    uint8_t frames = 0;

    s_blink_off = 0;
    render_set_world(0);
    render_clear_all();
    render_text(9, 1, lang_str(SID_MAP_TITLE));
    render_text(6, 16, lang_str(SID_MAP_CLOSE));   /* B closes the overview */
    for (ty = 0; ty < 7u; ty++) {
        for (tx = 0; tx < 8u; tx++) {
            uint8_t slot;
            compose_chunk(tx, ty, buf);
            slot = t4_raw(buf);
            if (tx == (uint8_t)(g_px / 4u) && ty == (uint8_t)(g_py / 4u))
                me_tile = slot;        /* the chunk holding the player */
            render_cell_tile((uint8_t)(MAP_ORG_X + tx),
                             (uint8_t)(MAP_ORG_Y + ty), slot);
        }
    }
    render_present();

    /* Old Game Boys have flaky SELECT contacts, so the overview must not hang
       on SELECT staying pressed. Once it is up it stays up — even if SELECT
       drops out — until you deliberately close it with B. */
    input_swallow_edges();      /* forget the SELECT hold that opened us */
    for (;;) {
        wait_vbl_done();
        if (input_pressed() & J_B) break;
        /* blink the player dot: recompose just its chunk in place */
        if ((++frames & 15u) == 0u && me_tile) {
            s_blink_off ^= 1u;
            compose_chunk((uint8_t)(g_px / 4u), (uint8_t)(g_py / 4u), buf);
            set_bkg_data(me_tile, 1, buf);
        }
    }
    s_blink_off = 0;

    view_world_enter();
    status_update();
    msg_refresh();
    render_present();
    input_swallow_edges();
}
