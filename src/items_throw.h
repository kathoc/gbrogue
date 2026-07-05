#ifndef ITEMS_THROW_H
#define ITEMS_THROW_H

#include <stdint.h>

/* Fire/throw: arrows with a wielded bow, else darts / shuriken.
   Prompts for a direction. Returns turns consumed. */
uint8_t items_throw(void);

#endif
