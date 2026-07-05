#include <gb/gb.h>
#include "bankcall.h"

/*
 * Safe cross-bank call. Lives in BANK0 (fixed, always mapped) so it
 * survives the ROM-bank switch it performs. Saves the caller's bank,
 * maps the target, runs a self-contained entry function there, then
 * restores. Nesting is safe because each frame saves/restores its own
 * bank on the stack.
 */
void call_bank(uint8_t bank, bank_fn_t fn) {
    uint8_t save = CURRENT_BANK;
    /* Interrupts OFF across the whole switch+call: the VBL handler and
       everything it touches live in BANK0/BANK1, so if an interrupt
       fired while a data/logic bank were mapped it would execute garbage
       (this is what crashed the longer save_write copy). Banked entries
       are all short, self-contained, and never wait on input/VBL, so
       masking interrupts for their duration is safe. */
    __critical {
        SWITCH_ROM(bank);
        fn();
        SWITCH_ROM(save);
    }
}
