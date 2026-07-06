#include <gb/gb.h>
#include "ui_rank.h"
#include "rank.h"
#include "render.h"
#include "input.h"
#include "lang.h"
#include "util.h"

/*
 * Top runs by gold, TWO rows each plus a blank spacer:
 *   line 1: "1st G:836 B6 12:34:56.7 DEADBEEF"
 *           rank / gold / deepest floor / play time / seed
 *   line 2: "  Slain by the dragon"   (cause; "escaped!" for a won run)
 * Entered from the title (which fades to black first); we fade IN here and
 * OUT on the way back so the tileset swap never tears.
 */

static const char *const ORD[RANK_N] = { "st", "nd", "rd", "th", "th", "th" };

static char *append_cause(char *o, uint8_t sid, uint8_t mon) {
    const char *p = lang_str(sid ? sid : SID_RANK_ESCAPED);
    const char *arg = (sid == SID_DEATH_MON) ? lang_name(LT_MNAME, mon) : "";
    while (*p) {
        if ((uint8_t)*p == 0x01u) {
            while (*arg) *o++ = *arg++;
            p++;
        } else {
            *o++ = *p++;
        }
    }
    return o;
}

static char *fmt2(char *p, uint8_t v) {          /* 2-digit, zero-padded */
    *p++ = (char)('0' + v / 10u);
    *p++ = (char)('0' + v % 10u);
    return p;
}

/* VBLANK frames (~60/s) -> "HH:MM:SS.d", clamped to 99:99:99.9. */
static void fmt_time(char *dst, uint32_t frames) {
    uint32_t tenths = frames / 6u;               /* 6 frames per 0.1s */
    uint8_t  d = (uint8_t)(tenths % 10u);
    uint32_t secs = tenths / 10u;
    uint8_t  s = (uint8_t)(secs % 60u);
    uint32_t mins = secs / 60u;
    uint8_t  m = (uint8_t)(mins % 60u);
    uint16_t h = (uint16_t)(mins / 60u);
    char *p = dst;
    if (h > 99u) { h = 99u; m = 99u; s = 99u; d = 9u; }
    p = fmt2(p, (uint8_t)h); *p++ = ':';
    p = fmt2(p, m); *p++ = ':';
    p = fmt2(p, s); *p++ = '.';
    *p++ = (char)('0' + d);
    *p = 0;
}

void ui_rank_show(void) {
    rank_entry_t ent[RANK_N];
    uint8_t n = rank_read(ent);
    uint8_t i;
    char buf[64];

    render_set_world(0);
    render_clear_all();
    if (!n)
        render_row(0, lang_str(SID_RANK_EMPTY));
    for (i = 0; i < n; i++) {
        char *p = buf;
        char tb[12];
        uint8_t d;
        /* line 1: rank / gold / floor / play time / seed */
        *p++ = (char)('1' + i);
        p = fmt_str(p, ORD[i]);
        p = fmt_str(p, " G:");
        p = fmt_u16(p, ent[i].gold);
        p = fmt_str(p, " B");
        p = fmt_u16(p, ent[i].deepest);
        *p++ = ' ';
        fmt_time(tb, ent[i].play_time);
        p = fmt_str(p, tb);
        *p++ = ' ';
        for (d = 0; d < 8u; d++) {
            uint8_t nib = (uint8_t)((ent[i].seed >> ((7u - d) * 4u)) & 0xFu);
            *p++ = (char)(nib < 10u ? '0' + nib : 'A' + (nib - 10u));
        }
        *p = 0;
        render_row((uint8_t)(3u * i), buf);
        /* line 2: cause of death, indented (blank line follows) */
        {
            char *q = fmt_str(buf, "  ");
            q = append_cause(q, ent[i].cause, ent[i].mon);
            *q = 0;
        }
        render_row((uint8_t)(3u * i + 1u), buf);
    }
    render_status(lang_str(SID_HINT_CANCEL));
    render_present();
    render_fade_in(FADE_IN_FRAMES);

    input_swallow_edges();
    for (;;) {
        wait_vbl_done();
        if (input_pressed() & (J_A | J_B | J_START)) break;
    }
    input_swallow_edges();
    render_fade_out(FADE_OUT_FRAMES);
}
