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

/* Shared 8-way aim prompt on the live world: a blinking arrow cursor on
   the player, D-pad to turn it (hold two for a diagonal), A confirms and
   fires, B cancels. verb_sid labels the log line ("<verb> which way?"). */
uint8_t items_prompt_dir(int8_t *dx, int8_t *dy, uint8_t verb_sid);

/* --- BANK2 relocation ---
   The post-aim wand effect lives in items_zap_fx.c (#pragma bank 2),
   reached only through items_zap(), which marshals the aim into these
   WRAM globals and hops in via call_bank(2, bank_zap_effect). All of the
   effect's callees were relocated into bank 0 first (msgq, mkind/
   monster_roll_hp, render_flash_add) so the banked code can reach them. */
extern uint8_t g_zap_slot;
extern int8_t  g_zap_dx, g_zap_dy;
extern uint8_t g_zap_turns;
void bank_zap_effect(void);   /* BANK2 entry — run via call_bank only */

#endif
