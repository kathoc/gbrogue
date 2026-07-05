#ifndef RENDER_H
#define RENDER_H

#include <stdint.h>
#include "tiles.h"

#define SCREEN_W 20
#define SCREEN_H 18
#define VIEW_H   16      /* rows 0..15: dungeon viewport */
#define ROW_MSG  16      /* one-line message band */
#define ROW_STAT 17      /* always-on status line */

/* World-mode constants: the whole level lives in the BG tile map. */
#define WORLD_W 32          /* == MAP_W */
#define WORLD_H 28          /* == MAP_H */

/* Sprite slots: 0 = player, 1.. = monsters. */
#define SPR_PLAYER 0
#define SPR_MON0   1

/* 0 = ASCII tileset, 1 = graphic tileset (defined in render.c). */
extern uint8_t g_render_mode;

void render_init(void);
/* M10: flip between the ASCII tileset and the graphic tileset. The
   caller repaints afterwards. */
void render_toggle_mode(void);

/*
 * Smooth-play world mode. on=1: the BG map holds the whole level,
 * SCX/SCY pan it, the Window pins the message/status rows, actors are
 * hardware sprites. on=0 (overlay): classic 20x18 g_screen path with
 * scroll reset, window hidden, sprites parked — used by every modal UI
 * (title, pack, menu, log, popups, death).
 */
void render_set_world(uint8_t on);
uint8_t render_world_on(void);
/* Write one world cell (internal tile id) into the world BG shadow. */
void render_world_cell(uint8_t wx, uint8_t wy, tile_id_t id);
/* Pixel scroll (safe any time; pure register write). */
void render_scroll(uint8_t x, uint8_t y);
/* Sprites. Positions are screen pixels (0,0 = top-left of viewport). */
void render_sprite_glyph(uint8_t i, char c);
void render_sprite_id(uint8_t i, tile_id_t id);   /* honors GFX mode */
void render_sprite_pos(uint8_t i, uint8_t sx, uint8_t sy);
void render_sprite_hide(uint8_t i);

/*
 * Damage feedback: queue a cell flash, then play the whole queue as
 * one blocking blink (3 on/off cycles, ~20 frames) after the movement
 * animation. GBC: the cell's background palette flips to red (you got
 * hurt) or yellow (you hit it). DMG: the actor's sprite blinks in the
 * inverted OBP1 ramp. spr = sprite slot, 0xFF for none.
 */
#define FLASH_HURT 0u
#define FLASH_HIT  1u
void render_flash_add(uint8_t wx, uint8_t wy, uint8_t style, uint8_t spr);
void render_flash_play(void);
/* Status-row tile columns of the HP readout (status.c keeps them
   current; the HURT blink turns them red on GBC). */
extern uint8_t g_hp_col0, g_hp_col1;
/* The latest message line (also the cause of death when HP hit 0). */
extern char g_last_msg[];

/*
 * Non-blocking message slide. render_message() only arms the incoming
 * line; render_msg_begin() (end of finish_turn — i.e., after the move
 * glide and damage flashes) starts it, and render_msg_tick() advances
 * it 2px per call (4 frames total). Call the tick once per frame from
 * every wait loop; input stays live throughout.
 */
void render_msg_begin(void);
void render_msg_tick(void);
/*
 * Full-screen art mode (title / game over). begin: GBC attributes go
 * flat (palette 0 everywhere) so art may occupy any VRAM slot. end:
 * restores the glyph/GFX tilesets the art overwrote and resets the
 * composed-text pool. Call end before returning to any normal screen.
 */
void render_art_begin(void);
void render_art_end(void);

/*
 * Palette fades (blocking; ~1 frame per tick, input edges stay
 * latched). Fade out to black (GBC) / white (DMG), fade back in.
 * The tilemap and OAM are untouched, so level loading and tileset
 * reloads can happen invisibly between the two.
 */
#define FADE_OUT_FRAMES 60u
#define FADE_IN_FRAMES  30u
void render_fade_out(uint8_t frames);
void render_fade_in(uint8_t frames);
void render_clear_view(void);
void render_clear_all(void);
void render_tile(uint8_t x, uint8_t y, tile_id_t id);
void render_glyph(uint8_t x, uint8_t y, char c);      /* full-width 8x8 */
/* Place a raw VRAM tile index (composed minimap chunks etc). */
void render_cell_tile(uint8_t x, uint8_t y, uint8_t tile);
/* Half-width UI text: 2 chars per tile cell, from tile column x. A row
   fits 40 characters. */
void render_text(uint8_t x, uint8_t y, const char *s);
void render_row(uint8_t y, const char *s);  /* padded to the full row */
void render_status(const char *s);          /* up to 40 chars */
void render_message(const char *s);         /* NULL clears the band */
void render_present(void);                  /* flush dirty rows to VRAM */

#endif
