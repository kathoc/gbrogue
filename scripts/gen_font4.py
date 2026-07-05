#!/usr/bin/env python3
"""
Generate the 4x8 half-width UI font (assets/font4_data.c).

Each ASCII char 0x20..0x7E gets 8 bytes: one byte per tile row with the
3x5 glyph pre-shifted into the HIGH nibble (pixel column 3 is the gap).
Two chars compose into one 8x8 tile: left | (right >> 4).
Lowercase letters reuse the uppercase shapes (small-caps look).
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "assets" / "font4_data.c"

# 3x5 glyphs; strings of '*'/'.' 3 wide, 5 tall (rows land on tile rows 1..5)
G = {
    "!": ".*. .*. .*. ... .*.",
    '"': "*.* *.* ... ... ...",
    "#": "*.* *** *.* *** *.*",
    "$": ".** **. .*. .** **.",
    "%": "*.. ..* .*. *.. ..*",
    "&": ".*. *.* .*. *.* .**",
    "'": ".*. .*. ... ... ...",
    "(": "..* .*. .*. .*. ..*",
    ")": "*.. .*. .*. .*. *..",
    "*": "... *.* .*. *.* ...",
    "+": "... .*. *** .*. ...",
    ",": "... ... ... .*. *..",
    "-": "... ... *** ... ...",
    ".": "... ... ... ... .*.",
    "/": "..* ..* .*. *.. *..",
    "0": "*** *.* *.* *.* ***",
    "1": ".*. **. .*. .*. ***",
    "2": "*** ..* *** *.. ***",
    "3": "*** ..* .** ..* ***",
    "4": "*.* *.* *** ..* ..*",
    "5": "*** *.. *** ..* ***",
    "6": ".** *.. *** *.* ***",
    "7": "*** ..* .*. .*. .*.",
    "8": "*** *.* *** *.* ***",
    "9": "*** *.* *** ..* **.",
    ":": "... .*. ... .*. ...",
    ";": "... .*. ... .*. *..",
    "<": "..* .*. *.. .*. ..*",
    "=": "... *** ... *** ...",
    ">": "*.. .*. ..* .*. *..",
    "?": "**. ..* .*. ... .*.",
    "@": ".*. *.* *** *.. .**",
    "A": ".*. *.* *** *.* *.*",
    "B": "**. *.* **. *.* **.",
    "C": ".** *.. *.. *.. .**",
    "D": "**. *.* *.* *.* **.",
    "E": "*** *.. **. *.. ***",
    "F": "*** *.. **. *.. *..",
    "G": ".** *.. *.* *.* .**",
    "H": "*.* *.* *** *.* *.*",
    "I": "*** .*. .*. .*. ***",
    "J": ".** ..* ..* *.* .*.",
    "K": "*.* **. *.. **. *.*",
    "L": "*.. *.. *.. *.. ***",
    "M": "*.* *** *** *.* *.*",
    "N": "**. *.* *.* *.* *.*",
    "O": ".*. *.* *.* *.* .*.",
    "P": "**. *.* **. *.. *..",
    "Q": ".*. *.* *.* .*. ..*",
    "R": "**. *.* **. **. *.*",
    "S": ".** *.. .*. ..* **.",
    "T": "*** .*. .*. .*. .*.",
    "U": "*.* *.* *.* *.* ***",
    "V": "*.* *.* *.* *.* .*.",
    "W": "*.* *.* *** *** *.*",
    "X": "*.* *.* .*. *.* *.*",
    "Y": "*.* *.* .*. .*. .*.",
    "Z": "*** ..* .*. *.. ***",
    "[": ".** .*. .*. .*. .**",
    "\\": "*.. *.. .*. ..* ..*",
    "]": "**. .*. .*. .*. **.",
    "^": ".*. *.* ... ... ...",
    "_": "... ... ... ... ***",
    "`": "*.. .*. ... ... ...",
    "{": "..* .*. **. .*. ..*",
    "|": ".*. .*. .*. .*. .*.",
    "}": "*.. .*. .** .*. *..",
    "~": "... .** **. ... ...",
}


def rows_for(ch: str) -> list[int]:
    if ch == " ":
        return [0] * 8
    src = G.get(ch) or G.get(ch.upper())
    if src is None:
        src = G["?"]
    rows5 = src.split()
    out = [0]
    for r in rows5:
        v = 0
        for i, c in enumerate(r):
            if c == "*":
                v |= 1 << (7 - i)      # pixel columns 0..2
        out.append(v)                  # already high-aligned in the byte
    out += [0, 0]
    return out


def main():
    lines = [
        "/*",
        " * GENERATED FILE — see scripts/gen_font4.py",
        " *",
        " * 4x8 half-width UI font, chars 0x20..0x7E, 8 bytes per char",
        " * (glyph in the high nibble). text4.c composes two chars into",
        " * one 8x8 tile: left | (right >> 4). Lives in ROM bank 2 and is",
        " * fetched through a NONBANKED helper.",
        " */",
        "#include <gb/gb.h>",
        "#include <stdint.h>",
        "",
        "#pragma bank 2",
        "",
        "BANKREF(FONT4)",
        "const uint8_t FONT4[95][8] = {",
    ]
    for code in range(0x20, 0x7F):
        ch = chr(code)
        bs = rows_for(ch)
        hexes = ", ".join(f"0x{b:02X}" for b in bs)
        lines.append(f"    /* 0x{code:02X} {ch!r:<5}*/ {{ {hexes} }},")
    lines.append("};")
    lines.append("")
    OUT.write_text("\n".join(lines))
    print(f"wrote {OUT.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
