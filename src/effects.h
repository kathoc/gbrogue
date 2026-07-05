#ifndef EFFECTS_H
#define EFFECTS_H

#include <stdint.h>

/* Per-turn upkeep: condition timers, hunger, worn-ring effects.
   Call exactly once per consumed player turn. */
void effects_turn(void);

/* Ring queries (worn rings only). */
uint8_t effects_ring_worn(uint8_t sub);       /* 1 if any worn ring has subtype */
int8_t  effects_ring_ench_sum(uint8_t sub);   /* sum of their enchants */

/* Relocate the player to a random walkable cell. */
void teleport_player(void);

#endif
