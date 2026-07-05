#ifndef FARCOPY_H
#define FARCOPY_H

#include <stdint.h>

/*
 * Copy bytes out of another ROM bank via a tiny routine executed from
 * WRAM. Our home code spans past 0x4000, so switching banks while
 * running from ROM would unmap the running code — the WRAM stub is
 * immune. Interrupts are disabled for the (tiny) copy.
 */
void farcopy_init(void);
void far_copy(uint8_t bank, const void *src, void *dst, uint8_t len);

#endif
