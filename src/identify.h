#ifndef IDENTIFY_H
#define IDENTIFY_H

#include <stdint.h>

/* Identification classes. */
enum { IDC_POTION, IDC_SCROLL, IDC_WAND, IDC_RING, IDC_COUNT };

/* Shuffle fresh per-game aliases and forget everything. */
void    identify_new_game(void);
/* Alias index shown for subtype `sub` of class `cls` this game. */
uint8_t identify_alias(uint8_t cls, uint8_t sub);
uint8_t identify_known(uint8_t cls, uint8_t sub);
void    identify_learn(uint8_t cls, uint8_t sub);

#endif
