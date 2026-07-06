#include <gb/gb.h>
#include "ui_title.h"
#include "render.h"
#include "input.h"
#include "rng.h"
#include "save.h"
#include "lang.h"
#include "world.h"
#include "util.h"
#include "ui_art.h"
#include "art_data.h"
#include "ui_rank.h"
#include "sfx.h"

/* Own translation unit: heavy render_text call sites must stay out of
   game.c (SDCC layout constraint, docs/architecture.md). */

/* The new-game RNG seed is normally DIV-based entropy so every run is
   different. That makes map-dependent tests (m2m4/m5) fragile: any change
   to boot timing shifts the seed and thus the first map, so an unrelated
   edit can hand the player a deadly or disconnected level. The debug build
   pins the seed instead, giving those tests a stable map. Overridable via
   -DGBR_TEST_SEED=... if a future map assertion needs a different level. */
#ifndef GBR_TEST_SEED
#define GBR_TEST_SEED 0x1D2Du
#endif

/* Menu items stack up from the art's blank band, LANGUAGE always last
   (its ASCII label stays readable in either language). With a save
   present the list is CONTINUE / NEW / RANKING / LANGUAGE, otherwise
   NEW / RANKING / LANGUAGE. */
#define ROW_LANG 15u
#define MENU_COL 5u

static uint8_t s_can;                 /* save present -> 4 items */

/* Debug invincibility handshake: 13 B taps then SELECT. */
#define DBG_BS 13u

static void draw_item(uint8_t y, uint8_t sid, uint8_t selected) {
    char buf[24];
    char *p;
    p = fmt_str(buf, selected ? "> " : "  ");
    p = fmt_str(p, lang_str(sid));
    /* Debug (invincibility) armed: mark NEW GAME / はじめる with a '*'. */
    if (g_debug && sid == SID_TITLE_NEW)
        p = fmt_str(p, "*");
    p = fmt_str(p, "   ");            /* wipe the longer other-language tail */
    *p = 0;
    render_text(MENU_COL, y, buf);
}

/* SID (or 0xFF for the special LANGUAGE row) for item index i of n. */
static uint8_t item_sid(uint8_t i, uint8_t n) {
    if (i == (uint8_t)(n - 1u)) return 0xFFu;              /* LANGUAGE */
    if (i == (uint8_t)(n - 2u)) return SID_MENU_RANK;      /* RANKING  */
    if (s_can && i == 0u) return SID_TITLE_CONT;
    return SID_TITLE_NEW;
}

static void draw_menu(uint8_t sel, uint8_t n) {
    uint8_t top = (uint8_t)(ROW_LANG - (n - 1u));
    uint8_t i;
    for (i = 0; i < n; i++) {
        uint8_t y = (uint8_t)(top + i);
        uint8_t sid = item_sid(i, n);
        if (sid == 0xFFu) {
            char buf[24];
            char *p = fmt_str(buf, sel == i ? "> " : "  ");
            p = fmt_str(p, "LANGUAGE:");
            p = fmt_str(p, g_lang ? "JPN" : "ENG");
            *p = 0;
            render_text(MENU_COL, y, buf);
        } else {
            draw_item(y, sid, (uint8_t)(sel == i));
        }
    }
    render_present();   /* debug-armed feedback is the sfx blip below */
}

static void paint_title(uint8_t sel, uint8_t n) {
    render_clear_all();
    render_art_begin();
    art_blit(BANK(art_title_tiles), art_title_tiles, ART_TITLE_TILES,
             art_title_map, ART_TITLE_ROWS);
    draw_menu(sel, n);
}

uint8_t ui_title_show(void) {
    uint8_t sel = 0;
    uint8_t n_items;
    uint8_t frames = 0;
    uint8_t bcount = 0;

    g_debug = 0;                      /* re-armed each visit */
    s_can = save_exists();
    n_items = (uint8_t)(s_can ? 4u : 3u);

    render_set_world(0);
    paint_title(sel, n_items);

    for (;;) {
        uint8_t keys;
        uint8_t lang_row = (uint8_t)(n_items - 1u);
        uint8_t rank_row = (uint8_t)(n_items - 2u);
        wait_vbl_done();
        frames++;
        keys = input_pressed();

        /* Debug code: 13 B taps, then a SELECT. Any other key resets. */
        if (keys & J_B) {
            bcount++;
        } else if (keys & J_SELECT) {
            if (bcount >= DBG_BS) {
                g_debug = 1;
                sfx_play(SFX_STAIRS);
            }
            bcount = 0;
        } else if (keys) {
            bcount = 0;
        }

        if (keys & (J_UP | J_DOWN | J_SELECT)) {
            sfx_play(SFX_MENU);
            if (keys & J_UP)
                sel = (uint8_t)((sel + n_items - 1u) % n_items);
            else
                sel = (uint8_t)((sel + 1u) % n_items);
            draw_menu(sel, n_items);
        }
        if (sel == lang_row && (keys & (J_LEFT | J_RIGHT | J_A))) {
            sfx_play(SFX_MENU);
            g_lang ^= 1u;
            draw_menu(sel, n_items);      /* every label swaps language */
            continue;
        }
        if (keys & (J_START | J_A)) {
            if (sel == rank_row) {
                sfx_play(SFX_MENU);
                /* Fade the title out BEFORE swapping the tileset: both
                   render_art_end() and paint_title()'s art_blit stream
                   tile PATTERN data straight to VRAM (set_bkg_data),
                   which tears if done on the lit title. ui_rank_show()
                   fades itself in and back out to black. */
                render_fade_out(FADE_OUT_FRAMES);
                render_art_end();          /* restore text tileset (dark) */
                ui_rank_show();
                paint_title(sel, n_items); /* rebuild the title art (dark) */
                render_fade_in(FADE_IN_FRAMES);
                input_swallow_edges();
                continue;
            }
            {
                uint8_t cont = (uint8_t)(s_can && sel == 0u);
                if (!cont)
#ifdef GBR_DEBUG_KIT
                    rng_seed(GBR_TEST_SEED);   /* stable maps for the suite */
#else
                    rng_seed((uint16_t)(((uint16_t)DIV_REG << 8) | frames));
#endif
                render_fade_out(FADE_OUT_FRAMES);
                render_art_end();
                return cont;
            }
        }
    }
}
