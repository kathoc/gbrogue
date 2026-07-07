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
#include "text4.h"

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
   between the eight digits, U/D changes the current one, A pins the seed
   and starts a new game immediately (returns 1), B cancels back to the
   title (returns 0). Both paths leave the screen faded to black. */
/* The value under edit lives here rather than as a draw_seed_ex() argument:
   SDCC's gbz80 calling convention marshals a uint32_t argument onto the
   stack at every call site, which bank0 has no byte budget for. */
static uint32_t s_seed_v;

/* One shared 8-char-row builder for both the digit line and the cursor
   line (is_cursor picks which): a single compute+render_text body, called
   twice, compiles far smaller than inlining both loops in draw_seed_ex. */
static void seed_row(uint8_t y, uint8_t is_cursor, uint8_t cur) {
    char buf[9];
    uint8_t d;
    for (d = 0; d < 8u; d++) {
        if (is_cursor) {
            buf[d] = (d == cur) ? '^' : ' ';
        } else {
            uint8_t nib = (uint8_t)((s_seed_v >> ((7u - d) * 4u)) & 0xFu);
            buf[d] = (char)(nib < 10u ? '0' + nib : 'A' + (nib - 10u));
        }
    }
    buf[8] = 0;
    render_text(MENU_COL, y, buf);
}

/* reset=1: fresh screen (clears + recycles the composed-tile pool), used
   for the very first paint over the title art. reset=0: D-pad repaint —
   skip render_clear_all/t4_reset (which would blank+free every composed
   tile behind a live, mid-recompose VRAM write) and rewrite only the one
   row the keypress actually changed (curs_only picks row 9 over row 8),
   the same "reset only when the screen is new" split ui_inv.c's
   draw_list_ex uses for scroll updates -- halving the VRAM traffic of a
   plain digit or cursor step also halves the odds of that step's redraw
   still being mid-flight when the wrap guard below has to retry it. */
static void draw_seed_ex(uint8_t cur, uint8_t reset, uint8_t curs_only) {
again:
    if (reset) {                                   /* layout is fixed, so
                                                        both go up front and
                                                        skip on a repaint */
        render_clear_all();
        render_text(MENU_COL, 6, lang_str(SID_TITLE_SEED));
        render_status(lang_str(SID_MENU_HINT));
    }
    if (reset || !curs_only) seed_row(8, 0, cur);
    if (reset || curs_only)  seed_row(9, 1, cur);
    /* A repaint that wrapped the composed-tile pool leaves every untouched
       cell pointing at recycled tiles -> repaint everything from a fresh
       pool before this reaches VRAM (same guard as ui_inv.c/ui_menu.c).
       Loop back in place (no recursive call) to skip re-marshalling cur
       onto the stack -- SDCC's gbz80 calling convention makes that pricey
       and bank0 has no headroom to spare. */
    if (!reset && g_t4_flushed) {
        g_t4_flushed = 0;
        reset = 1;
        goto again;
    }
    render_present();
}

static uint8_t edit_seed(void) {
    uint8_t cur = 0;
    s_seed_v = (g_seed_pinned || g_seed_override) ? g_seed_override : g_run_seed;
    render_set_world(0);
    draw_seed_ex(0, 1, 0);
    render_fade_in(FADE_IN_FRAMES);        /* caller faded out; reveal */
    input_swallow_edges();
    for (;;) {
        uint8_t keys;
        wait_vbl_done();
        keys = input_pressed();
        if (keys & J_B) {                             /* cancel */
            render_fade_out(FADE_OUT_FRAMES);
            return 0;
        }
        if (keys & J_A) {                             /* confirm + start */
            g_seed_override = s_seed_v;
            g_seed_pinned = 1u;      /* even 0x00000000 is now a real seed */
            render_fade_out(FADE_OUT_FRAMES);
            return 1;
        }
        if (keys & (J_LEFT | J_RIGHT)) {
            cur = (uint8_t)((cur + ((keys & J_LEFT) ? 7u : 1u)) & 7u);
            draw_seed_ex(cur, 0, 1);
        }
        if (keys & (J_UP | J_DOWN)) {
            uint8_t sh = (uint8_t)((7u - cur) * 4u);
            uint8_t nib = (uint8_t)((s_seed_v >> sh) & 0xFu);
            nib = (uint8_t)(((keys & J_UP) ? nib + 1u : nib + 15u) & 0xFu);
            s_seed_v = (s_seed_v & ~((uint32_t)0xFu << sh)) | ((uint32_t)nib << sh);
            draw_seed_ex(cur, 0, 0);
        }
    }
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
                if (edit_seed()) {         /* A: pinned seed, start now */
                    g_run_seed = g_seed_override;
                    rng_seed(g_seed_override);
                    /* edit_seed() already faded to black and render_art_end()
                       already ran above, matching the new-game start state
                       below (lines ~230) -- no second fade here. */
                    return 0;
                }
                paint_title(sel, n_items); /* B: cancelled, back to title */
                render_fade_in(FADE_IN_FRAMES);
                input_swallow_edges();
                continue;
            }
            {
                uint8_t cont = (uint8_t)(s_can && sel == 0u);
                if (!cont) {
                    uint32_t s32;
                    if (g_seed_pinned || g_seed_override) s32 = g_seed_override;
#ifdef GBR_DEBUG_KIT
                    else s32 = GBR_TEST_SEED;    /* stable maps for the suite */
#else
                    else s32 = (uint16_t)(((uint16_t)DIV_REG << 8) | frames);
#endif
                    g_run_seed = s32;            /* full 8-digit seed, for repro */
                    /* RNG state is a full 32 bits (xorshift32), so seed it
                       directly — every one of the eight hex digits selects a
                       distinct map, no folding collisions. */
                    rng_seed(s32);
                }
                render_fade_out(FADE_OUT_FRAMES);
                render_art_end();
                return cont;
            }
        }
    }
}
