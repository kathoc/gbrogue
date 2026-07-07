#ifndef ITEMS_H
#define ITEMS_H

#include <stdint.h>

/* Item categories. */
enum {
    IK_FOOD, IK_POTION, IK_SCROLL, IK_WAND, IK_RING,
    IK_WEAPON, IK_ARMOR, IK_GOLD, IK_AMULET,
    IK_COUNT
};

/* Subtype counts mirror Rogue 5.4. */
#define N_POTIONS 14
#define N_SCROLLS 13
#define N_WANDS   14
#define N_RINGS   14
#define N_WEAPONS 9
#define N_ARMORS  8
#define N_FOODS   2

/* WEAPON subtypes (item.sub). The bow (2) is abolished: arrows fire on
   their own. Arrows, darts and shuriken are thrown/fired; the rest are
   melee weapons that get wielded. */
#define WS_ARROW        3u
#define WS_DART         6u
#define WS_SHURIKEN     7u
#define WS_THROWABLE(sub) \
    ((sub) == WS_ARROW || (sub) == WS_DART || (sub) == WS_SHURIKEN)

/* item flags */
#define IF_CURSED       0x01u
#define IF_KNOWN_CURSED 0x02u
#define IF_WORN         0x04u   /* wielded / worn / on finger */
#define IF_IDENT        0x08u   /* ever equipped: ench value is known for good */
#define IF_PARTIAL      0x10u   /* partial-ident: sench is shown, true ench hidden */

typedef struct {
    uint8_t kind;       /* IK_*, IK_COUNT = empty slot */
    uint8_t sub;
    uint8_t x, y;       /* floor position (unused in pack) */
    uint8_t qty;        /* stack count, or gold amount low byte */
    int8_t  ench;       /* +/- enchantment (weapon/armor/ring) */
    int8_t  sench;      /* known enchant shown while IF_PARTIAL */
    uint8_t flags;
} item_t;

#define ITEM_NONE IK_COUNT

#define MAX_FLOOR_ITEMS 24
extern item_t g_floor[MAX_FLOOR_ITEMS];

void    items_clear_floor(void);
item_t *item_floor_at(uint8_t x, uint8_t y);
/* Returns the placed slot or 0 if the floor array is full. */
item_t *item_place(uint8_t kind, uint8_t sub, uint8_t x, uint8_t y);
/* Auto-pickup anything under the player. Handles gold, pack-full. */
void    item_pickup_here(void);

/* Compose the display name of an item into dst (<= 19 chars + NUL),
   honoring the identification state. Returns advanced pointer. */
char   *item_name(char *dst, const item_t *it);

/* Map an item kind to its map tile for floor rendering. */
uint8_t item_tile(uint8_t kind);

/* Weapon / armor ROM stats. */
extern const uint8_t WEAPON_DICE[N_WEAPONS][2];
extern const uint8_t ARMOR_AC[N_ARMORS];

#endif
