#!/usr/bin/env python3
"""
Japanese language support (Misaki font) + language toggle + SELECT-hold
full-map overview.
"""
from gbtest import GB, Failure
from lang_map import STRINGS

MENU_LANG_ROW = 2 + 6          # Language entry (index 6, after Log was added)
MENU_ON = lambda rows: any("MENU" in r for r in rows)
MENU_ON_JA = lambda rows: any(STRINGS["MENU_TITLE"][1] in r for r in rows)


def menu_cursor_to(gb, want):
    for _ in range(24):
        cur = gb.cursor_row()
        if cur == want:
            return
        gb.press("down" if 0 <= cur < want else "up", hold=6, settle=10)
    gb.expect(False, f"menu cursor never reached row {want}")


def main() -> int:
    gb = GB()

    # ---------------- title: ships in Japanese, LANGUAGE row toggles
    gb.expect(gb.wait_screen(lambda rows: any("> はじめる" in r for r in rows)),
              "title not in Japanese by default")
    gb.expect_on_screen("LANGUAGE:JPN")
    # rows are NEW GAME(0) / RANKING(1) / LANGUAGE(2): step down twice
    gb.press("down", hold=8, settle=20)        # onto RANKING
    gb.press("down", hold=8, settle=20)        # onto LANGUAGE
    gb.expect(gb.wait_screen(
        lambda rows: any("> LANGUAGE:JPN" in r for r in rows)),
        "LANGUAGE row never took the cursor")
    gb.press("a", hold=8, settle=20)           # A toggles to English
    gb.expect(gb.wait_screen(
        lambda rows: any("LANGUAGE:ENG" in r for r in rows)),
        "A did not switch the language to English")
    gb.expect(gb.wait_screen(lambda rows: any("NEW GAME" in r for r in rows)),
              "menu labels still Japanese after the toggle")
    gb.press("left", hold=8, settle=20)        # left/right toggles too
    gb.expect(gb.wait_screen(
        lambda rows: any("LANGUAGE:JPN" in r for r in rows)),
        "LEFT did not toggle back to Japanese")
    print("  title LANGUAGE row toggles JPN <-> ENG")

    gb.boot_game()

    # ---------------- SELECT hold -> full map; tap -> pack
    gb.tick(20)
    gb.hold("select")
    gb.tick(60)
    gb.expect(gb.wait_screen(lambda rows: any("MAP" in r for r in rows)),
              "map overview never appeared")
    rows = gb.screen_rows()
    chunks = sum(r.count("#") for r in rows)      # raw minimap tiles
    gb.expect(chunks >= 20, f"minimap barely drawn ({chunks} chunks)")
    gb.shot("lang_00_map")
    gb.release("select")
    gb.expect(gb.wait_screen(lambda rows: any("@" in r for r in rows[:16])),
              "world not restored after map")
    print(f"  SELECT hold shows the map ({chunks} chunks)")
    gb.press_until("select",
                   lambda rows: any("PACK" in r for r in rows))
    gb.press_until("b", lambda rows: not any("PACK" in r for r in rows))
    print("  SELECT tap still opens the pack")

    # ---------------- toggle to Japanese via the menu
    gb.tick(20)
    gb.press_until("start", MENU_ON)
    menu_cursor_to(gb, MENU_LANG_ROW)
    gb.press("a", hold=8, settle=20)
    gb.expect(gb.rd("g_lang") == 1, "language flag did not flip")
    gb.expect(gb.wait_screen(MENU_ON_JA), "menu did not redraw in Japanese")
    gb.expect_on_screen(STRINGS["MENU_REST"][1])      # やすむ
    gb.shot("lang_01_menu_ja")
    gb.press_until("b", lambda rows: not MENU_ON_JA(rows))

    # a rest posts the Japanese message (clear monsters so no attack
    # line overwrites it in the same turn)
    base = gb.addr("g_mons")
    for i in range(12):
        gb.pb.memory[base + i * 7] = 0xFF
    gb.tick(20)
    gb.hold("b")
    gb.tick(4)
    gb.press("a", hold=8, settle=8)
    gb.release("b")
    gb.tick(40)
    gb.expect(gb.wait_screen(
        lambda rows: STRINGS["YOU_WAIT"][1] in rows[16]),
        f"JA rest message missing: {gb.message_row()!r}")
    gb.shot("lang_02_msg_ja")
    print("  Japanese messages render (Misaki)")

    # pack opens with the JA hint line (odd-length ASCII runs get a pad
    # space before kana, so match the kana word rather than the full row)
    gb.press_until("select", lambda rows: "えらぶ" in rows[17])
    gb.shot("lang_03_pack_ja")
    gb.press_until("b", lambda rows: "えらぶ" not in rows[17])
    print("  Japanese pack screen renders")

    # ---------------- toggle back to English
    gb.tick(20)
    gb.press_until("start", MENU_ON_JA)
    menu_cursor_to(gb, MENU_LANG_ROW)
    gb.press("a", hold=8, settle=20)
    gb.expect(gb.rd("g_lang") == 0, "language did not flip back")
    gb.expect(gb.wait_screen(MENU_ON), "menu did not redraw in English")
    gb.press_until("b", lambda rows: not MENU_ON(rows))
    print("  toggled back to English")

    gb.stop()
    print("verify_lang: all checks passed")
    return 0


if __name__ == "__main__":
    import sys
    try:
        sys.exit(main())
    except Failure as e:
        print(f"verify_lang FAILED: {e}")
        sys.exit(1)
