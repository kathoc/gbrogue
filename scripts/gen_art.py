#!/usr/bin/env python3
"""
Full-screen art (title / game over) -> assets/art_data.c + src/art_data.h.

Source PNGs (samples/) are dark-ink-on-white drawings. Pixels quantize
to the 4 GB shades with ink at color 3, background at color 0 — the
same convention as the glyph tiles, so DMG shows black-on-white and the
GBC dark theme shows parchment-on-black through BG palette 0 without a
special palette.

Tiles are deduplicated. VRAM placement (mirrored by slot_for() in
src/ui_art.c): unique tile i sits at slot i while i < 83, then fills
255 downward — art never touches the composed-text pool's low slots,
which keeps render_text() usable on top of the art (menus, stats). The
generator fails the build if the art leaves fewer than MIN_POOL free
pool slots.
"""
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
T4_BASE = 83          # first composed-text slot (see text4.h)
MIN_POOL = 30         # pairs that must stay available for text

IMAGES = [
    # (c name, png, padded tile rows)
    ("art_title", "gbrogue_titile.png", 18),
    ("art_over", "gbrogue_gameover.png", 12),
]


def quantize(png: Path, rows: int) -> np.ndarray:
    a = np.asarray(Image.open(png).convert("L"))
    h, w = a.shape
    assert w == 160 and h <= rows * 8, f"{png}: {w}x{h} exceeds 160x{rows*8}"
    canvas = np.full((rows * 8, 160), 255, np.uint8)
    canvas[:h, :w] = a
    return np.select([canvas >= 192, canvas >= 128, canvas >= 64],
                     [0, 1, 2], 3).astype(np.uint8)


def tile_2bpp(q: np.ndarray) -> bytes:
    out = bytearray()
    for y in range(8):
        lo = hi = 0
        for x in range(8):
            c = int(q[y, x])
            lo = (lo << 1) | (c & 1)
            hi = (hi << 1) | (c >> 1)
        out += bytes((lo, hi))
    return bytes(out)


def slot_for(i: int) -> int:
    return i if i < T4_BASE else 255 - (i - T4_BASE)


def build(name: str, png: str, rows: int):
    q = quantize(ROOT / "samples" / png, rows)
    uniq: dict[bytes, int] = {tile_2bpp(np.zeros((8, 8), np.uint8)): 0}
    order = [next(iter(uniq))]            # index 0 = blank, always
    cells = []
    for ty in range(rows):
        for tx in range(20):
            t = tile_2bpp(q[ty*8:(ty+1)*8, tx*8:(tx+1)*8])
            if t not in uniq:
                uniq[t] = len(order)
                order.append(t)
            cells.append(slot_for(uniq[t]))
    n = len(order)
    high = max(0, n - T4_BASE)
    pool_free = (256 - high) - T4_BASE
    print(f"{name}: {rows} rows, {n} unique tiles, "
          f"{pool_free} text slots free")
    assert pool_free >= MIN_POOL, \
        f"{name}: art too detailed, only {pool_free} text slots left"
    return n, order, cells


def main() -> None:
    parts_c = [
        "/*",
        " * GENERATED FILE — see scripts/gen_art.py",
        " *",
        " * Full-screen art tiles + cell maps (map bytes are final VRAM",
        " * slots). Lives in ROM bank 2; ui_art.c blits through the",
        " * far-copy trampoline.",
        " */",
        "#include <gb/gb.h>",
        "#include <stdint.h>",
        "",
        "#pragma bank 2",
        "",
    ]
    parts_h = [
        "/* GENERATED FILE — see scripts/gen_art.py */",
        "#ifndef ART_DATA_H",
        "#define ART_DATA_H",
        "",
        "#include <gb/gb.h>",
        "#include <stdint.h>",
        "",
    ]
    for name, png, rows in IMAGES:
        n, order, cells = build(name, png, rows)
        parts_c.append(f"BANKREF({name}_tiles)")
        parts_c.append(f"const uint8_t {name}_tiles[{n} * 16] = {{")
        for i, t in enumerate(order):
            parts_c.append(f"    /* {i:3d} -> slot {slot_for(i):3d} */ "
                           + ", ".join(f"0x{b:02X}" for b in t) + ",")
        parts_c.append("};")
        parts_c.append("")
        parts_c.append(f"BANKREF({name}_map)")
        parts_c.append(f"const uint8_t {name}_map[{rows} * 20] = {{")
        for r in range(rows):
            row = cells[r*20:(r+1)*20]
            parts_c.append("    " + ", ".join(f"0x{c:02X}" for c in row) + ",")
        parts_c.append("};")
        parts_c.append("")
        up = name.upper()
        parts_h.append(f"#define {up}_TILES {n}u")
        parts_h.append(f"#define {up}_ROWS  {rows}u")
        parts_h.append(f"BANKREF_EXTERN({name}_tiles)")
        parts_h.append(f"extern const uint8_t {name}_tiles[];")
        parts_h.append(f"BANKREF_EXTERN({name}_map)")
        parts_h.append(f"extern const uint8_t {name}_map[];")
        parts_h.append("")
    parts_h += ["#endif", ""]
    (ROOT / "assets" / "art_data.c").write_text("\n".join(parts_c))
    (ROOT / "src" / "art_data.h").write_text("\n".join(parts_h))


if __name__ == "__main__":
    main()
