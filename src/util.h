#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>

/* Tiny string builders — all return the advanced destination pointer,
   none write a terminating NUL (caller ends with *p = 0). */
char *fmt_str(char *dst, const char *s);
char *fmt_u16(char *dst, uint16_t v);
char *fmt_char(char *dst, char c);

#endif
