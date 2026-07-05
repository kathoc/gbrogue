#ifndef COMBAT_H
#define COMBAT_H

#include <stdint.h>
#include "actor.h"

/* Bump attack: the player swings at m. Handles kill, xp, messages. */
void combat_player_attack(monster_t *m);
/* One full monster attack routine (all its dice) against the player. */
void combat_monster_attack(monster_t *m);
/* Award xp and handle level-ups (messages included). */
void combat_gain_xp(uint16_t xp);
/* "You killed the X" (shared with wand bolts). */
void combat_report_kill(uint8_t kind);
/* Scroll of monster confusion: next successful hit confuses. */
void combat_set_confuse_hit(void);

#endif
