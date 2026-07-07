#!/usr/bin/env python3
"""Full-descent + win-path check: on a pinned seed, descend all 26 floors
(validating each generated map is traversable and has down-stairs), grab
the Amulet that mapgen places on level 26, climb back to level 1, and win.

Floors are entered by teleporting the hero onto the down-stairs and using
them (fast + deterministic) rather than walking — this exercises mapgen,
the descend/climb transitions, and the victory trigger end to end.
"""
import sys
from gbtest import GB, Failure

MAP_W, MAP_H = 32, 28
WALK = {1, 2, 5, 6, 7, 8}         # floor,corridor,door,stairs*,trap
STAIRS_DN = 6
AMULET_LEVEL = 26
MON_STRIDE = 7


def load_map(gb):
    return gb.rdbuf("g_map", MAP_W * MAP_H)


def find_stairs(raw):
    for y in range(MAP_H):
        for x in range(MAP_W):
            if (raw[y * MAP_W + x] & 0x1F) == STAIRS_DN:
                return (x, y)
    return None


def clear_mons(gb):
    base = gb.addr("g_mons")
    for i in range(12):
        gb.pb.memory[base + i * MON_STRIDE] = 0xFF


def put_on_stairs(gb, xy):
    gb.pb.memory[gb.addr("g_px")] = xy[0]
    gb.pb.memory[gb.addr("g_py")] = xy[1]
    gb.tick(3)


def wait_depth(gb, want, frames=300):
    for _ in range(frames):
        gb.tick(1)
        if gb.rd("g_depth") == want:
            # depth flips just before mapgen; let the new floor finish
            # generating AND the arrival message/fade fully settle before
            # the next teleport+use (a too-early A after arriving is
            # swallowed and the descend silently no-ops)
            gb.tick(150)
            return True
    return False


def validate(gb, depth):
    raw = load_map(gb)
    walk = sum(1 for b in raw if (b & 0x1F) in WALK)
    st = find_stairs(raw)
    if walk < 40:
        raise Failure(f"floor {depth}: only {walk} walkable cells")
    if st is None:
        raise Failure(f"floor {depth}: no down-stairs generated")
    return st


def main() -> int:
    gb = GB()
    so = gb.addr("g_seed_override")
    gb.pb.memory[so] = 0x2D; gb.pb.memory[so + 1] = 0x1D    # pin 0x1D2D
    gb.boot_game()

    # keep us alive and unbothered for the whole run
    gb.pb.memory[gb.addr("g_debug")] = 1          # invincible (not recorded)

    # ---- descend 1 -> 26 ----
    for d in range(1, AMULET_LEVEL):
        if gb.rd("g_depth") != d:
            raise Failure(f"expected depth {d}, got {gb.rd('g_depth')}")
        st = validate(gb, d)
        clear_mons(gb)
        put_on_stairs(gb, st)
        gb.press("a", hold=8, settle=8)           # descend
        if not wait_depth(gb, d + 1):
            raise Failure(f"never descended past floor {d}")
    print(f"  descended 1 -> {AMULET_LEVEL}, every floor traversable")

    # ---- level 26: the Amulet must be here ----
    validate(gb, AMULET_LEVEL)
    fbase = gb.addr("g_floor")
    # scan all MAX_FLOOR_ITEMS=24 floor slots for IK_AMULET (kind == 8);
    # the amulet can land in any free slot, not just the low ones
    found_amulet = any(gb.pb.memory[fbase + i * 8] == 8   # item_t stride = 8
                       for i in range(24))
    if not found_amulet:
        raise Failure("Amulet of Yendor not placed on level 26")
    gb.pb.memory[gb.addr("g_has_amulet")] = 1     # pick it up
    print("  Amulet of Yendor present on level 26 (grabbed)")

    # ---- climb 26 -> 1 (B on the down-stairs, amulet in hand) ----
    for d in range(AMULET_LEVEL, 1, -1):
        st = validate(gb, d)
        clear_mons(gb)
        put_on_stairs(gb, st)
        gb.press("b", hold=8, settle=8)           # climb up
        if not wait_depth(gb, d - 1):
            raise Failure(f"never climbed above floor {d}")
    print("  climbed 26 -> 1 with the Amulet")

    # ---- level 1 + Amulet + B on the stairs = victory ----
    st = validate(gb, 1)
    clear_mons(gb)
    put_on_stairs(gb, st)
    gb.press("b", hold=8, settle=30)
    if not gb.wait_screen(lambda rows: (gb.rd("g_won") == 1) or
                          any("Amulet is yours" in r or "WON" in r.upper()
                              for r in rows), 300):
        raise Failure(f"win not triggered (g_won={gb.rd('g_won')})")
    print("  reached the surface with the Amulet — WIN")

    gb.stop()
    print("PROBE OK: dungeon is completable 1 -> 26 -> 1 (win reached)")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Failure as e:
        print(f"verify_clear FAILED: {e}")
        sys.exit(1)
