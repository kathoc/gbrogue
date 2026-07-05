#include "render.h"

/*
 * BANK0 (early-sorting filename, like bank0_rng.c). render_flash_add is
 * called from banked item logic (BANK2 wand bolts), and call_bank unmaps
 * bank 1 — so this enqueue must live in the fixed bank. It is a pure RAM
 * append; render_flash_play (render.c) drains the queue in the HOME
 * orchestrator. The queue state is shared via render.h.
 */
flash_ent_t g_flashq[FLASH_MAX];
uint8_t     g_flash_n;

void render_flash_add(uint8_t wx, uint8_t wy, uint8_t style, uint8_t spr) {
    if (g_flash_n >= FLASH_MAX) return;
    g_flashq[g_flash_n].x = wx;
    g_flashq[g_flash_n].y = wy;
    g_flashq[g_flash_n].style = style;
    g_flashq[g_flash_n].spr = spr;
    g_flash_n++;
}
