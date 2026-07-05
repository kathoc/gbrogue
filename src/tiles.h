#ifndef TILES_H
#define TILES_H

#include <stdint.h>

/*
 * Internal tile / thing IDs. The game logic only ever sees these.
 * Display glyphs (ASCII now, 8x8 art later) are resolved by the active
 * tileset in assets/ — never inline in game code.
 */
typedef uint8_t tile_id_t;

enum {
    TI_BLANK = 0,       /* unexplored / solid rock */
    TI_FLOOR,           /* room floor        '.' */
    TI_CORRIDOR,        /* passage           '#' */
    TI_WALL_H,          /* horizontal wall   '-' */
    TI_WALL_V,          /* vertical wall     '|' */
    TI_DOOR,            /* doorway           '+' */
    TI_STAIRS_DOWN,     /* down staircase    '>' */
    TI_STAIRS_UP,       /* up staircase      '<' */
    TI_TRAP,            /* discovered trap   '^' */
    TI_GOLD,            /* gold pile         '*' */
    TI_FOOD,            /* food ration       '%' */
    TI_POTION,          /* potion            '!' */
    TI_SCROLL,          /* scroll            '?' */
    TI_WAND,            /* wand / staff      '/' */
    TI_RING,            /* ring              '=' */
    TI_WEAPON,          /* weapon            ')' */
    TI_ARMOR,           /* armor             ']' */
    TI_AMULET,          /* the Amulet        ',' */
    TI_PLAYER,          /* the hero          '@' */
    TI_COUNT
};

/* Resolved by the active tileset (assets/tiles_ascii.c). */
char tile_glyph(tile_id_t id);

#endif
