#include "util.h"

char *fmt_str(char *dst, const char *s) {
    while (*s) *dst++ = *s++;
    return dst;
}

char *fmt_char(char *dst, char c) {
    *dst++ = c;
    return dst;
}

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
