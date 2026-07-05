#ifndef TEXT4_H
#define TEXT4_H

#include <stdint.h>

/*
 * Half-width UI text: two 4x8 chars composed into one 8x8 tile.
 * Composed tiles live in VRAM slots DYN_BASE..255 and are cached by
 * their character pair; render_clear_all() resets the cache (every
 * modal screen starts fresh). Map glyphs and actor sprites stay 8x8.
 */

/* VRAM 63..82: the 20 fixed message-row tiles (render.c streams line
   bitmaps into them so new messages can slide in from below). */
#define MSGROW_BASE  63u
#define MSGROW_TILES 20u
#define T4_BASE  83u                  /* dynamic composed-text pool */
#define T4_COUNT (256u - T4_BASE)     /* 173 dynamic slots */

/* VRAM tile for the pair (a,b). Both spaces -> the shared blank. */
uint8_t t4_pair(char a, char b);
/* VRAM tile for a full-width kana glyph byte (0x02..0x1F / 0x80..). */
uint8_t t4_full(uint8_t code);
/* VRAM tile for a raw 16-byte 2bpp bitmap (minimap chunks etc). Not
   deduplicated; slots recycle with the pool. */
uint8_t t4_raw(const uint8_t *tile16);
/* True if the string byte is a full-width glyph code. */
#define T4_IS_FULL(b) ((uint8_t)(b) >= 0x80u || \
                       ((uint8_t)(b) >= 0x02u && (uint8_t)(b) < 0x20u))
/* Drop the cache (composed tiles get reallocated on demand). */
void    t4_reset(void);
/* Render a text line into a 20x8 pixel-row bitmap (dst[cell*8+row]),
   blank-padded — the smooth message scroll mixes two of these. */
void    t4_line_bitmap(const char *s, uint8_t *dst);

/* Cache state, exposed for the headless harness to decode text. */
extern uint16_t g_t4_keys[T4_COUNT];
extern uint8_t  g_t4_used;
/* Set when the pool wrapped mid-scene; render_present() then redraws
   the HUD rows so no on-screen text references recycled slots. */
extern uint8_t  g_t4_flushed;

#endif
