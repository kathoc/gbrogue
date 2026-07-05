#ifndef ITEMS_USE_H
#define ITEMS_USE_H

#include <stdint.h>

/* Context action on pack slot: quaff / read / eat / wield / wear /
   put on. Returns turns consumed (0 or 1). */
uint8_t items_use(uint8_t slot);

#endif
