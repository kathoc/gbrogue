"""
Headless test harness for gb-rogue-org.

Boots the ROM in PyBoy with no window and exposes:

  * button scripting        gb.press("start"), gb.combo("a", "right")
  * screen-as-text          gb.screen_rows() decodes the BG tile map back
                            into ASCII (tile index i == chr(i + 0x20),
                            guaranteed by the renderer's atlas layout)
  * WRAM peeking            gb.rd("g_px") via the no$gmb .sym file
  * screenshots             gb.shot("name") -> build/shots/name.png

Every verify_*.py script builds on this. Keep assertions on decoded text
or WRAM values — pixels are only for humans.
"""
from __future__ import annotations

import os
import re
import sys
from pathlib import Path

from pyboy import PyBoy

# Watch mode: set GBR_WATCH=1 to open a real PyBoy window and run at
# real-time so you can SEE a verify_*.py script play out (headless + full
# speed otherwise). GBR_WATCH=<n> throttles to n x real-time (e.g. 2 = 2x,
# 0 = unlimited-but-visible). Example:
#   GBR_WATCH=1 python tests/verify_clear.py
_WATCH = os.environ.get("GBR_WATCH")

ROOT = Path(__file__).resolve().parent.parent
ROM = ROOT / "build" / "gbrogue.gb"
SYM = ROOT / "build" / "gbrogue.noi"   # no$gmb symbols: "DEF _name 0xADDR"
SHOTS = ROOT / "build" / "shots"

BG_MAP = 0x9800
ATLAS_FIRST = 0x20
ATLAS_COUNT = 95


class Failure(AssertionError):
    pass


class GB:
    def __init__(self, rom: Path = ROM, keep_save: bool = False,
                 boot_frames: int = 240, cgb: bool = True,
                 sound: bool = False):
        """Defaults to a real CGB machine: with a CGB-flagged cart,
        PyBoy's cgb=False still runs the CGB boot ROM (A=0x11) on DMG
        silicon — a half-state no real hardware has. DMG behavior is
        tested via the header-patched copy in verify_gbc.
        sound=True emulates the APU so NR52 reads back channel state
        (verify_sfx); off elsewhere to keep the runs fast."""
        if not rom.exists():
            raise Failure(f"ROM not found: {rom} (run `make` first)")
        if not keep_save:
            sav = rom.with_suffix(".sav")
            if sav.exists():
                sav.unlink()
        window = "SDL2" if _WATCH is not None else "null"
        self.pb = PyBoy(str(rom), window=window, sound_emulated=sound, cgb=cgb)
        if _WATCH is not None:
            try:
                speed = float(_WATCH)
            except ValueError:
                speed = 1.0
            # 0 = unlimited; anything else throttles to n x real-time so the
            # run is actually watchable instead of an instant blur.
            self.pb.set_emulation_speed(speed if speed >= 0 else 1.0)
        self.sym_path = rom.with_suffix(".noi")
        self._syms: dict[str, tuple[int, int]] | None = None
        self.tick(boot_frames)

    # ------------------------------------------------------------ driving
    def tick(self, n: int = 1) -> None:
        for _ in range(n):
            self.pb.tick()

    def press(self, btn: str, hold: int = 8, settle: int = 16) -> None:
        self.pb.button_press(btn)
        self.tick(hold)
        self.pb.button_release(btn)
        self.tick(settle)

    def boot_game(self) -> None:
        """Confirm the title menu and wait out the fade-out + level
        load + fade-in until the world's status row is up. The cart
        ships in Japanese; tests run in English, so flip the language
        first (the title's LANGUAGE row covers the UI path in
        verify_lang)."""
        self.pb.memory[self.addr("g_lang")] = 0
        self.press("start", hold=8, settle=8)
        if not self.wait_screen(
                lambda rows: rows[17].lstrip().startswith("B"), 300):
            raise Failure(f"game never started from the title\n{self.screen()}")
        # the status row decodes while the palette fade-in is still
        # running; ride it out so the first test input isn't queued
        # behind the fade (FADE_IN_FRAMES=30 + margin)
        self.tick(40)

    def combo(self, *btns: str, hold: int = 8, settle: int = 16) -> None:
        for b in btns:
            self.pb.button_press(b)
        self.tick(hold)
        for b in reversed(btns):
            self.pb.button_release(b)
        self.tick(settle)

    def hold(self, *btns: str) -> None:
        for b in btns:
            self.pb.button_press(b)

    def release(self, *btns: str) -> None:
        for b in btns:
            self.pb.button_release(b)

    def stop(self) -> None:
        self.pb.stop(save=False)

    # ------------------------------------------------------------ screen
    # VRAM layout (see src/render.c): 0..44 full-width glyphs,
    # 45..62 graphic tiles, 63..82 fixed message-row tiles (streamed
    # bitmaps — decoded via the g_last_msg string instead),
    # 83.. composed half-width text pool.
    TILE8_COUNT = 45
    GFX_BASE = 45
    MSGROW_BASE = 63
    T4_BASE = 83

    def _decode_msg_bytes(self, raw: bytes) -> str:
        try:
            from lang_map import KANA
        except ImportError:
            KANA = {}
        out = []
        for b in raw:
            if b == 0:
                break
            if b >= 0x80 or 0x02 <= b < 0x20:
                out.append(KANA.get(b, "?"))
            else:
                out.append(chr(b))
        return "".join(out).ljust(40)

    def message_band(self) -> str:
        """World-mode message line (streamed pixels; read the string)."""
        return self._decode_msg_bytes(self.rdbuf("g_last_msg", 43))

    def _tile8_chars(self) -> bytes:
        if not hasattr(self, "_t8"):
            self._t8 = self.rdbuf("TILE8_CHARS", self.TILE8_COUNT)
        return self._t8

    def _tile_char(self, t: int) -> str:
        if t < self.TILE8_COUNT:
            return chr(self._tile8_chars()[t])
        return "?"

    def _t4_map(self) -> dict[int, str]:
        """Composed text tiles -> their characters (ASCII pairs, kana
        full-width glyphs, or raw minimap chunks)."""
        try:
            from lang_map import KANA
        except ImportError:
            KANA = {}
        try:
            used = self.rd("g_t4_used")
            raw = self.rdbuf("g_t4_keys", used * 2)
        except Failure:
            return {}
        out = {}
        for i in range(used):
            key = raw[2 * i] | (raw[2 * i + 1] << 8)
            hi = key >> 8
            if hi == 0xFF:                      # full-width kana glyph
                out[self.T4_BASE + i] = KANA.get(key & 0xFF, "?")
            elif hi == 0xFD:                    # raw bitmap (minimap)
                out[self.T4_BASE + i] = "#"
            else:                               # half-width ASCII pair
                out[self.T4_BASE + i] = chr(hi) + chr(key & 0xFF)
        return out

    def _expand(self, t: int, dyn: dict[int, str]) -> str:
        if t >= self.T4_BASE:
            return dyn.get(t, "??")
        return self._tile_char(t)

    def screen_rows(self) -> list[str]:
        """Decode the visible screen to 18 rows of 20 chars.

        Overlay mode (window off): straight read of BG rows 0..17.
        World mode (window on): BG viewport through SCX/SCY (16 rows),
        then the two Window rows, with OAM sprites (player '@', monster
        letters) composited onto the viewport.
        """
        mem = self.pb.memory
        lcdc = mem[0xFF40]
        dyn = self._t4_map()
        rows: list[str]
        if lcdc & 0x20:  # window enabled -> world mode
            scx, scy = mem[0xFF43], mem[0xFF42]
            tx, ty = scx // 8, scy // 8
            grid = []
            for y in range(16):
                base = BG_MAP + 32 * ((ty + y) % 32)
                grid.append([self._tile_char(mem[base + ((tx + x) % 32)])
                             for x in range(20)])
            for i in range(40):
                oy = mem[0xFE00 + 4 * i]
                ox = mem[0xFE00 + 4 * i + 1]
                tile = mem[0xFE00 + 4 * i + 2]
                if oy == 0:
                    continue
                cx, cy = (ox - 8) // 8, (oy - 16) // 8
                if 0 <= cx < 20 and 0 <= cy < 16:
                    grid[cy][cx] = self._tile_char(tile)
            rows = ["".join(r) for r in grid]
            wmap = 0x9C00 if lcdc & 0x40 else 0x9800
            rows.append(self.message_band())     # streamed-bitmap band
            base = wmap + 32
            rows.append("".join(self._expand(mem[base + x], dyn)
                                for x in range(20)))
        else:
            rows = []
            for y in range(18):
                base = BG_MAP + 32 * y
                rows.append("".join(self._expand(mem[base + x], dyn)
                                    for x in range(20)))
        return rows

    def screen(self) -> str:
        return "\n".join(self.screen_rows())

    def viewport(self) -> list[str]:
        return self.screen_rows()[:16]

    def status_row(self) -> str:
        return self.screen_rows()[17]

    def message_row(self) -> str:
        return self.screen_rows()[16]

    def find_char(self, ch: str, rows: list[str] | None = None) -> list[tuple[int, int]]:
        rows = self.viewport() if rows is None else rows
        return [(x, y) for y, row in enumerate(rows) for x, c in enumerate(row) if c == ch]

    def player_pos(self) -> tuple[int, int]:
        hits = self.find_char("@")
        if len(hits) != 1:
            raise Failure(f"expected exactly one '@', found {hits}\n{self.screen()}")
        return hits[0]

    def shot(self, name: str) -> Path:
        SHOTS.mkdir(parents=True, exist_ok=True)
        path = SHOTS / f"{name}.png"
        self.pb.screen.image.save(path)
        return path

    # ------------------------------------------------------------ symbols
    def _load_syms(self) -> dict[str, tuple[int, int]]:
        if self._syms is None:
            syms: dict[str, tuple[int, int]] = {}
            if self.sym_path.exists():
                for line in self.sym_path.read_text().splitlines():
                    m = re.match(r"^DEF\s+_(\w+)\s+0x([0-9A-Fa-f]+)", line)
                    if m:
                        syms[m.group(1)] = (0, int(m.group(2), 16))
            self._syms = syms
        return self._syms

    def addr(self, name: str) -> int:
        syms = self._load_syms()
        if name not in syms:
            raise Failure(f"symbol _{name} not in {self.sym_path}")
        return syms[name][1]

    def rd(self, name: str, offset: int = 0) -> int:
        return self.pb.memory[self.addr(name) + offset]

    def rd16(self, name: str, offset: int = 0) -> int:
        a = self.addr(name) + offset
        return self.pb.memory[a] | (self.pb.memory[a + 1] << 8)

    def rdbuf(self, name: str, length: int, offset: int = 0) -> bytes:
        a = self.addr(name) + offset
        return bytes(self.pb.memory[a + i] for i in range(length))

    # Deterministic debug items for M7/M9. The debug ROM used to build
    # these on-cart, but that overflowed HOME once the button-icon UI
    # landed; they are injected straight into g_pack after boot instead.
    # Columns: kind, sub, qty, ench, flags. g_food's short fuse still
    # comes from the ROM (GBR_DEBUG_KIT).
    DEBUG_KIT = [
        (1, 5, 1, 0, 0),     # healing potion
        (1, 0, 1, 0, 0),     # confusion potion
        (2, 1, 1, 0, 0),     # scroll of magic mapping
        (3, 6, 5, 0, 0),     # wand of missiles (5 charges)
        (4, 9, 1, 1, 0),     # ring +1
        (5, 4, 1, 0xFF, 1),  # cursed dagger (-1, IF_CURSED)
        (5, 7, 5, 0, 0),     # shuriken x5 (M9 throw test)
    ]

    def inject_debug_kit(self) -> None:
        """Write the M7/M9 test items into g_pack, filling the free slots
        right after the 5-item starting kit."""
        base = self.addr("g_pack")
        stride, none = 8, 9      # item_t: kind,sub,x,y,qty,ench,sench,flags
        slot = 0
        while slot < 16 and self.pb.memory[base + slot * stride] != none:
            slot += 1
        for k, sub, qty, ench, flags in self.DEBUG_KIT:
            a = base + slot * stride
            self.pb.memory[a + 0] = k
            self.pb.memory[a + 1] = sub
            self.pb.memory[a + 2] = 0        # x
            self.pb.memory[a + 3] = 0        # y
            self.pb.memory[a + 4] = qty
            self.pb.memory[a + 5] = ench
            self.pb.memory[a + 6] = 0        # sench
            self.pb.memory[a + 7] = flags
            slot += 1
        # short hunger fuse for the M7 hunger check (was set on-cart)
        fa = self.addr("g_food")
        self.pb.memory[fa] = 340 & 0xFF
        self.pb.memory[fa + 1] = (340 >> 8) & 0xFF

    # ------------------------------------------------------------ asserts
    def expect(self, cond: bool, why: str) -> None:
        if not cond:
            raise Failure(f"{why}\n--- screen ---\n{self.screen()}")

    def expect_on_screen(self, text: str) -> None:
        self.expect(any(text in row for row in self.screen_rows()),
                    f"expected {text!r} somewhere on screen")

    # ------------------------------------------------------- state sync
    def wait_screen(self, pred, frames: int = 240) -> bool:
        """Tick until pred(rows) is true; heavy repaints make fixed
        settles unreliable, so modal transitions should sync on this."""
        for _ in range(frames):
            if pred(self.screen_rows()):
                return True
            self.tick(1)
        return False

    def press_until(self, btn: str, pred, tries: int = 5,
                    frames: int = 90) -> None:
        """Press btn (re-pressing like a human) until pred(rows) holds.
        Retries absorb edges eaten by modal-transition swallows."""
        for _ in range(tries):
            self.press(btn, hold=8, settle=8)
            if self.wait_screen(pred, frames):
                return
        raise Failure(f"screen never satisfied predicate after {btn!r} x{tries}\n"
                      f"--- screen ---\n{self.screen()}")

    def cursor_row(self) -> int:
        """Row index of the '>' cursor on a modal list screen (pack has
        it at column 0, the menu at column 2), or -1. Only meaningful
        while a modal UI is open (the world may contain '>' stairs)."""
        for i, row in enumerate(self.screen_rows()):
            if ">" in row[:4]:
                return i
        return -1


def check(name: str, fn) -> bool:
    try:
        fn()
        print(f"  ok    {name}")
        return True
    except Failure as e:
        print(f"  FAIL  {name}: {e}")
        return False
