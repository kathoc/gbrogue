#ifndef GBR_RANK_H
#define GBR_RANK_H

#include <stdint.h>

/*
 * Persistent high-score ranking (SRAM, survives permadeath). Top runs by
 * gold; each row keeps the depth reached, with the deepest/final pair so
 * a climb-back reads "B26->B3".
 *
 * Bank layout: the SRAM storage lives in BANK5 (bank_rank_*). BANK0 calls
 * it only through the rank_read/rank_insert shims (bank_api.c), which
 * marshal arguments through the RAM globals below — the BANK5 code itself
 * touches nothing outside its bank.
 */
#define RANK_N 6u

typedef struct {
    uint16_t gold;
    uint8_t  deepest;   /* deepest floor reached */
    uint8_t  final;     /* floor at run end */
    uint8_t  amulet;    /* 1 if the Amulet was obtained (climb-back form) */
    uint8_t  cause;     /* SID_DEATH_* of the fatal blow, 0 = survived/won */
    uint8_t  mon;       /* monster kind when cause == SID_DEATH_MON */
} rank_entry_t;

/* BANK0 shims (bank_api.c) — the only entry points the game calls. */
uint8_t rank_read(rank_entry_t *out);       /* fills out[]; returns count */
void    rank_insert(const rank_entry_t *e); /* sorted by gold, top RANK_N */

/* Marshalling globals (WRAM, reachable from any bank). */
extern rank_entry_t g_rank_io[RANK_N];      /* bank_rank_read fills this */
extern rank_entry_t g_rank_new;             /* bank_rank_insert reads this */
extern uint8_t      g_rank_n;               /* bank_rank_read stores count */

/* BANK5 entry functions (run via call_bank; self-contained). */
void bank_rank_read(void);
void bank_rank_insert(void);

#endif
