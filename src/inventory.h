#ifndef INVENTORY_H
#define INVENTORY_H

#include <stdint.h>
#include "items.h"

#define PACK_SLOTS 16          /* labels a..p (GB screen height limit) */
#define SLOT_NONE  0xFFu

extern item_t  g_pack[PACK_SLOTS];
/* Equipped slot indices into g_pack, SLOT_NONE when empty. */
extern uint8_t g_wield;         /* weapon in hand */
extern uint8_t g_worn;          /* armor being worn */
extern uint8_t g_ring_l, g_ring_r;

void    inv_clear(void);
/* Add a copy of *it; returns slot or SLOT_NONE if full. Stacks food /
   potions / scrolls / ammo of identical subtype. */
uint8_t inv_add(const item_t *it);
/* Remove one from a slot (clears it when qty hits 0). Unequips. */
void    inv_consume(uint8_t slot);
/* Close pack gaps (equip indices follow). Call after any removal. */
void    inv_compact(void);
uint8_t inv_count(void);
/* Effective player AC from worn armor + protection rings. */
uint8_t inv_player_ac(void);
/* Wield/wear/put on: handles curse rules + messages. Returns 1 if a
   turn was consumed. */
uint8_t inv_equip(uint8_t slot);
/* Starting kit: some food, mace +1, ring mail +1 (worn), bow + arrows. */
void    inv_starting_kit(void);

#endif
