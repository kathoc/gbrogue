#include <gb/gb.h>
#include "ui_rank.h"
#include "rank.h"
#include "render.h"
#include "input.h"
#include "lang.h"
#include "util.h"

/*
 * Top runs by gold, two rows each:
 *   "1 G1020  B26>B3"   rank / gold / floor reached (deepest>final once
 *                       the Amulet has been taken, else just the depth).
 *   "  Slain by the bat" the cause of death, rebuilt from the stored SID
 *                       (+ monster kind); blank on a won run.
 * The SRAM store lives in BANK5 (rank.c); this screen only reads it back
 * through the rank_read shim. Stays in HOME for now (it calls render);
 * moving it into the UI bank waits on the render-orchestration refactor.
 */

/* Rebuild the death-cause sentence from the stored SID, substituting the
   monster name for the %s slot (byte 0x01) of SID_DEATH_MON. lang_str and
   lang_name draw from a 2-deep buffer ring, so the two consecutive calls
   below land in distinct buffers and don't clobber each other. */
static void fmt_cause(char *dst, uint8_t sid, uint8_t mon) {
    const char *p = lang_str(sid);
    const char *arg = (sid == SID_DEATH_MON) ? lang_name(LT_MNAME, mon) : "";
    char *o = dst;
    *o++ = ' ';
    *o++ = ' ';
    while (*p) {
        if ((uint8_t)*p == 0x01u) {
            while (*arg) *o++ = *arg++;
            p++;
        } else {
            *o++ = *p++;
        }
    }
    *o = 0;
}

void ui_rank_show(void) {
    rank_entry_t ent[RANK_N];
    uint8_t n = rank_read(ent);
    uint8_t i;
    char buf[42];

    render_set_world(0);
    render_clear_all();
    render_row(0, lang_str(n ? SID_RANK_TITLE : SID_RANK_EMPTY));
    for (i = 0; i < n; i++) {
        /* "1 G1020 B26>B3" — gold, then floor: deepest>final once the
           Amulet was taken (a climb-back run), else just the final. */
        char *p = buf;
        *p++ = (char)('1' + i);
        p = fmt_str(p, " G");
        p = fmt_u16(p, ent[i].gold);
        p = fmt_str(p, " B");
        if (ent[i].amulet) {
            p = fmt_u16(p, ent[i].deepest);
            *p++ = '>';
            *p++ = 'B';
        }
        p = fmt_u16(p, ent[i].final);
        *p = 0;
        render_row((uint8_t)(2u + i * 2u), buf);
        /* cause line just below (blank for a won run: cause == 0) */
        if (ent[i].cause) {
            fmt_cause(buf, ent[i].cause, ent[i].mon);
            render_row((uint8_t)(3u + i * 2u), buf);
        }
    }
    render_status(lang_str(SID_HINT_CANCEL));
    render_present();

    input_swallow_edges();
    for (;;) {
        wait_vbl_done();
        if (input_pressed() & (J_A | J_B | J_START)) break;
    }
    input_swallow_edges();
}
