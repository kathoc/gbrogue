#include "status.h"
#include "world.h"
#include "map.h"
#include "render.h"
#include "util.h"
#include "lang.h"
#include "text4.h"

uint8_t g_status_hint;

/* Active-effect tags shown right of the gold count (order matches the
   SID_ST_* block). JA tags are single kanji, EN four letters. */
static const uint8_t *const EFF_TIMERS[9] = {
    &g_haste_t, &g_conf_t, &g_blind_t, &g_halluc_t, &g_levit_t,
    &g_sleep_t, &g_held_t, &g_seeinv_t, &g_mondet_t,
};

/* Row width in half-width columns, mirroring the renderer: kana/kanji
   take two columns and get a pad space first when they land on an odd
   column (text4 composes half-width chars in pairs). */
static uint8_t cols(const char *s) {
    uint8_t n = 0;
    while (*s) {
        if (T4_IS_FULL((uint8_t)*s)) {
            if (n & 1u) n++;
            n = (uint8_t)(n + 2u);
        } else {
            n++;
        }
        s++;
    }
    return n;
}

/* Half-width text gives the status row 40 columns:
   `B3 HP18/24 St16 AC4 G120 Hu` all fits at once. The floor is shown
   as B<n> ("basement <n>"), never L<n> — L reads as character level. */
void status_update(void) {
    char buf[48];
    char *p = buf;
    p = fmt_char(p, 'B');
    p = fmt_u16(p, g_depth);
    p = fmt_str(p, " LV");                        /* level, just left of HP */
    p = fmt_u16(p, g_level);
    p = fmt_str(p, " ");
    g_hp_col0 = (uint8_t)((p - buf) / 2u);       /* "HP..." starts here */
    p = fmt_str(p, "HP");
    p = fmt_u16(p, g_hp);
    p = fmt_char(p, '/');
    p = fmt_u16(p, g_maxhp);
    g_hp_col1 = (uint8_t)((p - buf - 1u) / 2u);  /* last HP tile column */
    p = fmt_str(p, " St");
    p = fmt_u16(p, g_str);
    p = fmt_str(p, " AC");
    p = fmt_u16(p, g_ac);
    /* gold is shown in the inventory now, not on the status row */
    {
        /* active effects, at most three: " 速/混/盲" or " hast/conf" */
        uint8_t i, n = 0;
        for (i = 0; i < 9u && n < 3u; i++) {
            if (!*EFF_TIMERS[i]) continue;
            p = fmt_char(p, n ? '/' : ' ');
            p = fmt_str(p, lang_str((uint8_t)(SID_ST_HASTE + i)));
            n++;
        }
    }
    if (g_hunger) {
        /* no nested ?: with string literals — SDCC/sm83 has produced
           garbage pointers for that shape (docs/status.md 教訓) */
        const char *tag = "Hu";
        if (g_hunger == 2u) tag = "Wk";
        else if (g_hunger >= 3u) tag = "Ft";
        p = fmt_char(p, ' ');
        p = fmt_str(p, tag);
    }
    if (g_status_hint || map_terrain(g_px, g_py) == TI_STAIRS_DOWN) {
        /* bottom-right corner: an override (wand aiming etc), else
           what A does on this staircase */
        const char *hint;
        if (g_status_hint) hint = lang_str(g_status_hint);
        else hint = lang_str(g_has_amulet ? SID_HINT_BOTH
                                          : SID_HINT_DESCEND);
        uint8_t used, want;
        *p = 0;
        used = cols(buf);
        want = (uint8_t)(40u - cols(hint));
        /* bytes <= columns, so buf[48] can't overflow padding to 40 */
        while (used < want) {
            p = fmt_char(p, ' ');
            used++;
        }
        p = fmt_str(p, hint);
    }
    *p = 0;
    render_status(buf);
}
