#include "bankcall.h"
#include "items_throw.h"
#include "items.h"
#include "items_zap.h"
#include "inventory.h"
#include "world.h"
#include "msg.h"
#include "lang.h"

/* WEAPON subtypes */
#define WS_BOW     2u
#define WS_ARROW   3u
#define WS_DART    6u
#define WS_SHURIKEN 7u

static uint8_t find_ammo(uint8_t *bonus_hit) {
    uint8_t i;
    uint8_t have_bow = (g_wield != SLOT_NONE &&
                        g_pack[g_wield].kind == IK_WEAPON &&
                        g_pack[g_wield].sub == WS_BOW);
    *bonus_hit = 0;
    if (have_bow) {
        for (i = 0; i < PACK_SLOTS; i++)
            if (g_pack[i].kind == IK_WEAPON && g_pack[i].sub == WS_ARROW) {
                *bonus_hit = 2;        /* launcher match, like Rogue */
                return i;
            }
    }
    for (i = 0; i < PACK_SLOTS; i++)
        if (g_pack[i].kind == IK_WEAPON &&
            (g_pack[i].sub == WS_DART || g_pack[i].sub == WS_SHURIKEN))
            return i;
    return SLOT_NONE;
}

/* UI orchestrator (stays in the fixed bank): pick ammo, aim on the live
   world (rule 4 input-wait), then marshal and hop into BANK2 for the
   flight + hit (items_throw_fx.c). ui_menu's msgq_flush replays after. */
uint8_t items_throw(void) {
    int8_t dx, dy;
    uint8_t bonus, slot = find_ammo(&bonus);

    if (slot == SLOT_NONE) {
        msgq_id(SID_TH_NOTHING);
        return 0;
    }
    if (!items_prompt_dir(&dx, &dy, SID_AIM_THROW)) return 0;
    g_throw_slot = slot;
    g_throw_bonus = bonus;
    g_throw_dx = dx;
    g_throw_dy = dy;
    call_bank(2u, bank_throw_effect);
    return g_throw_turns;
}
