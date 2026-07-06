#include <gb/gb.h>
#include "ui_log.h"
#include "render.h"
#include "input.h"
#include "msg.h"
#include "lang.h"

/* rows 2..13: the 12 most recent lines, oldest on top. Kept to 12 (not
   the full 16-line ring) so a screen of long lines stays under the
   composed-tile pool budget and never wraps mid-draw. */
#define LOG_VIEW_ROWS 12u

void ui_log_show(void) {
    uint8_t r;
    render_set_world(0);
    render_clear_all();
    render_row(0, lang_str(SID_MENU_LOG));
    for (r = 0; r < LOG_VIEW_ROWS; r++)
        render_row((uint8_t)(2u + r),
                   msg_log_line((uint8_t)(LOG_VIEW_ROWS - 1u - r)));
    render_status(lang_str(SID_HINT_CANCEL));
    render_present();

    input_swallow_edges();
    for (;;) {
        wait_vbl_done();
        if (input_pressed() & (J_A | J_B | J_START)) break;
    }
    input_swallow_edges();
}
