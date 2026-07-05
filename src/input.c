#include <gb/gb.h>
#include "input.h"
#include "world.h"

static uint8_t prev_keys;
static uint8_t pending_edges;   /* edges latched by input_tick() */
uint8_t g_input_repeat;

/* D-pad auto-repeat: after an edge, the next synthesised press fires
   REPEAT_DELAY frames later, then every REPEAT_RATE frames while held
   (rate follows the player's 3-step setting). A/B/SELECT/START stay
   edge-only. */
#define DIR_MASK     (J_UP | J_DOWN | J_LEFT | J_RIGHT)
#define REPEAT_DELAY 8u
static const uint8_t RATE_TBL[3] = { 8u, 4u, 2u };
#define REPEAT_RATE  (RATE_TBL[g_repeat_speed])
static uint8_t repeat_timer;

void input_init(void) {
    prev_keys = joypad();
    pending_edges = 0;
    repeat_timer = REPEAT_DELAY;
}

void input_swallow_edges(void) {
    prev_keys = 0xFFu;
    pending_edges = 0;
    repeat_timer = REPEAT_DELAY;
}

void input_tick(void) {
    uint8_t keys = joypad();
    pending_edges |= (uint8_t)(keys & ~prev_keys);
    prev_keys = keys;
}

uint8_t input_held(void) {
    return joypad();
}

uint8_t input_pressed(void) {
    uint8_t out, dir_held;

    input_tick();
    out = pending_edges;
    pending_edges = 0;
    dir_held = (uint8_t)(prev_keys & DIR_MASK);
    g_input_repeat = 0;

    if (out & DIR_MASK) {
        repeat_timer = REPEAT_DELAY;
    } else if (dir_held) {
        if (repeat_timer == 0) {
            out |= dir_held;
            g_input_repeat = 1;
            repeat_timer = REPEAT_RATE;
        } else {
            repeat_timer--;
        }
    } else {
        repeat_timer = REPEAT_DELAY;
    }

    return out;
}
