#ifndef ITEMS_ZAP_H
#define ITEMS_ZAP_H

#include <stdint.h>

/* Zap the wand in pack slot: prompts for a direction (D-pad), fires a
   ray, applies the effect to the first monster hit. Returns turns. */
uint8_t items_zap(uint8_t slot);

/* Set whenever items_prompt_dir() ran: the pack modal was replaced by
   the live world for aiming, so the pack UI must close even if the
   prompt was cancelled (no turn spent). */
extern uint8_t g_zap_prompted;

/* Shared modal direction prompt (B cancels). 8-way: hold two D-pad
   directions together for a diagonal. */
uint8_t items_prompt_dir(int8_t *dx, int8_t *dy);

#endif
