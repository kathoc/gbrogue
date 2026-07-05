#include <gb/gb.h>
#include "ui_rank.h"
#include "rank.h"
#include "render.h"
#include "input.h"
#include "lang.h"
#include "util.h"

/*
 * Top runs by gold, one line each:
 *   "1 G1020  B26>B3"   rank / gold / floor reached (deepest>final once
 *                       the Amulet has been taken, else just the depth).
 * The SRAM store lives in BANK5 (rank.c); this screen only reads it back
 * through the rank_read shim. Stays in HOME for now (it calls render);
 * moving it into the UI bank waits on the render-orchestration refactor.
 */
void ui_rank_show(void) {
    rank_entry_t ent[RANK_N];
    uint8_t n = rank_read(ent);
    uint8_t i;
    char buf[24];

    render_set_world(0);
    render_clear_all();
    render_row(0, lang_str(n ? SID_RANK_TITLE : SID_RANK_EMPTY));
    for (i = 0; i < n; i++) {
        /* "1 G1020 B17" — gold and the floor reached (the deepest>final
           climb-back form returns once a later bank move frees HOME). */
        char *p = buf;
        *p++ = (char)('1' + i);
        p = fmt_str(p, " G");
        p = fmt_u16(p, ent[i].gold);
        p = fmt_str(p, " B");
        p = fmt_u16(p, ent[i].final);
        *p = 0;
        render_row((uint8_t)(2u + i * 2u), buf);
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
