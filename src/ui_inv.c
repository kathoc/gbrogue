#include <gb/gb.h>
#include "ui_inv.h"
#include "inventory.h"
#include "items.h"
#include "items_use.h"
#include "items_zap.h"
#include "identify.h"
#include "world.h"
#include "render.h"
#include "input.h"
#include "msg.h"
#include "lang.h"
#include "util.h"
#include "worldview.h"
#include "status.h"
#include "sfx.h"
#include "text4.h"

/*
 * Pack screen. Lives in its own translation unit — render-heavy code
 * inside game.c breaks the SDCC boot path (docs/architecture.md).
 *
 * Layout: title / a 12-row scrolling item list (the pack is always
 * gap-free, see inv_compact) / a two-row description panel. Known
 * magic items describe their effect; unknown ones just admit it.
 */
#define LIST_TOP  2u
#define LIST_ROWS 12u
#define ROW_DESC1 15u
#define ROW_DESC2 16u

static uint8_t s_top;                  /* first visible item index */

static void draw_line(uint8_t idx, uint8_t cursor) {
    char buf[32];
    char *p = buf;
    const item_t *it = &g_pack[idx];

    p = fmt_str(p, cursor == idx ? "> " : "  ");
    if (it->kind != ITEM_NONE) {
        p = item_name(p, it);
        if (it->qty > 1u) {
            p = fmt_str(p, " x");
            p = fmt_u16(p, it->qty);
        }
        if (it->flags & IF_WORN) p = fmt_str(p, " *");
    }
    *p = 0;
    render_row((uint8_t)(LIST_TOP + idx - s_top), buf);
}

/* Bottom panel: effect line for the highlighted item. */
static void draw_desc(uint8_t idx, uint8_t n) {
    const item_t *it = &g_pack[idx];
    uint8_t sid = 0, hint = 0;

    if (idx >= n) {
        render_row(ROW_DESC1, "");
        render_row(ROW_DESC2, "");
        return;
    }
    if (it->kind >= IK_POTION && it->kind <= IK_RING) {
        /* magic classes: IK_POTION..IK_RING map 1:1 onto IDC_* and the
           contiguous SID_D_UNK_* / per-class description blocks */
        static const uint8_t BASE[4] = {
            SID_D_PTN0, SID_D_SCR0, SID_D_WND0, SID_D_RNG0,
        };
        uint8_t idc = (uint8_t)(it->kind - IK_POTION);
        if (identify_known(idc, it->sub))
            sid = (uint8_t)(BASE[idc] + it->sub);
        else {
            sid = (uint8_t)(SID_D_UNK_PTN + idc);
            hint = 1;
        }
    } else if (it->kind == IK_WEAPON) sid = SID_D_WEAPON;
    else if (it->kind == IK_ARMOR)    sid = SID_D_ARMOR;
    else if (it->kind == IK_AMULET)   sid = SID_D_AMULET;
    else                              sid = SID_D_FOOD;
    render_row(ROW_DESC1, lang_str(sid));
    render_row(ROW_DESC2, hint ? lang_str(SID_D_TRYIT) : "");
}

static void draw_list(uint8_t cursor) {
    char buf[32];
    char *p;
    uint8_t r, n = inv_count();

    render_clear_all();
    p = fmt_str(buf, lang_str(SID_PACK_TITLE));
    p = fmt_u16(p, g_gold);
    *p = 0;
    render_text(0, 0, buf);
    for (r = 0; r < LIST_ROWS; r++) {
        uint8_t idx = (uint8_t)(s_top + r);
        if (idx < n) draw_line(idx, cursor);
        else render_row((uint8_t)(LIST_TOP + r), "");
    }
    /* more-above / more-below markers */
    render_text(19, 1, s_top ? "^" : " ");
    render_text(19, (uint8_t)(LIST_TOP + LIST_ROWS),
                (uint8_t)(s_top + LIST_ROWS) < n ? "v" : " ");
    draw_desc(cursor, n);
    render_status(lang_str(SID_PACK_HINT));
    render_present();
}

/* Row-17 hint for the action submenu: the primary verb depends on the
   item kind (and whether it is currently equipped). */
static uint8_t action_hint_sid(const item_t *it) {
    switch (it->kind) {
    case IK_WEAPON:
        if (it->sub == WS_ARROW)     return SID_ACT_FIRE;    /* はなつ */
        if (WS_THROWABLE(it->sub))   return SID_ACT_THROW;   /* なげる */
        return (it->flags & IF_WORN) ? SID_ACT_REMOVE : SID_ACT_WEAPON;
    case IK_ARMOR:  return (it->flags & IF_WORN) ? SID_ACT_REMOVE
                                                 : SID_ACT_ARMOR;
    case IK_RING:   return (it->flags & IF_WORN) ? SID_ACT_REMOVE
                                                 : SID_ACT_RING;
    case IK_POTION: return SID_ACT_POTION;
    case IK_SCROLL: return SID_ACT_SCROLL;
    case IK_WAND:   return SID_ACT_WAND;
    case IK_AMULET: return SID_ACT_AMULET;
    default:        return SID_ACT_FOOD;
    }
}

/* Replace the description panel with a 3-option vertical menu (primary
   verb / drop / cancel), stacked from row 14: press A on an item and the
   choices appear right where you were reading its blurb. Up/Down move, A
   picks, B backs to the list, SELECT closes the pack — the hint row says
   so. Row 0's label is the kind-specific verb; 1/2 are drop/cancel. */
#define ROW_ACT0 14u
static void draw_action_rows(uint8_t cursor, uint8_t sel) {
    static const uint8_t OPT[3] = { 0u, SID_ACT_DROP, SID_ACT_CANCEL };
    uint8_t r;
    for (r = 0; r < 3u; r++) {
        char buf[24];
        char *p = fmt_str(buf, r == sel ? "> " : "  ");
        p = fmt_str(p, lang_str(r ? OPT[r]
                                  : action_hint_sid(&g_pack[cursor])));
        *p = 0;
        render_row((uint8_t)(ROW_ACT0 + r), buf);
    }
    render_status(lang_str(SID_ACT_HINT));
}

static void draw_action(uint8_t cursor, uint8_t sel) {
    draw_action_rows(cursor, sel);
    if (g_t4_flushed) {
        /* Composing the submenu wrapped the composed-tile pool: the list
           rows behind it now point at recycled tiles. Rebuild the list
           from a fresh pool (draw_list resets it), then re-lay the submenu
           on top — before anything reaches VRAM. */
        g_t4_flushed = 0;
        draw_list(cursor);
        draw_action_rows(cursor, sel);
    }
    render_present();
}

/* Keep the cursor on the glass; returns 1 if the window scrolled. */
static uint8_t scroll_to(uint8_t cursor) {
    if (cursor < s_top) {
        s_top = cursor;
        return 1;
    }
    if (cursor >= (uint8_t)(s_top + LIST_ROWS)) {
        s_top = (uint8_t)(cursor - LIST_ROWS + 1u);
        return 1;
    }
    return 0;
}

static uint8_t drop_item(uint8_t slot) {
    item_t *it = &g_pack[slot];
    item_t copy;
    if (it->kind == ITEM_NONE) return 0;
    if (it->flags & IF_WORN) {
        if (it->flags & IF_CURSED) {
            it->flags |= IF_KNOWN_CURSED;
            msg_post_id(SID_CURSED);
            return 1;
        }
        msg_post_id(SID_REMOVE_FIRST);
        return 0;
    }
    if (item_floor_at(g_px, g_py)) {
        msg_post_id(SID_SOMETHING_HERE);
        return 0;
    }
    copy = *it;
    {
        item_t *f = item_place(copy.kind, copy.sub, g_px, g_py);
        if (!f) {
            msg_post_id(SID_NO_ROOM);
            return 0;
        }
        f->qty = copy.qty;
        f->ench = copy.ench;
        f->flags = (uint8_t)(copy.flags & ~IF_WORN);
    }
    it->kind = ITEM_NONE;
    it->flags = 0;
    inv_compact();
    msg_post_id(SID_DROPPED);
    return 1;
}

/* mode: 0 = browsing the list, 1 = the action submenu for one item. */
#define M_LIST   0u
#define M_ACTION 1u

uint8_t ui_inv_show(void) {
    uint8_t cursor = 0, turns = 0;
    uint8_t n, mode = M_LIST, act_sel = 0;

    sfx_play(SFX_MENU);
    render_set_world(0);
    input_swallow_edges();
    s_top = 0;
    draw_list(cursor);

    for (;;) {
        uint8_t keys;
        wait_vbl_done();
        keys = input_pressed();
        n = inv_count();
        if (cursor >= n && n) cursor = (uint8_t)(n - 1u);

        if (mode == M_LIST) {
            /* SELECT and B both just close — no more one-tap drops. */
            if (keys & (J_B | J_SELECT)) break;
            if (n && (keys & (J_UP | J_DOWN))) {
                uint8_t old = cursor;
                sfx_play(SFX_MENU);
                /* wrap by hand — avoids an 8-bit %n division helper */
                if (keys & J_UP)
                    cursor = cursor ? (uint8_t)(cursor - 1u) : (uint8_t)(n - 1u);
                else
                    cursor = (uint8_t)(cursor + 1u) < n ? (uint8_t)(cursor + 1u) : 0u;
                /* Repaint only the two changed rows (+blurb) unless the
                   window actually scrolled — a full render_clear_all on
                   every step made the list flash. */
                if (scroll_to(cursor)) {
                    draw_list(cursor);
                } else {
                    draw_line(old, cursor);
                    draw_line(cursor, cursor);
                    draw_desc(cursor, n);
                    /* Partial redraws never reset the composed-tile pool,
                       so distinct item/desc glyph pairs accumulate as you
                       scroll. If drawing these rows just wrapped the pool,
                       the rows we did NOT touch now point at recycled
                       tiles (and set_cell won't re-flush them — it dedups
                       by tile index, which is unchanged). Repaint the
                       whole list from a fresh pool BEFORE presenting, so a
                       corrupted frame is never shown. */
                    if (g_t4_flushed) {
                        g_t4_flushed = 0;
                        draw_list(cursor);
                    } else {
                        render_present();
                    }
                }
            }
            if (n && (keys & J_A)) {
                /* open the 3-option action menu for the highlighted item */
                sfx_play(SFX_MENU);
                mode = M_ACTION;
                act_sel = 0;
                draw_action(cursor, act_sel);
            }
        } else {                       /* M_ACTION */
            if (keys & J_SELECT) break;    /* close the pack entirely */
            if (keys & (J_UP | J_DOWN)) {
                sfx_play(SFX_MENU);
                /* wrap 0..2 by hand — avoids a %3 division helper */
                if (keys & J_UP) act_sel = act_sel ? (uint8_t)(act_sel - 1u) : 2u;
                else             act_sel = (act_sel == 2u) ? 0u
                                                           : (uint8_t)(act_sel + 1u);
                draw_action(cursor, act_sel);
            } else if (keys & (J_A | J_B)) {
                /* A confirms the highlighted option; B is a shortcut for
                   the cancel option. All three funnel to the same tail. */
                uint8_t act = (keys & J_A) ? act_sel : 2u;
                sfx_play(SFX_MENU);
                if (act == 0u) {           /* primary action (use/equip/…) */
                    g_zap_prompted = 0;
                    turns = items_use(cursor);
                    msgq_flush();  /* render deferred item-use messages */
                    /* aiming a wand traded the pack for the live world —
                       stay there even if the aim was cancelled */
                    if (turns || g_zap_prompted) break;
                } else if (act == 1u) {    /* drop (may refuse: worn/no room) */
                    turns = drop_item(cursor);
                    if (turns) break;
                }
                mode = M_LIST;             /* cancel / refused / done */
                n = inv_count();
                if (cursor >= n && n) cursor = (uint8_t)(n - 1u);
                scroll_to(cursor);
                draw_list(cursor);
            }
        }
        if (g_t4_flushed) {
            /* composed-tile pool wrapped mid-modal: repaint everything
               so no row references a recycled slot */
            g_t4_flushed = 0;
            draw_list(cursor);
            if (mode == M_ACTION) draw_action(cursor, act_sel);
            else render_present();
        }
    }

    /* restore the world view (keep the latest message visible) */
    view_world_enter();
    status_update();
    msg_refresh();
    render_present();
    input_swallow_edges();
    return turns;
}
