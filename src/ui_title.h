#ifndef UI_TITLE_H
#define UI_TITLE_H

#include <stdint.h>

/* Title screen with the full-screen art and a cursor menu. Returns 1
   when the player chose Continue (a valid save exists), 0 for a new
   game (the RNG is seeded on that confirm). */
uint8_t ui_title_show(void);

#endif
