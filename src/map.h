#ifndef GBR_MAP_H
#define GBR_MAP_H

#include <stdint.h>
#include "tiles.h"

/* 32x28 so the whole level fits the 32x32 BG tile map: hardware
   scrolling (SCX/SCY) can then pan smoothly with no tile streaming. */
#define MAP_W 32
#define MAP_H 28

/* Cell byte: terrain in the low 5 bits, flags above. */
#define MF_TERRAIN  0x1Fu
#define MF_EXPLORED 0x20u
#define MF_HIDDEN   0x80u   /* trap / secret not yet discovered (M7) */

extern uint8_t g_map[MAP_H][MAP_W];

void      map_clear(void);
uint8_t   map_cell(uint8_t x, uint8_t y);
tile_id_t map_terrain(uint8_t x, uint8_t y);
void      map_set_terrain(uint8_t x, uint8_t y, tile_id_t t);
void      map_set_flag(uint8_t x, uint8_t y, uint8_t flag);
void      map_clear_flag(uint8_t x, uint8_t y, uint8_t flag);
uint8_t   map_walkable(uint8_t x, uint8_t y);
/* Shared diagonal legality for both steps AND strikes, players and
   monsters alike: doorways are strictly orthogonal and wall corners
   cannot be cut. */
uint8_t   map_diag_ok(uint8_t fx, uint8_t fy, uint8_t tx, uint8_t ty);

/* Rooms (filled in by mapgen). flags bit0 = "gone" (corridor junction
   instead of a real room), bit1 = explored. */
#define MAX_ROOMS 9
#define ROOM_GONE     0x01u
#define ROOM_EXPLORED 0x02u
typedef struct {
    uint8_t x, y, w, h;    /* rect including walls */
    uint8_t flags;
} room_t;

extern room_t  g_rooms[MAX_ROOMS];
extern uint8_t g_room_count;

/* Room whose rect (walls included) contains (x,y), else 0xFF. */
uint8_t map_room_at(uint8_t x, uint8_t y);

#endif
