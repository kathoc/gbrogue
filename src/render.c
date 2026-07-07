#include <gb/gb.h>
#include <gb/cgb.h>
#include "render.h"
#include "input.h"
#include "text4.h"
#include "farcopy.h"
#include "status.h"

/*
 * Shadow-buffer renderer.
 *
 * Every API call is a pure WRAM write into g_screen plus a dirty-row
 * bit. render_present() copies each dirty row to BG VRAM with one
 * set_bkg_tiles(0, y, 20, 1, row) call. Client code never touches VRAM
 * directly, so PPU timing never matters at a call site.
 *
 * VRAM tiles 0..94 hold the ASCII atlas; the tile index for char c is
 * c - 0x20. tests/gbtest.py relies on this to decode the BG map back
 * into text, so keep the mapping stable.
 */

BANKREF_EXTERN(tiles_ascii_data)
BANKREF_EXTERN(tiles_gfx_data)
extern const uint8_t tiles_ascii_data[];   /* ROM bank 2 */
extern const uint8_t tiles_gfx_data[];     /* ROM bank 2 */
extern const uint8_t TILE_GFX_COUNT;       /* home */
extern const uint8_t TILE_GFX_INDEX[];     /* home */

/* VRAM tile layout:
     0..44   full-width glyphs (map symbols, A..Z, '@'; gen_font.py)
     45..62  graphic tileset (gen_gfx.py)
     63..255 composed half-width text pairs (text4.c)
   The harness decodes screens through the ROM tables TILE8_CHARS /
   g_t4_keys, so keep this layout in sync with text4.h / gbtest.py. */
#define TILES8_COUNT 45u
#define GFX_BASE     TILES8_COUNT

extern const uint8_t TILE8_CHARS[];
extern const uint8_t CHAR_TO_TILE8[];

/* 0 = ASCII glyphs, 1 = graphic tiles (UI text stays ASCII). */
uint8_t g_render_mode;

/* 1 when running on Game Boy Color hardware. */
uint8_t g_is_gbc;

/*
 * GBC dark theme — a designed palette set, NOT a luminance inversion:
 * ink is warm parchment on a blue-tinted near-black, walls glow amber,
 * items aqua, exits green; the player is bright white-green, monsters
 * ember-red. Entry order per palette: 0 = background, 1..3 = shades
 * (glyph strokes are drawn at color 3).
 */
#define C_BLACK RGB(1, 1, 3)

/* BG palettes: 0 text/floor, 1 walls+doors, 2 items, 3 stairs,
   4 damage-taken flash (red bg), 5 damage-dealt flash (yellow bg) */
static const palette_color_t BG_PALS[6 * 4] = {
    C_BLACK, RGB( 7,  7,  9), RGB(15, 15, 16), RGB(27, 26, 22),
    C_BLACK, RGB(10,  6,  2), RGB(18, 12,  4), RGB(28, 19,  6),
    C_BLACK, RGB( 4, 10, 10), RGB( 8, 18, 18), RGB(12, 28, 26),
    C_BLACK, RGB( 4, 10,  4), RGB( 8, 18,  8), RGB(14, 28, 12),
    RGB(20, 3, 2), RGB(24, 8, 5), RGB(28, 12, 8), RGB(31, 28, 24),
    RGB(24, 19, 3), RGB(27, 22, 5), RGB(30, 25, 8), RGB(6, 5, 2),
};

/* OBJ palettes: 0 player, 1 monsters (entry 0 is transparent). */
static const palette_color_t OBJ_PALS[2 * 4] = {
    C_BLACK, RGB( 8, 12,  8), RGB(18, 24, 18), RGB(30, 31, 28),
    C_BLACK, RGB(12,  4,  2), RGB(22,  8,  4), RGB(31, 14,  8),
};

/* BG attribute (palette index) for a VRAM tile. Glyph tiles classify
   by their ASCII char; GFX atlas tiles by their slot; composed
   half-width text tiles are always ink-colored. */
static uint8_t attr_for_tile(uint8_t t) {
    if (t >= MSGROW_BASE) return 0;    /* message row, pool text, raw */
    if (t >= GFX_BASE) {
        uint8_t g = (uint8_t)(t - GFX_BASE);
        if (g >= 2u && g <= 4u) return 1;      /* walls, door */
        if (g == 5u || g == 6u) return 3;      /* stairs */
        if (g <= 1u || g == 17u) return 0;     /* floor, corridor, player */
        return 2;                              /* trap, items, amulet */
    }
    switch ((char)TILE8_CHARS[t]) {
    case '-': case '|': case '+':
        return 1;
    case '>': case '<':
        return 3;
    case '!': case '?': case '/': case '=': case ')': case ']':
    case '%': case '*': case ',': case '^':
        return 2;
    default:
        return 0;
    }
}

/* Art screens: every cell renders through BG palette 0 (ink on the
   theme background) regardless of which VRAM slot it occupies. */
static uint8_t g_attr_flat;

/* Set by render_set_world(0): the next overlay present() performs the
   atomic world->overlay swap (blank, scroll-snap, flush, unblank). */
static uint8_t g_overlay_enter;

/* Mirror a row of tiles into the GBC attribute map (VRAM bank 1). */
static void flush_row_attrs(uint8_t y, const uint8_t *tiles, uint8_t w,
                            uint8_t to_window) {
    uint8_t attrs[WORLD_W];
    uint8_t i;
    if (!g_is_gbc) return;
    if (g_attr_flat) {
        for (i = 0; i < w; i++) attrs[i] = 0;
    } else {
        for (i = 0; i < w; i++) attrs[i] = attr_for_tile(tiles[i]);
    }
    VBK_REG = 1;
    if (to_window) set_win_tiles(0, y, w, 1, attrs);
    else set_bkg_tiles(0, y, w, 1, attrs);
    VBK_REG = 0;
}

static uint8_t  g_screen[SCREEN_H][SCREEN_W];
static uint32_t g_dirty_rows;        /* bit y = row y dirty */

/* World mode state (see render.h). The world shadow mirrors the whole
   level in the BG map; the window shadow pins rows 16/17. */
static uint8_t  g_world_on;
static uint8_t  g_world[WORLD_H][WORLD_W];
static uint32_t g_world_dirty;       /* bit y = world row y dirty */
#define WIN_ROWS 2u
static uint8_t  g_win[WIN_ROWS][SCREEN_W];
static uint8_t  g_win_dirty;
#define N_SPRITES 14u                /* player + MAX_MONSTERS + aim cursor */

static uint8_t tile_for_char(char c) {
    uint8_t u = (uint8_t)c;
    uint8_t t;
    if (u < 0x20u || u > 0x7Eu) return 0;
    t = CHAR_TO_TILE8[u - 0x20u];
    if (t == 0xFFu) t = CHAR_TO_TILE8['?' - 0x20u];
    return t;
}

static void set_cell(uint8_t x, uint8_t y, uint8_t tile) {
    if (x >= SCREEN_W || y >= SCREEN_H) return;
    if (g_screen[y][x] == tile) return;
    g_screen[y][x] = tile;
    g_dirty_rows |= (1UL << y);
}

/* Resolve an internal tile id to a VRAM tile index for the active
   tileset (shared by overlay cells, world cells and sprites). */
static uint8_t tile_index_for(tile_id_t id) {
    if (g_render_mode && id < TI_COUNT) {
        uint8_t gi = TILE_GFX_INDEX[id];
        if (gi != 0xFFu) return (uint8_t)(GFX_BASE + gi);
    }
    return tile_for_char(tile_glyph(id));
}

static void set_world_cell(uint8_t x, uint8_t y, uint8_t tile) {
    if (x >= WORLD_W || y >= WORLD_H) return;
    if (g_world[y][x] == tile) return;
    g_world[y][x] = tile;
    g_world_dirty |= (1UL << y);
}

static void set_win_cell(uint8_t x, uint8_t y, uint8_t tile) {
    if (x >= SCREEN_W || y >= WIN_ROWS) return;
    if (g_win[y][x] == tile) return;
    g_win[y][x] = tile;
    g_win_dirty |= (uint8_t)(1u << y);
}

static void hide_all_sprites(void) {
    uint8_t i;
    for (i = 0; i < N_SPRITES; i++) move_sprite(i, 0, 0);
}

/* Message-band machinery (bodies further down). */
#define HUD_KEEP 42u
static char last_status[HUD_KEEP + 1];
char g_last_msg[HUD_KEEP + 1];
static uint8_t msg_bmp[MSGROW_TILES * 8u];
static uint8_t msg_new[MSGROW_TILES * 8u];
static uint8_t msg_shift;      /* 8 = settled, <8 = slide in flight */
static uint8_t msg_armed;      /* target composed, start deferred */
static void msgrow_upload_mix(uint8_t shift);
static void msgrow_commit(void);

/* Copy tiles out of another ROM bank into VRAM, one tile at a time
   through the WRAM far-copy trampoline (see farcopy.c). */
static void load_banked_tiles(uint8_t bank, const uint8_t *src,
                              uint8_t first, uint8_t count) {
    uint8_t buf[16];
    uint8_t t;
    for (t = 0; t < count; t++) {
        far_copy(bank, src + (uint16_t)t * 16u, buf, 16);
        set_bkg_data((uint8_t)(first + t), 1, buf);
    }
}

void render_init(void) {
    uint8_t x, y;
    DISPLAY_ON;
    SHOW_BKG;
    SHOW_SPRITES;
    /* DMG leaves OBP0 undefined; match the BG ramp or sprites render
       invisible (white-on-white). */
    BGP_REG = 0xE4u;
    OBP0_REG = 0xE4u;
    g_is_gbc = (uint8_t)(_cpu == CGB_TYPE);
    if (g_is_gbc) {
        set_bkg_palette(0, 6, BG_PALS);
        set_sprite_palette(0, 2, OBJ_PALS);
    }
    /* DMG flash ramp: inverted, so a flashing sprite pops. */
    OBP1_REG = 0x1Bu;
    /* BG must use 0x8000 unsigned tile addressing so BG and OBJ share
       one tile table — sprites show the same glyph tiles the BG uses. */
    LCDC_REG |= LCDCF_BG8000;
    load_banked_tiles(BANK(tiles_ascii_data), tiles_ascii_data,
                      0, TILES8_COUNT);
    load_banked_tiles(BANK(tiles_gfx_data), tiles_gfx_data,
                      GFX_BASE, TILE_GFX_COUNT);
    /* Window: bottom two rows (msg + status), map at 0x9C00. Hidden
       until world mode turns on. */
    LCDC_REG |= LCDCF_WIN9C00;
    WX_REG = 7u;                        /* biased: 7 = screen x 0 */
    WY_REG = 128u;                      /* 16 tile rows down */
    hide_all_sprites();
    for (y = 0; y < SCREEN_H; y++)
        for (x = 0; x < SCREEN_W; x++) g_screen[y][x] = 0;
    g_dirty_rows = 0xFFFFFFFFUL;
    g_world_on = 0;
    msg_shift = 8;
    SCX_REG = 0;
    SCY_REG = 0;
    render_present();
}

/*
 * Palette fades. Level k = 0 (blanked out) .. 8 (full). GBC scales
 * every palette entry toward black (the dark theme's floor); DMG steps
 * the BGP/OBP0 ramp toward all-white. Everything else (tilemap, OAM,
 * WRAM state) is untouched, so loading can happen "behind" a fade.
 */
static const uint8_t DMG_FADE[4] = { 0x00u, 0x40u, 0x90u, 0xE4u };
static uint8_t fade_k = 8;

static void scale_pals(const palette_color_t *src, palette_color_t *dst,
                       uint8_t n, uint8_t k) {
    uint8_t i;
    for (i = 0; i < n; i++) {
        palette_color_t c = src[i];
        dst[i] = RGB(((c & 0x1Fu) * k) >> 3,
                     (((c >> 5) & 0x1Fu) * k) >> 3,
                     (((c >> 10) & 0x1Fu) * k) >> 3);
    }
}

static void fade_apply(uint8_t k) {
    if (k == fade_k) return;
    fade_k = k;
    if (g_is_gbc) {
        palette_color_t buf[6 * 4];
        scale_pals(BG_PALS, buf, 6u * 4u, k);
        set_bkg_palette(0, 6, buf);
        scale_pals(OBJ_PALS, buf, 2u * 4u, k);
        set_sprite_palette(0, 2, buf);
    } else {
        uint8_t v = DMG_FADE[(uint8_t)((k * 3u + 4u) / 8u)];
        BGP_REG = v;
        OBP0_REG = v;
    }
}

/* Ramp the palette level 0..8 across `frames`; `up` picks direction.
   One body serves both public entry points (saves HOME). */
static void fade_ramp(uint8_t frames, uint8_t up) {
    uint8_t f;
    for (f = 0; f < frames; f++) {
        uint8_t g = up ? (uint8_t)(f + 1u) : (uint8_t)(frames - 1u - f);
        wait_vbl_done();
        input_tick();
        fade_apply((uint8_t)(((uint16_t)g * 8u) / frames));
    }
}

void render_fade_out(uint8_t frames) { fade_ramp(frames, 0); }
void render_fade_in(uint8_t frames)  { fade_ramp(frames, 1); }

uint8_t render_faded_out(void) { return (uint8_t)(fade_k == 0u); }

/* One palette entry for the death wipe: bend it toward pure red (R->31,
   G/B->0 as `redness` goes 0..256), then dim the whole colour toward
   black (as `dim` goes 256..0). Two staged >>8 steps keep the maths in
   uint16 range. */
static palette_color_t death_mix(palette_color_t c, uint16_t redness,
                                 uint16_t dim) {
    uint8_t r = (uint8_t)(c & 0x1Fu);
    uint8_t g = (uint8_t)((c >> 5) & 0x1Fu);
    uint8_t b = (uint8_t)((c >> 10) & 0x1Fu);
    r = (uint8_t)(r + (((uint16_t)(31u - r) * redness) >> 8));
    g = (uint8_t)(((uint16_t)g * (256u - redness)) >> 8);
    b = (uint8_t)(((uint16_t)b * (256u - redness)) >> 8);
    r = (uint8_t)(((uint16_t)r * dim) >> 8);
    g = (uint8_t)(((uint16_t)g * dim) >> 8);
    b = (uint8_t)(((uint16_t)b * dim) >> 8);
    return RGB(r, g, b);
}

void render_death_to_red(uint8_t frames) {
    uint8_t f;
    if (!g_is_gbc || !frames) return;
    for (f = 0; f < frames; f++) {
        palette_color_t bg[6 * 4];
        palette_color_t ob[2 * 4];
        uint16_t p = (uint16_t)(((uint16_t)(f + 1u) * 256u) / frames); /* 0..256 */
        uint16_t redness = (uint16_t)(((uint32_t)p * 4u) / 3u);        /* full red by 75% */
        uint16_t dim;
        uint8_t i;
        if (redness > 256u) redness = 256u;
        /* the final quarter dims the pure red down to black */
        dim = (p <= 192u) ? 256u : (uint16_t)(((256u - p) * 256u) / 64u);
        for (i = 0; i < 6u * 4u; i++) bg[i] = death_mix(BG_PALS[i], redness, dim);
        for (i = 0; i < 2u * 4u; i++) ob[i] = death_mix(OBJ_PALS[i], redness, dim);
        wait_vbl_done();
        input_tick();
        render_msg_tick();          /* keep the fatal line sliding under the red */
        set_bkg_palette(0, 6, bg);
        set_sprite_palette(0, 2, ob);
    }
    fade_k = 0;                     /* screen is black; keep the fader in sync */
}

void render_art_begin(void) {
    g_attr_flat = 1;
}

void render_art_end(void) {
    g_attr_flat = 0;
    /* The art clobbered the glyph atlas + GFX tiles and the top of the
       composed-text pool: reload the tilesets and drop the pool cache
       so no cached pair resolves to an art tile. (The message-row
       tiles re-stream on the next render_set_world(1).) */
    load_banked_tiles(BANK(tiles_ascii_data), tiles_ascii_data,
                      0, TILES8_COUNT);
    load_banked_tiles(BANK(tiles_gfx_data), tiles_gfx_data,
                      GFX_BASE, TILE_GFX_COUNT);
    t4_reset();
}

void render_set_world(uint8_t on) {
    if (on) {
        uint8_t x;
        g_world_on = 1;
        /* Re-entering the world reflushes every tile below; the status
           row's composed tiles were recycled (t4_reset) while the overlay
           owned the pool, so force the next status_update to repaint even
           if no value changed since we left. */
        status_invalidate();
        /* The message band uses its 20 fixed tiles; pin the map ids
           and re-stream the current line's pixels. */
        for (x = 0; x < MSGROW_TILES; x++)
            set_win_cell(x, 0, (uint8_t)(MSGROW_BASE + x));
        t4_line_bitmap(g_last_msg, msg_new);
        msgrow_upload_mix(8);
        msgrow_commit();
        msg_shift = 8;
        msg_armed = 0;
        /* Force a full mismatch-proof reflush of world + window. */
        g_world_dirty = 0xFFFFFFFFUL;
        g_win_dirty = 0xFFu;
        SHOW_WIN;
    } else {
        /* Keep the world map + sprites on screen while the caller
           composes the overlay text — composition can span many frames
           (banked far-copies per glyph) and must not flash. Only pull
           the HUD window, whose pool tiles get recycled mid-compose
           (that garble was the "mess before the menu opens"). The
           scroll snap + full BG rewrite is deferred to the first
           overlay present() and done under a briefly blanked LCD so it
           is atomic — one snap, no glitch, no long white flash. */
        g_world_on = 0;
        HIDE_WIN;
        g_overlay_enter = 1;
        /* BG VRAM currently holds world rows: overlay must rewrite
           every row on the next present regardless of the dedup. */
        g_dirty_rows = 0xFFFFFFFFUL;
    }
}

uint8_t render_world_on(void) {
    return g_world_on;
}

void render_world_cell(uint8_t wx, uint8_t wy, tile_id_t id) {
    set_world_cell(wx, wy, tile_index_for(id));
}

void render_scroll(uint8_t x, uint8_t y) {
    SCX_REG = x;
    SCY_REG = y;
}

static void sprite_colorize(uint8_t i) {
    if (g_is_gbc)
        set_sprite_prop(i, i == SPR_PLAYER ? 0u : 1u);
}

/* ---------------------------------------------------- aiming cursor
 * An 8x8 arrow overlaid on the player's tile while aiming a throw/zap.
 * The eight direction glyphs (baked from samples/*cursor.png) are loaded
 * on demand into the reserved cursor tile; only a change of direction
 * touches VRAM, so blinking is a pure OAM show/hide. */
#define CURSOR_TILE 255u
/* Bold, tile-filling arrows so the aim direction reads at a glance (the
   old few-pixel chevrons were nearly invisible). Cardinals are solid
   arrows; diagonals are solid corner wedges pointing that way. */
static const uint8_t CURSOR_ARROWS[8][8] = {
    { 0x18, 0x3C, 0x7E, 0xFF, 0x18, 0x18, 0x18, 0x00 },  /* UP */
    { 0x00, 0x18, 0x18, 0x18, 0xFF, 0x7E, 0x3C, 0x18 },  /* DN */
    { 0x00, 0x10, 0x30, 0x7F, 0xFF, 0x7F, 0x30, 0x10 },  /* LT */
    { 0x00, 0x08, 0x0C, 0xFE, 0xFF, 0xFE, 0x0C, 0x08 },  /* RT */
    { 0xFE, 0xFC, 0xF8, 0xF0, 0xE0, 0xC0, 0x80, 0x00 },  /* UL */
    { 0x7F, 0x3F, 0x1F, 0x0F, 0x07, 0x03, 0x01, 0x00 },  /* UR */
    { 0x00, 0x80, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC, 0xFE },  /* DL */
    { 0x00, 0x01, 0x03, 0x07, 0x0F, 0x1F, 0x3F, 0x7F },  /* DR */
};
static uint8_t aim_last_dir = 0xFFu;

void render_aim_cursor(uint8_t dir, uint8_t sx, uint8_t sy) {
    if (dir != aim_last_dir) {
        uint8_t buf[16], r;
        for (r = 0; r < 8u; r++) {         /* 1bpp glyph -> both planes */
            buf[r * 2u] = CURSOR_ARROWS[dir][r];
            buf[r * 2u + 1u] = CURSOR_ARROWS[dir][r];
        }
        set_bkg_data(CURSOR_TILE, 1, buf);
        aim_last_dir = dir;
    }
    set_sprite_tile(SPR_CURSOR, CURSOR_TILE);
    if (g_is_gbc) set_sprite_prop(SPR_CURSOR, 0u);   /* player palette */
    render_sprite_pos(SPR_CURSOR, sx, sy);
}

void render_aim_hide(void) {
    render_sprite_hide(SPR_CURSOR);
}

void render_sprite_glyph(uint8_t i, char c) {
    set_sprite_tile(i, tile_for_char(c));
    sprite_colorize(i);
}

void render_sprite_id(uint8_t i, tile_id_t id) {
    set_sprite_tile(i, tile_index_for(id));
    sprite_colorize(i);
}

void render_sprite_pos(uint8_t i, uint8_t sx, uint8_t sy) {
    /* OAM bias: (8,16) puts the sprite at screen origin. */
    move_sprite(i, (uint8_t)(sx + 8u), (uint8_t)(sy + 16u));
}

void render_sprite_hide(uint8_t i) {
    move_sprite(i, 0, 0);
}

/* ------------------------------------------------------ damage flash */

/* g_flashq / g_flash_n / render_flash_add live in bank0_flash.c (fixed
   bank); the queue type + externs are in render.h. */

static void flash_cell_attr(uint8_t x, uint8_t y, uint8_t attr) {
    if (!g_is_gbc) return;
    VBK_REG = 1;
    set_bkg_tiles(x, y, 1, 1, &attr);
    VBK_REG = 0;
}

/* Tile-column span of the HP readout on the status row (set by
   status_update so the red damage blink can hit the numbers too). */
uint8_t g_hp_col0, g_hp_col1;

/* Paint one attr across the HP columns of the window status row. */
static void hp_field_attr(uint8_t attr) {
    uint8_t attrs[12];
    uint8_t n, i;
    if (!g_is_gbc || g_hp_col1 < g_hp_col0) return;
    n = (uint8_t)(g_hp_col1 - g_hp_col0 + 1u);
    if (n > 12u) n = 12u;
    for (i = 0; i < n; i++) attrs[i] = attr;
    VBK_REG = 1;
    set_win_tiles(g_hp_col0, 1, n, 1, attrs);
    VBK_REG = 0;
}

void render_flash_play(uint8_t slow) {
    uint8_t c, i, f;
    uint8_t hurt = 0;
    if (!slow) slow = 1u;
    if (!g_flash_n || !g_world_on) {
        g_flash_n = 0;
        return;
    }
    for (i = 0; i < g_flash_n; i++)
        if (g_flashq[i].style == FLASH_HURT) hurt = 1;
    for (c = 0; c < 2u; c++) {
        for (i = 0; i < g_flash_n; i++) {
            flash_cell_attr(g_flashq[i].x, g_flashq[i].y,
                            (uint8_t)(4u + g_flashq[i].style));
            if (!g_is_gbc && g_flashq[i].spr != 0xFFu)
                set_sprite_prop(g_flashq[i].spr, S_PALETTE);
        }
        if (hurt) hp_field_attr(4u);   /* the HP readout blinks red too */
        for (f = 0; f < (uint8_t)(4u * slow); f++) {
            wait_vbl_done();
            input_tick();
        }
        for (i = 0; i < g_flash_n; i++) {
            flash_cell_attr(g_flashq[i].x, g_flashq[i].y,
                            attr_for_tile(g_world[g_flashq[i].y][g_flashq[i].x]));
            if (!g_is_gbc && g_flashq[i].spr != 0xFFu)
                set_sprite_prop(g_flashq[i].spr, 0);
        }
        if (hurt) hp_field_attr(0u);
        for (f = 0; f < (uint8_t)(3u * slow); f++) {
            wait_vbl_done();
            input_tick();
        }
    }
    g_flash_n = 0;
}

/* A whole-screen red danger pulse, played once per turn while the
   player is starving. GBC slams every BG palette to the damage-red
   set for a few frames; DMG has no color, so it darkens the screen
   instead. Either way the live palette is restored, so nothing stays
   tinted. Kept short (~3F lit) so back-to-back moves still read. */
void render_danger_flash(void) {
    uint8_t f;
    if (!g_world_on) return;
    if (g_is_gbc) {
        palette_color_t red[6 * 4];
        uint8_t i;
        for (i = 0; i < 6u * 4u; i++)
            red[i] = BG_PALS[4u * 4u + (i & 3u)];  /* palette 4: red bg */
        set_bkg_palette(0, 6, red);
        for (f = 0; f < 3u; f++) {
            wait_vbl_done();
            input_tick();
            render_msg_tick();
        }
        set_bkg_palette(0, 6, BG_PALS);            /* back to the theme */
    } else {
        uint8_t save = BGP_REG;
        BGP_REG = 0xFFu;                           /* every shade darkest */
        for (f = 0; f < 3u; f++) {
            wait_vbl_done();
            input_tick();
            render_msg_tick();
        }
        BGP_REG = save;
    }
}

void render_toggle_mode(void) {
    g_render_mode ^= 1u;
}

void render_tile(uint8_t x, uint8_t y, tile_id_t id) {
    set_cell(x, y, tile_index_for(id));
}

void render_glyph(uint8_t x, uint8_t y, char c) {
    set_cell(x, y, tile_for_char(c));
}

void render_cell_tile(uint8_t x, uint8_t y, uint8_t tile) {
    set_cell(x, y, tile);
}

/* One text cell: two half-width ASCII chars or one full-width kana.
   Advances *ps; returns the tile. */
static uint8_t text_cell(const char **ps) {
    const char *s = *ps;
    uint8_t b0 = (uint8_t)*s;
    if (T4_IS_FULL(b0)) {
        *ps = s + 1;
        return t4_full(b0);
    }
    {
        char a = *s++;
        char b = ' ';
        if (*s && !T4_IS_FULL(*s)) {
            b = *s++;
        }
        *ps = s;
        return t4_pair(a, b);
    }
}

/* UI text: half-width ASCII (2/tile) mixed with full-width kana. */
void render_text(uint8_t x, uint8_t y, const char *s) {
    while (*s && x < SCREEN_W) set_cell(x++, y, text_cell(&s));
}

void render_clear_view(void) {
    uint8_t x, y;
    for (y = 0; y < VIEW_H; y++)
        for (x = 0; x < SCREEN_W; x++) set_cell(x, y, 0);
}

void render_clear_all(void) {
    uint8_t x, y;
    for (y = 0; y < SCREEN_H; y++)
        for (x = 0; x < SCREEN_W; x++) set_cell(x, y, 0);
    /* Every modal starts from a cleared screen: recycle the composed
       text tiles so long sessions never leak the pool. */
    t4_reset();
}

static void draw_row_padded(uint8_t y, const char *s) {
    uint8_t x = 0;
    while (*s && x < SCREEN_W) set_cell(x++, y, text_cell(&s));
    while (x < SCREEN_W) set_cell(x++, y, 0);
}

void render_row(uint8_t y, const char *s) {
    draw_row_padded(y, s);
}

static void win_draw_row(uint8_t wy, const char *s) {
    uint8_t x = 0;
    if (s) {
        while (*s && x < SCREEN_W) set_win_cell(x++, wy, text_cell(&s));
    }
    while (x < SCREEN_W) set_win_cell(x++, wy, 0);
}

static void hud_keep(char *dst, const char *s) {
    uint8_t i = 0;
    while (s[i] && i < HUD_KEEP) {
        dst[i] = s[i];
        i++;
    }
    dst[i] = 0;
}

void render_status(const char *s) {
    hud_keep(last_status, s);
    if (g_world_on) win_draw_row(1, s);
    else draw_row_padded(ROW_STAT, s);
}

/*
 * World-mode message band: 20 dedicated VRAM tiles (MSGROW_BASE..)
 * whose pixel data is streamed from a WRAM line bitmap. A new message
 * pushes in from below: 8 one-pixel steps through a 16px virtual strip
 * of old-line-above / new-line-below.
 */
static void msgrow_upload_mix(uint8_t shift) {
    uint8_t tile[16];
    uint8_t c, r;
    for (c = 0; c < MSGROW_TILES; c++) {
        const uint8_t *o = msg_bmp + (uint16_t)c * 8u;
        const uint8_t *n = msg_new + (uint16_t)c * 8u;
        for (r = 0; r < 8u; r++) {
            uint8_t v = (uint8_t)(shift + r);
            uint8_t b = (v < 8u) ? o[v] : n[v - 8u];
            tile[r * 2u] = b;
            tile[r * 2u + 1u] = b;
        }
        set_bkg_data((uint8_t)(MSGROW_BASE + c), 1, tile);
    }
}

static void msgrow_commit(void) {
    uint8_t i;
    for (i = 0; i < sizeof(msg_bmp); i++) msg_bmp[i] = msg_new[i];
}

static uint8_t same_msg(const char *s) {
    uint8_t i = 0;
    while (g_last_msg[i] && s[i] && g_last_msg[i] == s[i]) i++;
    return g_last_msg[i] == s[i];
}

void render_msg_begin(void) {
    if (msg_armed) {
        msg_armed = 0;
        msg_shift = 0;
    }
}

void render_msg_tick(void) {
    if (!g_world_on || msg_shift >= 8u) return;
    msg_shift += 2u;                         /* 4 frames per slide */
    msgrow_upload_mix(msg_shift);
    if (msg_shift >= 8u) msgrow_commit();
}

void render_message(const char *s) {
    if (g_world_on) {
        uint8_t had = g_last_msg[0];
        if (s && same_msg(s) && !msg_armed && msg_shift >= 8u)
            return;                          /* refresh: nothing new */
        if (msg_shift < 8u) {
            /* a slide is mid-flight: snap it before retargeting */
            msg_shift = 8;
            msgrow_upload_mix(8);
            msgrow_commit();
        }
        t4_line_bitmap(s ? s : "", msg_new);
        if (s == 0) g_last_msg[0] = 0;
        else hud_keep(g_last_msg, s);
        if (s && had) {
            msg_armed = 1;                   /* slides after the turn */
        } else {
            msgrow_upload_mix(8);            /* first line / clear */
            msgrow_commit();
            msg_armed = 0;
        }
        return;
    }
    if (s == 0) g_last_msg[0] = 0;
    else hud_keep(g_last_msg, s);
    if (s == 0) {
        uint8_t x;
        for (x = 0; x < SCREEN_W; x++) set_cell(x, ROW_MSG, 0);
        return;
    }
    draw_row_padded(ROW_MSG, s);
}

void render_present(void) {
    uint8_t y;
    if (g_world_on) {
        if (g_t4_flushed) {
            /* Composed-tile pool wrapped: the status row may reference
               recycled slots — recompose it (the message band lives on
               its own fixed tiles and is immune). */
            g_t4_flushed = 0;
            win_draw_row(1, last_status);
        }
        if (g_world_dirty) {
            for (y = 0; y < WORLD_H; y++) {
                if (g_world_dirty & (1UL << y)) {
                    flush_row_attrs(y, g_world[y], WORLD_W, 0);
                    set_bkg_tiles(0, y, WORLD_W, 1, g_world[y]);
                    input_tick();   /* full reflushes span frames */
                }
            }
            g_world_dirty = 0;
        }
        if (g_win_dirty) {
            for (y = 0; y < WIN_ROWS; y++) {
                if (g_win_dirty & (uint8_t)(1u << y)) {
                    flush_row_attrs(y, g_win[y], SCREEN_W, 1);
                    set_win_tiles(0, y, SCREEN_W, 1, g_win[y]);
                }
            }
            g_win_dirty = 0;
        }
        return;
    }
    /* First present after a world->overlay swap: the overlay shadow is
       now fully composed. Blank the LCD, snap the scroll to 0, drop the
       world sprites and flush every row — all invisible — then light
       back up on the finished overlay. Subsequent presents (cursor
       moves etc.) just flush the touched rows with the LCD on. */
    if (g_overlay_enter) {
        DISPLAY_OFF;
        hide_all_sprites();
        SCX_REG = 0;
        SCY_REG = 0;
    }
    if (g_dirty_rows) {
        for (y = 0; y < SCREEN_H; y++) {
            if (g_dirty_rows & (1UL << y)) {
                flush_row_attrs(y, g_screen[y], SCREEN_W, 0);
                set_bkg_tiles(0, y, SCREEN_W, 1, g_screen[y]);
            }
        }
        g_dirty_rows = 0;
    }
    if (g_overlay_enter) {
        g_overlay_enter = 0;
        DISPLAY_ON;
    }
}
