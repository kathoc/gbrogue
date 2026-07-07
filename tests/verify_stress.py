#!/usr/bin/env python3
"""
verify_stress.py: save/resume round-trip stress test across 20 distinct
play patterns (plain movement, B-dash, stair descent, search/rest, and
composites of these). Exercises the static/dynamic save split, the
incremental checksum, and the separated EXPLORED bitmap under varied
play rather than the single fixed script verify_m8 uses.

Each pattern boots a fresh, differently-seeded run (boot_frames varies
per pattern), drives a distinct action sequence, snapshots state, does
a suspend-save -> title CONTINUE -> resume round trip (same mechanism
as verify_m8), and checks pre == post plus basic no-crash/HUD sanity.
A pattern where a direction bump does nothing (wall) is still a pass:
the round trip only needs to preserve whatever state was actually
reached.
"""
import sys
from gbtest import GB, Failure

PACK_SLOTS, STRIDE = 16, 7
MAP_W, MAP_H = 32, 28
STAIRS_DN = 6


def snapshot(gb):
    return {
        "px": gb.rd("g_px"), "py": gb.rd("g_py"),
        "depth": gb.rd("g_depth"), "hp": gb.rd("g_hp"),
        "gold": gb.rd16("g_gold"), "turns": gb.rd16("g_turns"),
        "food": gb.rd16("g_food"),
        "pack": gb.rdbuf("g_pack", PACK_SLOTS * STRIDE),
        "map": gb.rdbuf("g_map", MAP_W * MAP_H),
        "explored": gb.rdbuf("g_explored", 28 * 4),
        "wield": gb.rd("g_wield"), "worn": gb.rd("g_worn"),
        "known": gb.rdbuf("g_id_known", 8),
    }


def find_stairs(gb):
    raw = gb.rdbuf("g_map", MAP_W * MAP_H)
    for y in range(MAP_H):
        for x in range(MAP_W):
            if (raw[y * MAP_W + x] & 0x1F) == STAIRS_DN:
                return (x, y)
    return None


def try_descend(gb):
    """Teleport onto the down-stairs (if the current floor has any) and
    press A to descend, the same teleport-and-use approach verify_clear
    uses. Returns True iff g_depth actually changed."""
    st = find_stairs(gb)
    if st is None:
        return False
    before = gb.rd("g_depth")
    gb.pb.memory[gb.addr("g_px")] = st[0]
    gb.pb.memory[gb.addr("g_py")] = st[1]
    gb.tick(3)
    gb.press("a", hold=8, settle=8)
    for _ in range(300):
        gb.tick(1)
        if gb.rd("g_depth") != before:
            gb.tick(150)   # let the new floor's mapgen + fade settle
            return True
    return False


def search_here(gb, n, stats):
    """A bare A-tap with no adjacent monster and not on stairs runs
    wait_search() (src/game.c player_tap_a) -- rest and search were
    merged into one action, so this is how the current build drives
    the MF_HIDDEN reveal / STATIC-dirty path that used to be the
    dedicated Search menu item."""
    for _ in range(n):
        gb.press("a", hold=8, settle=10)
    stats["searched"] += n


# --------------------------------------------------------- 20 patterns
def p1(gb, stats):     # straight run right
    for _ in range(6):
        gb.press("right", hold=8, settle=10)


def p2(gb, stats):     # straight run down
    for _ in range(6):
        gb.press("down", hold=8, settle=10)


def p3(gb, stats):     # left then up
    for _ in range(3):
        gb.press("left", hold=8, settle=10)
    for _ in range(3):
        gb.press("up", hold=8, settle=10)


def p4(gb, stats):     # perimeter box loop
    for btn in ("right", "right", "down", "down", "left", "left", "up", "up"):
        gb.press(btn, hold=8, settle=10)


def p5(gb, stats):     # wide zig-zag exploration (grows EXPLORED)
    for btn in ("right", "right", "down", "down", "left", "up", "right",
                "down", "right", "up"):
        gb.press(btn, hold=6, settle=10)


def p6(gb, stats):     # down/right staircase walk
    for _ in range(4):
        gb.press("down", hold=6, settle=8)
        gb.press("right", hold=6, settle=8)


def p7(gb, stats):     # dash right, twice
    gb.combo("b", "right", hold=10, settle=20)
    gb.combo("b", "right", hold=10, settle=20)


def p8(gb, stats):     # dash down, twice
    gb.combo("b", "down", hold=10, settle=20)
    gb.combo("b", "down", hold=10, settle=20)


def p9(gb, stats):     # a plain step, then a dash the other way
    gb.press("right", hold=8, settle=10)
    gb.combo("b", "left", hold=10, settle=20)


def p10(gb, stats):    # four consecutive dashes around the compass
    for d in ("right", "down", "left", "up"):
        gb.combo("b", d, hold=10, settle=20)


def p11(gb, stats):    # single descent attempt
    if try_descend(gb):
        stats["descend_ok"] += 1
    stats["descend_tries"] += 1


def p12(gb, stats):    # walk, then two descent attempts back to back
    for _ in range(3):
        gb.press("down", hold=8, settle=10)
    for _ in range(2):
        stats["descend_tries"] += 1
        if try_descend(gb):
            stats["descend_ok"] += 1


def p13(gb, stats):    # walk a longer path, then descend
    for btn in ("right", "down", "right", "down"):
        gb.press(btn, hold=8, settle=10)
    stats["descend_tries"] += 1
    if try_descend(gb):
        stats["descend_ok"] += 1


def p14(gb, stats):    # descend, dash, then descend again on the new floor
    stats["descend_tries"] += 1
    if try_descend(gb):
        stats["descend_ok"] += 1
    gb.combo("b", "right", hold=10, settle=20)
    stats["descend_tries"] += 1
    if try_descend(gb):
        stats["descend_ok"] += 1


def p15(gb, stats):    # rest/search in place
    search_here(gb, 10, stats)


def p16(gb, stats):    # move, search, move, search (different counts)
    gb.press("right", hold=8, settle=10)
    search_here(gb, 6, stats)
    gb.press("down", hold=8, settle=10)
    search_here(gb, 4, stats)


def p17(gb, stats):    # search interleaved between single steps
    for btn in ("right", None, "right", None, "down", None):
        if btn is None:
            search_here(gb, 1, stats)
        else:
            gb.press(btn, hold=8, settle=10)


def p18(gb, stats):    # composite: move + dash + search + move
    gb.press("right", hold=8, settle=10)
    gb.combo("b", "down", hold=10, settle=20)
    search_here(gb, 3, stats)
    gb.press("left", hold=8, settle=10)


def p19(gb, stats):    # composite: dash + descend attempt + move
    gb.combo("b", "right", hold=10, settle=20)
    stats["descend_tries"] += 1
    if try_descend(gb):
        stats["descend_ok"] += 1
    for btn in ("down", "left", "up"):
        gb.press(btn, hold=8, settle=10)


def p20(gb, stats):    # heaviest composite: moves + dashes + search + descend
    for btn in ("right", "down", "right", "down"):
        gb.press(btn, hold=6, settle=8)
    gb.combo("b", "right", hold=10, settle=20)
    search_here(gb, 4, stats)
    stats["descend_tries"] += 1
    if try_descend(gb):
        stats["descend_ok"] += 1
    gb.combo("b", "down", hold=10, settle=20)
    search_here(gb, 2, stats)


PATTERNS = [p1, p2, p3, p4, p5, p6, p7, p8, p9, p10,
            p11, p12, p13, p14, p15, p16, p17, p18, p19, p20]


def run_one(i, fn, stats):
    """Boot pattern i with a distinct seed, drive its action sequence,
    then do the same suspend-save/title-CONTINUE/resume round trip as
    verify_m8 and check pre == post plus basic HUD/no-crash sanity."""
    gb = GB(boot_frames=240 + i * 4)
    try:
        gb.boot_game()
        fn(gb, stats)
        gb.tick(30)     # let glide/held-walk settle before snapshotting
        pre = snapshot(gb)

        # --- suspend (START menu -> Save & quit, closed-loop cursor)
        menu_on = lambda rows: any("MENU" in r for r in rows)
        gb.press_until("start", menu_on)
        for _ in range(20):
            cur = gb.cursor_row()
            if cur == 7:                     # Log/Display/Speed/Lang/Map/Quit
                break
            gb.press("down" if cur < 7 else "up", hold=6, settle=10)
        gb.expect(gb.cursor_row() == 7,
                  "menu cursor never reached Save & quit")
        gb.press("a", hold=8, settle=8)
        gb.expect(gb.wait_screen(lambda rows: any("Game saved" in r for r in rows)),
                  "save popup never appeared")
        gb.press_until("a", lambda rows: any("CONTINUE" in r for r in rows))
        gb.expect(gb.wait_screen(lambda rows: any("> CONTINUE" in r for r in rows)),
                  "continue not pre-selected on the title after suspend")
        gb.expect(gb.wait_screen(lambda rows: any("NEW GAME" in r for r in rows)),
                  "new game missing from the title menu")

        # --- resume ---
        gb.press_until("start",
                       lambda rows: any("Welcome back" in r for r in rows))
        # the world redraw (title art -> BG tilemap) is still fading in
        # when "Welcome back" first appears; ride it out before reading
        # screen state, same margin boot_game() gives the title->world
        # transition (fixed settles are unreliable across this repaint).
        gb.tick(40)
        post = snapshot(gb)

        for key in ("px", "py", "depth", "hp", "gold", "turns", "food",
                    "wield", "worn"):
            gb.expect(pre[key] == post[key],
                      f"{key} mismatch: pre={pre[key]} post={post[key]}")
        gb.expect(pre["pack"] == post["pack"], "pack mismatch after resume")
        gb.expect(pre["map"] == post["map"], "map (terrain) mismatch after resume")
        gb.expect(pre["explored"] == post["explored"],
                  "explored bitmap mismatch after resume")
        gb.expect(pre["known"] == post["known"], "identify state mismatch")

        # --- no-crash / HUD sanity ---
        gb.expect(post["hp"] > 0, "hp is 0 after resume")
        gb.expect(gb.status_row().strip() != "", "status row empty after resume")
        gb.player_pos()      # raises Failure unless exactly one '@' is on screen

        return True, None, pre["depth"]
    except Failure as e:
        return False, str(e), None
    finally:
        gb.stop()


def main() -> int:
    stats = {"descend_tries": 0, "descend_ok": 0, "searched": 0}
    passed = 0
    depth_gained_patterns = 0
    for i, fn in enumerate(PATTERNS, start=1):
        try:
            ok, err, depth = run_one(i, fn, stats)
        except Exception as e:   # boot/crash-level failure, not an assertion
            ok, err, depth = False, f"unhandled exception: {e!r}", None
        if ok:
            print(f"  ok   P{i}")
            passed += 1
            if depth is not None and depth > 1:
                depth_gained_patterns += 1
        else:
            print(f"  FAIL P{i}: {err}")

    print(f"verify_stress: {passed}/20 patterns passed")
    print(f"  depth actually increased in {depth_gained_patterns} pattern(s) "
          f"({stats['descend_ok']}/{stats['descend_tries']} descend attempts succeeded)")
    print(f"  search/rest attempted {stats['searched']} times total")
    return 0 if passed == 20 else 1


if __name__ == "__main__":
    sys.exit(main())
