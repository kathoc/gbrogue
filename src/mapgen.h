#ifndef MAPGEN_H
#define MAPGEN_H

#include <stdint.h>

/* Generate a fresh level into g_map / g_rooms and place the player. */
void mapgen_generate(void);

#endif
