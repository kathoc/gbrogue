#include "world.h"
#include "msg.h"

uint8_t  g_px, g_py;
uint8_t  g_depth;
uint8_t  g_hp, g_maxhp;
uint8_t  g_str, g_maxstr;
uint8_t  g_ac;
uint8_t  g_level;
uint16_t g_xp;
uint16_t g_gold;
uint16_t g_turns;

uint16_t g_food;
uint8_t  g_hunger;
uint8_t g_conf_t, g_blind_t, g_haste_t, g_sleep_t, g_levit_t;
uint8_t g_seeinv_t, g_halluc_t, g_mondet_t, g_held_t;
uint8_t g_has_amulet;
uint8_t g_won;
uint8_t g_deepest;
uint8_t g_debug;         /* set at the title; not persisted */
/* RNG seed control (set at the title / by the test harness; not reset by
   world_new). g_seed_override != 0 forces that seed for the next new
   game; g_run_seed records the seed actually used so it can be shown and
   reproduced. */
uint16_t g_seed_override;
uint16_t g_run_seed;
uint8_t  g_repeat_speed = 1;
uint16_t g_wander_t;
uint8_t  g_lang = 1;     /* player setting — survives new games;
                            ships in Japanese (LANG_JA), the title's
                            LANGUAGE row switches to English */

void world_new(void) {
    g_death_cause[0] = 0;
    g_death_sid = 0;    /* no fatal blow yet (won runs keep this 0) */
    g_death_mon = 0;
    /* Rogue 5.4 opening stats. AC 8 = the starting ring mail. */
    g_depth = 1;
    g_hp = 12; g_maxhp = 12;
    g_str = 16; g_maxstr = 16;
    g_ac = 8;
    g_level = 1;
    g_xp = 0;
    g_gold = 0;
    g_turns = 0;
    g_food = 1300u;              /* Rogue's HUNGERTIME */
    g_hunger = 0;
    g_conf_t = g_blind_t = g_haste_t = g_sleep_t = g_levit_t = 0;
    g_seeinv_t = g_halluc_t = g_mondet_t = g_held_t = 0;
    g_has_amulet = 0;
    g_won = 0;
    g_deepest = 1;
    g_wander_t = 0;
    /* g_debug is set at the title and must NOT be cleared here (world_new
       runs after the title code has already flagged the run). */
    /* g_repeat_speed is a player setting — survives new games */
}
