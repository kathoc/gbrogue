#ifndef INPUT_H
#define INPUT_H

#include <stdint.h>

void input_init(void);
/* Treat every button as already-held so stale edges don't fire on the
   next poll. Call after screen transitions. */
void input_swallow_edges(void);
/* Latch edges without consuming them. Call once per frame from busy
   loops (movement animation) so short taps are never lost. */
void input_tick(void);
/* Latched edges since the last call, with D-pad auto-repeat. */
uint8_t input_pressed(void);
/* Raw held state (for A+dir / B+dir combos). */
uint8_t input_held(void);
/* 1 when the directions in the last input_pressed() result came from
   auto-repeat (button held) rather than a fresh tap — repeated steps
   play their animation at double speed. */
extern uint8_t g_input_repeat;

#endif
