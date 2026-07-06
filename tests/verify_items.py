#!/usr/bin/env python3
"""Exhaustive item pass: inject every sub-type of every category into the
pack, use/equip it through the real pack UI, and assert it took effect
(consumed / charge spent / worn) with no crash or state corruption.

Runs on a pinned seed so the world is stable regardless of build timing.
"""
import sys
from gbtest import GB, Failure

# IK_* kinds (src/items.h) and how many sub-types each has.
FOOD, POTION, SCROLL, WAND, RING, WEAPON, ARMOR = 0, 1, 2, 3, 4, 5, 6
ITEM_NONE = 9          # IK_COUNT == empty slot
IF_WORN = 0x04
STRIDE = 7
CATS = [
    ("food",   FOOD,   2,  "consume"),
    ("potion", POTION, 14, "consume"),
    ("scroll", SCROLL, 13, "consume"),
    ("wand",   WAND,   14, "wand"),
    ("ring",   RING,   14, "wear"),
    ("weapon", WEAPON, 9,  "wield"),
    ("armor",  ARMOR,  8,  "wear"),
]


def clear_pack(gb):
    base = gb.addr("g_pack")
    for i in range(16):
        gb.pb.memory[base + i * STRIDE] = ITEM_NONE
    gb.pb.memory[gb.addr("g_wield")] = 0xFF
    gb.pb.memory[gb.addr("g_worn")] = 0xFF
    gb.pb.memory[gb.addr("g_ring_l")] = 0xFF   # both fingers empty
    gb.pb.memory[gb.addr("g_ring_r")] = 0xFF


def inject(gb, kind, sub, qty):
    base = gb.addr("g_pack")
    m = gb.pb.memory
    m[base+0] = kind; m[base+1] = sub; m[base+2] = 0; m[base+3] = 0
    m[base+4] = qty; m[base+5] = 0; m[base+6] = 0
    gb.tick(2)


def slot0(gb):
    base = gb.addr("g_pack")
    m = gb.pb.memory
    return {"kind": m[base+0], "sub": m[base+1], "qty": m[base+4],
            "flags": m[base+6]}


def pack_open(rows):
    return any("PACK" in r for r in rows)


def use_slot0(gb, mode):
    # SELECT opens the pack; cursor sits on slot 0 (the injected item)
    gb.press_until("select", lambda rows: pack_open(rows) and "close" in rows[17])
    gb.press("a", hold=8, settle=16)      # action submenu (primary verb)
    gb.press("a", hold=8, settle=24)      # execute the primary action
    if mode == "wand":
        gb.press("right", hold=6, settle=10)   # aim
        gb.press("a", hold=8, settle=24)       # fire
    # clear any follow-up popup / identify prompt, then make sure we are
    # back on the world (equip keeps the pack open)
    for _ in range(3):
        if not pack_open(gb.screen_rows()):
            break
        gb.press("b", hold=6, settle=10)
    gb.press_until("b", lambda rows: not pack_open(rows), tries=4)
    gb.tick(10)


def main() -> int:
    gb = GB()
    so = gb.addr("g_seed_override")
    gb.pb.memory[so] = 0x2D; gb.pb.memory[so+1] = 0x1D   # pin 0x1D2D
    gb.boot_game()

    total = 0
    for name, kind, n, mode in CATS:
        for sub in range(n):
            total += 1
            gb.pb.memory[gb.addr("g_hp")] = 99
            gb.pb.memory[gb.addr("g_maxhp")] = 99
            clear_pack(gb)
            qty = 5 if mode == "wand" else (2 if mode == "consume" else 1)
            inject(gb, kind, sub, qty)
            use_slot0(gb, mode)

            s = slot0(gb)
            hp = gb.rd("g_hp")
            # no crash / corruption: a banked-code crash zeroes HP and fills
            # WRAM with 0x39; a healing potion may legitimately push HP a
            # little over the forced value, so allow a wide sane band.
            if not (0 < hp < 250):
                raise Failure(f"{name}#{sub}: HP went insane ({hp}) — likely a crash")
            if s["kind"] not in (ITEM_NONE, FOOD, POTION, SCROLL, WAND,
                                 RING, WEAPON, ARMOR):
                raise Failure(f"{name}#{sub}: pack slot corrupted (kind={s['kind']})")
            if mode in ("consume",):
                ok = (s["kind"] == ITEM_NONE) or (s["qty"] < qty)
                if not ok:
                    raise Failure(f"{name}#{sub}: not consumed (slot={s})")
            elif mode == "wand":
                if not (s["kind"] == WAND and s["qty"] < qty):
                    raise Failure(f"{name}#{sub}: charge not spent (slot={s})")
            elif mode in ("wield", "wear"):
                worn = (s["flags"] & IF_WORN) != 0
                eq = (gb.rd("g_wield") == 0) or (gb.rd("g_worn") == 0) or worn
                if not eq:
                    raise Failure(f"{name}#{sub}: not equipped (slot={s} "
                                  f"wield={gb.rd('g_wield')} worn={gb.rd('g_worn')})")
        print(f"  {name}: all {n} sub-types OK")

    gb.stop()
    print(f"PROBE OK: all {total} item sub-types used without crash/corruption")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Failure as e:
        print(f"verify_items FAILED: {e}")
        sys.exit(1)
