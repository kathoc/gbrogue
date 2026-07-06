#ifndef ITEMS_THROW_H
#define ITEMS_THROW_H

#include <stdint.h>

/* Fire/throw the ammo in pack slot `slot` (arrows / darts / shuriken):
   prompts for a direction, then the projectile flies. No bow needed.
   Returns turns consumed (0 = aim cancelled). */
uint8_t items_fire(uint8_t slot);

/* --- BANK2 relocation ---
   The post-aim projectile flight + hit lives in items_throw_fx.c
   (#pragma bank 2), reached only through items_throw(), which marshals
   the ammo slot / launcher bonus / aim into these WRAM globals and hops
   in via call_bank(2, bank_throw_effect). All callees are in bank 0. */
extern uint8_t g_throw_slot, g_throw_bonus;
extern int8_t  g_throw_dx, g_throw_dy;
extern uint8_t g_throw_turns;
void bank_throw_effect(void);   /* BANK2 entry — run via call_bank only */

#endif
