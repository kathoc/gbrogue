#ifndef GBR_BANKCALL_H
#define GBR_BANKCALL_H
#include <stdint.h>
typedef void (*bank_fn_t)(void);
/* Run self-contained entry fn (which resides in `bank`) with a safe
   save/switch/restore. call_bank itself is pinned in BANK0. */
void call_bank(uint8_t bank, bank_fn_t fn);
#endif
