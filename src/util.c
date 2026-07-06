#include "util.h"

/* fmt_str / fmt_char moved to bank0_fmt.c (fixed bank) so banked scroll
   naming can reach them. fmt_u16 stays here: it emits __divuint/__moduint
   (bank 1) and no banked path builds a numeric name. */

char *fmt_u16(char *dst, uint16_t v) {
    char tmp[5];
    uint8_t n = 0;
    if (v == 0) {
        *dst++ = '0';
        return dst;
    }
    while (v) {
        tmp[n++] = (char)('0' + (uint8_t)(v % 10u));
        v /= 10u;
    }
    while (n) *dst++ = tmp[--n];
    return dst;
}
