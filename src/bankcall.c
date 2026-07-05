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
    SWITCH_ROM(bank);
    fn();
    SWITCH_ROM(save);
}
