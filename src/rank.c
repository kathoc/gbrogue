#include <gb/gb.h>
#include "rank.h"
/* Storage stubbed until the bank redesign frees room for the real
   SRAM-backed ranking. */
uint8_t rank_read(rank_entry_t *out) { (void)out; return 0; }
void rank_insert(const rank_entry_t *e) { (void)e; }
