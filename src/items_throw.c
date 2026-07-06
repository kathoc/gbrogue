#include "bankcall.h"
#include "items_throw.h"
#include "items.h"
#include "items_zap.h"
#include "inventory.h"
#include "world.h"
#include "msg.h"
#include "lang.h"

/* Fire/throw the item in `slot` (arrows, darts, shuriken) from the pack:
   aim on the live world (rule 4 input-wait), then marshal and hop into
   BANK2 for the flight + hit (items_throw_fx.c). No bow needed — arrows
   fly on their own. ui_inv's msgq_flush replays the messages after. */
uint8_t items_fire(uint8_t slot) {
    item_t *it = &g_pack[slot];
    int8_t dx, dy;

    if (it->qty == 0) {
        msgq_id(SID_TH_NOTHING);
        return 1;
    }
    if (!items_prompt_dir(&dx, &dy,
                          it->sub == WS_ARROW ? SID_AIM_FIRE : SID_AIM_THROW))
        return 0;
    g_throw_slot = slot;
    g_throw_bonus = 0;               /* no launcher: no bow bonus */
    g_throw_dx = dx;
    g_throw_dy = dy;
    call_bank(2u, bank_throw_effect);
    return g_throw_turns;
}
