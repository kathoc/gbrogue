#include "rng.h"
/*
 * NOTE: filename is deliberately early-sorting so the linker places this
 * translation unit in BANK0 (the fixed, always-mapped 16KB). rng must be
 * reachable from banked code (BANK2/3 items & monsters call it), and a
 * banked caller can only reach BANK0 or its own bank. Self-contained LCG.
 */


/* Full 32-bit state so every one of the seed's eight hex digits selects a
   distinct run (2^32 maps, no folding collisions). */
static uint32_t s;

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

/* xorshift32 (Marsaglia). Chosen over an LCG because it uses NO multiply,
   so — like mul16 above — it never reaches a cross-bank SDCC helper
   (__mullong / __mulint) that would crash when the RNG is called with
   BANK2/3 mapped. The three shifts are done byte-wise (byte reindex + a
   single small in-byte shift each) so SDCC emits only loads, stores, xors
   and shift-by-1s — never a 32-bit shift helper (__rrulong/__rlulong),
   which would have the same cross-bank hazard. Period 2^32-1; 0 is a fixed
   point and is kept out by rng_seed(). */
static void xorshift32(void) {
    /* s ^= s << 13 */
    {
        uint8_t b0 = (uint8_t)s;
        uint8_t b1 = (uint8_t)(s >> 8);
        uint8_t b2 = (uint8_t)(s >> 16);
        /* (s << 13) low..high bytes: byte0=0, byte1=b0<<5,
           byte2=(b1<<5)|(b0>>3), byte3=(b2<<5)|(b1>>3) */
        uint8_t t1 = (uint8_t)(b0 << 5);
        uint8_t t2 = (uint8_t)((b1 << 5) | (b0 >> 3));
        uint8_t t3 = (uint8_t)((b2 << 5) | (b1 >> 3));
        s ^= ((uint32_t)t3 << 24) | ((uint32_t)t2 << 16) | ((uint32_t)t1 << 8);
    }
    /* s ^= s >> 17 */
    {
        uint8_t b2 = (uint8_t)(s >> 16);
        uint8_t b3 = (uint8_t)(s >> 24);
        /* (s >> 17) = (top two bytes >> 1), landing in byte0/byte1 */
        uint8_t r0 = (uint8_t)((b2 >> 1) | (b3 << 7));
        uint8_t r1 = (uint8_t)(b3 >> 1);
        s ^= ((uint32_t)r1 << 8) | (uint32_t)r0;
    }
    /* s ^= s << 5 */
    {
        uint8_t b0 = (uint8_t)s;
        uint8_t b1 = (uint8_t)(s >> 8);
        uint8_t b2 = (uint8_t)(s >> 16);
        uint8_t b3 = (uint8_t)(s >> 24);
        uint8_t t0 = (uint8_t)(b0 << 5);
        uint8_t t1 = (uint8_t)((b1 << 5) | (b0 >> 3));
        uint8_t t2 = (uint8_t)((b2 << 5) | (b1 >> 3));
        uint8_t t3 = (uint8_t)((b3 << 5) | (b2 >> 3));
        s ^= ((uint32_t)t3 << 24) | ((uint32_t)t2 << 16)
           | ((uint32_t)t1 << 8) | (uint32_t)t0;
    }
}

void rng_seed(uint32_t seed) {
    if (seed == 0) seed = 0x1D2D1D2Du;   /* xorshift32 must never be 0 */
    s = seed;
}

uint32_t rng_state(void) {
    return s;
}

uint16_t rng_word(void) {
    xorshift32();
    return (uint16_t)(s >> 16);          /* high half is the better-mixed one */
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
