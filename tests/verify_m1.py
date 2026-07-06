#!/usr/bin/env python3
"""M1: ROM boots to the title, START enters the dungeon, the player renders."""
from gbtest import GB, Failure, ROOT


def main() -> int:
    gb = GB()

    # flash-cart boot stub: entry must set ROM bank 1 before crt0
    # (EverDrive-style menus can leave the MBC1 bank register off 1,
    # and crt0 calls upper-HOME code on its first instructions)
    rom = (ROOT / "build" / "gbrogue.gb").read_bytes()
    gb.expect(rom[0x100:0x103] == b"\xC3\x08\x00" and rom[0x08] == 0x67,
              "flash-cart boot stub missing (scripts/fix_boot.py)")

    # title art fills the screen (cells reference the high art slots
    # that no text machinery ever allocates) with the menu on top;
    # the cart ships in Japanese with the LANGUAGE row on JPN
    gb.expect_on_screen("> はじめる")
    gb.expect_on_screen("LANGUAGE:JPN")
    art_cells = sum(1 for y in range(12) for x in range(20)
                    if gb.pb.memory[0x9800 + 32 * y + x] >= 120)
    gb.expect(art_cells > 60, f"title art missing ({art_cells} art cells)")
    gb.shot("m1_01_title")

    gb.boot_game()
    x, y = gb.player_pos()          # exactly one '@' on screen
    gb.expect_on_screen("B1 LV1 HP12/12")
    gb.expect_on_screen("Welcome to the tomb")
    gb.shot("m1_02_dungeon")

    gb.stop()
    print("verify_m1: all checks passed")
    return 0


if __name__ == "__main__":
    import sys
    try:
        sys.exit(main())
    except Failure as e:
        print(f"verify_m1 FAILED: {e}")
        sys.exit(1)
