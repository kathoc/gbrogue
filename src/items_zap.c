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

/* Wait for a D-pad direction (B cancels). The pack modal is dropped
   first so the player aims on the live world, with "Which way?" on the
   message band and "B:cancel" bottom-right. Holding two directions for
   a moment yields a diagonal: after the first edge we give the second
   button a 4-frame grace window, matching the walk input. */
uint8_t g_zap_prompted;

uint8_t items_prompt_dir(int8_t *dx, int8_t *dy) {
    uint8_t picked = 0;
    g_zap_prompted = 1;
    view_world_enter();
    msg_post_id(SID_W_WHICHWAY);   /* UI prompt (rule 4): render live, not queued */
    g_status_hint = SID_HINT_CANCEL;
    status_update();
    msg_refresh();
    /* swallow BEFORE the multi-frame world flush: presses landing
       during the repaint stay latched and register as the aim */
    input_swallow_edges();
    render_present();
    for (;;) {
        uint8_t keys;
        wait_vbl_done();
        view_breathe();
        render_msg_tick();
        keys = input_pressed();
        if (keys & J_B) break;
        if (keys & (J_UP | J_DOWN | J_LEFT | J_RIGHT)) {
            uint8_t grace, held = (uint8_t)(keys | input_held());
            for (grace = 0; grace < 4u; grace++) {
                wait_vbl_done();
                held |= input_held();
            }
            *dx = 0;
            *dy = 0;
            if (held & J_LEFT)  *dx = -1;
            if (held & J_RIGHT) *dx = 1;
            if (held & J_UP)    *dy = -1;
            if (held & J_DOWN)  *dy = 1;
            picked = 1;
            break;
        }
    }
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
    if (!items_prompt_dir(&dx, &dy)) return 0;
    g_zap_slot = slot;
    g_zap_dx = dx;
    g_zap_dy = dy;
    call_bank(2u, bank_zap_effect);
    return g_zap_turns;
}
