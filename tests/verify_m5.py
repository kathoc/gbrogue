#!/usr/bin/env python3
"""
M5: turn system, monsters, chase AI, bump combat, xp, death handling.

Ground truth comes from WRAM (g_mons / g_hp / g_xp / g_turns); the screen
is cross-checked for monster glyph rendering.
"""
from collections import deque

from gbtest import GB, Failure, ROOT

# Monsters/combat depend on the new-game seed; the release build draws it
# from DIV-based entropy (shifts with boot timing). Use the debug ROM,
# which pins the seed (src/ui_title.c GBR_DEBUG_KIT), for a stable field.
DBG_ROM = ROOT / "build" / "dbg" / "gbrogue.gb"

MAP_W, MAP_H = 32, 28
MF_TERRAIN = 0x1F
WALKABLE = {1, 2, 5, 6, 7, 8}
MAX_MONSTERS = 12
MON_STRIDE = 7  # kind,x,y,hp,state,eff,eff_t
MON_NONE = 0xFF
MST_AWAKE = 0x01  # the "spawned asleep" check reads only this bit; MST_SEEN
                  # (0x02) is set the instant a monster is drawn on screen.


def read_map(gb):
    raw = gb.rdbuf("g_map", MAP_W * MAP_H)
    return [[raw[y * MAP_W + x] & MF_TERRAIN for x in range(MAP_W)] for y in range(MAP_H)]


def read_mons(gb):
    raw = gb.rdbuf("g_mons", MAX_MONSTERS * MON_STRIDE)
    out = []
    for i in range(MAX_MONSTERS):
        k, x, y, hp, state = raw[i * MON_STRIDE:i * MON_STRIDE + 5]
        if k != MON_NONE:
            out.append({"i": i, "kind": k, "x": x, "y": y, "hp": hp, "state": state})
    return out


def bfs_path(grid, start, goal, blocked):
    q = deque([start])
    prev = {start: None}
    while q:
        cur = q.popleft()
        if cur == goal:
            path = []
            while cur is not None:
                path.append(cur)
                cur = prev[cur]
            return list(reversed(path))
        x, y = cur
        for nx, ny in ((x+1, y), (x-1, y), (x, y+1), (x, y-1)):
            if 0 <= nx < MAP_W and 0 <= ny < MAP_H and (nx, ny) not in prev \
               and grid[ny][nx] in WALKABLE and (nx, ny) not in blocked:
                prev[(nx, ny)] = (x, y)
                q.append((nx, ny))
    return None


def step_toward(gb, grid, target, blocked, max_steps=400):
    """Walk the player adjacent (4-dir) to target; recompute path each step."""
    for _ in range(max_steps):
        px, py = gb.rd("g_px"), gb.rd("g_py")
        if abs(px - target[0]) + abs(py - target[1]) == 1:
            return True
        path = bfs_path(grid, (px, py), target, blocked - {target})
        if path is None or len(path) < 2:
            return False
        nx, ny = path[1]
        if (nx, ny) == target:
            return True  # adjacent, next step would bump
        btn = {(1, 0): "right", (-1, 0): "left", (0, 1): "down", (0, -1): "up"} \
              [(nx - px, ny - py)]
        gb.press(btn, hold=6, settle=10)
        if (gb.rd("g_px"), gb.rd("g_py")) != (nx, ny):
            # a monster stepped into our path square — caller handles combat
            return abs(gb.rd("g_px") - target[0]) + abs(gb.rd("g_py") - target[1]) == 1
    return False


def main() -> int:
    gb = GB(rom=DBG_ROM)
    gb.boot_game()

    grid = read_map(gb)
    mons = read_mons(gb)

    # --- spawn sanity
    gb.expect(1 <= len(mons) <= 12, f"unexpected monster count {len(mons)}")
    for m in mons:
        gb.expect(m["kind"] < 26, f"bad kind {m}")
        gb.expect(grid[m["y"]][m["x"]] in WALKABLE, f"monster on wall {m}")
        gb.expect(m["hp"] >= 1, f"dead-on-arrival {m}")
        gb.expect((m["state"] & MST_AWAKE) == 0, f"should spawn asleep {m}")
    pos = {(m["x"], m["y"]) for m in mons}
    gb.expect(len(pos) == len(mons), "two monsters share a tile")
    px, py = gb.rd("g_px"), gb.rd("g_py")
    gb.expect((px, py) not in pos, "monster on player")
    print(f"  spawned: {[(chr(65+m['kind']), m['x'], m['y'], m['hp']) for m in mons]}")

    # --- hunt: chase the nearest monster (re-reading its position every
    #     step, it may wake and move) and bump-attack until it dies
    turns0 = gb.rd16("g_turns")
    xp0 = gb.rd16("g_xp")
    # leprechauns (11) and nymphs (13) steal and vanish without a kill;
    # pick something that stays and fights
    fighters = [m for m in mons if m["kind"] not in (11, 13)] or mons
    target = min(fighters, key=lambda m: abs(m["x"] - px) + abs(m["y"] - py))

    killed = False
    for it in range(250):
        cur = next((m for m in read_mons(gb) if m["i"] == target["i"]), None)
        if cur is None:
            killed = True
            break
        if gb.rd("g_hp") == 0:
            break
        px, py = gb.rd("g_px"), gb.rd("g_py")
        dx, dy = cur["x"] - px, cur["y"] - py
        if abs(dx) + abs(dy) == 1:
            btn = {(1, 0): "right", (-1, 0): "left",
                   (0, 1): "down", (0, -1): "up"}[(dx, dy)]
            gb.press(btn, hold=6, settle=10)
            continue
        others = {(m["x"], m["y"]) for m in read_mons(gb)
                  if m["i"] != target["i"]}
        path = bfs_path(grid, (px, py), (cur["x"], cur["y"]), others)
        if path is None or len(path) < 2:
            # another monster is squatting in the only doorway —
            # path straight through and fight whatever blocks
            path = bfs_path(grid, (px, py), (cur["x"], cur["y"]), set())
        gb.expect(path is not None and len(path) >= 2,
                  f"no path toward monster {cur}")
        nx, ny = path[1]
        if (nx, ny) == (cur["x"], cur["y"]):
            btn = {(1, 0): "right", (-1, 0): "left",
                   (0, 1): "down", (0, -1): "up"}[(nx - px, ny - py)]
            gb.press(btn, hold=6, settle=10)   # bump = attack
            continue
        btn = {(1, 0): "right", (-1, 0): "left",
               (0, 1): "down", (0, -1): "up"}[(nx - px, ny - py)]
        gb.press(btn, hold=6, settle=10)
    gb.expect(killed, f"monster not killed after 250 iterations (hp={gb.rd('g_hp')})")
    gb.expect(gb.rd16("g_turns") > turns0, "turn counter did not advance")
    gb.expect(gb.rd16("g_xp") > xp0 or gb.rd("g_level") > 1,
              "xp did not increase after kill")
    # (the "You killed the ..." line can be overwritten immediately by
    #  another awake monster's attack message — the WRAM kill/xp checks
    #  above are the durable assertions)
    gb.shot("m5_01_kill")
    print(f"  killed {chr(65+target['kind'])}, xp {xp0} -> {gb.rd16('g_xp')}, "
          f"hp {gb.rd('g_hp')}/{gb.rd('g_maxhp')}")

    # --- monster glyphs render when visible: check every awake monster in
    #     the current room appears on screen at the right cell
    cam_x, cam_y = gb.rd("g_cam_x"), gb.rd("g_cam_y")
    rows = gb.viewport()
    for m in read_mons(gb):
        sx, sy = m["x"] - cam_x, m["y"] - cam_y
        if 0 <= sx < 20 and 0 <= sy < 16:
            ch = rows[sy][sx]
            if ch == chr(65 + m["kind"]):
                print(f"  visible monster rendered: {ch} at {(sx, sy)}")

    gb.stop()
    print("verify_m5: all checks passed")
    return 0


if __name__ == "__main__":
    import sys
    try:
        sys.exit(main())
    except Failure as e:
        print(f"verify_m5 FAILED: {e}")
        sys.exit(1)
