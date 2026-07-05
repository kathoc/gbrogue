#include "rng.h"
/*
 * NOTE: filename is deliberately early-sorting so the linker places this
 * translation unit in BANK0 (the fixed, always-mapped 16KB). rng must be
 * reachable from banked code (BANK2/3 items & monsters call it), and a
 * banked caller can only reach BANK0 or its own bank. Self-contained LCG.
 */


static uint16_t s;

/* 16-bit multiply done with shift-and-add so it emits ONLY inline adds and
   shift-by-1s — never a call to SDCC's __mulint helper. That matters
   because __mulint lives in bank 1, and this RNG is called from banked
   code (BANK2 item effects); reaching a bank-1 helper while bank 2 is
   mapped executes garbage and crashes. Result is identical to `a * b`
   (mod 2^16), so the RNG sequence is unchanged. */
static uint16_t mul16(uint16_t a, uint16_t b) {
    uint16_t r = 0;
    while (b) {
        if (b & 1u) r = (uint16_t)(r + a);
        a = (uint16_t)(a << 1);
        b = (uint16_t)(b >> 1);
    }
    return r;
}

void rng_seed(uint16_t seed) {
    if (seed == 0) seed = 0x1D2Du;
    s = seed;
}

uint16_t rng_state(void) {
    return s;
}

uint16_t rng_word(void) {
    s = (uint16_t)(mul16(s, 25173u) + 13849u);
    return s;
}

uint8_t rng_byte(void) {
    return (uint8_t)(rng_word() >> 8);
}

/* Uniform-ish 0..n-1 (n >= 1). High byte drives the scale. */
uint8_t rng_range(uint8_t n) {
    return (uint8_t)(mul16((uint16_t)rng_byte(), (uint16_t)n) >> 8);
}

/* Rogue-style dice: total of `count` rolls of 1..sides. */
uint8_t rng_dice(uint8_t count, uint8_t sides) {
    uint8_t total = 0;
    while (count--) total += (uint8_t)(1u + rng_range(sides));
    return total;
}
