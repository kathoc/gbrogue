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

/* SID (or 0xFF for the special LANGUAGE row) for item index i of n.
   Order: [CONTINUE] NEW RANKING SEED LANGUAGE. */
static uint8_t item_sid(uint8_t i, uint8_t n) {
    if (i == (uint8_t)(n - 1u)) return 0xFFu;              /* LANGUAGE */
    if (i == (uint8_t)(n - 2u)) return SID_TITLE_SEED;     /* SEED     */
    if (i == (uint8_t)(n - 3u)) return SID_MENU_RANK;      /* RANKING  */
    if (s_can && i == 0u) return SID_TITLE_CONT;
    return SID_TITLE_NEW;
}

/* 8-hex-digit (32-bit) seed editor reached from the title. D-pad L/R moves
   between the eight digits, U/D changes the current one, A confirms (pins
   the seed for the next new game), B cancels. */
static void draw_seed(uint32_t v, uint8_t cur) {
    char buf[10];
    char cus[10];
    uint8_t d;
    render_clear_all();
    render_text(MENU_COL, 6, lang_str(SID_TITLE_SEED));
    for (d = 0; d < 8u; d++) {
        uint8_t nib = (uint8_t)((v >> ((7u - d) * 4u)) & 0xFu);
        buf[d] = (char)(nib < 10u ? '0' + nib : 'A' + (nib - 10u));
        cus[d] = (d == cur) ? '^' : ' ';
    }
    buf[8] = 0;
    cus[8] = 0;
    render_text(MENU_COL, 8, buf);
    render_text(MENU_COL, 9, cus);
    render_status(lang_str(SID_MENU_HINT));
    render_present();
}

static void edit_seed(void) {
    uint32_t v = g_seed_override ? g_seed_override : g_run_seed;
    uint8_t cur = 0;
    render_set_world(0);
    draw_seed(v, cur);
    render_fade_in(FADE_IN_FRAMES);        /* caller faded out; reveal */
    input_swallow_edges();
    for (;;) {
        uint8_t keys;
        wait_vbl_done();
        keys = input_pressed();
        if (keys & J_B) break;                       /* cancel */
        if (keys & J_A) { g_seed_override = v; break; }  /* confirm + pin */
        if (keys & J_LEFT)  { cur = (uint8_t)((cur + 7u) & 7u); draw_seed(v, cur); }
        if (keys & J_RIGHT) { cur = (uint8_t)((cur + 1u) & 7u); draw_seed(v, cur); }
        if (keys & (J_UP | J_DOWN)) {
            uint8_t sh = (uint8_t)((7u - cur) * 4u);
            uint8_t nib = (uint8_t)((v >> sh) & 0xFu);
            nib = (uint8_t)(((keys & J_UP) ? nib + 1u : nib + 15u) & 0xFu);
            v = (v & ~((uint32_t)0xFu << sh)) | ((uint32_t)nib << sh);
            draw_seed(v, cur);
        }
    }
    render_fade_out(FADE_OUT_FRAMES);
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
    n_items = (uint8_t)(s_can ? 5u : 4u);

    render_set_world(0);
    paint_title(sel, n_items);

    for (;;) {
        uint8_t keys;
        uint8_t lang_row = (uint8_t)(n_items - 1u);
        uint8_t seed_row = (uint8_t)(n_items - 2u);
        uint8_t rank_row = (uint8_t)(n_items - 3u);
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
            if (sel == seed_row) {
                sfx_play(SFX_MENU);
                render_fade_out(FADE_OUT_FRAMES);
                render_art_end();          /* text tileset for the editor */
                edit_seed();               /* fades itself in and out */
                paint_title(sel, n_items);
                render_fade_in(FADE_IN_FRAMES);
                input_swallow_edges();
                continue;
            }
            {
                uint8_t cont = (uint8_t)(s_can && sel == 0u);
                if (!cont) {
                    uint32_t s32;
                    if (g_seed_override) s32 = g_seed_override;
#ifdef GBR_DEBUG_KIT
                    else s32 = GBR_TEST_SEED;    /* stable maps for the suite */
#else
                    else s32 = (uint16_t)(((uint16_t)DIV_REG << 8) | frames);
#endif
                    g_run_seed = s32;            /* full 8-digit seed, for repro */
                    /* the RNG state is 16-bit; fold the 32-bit seed into it so
                       all eight hex digits influence the run (a 16-bit-only
                       seed like the debug pin folds to itself) */
                    rng_seed((uint16_t)(s32 ^ (s32 >> 16)));
                }
                render_fade_out(FADE_OUT_FRAMES);
                render_art_end();
                return cont;
            }
        }
    }
}
