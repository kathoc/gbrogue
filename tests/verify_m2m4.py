#!/usr/bin/env python3
"""
M2-M4: logical map + scrolling viewport + explored/unexplored + random
generation.

Strategy: read g_map / rooms / player straight out of WRAM via the .sym
file, then verify the *screen* against that ground truth while driving
the player along a BFS path computed in Python.
"""
from collections import deque

from gbtest import GB, Failure, ROOT

# Map generation depends on the new-game seed, which the release build
# draws from DIV-based entropy — so it shifts with any boot-timing change
# and could hand this walk a deadly/disconnected level. Run against the
# debug ROM, which pins the seed (src/ui_title.c GBR_DEBUG_KIT).
DBG_ROM = ROOT / "build" / "dbg" / "gbrogue.gb"

MAP_W, MAP_H = 32, 28
MF_TERRAIN = 0x1F
MF_EXPLORED = 0x20

# tile ids (src/tiles.h)
TI_BLANK, TI_FLOOR, TI_CORRIDOR, TI_WALL_H, TI_WALL_V, TI_DOOR = range(6)
TI_STAIRS_DOWN, TI_STAIRS_UP, TI_TRAP = 6, 7, 8
WALKABLE = {TI_FLOOR, TI_CORRIDOR, TI_DOOR, TI_STAIRS_DOWN, TI_STAIRS_UP, TI_TRAP}
GLYPH = {TI_BLANK: " ", TI_FLOOR: ".", TI_CORRIDOR: "#", TI_WALL_H: "-",
         TI_WALL_V: "|", TI_DOOR: "+", TI_STAIRS_DOWN: ">", TI_STAIRS_UP: "<",
         TI_TRAP: "^"}


def read_map(gb: GB) -> list[list[int]]:
    raw = gb.rdbuf("g_map", MAP_W * MAP_H)
    return [list(raw[y * MAP_W:(y + 1) * MAP_W]) for y in range(MAP_H)]


def bfs_path(grid, start, goal):
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
               and (grid[ny][nx] & MF_TERRAIN) in WALKABLE:
                prev[(nx, ny)] = (x, y)
                q.append((nx, ny))
    return None


def main() -> int:
    gb = GB(rom=DBG_ROM)
    gb.boot_game()

    grid = read_map(gb)
    px, py = gb.rd("g_px"), gb.rd("g_py")
    cam_x, cam_y = gb.rd("g_cam_x"), gb.rd("g_cam_y")

    # --- M4: generation sanity
    terr = [[c & MF_TERRAIN for c in row] for row in grid]
    floors = sum(t == TI_FLOOR for row in terr for t in row)
    doors = sum(t == TI_DOOR for row in terr for t in row)
    stairs = [(x, y) for y in range(MAP_H) for x in range(MAP_W)
              if terr[y][x] == TI_STAIRS_DOWN]
    gb.expect(floors >= 60, f"too few floor tiles: {floors}")
    gb.expect(doors >= 4, f"too few doors: {doors}")
    gb.expect(len(stairs) == 1, f"expected exactly 1 stairs-down, got {stairs}")
    gb.expect(terr[py][px] in WALKABLE, "player spawned on non-walkable tile")

    # every walkable tile reachable from the player (full connectivity)
    reach = {(px, py)}
    q = deque([(px, py)])
    while q:
        x, y = q.popleft()
        for nx, ny in ((x+1, y), (x-1, y), (x, y+1), (x, y-1)):
            if 0 <= nx < MAP_W and 0 <= ny < MAP_H and (nx, ny) not in reach \
               and terr[ny][nx] in WALKABLE:
                reach.add((nx, ny))
                q.append((nx, ny))
    all_walk = {(x, y) for y in range(MAP_H) for x in range(MAP_W)
                if terr[y][x] in WALKABLE}
    orphans = all_walk - reach
    gb.expect(not orphans, f"{len(orphans)} walkable tiles unreachable, e.g. {sorted(orphans)[:5]}")
    gb.expect(stairs[0] in reach, "stairs not reachable from spawn")

    # --- M3: initial exploration = spawn room lit, far cells dark
    explored = sum(1 for row in grid for c in row if c & MF_EXPLORED)
    gb.expect(9 <= explored <= 250, f"initial explored count odd: {explored}")

    # screen matches ground truth at the player cell
    sx, sy = px - cam_x, py - cam_y
    gb.expect(gb.player_pos() == (sx, sy),
              f"@ at {gb.player_pos()}, expected {(sx, sy)}")

    # unexplored world cells must render blank
    rows = gb.viewport()
    for vy in range(16):
        for vx in range(20):
            cell = grid[cam_y + vy][cam_x + vx]
            ch = rows[vy][vx]
            if not (cell & MF_EXPLORED):
                gb.expect(ch == " ",
                          f"unexplored ({cam_x+vx},{cam_y+vy}) shows {ch!r}")
            elif (vx, vy) != (sx, sy):
                want = GLYPH[cell & MF_TERRAIN]
                overlay = ch.isupper() or ch in "!?/=)]%*,"
                if overlay and (cell & MF_TERRAIN) in WALKABLE:
                    continue  # a monster (M5) or floor item (M6) sits here
                gb.expect(ch == want,
                          f"explored ({cam_x+vx},{cam_y+vy}) shows {ch!r}, want {want!r}")
    gb.shot("m2m4_01_spawn")

    # --- drive the player to the stairs, replanning every step (monsters
    #     wander into the path and must be fought through)
    gb.expect(bfs_path(grid, (px, py), stairs[0]) is not None,
              "no path to stairs")
    moved_ok = 0
    for _ in range(600):
        p = (gb.rd("g_px"), gb.rd("g_py"))
        if p == stairs[0]:
            break
        gb.expect(gb.rd("g_hp") > 0, "died on the way to the stairs")
        path = bfs_path(grid, p, stairs[0])
        gb.expect(path is not None and len(path) >= 2, f"no path from {p}")
        nx, ny = path[1]
        btn = {(1, 0): "right", (-1, 0): "left", (0, 1): "down", (0, -1): "up"} \
              [(nx - p[0], ny - p[1])]
        gb.press(btn, hold=6, settle=10)
        if (gb.rd("g_px"), gb.rd("g_py")) == (nx, ny):
            moved_ok += 1        # presses do move exactly one tile
    gb.expect((gb.rd("g_px"), gb.rd("g_py")) == stairs[0],
              "never reached the stairs")
    gb.expect(moved_ok >= 10, f"too few clean single-tile steps ({moved_ok})")
    gb.tick(40)   # let the last glide / damage flash settle before reading OAM

    # on arrival: standing on the stairs, screen shows @ there, camera sane
    cam_x, cam_y = gb.rd("g_cam_x"), gb.rd("g_cam_y")
    gb.expect(gb.player_pos() == (stairs[0][0] - cam_x, stairs[0][1] - cam_y),
              "player not rendered on stairs cell")
    gb.expect(cam_x <= MAP_W - 20 and cam_y <= MAP_H - 16, "camera out of range")
    gb.shot("m2m4_02_at_stairs")

    # --- M3: walking explored new ground
    grid2 = read_map(gb)
    explored2 = sum(1 for row in grid2 for c in row if c & MF_EXPLORED)
    gb.expect(explored2 > explored,
              f"exploration did not grow ({explored} -> {explored2})")

    # --- M4: same seed => identical map on reboot
    gb.stop()
    gb2 = GB(rom=DBG_ROM)
    gb2.boot_game()
    gb.expect(read_map(gb2) is not None, "reboot failed")
    same = all((grid[y][x] & MF_TERRAIN) == (read_map(gb2)[y][x] & MF_TERRAIN)
               for y in (0, 7, 14, 21, 27) for x in range(MAP_W))
    gb2.expect(same, "map not deterministic for fixed seed")
    gb2.stop()

    print("verify_m2m4: all checks passed")
    return 0


if __name__ == "__main__":
    import sys
    try:
        sys.exit(main())
    except Failure as e:
        print(f"verify_m2m4 FAILED: {e}")
        sys.exit(1)
