#!/usr/bin/env python3
"""
M7: identification, potion/scroll/wand effects, curse, hunger, traps.

Runs against the debug ROM (build/dbg/gbrogue.gb, `make debugrom`) whose
starting pack carries a known test kit and a nearly-empty stomach.
"""
from pathlib import Path

from gbtest import GB, Failure, ROOT

DBG_ROM = ROOT / "build" / "dbg" / "gbrogue.gb"

MAP_W, MAP_H = 32, 28
MF_TERRAIN, MF_EXPLORED, MF_HIDDEN = 0x1F, 0x20, 0x80
TI_TRAP = 8
PACK_SLOTS = 16
STRIDE = 7
ITEM_NONE = 9
IF_CURSED, IF_KNOWN_CURSED, IF_WORN = 1, 2, 4


def read_pack(gb):
    raw = gb.rdbuf("g_pack", PACK_SLOTS * STRIDE)
    out = {}
    for i in range(PACK_SLOTS):
        k, sub, x, y, qty, ench, flags = raw[i * STRIDE:(i + 1) * STRIDE]
        if k != ITEM_NONE:
            out[i] = {"kind": k, "sub": sub, "qty": qty,
                      "ench": ench - 256 if ench > 127 else ench, "flags": flags}
    return out


def pack_open(rows):
    # rows flush top-to-bottom; the bottom hint landing means the whole
    # pack screen (all 18 rows) is on the glass
    return any("PACK" in r for r in rows) and "close" in rows[17]


def open_pack(gb):
    if pack_open(gb.screen_rows()):
        return
    gb.press_until("select", pack_open)


LIST_TOP = 2      # first pack list row (see ui_inv.c)


def cursor_to(gb, idx):
    """Closed-loop: read the on-screen '>' and steer it to item idx.
    Valid while idx keeps the list unscrolled (idx <= 11)."""
    if idx > 11:
        raise Failure(f"cursor_to only handles unscrolled rows, got {idx}")
    want = LIST_TOP + idx
    for _ in range(60):
        cur = gb.cursor_row()
        if cur < 0:
            gb.tick(4)          # mid-repaint; let the rows settle
            continue
        if cur == want:
            return
        gb.press("down" if cur < want else "up", hold=6, settle=10)
    gb.expect(False, f"cursor never reached row {want}")


def idx_of(gb, kind, sub):
    """The pack compacts on every removal — always resolve fresh."""
    for i, it in read_pack(gb).items():
        if it["kind"] == kind and it["sub"] == sub:
            return i
    raise Failure(f"pack item kind={kind} sub={sub} missing")


def use_item(gb, kind, sub):
    open_pack(gb)
    cursor_to(gb, idx_of(gb, kind, sub))
    gb.press("a", hold=8, settle=20)     # opens the action submenu
    gb.press("a", hold=8, settle=40)     # confirms the primary action


def explored_count(gb):
    # explored is a separate g_explored bitmap now (1 bit per cell)
    stride = (MAP_W + 7) // 8
    raw = gb.rdbuf("g_explored", MAP_H * stride)
    n = 0
    for y in range(MAP_H):
        for x in range(MAP_W):
            if (raw[y * stride + (x >> 3)] >> (x & 7)) & 1:
                n += 1
    return n


def known_bits(gb, cls):
    return gb.rd16("g_id_known", cls * 2)


def main() -> int:
    gb = GB(rom=DBG_ROM)
    gb.boot_game()
    gb.inject_debug_kit()                 # M7 items now injected, not on-cart

    pack = read_pack(gb)
    print(f"  pack: {pack}")

    # Beef up HP so unlucky monster trains can't kill the test run
    # (we're testing item/trap mechanics here, not survival).
    gb.pb.memory[gb.addr("g_maxhp")] = 120
    gb.pb.memory[gb.addr("g_hp")] = 120

    # --- unidentified name shows on the pack screen (alias, not
    #     "healing"), and the description panel admits ignorance
    open_pack(gb)
    heal_idx = idx_of(gb, 1, 5)
    rows = gb.screen_rows()
    line = rows[LIST_TOP + heal_idx]
    gb.expect("ptn" in line and "healing" not in line,
              f"unidentified potion line looks wrong: {line!r}")
    cursor_to(gb, heal_idx)
    gb.expect(gb.wait_screen(
        lambda rows: any("unknown" in r for r in rows[15:17])),
        "no unknown-effect description for an unidentified potion")
    gb.press_until("b", lambda rows: not pack_open(rows))

    # --- scroll of magic mapping: exploration explodes + scroll learned
    e0 = explored_count(gb)
    use_item(gb, 2, 1)
    e1 = explored_count(gb)
    gb.expect(e1 > e0 + 150, f"magic map barely explored: {e0} -> {e1}")
    gb.expect(known_bits(gb, 1) & (1 << 1), "magic-map scroll not learned")
    gb.shot("m7_01_magicmap")

    # --- healing potion at full HP bumps max HP (Rogue overheal).
    #     Top the HP up while the pack modal is open: the world is
    #     frozen there, so no monster can chip it before the quaff.
    open_pack(gb)
    cursor_to(gb, idx_of(gb, 1, 5))
    max0 = gb.rd("g_maxhp")
    gb.pb.memory[gb.addr("g_hp")] = max0
    gb.press("a", hold=8, settle=20)     # opens the action submenu
    gb.press("a", hold=8, settle=40)     # confirms (quaff)
    gb.expect(gb.rd("g_maxhp") == max0 + 1,
              f"overheal should bump maxhp {max0} -> {gb.rd('g_maxhp')}")
    gb.expect(known_bits(gb, 0) & (1 << 5), "healing potion not learned")

    # --- a learned potion now shows its effect in the pack panel
    open_pack(gb)
    cursor_to(gb, idx_of(gb, 1, 0))       # confusion: still unknown
    gb.expect(gb.wait_screen(
        lambda rows: any("unknown" in r for r in rows[15:17])),
        "unlearned potion should stay unknown in the panel")
    gb.press_until("b", lambda rows: not pack_open(rows))

    # --- confusion potion sets the timer; walking staggers
    use_item(gb, 1, 0)
    gb.expect(gb.rd("g_conf_t") > 0, "confusion timer not set")
    gb.expect(known_bits(gb, 0) & 1, "confusion potion not learned")
    # wait it out (resting consumes turns; damage flashes stretch turns,
    # so taps can coalesce — budget generously)
    for _ in range(80):
        if gb.rd("g_conf_t") == 0:
            break
        gb.press("a", hold=6, settle=12)
    gb.expect(gb.rd("g_conf_t") == 0, "confusion never wore off")

    # --- wand: zap right; charge spent, wand class learned
    q0 = read_pack(gb)[idx_of(gb, 3, 6)]["qty"]
    open_pack(gb)
    cursor_to(gb, idx_of(gb, 3, 6))
    gb.press("a", hold=8, settle=20)     # opens the action submenu
    gb.press("a", hold=8, settle=30)     # confirm -> aim on the live world
    gb.press("right", hold=6, settle=10) # turn the aim cursor right
    gb.press("a", hold=8, settle=30)     # A confirms the aim -> fires
    gb.expect(read_pack(gb)[idx_of(gb, 3, 6)]["qty"] == q0 - 1,
              "wand charge not spent")
    gb.expect(known_bits(gb, 2) & (1 << 6), "wand not learned")

    # --- ring: put on -> AC unchanged (regen ring) but flags WORN; regen ticks
    use_item(gb, 4, 9)
    gb.expect(read_pack(gb)[idx_of(gb, 4, 9)]["flags"] & IF_WORN,
              "ring not worn")

    # --- cursed dagger: wield, then try to unwield -> blocked + revealed
    use_item(gb, 5, 4)
    gb.expect(gb.rd("g_wield") == idx_of(gb, 5, 4), "dagger not wielded")
    use_item(gb, 5, 4)                    # attempt unwield
    gb.expect(gb.rd("g_wield") == idx_of(gb, 5, 4), "cursed dagger came off!")
    gb.expect(read_pack(gb)[idx_of(gb, 5, 4)]["flags"] & IF_KNOWN_CURSED,
              "curse not revealed")
    gb.shot("m7_02_cursed")

    # --- hunger: rest until the tag appears, then eat
    for _ in range(80):
        if gb.rd("g_hunger") >= 1:
            break
        gb.press("a", hold=6, settle=8)   # search = rest
    gb.expect(gb.rd("g_hunger") >= 1, f"never got hungry (food={gb.rd16('g_food')})")
    gb.expect(gb.wait_screen(
        lambda rows: rows[17].rstrip().endswith(("Hu", "Wk", "Ft"))),
        f"status lacks hunger tag: {gb.status_row()!r}")
    gb.shot("m7_03_hungry")
    use_item(gb, 0, 0)
    gb.expect(gb.rd("g_hunger") == 0, "eating did not clear hunger")
    gb.expect(gb.rd16("g_food") > 800, f"food low after eating: {gb.rd16('g_food')}")

    # --- traps: descend until the level has one, then step on it
    from collections import deque

    def read_grid():
        raw = gb.rdbuf("g_map", MAP_W * MAP_H)
        return [[raw[y * MAP_W + x] for x in range(MAP_W)] for y in range(MAP_H)]

    def mons_pos():
        raw = gb.rdbuf("g_mons", 12 * 7)
        return {(raw[i*7+1], raw[i*7+2]): i for i in range(12) if raw[i*7] != 0xFF}

    WALK = {1, 2, 5, 6, 7, 8}

    def path_to(grid, start, goal, avoid):
        q, prev = deque([start]), {start: None}
        while q:
            cur = q.popleft()
            if cur == goal:
                out = []
                while cur is not None:
                    out.append(cur)
                    cur = prev[cur]
                return out[::-1]
            x, y = cur
            for nx, ny in ((x+1, y), (x-1, y), (x, y+1), (x, y-1)):
                if 0 <= nx < MAP_W and 0 <= ny < MAP_H and (nx, ny) not in prev \
                   and (grid[ny][nx] & MF_TERRAIN) in WALK and (nx, ny) not in avoid:
                    prev[(nx, ny)] = (x, y)
                    q.append((nx, ny))
        return None

    def goto(goal, limit=600, clear=False):
        mbase = gb.addr("g_mons")
        for _ in range(limit):
            if clear:
                # this test isn't about combat: keep monsters off the
                # board so heavy chasing can't stall the navigation
                for i in range(12):
                    gb.pb.memory[mbase + i * 7] = 0xFF
            p = (gb.rd("g_px"), gb.rd("g_py"))
            if p == goal:
                return True
            if gb.rd("g_hp") == 0:
                raise Failure("died en route")
            grid = read_grid()
            mons = mons_pos()
            path = path_to(grid, p, goal, set(mons) - {goal})
            if not path or len(path) < 2:
                # a monster may be squatting on the target; fight through
                path = path_to(grid, p, goal, set())
                if not path or len(path) < 2:
                    return False
            nx, ny = path[1]
            btn = {(1, 0): "right", (-1, 0): "left",
                   (0, 1): "down", (0, -1): "up"}[(nx - p[0], ny - p[1])]
            # closed loop: under heavy harassment the flashes stretch
            # turns and coalesce naive presses — insist on the turn
            t0 = gb.rd16("g_turns")
            gb.press(btn, hold=6, settle=6)
            for _ in range(12):
                if gb.rd16("g_turns") != t0:
                    break
                gb.tick(6)
        return False

    trap_hit = False
    # not every level rolls a (reachable) trap — some room picks land on
    # gone/occupied cells — so give the search a generous floor budget.
    for _ in range(8):
        # find stairs on this level
        grid = read_grid()
        # map property: doors never touch (no corridor may run along a
        # room wall, and no two legs may pierce a wall side by side)
        doors = {(x, y) for y in range(MAP_H) for x in range(MAP_W)
                 if (grid[y][x] & MF_TERRAIN) == 5}
        for (x, y) in doors:
            gb.expect((x + 1, y) not in doors and (x, y + 1) not in doors,
                      f"adjacent doors around {(x, y)} on depth "
                      f"{gb.rd('g_depth')}")
        stairs = next(((x, y) for y in range(MAP_H) for x in range(MAP_W)
                       if (grid[y][x] & MF_TERRAIN) == 6), None)
        gb.expect(stairs is not None, "no stairs on level")
        n_traps = gb.rd("g_trap_count")
        if n_traps:
            traps = gb.rdbuf("g_traps", n_traps * 3)
            tx, ty, tkind = traps[0], traps[1], traps[2]
            d0 = gb.rd("g_depth")
            if goto((tx, ty), limit=400, clear=True):
                cell = read_grid()[ty][tx]
                revealed = not (cell & MF_HIDDEN)
                fell = gb.rd("g_depth") == d0 + 1
                ported = (gb.rd("g_px"), gb.rd("g_py")) != (tx, ty)
                gb.expect(revealed or fell or ported,
                          f"trap at {(tx,ty)} kind={tkind} did nothing")
                print(f"  trap kind={tkind} triggered (fell={fell})")
                trap_hit = True
                break
        d0 = gb.rd("g_depth")
        # terrain-only snapshot: walking only flips explored/hidden
        # flags, so the terrain bits identify "still the old level"
        pre = bytes(c & MF_TERRAIN
                    for c in gb.rdbuf("g_map", MAP_W * MAP_H))
        # self-healing: re-locate the stairs from a fresh grid and
        # re-walk each try (queued fade-in inputs can drift the player,
        # and a snapshot's stairs can go stale)
        for _ in range(6):
            grid = read_grid()
            stairs = next(((x, y) for y in range(MAP_H)
                           for x in range(MAP_W)
                           if (grid[y][x] & MF_TERRAIN) == 6), None)
            gb.expect(stairs is not None, "stairs vanished")
            gb.expect(goto(stairs), "could not reach stairs")
            gb.press("a", hold=8, settle=8)
            if gb.wait_screen(lambda rows: gb.rd("g_depth") == d0 + 1, 120):
                break
        gb.expect(gb.rd("g_depth") == d0 + 1, "descend failed")
        # depth flips right before mapgen, and the OLD map (which also
        # has stairs) stays readable through the stair fade — wait for
        # the terrain to actually change and the new stairs to land
        for _ in range(200):
            raw = bytes(c & MF_TERRAIN
                        for c in gb.rdbuf("g_map", MAP_W * MAP_H))
            if raw != pre and any(c == 6 for c in raw):
                break
            gb.tick(8)
    gb.expect(trap_hit, "no trap found within 4 levels")

    gb.stop()
    print("verify_m7: all checks passed")
    return 0


if __name__ == "__main__":
    import sys
    try:
        sys.exit(main())
    except Failure as e:
        print(f"verify_m7 FAILED: {e}")
        sys.exit(1)
