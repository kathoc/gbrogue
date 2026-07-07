#!/usr/bin/env python3
"""
M8: suspend save, resume, single-use save (anti-scum), permadeath wipe.

Uses PyBoy memory *writes* to force a quick death for the wipe check.
"""
from gbtest import GB, Failure

PACK_SLOTS, STRIDE = 16, 7


def snapshot(gb):
    return {
        "px": gb.rd("g_px"), "py": gb.rd("g_py"),
        "depth": gb.rd("g_depth"), "hp": gb.rd("g_hp"),
        "gold": gb.rd16("g_gold"), "turns": gb.rd16("g_turns"),
        "food": gb.rd16("g_food"),
        "pack": gb.rdbuf("g_pack", PACK_SLOTS * STRIDE),
        "map": gb.rdbuf("g_map", 32 * 28),
        "explored": gb.rdbuf("g_explored", 28 * 4),  # 112B EXPLORED bitmap
        "wield": gb.rd("g_wield"), "worn": gb.rd("g_worn"),
        "known": gb.rdbuf("g_id_known", 8),
    }


def main() -> int:
    gb = GB()

    # fresh boot: no save -> no continue offer
    gb.expect(not any("SELECT: continue" in r for r in gb.screen_rows()),
              "fresh boot should not offer continue")

    gb.boot_game()

    # play a little so the state is distinctive
    for btn in ("right", "down", "right", "a", "left"):
        gb.press(btn, hold=6, settle=10)
    # let the last step's glide/held-walk settle before snapshotting, else
    # a straggler move lands between the snapshot and the suspend and the
    # resume check sees an off-by-one position (map-timing sensitive).
    gb.tick(30)
    pre = snapshot(gb)

    # --- suspend (START menu -> Save & quit, closed-loop cursor)
    menu_on = lambda rows: any("MENU" in r for r in rows)
    gb.press_until("start", menu_on)
    for _ in range(20):
        cur = gb.cursor_row()
        if cur == 6:                         # Save & quit row (2 + 4: Log/Display/Speed/Lang/Quit)
            break
        gb.press("down" if cur < 6 else "up", hold=6, settle=10)
    gb.expect(gb.cursor_row() == 6, "menu cursor never reached Save & quit")
    gb.press("a", hold=8, settle=8)
    gb.expect(gb.wait_screen(lambda rows: any("Game saved" in r for r in rows)),
              "save popup never appeared")
    gb.shot("m8_01_saved")
    gb.press_until("a", lambda rows: any("CONTINUE" in r for r in rows))
    # CONTINUE leads the menu and starts selected; NEW GAME sits below
    gb.expect(gb.wait_screen(lambda rows: any("> CONTINUE" in r for r in rows)),
              "continue not pre-selected on the title after suspend")
    # the NEW GAME row flushes a couple frames later — wait, don't peek
    gb.expect(gb.wait_screen(lambda rows: any("NEW GAME" in r for r in rows)),
              "new game missing from the title menu")
    gb.shot("m8_02_title_continue")

    # --- resume: START confirms the pre-selected CONTINUE
    gb.press_until("start",
                   lambda rows: any("Welcome back" in r for r in rows))
    post = snapshot(gb)
    for key in ("px", "py", "depth", "hp", "gold", "turns", "food",
                "wield", "worn"):
        gb.expect(pre[key] == post[key],
                  f"{key} mismatch after resume: {pre[key]} != {post[key]}")
    gb.expect(pre["pack"] == post["pack"], "pack mismatch after resume")
    gb.expect(pre["map"] == post["map"], "map (terrain) mismatch after resume")
    gb.expect(pre["explored"] == post["explored"],
              "explored bitmap mismatch after resume")
    gb.expect(pre["known"] == post["known"], "identify state mismatch")
    print("  resume state matches suspend state")

    # --- the save was consumed at load: suspend-scumming impossible.
    #     Force death and confirm the title no longer offers continue.
    gb.pb.memory[gb.addr("g_hp")] = 1
    # rest until something kills us, or starve via food
    gb.pb.memory[gb.addr("g_food")] = 30
    gb.pb.memory[gb.addr("g_food") + 1] = 0
    for _ in range(80):
        gb.press("a", hold=6, settle=8)
        if gb.rd("g_hp") == 0:
            break
    gb.expect(gb.rd("g_hp") == 0, "could not force death")
    # the death sequence (1/8-speed killing blow + 2s red wipe + slow
    # fade-in) delays R.I.P. by a few hundred frames, so give it room
    gb.expect(gb.wait_screen(lambda rows: any("R.I.P." in r for r in rows), 720),
              "game-over screen never appeared")
    gb.shot("m8_03_dead")
    gb.press_until("start", lambda rows: any("NEW GAME" in r for r in rows))
    # save wiped -> the menu offers a selected NEW GAME and no CONTINUE
    gb.expect(gb.wait_screen(lambda rows: any("> NEW GAME" in r for r in rows)),
              "title never finished drawing")
    gb.tick(30)
    gb.expect(not any("CONTINUE" in r for r in gb.screen_rows()),
              "save survived death — permadeath broken!")
    gb.shot("m8_04_title_no_save")
    print("  permadeath: save wiped on death")

    gb.stop()
    print("verify_m8: all checks passed")
    return 0


if __name__ == "__main__":
    import sys
    try:
        sys.exit(main())
    except Failure as e:
        print(f"verify_m8 FAILED: {e}")
        sys.exit(1)
