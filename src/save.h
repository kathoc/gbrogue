#ifndef SAVE_H
#define SAVE_H

#include <stdint.h>

/* Suspend save in battery-backed SRAM (MBC1+RAM+BATTERY, 8KB).
 *
 * The SRAM serialisation lives in BANK5 (bank_save_*, self-contained:
 * SRAM byte loops + the CHUNKS table, no library/cross-bank calls).
 * These BANK0 shims (bank_api.c) are the public API; they handle the
 * rng and view calls (which the banked code must not make) and hop in
 * via call_bank, marshalling through the globals below. */
uint8_t save_exists(void);
void    save_write(void);
uint8_t save_load(void);        /* 1 = restored */
void    save_invalidate(void);  /* death / victory wipes the save */

/* Marshalling globals (WRAM). */
extern uint16_t g_save_rng;     /* rng state in/out across the bank hop */
extern uint8_t  g_save_ok;      /* bank_save_exists result */

/* Static-map dirty flag (WRAM). Set whenever g_map changes (dig / trap
   reveal / new floor); cleared after the STATIC region is (re)written.
   1 = the next save must re-flush the 896-byte g_map block, else the save
   reuses the cached STATIC bytes + checksum already in SRAM. */
extern uint8_t  g_save_static_dirty;

/* Mark the static map dirty. Pure WRAM flag set (bank_api.c, BANK0) — no
   bank hop, callable from anywhere (map.c accessors). */
void save_mark_map_dirty(void);

/* BANK5 entry functions (run via call_bank; self-contained). */
void bank_save_write(void);
void bank_save_load(void);
void bank_save_exists(void);
void bank_save_invalidate(void);

#endif
