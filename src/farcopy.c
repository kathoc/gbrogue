#include <gb/gb.h>
#include "farcopy.h"

/*
 * WRAM trampoline (hand-assembled sm83):
 *
 *   di
 *   ld a,(g_fc_bank) ; ld (0x2000),a   ; map the data bank
 *   ld a,(src.lo) ; ld l,a ; ld a,(src.hi) ; ld h,a
 *   ld a,(dst.lo) ; ld e,a ; ld a,(dst.hi) ; ld d,a
 *   ld a,(len)    ; ld b,a
 * loop:
 *   ld a,(hl+) ; ld (de),a ; inc de ; dec b ; jr nz,loop
 *   ld a,(_current_bank) ; ld (0x2000),a   ; restore the CALLER's bank
 *   ei ; ret
 *
 * The absolute addresses of the parameter globals are patched into the
 * byte stream at init.
 */

static uint8_t  g_fc_bank;
static const uint8_t *g_fc_src;
static uint8_t *g_fc_dst;
static uint8_t  g_fc_len;

static uint8_t code_buf[41];

static const uint8_t CODE_TMPL[41] = {
    0xF3,                   /*  0: di            */
    0xFA, 0x00, 0x00,       /*  1: ld a,(bank)   */
    0xEA, 0x00, 0x20,       /*  4: ld (2000h),a  */
    0xFA, 0x00, 0x00,       /*  7: ld a,(src.lo) */
    0x6F,                   /* 10: ld l,a        */
    0xFA, 0x00, 0x00,       /* 11: ld a,(src.hi) */
    0x67,                   /* 14: ld h,a        */
    0xFA, 0x00, 0x00,       /* 15: ld a,(dst.lo) */
    0x5F,                   /* 18: ld e,a        */
    0xFA, 0x00, 0x00,       /* 19: ld a,(dst.hi) */
    0x57,                   /* 22: ld d,a        */
    0xFA, 0x00, 0x00,       /* 23: ld a,(len)    */
    0x47,                   /* 26: ld b,a        */
    0x2A,                   /* 27: ld a,(hl+)    */
    0x12,                   /* 28: ld (de),a     */
    0x13,                   /* 29: inc de        */
    0x05,                   /* 30: dec b         */
    0x20, 0xFA,             /* 31: jr nz,-6      */
    0xFA, 0x00, 0x00,       /* 33: ld a,(_current_bank) */
    0xEA, 0x00, 0x20,       /* 36: ld (2000h),a  */
    0xFB,                   /* 39: ei            */
    0xC9,                   /* 40: ret           */
};

static void patch(uint8_t at, uint16_t addr) {
    code_buf[at] = (uint8_t)addr;
    code_buf[at + 1u] = (uint8_t)(addr >> 8);
}

void farcopy_init(void) {
    uint8_t i;
    for (i = 0; i < sizeof(CODE_TMPL); i++) code_buf[i] = CODE_TMPL[i];
    patch(2, (uint16_t)&g_fc_bank);
    patch(8, (uint16_t)&g_fc_src);
    patch(12, (uint16_t)((uint16_t)&g_fc_src + 1u));
    patch(16, (uint16_t)&g_fc_dst);
    patch(20, (uint16_t)((uint16_t)&g_fc_dst + 1u));
    patch(24, (uint16_t)&g_fc_len);
    patch(34, (uint16_t)&_current_bank);
}

void far_copy(uint8_t bank, const void *src, void *dst, uint8_t len) {
    g_fc_bank = bank;
    g_fc_src = (const uint8_t *)src;
    g_fc_dst = (uint8_t *)dst;
    g_fc_len = len;
    ((void (*)(void))code_buf)();
}
