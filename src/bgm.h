#ifndef GBR_BGM_H
#define GBR_BGM_H

#include <stdint.h>

/*
 * Ambient BGM: a 15-note whole-note melody at BPM 60 on pulse 2
 * (12.5% duty, soft envelope attack, slight vibrato), echoed a
 * quarter note later on the wave channel at half volume. Deeper
 * floors play slower and lower. SFX own channels 1 and 4, so the
 * two never fight.
 */
void bgm_start(void);
/* Advance one frame; called from sfx.c's VBL handler (one handler
   total — a second add_VBL handler breaks the GBDK ISR chain). */
void bgm_tick(void);
void bgm_stop(void);
void bgm_set_depth(uint8_t depth);

/* Test hooks. */
extern volatile uint8_t  g_bgm_idx;
extern volatile uint16_t g_bgm_len;

#endif
