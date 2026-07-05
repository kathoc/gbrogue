#ifndef UI_INV_H
#define UI_INV_H

#include <stdint.h>

/* Modal pack screen. Returns the number of turns consumed (0 or 1).
   A = use/equip, SELECT = drop, B = close. */
uint8_t ui_inv_show(void);

#endif
