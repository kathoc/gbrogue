#ifndef GBR_WORLD_H
#define GBR_WORLD_H

#include <stdint.h>

/*
 * Player + run state. Individual globals (not one struct) so the .sym
 * file gives tests a stable address per field and sm83 code gets direct
 * addressing.
 */
extern uint8_t  g_px, g_py;        /* player world position */
extern uint8_t  g_depth;           /* dungeon level, 1-based */
extern uint8_t  g_hp, g_maxhp;
extern uint8_t  g_str, g_maxstr;
extern uint8_t  g_ac;              /* lower is better, Rogue-style */
extern uint8_t  g_level;
extern uint16_t g_xp;
extern uint16_t g_gold;
extern uint16_t g_turns;

/* Hunger. HS_* thresholds live in effects.c. */
extern uint16_t g_food;
extern uint8_t  g_hunger;      /* 0 ok, 1 hungry, 2 weak, 3 fainting */

/* Timed player conditions (turns remaining). */
extern uint8_t g_conf_t;       /* confused: moves go random */
extern uint8_t g_blind_t;      /* blind: no sight beyond own tile */
extern uint8_t g_haste_t;      /* hasted: two actions per monster turn */
extern uint8_t g_sleep_t;      /* asleep / frozen: turns skipped */
extern uint8_t g_levit_t;      /* floating: traps don't trigger */
extern uint8_t g_seeinv_t;     /* see invisible */
extern uint8_t g_halluc_t;     /* hallucinating: monster glyphs scramble */
extern uint8_t g_mondet_t;     /* sense all monsters on the level */
extern uint8_t g_held_t;       /* bear trap / flytrap grip */
extern uint8_t g_has_amulet;
extern uint8_t g_won;
/* Deepest floor reached this run (for the ranking's "B26->B3" form). */
extern uint8_t g_deepest;
/* Debug invincibility (set from the title code). A run flagged debug is
   kept out of the ranking. Not saved/persisted. */
extern uint8_t g_debug;

/* RNG seed control. g_seed_override != 0 pins the seed for the next new
   game (title seed entry / test harness); g_run_seed is the seed actually
   used, for display and reproduction. Not reset by world_new. */
extern uint32_t g_seed_override;
extern uint32_t g_run_seed;

/* Input repeat speed setting: 0 slow / 1 normal / 2 fast. Applies to
   held-D-pad walking and to the A+B rest repeat. */
extern uint8_t  g_repeat_speed;
/* Turns since the last wandering-monster spawn. */
extern uint16_t g_wander_t;
/* UI language: 0 English, 1 Japanese (kana, Misaki font). */
extern uint8_t  g_lang;

void world_new(void);

#endif
