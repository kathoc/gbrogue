#include "mapgen.h"
#include "map.h"
#include "world.h"
#include "rng.h"
#include "items.h"
#include "traps.h"

/*
 * Rogue 5.4-style generation: a 3x3 grid of cells, one room (or a
 * "gone" corridor junction) per cell, corridors along a random spanning
 * tree of the grid plus a couple of extra links.
 *
 * Cell layout on the 64x32 map: 21x10 cells with a 1-tile outer frame
 * of the cell reserved as rock so adjacent rooms never share walls.
 */

#define GRID       3
#define CELL_W     10
#define CELL_H     9

/* Per-room corridor anchor: interior point corridors route to. */
static uint8_t anchor_x[MAX_ROOMS];
static uint8_t anchor_y[MAX_ROOMS];
static uint8_t connected[MAX_ROOMS];

static void carve_room(uint8_t idx) {
    room_t *r = &g_rooms[idx];
    uint8_t cx0 = (uint8_t)((idx % GRID) * CELL_W);
    uint8_t cy0 = (uint8_t)((idx / GRID) * CELL_H);
    uint8_t x, y;

    if (r->flags & ROOM_GONE) {
        /* A 1x1 corridor junction somewhere mid-cell. */
        x = (uint8_t)(cx0 + 3u + rng_range(CELL_W - 6u));
        y = (uint8_t)(cy0 + 3u + rng_range(CELL_H - 6u));
        r->x = x; r->y = y; r->w = 1; r->h = 1;
        anchor_x[idx] = x;
        anchor_y[idx] = y;
        map_set_terrain(x, y, TI_CORRIDOR);
        return;
    }

    r->w = (uint8_t)(5u + rng_range(CELL_W - 2u - 5u + 1u));   /* 5..8 */
    r->h = (uint8_t)(4u + rng_range(CELL_H - 2u - 4u + 1u));   /* 4..7 */
    r->x = (uint8_t)(cx0 + 1u + rng_range(CELL_W - r->w - 1u));
    r->y = (uint8_t)(cy0 + 1u + rng_range(CELL_H - r->h - 1u));

    for (y = r->y; y < (uint8_t)(r->y + r->h); y++) {
        for (x = r->x; x < (uint8_t)(r->x + r->w); x++) {
            tile_id_t t;
            if (y == r->y || y == (uint8_t)(r->y + r->h - 1u)) t = TI_WALL_H;
            else if (x == r->x || x == (uint8_t)(r->x + r->w - 1u)) t = TI_WALL_V;
            else t = TI_FLOOR;
            map_set_terrain(x, y, t);
        }
    }
    anchor_x[idx] = (uint8_t)(r->x + 1u + rng_range(r->w - 2u));
    anchor_y[idx] = (uint8_t)(r->y + 1u + rng_range(r->h - 2u));
}

/* Carve one corridor step: rock becomes corridor, room walls become
   doors, existing floor / doors / corridors are left alone. */
static void dig(uint8_t x, uint8_t y) {
    switch (map_terrain(x, y)) {
    case TI_BLANK:
        map_set_terrain(x, y, TI_CORRIDOR);
        break;
    case TI_WALL_H:
    case TI_WALL_V:
        map_set_terrain(x, y, TI_DOOR);
        break;
    default:
        break;
    }
}

static void dig_h(uint8_t x0, uint8_t x1, uint8_t y) {
    uint8_t x;
    if (x0 > x1) { x = x0; x0 = x1; x1 = x; }
    for (x = x0; x <= x1; x++) dig(x, y);
}

static void dig_v(uint8_t y0, uint8_t y1, uint8_t x) {
    uint8_t y;
    if (y0 > y1) { y = y0; y0 = y1; y1 = y; }
    for (y = y0; y <= y1; y++) dig(x, y);
}

/* Grid edges already dug. Re-digging a pair with the other elbow
   direction lays a second corridor one tile over — twin doors. */
static uint16_t dug_links;

static uint8_t link_bit(uint8_t a, uint8_t b) {
    uint8_t lo = a < b ? a : b;
    return (uint8_t)(lo * 2u + (uint8_t)((b - a == 1u || a - b == 1u) ? 0u : 1u));
}

/*
 * Connect two neighboring cells' anchors with a Z-shaped corridor whose
 * middle leg runs in the room-free strip between the cells (rooms can
 * never occupy those two columns/rows). Every leg thus crosses walls
 * strictly perpendicular, only at the two rooms' own anchor lines — so
 * a room carries at most four doors and no two doors can ever touch,
 * regardless of how the other rooms landed.
 */
static void connect_rooms(uint8_t a, uint8_t b) {
    uint8_t lo = a < b ? a : b;
    uint8_t hi = a < b ? b : a;
    uint16_t bit = (uint16_t)1u << link_bit(a, b);
    if (dug_links & bit) return;
    dug_links |= bit;
    if ((uint8_t)(hi - lo) == 1u) {            /* east-west neighbors */
        uint8_t mc = (uint8_t)((lo % GRID) * CELL_W + CELL_W - 1u +
                               (rng_byte() & 1u));
        dig_h(anchor_x[lo], mc, anchor_y[lo]);
        dig_v(anchor_y[lo], anchor_y[hi], mc);
        dig_h(mc, anchor_x[hi], anchor_y[hi]);
    } else {                                   /* north-south neighbors */
        uint8_t mr = (uint8_t)((lo / GRID) * CELL_H + CELL_H - 1u +
                               (rng_byte() & 1u));
        dig_v(anchor_y[lo], mr, anchor_x[lo]);
        dig_h(anchor_x[lo], anchor_x[hi], mr);
        dig_v(mr, anchor_y[hi], anchor_x[hi]);
    }
}

/* Interior anchors are dug over by connect_rooms (they sit inside the
   room so dig() ignores them); nothing else to fix up. */

/* Retract a dead-end corridor left behind by a sealed door. */
static void trim_stub(uint8_t x, uint8_t y) {
    while (map_terrain(x, y) == TI_CORRIDOR) {
        uint8_t n = 0, tx = 0, ty = 0, k;
        static const int8_t OX[4] = { 1, -1, 0, 0 };
        static const int8_t OY[4] = { 0, 0, 1, -1 };
        for (k = 0; k < 4u; k++) {
            uint8_t nx = (uint8_t)(x + OX[k]);
            uint8_t ny = (uint8_t)(y + OY[k]);
            if (map_walkable(nx, ny)) { n++; tx = nx; ty = ny; }
        }
        if (n > 1u) return;
        map_set_terrain(x, y, TI_BLANK);
        if (n == 0u) return;
        x = tx; y = ty;
    }
}

/*
 * Insurance: with Z-corridors adjacent doors are impossible by
 * construction, but keep a cheap sweep so a future digging change can't
 * silently reintroduce them. Sealing the second door of a side-by-side
 * pair is safe when both outer cells are walkable (traffic reroutes
 * through the twin one tile over); the orphaned corridor is retracted.
 */
static void fix_door_pairs(void) {
    uint8_t x, y;
    for (y = 0; y < (uint8_t)(MAP_H - 1u); y++) {
        for (x = 0; x < (uint8_t)(MAP_W - 1u); x++) {
            if (map_terrain(x, y) != TI_DOOR) continue;
            if (map_terrain((uint8_t)(x + 1u), y) == TI_DOOR &&
                ((map_walkable(x, (uint8_t)(y - 1u)) &&
                  map_walkable((uint8_t)(x + 1u), (uint8_t)(y - 1u))) ||
                 (map_walkable(x, (uint8_t)(y + 1u)) &&
                  map_walkable((uint8_t)(x + 1u), (uint8_t)(y + 1u))))) {
                map_set_terrain((uint8_t)(x + 1u), y, TI_WALL_H);
                trim_stub((uint8_t)(x + 1u), (uint8_t)(y - 1u));
                trim_stub((uint8_t)(x + 1u), (uint8_t)(y + 1u));
            }
            if (map_terrain(x, (uint8_t)(y + 1u)) == TI_DOOR &&
                ((map_walkable((uint8_t)(x - 1u), y) &&
                  map_walkable((uint8_t)(x - 1u), (uint8_t)(y + 1u))) ||
                 (map_walkable((uint8_t)(x + 1u), y) &&
                  map_walkable((uint8_t)(x + 1u), (uint8_t)(y + 1u))))) {
                map_set_terrain(x, (uint8_t)(y + 1u), TI_WALL_V);
                trim_stub((uint8_t)(x - 1u), (uint8_t)(y + 1u));
                trim_stub((uint8_t)(x + 1u), (uint8_t)(y + 1u));
            }
        }
    }
}

static const int8_t NEIGH_DX[4] = { 0, 0, -1, 1 };
static const int8_t NEIGH_DY[4] = { -1, 1, 0, 0 };

static uint8_t grid_neighbor(uint8_t idx, uint8_t dir) {
    int8_t c = (int8_t)(idx % GRID) + NEIGH_DX[dir];
    int8_t r = (int8_t)(idx / GRID) + NEIGH_DY[dir];
    if (c < 0 || c >= GRID || r < 0 || r >= GRID) return 0xFFu;
    return (uint8_t)(r * GRID + c);
}

/* Random interior cell of a room that is free of stairs / items. */
static uint8_t free_spot(const room_t *rm, uint8_t *ox, uint8_t *oy) {
    uint8_t tries;
    for (tries = 0; tries < 8; tries++) {
        uint8_t x = (uint8_t)(rm->x + 1u + rng_range(rm->w - 2u));
        uint8_t y = (uint8_t)(rm->y + 1u + rng_range(rm->h - 2u));
        if (map_terrain(x, y) != TI_FLOOR) continue;
        if (item_floor_at(x, y)) continue;
        *ox = x;
        *oy = y;
        return 1;
    }
    return 0;
}

/* Rogue-ish loot mix: heavy on potions/scrolls, light on jewelry. */
static void place_one_item(const room_t *rm) {
    uint8_t x, y, r;
    item_t *it;
    if (!free_spot(rm, &x, &y)) return;
    r = rng_byte();
    if (r < 70u)       it = item_place(IK_POTION, rng_range(N_POTIONS), x, y);
    else if (r < 140u) it = item_place(IK_SCROLL, rng_range(N_SCROLLS), x, y);
    else if (r < 186u) it = item_place(IK_FOOD, rng_byte() < 200u ? 0 : 1, x, y);
    else if (r < 209u) it = item_place(IK_WEAPON, rng_range(N_WEAPONS), x, y);
    else if (r < 232u) it = item_place(IK_ARMOR, rng_range(N_ARMORS), x, y);
    else if (r < 244u) it = item_place(IK_RING, rng_range(N_RINGS), x, y);
    else               it = item_place(IK_WAND, rng_range(N_WANDS), x, y);
    if (!it) return;

    /* Enchant / curse weapons, armor, rings the way Rogue does. */
    if (it->kind == IK_WEAPON || it->kind == IK_ARMOR) {
        uint8_t roll = rng_byte();
        if (roll < 26u) {                       /* ~10% cursed */
            it->flags |= IF_CURSED;
            it->ench = (int8_t)-(int8_t)(1u + rng_range(3));
        } else if (roll < 64u) {                /* ~15% blessed */
            it->ench = (int8_t)(1u + rng_range(3));
        }
    } else if (it->kind == IK_RING) {
        /* rings with a magnitude: protection/add str/dex/damage */
        if (it->sub == 0u || it->sub == 1u || it->sub == 7u || it->sub == 8u) {
            if (rng_byte() < 64u) {
                it->flags |= IF_CURSED;
                it->ench = -1;
            } else {
                it->ench = (int8_t)(1u + rng_range(2));
            }
        }
        /* aggravate / teleport rings are always a curse */
        if (it->sub == 6u || it->sub == 11u) it->flags |= IF_CURSED;
    }
}

static void place_items(void) {
    uint8_t r;
    items_clear_floor();
    for (r = 0; r < g_room_count; r++) {
        const room_t *rm = &g_rooms[r];
        if (rm->flags & ROOM_GONE) continue;
        if (rng_byte() < 102u) {                /* 40%: a gold pile */
            uint8_t x, y;
            if (free_spot(rm, &x, &y)) {
                item_t *it = item_place(IK_GOLD, 0, x, y);
                if (it)
                    it->qty = (uint8_t)(2u + rng_range(12) + 3u * g_depth);
            }
        }
        if (rng_byte() < 90u)                   /* 35%: one item */
            place_one_item(rm);
    }
}

void mapgen_generate(void) {
    uint8_t i, gone_budget, done, guard;
    uint8_t stairs_room, spawn_room;

    map_clear();
    g_room_count = MAX_ROOMS;

    /* Up to 2 gone rooms, never adjacent-guaranteed (not needed). */
    gone_budget = 2;
    for (i = 0; i < MAX_ROOMS; i++) {
        g_rooms[i].flags = 0;
        connected[i] = 0;
        if (gone_budget && rng_byte() < 32u) {   /* ~12.5% each */
            g_rooms[i].flags = ROOM_GONE;
            gone_budget--;
        }
    }

    for (i = 0; i < MAX_ROOMS; i++) carve_room(i);

    /* Random spanning tree over the 3x3 grid. */
    dug_links = 0;
    connected[rng_range(MAX_ROOMS)] = 1;
    done = 1;
    guard = 0;
    while (done < MAX_ROOMS && ++guard != 0) {
        uint8_t a = rng_range(MAX_ROOMS);
        uint8_t d, b;
        if (!connected[a]) continue;
        d = rng_range(4);
        b = grid_neighbor(a, d);
        if (b == 0xFFu || connected[b]) continue;
        connect_rooms(a, b);
        connected[b] = 1;
        done++;
    }
    /* A couple of extra links so the tree gains loops like real Rogue. */
    for (i = (uint8_t)(1u + rng_range(2)); i != 0; i--) {
        uint8_t a = rng_range(MAX_ROOMS);
        uint8_t b = grid_neighbor(a, rng_range(4));
        if (b != 0xFFu) connect_rooms(a, b);
    }
    fix_door_pairs();

    /* Spawn + stairs in distinct non-gone rooms when possible. */
    do { spawn_room = rng_range(MAX_ROOMS); }
    while (g_rooms[spawn_room].flags & ROOM_GONE);
    guard = 0;
    do { stairs_room = rng_range(MAX_ROOMS); }
    while ((g_rooms[stairs_room].flags & ROOM_GONE ||
            stairs_room == spawn_room) && ++guard < 200u);
    if (g_rooms[stairs_room].flags & ROOM_GONE) stairs_room = spawn_room;

    map_set_terrain(anchor_x[stairs_room], anchor_y[stairs_room],
                    TI_STAIRS_DOWN);

    g_px = anchor_x[spawn_room];
    g_py = anchor_y[spawn_room];
    if (g_px == anchor_x[stairs_room] && g_py == anchor_y[stairs_room]) {
        /* Same tile can only happen in the degenerate fallback. */
        g_px--;
    }

    place_items();

    /* Traps: none on level 1, then 1..3 hidden ones scaling with depth. */
    traps_clear();
    if (g_depth >= 2u) {
        uint8_t n = (uint8_t)(1u + rng_range(g_depth < 8u ? 2u : 3u));
        while (n--) {
            uint8_t r = rng_range(MAX_ROOMS);
            uint8_t x, y;
            if (g_rooms[r].flags & ROOM_GONE) continue;
            if (!free_spot(&g_rooms[r], &x, &y)) continue;
            if (x == g_px && y == g_py) continue;
            map_set_terrain(x, y, TI_TRAP);
            map_set_flag(x, y, MF_HIDDEN);
            traps_add(x, y, rng_range(TR_KIND_COUNT));
        }
    }

    /* The Amulet of Yendor waits on level 26 (Rogue's AMULETLEVEL). */
    if (g_depth >= 26u && !g_has_amulet) {
        uint8_t x, y;
        if (free_spot(&g_rooms[stairs_room], &x, &y))
            item_place(IK_AMULET, 0, x, y);
    }
}
