#ifndef ACTOR_H
#define ACTOR_H

#include <stdint.h>

#define MAX_MONSTERS 12
#define MON_NONE 0xFFu

/* Runtime monster state flags. */
#define MST_AWAKE 0x01u
/* The player has actually seen this monster drawn on screen at least
   once. Cleared for freshly spawned monsters (reveal grace): a monster
   the player has never seen may not strike on the turn it first appears. */
#define MST_SEEN  0x02u

/* Timed / magical conditions (eff). HELD, CONF and FLEE expire when
   eff_t hits 0; SLOW / HASTE / INVIS persist until cancelled. */
#define MEF_HELD  0x01u
#define MEF_CONF  0x02u
#define MEF_FLEE  0x04u
#define MEF_SLOW  0x08u
#define MEF_HASTE 0x10u
#define MEF_INVIS 0x20u

typedef struct {
    uint8_t kind;      /* MON_NONE = empty slot */
    uint8_t x, y;
    uint8_t hp;
    uint8_t state;
    uint8_t eff;
    uint8_t eff_t;
} monster_t;

extern monster_t g_mons[MAX_MONSTERS];

void       mons_clear(void);
void       mons_spawn_level(void);
monster_t *mon_at(uint8_t x, uint8_t y);
/* Apply damage; returns 1 if the monster died (slot freed, xp NOT yet
   granted — combat.c handles rewards/messages). */
uint8_t    mon_damage(monster_t *m, uint8_t dmg);
/* One turn for every living monster: wake checks, chase, attack. */
void       mons_take_turns(void);
/* Toggle B-dash mode: throttles the chase-map rebuild so a run stays
   fast even with monsters awake (they are off-screen during a dash). */
void       mons_dash(uint8_t on);
/* Wandering-monster clock: call once per player turn. Every so often a
   fresh hunter appears somewhere out of sight. */
void       mons_wander_tick(void);

#endif
