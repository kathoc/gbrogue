#include <gb/gb.h>
#include "ui_popup.h"
#include "render.h"
#include "input.h"
#include "worldview.h"
#include "status.h"
#include "msg.h"
#include "lang.h"
#include "text4.h"

static void popup_row(uint8_t y, const char *s) {
    /* half-width text between full-width '|' borders (36 char field) */
    char buf[37];
    uint8_t i = 0;
    render_glyph(0, y, '|');
    if (s) {
        while (s[i] && i < 36u) {
            buf[i] = s[i];
            i++;
        }
    }
    while (i < 36u) buf[i++] = ' ';
    buf[36] = 0;
    render_text(1, y, buf);
    render_glyph(SCREEN_W - 1u, y, '|');
}

static void popup_border(uint8_t y) {
    uint8_t x;
    for (x = 0; x < SCREEN_W; x++) render_glyph(x, y, '-');
}

void ui_popup(const char *l1, const char *l2, const char *l3) {
    uint8_t was_world = render_world_on();
    if (was_world) {
        /* Freeze a snapshot of the scene as the popup backdrop. */
        render_set_world(0);
        render_clear_all();
        view_draw();
        status_update();
        msg_refresh();
    }
    popup_border(5);
    popup_row(6, l1);
    popup_row(7, l2);
    popup_row(8, l3 ? l3 : "");
    popup_row(9, "        -A-");
    popup_border(10);
    render_present();
    input_swallow_edges();
    for (;;) {
        wait_vbl_done();
        if (input_pressed() & (J_A | J_B)) break;
    }
    input_swallow_edges();
    if (was_world) {
        view_world_enter();
        status_update();
        msg_refresh();
        render_present();
    }
}

/* Fast fade for the trap-door plunge (snappier than a floor transition). */
#define TRAP_FADE 15u

/* Tile column that horizontally centers a mixed half/full-width string on
   the 40-half-width row (full-width glyphs count double). */
static uint8_t center_of(const char *s) {
    uint8_t w = 0;
    while (*s) {
        w = (uint8_t)(w + (T4_IS_FULL((uint8_t)*s) ? 2u : 1u));
        s++;
    }
    return w >= 40u ? 0u : (uint8_t)((40u - w) / 4u);
}

void ui_trapdoor(void) {
    render_fade_out(TRAP_FADE);
    render_set_world(0);
    render_clear_all();
    /* centered vertically (rows 7/8) with the A-icon just below (row 10) */
    {
        const char *s = lang_str(SID_PLUNGE1);
        render_text(center_of(s), 7, s);
    }
    {
        const char *s = lang_str(SID_PLUNGE2);
        render_text(center_of(s), 8, s);
    }
    {
        const char *s = lang_str(SID_TRAP_A);
        render_text(center_of(s), 10, s);
    }
    render_present();
    render_fade_in(TRAP_FADE);

    input_swallow_edges();
    for (;;) {
        wait_vbl_done();
        if (input_pressed() & J_A) break;
    }
    input_swallow_edges();
    render_fade_out(TRAP_FADE);
}
