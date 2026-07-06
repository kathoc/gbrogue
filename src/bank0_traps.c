#include "traps.h"

/*
 * BANK0 (early-sorting filename, like bank0_rng.c). traps_clear/traps_add
 * are the only bank-1 leaves banked mapgen (BANK2) reaches; call_bank unmaps
 * bank 1, so they must live in the fixed bank. Both are pure RAM appends to
 * g_traps — the trap-trigger logic (which renders/plays SFX) stays in
 * traps.c in HOME.
 */

void traps_clear(void) {
    g_trap_count = 0;
}

void traps_add(uint8_t x, uint8_t y, uint8_t kind) {
    if (g_trap_count >= MAX_TRAPS) return;
    g_traps[g_trap_count].x = x;
    g_traps[g_trap_count].y = y;
    g_traps[g_trap_count].kind = kind;
    g_trap_count++;
}
