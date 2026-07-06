#include <gb/gb.h>
#include "game.h"
#include "render.h"
#include "input.h"
#include "tiles.h"
#include "world.h"
#include "map.h"
#include "mapgen.h"
#include "worldview.h"
#include "status.h"
#include "rng.h"
#include "actor.h"
#include "combat.h"
#include "msg.h"
#include "lang.h"
#include "ui_dead.h"
#include "ui_inv.h"
#include "ui_popup.h"
#include "items.h"
#include "inventory.h"
#include "identify.h"
#include "traps.h"
#include "effects.h"
#include "save.h"
#include "ui_menu.h"
#include "ui_map.h"
#include "ui_title.h"
#include "sfx.h"
#include "bgm.h"
#include "rank.h"
#include "bankcall.h"

/*
 * State machine + turn loop only. UI screens live in their own
 * translation units (SDCC layout constraint — see docs/architecture.md).
 */

/* Seeded from the DIV register at the START press: real randomness on
   hardware, still deterministic under the frame-exact PyBoy harness. */

/* Player regen: +1 HP every 8 turns below max. */
#define REGEN_PERIOD 8u

static void new_level(void);
static void level_transition(int8_t ddepth);
static void after_player_turn(void);

/* Set by level changes: the turn that took the stairs (or fell) must
   not hand the new floor's monsters a free move before the player. */
static uint8_t fresh_floor;

/*
 * Smooth stepping: positions are snapshotted before each action; after
 * the turn resolves, camera scroll and actor sprites glide from the old
 * spots to the new ones over ANIM_FRAMES frames (2px per frame).
 * Anything that jumped more than one tile (stairs, teleport, spawn)
 * snaps instead of gliding.
 */
#define ANIM_FRAMES 4u

static uint8_t o_px, o_py;
static uint8_t o_mx[MAX_MONSTERS], o_my[MAX_MONSTERS];

static void snapshot_positions(void) {
    uint8_t i;
    o_px = g_px;
    o_py = g_py;
    for (i = 0; i < MAX_MONSTERS; i++) {
        o_mx[i] = g_mons[i].x;
        o_my[i] = g_mons[i].y;
    }
}

static uint8_t cam_target_for(uint8_t pt, uint8_t span_tiles,
                              uint8_t view_px, uint8_t center) {
    uint16_t p = (uint16_t)pt * 8u;
    uint16_t lim = (uint16_t)(span_tiles * 8u - view_px);
    if (p <= center) return 0;
    p -= center;
    if (p > lim) p = lim;
    return (uint8_t)p;
}

/* Doubled animation speed for auto-repeated steps ("repeat = 2x"). */
static uint8_t anim_fast;
/* B-dash: skip the glide entirely (snap steps). */
static uint8_t anim_skip;

/* A+B rest repeat interval per speed setting (frames). */
static const uint8_t REST_RATE[3] = { 16u, 8u, 4u };

/* Interpolate a*8 -> b*8, frame f of `frames` (step = 8/frames px). */
static uint8_t lerp8(uint8_t a, uint8_t b, uint8_t f, uint8_t step) {
    int8_t d = (int8_t)(b - a);
    return (uint8_t)((uint8_t)(a * 8u) + (uint8_t)(d * (int8_t)step * (int8_t)f));
}

static void animate_turn(void) {
    uint8_t f, i;
    /* Hasted play glides at double speed — the effect reads in the
       body language, not just the status tag. (No player-slow effect
       exists yet; wands of slow only target monsters.) */
    uint8_t quick = (uint8_t)(anim_fast || g_haste_t);
    uint8_t frames = quick ? 2u : ANIM_FRAMES;
    uint8_t step = quick ? 4u : 2u;
    uint8_t ocx = cam_target_for(o_px, WORLD_W, 160u, 72u);
    uint8_t ocy = cam_target_for(o_py, WORLD_H, 128u, 64u);
    uint8_t ncx = view_cam_px(), ncy = view_cam_py();
    uint8_t jump = (uint8_t)((uint8_t)(g_px - o_px + 1u) > 2u ||
                             (uint8_t)(g_py - o_py + 1u) > 2u);

    if (!render_world_on() || jump || anim_skip) {
        render_scroll(ncx, ncy);
        view_sync_sprites();
        return;
    }

    for (f = 1; f <= frames; f++) {
        uint8_t scx = (uint8_t)(ocx + (uint8_t)((int8_t)(ncx - ocx) / (int8_t)frames * (int8_t)f));
        uint8_t scy = (uint8_t)(ocy + (uint8_t)((int8_t)(ncy - ocy) / (int8_t)frames * (int8_t)f));
        wait_vbl_done();
        input_tick();                 /* never drop taps mid-animation */
        render_msg_tick();
        render_scroll(scx, scy);
        render_sprite_pos(SPR_PLAYER,
                          (uint8_t)(lerp8(o_px, g_px, f, step) - scx),
                          (uint8_t)(lerp8(o_py, g_py, f, step) - scy));
        for (i = 0; i < MAX_MONSTERS; i++) {
            const monster_t *m = &g_mons[i];
            uint8_t sx, sy;
            if (m->kind == MON_NONE) continue;
            /* never animate a hidden sprite into view: unexplored /
               out-of-sight monsters stay hidden through the glide */
            if (!view_mon_shown_idx(i)) continue;
            if ((uint8_t)(m->x - o_mx[i] + 1u) > 2u ||
                (uint8_t)(m->y - o_my[i] + 1u) > 2u) continue;  /* snaps below */
            sx = (uint8_t)(lerp8(o_mx[i], m->x, f, step) - scx);
            sy = (uint8_t)(lerp8(o_my[i], m->y, f, step) - scy);
            if (sx < 160u && sy < 128u)
                render_sprite_pos((uint8_t)(SPR_MON0 + i), sx, sy);
        }
        render_present();
    }
    view_sync_sprites();
}

/* One consumed turn: world reacts, screen glides, HUD refreshes, and
   the run is committed to SRAM — every step is final. */
static void finish_turn(void) {
    after_player_turn();
    if (g_debug && !g_hp) g_hp = g_maxhp;   /* debug invincibility */
    view_worldpaint_around();
    animate_turn();
    view_lunge_play();        /* attackers jab toward their victims */
    render_flash_play();      /* queued damage blinks, if any */
    render_msg_begin();       /* message slides in after the action */
    /* Rebuilding the status row composes ~40 banked text tiles — far
       too costly to redo every dash step. During a dash (anim_skip)
       skip it; fast_move refreshes it once when the run ends. */
    if (!anim_skip) status_update();
    render_present();
    /* dash defers the autosave to its end (one save per run) */
    if (g_hp && !g_won && !anim_skip) save_write();
}

/* Footsteps alternate "zu, za" — one toggle per landed step. */
static uint8_t step_alt;

/* Walked into a wall: thud + a 3px camera jolt toward the wall.
   The player sprite stays put, so the world reads as the thing that
   shudders. Clamped at the map edges (the jolt just vanishes there —
   never expose wrapped BG rows). */
static uint8_t jolt_clamp(uint8_t cam, int8_t d, uint8_t lim) {
    int16_t v = (int16_t)cam + d * 3;
    if (v < 0) v = 0;
    if (v > (int16_t)lim) v = lim;
    return (uint8_t)v;
}

static void wall_bump(int8_t dx, int8_t dy) {
    /* A dash that runs into a wall should just come to rest beside it,
       not slam into it — skip the thud and screen jolt while dashing.
       try_move still returns 0 here, so the run stops at this cell. */
    if (anim_skip) return;
    sfx_play(SFX_BUMP);
    if (render_world_on()) {
        uint8_t bx = jolt_clamp(view_cam_px(), dx, WORLD_W * 8u - 160u);
        uint8_t by = jolt_clamp(view_cam_py(), dy, WORLD_H * 8u - 128u);
        uint8_t f;
        for (f = 0; f < 3u; f++) {
            wait_vbl_done();
            input_tick();
            render_msg_tick();
            render_scroll(bx, by);
        }
        wait_vbl_done();
        input_tick();
        render_msg_tick();
        render_scroll(view_cam_px(), view_cam_py());
    }
}

/* Returns 1 if the input consumed a game turn. */
static uint8_t try_move(int8_t dx, int8_t dy) {
    uint8_t nx, ny;
    monster_t *m;

    if (g_conf_t && rng_byte() < 190u) {
        dx = (int8_t)(rng_range(3)) - 1;
        dy = (int8_t)(rng_range(3)) - 1;
        if (!dx && !dy) return 1;       /* stumble in place */
    }
    nx = (uint8_t)(g_px + dx);
    ny = (uint8_t)(g_py + dy);
    m = mon_at(nx, ny);
    /* Diagonal rules: a STEP obeys the strict corner/door rule, but a
       diagonal STRIKE only needs the looser attack rule (hit around a
       wall corner, never through a doorway). */
    if (dx && dy) {
        if (m) {
            if (!map_diag_attack_ok(g_px, g_py, nx, ny)) {
                wall_bump(dx, dy);
                return 0;
            }
        } else if (!map_diag_ok(g_px, g_py, nx, ny)) {
            wall_bump(dx, dy);         /* blocked corner: same thud */
            return 0;
        }
    }
    if (m) {
        /* being held stops your feet, not your arms — otherwise a
           gripping flytrap would be an unwinnable soft-lock */
        combat_player_attack(m);
        return 1;
    }
    if (g_held_t) {
        msg_post_id(SID_A_HELD);
        return 1;                       /* struggling wastes the turn */
    }
    if (!map_walkable(nx, ny)) {
        wall_bump(dx, dy);
        return g_conf_t ? 1u : 0u;
    }
    g_px = nx;
    g_py = ny;
    sfx_play((step_alt ^= 1u) ? SFX_STEP_A : SFX_STEP_B);
    view_player_moved();
    item_pickup_here();
    if (traps_step()) {
        /* trap door: plunge one level down */
        ui_popup(lang_str(SID_PLUNGE1), lang_str(SID_PLUNGE2), 0);
        g_depth++;
        new_level();
        fresh_floor = 1;
        {
            uint8_t d = rng_dice(1, 6);
            if (d >= g_hp) g_hp = 1;    /* the fall hurts, never kills */
            else g_hp -= d;
        }
        msg_post_id(SID_LAND_HARD);
    }
    return 1;
}

#define DIR_KEYS (J_UP | J_DOWN | J_LEFT | J_RIGHT)

/* Small grace window so a two-button diagonal press registers as one
   8-way step: gather held D-pad bits over 3 extra frames. */
static uint8_t gather_dirs(uint8_t keys) {
    uint8_t vec = (uint8_t)((keys | input_held()) & DIR_KEYS);
    uint8_t grace;
    for (grace = 0; grace < 3u; grace++) {
        wait_vbl_done();
        vec |= (uint8_t)(input_held() & DIR_KEYS);
    }
    return vec;
}

static void dirs_to_vec(uint8_t dirs, int8_t *dx, int8_t *dy) {
    *dx = 0;
    *dy = 0;
    if (dirs & J_LEFT)  *dx = -1;
    if (dirs & J_RIGHT) *dx = 1;
    if (dirs & J_UP)    *dy = -1;
    if (dirs & J_DOWN)  *dy = 1;
}

/* Count walkable neighbors — used to stop fast-move at junctions. */
static uint8_t walk_neighbors(uint8_t x, uint8_t y) {
    uint8_t n = 0;
    if (map_walkable((uint8_t)(x + 1u), y)) n++;
    if (map_walkable((uint8_t)(x - 1u), y)) n++;
    if (map_walkable(x, (uint8_t)(y + 1u))) n++;
    if (map_walkable(x, (uint8_t)(y - 1u))) n++;
    return n;
}

static uint8_t any_monster_visible(void) {
    uint8_t i;
    for (i = 0; i < MAX_MONSTERS; i++) {
        const monster_t *m = &g_mons[i];
        if (m->kind == MON_NONE) continue;
        if (view_visible(m->x, m->y)) return 1;
    }
    return 0;
}

/* B+dir: run until something interesting stops us. Returns turns. */
static uint8_t fast_move(int8_t dx, int8_t dy) {
    uint8_t steps = 0, hp0 = g_hp;
    uint8_t room0 = g_cur_room;
    anim_skip = 1;                    /* dash: no glide, just go */
    mons_dash(1);                     /* throttle the chase-map rebuild */
    while (steps < 40u) {
        tile_id_t here;
        snapshot_positions();
        if (!try_move(dx, dy)) break;
        steps++;
        finish_turn();
        /* light pacing so the dash reads as motion, not teleport */
        wait_vbl_done();
        input_tick();
        render_msg_tick();
        if (!g_hp || g_won) break;
        here = map_terrain(g_px, g_py);
        if (here == TI_DOOR || here == TI_STAIRS_DOWN ||
            here == TI_STAIRS_UP)
            break;
        if (here == TI_TRAP && !(map_cell(g_px, g_py) & MF_HIDDEN)) break;
        if (g_hp < hp0 || g_sleep_t || g_held_t) break;
        if (any_monster_visible()) break;
        if (item_floor_at(g_px, g_py)) break;
        if (g_cur_room != room0) break;              /* entered / left a room */
        if (g_cur_room == 0xFFu && walk_neighbors(g_px, g_py) > 2u) break;
    }
    anim_skip = 0;
    mons_dash(0);                     /* back to per-step chase precision */
    if (steps) {
        status_update();          /* one HUD refresh for the whole run */
        render_present();
        if (g_hp && !g_won) save_write();
    }
    return steps ? 1u : 0u;
}

/* A button in the world: stairs, else search for traps. */
/* A on the stairs always descends (deeper). After the Amulet you may
   also climb with B (see play()); reaching level 1 by climbing wins. */
static uint8_t do_action(void) {
    if (map_terrain(g_px, g_py) == TI_STAIRS_DOWN) {
        sfx_play(SFX_STAIRS);
        level_transition(1);
        msg_post_id(SID_DESCEND);
        return 1;
    }
    sfx_play(SFX_REST);                 /* soft "sa" for resting in place */
    traps_search();
    return 1;                           /* searching takes a turn */
}

/* B on the stairs after the Amulet climbs back up; level 1 = escape. */
static uint8_t climb_action(void) {
    if (!g_has_amulet || map_terrain(g_px, g_py) != TI_STAIRS_DOWN)
        return 0;
    if (g_depth <= 1u) {
        g_won = 1;
    } else {
        sfx_play(SFX_STAIRS);
        level_transition(-1);
        msg_post_id(SID_CLIMB);
    }
    return 1;
}

static void new_level(void) {
    /* Level generation lives in BANK2 (mapgen.c); hop in via call_bank. All
       its callees are bank 0 and it renders nothing — the view repaint below
       runs after call_bank returns (bank 1 remapped). */
    call_bank(2u, mapgen_generate);
    mons_spawn_level();
    view_player_moved();
    view_world_enter();
    bgm_set_depth(g_depth);       /* deeper: slower and lower */
}

/* Stairs: the old floor fades away, the new one is generated and
   painted behind the black, then fades back in. The depth changes
   only after the fade so "depth moved" implies "new map exists"
   (the harness keys on that). */
static void level_transition(int8_t ddepth) {
    render_fade_out(FADE_OUT_FRAMES);
    g_depth = (uint8_t)(g_depth + ddepth);
    if (g_depth > g_deepest) g_deepest = g_depth;
    new_level();
    fresh_floor = 1;
    render_present();
    render_fade_in(FADE_IN_FRAMES);
}

static uint8_t haste_phase;

static void after_player_turn(void) {
    g_turns++;
    mons_wander_tick();
    /* Hasted: monsters only move on every other player action. */
    if (fresh_floor) {
        fresh_floor = 0;
    } else if (g_haste_t && (haste_phase ^= 1u)) {
        /* free action */
    } else {
        mons_take_turns();
    }
    if (!g_hp) return;
    effects_turn();
    msgq_flush();  /* render deferred upkeep messages (before death-return) */
    if (!g_hp) return;
    if (g_hp < g_maxhp && (g_turns % REGEN_PERIOD) == 0u)
        g_hp++;
    /* Fainting from hunger can black you out mid-stride. */
    if (g_hunger == 3u && !g_sleep_t && rng_byte() < 50u) {
        msg_post_id(SID_H_FAINTED);
        g_sleep_t = (uint8_t)(1u + rng_range(3));
    }
}

/* Play-loop exit reasons. */
#define END_DEAD      0u
#define END_WON       1u
#define END_SUSPENDED 2u

static uint8_t play(void);

/* File a finished run into the persistent ranking (skipped for debug
   invincibility runs). */
static void rank_record(void) {
    rank_entry_t e;
    if (g_debug) return;
    e.gold = g_gold;
    e.deepest = g_deepest;
    e.final = g_depth;
    e.amulet = g_has_amulet;
    rank_insert(&e);
}

void game_run(void) {
    for (;;) {
        uint8_t reason;

        if (ui_title_show() && save_load()) {
            msg_clear();
            view_world_enter();
            msg_post_id(SID_WELCOME_BACK);
        } else {
            world_new();
            identify_new_game();
            inv_clear();
            inv_starting_kit();
            msg_clear();
            new_level();
            msg_post_id(SID_WELCOME);
        }

        status_update();
        msg_refresh();
        render_present();
        /* swallow title-exit edges BEFORE the fade so taps during the
           fade-in stay latched and act as queued input */
        input_swallow_edges();
        bgm_start();
        render_fade_in(FADE_IN_FRAMES);    /* title faded out to here */

        reason = play();
        bgm_stop();
        if (reason != END_SUSPENDED)
            rank_record();
        if (reason == END_WON) {
            save_invalidate();
            ui_win_show();
        } else if (reason == END_DEAD) {
            /* let the fatal line (monster hit / trap / starvation)
               slide in and sit on screen before the game-over art */
            uint8_t f;
            for (f = 0; f < 100u; f++) {
                wait_vbl_done();
                view_breathe();
                render_msg_tick();
            }
            save_invalidate();          /* permadeath: no reload */
            ui_dead_show();
        }
        /* END_SUSPENDED: save already written; back to the title. */
    }
}

static uint8_t play(void) {
        while (g_hp && !g_won) {
            uint8_t keys;
            uint8_t acted = 0;
            wait_vbl_done();
            view_breathe();
            render_msg_tick();

            /* Asleep / frozen: the world moves on without you. */
            if (g_sleep_t) {
                g_sleep_t--;
                snapshot_positions();
                finish_turn();
                input_swallow_edges();
                continue;
            }

            keys = input_pressed();
            if (keys) snapshot_positions();
            if (keys & DIR_KEYS) {
                int8_t dx, dy;
                uint8_t held = input_held();
                uint8_t was_repeat = g_input_repeat;
                if (held & J_START) {
                    /* START held: diagonal-locked even when both edges
                       land in the same (latched) poll */
                    dirs_to_vec(gather_dirs(keys), &dx, &dy);
                    if (dx && dy) acted = try_move(dx, dy);
                } else if (held & J_B) {
                    /* B+dir: fast move (handles its own turns/redraw) */
                    dirs_to_vec((uint8_t)(keys & DIR_KEYS), &dx, &dy);
                    if (dx && dy) dy = 0;            /* runs are 4-way */
                    fast_move(dx, dy);
                    input_swallow_edges();
                    acted = 0;
                } else {
                    dirs_to_vec(gather_dirs(keys), &dx, &dy);
                    anim_fast = was_repeat;   /* held-walk glides at 2x */
                    acted = try_move(dx, dy);
                }
            } else if ((keys & (J_A | J_B)) &&
                       (input_held() & (J_A | J_B)) == (J_A | J_B)) {
                /* A+B together: rest one turn; keep holding both to
                   repeat after 0.5s at the configured rate, with the
                   glide animation doubled while repeating */
                uint8_t wait_f = 30u;
                msg_post_id(SID_YOU_WAIT);
                snapshot_positions();
                finish_turn();
                while (g_hp && !g_won
                       && (input_held() & (J_A | J_B)) == (J_A | J_B)) {
                    wait_vbl_done();
                    view_breathe();
                    input_tick();
                    render_msg_tick();
                    if (--wait_f) continue;
                    wait_f = REST_RATE[g_repeat_speed];
                    snapshot_positions();
                    anim_fast = 1;
                    finish_turn();
                    anim_fast = 0;
                }
                input_swallow_edges();
                acted = 0;                    /* turns already applied */
            } else if (keys & J_A) {
                acted = do_action();
            } else if (keys & J_B) {
                acted = climb_action();   /* Amulet: climb up the stairs */
            } else if (keys & J_SELECT) {
                /* tap = pack; hold ~0.4s = full-map overview */
                uint8_t held_f = 0;
                while ((input_held() & J_SELECT) && held_f < 24u) {
                    wait_vbl_done();
                    view_breathe();
                    input_tick();
                    render_msg_tick();
                    held_f++;
                }
                if (held_f >= 24u) {
                    ui_map_show();
                    acted = 0;
                } else {
                    acted = ui_inv_show();
                }
            } else if (keys & J_START) {
                /* START held + D-pad: diagonal-locked walking (both
                   D-pad buttons together, cardinals ignored). Releasing
                   START without touching the D-pad opens the menu. */
                uint8_t dir_used = 0;
                while (input_held() & J_START) {
                    uint8_t k2;
                    wait_vbl_done();
                    view_breathe();
                    render_msg_tick();
                    k2 = input_pressed();
                    if (k2 & DIR_KEYS) {
                        int8_t dx, dy;
                        dir_used = 1;
                        dirs_to_vec(gather_dirs(k2), &dx, &dy);
                        if (dx && dy) {
                            snapshot_positions();
                            if (try_move(dx, dy)) finish_turn();
                            if (!g_hp || g_won) break;
                        }
                    }
                }
                input_swallow_edges();
                if (!dir_used && g_hp && !g_won) {
                    uint8_t r = ui_menu_show();
                    if (r == MENU_SUSPEND) {
                        save_write();
                        ui_popup(lang_str(SID_SAVED), lang_str(SID_SAFE_OFF), 0);
                        return END_SUSPENDED;
                    }
                    acted = (r == MENU_REST) ? 1u : 0u;
                }
            }
            if (acted) finish_turn();
            else render_present();
            anim_fast = 0;
        }

        return g_won ? END_WON : END_DEAD;
}
