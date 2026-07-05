#include <gb/gb.h>
#include "bankcall.h"
#include "rank.h"

/*
 * BANK0 shims for banked subsystems. These stay pinned in the fixed bank
 * (early filename => early in _CODE) so game code can call them normally;
 * inside, they marshal arguments through RAM globals and hop into the
 * owning bank with call_bank(). Keep these tiny — every byte here is
 * BANK0 budget.
 */

/* --- ranking (BANK5) --- */

uint8_t rank_read(rank_entry_t *out) {
    uint8_t i;
    call_bank(5u, bank_rank_read);
    for (i = 0; i < RANK_N; i++) out[i] = g_rank_io[i];
    return g_rank_n;
}

void rank_insert(const rank_entry_t *e) {
    g_rank_new = *e;
    call_bank(5u, bank_rank_insert);
}
