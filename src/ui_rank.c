#include <gb/gb.h>
#include "ui_rank.h"
#include "input.h"
#include "render.h"
#include "lang.h"
/* Placeholder until the bank redesign frees HOME room for the full
   ranking screen; shows the title + "no records" line and waits. */
void ui_rank_show(void) {
    render_set_world(0);
    render_clear_all();
    render_row(0, lang_str(SID_RANK_TITLE));
    render_row(8u, lang_str(SID_RANK_EMPTY));
    render_present();
    input_swallow_edges();
    for (;;) { wait_vbl_done(); if (input_pressed()) break; }
    input_swallow_edges();
}
