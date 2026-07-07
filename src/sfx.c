#include <gb/gb.h>
#include "sfx.h"
#include "bgm.h"

/*
 * Tiny APU effect driver. Each effect is a short list of register
 * "steps" on pulse 1 (NR10..14) or noise (NR41..44); a VBL handler
 * advances the list once per frame, so effects keep time through every
 * blocking wait (move glide, damage flashes, popups) with no per-loop
 * calls at the wait sites.
 *
 * One effect plays at a time. A request that arrives while another is
 * sounding parks in a single pending slot when it is at least as loud
 * (priority), so "you hit, then it hits back" plays as two sounds in
 * sequence instead of the second erasing the first; quieter requests
 * (footsteps under a coin ring) are dropped.
 */

typedef struct {
    uint8_t dur;    /* frames this step lasts; 0 terminates the list */
    uint8_t ch;     /* CH_PULSE / CH_SWEEP / CH_NOISE */
    uint8_t env;    /* NR12 / NR42: initial volume + decay */
    uint8_t shp;    /* NR11 duty (pulse) / NR43 polynomial (noise) */
    uint8_t flo;    /* NR13, pulse only */
    uint8_t fhi;    /* NR14 low 3 bits, pulse only */
} sfx_step_t;

#define CH_PULSE 0u
#define CH_SWEEP 1u     /* pulse 1 with a fast downward pitch sweep */
#define CH_NOISE 2u

/* Footsteps: two noise scrapes, dull then slightly brighter (~2048 vs
   ~2731 Hz noise base — a close pair, not an octave) — "zu, za".
   Volume 5 ~ 75% of the old 6: present but under everything else. */
static const sfx_step_t FX_STEP_A[] = {
    { 4u, CH_NOISE, 0x52u, 0x62u, 0, 0 },
    { 0, 0, 0, 0, 0, 0 },
};
static const sfx_step_t FX_STEP_B[] = {
    { 4u, CH_NOISE, 0x52u, 0x53u, 0, 0 },
    { 0, 0, 0, 0, 0, 0 },
};

/* Whiff: two soft high hisses — "su, sa". */
static const sfx_step_t FX_MISS[] = {
    { 3u, CH_NOISE, 0x31u, 0x25u, 0, 0 },
    { 4u, CH_NOISE, 0x21u, 0x13u, 0, 0 },
    { 0, 0, 0, 0, 0, 0 },
};

/* Wall bump: a short 12.5%-duty blip on pulse 1 with a fast downward
   sweep — a clean Dragon-Quest-ish "boop" instead of harsh noise. The
   envelope kills it quickly; game.c adds the screen jolt. */
static const sfx_step_t FX_BUMP[] = {
    { 4u, CH_SWEEP, 0xA1u, 0x00u, 0x80u, 0x05u },   /* 12.5% duty, ~205 Hz swept down */
    { 0, 0, 0, 0, 0, 0 },
};

/* Item pickup: quick high-low blip — "pi, ko". */
static const sfx_step_t FX_ITEM[] = {
    { 3u, CH_PULSE, 0x81u, 0x80u, 0xACu, 0x07u },   /* ~1568 Hz */
    { 5u, CH_PULSE, 0x72u, 0x80u, 0x7Bu, 0x07u },   /* ~988 Hz */
    { 0, 0, 0, 0, 0, 0 },
};

/* Gold: two-tone coin chime an octave up, short release — "charin". */
static const sfx_step_t FX_GOLD[] = {
    { 4u, CH_PULSE, 0x91u, 0x40u, 0xBEu, 0x07u },   /* ~1976 Hz */
    { 8u, CH_PULSE, 0xA2u, 0x40u, 0xCEu, 0x07u },   /* ~2637 Hz */
    { 0, 0, 0, 0, 0, 0 },
};

/* Your blow lands: double metallic crunch — "ga, ga". */
static const sfx_step_t FX_HIT[] = {
    { 5u, CH_NOISE, 0xA1u, 0x4Eu, 0, 0 },
    { 6u, CH_NOISE, 0xC1u, 0x5Du, 0, 0 },
    { 0, 0, 0, 0, 0, 0 },
};

/* You get hit: heavy boom, then the crack — "do, ka". */
static const sfx_step_t FX_HURT[] = {
    { 6u, CH_NOISE, 0xF2u, 0x71u, 0, 0 },
    { 8u, CH_NOISE, 0xA2u, 0x53u, 0, 0 },
    { 0, 0, 0, 0, 0, 0 },
};

/* Trap: three-note falling warble — "de, ro, re". */
static const sfx_step_t FX_TRAP[] = {
    { 6u, CH_PULSE, 0xA2u, 0x40u, 0x21u, 0x07u },   /* ~587 Hz */
    { 6u, CH_PULSE, 0xA2u, 0x40u, 0x05u, 0x07u },   /* ~523 Hz */
    { 8u, CH_PULSE, 0x92u, 0x40u, 0xD6u, 0x06u },   /* ~440 Hz */
    { 0, 0, 0, 0, 0, 0 },
};

/* Menu cursor / toggle: one clean high blip — "pi". */
static const sfx_step_t FX_MENU[] = {
    { 3u, CH_PULSE, 0x71u, 0x40u, 0xC1u, 0x07u },   /* ~2093 Hz */
    { 0, 0, 0, 0, 0, 0 },
};

/* Stairs: three heavy footfalls — "za, za, za". Spaced ~0.3 s apart (18
   frames each, ~0.9 s total) so the descent paces the ~1 s fade-out. */
static const sfx_step_t FX_STAIRS[] = {
    { 18u, CH_NOISE, 0x81u, 0x62u, 0, 0 },
    { 18u, CH_NOISE, 0x81u, 0x62u, 0, 0 },
    { 18u, CH_NOISE, 0x91u, 0x62u, 0, 0 },
    { 0, 0, 0, 0, 0, 0 },
};

/* Rest / search in place: one soft brush of noise — "sa". */
static const sfx_step_t FX_REST[] = {
    { 4u, CH_NOISE, 0x31u, 0x24u, 0, 0 },
    { 0, 0, 0, 0, 0, 0 },
};

/* Level up: quick rising major arpeggio, last note rings. */
static const sfx_step_t FX_LVLUP[] = {
    { 4u, CH_PULSE, 0x91u, 0x80u, 0x05u, 0x07u },   /* C5 ~523 Hz */
    { 4u, CH_PULSE, 0x91u, 0x80u, 0x39u, 0x07u },   /* E5 ~659 Hz */
    { 4u, CH_PULSE, 0x91u, 0x80u, 0x59u, 0x07u },   /* G5 ~784 Hz */
    { 12u, CH_PULSE, 0xA3u, 0x80u, 0x83u, 0x07u },  /* C6 ~1047 Hz */
    { 0, 0, 0, 0, 0, 0 },
};

static const sfx_step_t * const SEQ[SFX_COUNT] = {
    0, FX_STEP_A, FX_STEP_B, FX_MISS, FX_BUMP,
    FX_ITEM, FX_GOLD, FX_HIT, FX_HURT, FX_TRAP,
    FX_MENU, FX_STAIRS, FX_LVLUP, FX_REST,
};
static const uint8_t PRIO[SFX_COUNT] = {
    0, 1u, 1u, 2u, 2u, 3u, 3u, 4u, 5u, 6u,
    2u, 4u, 5u, 1u,
};

/* Shared between sfx_play (main, interrupts off) and the VBL step. */
static const sfx_step_t * volatile cur;   /* current step; NULL = idle */
static volatile uint8_t cur_t;  /* frames left in the current step */
static volatile uint8_t cur_prio;
static volatile uint8_t pend;   /* SFX_NONE = nothing queued */
volatile uint8_t g_sfx_last;

static void play_step(const sfx_step_t *s) {
    if (s->ch == CH_NOISE) {
        NR42_REG = s->env;
        NR43_REG = s->shp;
        NR44_REG = 0x80u;
    } else {
        NR10_REG = (s->ch == CH_SWEEP) ? 0x2Du : 0x00u;
        NR11_REG = s->shp;
        NR12_REG = s->env;
        NR13_REG = s->flo;
        NR14_REG = (uint8_t)(0x80u | s->fhi);
    }
}

static void start(uint8_t id) {
    cur = SEQ[id];
    cur_t = cur->dur;
    cur_prio = PRIO[id];
    play_step(cur);
}

static void sfx_isr(void) {
    bgm_tick();
    if (!cur) return;
    if (--cur_t) return;
    cur++;
    if (cur->dur) {
        cur_t = cur->dur;
        play_step(cur);
        return;
    }
    cur = 0;                     /* the last envelope decays on its own */
    if (pend) {
        uint8_t id = pend;
        pend = SFX_NONE;
        start(id);
    }
}

void sfx_play(uint8_t id) {
    if (!id || id >= SFX_COUNT) return;
    g_sfx_last = id;
    disable_interrupts();
    if (!cur) start(id);
    else if (PRIO[id] >= cur_prio) pend = id;
    enable_interrupts();
}

void sfx_init(void) {
    NR52_REG = 0x80u;            /* APU on (gates every other write) */
    NR50_REG = 0x77u;            /* full master volume, both terminals */
    NR51_REG = 0xFFu;            /* all channels to both speakers */
    disable_interrupts();
    add_VBL(sfx_isr);
    enable_interrupts();
}
