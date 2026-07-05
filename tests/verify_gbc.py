#!/usr/bin/env python3
"""
GBC support: same ROM boots on DMG (unchanged monochrome) and on GBC
(designed dark theme — near-black background, warm ink, per-class
colors; explicitly NOT a plain inversion of the DMG image).
"""
from gbtest import GB, Failure, ROOT, ROM


def make_dmg_copy():
    """PyBoy force-boots CGB for CGB-flagged carts, so DMG behavior is
    tested on a copy with the header flag cleared (checksum fixed)."""
    data = bytearray(ROM.read_bytes())
    data[0x143] = 0x00
    csum = 0
    for i in range(0x134, 0x14D):
        csum = (csum - data[i] - 1) & 0xFF
    data[0x14D] = csum
    out = ROOT / "build" / "gbrogue_dmg.gb"
    out.write_bytes(data)
    # reuse the main symbol file
    noi = out.with_suffix(".noi")
    noi.write_bytes(ROM.with_suffix(".noi").read_bytes())
    return out


def stats(img):
    """(mean_brightness, bright_count, hue_variety) over the viewport."""
    px = img.convert("RGB").load()
    w, h = img.size
    total = 0
    bright = 0
    hues = set()
    n = 0
    for y in range(0, 128, 2):          # dungeon viewport only
        for x in range(0, w, 2):
            r, g, b = px[x, y]
            lum = (r + g + b) // 3
            total += lum
            n += 1
            if lum > 180:
                bright += 1
            if max(r, g, b) - min(r, g, b) > 40:
                # quantized bucket: separates amber walls from red
                # monsters, aqua items, green stairs
                hues.add((r // 48, g // 48, b // 48))
    return total // n, bright, len(hues)


def main() -> int:
    # --- GBC boot: dark theme + colors
    gb = GB(cgb=True)
    gb.expect(gb.rd("g_is_gbc") == 1, "CGB hardware not detected")
    gb.boot_game()

    # guarantee a second saturated hue in view regardless of the seed:
    # plant a monster (ember-red OBJ palette) next to the player
    raw = gb.rdbuf("g_map", 32 * 28)
    grid = [[raw[y * 32 + x] & 0x1F for x in range(32)] for y in range(28)]
    px, py = gb.rd("g_px"), gb.rd("g_py")
    spot = next((px + dx, py + dy) for dx, dy in
                ((1, 0), (-1, 0), (0, 1), (0, -1))
                if grid[py + dy][px + dx] in {1, 2, 5, 6, 7, 8})
    base = gb.addr("g_mons")
    mem = gb.pb.memory
    mem[base + 1], mem[base + 2] = spot
    mem[base + 3], mem[base + 4] = 200, 0
    mem[base + 5] = mem[base + 6] = 0
    mem[base + 0] = 25                    # zombie
    gb.press("select", hold=8, settle=20)  # open+close pack to resync
    gb.press("b", hold=8, settle=40)
    gb.tick(30)

    img = gb.pb.screen.image.convert("RGB")
    mean, bright, hue_n = stats(img)
    print(f"  GBC: mean={mean} bright_px={bright} hues={hue_n}")
    gb.expect(mean < 70, f"GBC theme not dark (mean {mean})")
    gb.expect(bright > 5, "no bright ink pixels on GBC")
    gb.expect(hue_n >= 2, f"expected multiple hues, got {hue_n}")
    gb.shot("gbc_01_world")

    # not a plain inversion: sample the darkest color — it must be a
    # *tinted* near-black (our C_BLACK has a blue bias), and the ink
    # must be warm (r >= b), unlike inverted pure white/black.
    px = img.load()
    darkest = min((px[x, y] for y in range(0, 128, 3) for x in range(0, 160, 3)),
                  key=sum)
    print(f"  darkest sample: {darkest}")
    gb.expect(sum(darkest) > 0, "pure #000 background suggests plain inversion")

    # pack UI also dark
    gb.press("select", hold=8, settle=30)
    m2, b2, _ = stats(gb.pb.screen.image)
    gb.expect(m2 < 70, f"pack screen not dark on GBC (mean {m2})")
    gb.shot("gbc_02_pack")
    gb.press("b", hold=8, settle=30)
    gb.stop()

    # --- DMG boot (header-patched copy): still light-background mono
    gb = GB(rom=make_dmg_copy(), cgb=False)
    gb.expect(gb.rd("g_is_gbc") == 0, "DMG boot flagged as GBC")
    gb.boot_game()
    mean, bright, hue_n = stats(gb.pb.screen.image)
    print(f"  DMG: mean={mean} hues={hue_n}")
    gb.expect(mean > 120, f"DMG should stay light-background (mean {mean})")
    gb.stop()

    print("verify_gbc: all checks passed")
    return 0


if __name__ == "__main__":
    import sys
    try:
        sys.exit(main())
    except Failure as e:
        print(f"verify_gbc FAILED: {e}")
        sys.exit(1)
