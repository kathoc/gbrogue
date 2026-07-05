#include "rng.h"

static uint16_t s;

void rng_seed(uint16_t seed) {
    if (seed == 0) seed = 0x1D2Du;
    s = seed;
}

uint16_t rng_state(void) {
    return s;
}

uint16_t rng_word(void) {
    s = (uint16_t)(s * 25173u + 13849u);
    return s;
}

uint8_t rng_byte(void) {
    return (uint8_t)(rng_word() >> 8);
}

/* Uniform-ish 0..n-1 (n >= 1). High byte drives the scale. */
uint8_t rng_range(uint8_t n) {
    return (uint8_t)(((uint16_t)rng_byte() * n) >> 8);
}

/* Rogue-style dice: total of `count` rolls of 1..sides. */
uint8_t rng_dice(uint8_t count, uint8_t sides) {
    uint8_t total = 0;
    while (count--) total += (uint8_t)(1u + rng_range(sides));
    return total;
}
