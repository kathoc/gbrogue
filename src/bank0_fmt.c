#include "util.h"

/*
 * BANK0 (early-sorting filename, like bank0_rng.c). fmt_str / fmt_char build
 * item names for item_name(), which banked scroll logic (BANK2 identify)
 * reaches through class_name(); call_bank unmaps bank 1, so these string
 * builders must live in the fixed bank. fmt_u16 stays in util.c — it calls
 * __divuint/__moduint (bank 1) and is never reached from banked code
 * (numeric item names only appear on weapon/armor/gold, all HOME paths).
 */

char *fmt_str(char *dst, const char *s) {
    while (*s) *dst++ = *s++;
    return dst;
}

char *fmt_char(char *dst, char c) {
    *dst++ = c;
    return dst;
}
