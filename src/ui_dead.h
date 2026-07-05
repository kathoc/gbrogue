#ifndef UI_DEAD_H
#define UI_DEAD_H

#include <gb/gb.h>

/* Full-screen tombstone; returns when the player presses START. Banked
   (called through the trampoline) to keep HOME free. */
void ui_dead_show(void);
/* Victory screen (escaped with the Amulet). */
void ui_win_show(void);

#endif
