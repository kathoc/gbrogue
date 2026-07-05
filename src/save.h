#ifndef SAVE_H
#define SAVE_H

#include <stdint.h>

/* Suspend save in battery-backed SRAM (MBC1+RAM+BATTERY, 8KB).
   Single-use: loading consumes the save so it cannot be scummed. */
uint8_t save_exists(void);
void    save_write(void);
uint8_t save_load(void);        /* 1 = restored (and invalidated) */
void    save_invalidate(void);  /* death / victory wipes the save */


#endif
