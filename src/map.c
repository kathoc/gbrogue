#include "map.h"

uint8_t g_map[MAP_H][MAP_W];
room_t  g_rooms[MAX_ROOMS];
uint8_t g_room_count;

void map_clear(void) {
    uint8_t x, y;
    for (y = 0; y < MAP_H; y++)
        for (x = 0; x < MAP_W; x++) g_map[y][x] = TI_BLANK;
    g_room_count = 0;
}

uint8_t map_cell(uint8_t x, uint8_t y) {
    if (x >= MAP_W || y >= MAP_H) return TI_BLANK;
    return g_map[y][x];
}

tile_id_t map_terrain(uint8_t x, uint8_t y) {
    return (tile_id_t)(map_cell(x, y) & MF_TERRAIN);
}

void map_set_terrain(uint8_t x, uint8_t y, tile_id_t t) {
    if (x >= MAP_W || y >= MAP_H) return;
    g_map[y][x] = (uint8_t)((g_map[y][x] & ~MF_TERRAIN) | t);
}

void map_set_flag(uint8_t x, uint8_t y, uint8_t flag) {
    if (x >= MAP_W || y >= MAP_H) return;
    g_map[y][x] |= flag;
}

void map_clear_flag(uint8_t x, uint8_t y, uint8_t flag) {
    if (x >= MAP_W || y >= MAP_H) return;
    g_map[y][x] &= (uint8_t)~flag;
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

uint8_t map_diag_attack_ok(uint8_t fx, uint8_t fy, uint8_t tx, uint8_t ty) {
    /* A diagonal STRIKE is looser than a diagonal STEP: you may hit
       around an open wall corner (so a monster never freezes one
       diagonal away from a cornered target), but never *through a
       doorway* — the original "no diagonal attack from a room into the
       corridor outside a door" rule. Block if any of the four cells
       framing the diagonal is a door. */
    if (map_terrain(fx, fy) == TI_DOOR || map_terrain(tx, ty) == TI_DOOR)
        return 0;
    if (map_terrain(tx, fy) == TI_DOOR || map_terrain(fx, ty) == TI_DOOR)
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
