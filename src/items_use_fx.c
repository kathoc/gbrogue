#include <gb/gb.h>
#pragma bank 2
#include "items_use.h"
#include "items.h"
#include "inventory.h"
#include "identify.h"
#include "world.h"
#include "effects.h"
#include "rng.h"
#include "msg.h"
#include "lang.h"

/*
 * BANK2. The food + potion effect logic, out of the full HOME bank.
 * Reached via call_bank(2, bank_consume_effect) from items_use() in HOME.
 * call_bank maps bank 2 over bank 1, so every callee here is in the fixed
 * bank 0 (rng, identify_learn, inv_consume, effects_ring_worn) and no line
 * renders: messages/xp are enqueued via msgq for the orchestrator to
 * replay. (Scrolls stay in HOME for now: their teleport/spawn/FOV cases
 * still reach bank-1 render/monster helpers.)
 */

/* Marshalling globals (WRAM, bank-independent). */
uint8_t g_use_slot, g_use_turns;

/* ---------------------------------------------------------------- food */

static uint8_t eat(uint8_t slot) {
    uint16_t add = (uint16_t)(750u + (uint16_t)rng_byte() * 3u);
    g_food = (uint16_t)(g_food + add);
    if (g_food > 2000u) g_food = 2000u;
    if (g_food > 300u) g_hunger = 0;
    msgq_id(SID_YUM);
    inv_consume(slot);
    return 1;
}

/* -------------------------------------------------------------- potions */

static void heal(uint8_t dice_sides) {
    uint8_t h = rng_dice(g_level, dice_sides);
    if ((uint16_t)g_hp + h >= g_maxhp) {
        if (g_hp == g_maxhp) g_maxhp++;   /* overheal nudges max, like Rogue */
        g_hp = g_maxhp;
    } else {
        g_hp = (uint8_t)(g_hp + h);
    }
    if (g_blind_t) g_blind_t = 0;
    msgq_id(SID_P_HEAL);
}

static uint8_t quaff(uint8_t slot) {
    uint8_t sub = g_pack[slot].sub;
    identify_learn(IDC_POTION, sub);
    inv_consume(slot);

    switch (sub) {
    case 0:  /* confusion */
        g_conf_t = (uint8_t)(12u + rng_range(8));
        msgq_id(SID_P_CONF);
        break;
    case 1:  /* hallucination */
        g_halluc_t = 120u;
        msgq_id(SID_P_HALLU);
        break;
    case 2:  /* poison */
        if (effects_ring_worn(2u)) {          /* sustain strength */
            msgq_id(SID_P_SICK_M);
        } else {
            uint8_t loss = (uint8_t)(1u + rng_range(3));
            g_str = (g_str > (uint8_t)(loss + 3u)) ? (uint8_t)(g_str - loss) : 3u;
            msgq_id(SID_P_SICK);
        }
        break;
    case 3:  /* gain strength */
        if (g_str < 31u) g_str++;
        if (g_str > g_maxstr) g_maxstr = g_str;
        msgq_id(SID_P_STR);
        break;
    case 4:  /* see invisible */
        g_seeinv_t = 200u;
        msgq_id(SID_P_SEEINV);
        break;
    case 5:  /* healing */
        heal(4);
        break;
    case 6:  /* monster detection */
        g_mondet_t = 60u;
        msgq_id(SID_P_SEEMON);
        break;
    case 7:  /* magic detection */
        msgq_id(SID_P_SEEMAGIC);
        break;
    case 8:  /* raise level */
        /* Both deferred so the pair stays ordered (P_RAISE then the
           welcome-to-level line combat_gain_xp posts) and neither renders
           mid-processing here in BANK2. */
        msgq_id(SID_P_RAISE);
        /* g_level*12 via shifts (12 = 8+4) so it never calls __mulint,
           which lives in bank 1 (unreachable from BANK2). */
        msgq_xp((uint16_t)(((uint16_t)g_level << 3) + ((uint16_t)g_level << 2) + 10u));
        break;
    case 9:  /* extra healing */
        heal(8);
        break;
    case 10: /* haste self */
        g_haste_t = (uint8_t)(10u + rng_range(10));
        msgq_id(SID_P_HASTE);
        break;
    case 11: /* restore strength */
        g_str = g_maxstr;
        msgq_id(SID_P_FIXSTR);
        break;
    case 12: /* blindness */
        g_blind_t = 120u;
        msgq_id(SID_P_BLIND);
        break;
    default: /* levitation */
        g_levit_t = 30u;
        msgq_id(SID_P_LEVIT);
        break;
    }
    return 1;
}

/* call_bank(2, ...) entry: eat or quaff the marshalled pack slot. */
void bank_consume_effect(void) {
    uint8_t slot = g_use_slot;
    if (g_pack[slot].kind == IK_FOOD) g_use_turns = eat(slot);
    else                              g_use_turns = quaff(slot);
}
