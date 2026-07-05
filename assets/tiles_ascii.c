#include "tiles.h"

/*
 * ASCII tileset: the single place an internal tile ID becomes a glyph.
 * Swapping to graphics later replaces the lookup, not the callers.
 */
static const char GLYPHS[TI_COUNT] = {
    ' ',   /* TI_BLANK       */
    '.',   /* TI_FLOOR       */
    '#',   /* TI_CORRIDOR    */
    '-',   /* TI_WALL_H      */
    '|',   /* TI_WALL_V      */
    '+',   /* TI_DOOR        */
    '>',   /* TI_STAIRS_DOWN */
    '<',   /* TI_STAIRS_UP   */
    '^',   /* TI_TRAP        */
    '*',   /* TI_GOLD        */
    '%',   /* TI_FOOD        */
    '!',   /* TI_POTION      */
    '?',   /* TI_SCROLL      */
    '/',   /* TI_WAND        */
    '=',   /* TI_RING        */
    ')',   /* TI_WEAPON      */
    ']',   /* TI_ARMOR       */
    ',',   /* TI_AMULET      */
    '@',   /* TI_PLAYER      */
};

char tile_glyph(tile_id_t id) {
    return (id < TI_COUNT) ? GLYPHS[id] : '?';
}
