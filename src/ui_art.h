#ifndef UI_ART_H
#define UI_ART_H

#include <stdint.h>

/*
 * Blit generated full-screen art (assets/art_data.c) into the overlay
 * screen from tile row 0. Loads the deduplicated tiles into VRAM
 * (slots 0..82, then 255 downward — see scripts/gen_art.py) and writes
 * the cell map. Call between render_art_begin()/render_art_end();
 * render_text() still works on top for menus and stats.
 */
void art_blit(uint8_t bank, const uint8_t *tiles, uint8_t ntiles,
              const uint8_t *map, uint8_t rows);

#endif
