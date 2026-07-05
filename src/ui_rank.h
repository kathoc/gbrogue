#ifndef GBR_UI_RANK_H
#define GBR_UI_RANK_H

#include <gb/gb.h>

/* Persistent high-score ranking screen (opened from the title). Returns
   when the player backs out with B/A/START. Lives in a switchable ROM
   bank (called through the GBDK banked-call trampoline) to keep the
   32KB HOME area free. */
void ui_rank_show(void);

#endif
