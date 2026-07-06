#include <gb/gb.h>
#include "bankcall.h"
#include "items_zap.h"
#include "items.h"
#include "inventory.h"
#include "identify.h"
#include "world.h"
#include "map.h"
#include "actor.h"
#include "monsters.h"
#include "combat.h"
#include "effects.h"
#include "rng.h"
#include "msg.h"
#include "lang.h"
#include "render.h"
#include "input.h"
#include "worldview.h"
#include "status.h"
#include "util.h"

/* Aim a throw/zap on the live world. The pack modal is dropped first, a
   "<verb> which way?" line lands on the log, and a blinking arrow cursor
   sits on the player pointing in the current direction. The D-pad turns
   the cursor (holding two for a moment yields a diagonal, a 4-frame grace
   matching the walk input), A confirms and fires, B cancels. */
uint8_t g_zap_prompted;

uint8_t items_prompt_dir(int8_t *dx, int8_t *dy, uint8_t verb_sid) {
    uint8_t picked = 0, blink = 1, bt = 0;
    int8_t cx = 0, cy = -1;                 /* default aim: straight up */
    g_zap_prompted = 1;
    view_world_enter();
    {
        /* "<verb> which way?" — fmt_str copies each lang_str out before
           the next call, so the 2-deep lang buffer never clobbers. */
        char b[LOG_COLS + 1];
        char *q = fmt_str(b, lang_str(verb_sid));
        q = fmt_str(q, " ");
        q = fmt_str(q, lang_str(SID_AIM_DIR));
        *q = 0;
        msg_post(b);
    }
    g_status_hint = SID_HINT_CANCEL;
    status_update();
    msg_refresh();
    /* swallow BEFORE the multi-frame world flush: presses landing during
       the repaint stay latched */
    input_swallow_edges();
    render_present();
    view_aim_cursor(cx, cy, 1);
    for (;;) {
        uint8_t keys;
        wait_vbl_done();
        view_breathe();
        render_msg_tick();
        if (++bt >= 12u) { bt = 0; blink ^= 1u; }   /* ~0.4s blink */
        view_aim_cursor(cx, cy, blink);
        keys = input_pressed();
        if (keys & J_B) break;                       /* cancel */
        if (keys & (J_UP | J_DOWN | J_LEFT | J_RIGHT)) {
            uint8_t grace, held = (uint8_t)(keys | input_held());
            int8_t nx = 0, ny = 0;
            for (grace = 0; grace < 4u; grace++) {
                wait_vbl_done();
                held |= input_held();
            }
            if (held & J_LEFT)  nx = -1; else if (held & J_RIGHT) nx = 1;
            if (held & J_UP)    ny = -1; else if (held & J_DOWN)  ny = 1;
            if (nx || ny) { cx = nx; cy = ny; blink = 1; bt = 0; }
            view_aim_cursor(cx, cy, 1);
        }
        if (keys & J_A) {                            /* confirm + fire */
            *dx = cx;
            *dy = cy;
            picked = 1;
            break;
        }
    }
    view_aim_cursor(0, 0, 0);          /* remove the cursor sprite */
    g_status_hint = 0;
    status_update();
    render_present();
    return picked;
}

/* UI orchestrator (stays in the fixed bank): charge check, then aim on the
   live world (rule 4 input-wait), then marshal the aim and hop into BANK2
   for the effect (items_zap_fx.c). msgq_flush in the caller (ui_inv) then
   replays the queued messages/kills the banked effect recorded. */
uint8_t items_zap(uint8_t slot) {
    item_t *it = &g_pack[slot];
    int8_t dx, dy;

    if (it->qty == 0) {
        msgq_id(SID_S_NOTHING);
        identify_learn(IDC_WAND, it->sub);
        return 1;
    }
    if (!items_prompt_dir(&dx, &dy, SID_AIM_ZAP)) return 0;
    g_zap_slot = slot;
    g_zap_dx = dx;
    g_zap_dy = dy;
    call_bank(2u, bank_zap_effect);
    return g_zap_turns;
}
