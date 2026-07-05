#include <gb/gb.h>
#include "ui_art.h"
#include "render.h"
#include "farcopy.h"

/* Unique tile i -> VRAM slot; must mirror slot_for() in gen_art.py. */
static uint8_t slot_for(uint8_t i) {
    return i < 83u ? i : (uint8_t)(255u - (uint8_t)(i - 83u));
}

void art_blit(uint8_t bank, const uint8_t *tiles, uint8_t ntiles,
              const uint8_t *map, uint8_t rows) {
    uint8_t buf[20];
    uint8_t i, x, y;

    i = 0;
    do {
        far_copy(bank, tiles + (uint16_t)i * 16u, buf, 16);
        set_bkg_data(slot_for(i), 1, buf);
        i++;
    } while (i != ntiles && i != 0u);

    for (y = 0; y < rows; y++) {
        far_copy(bank, map + (uint16_t)y * 20u, buf, 20);
        for (x = 0; x < 20u; x++)
            render_cell_tile(x, y, buf[x]);
    }
}
