#ifndef WORLDVIEW_H
#define WORLDVIEW_H

#include <stdint.h>

extern uint8_t g_cam_x, g_cam_y;    /* camera in tiles (= SCX/8, SCY/8) */
extern uint8_t g_cur_room;          /* room index player stands in, 0xFF = corridor */

/* Call after the player position changes: updates the current room,
   marks newly seen cells explored (painting them into the world BG),
   and recomputes the camera target. */
void view_player_moved(void);

/* RAM-only sibling (bank0_view.c): update room/explored state for the new
   player cell but paint nothing — for banked teleport (BANK2), whose
   repaint is deferred to view_world_enter() when the pack UI closes. */
void view_player_moved_ram(void);

/* True if world cell (wx,wy) is in the player's line of sight. */
uint8_t view_visible(uint8_t wx, uint8_t wy);

/* Overlay-mode snapshot: paint the full 20x16 viewport (terrain, items,
   monster glyphs, player) into the g_screen shadow. Backdrop for UIs. */
void view_draw(void);

/* --- smooth world mode --- */

/* Camera pixel targets for the current player position (multiples of 8). */
uint8_t view_cam_px(void);
uint8_t view_cam_py(void);
/* Repaint world BG cells (terrain + floor items; actors are sprites). */
void view_worldpaint_full(void);
void view_worldpaint_around(void);           /* 3x3 around the player */
/* Assign sprite tiles/visibility/positions for player + monsters,
   using the current hardware scroll. */
void view_sync_sprites(void);
/* Call once per frame from the play loop: actors bob one pixel on a
   ~0.5s cycle (phase-shifted per actor) so they look alive. */
void view_breathe(void);
/* Is monster slot i currently visible to the player? (Same rule the
   sprite sync uses — the move animation must respect it too.) */
uint8_t view_mon_shown_idx(uint8_t i);
/* Attack feedback: queue a short sprite jab toward (dx,dy); combat
   adds them, finish_turn plays the queue after the move glide. */
void view_lunge_add(uint8_t spr, int8_t dx, int8_t dy);
void view_lunge_play(void);
/* Enter world mode: full paint, snap scroll to target, sync sprites. */
void view_world_enter(void);

#endif
