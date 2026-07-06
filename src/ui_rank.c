#include <gb/gb.h>
#include "ui_rank.h"
#include "rank.h"
#include "render.h"
#include "input.h"
#include "lang.h"
#include "util.h"

/*
 * Top runs by gold, one row each, with a legend on the top row:
 *   "1st G:836 B6 <cause> 16"
 *      rank  / gold / deepest floor / cause of death / lifetime play no.
 * The cause is rebuilt from the stored SID (+ monster kind); a won run
 * shows "escaped!". The SRAM store lives in BANK5 (rank.c); this screen
 * reads it back through rank_read. Entered from the title, which fades to
 * black first (ui_title.c), so we fade IN here and fade OUT on the way
 * back — no lit-VRAM tearing during the tileset swap.
 */

/* 1st / 2nd / 3rd / 4th ... (RANK_N == 6). */
static const char *const ORD[RANK_N] = { "st", "nd", "rd", "th", "th", "th" };

/* Append the localized cause at o, substituting the monster name for the
   %s slot (byte 0x01) of SID_DEATH_MON; a won run (cause 0) reads
   "escaped!". lang_str and lang_name use a 2-deep buffer ring, so the two
   consecutive calls land in distinct buffers and don't clobber. Returns
   the new end pointer. */
static char *append_cause(char *o, uint8_t sid, uint8_t mon) {
    const char *p = lang_str(sid ? sid : SID_RANK_ESCAPED);
    const char *arg = (sid == SID_DEATH_MON) ? lang_name(LT_MNAME, mon) : "";
    while (*p) {
        if ((uint8_t)*p == 0x01u) {
            while (*arg) *o++ = *arg++;
            p++;
        } else {
            *o++ = *p++;
        }
    }
    return o;
}

void ui_rank_show(void) {
    rank_entry_t ent[RANK_N];
    uint8_t n = rank_read(ent);
    uint8_t i;
    char buf[64];

    render_set_world(0);
    render_clear_all();
    render_row(0, lang_str(n ? SID_RANK_LEGEND : SID_RANK_EMPTY));
    for (i = 0; i < n; i++) {
        /* "1st G:836 B6 <cause> 16" */
        char *p = buf;
        *p++ = (char)('1' + i);
        p = fmt_str(p, ORD[i]);
        p = fmt_str(p, " G:");
        p = fmt_u16(p, ent[i].gold);
        p = fmt_str(p, " B");
        p = fmt_u16(p, ent[i].deepest);
        *p++ = ' ';
        p = append_cause(p, ent[i].cause, ent[i].mon);
        *p++ = ' ';
        p = fmt_u16(p, ent[i].play_no);
        *p = 0;
        render_row((uint8_t)(2u + i), buf);
    }
    render_status(lang_str(SID_HINT_CANCEL));
    render_present();
    render_fade_in(FADE_IN_FRAMES);

    input_swallow_edges();
    for (;;) {
        wait_vbl_done();
        if (input_pressed() & (J_A | J_B | J_START)) break;
    }
    input_swallow_edges();
    render_fade_out(FADE_OUT_FRAMES);
}
