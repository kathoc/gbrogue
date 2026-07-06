#include <gb/gb.h>
#include "monsters.h"
#include "rng.h"
#include "farcopy.h"

/* Stats follow Rogue 5.4's monsters[] table (level, armor, exp, damage
   strings). Special on-hit abilities (rust, freeze, steal, drain) land
   in M7 — docs/status.md tracks which are live. */
/* mkind(), monster_roll_hp() and monster_pick() live in bank0_monster.c
   (fixed bank) so banked item logic can reach them; the stat cache and the
   spawn ladder live there too. */

