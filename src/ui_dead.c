#include <gb/gb.h>
#include "ui_dead.h"
#include "render.h"
#include "input.h"
#include "world.h"
#include "util.h"
#include "lang.h"
#include "msg.h"
#include "text4.h"
#include "ui_art.h"
#include "art_data.h"

/* Own translation unit: heavy render_text call sites must stay out of
   game.c (SDCC layout constraint, docs/architecture.md). */

/* The death screen fades out slowly and deliberately (a longer ramp than
   an ordinary floor transition) so the world dims away before GAME OVER
   appears. */
#define DEATH_FADE_FRAMES 100u

static void draw_score(uint8_t y) {
    char buf[32];
    char *p;
    p = fmt_str(buf, "xp ");
    p = fmt_u16(p, g_xp);
    p = fmt_str(p, "  gold ");
    p = fmt_u16(p, g_gold);
    *p = 0;
    render_text(5, y, buf);
}

/* Column that centers a mixed half/full-width string on the 40-column
   row (full-width glyphs count double; parity padding may shift the
   result one column — fine for centering). */
static uint8_t center_col(const char *s) {
    uint8_t w = 0;
    while (*s) {
        w = (uint8_t)(w + (T4_IS_FULL((uint8_t)*s) ? 2u : 1u));
        s++;
    }
    return w >= 40u ? 0u : (uint8_t)((40u - w) / 4u);
}

/* prompt + status line, then block until START. */
static void wait_start(uint8_t y, const char *line, uint8_t sid) {
    char st[44];
    char *p;
    uint8_t pad, i;
    render_text(center_col(line), y, line);
    /* center the epitaph on the status row */
    p = st;
    pad = (uint8_t)(center_col(lang_str(sid)) * 2u);
    for (i = 0; i < pad; i++) p = fmt_char(p, ' ');
    p = fmt_str(p, lang_str(sid));
    *p = 0;
    render_status(st);
    render_message(0);
    render_present();
    /* The caller faded to black before rewriting VRAM (matching
       level_transition); reveal the finished screen now. */
    render_fade_in(FADE_IN_FRAMES);
    input_swallow_edges();
    for (;;) {
        wait_vbl_done();
        if (input_pressed() & J_START) return;
    }
}

void ui_dead_show(void) {
    char buf[42];
    char *p;

    /* Fade to black FIRST: art_blit() streams tile-pattern data straight
       into VRAM (set_bkg_data), and doing that with the LCD lit over the
       still-visible world tore the screen. Hide it behind the fade, load
       the art, then wait_start() fades back in — the level_transition
       pattern (game.c). A slower ramp for the death screen (ゆっくり). */
    render_fade_out(DEATH_FADE_FRAMES);
    render_set_world(0);
    render_clear_all();
    render_art_begin();
    art_blit(BANK(art_over_tiles), art_over_tiles, ART_OVER_TILES,
             art_over_map, ART_OVER_ROWS);

    p = fmt_str(buf, lang_str(SID_ON_LEVEL));
    p = fmt_u16(p, g_depth);
    *p = 0;
    render_text(5, 12, buf);
    draw_score(13);
    /* the run's seed, so a memorable run can be replayed from the title */
    {
        char sb[12];
        char *q = fmt_str(sb, "seed ");
        uint8_t d;
        for (d = 0; d < 4u; d++) {
            uint8_t nib = (uint8_t)((g_run_seed >> ((3u - d) * 4u)) & 0xFu);
            *q++ = (char)(nib < 10u ? '0' + nib : 'A' + (nib - 10u));
        }
        *q = 0;
        render_text(5, 14, sb);
    }

    /* the cause of death, as a sentence (raw log line as fallback) */
    wait_start(15, g_death_cause[0] ? g_death_cause : g_last_msg, SID_RIP);
    /* blank the screen before the tileset reload repaints the art
       cells with glyphs (visible letter burst otherwise) */
    render_clear_all();
    render_present();
    render_art_end();
}

void ui_win_show(void) {
    /* Same clean swap as the death screen: fade out over the world, build
       the victory screen, then wait_start() fades it in. */
    render_fade_out(FADE_OUT_FRAMES);
    render_set_world(0);
    render_clear_all();
    render_text(3, 3, lang_str(SID_WON));
    render_text(1, 6, lang_str(SID_WON_SUB));
    draw_score(9);
    wait_start(13, "- START -", SID_VICTORY);
}
