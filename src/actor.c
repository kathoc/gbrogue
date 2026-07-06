#include <string.h>
#include "actor.h"
#include "monsters.h"
#include "map.h"
#include "world.h"
#include "worldview.h"
#include "rng.h"
#include "combat.h"
#include "effects.h"

monster_t g_mons[MAX_MONSTERS];

/*
 * Chase pathing: one BFS "distance to player" map per player turn
 * (classic Dijkstra-map). Greedy sign-stepping got stuck on room
 * walls — a monster beside the doorway would slide along the wall
 * forever instead of following you out. Each awake monster now just
 * rolls downhill on this map.
 */
#define DIST_INF 0xFFu
/* 16 covers everything on or just off the 20x16 viewport; farther
   monsters fall back to greedy chase. Radius is the main BFS cost. */
#define DIST_CAP 16u
static uint8_t  g_dist[MAP_H][MAP_W];
static uint16_t bfs_q[128];

static const int8_t DX8[8] = { 1, -1, 1, -1, 1, -1, 0, 0 };
static const int8_t DY8[8] = { 1, 1, -1, -1, 0, 0, 1, -1 };

/*
 * Flat-index BFS, hand-tuned: this runs on every player turn with an
 * awake monster and used to cost several frames through the
 * map_walkable()/map_diag_ok() call graph (visible as sluggish
 * walking that "healed" when monsters died). Direct g_map reads, a
 * terrain->walkable table and no bounds checks (the map border is
 * never walkable, so wrapped neighbor indices always land on rock).
 */
static const uint8_t T_WALK[TI_COUNT] = {
    0, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};
#define FLAT(p) ((uint8_t *)(p))

/* The map only depends on terrain + player position: skip the rebuild
   on turns where the player stood still (fighting, resting). A new
   level invalidates the cache via mons_spawn_level(). */
static uint8_t dm_px = 0xFFu, dm_py;

/* A single rebuild costs several frames; during a B-dash the player
   moves every step so the cache misses every step, which is what made
   dashing crawl once anything was awake. While dashing we refresh the
   map only every few steps — the chasers are off-screen (the dash
   stops the instant one is visible), so a slightly stale target is
   invisible. dm_skip counts the reuse steps left. */
static uint8_t dm_dash;
static uint8_t dm_skip;

void mons_dash(uint8_t on) {
    dm_dash = on;
    dm_skip = 0;                       /* first dash step rebuilds fresh */
}

static void build_distmap(void) {
    static const int8_t OFF8[8] = { -33, -31, 31, 33, -1, 1, -32, 32 };
    uint8_t *dist = FLAT(g_dist);
    const uint8_t *map = FLAT(g_map);
    uint8_t head = 0, tail = 0;

    if (dm_px == g_px && dm_py == g_py) return;
    if (dm_dash && dm_skip) {           /* dash: reuse the current map */
        dm_skip--;
        return;
    }
    if (dm_dash) dm_skip = 3u;          /* rebuild now, coast 3 steps */
    dm_px = g_px;
    dm_py = g_py;
    memset(g_dist, DIST_INF, sizeof(g_dist));
    {
        uint16_t p0 = (uint16_t)(((uint16_t)g_py << 5) | g_px);
        dist[p0] = 0;
        bfs_q[tail++] = p0;
    }
    while (head != tail) {
        uint16_t cur = bfs_q[head++];
        uint8_t d = dist[cur];
        uint8_t from_door, k;
        head &= 127u;
        if (d >= DIST_CAP) continue;
        d++;
        from_door = (uint8_t)((map[cur] & MF_TERRAIN) == TI_DOOR);
        for (k = 0; k < 8u; k++) {
            uint16_t n = (uint16_t)(cur + OFF8[k]);
            uint8_t t, next;
            if (n >= MAP_W * MAP_H) continue;        /* top/bottom wrap */
            if (dist[n] != DIST_INF) continue;
            t = (uint8_t)(map[n] & MF_TERRAIN);
            if (!T_WALK[t]) continue;
            if (k < 4u) {                            /* diagonal rules */
                uint16_t ox = (uint16_t)(cur + (OFF8[k] > 0 ? OFF8[k] - 32 : OFF8[k] + 32));
                uint16_t oy = (uint16_t)(cur + (OFF8[k] > 0 ? 32 : -32));
                if (from_door || t == TI_DOOR) continue;
                if (!T_WALK[map[ox] & MF_TERRAIN]) continue;
                if (!T_WALK[map[oy] & MF_TERRAIN]) continue;
            }
            dist[n] = d;
            next = (uint8_t)((tail + 1u) & 127u);
            if (next == head) continue;          /* frontier full: partial map */
            bfs_q[tail] = n;
            tail = next;
        }
    }
}

void mons_clear(void) {
    uint8_t i;
    for (i = 0; i < MAX_MONSTERS; i++) g_mons[i].kind = MON_NONE;
}

monster_t *mon_at(uint8_t x, uint8_t y) {
    uint8_t i;
    for (i = 0; i < MAX_MONSTERS; i++) {
        if (g_mons[i].kind != MON_NONE && g_mons[i].x == x && g_mons[i].y == y)
            return &g_mons[i];
    }
    return 0;
}

static uint8_t cell_free(uint8_t x, uint8_t y) {
    if (!map_walkable(x, y)) return 0;
    if (map_terrain(x, y) == TI_STAIRS_DOWN) return 0;
    if (x == g_px && y == g_py) return 0;
    return mon_at(x, y) == 0;
}

void mons_spawn_level(void) {
    uint8_t r, slot = 0;
    dm_px = 0xFFu;                     /* new terrain: drop the cache */
    mons_clear();
    for (r = 0; r < g_room_count && slot < MAX_MONSTERS; r++) {
        const room_t *rm = &g_rooms[r];
        uint8_t want, tries;
        if (rm->flags & ROOM_GONE) continue;
        want = (uint8_t)(rng_byte() < 166u);       /* ~65% per room */
        if (want && rng_byte() < 77u) want = 2;    /* ~30% of those: a pair */
        for (; want && slot < MAX_MONSTERS; want--) {
            for (tries = 0; tries < 8; tries++) {
                uint8_t x = (uint8_t)(rm->x + 1u + rng_range(rm->w - 2u));
                uint8_t y = (uint8_t)(rm->y + 1u + rng_range(rm->h - 2u));
                if (!cell_free(x, y)) continue;
                g_mons[slot].kind = monster_pick(g_depth);
                g_mons[slot].x = x;
                g_mons[slot].y = y;
                g_mons[slot].hp = monster_roll_hp(g_mons[slot].kind);
                g_mons[slot].state = 0;            /* asleep */
                g_mons[slot].eff = 0;
                g_mons[slot].eff_t = 0;
                slot++;
                break;
            }
        }
    }
}

uint8_t mon_damage(monster_t *m, uint8_t dmg) {
    m->state |= MST_AWAKE;                         /* pain wakes anything */
    if (m->hp <= dmg) {
        m->kind = MON_NONE;
        return 1;
    }
    m->hp -= dmg;
    return 0;
}

static uint8_t adjacent8(uint8_t ax, uint8_t ay, uint8_t bx, uint8_t by) {
    return (uint8_t)(ax - bx + 1u) <= 2u && (uint8_t)(ay - by + 1u) <= 2u &&
           !(ax == bx && ay == by);
}

/* A monster may STEP into a diagonal cell only under the same rules as
   the player: no doorway diagonals, no corner cutting. */
static uint8_t mon_can_reach(uint8_t fx, uint8_t fy, uint8_t tx, uint8_t ty) {
    if (fx != tx && fy != ty)
        return map_diag_ok(fx, fy, tx, ty);
    return 1;
}

static void mon_step(monster_t *m) {
    int8_t sx = 0, sy = 0;
    if (m->eff & MEF_CONF) {
        /* stagger randomly */
        sx = (int8_t)(rng_range(3)) - 1;
        sy = (int8_t)(rng_range(3)) - 1;
    } else if (m->eff & MEF_FLEE) {
        if (m->x < g_px) sx = -1; else if (m->x > g_px) sx = 1;
        if (m->y < g_py) sy = -1; else if (m->y > g_py) sy = 1;
    } else {
        /* chase: roll downhill on the player-distance map */
        uint8_t best = g_dist[m->y][m->x];
        uint8_t k, bx = m->x, by = m->y;
        for (k = 0; k < 8u; k++) {
            uint8_t nx = (uint8_t)(m->x + DX8[k]);
            uint8_t ny = (uint8_t)(m->y + DY8[k]);
            if (nx >= MAP_W || ny >= MAP_H) continue;
            if (g_dist[ny][nx] >= best) continue;
            if (!cell_free(nx, ny)) continue;
            if (!mon_can_reach(m->x, m->y, nx, ny)) continue;
            best = g_dist[ny][nx];
            bx = nx;
            by = ny;
        }
        if (bx != m->x || by != m->y) {
            m->x = bx;
            m->y = by;
            return;
        }
        /* off the map's reach (or boxed in): legacy greedy fallback */
        if (m->x < g_px) sx = 1; else if (m->x > g_px) sx = -1;
        if (m->y < g_py) sy = 1; else if (m->y > g_py) sy = -1;
    }
    /* Diagonal first, then each axis — greedy like Rogue's chase. */
    if (sx && sy && cell_free((uint8_t)(m->x + sx), (uint8_t)(m->y + sy)) &&
        map_diag_ok(m->x, m->y,
                    (uint8_t)(m->x + sx), (uint8_t)(m->y + sy))) {
        m->x = (uint8_t)(m->x + sx);
        m->y = (uint8_t)(m->y + sy);
        return;
    }
    if (sx && cell_free((uint8_t)(m->x + sx), m->y)) {
        m->x = (uint8_t)(m->x + sx);
        return;
    }
    if (sy && cell_free(m->x, (uint8_t)(m->y + sy))) {
        m->y = (uint8_t)(m->y + sy);
        return;
    }
}

static void mon_one_turn(monster_t *m) {
    if (m->eff & MEF_HELD) return;
    if ((m->eff & MEF_FLEE) || (m->eff & MEF_CONF)) {
        mon_step(m);
        return;
    }
    if (adjacent8(m->x, m->y, g_px, g_py) &&
        mon_can_reach(m->x, m->y, g_px, g_py)) {
        /* A diagonal strike obeys the SAME strict rule as a step: no
           cutting a wall corner, no reaching through a doorway. A monster
           wedged one diagonal away past a wall/door must come around to
           an orthogonal cell before it can hit — no more diagonal jabs
           through the corner of an L-corridor or across a doorway. */
        combat_monster_attack(m);
    } else {
        mon_step(m);
    }
}

void mons_wander_tick(void) {
    uint8_t slot, tries;

    g_wander_t++;
    if (g_wander_t < 45u || rng_byte() >= 12u) return;

    for (slot = 0; slot < MAX_MONSTERS; slot++)
        if (g_mons[slot].kind == MON_NONE) break;
    if (slot >= MAX_MONSTERS) return;

    for (tries = 0; tries < 20u; tries++) {
        uint8_t x = rng_range(MAP_W);
        uint8_t y = rng_range(MAP_H);
        if (!cell_free(x, y)) continue;
        if (view_visible(x, y)) continue;      /* never pops in on-screen */
        g_mons[slot].kind = monster_pick(g_depth);
        g_mons[slot].x = x;
        g_mons[slot].y = y;
        g_mons[slot].hp = monster_roll_hp(g_mons[slot].kind);
        g_mons[slot].state = MST_AWAKE;        /* wanderers hunt */
        g_mons[slot].eff = 0;
        g_mons[slot].eff_t = 0;
        g_wander_t = 0;
        return;
    }
}

void mons_take_turns(void) {
    uint8_t i;
    uint8_t aggravated = effects_ring_worn(6u);   /* aggravate ring */
    uint8_t stealthy = effects_ring_worn(12u);    /* stealth ring */
    uint8_t mapped = 0;

    for (i = 0; i < MAX_MONSTERS; i++) {
        monster_t *m = &g_mons[i];
        if (m->kind == MON_NONE) continue;

        if (m->eff_t) {
            m->eff_t--;
            if (m->eff_t == 0)
                m->eff &= (uint8_t)~(MEF_HELD | MEF_CONF | MEF_FLEE);
        }

        if (!(m->state & MST_AWAKE)) {
            /* adjacent = you are right next to it (Chebyshev <= 1) */
            uint8_t adjacent = (uint8_t)((m->x - g_px + 1u) <= 2u &&
                                         (m->y - g_py + 1u) <= 2u);
            if (aggravated || (!stealthy && adjacent)) {
                /* ANY monster stirs when you step next to it — a bat you
                   walk up to should not sit there like a statue. */
                m->state |= MST_AWAKE;
            } else if (!stealthy &&
                       (mkind(m->kind)->flags & MFL_MEAN) &&
                       (view_visible(m->x, m->y) ||
                        /* doorway case: close enough to hear you even
                           when the room-light rule says "unseen" */
                        ((uint8_t)(m->x - g_px + 2u) <= 4u &&
                         (uint8_t)(m->y - g_py + 2u) <= 4u))) {
                /* Mean monsters spring when they can see you (same lit
                   room / adjacent); others sleep until hurt. */
                m->state |= MST_AWAKE;
            } else {
                continue;
            }
            /* It only just woke THIS turn — it stirs but does not get to
               move or strike yet. Stepping next to a sleeper no longer
               hands it a free first hit; it acts from next turn on.
               (A monster woken by being HIT sets MST_AWAKE inside
               mon_damage, not here, so it still retaliates normally.) */
            continue;
        }

        if (!mapped) {
            build_distmap();
            mapped = 1;
        }

        if ((m->eff & MEF_SLOW) && (g_turns & 1u)) continue;
        mon_one_turn(m);
        if (g_hp == 0) return;
        if ((m->eff & MEF_HASTE) && m->kind != MON_NONE) {
            mon_one_turn(m);
            if (g_hp == 0) return;
        }
    }
}
