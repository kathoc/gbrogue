#include <gb/gb.h>
#include "ui_menu.h"
#include "render.h"
#include "input.h"
#include "world.h"
#include "worldview.h"
#include "status.h"
#include "msg.h"
#include "lang.h"
#include "ui_log.h"
#include "util.h"
#include "sfx.h"
#include "text4.h"

/* Entry order (cursor index). ENTRY_* below must match. Single-line rows
   2.. fit well under the title. */
/* Throwing/firing moved to the inventory (select ammo -> A), so the menu
   no longer has a Throw entry. */
#define N_ENTRIES 5

static const uint8_t ENTRY_SID[N_ENTRIES] = {
    SID_MENU_LOG, SID_MENU_DISPLAY, SID_MENU_SPEED,
    SID_MENU_LANG, SID_MENU_QUIT,
};
#define ENTRY_LOG     0u
#define ENTRY_DISPLAY 1u
#define ENTRY_SPEED   2u
#define ENTRY_LANG    3u
#define ENTRY_QUIT    4u

static void draw_entry(uint8_t i, uint8_t cursor) {
    char buf[32];
    char *p = buf;
    if (cursor == i) p = fmt_str(p, "> ");
    else p = fmt_str(p, "  ");
    p = fmt_str(p, lang_str(ENTRY_SID[i]));
    if (i == ENTRY_SPEED) {
        p = fmt_str(p, ": ");
        p = fmt_str(p, lang_str((uint8_t)(SID_SPEED_SLOW + g_repeat_speed)));
        p = fmt_str(p, "  ");         /* wipe a longer previous label */
    }
    *p = 0;
    render_text(2, (uint8_t)(2u + i), buf);
}

static void draw_menu(uint8_t cursor) {
    uint8_t i;
    render_clear_all();
    render_text(2, 0, lang_str(SID_MENU_TITLE));
    for (i = 0; i < N_ENTRIES; i++) draw_entry(i, cursor);
    render_status(lang_str(SID_MENU_HINT));
    render_present();
}

static void restore_world(void) {
    view_world_enter();
    status_update();
    msg_refresh();
    render_present();
    input_swallow_edges();
}

uint8_t ui_menu_show(void) {
    uint8_t cursor = 0;

    sfx_play(SFX_MENU);
    render_set_world(0);
    input_swallow_edges();
    draw_menu(cursor);

    for (;;) {
        uint8_t keys;
        wait_vbl_done();
        keys = input_pressed();

        if (keys & (J_B | J_START)) {
            restore_world();
            return MENU_CANCEL;
        }
        if (keys & (J_UP | J_DOWN)) {
            uint8_t old = cursor;
            sfx_play(SFX_MENU);
            if (keys & J_UP)
                cursor = (uint8_t)((cursor + N_ENTRIES - 1u) % N_ENTRIES);
            else
                cursor = (uint8_t)((cursor + 1u) % N_ENTRIES);
            draw_entry(old, cursor);
            draw_entry(cursor, cursor);
            /* if the two redrawn rows wrapped the composed-tile pool, the
               untouched rows behind them now point at recycled tiles —
               repaint the whole menu from a fresh pool before presenting */
            if (g_t4_flushed) { g_t4_flushed = 0; draw_menu(cursor); }
            else render_present();
        }
        if (keys & J_A) {
            sfx_play(SFX_MENU);
            if (cursor == ENTRY_SPEED) {
                /* cycle 1 -> 2 -> 3 -> 1 (slow / normal / fast) */
                g_repeat_speed = (uint8_t)((g_repeat_speed + 1u) % 3u);
                draw_entry(ENTRY_SPEED, cursor);
                render_present();
                continue;
            }
            if (cursor == ENTRY_LANG) {
                g_lang ^= 1u;
                draw_menu(cursor);    /* every label swaps language */
                continue;
            }
            switch (cursor) {
            case ENTRY_LOG:                  /* message-log viewer */
                ui_log_show();
                restore_world();
                return MENU_CANCEL;
            case ENTRY_DISPLAY:              /* display toggle (M10) */
                render_toggle_mode();
                restore_world();
                return MENU_CANCEL;
            default:                         /* ENTRY_QUIT */
                restore_world();
                return MENU_SUSPEND;
            }
        }
    }
}
