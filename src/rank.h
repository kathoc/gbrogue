#ifndef GBR_RANK_H
#define GBR_RANK_H

#include <stdint.h>

/*
 * Persistent high-score ranking (SRAM, survives permadeath). Top runs by
 * gold; each row keeps the depth reached, with the deepest/final pair so
 * a climb-back reads "B26->B3". Kept deliberately tiny — the 32KB HOME
 * budget has no room for the death-cause/play-count text.
 */
#define RANK_N 6u

typedef struct {
    uint16_t gold;
    uint8_t  deepest;   /* deepest floor reached */
    uint8_t  final;     /* floor at run end */
    uint8_t  amulet;    /* 1 if the Amulet was obtained (climb-back form) */
} rank_entry_t;

/* Fill out[0..RANK_N-1]; returns how many rows are real (deepest != 0). */
uint8_t rank_read(rank_entry_t *out);
/* Insert a finished run, sorted by gold, keeping only the top RANK_N. */
void    rank_insert(const rank_entry_t *e);

#endif
