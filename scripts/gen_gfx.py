#!/usr/bin/env python3
"""
Generate the 8x8 graphic tile atlas (assets/tiles_gfx_data.c).

Art shares the internal tile IDs with the ASCII set — the mapping table
TILE_GFX_INDEX is emitted alongside, indexed by tile_id (tiles.h order).
Pixels: '.'=color0, '+'=color1, '#'=color2, '*'=color3.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "assets" / "tiles_gfx_data.c"

# order matters: index in this list == gfx atlas index
TILES = [
    ("floor", [
        "........",
        "........",
        "........",
        "...+....",
        "........",
        "........",
        "........",
        "........",
    ]),
    ("corridor", [
        "..#...#.",
        "........",
        "#...#...",
        "........",
        "..#...#.",
        "........",
        "#...#...",
        "........",
    ]),
    ("wall_h", [
        "********",
        "*..*..*.",
        "********",
        ".*..*..*",
        "********",
        "*..*..*.",
        "********",
        "........",
    ]),
    ("wall_v", [
        ".******.",
        ".*..*.*.",
        ".******.",
        ".*.*..*.",
        ".******.",
        ".*..*.*.",
        ".******.",
        ".*.*..*.",
    ]),
    ("door", [
        "********",
        "*......*",
        "*.****.*",
        "*.*..*.*",
        "*.*..*.*",
        "*.*.**.*",
        "*.****.*",
        "********",
    ]),
    ("stairs_down", [
        "*.......",
        "**......",
        "***.....",
        "****....",
        "*****...",
        "******..",
        "*******.",
        "********",
    ]),
    ("stairs_up", [
        ".......*",
        "......**",
        ".....***",
        "....****",
        "...*****",
        "..******",
        ".*******",
        "********",
    ]),
    ("trap", [
        "........",
        ".*.*.*..",
        ".*.*.*..",
        ".*.*.*..",
        "*******.",
        ".*.*.*..",
        "........",
        "........",
    ]),
    ("gold", [
        "........",
        "..***...",
        ".*###*..",
        "*#***#*.",
        "*#*.*#*.",
        ".*###*..",
        "..***...",
        "........",
    ]),
    ("food", [
        "...*....",
        "..*.....",
        ".****...",
        "*####*..",
        "*####*..",
        "*####*..",
        ".****...",
        "........",
    ]),
    ("potion", [
        "..***...",
        "...*....",
        "...*....",
        "..***...",
        ".*###*..",
        "*#####*.",
        ".*****..",
        "........",
    ]),
    ("scroll", [
        ".*****..",
        "*.....*.",
        "*.***.*.",
        "*.....*.",
        "*.**..*.",
        "*.....*.",
        ".*****..",
        "........",
    ]),
    ("wand", [
        "......*.",
        ".....**.",
        "....**..",
        "...**...",
        "..**....",
        ".**.....",
        "**......",
        "*.......",
    ]),
    ("ring", [
        "........",
        "...*....",
        "..*.*...",
        ".*...*..",
        ".*...*..",
        "..*.*...",
        "...*....",
        "........",
    ]),
    ("weapon", [
        ".....**.",
        "....**..",
        "...**...",
        "..**....",
        "***.....",
        "**......",
        "*.*.....",
        "........",
    ]),
    ("armor", [
        ".*...*..",
        "*******.",
        "*#####*.",
        "*#####*.",
        ".*###*..",
        ".*###*..",
        "..***...",
        "........",
    ]),
    ("amulet", [
        ".*****..",
        "*.....*.",
        "*.....*.",
        ".*...*..",
        "..***...",
        "..*#*...",
        "..***...",
        "........",
    ]),
    ("player", [
        "..***...",
        "..*#*...",
        "..***...",
        ".*****..",
        "*..*..*.",
        "...*....",
        "..*.*...",
        ".*...*..",
    ]),
]

# tiles.h order -> atlas name (None = keep ASCII glyph)
TILE_ID_ORDER = [
    None,           # TI_BLANK
    "floor", "corridor", "wall_h", "wall_v", "door",
    "stairs_down", "stairs_up", "trap", "gold", "food",
    "potion", "scroll", "wand", "ring", "weapon", "armor",
    "amulet", "player",
]

LEVELS = {".": 0, "+": 1, "#": 2, "*": 3}


def tile_bytes(rows):
    out = []
    for row in rows:
        lo = hi = 0
        for i, ch in enumerate(row):
            c = LEVELS[ch]
            if c & 1:
                lo |= 1 << (7 - i)
            if c & 2:
                hi |= 1 << (7 - i)
        out += [lo, hi]
    return out


def main():
    names = [n for n, _ in TILES]
    lines = [
        "/*",
        " * GENERATED FILE — see scripts/gen_gfx.py",
        " *",
        " * 8x8 graphic tiles, ROM bank 2 (render_init loads them via the",
        " * NONBANKED helper).",
        " */",
        "#include <gb/gb.h>",
        "#include <stdint.h>",
        "",
        "#pragma bank 2",
        "",
        "BANKREF(tiles_gfx_data)",
        f"const uint8_t tiles_gfx_data[{len(TILES)} * 16] = {{",
    ]
    for name, rows in TILES:
        bs = tile_bytes(rows)
        hexes = ", ".join(f"0x{b:02X}" for b in bs)
        lines.append(f"    /* {names.index(name):2d} {name:<12}*/ {hexes},")
    lines.append("};")
    lines.append("")
    OUT.write_text("\n".join(lines))

    # index table stays in the home bank (hot per-tile lookups)
    map_out = ROOT / "assets" / "tiles_gfx_map.c"
    idx = []
    for entry in TILE_ID_ORDER:
        idx.append("0xFF" if entry is None else str(names.index(entry)))
    lines = [
        "/*",
        " * GENERATED FILE — see scripts/gen_gfx.py",
        " *",
        " * TILE_GFX_INDEX maps internal tile IDs to gfx atlas slots",
        " * (0xFF = no art, fall back to the glyph). Home bank.",
        " */",
        "#include <stdint.h>",
        "",
        f"const uint8_t TILE_GFX_COUNT = {len(TILES)};",
        "",
        f"const uint8_t TILE_GFX_INDEX[{len(TILE_ID_ORDER)}] = {{",
        "    " + ", ".join(idx) + ",",
        "};",
        "",
    ]
    map_out.write_text("\n".join(lines))
    print(f"wrote {OUT.relative_to(ROOT)} + tiles_gfx_map.c")


if __name__ == "__main__":
    main()
