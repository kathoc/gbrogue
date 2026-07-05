#ifndef TRAPS_H
#define TRAPS_H

#include <stdint.h>

/* Rogue 5.4 trap set. */
enum {
    TR_TRAPDOOR, TR_BEAR, TR_SLEEP, TR_ARROW, TR_DART, TR_TELEPORT,
    TR_KIND_COUNT
};

#define MAX_TRAPS 8

typedef struct {
    uint8_t x, y, kind;
} trap_t;

extern trap_t  g_traps[MAX_TRAPS];
extern uint8_t g_trap_count;

void traps_clear(void);
/* Register a trap; the map cell must already be TI_TRAP | MF_HIDDEN. */
void traps_add(uint8_t x, uint8_t y, uint8_t kind);
/* Player stepped on (x,y): trigger if a trap is there. Returns 1 if a
   trapdoor fired (caller must regenerate the level). */
uint8_t traps_step(void);
/* Reveal hidden traps in the 8 cells around the player (search). */
void traps_search(void);

#endif
