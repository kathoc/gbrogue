#!/usr/bin/env python3
"""
M9/M10: diagonal movement, B+dir fast move, B+A rest, START menu,
message log, throw, ASCII/GFX tileset toggle.

Runs on the debug ROM (shuriken in the kit for the throw check).
"""
from gbtest import GB, Failure, ROOT

DBG_ROM = ROOT / "build" / "dbg" / "gbrogue.gb"
MAP_W, MAP_H = 32, 28
MF_TERRAIN, MF_HIDDEN = 0x1F, 0x80
WALK = {1, 2, 5, 6, 7, 8}
TI_DOOR = 5
GFX_BASE = 45
PACK_SLOTS, STRIDE = 16, 7


def read_grid(gb):
    raw = gb.rdbuf("g_map", MAP_W * MAP_H)
    return [[raw[y * MAP_W + x] for x in range(MAP_W)] for y in range(MAP_H)]


def read_pack(gb):
    raw = gb.rdbuf("g_pack", PACK_SLOTS * STRIDE)
    return {i: tuple(raw[i * STRIDE:(i + 1) * STRIDE])
            for i in range(PACK_SLOTS) if raw[i * STRIDE] != 9}


def main() -> int:
    gb = GB(rom=DBG_ROM)
    gb.boot_game()
    gb.inject_debug_kit()                 # shuriken etc. injected, not on-cart

    grid = read_grid(gb)
    px, py = gb.rd("g_px"), gb.rd("g_py")

    # --- diagonal step: both orthogonals must be open (no corner cut)
    DIAGS = ((1, 1, "right", "down"), (-1, 1, "left", "down"),
             (1, -1, "right", "up"), (-1, -1, "left", "up"))
    diag = None
    for dx, dy, b1, b2 in DIAGS:
        tx, ty = px + dx, py + dy
        if not (0 <= tx < MAP_W and 0 <= ty < MAP_H):
            continue
        if (grid[ty][tx] & MF_TERRAIN) in WALK \
           and (grid[ty][tx] & MF_TERRAIN) != TI_DOOR \
           and (grid[py][px] & MF_TERRAIN) != TI_DOOR \
           and (grid[py][tx] & MF_TERRAIN) in WALK \
           and (grid[ty][px] & MF_TERRAIN) in WALK:
            diag = (dx, dy, b1, b2)
            break
    gb.expect(diag is not None, "no diagonal-walkable neighbor at spawn")
    dx, dy, b1, b2 = diag
    gb.combo(b1, b2, hold=10, settle=20)
    gb.expect((gb.rd("g_px"), gb.rd("g_py")) == (px + dx, py + dy),
              f"diagonal failed: at {(gb.rd('g_px'), gb.rd('g_py'))}, "
              f"wanted {(px + dx, py + dy)}")
    print(f"  diagonal step {dx},{dy} ok")

    # --- corridor elbows: a blocked orthogonal forbids the diagonal
    corner = None
    for y in range(1, MAP_H - 1):
        for x in range(1, MAP_W - 1):
            if (grid[y][x] & MF_TERRAIN) not in WALK \
               or (grid[y][x] & MF_TERRAIN) == TI_DOOR:
                continue
            for cdx, cdy, cb1, cb2 in DIAGS:
                t = grid[y + cdy][x + cdx] & MF_TERRAIN
                o1 = grid[y][x + cdx] & MF_TERRAIN
                o2 = grid[y + cdy][x] & MF_TERRAIN
                if t in WALK and t != TI_DOOR \
                   and (o1 in WALK) != (o2 in WALK):   # exactly one open
                    corner = (x, y, cdx, cdy, cb1, cb2)
                    break
            if corner:
                break
        if corner:
            break
    gb.expect(corner is not None, "no corridor elbow found on this map")
    cx, cy, cdx, cdy, cb1, cb2 = corner
    gb.pb.memory[gb.addr("g_px")] = cx
    gb.pb.memory[gb.addr("g_py")] = cy
    gb.tick(4)
    gb.combo(cb1, cb2, hold=10, settle=20)
    gb.expect((gb.rd("g_px"), gb.rd("g_py")) == (cx, cy),
              f"corner cut: slipped diagonally to "
              f"{(gb.rd('g_px'), gb.rd('g_py'))} from {(cx, cy)}")
    gb.pb.memory[gb.addr("g_px")] = px + dx     # back where we were
    gb.pb.memory[gb.addr("g_py")] = py + dy
    gb.tick(4)
    print("  corridor corner cut blocked")

    # --- B+A rest consumes a turn without moving
    t0 = gb.rd16("g_turns")
    p0 = (gb.rd("g_px"), gb.rd("g_py"))
    gb.hold("b")
    gb.tick(4)
    gb.press("a", hold=8, settle=12)
    gb.release("b")
    gb.tick(10)
    gb.expect(gb.rd16("g_turns") == t0 + 1, f"rest turns {t0} -> {gb.rd16('g_turns')}")
    gb.expect((gb.rd("g_px"), gb.rd("g_py")) == p0, "rest moved the player")
    print("  B+A rest ok")

    # --- B+dir fast move: clear all monsters (visible monsters stop the
    #     run by design), teleport into the widest room, then glide.
    mons_base = gb.addr("g_mons")
    for i in range(12):
        gb.pb.memory[mons_base + i * 7] = 0xFF

    # rooms: x,y,w,h,flags (5 bytes); pick the widest non-gone one
    rraw = gb.rdbuf("g_rooms", 9 * 5)
    rooms = [tuple(rraw[i*5:(i+1)*5]) for i in range(9)
             if not (rraw[i*5+4] & 1) and rraw[i*5+2] >= 6]
    gb.expect(rooms, "no room wide enough for a fast-move run")
    rx, ry, rw, rh, _ = max(rooms, key=lambda r: r[2])
    # avoid interior rows holding items or stairs (they stop the run)
    fraw = gb.rdbuf("g_floor", 24 * 7)
    item_rows = {fraw[i*7+3] for i in range(24) if fraw[i*7] != 9}
    cand = [y for y in range(ry + 1, ry + rh - 1)
            if y not in item_rows]
    sy = cand[len(cand) // 2] if cand else ry + rh // 2
    gb.pb.memory[gb.addr("g_px")] = rx + 1
    gb.pb.memory[gb.addr("g_py")] = sy
    gb.tick(10)                            # clear any swallow window
    for _ in range(4):                     # one real step resyncs view
        gb.press("right", hold=6, settle=20)
        if gb.rd("g_px") != rx + 1:
            break
    gb.expect(gb.rd("g_px") == rx + 2, "resync step did not land")
    px, py = gb.rd("g_px"), gb.rd("g_py")
    gb.hold("b")
    gb.tick(4)
    gb.press("right", hold=8, settle=8)
    gb.release("b")
    gb.tick(160)
    moved = abs(gb.rd("g_px") - px) + abs(gb.rd("g_py") - py)
    gb.expect(moved >= 2, f"fast move only moved {moved} in a {rw}-wide room")
    print(f"  fast move ran {moved} tiles")

    # --- START menu (closed-loop cursor: rows are 2 + i, single spacing)
    menu_on = lambda rows: any("MENU" in r for r in rows)

    def menu_pick(idx):
        gb.press_until("start", menu_on)
        want = 2 + idx
        for _ in range(20):
            cur = gb.cursor_row()
            if cur == want:
                break
            gb.press("down" if cur < want else "up", hold=6, settle=10)
        gb.expect(gb.cursor_row() == want, f"menu cursor stuck (want row {want})")
        gb.press("a", hold=8, settle=8)

    gb.press_until("start", menu_on)
    gb.expect(gb.wait_screen(lambda rows: any("Message log" in r for r in rows)
                             and any("Save & quit" in r for r in rows)),
              "menu entries never finished drawing")
    gb.shot("m9_01_menu")
    gb.press_until("b", lambda rows: not any("MENU" in r for r in rows))

    # --- throw: shuriken is now thrown FROM THE INVENTORY (select it, A ->
    #     "throw" action -> aim), not from the menu.
    def shuriken(kv=False):
        for slot, it in read_pack(gb).items():
            if it[0] == 5 and it[1] == 7:
                return (slot, it[4]) if kv else it[4]
        return (None, 0) if kv else 0
    sl, q0 = shuriken(kv=True)
    gb.expect(sl is not None and q0 >= 1, "debug kit lacks shuriken")
    gb.press_until("select",
                   lambda rows: any("PACK" in r for r in rows) and "close" in rows[17])
    for _ in range(sl):                   # cursor 0 -> the shuriken row
        gb.press("down", hold=6, settle=8)
    gb.press("a", hold=8, settle=20)      # action submenu (primary = throw)
    gb.press("a", hold=8, settle=30)      # confirm -> aim on the world
    gb.press("right", hold=6, settle=10)  # aim
    for _ in range(5):
        gb.press("a", hold=8, settle=30)  # A confirms -> throws
        if shuriken() == q0 - 1:
            break
    gb.expect(shuriken() == q0 - 1, f"shuriken {q0} -> {shuriken()}")
    print("  throw consumed ammo (from inventory)")

    # --- M10: display toggle -> world uses GFX tile indices 45..62
    def gfx_cells():
        # scan the whole BG map (world mode keeps the full level there)
        mem = gb.pb.memory
        n = 0
        for y in range(32):
            for x in range(32):
                if GFX_BASE <= mem[0x9800 + 32 * y + x] < GFX_BASE + 18:
                    n += 1
        return n
    gb.expect(gfx_cells() == 0, "ASCII mode should have no gfx tiles")
    menu_pick(1)                          # Display mode (index 1: Log/Display/Speed/Lang/Map/Quit)
    gb.expect(gb.wait_screen(lambda rows: gb.rd("g_render_mode") == 1),
              "render mode flag not set")
    gb.tick(60)                           # let the repaint land
    n = gfx_cells()
    gb.expect(n > 30, f"GFX mode shows only {n} gfx cells")
    gb.shot("m9_03_gfx_mode")
    # toggle back
    menu_pick(1)                          # Display mode (index 1: Log/Display/Speed/Lang/Map/Quit)
    gb.expect(gb.wait_screen(lambda rows: gb.rd("g_render_mode") == 0),
              "render mode flag not cleared")
    gb.tick(60)
    gb.expect(gfx_cells() == 0, "ASCII mode not restored")
    print(f"  GFX toggle ok ({n} gfx cells)")

    # --- suspend via menu (Save & quit, index 5: Log/Display/Speed/Lang/Map/Quit)
    menu_pick(5)
    gb.expect(gb.wait_screen(lambda rows: any("Game saved" in r for r in rows)),
              "save popup never appeared")
    gb.press_until("a", lambda rows: any("> CONTINUE" in r for r in rows))
    print("  menu suspend ok")

    # --- status row: active-effect tags right of gold + stairs hint.
    #     (suspend put us back on the title: restart a run first)
    gb.boot_game()
    gb.inject_debug_kit()
    mem = gb.pb.memory
    mem[gb.addr("g_haste_t")] = 60
    mem[gb.addr("g_conf_t")] = 0
    gb.press("a", hold=8, settle=40)            # any turn repaints status
    gb.expect(gb.wait_screen(lambda rows: "hast" in rows[17]),
              f"haste tag missing from status: {gb.screen_rows()[17]!r}")
    mem[gb.addr("g_levit_t")] = 60
    gb.press("a", hold=8, settle=40)
    gb.expect(gb.wait_screen(lambda rows: "hast/levt" in rows[17]),
              f"combined tags missing: {gb.screen_rows()[17]!r}")
    mem[gb.addr("g_haste_t")] = 0
    mem[gb.addr("g_levit_t")] = 0

    # stairs hint bottom-right when standing on the staircase
    raw = gb.rdbuf("g_map", MAP_W * MAP_H)
    st = next(((x, y) for y in range(MAP_H) for x in range(MAP_W)
               if (raw[y * MAP_W + x] & MF_TERRAIN) == 6), None)
    gb.expect(st is not None, "no stairs on level")
    mem[gb.addr("g_px")], mem[gb.addr("g_py")] = st
    gb.tick(4)
    gb.hold("b")                                # A+B rest: repaint, no descend
    gb.tick(4)
    gb.press("a", hold=8, settle=8)
    gb.release("b")
    gb.tick(30)
    gb.expect(gb.wait_screen(
        lambda rows: rows[17].rstrip().endswith("down")),   # A-icon + "down"
        f"stairs hint missing: {gb.screen_rows()[17]!r}")
    print("  status shows effect tags and the stairs hint")

    gb.stop()
    print("verify_m9: all checks passed")
    return 0


if __name__ == "__main__":
    import sys
    try:
        sys.exit(main())
    except Failure as e:
        print(f"verify_m9 FAILED: {e}")
        sys.exit(1)
