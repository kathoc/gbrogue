#ifndef ITEMS_USE_H
#define ITEMS_USE_H

#include <stdint.h>

/* Context action on pack slot: quaff / read / eat / wield / wear /
   put on. Returns turns consumed (0 or 1). */
uint8_t items_use(uint8_t slot);

/* --- BANK2 relocation ---
   The food/potion effect logic lives in items_use_fx.c (#pragma bank 2),
   reached from items_use() (the FOOD/POTION dispatch) via call_bank(2,
   bank_consume_effect); the slot is marshalled through g_use_slot and the
   turn count comes back in g_use_turns. All callees are in bank 0. */
extern uint8_t g_use_slot, g_use_turns;
void bank_consume_effect(void);   /* BANK2 entry — run via call_bank only */
void bank_read_scroll(void);      /* BANK2 entry — run via call_bank only */

#endif
