#include <gb/gb.h>
#include "ui_popup.h"
#include "render.h"
#include "input.h"
#include "worldview.h"
#include "status.h"
#include "msg.h"

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
