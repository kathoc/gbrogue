#include <gb/gb.h>
#include "worldview.h"
#include "world.h"
#include "map.h"

/*
 * BANK0 (early-sorting filename, like bank0_rng.c). view_visible and the
 * RAM-only player-move helper are reached from banked scroll logic (BANK2:
 * hold/scare FOV and teleport); call_bank unmaps bank 1, so these must live
 * in the fixed bank. They touch RAM only — NO render_* calls. After a banked
 * teleport the world repaint is done by view_world_enter() in the
 * orchestrator (ui_inv close), which recomputes the camera and does a full
 * worldpaint, so the deferred repaint costs nothing extra.
 */

uint8_t view_visible(uint8_t wx, uint8_t wy) {
    if (g_blind_t)
        return wx == g_px && wy == g_py;
    if (g_cur_room != 0xFFu) {
        const room_t *r = &g_rooms[g_cur_room];
        if (wx >= r->x && wx < (uint8_t)(r->x + r->w) &&
            wy >= r->y && wy < (uint8_t)(r->y + r->h)) return 1;
    }
    if ((uint8_t)(wx - g_px + 1u) <= 2u && (uint8_t)(wy - g_py + 1u) <= 2u)
        return 1;
    return 0;
}

/* RAM-only counterpart to view_player_moved(): refresh g_cur_room and the
   explored flags for the new player cell WITHOUT painting. The rendering
   view_player_moved() (worldview.c, HOME) keeps its own first-entry room
   paint for the live-move paths; here the paint is deferred to the
   orchestrator's full repaint, so we only need the flag/room state right. */
static void mark_room_explored_ram(uint8_t idx) {
    room_t *r = &g_rooms[idx];
    uint8_t x, y;
    if (r->flags & ROOM_EXPLORED) return;
    r->flags |= ROOM_EXPLORED;
    for (y = r->y; y < (uint8_t)(r->y + r->h); y++)
        for (x = r->x; x < (uint8_t)(r->x + r->w); x++)
            map_set_flag(x, y, MF_EXPLORED);
}

void view_player_moved_ram(void) {
    g_cur_room = map_room_at(g_px, g_py);
    if (g_cur_room != 0xFFu) mark_room_explored_ram(g_cur_room);
    /* reveal the ring around the player (corridors, door thresholds) */
    {
        uint8_t x, y;
        for (y = (uint8_t)(g_py - 1u); y != (uint8_t)(g_py + 2u); y++)
            for (x = (uint8_t)(g_px - 1u); x != (uint8_t)(g_px + 2u); x++)
                map_set_flag(x, y, MF_EXPLORED);
    }
}
