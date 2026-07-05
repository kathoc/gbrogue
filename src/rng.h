#ifndef RNG_H
#define RNG_H

#include <stdint.h>

void     rng_seed(uint16_t seed);
uint16_t rng_state(void);        /* for save.c */
uint16_t rng_word(void);
uint8_t  rng_byte(void);
uint8_t  rng_range(uint8_t n);   /* 0..n-1 */
uint8_t  rng_dice(uint8_t count, uint8_t sides);

#endif
