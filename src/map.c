#include "map.h"
#include "save.h"

uint8_t g_map[MAP_H][MAP_W];
uint8_t g_explored[MAP_H][EXPLORED_STRIDE];
room_t  g_rooms[MAX_ROOMS];
uint8_t g_room_count;

void map_clear(void) {
    uint8_t x, y;
    for (y = 0; y < MAP_H; y++) {
        for (x = 0; x < MAP_W; x++) g_map[y][x] = TI_BLANK;
        for (x = 0; x < EXPLORED_STRIDE; x++) g_explored[y][x] = 0;
    }
    g_room_count = 0;
    /* New floor: the static map is completely rewritten, so the next save
       must flush it (also guarantees the very first save writes STATIC). */
    save_mark_map_dirty();
}

uint8_t map_cell(uint8_t x, uint8_t y) {
    if (x >= MAP_W || y >= MAP_H) return TI_BLANK;
    return g_map[y][x];
}

tile_id_t map_terrain(uint8_t x, uint8_t y) {
    return (tile_id_t)(map_cell(x, y) & MF_TERRAIN);
}

uint8_t map_is_explored(uint8_t x, uint8_t y) {
    if (x >= MAP_W || y >= MAP_H) return 0;
    return (uint8_t)((g_explored[y][x >> 3] >> (x & 7u)) & 1u);
}

void map_set_explored(uint8_t x, uint8_t y) {
    if (x >= MAP_W || y >= MAP_H) return;
    g_explored[y][x >> 3] |= (uint8_t)(1u << (x & 7u));
}

void map_set_terrain(uint8_t x, uint8_t y, tile_id_t t) {
    if (x >= MAP_W || y >= MAP_H) return;
    g_map[y][x] = (uint8_t)((g_map[y][x] & ~MF_TERRAIN) | t);
    save_mark_map_dirty();                 /* digging changes the static map */
}

void map_set_flag(uint8_t x, uint8_t y, uint8_t flag) {
    if (x >= MAP_W || y >= MAP_H) return;
    g_map[y][x] |= flag;
    if (flag & MF_HIDDEN) save_mark_map_dirty();   /* trap revealed */
}

void map_clear_flag(uint8_t x, uint8_t y, uint8_t flag) {
    if (x >= MAP_W || y >= MAP_H) return;
    g_map[y][x] &= (uint8_t)~flag;
    if (flag & MF_HIDDEN) save_mark_map_dirty();   /* trap revealed */
}

uint8_t map_walkable(uint8_t x, uint8_t y) {
    switch (map_terrain(x, y)) {
    case TI_FLOOR:
    case TI_CORRIDOR:
    case TI_DOOR:
    case TI_STAIRS_DOWN:
    case TI_STAIRS_UP:
    case TI_TRAP:
        return 1;
    default:
        return 0;
    }
}

uint8_t map_diag_ok(uint8_t fx, uint8_t fy, uint8_t tx, uint8_t ty) {
    if (map_terrain(fx, fy) == TI_DOOR || map_terrain(tx, ty) == TI_DOOR)
        return 0;
    /* No corner cutting: BOTH orthogonal cells must be open (Rogue).
       One blocked side is enough to forbid it — otherwise you can
       slip diagonally around corridor elbows. */
    if (!map_walkable(tx, fy) || !map_walkable(fx, ty))
        return 0;
    return 1;
}

uint8_t map_room_at(uint8_t x, uint8_t y) {
    uint8_t i;
    for (i = 0; i < g_room_count; i++) {
        const room_t *r = &g_rooms[i];
        if (r->flags & ROOM_GONE) continue;
        if (x >= r->x && x < (uint8_t)(r->x + r->w) &&
            y >= r->y && y < (uint8_t)(r->y + r->h)) {
            return i;
        }
    }
    return 0xFFu;
}
