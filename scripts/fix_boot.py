#!/usr/bin/env python3
"""
Flash-cart boot fix. GBDK's crt0 never initializes the MBC1 ROM-bank
register and calls upper-HOME (0x4000+) code on its very first
instructions. Emulators and cold-booted consoles power up with the
bank register at 0 (which maps bank 1), so it works there — but flash
carts (EverDrive GB, GB USB smart card, ...) jump in from a menu OS
that may leave the register on another bank, and the ROM dies at boot.

This post-link pass injects, at the unused RST 0x08 slot:

    ld  h,a          ; keep the boot ROM's A (CGB detect) intact
    ld  a,1
    ld  (0x2000),a   ; ROM bank 1 -> flat HOME is whole again
    ld  a,h
    jp  0x0157       ; original crt0 entry

and repoints the cartridge entry (0x100) at it, then refreshes the
global checksum. Idempotent.
"""
import sys
from pathlib import Path

STUB_AT = 0x08
CRT0 = 0x0157
STUB = bytes([0x67,                       # ld h,a
              0x3E, 0x01,                 # ld a,1
              0xEA, 0x00, 0x20,           # ld (0x2000),a
              0x7C,                       # ld a,h
              0xC3, CRT0 & 0xFF, CRT0 >> 8])  # jp crt0
ENTRY = bytes([0xC3, STUB_AT, 0x00])      # jp stub


def fix(path: Path) -> None:
    d = bytearray(path.read_bytes())
    if bytes(d[0x100:0x103]) == ENTRY:
        print(f"{path.name}: already boot-fixed")
        return
    if bytes(d[0x100:0x102]) != b"\x18\x55":
        raise SystemExit(f"{path.name}: unexpected entry "
                         f"{d[0x100:0x104].hex()} (crt0 changed?)")
    free = all(b == 0xFF for b in d[STUB_AT:STUB_AT + len(STUB)])
    if not free:
        raise SystemExit(f"{path.name}: RST 0x08 slot not free")
    d[STUB_AT:STUB_AT + len(STUB)] = STUB
    d[0x100:0x103] = ENTRY
    gc = (sum(d) - d[0x14E] - d[0x14F]) & 0xFFFF
    d[0x14E] = gc >> 8
    d[0x14F] = gc & 0xFF
    path.write_bytes(bytes(d))
    print(f"{path.name}: boot stub installed (bank 1 before crt0)")


if __name__ == "__main__":
    for arg in sys.argv[1:]:
        fix(Path(arg))
