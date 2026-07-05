#ifndef GBR_SFX_H
#define GBR_SFX_H

#include <stdint.h>

/*
 * Sound effects. IDs double as test hooks: g_sfx_last records the most
 * recent request (even one the priority rules muted), so the PyBoy
 * harness can assert "this action made this sound" without audio.
 */
#define SFX_NONE   0u
#define SFX_STEP_A 1u   /* footstep "zu"  (alternates with STEP_B) */
#define SFX_STEP_B 2u   /* footstep "za" */
#define SFX_MISS   3u   /* your swing whiffs      "susa" */
#define SFX_BUMP   4u   /* walked into a wall     "don"  */
#define SFX_ITEM   5u   /* picked up an item      "piko" */
#define SFX_GOLD   6u   /* picked up gold         "charin" */
#define SFX_HIT    7u   /* your blow lands        "gaga" */
#define SFX_HURT   8u   /* a monster hits you     "doka" */
#define SFX_TRAP   9u   /* a trap springs         "derore" */
#define SFX_MENU   10u  /* menu cursor / toggle   "pi" */
#define SFX_STAIRS 11u  /* taking the stairs      "da da da" */
#define SFX_LVLUP  12u  /* level up               rising arpeggio */
#define SFX_REST   13u  /* resting / searching    "sa" */
#define SFX_COUNT  14u

void sfx_init(void);
void sfx_play(uint8_t id);

extern volatile uint8_t g_sfx_last;

#endif
