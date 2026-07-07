#include <gb/gb.h>
#include "bankcall.h"
#include "rank.h"
#include "save.h"
#include "rng.h"
#include "worldview.h"

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

void games_played_inc(void) {
    call_bank(5u, bank_games_inc);
}

void games_played_load(void) {
    call_bank(5u, bank_games_load);
}

/* --- suspend save (BANK5) --- */

uint8_t save_exists(void) {
    call_bank(5u, bank_save_exists);
    return g_save_ok;
}

void save_write(void) {
    g_save_rng = rng_state();          /* rng lives in BANK1 — read it here */
    call_bank(5u, bank_save_write);
}

uint8_t save_load(void) {
    call_bank(5u, bank_save_exists);
    if (!g_save_ok) return 0;
    call_bank(5u, bank_save_load);
    rng_seed(g_save_rng);              /* reseed + repaint from BANK0 */
    view_player_moved();
    return 1;
}

void save_invalidate(void) {
    call_bank(5u, bank_save_invalidate);
}

/* Static-map dirty flag setter. g_save_static_dirty is a WRAM global, so
   this is a plain store — no bank hop needed (unlike the bank_save_* calls
   above, which run banked SRAM code). Kept in BANK0 so map.c can call it
   directly from any bank. */
void save_mark_map_dirty(void) {
    g_save_static_dirty = 1u;
}
