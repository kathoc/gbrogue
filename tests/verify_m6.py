#!/usr/bin/env python3
"""
M6: floor items, auto-pickup, pack UI, equip framework, gold, descend.
"""
from collections import deque

from gbtest import GB, Failure

MAP_W, MAP_H = 32, 28
MF_TERRAIN = 0x1F
WALKABLE = {1, 2, 5, 6, 7, 8}
MAX_FLOOR = 24
FLOOR_STRIDE = 7   # kind,sub,x,y,qty,ench,flags
PACK_SLOTS = 16
IK = {0: "food", 1: "potion", 2: "scroll", 3: "wand", 4: "ring",
      5: "weapon", 6: "armor", 7: "gold", 8: "amulet"}
ITEM_NONE = 9
TI_STAIRS_DOWN = 6


def read_map(gb):
    raw = gb.rdbuf("g_map", MAP_W * MAP_H)
    return [[raw[y * MAP_W + x] & MF_TERRAIN for x in range(MAP_W)] for y in range(MAP_H)]


def read_items(gb, sym, count):
    raw = gb.rdbuf(sym, count * FLOOR_STRIDE)
    out = []
    for i in range(count):
        k, sub, x, y, qty, ench, flags = raw[i * FLOOR_STRIDE:(i + 1) * FLOOR_STRIDE]
        if k != ITEM_NONE:
            if ench > 127:
                ench -= 256
            out.append({"i": i, "kind": k, "sub": sub, "x": x, "y": y,
                        "qty": qty, "ench": ench, "flags": flags})
    return out


def read_mons(gb):
    raw = gb.rdbuf("g_mons", 12 * 7)
    return {(raw[i*7+1], raw[i*7+2]) for i in range(12) if raw[i*7] != 0xFF}


def bfs_next(grid, start, goal, blocked):
    q = deque([start])
    prev = {start: None}
    while q:
        cur = q.popleft()
        if cur == goal:
            path = []
            while cur is not None:
                path.append(cur)
                cur = prev[cur]
            path.reverse()
            return path
        x, y = cur
        for nx, ny in ((x+1, y), (x-1, y), (x, y+1), (x, y-1)):
            if 0 <= nx < MAP_W and 0 <= ny < MAP_H and (nx, ny) not in prev \
               and grid[ny][nx] in WALKABLE and (nx, ny) not in blocked:
                prev[(nx, ny)] = (x, y)
                q.append((nx, ny))
    return None


def walk_to(gb, grid, goal, max_steps=500):
    for _ in range(max_steps):
        p = (gb.rd("g_px"), gb.rd("g_py"))
        if p == goal:
            return True
        path = bfs_next(grid, p, goal, read_mons(gb) - {goal})
        if not path or len(path) < 2:
            # a monster blocks the only route — path through and bump it
            path = bfs_next(grid, p, goal, set())
            if not path or len(path) < 2:
                return False
        nx, ny = path[1]
        btn = {(1, 0): "right", (-1, 0): "left", (0, 1): "down", (0, -1): "up"} \
              [(nx - p[0], ny - p[1])]
        gb.press(btn, hold=6, settle=10)
        if gb.rd("g_hp") == 0:
            raise Failure("player died while walking (unlucky seed?)")
    return False


def main() -> int:
    gb = GB()
    gb.boot_game()

    # survival is not under test here; long walks meet mean monsters
    gb.pb.memory[gb.addr("g_maxhp")] = 200
    gb.pb.memory[gb.addr("g_hp")] = 200

    grid = read_map(gb)
    floor = read_items(gb, "g_floor", MAX_FLOOR)
    pack = read_items(gb, "g_pack", PACK_SLOTS)

    # --- starting kit
    kinds = [(i["kind"], i["sub"]) for i in pack]
    gb.expect((0, 0) in kinds, f"no food ration in kit: {pack}")
    gb.expect((5, 0) in kinds, "no mace in kit")
    gb.expect((6, 1) in kinds, "no ring mail in kit")
    gb.expect(gb.rd("g_wield") != 0xFF, "no weapon wielded")
    gb.expect(gb.rd("g_worn") != 0xFF, "no armor worn")
    gb.expect(gb.rd("g_ac") == 6, f"AC should be 6 (ring mail 7 - ench 1), got {gb.rd('g_ac')}")
    # (hp boosted above for walk survival; not part of the kit checks)

    # --- floor items generated
    gb.expect(len(floor) >= 2, f"too few floor items: {floor}")
    for it in floor:
        gb.expect(grid[it["y"]][it["x"]] in WALKABLE, f"item in wall: {it}")
    print(f"  floor items: {[(IK[i['kind']], i['x'], i['y']) for i in floor]}")

    # --- walk onto the nearest non-gold item -> auto pickup
    px, py = gb.rd("g_px"), gb.rd("g_py")
    nongold = [i for i in floor if i["kind"] != 7]
    gold = [i for i in floor if i["kind"] == 7]
    pack_before = len(pack)
    if nongold:
        t = min(nongold, key=lambda i: abs(i["x"] - px) + abs(i["y"] - py))
        gb.expect(walk_to(gb, grid, (t["x"], t["y"])), f"could not reach item {t}")
        now_floor = read_items(gb, "g_floor", MAX_FLOOR)
        gb.expect(all(i["i"] != t["i"] for i in now_floor), "item still on floor")
        # durable check: the item landed in the pack (the "Got ..." line
        # can be overwritten at once by a monster message)
        now_pack = read_items(gb, "g_pack", PACK_SLOTS)
        gb.expect(any(i["kind"] == t["kind"] and i["sub"] == t["sub"]
                      for i in now_pack), "picked item not in pack")
        gb.shot("m6_01_pickup")

    # --- gold pickup (re-read the floor: the first walk may have
    #     crossed a pile already)
    gold = [i for i in read_items(gb, "g_floor", MAX_FLOOR) if i["kind"] == 7]
    if gold:
        px, py = gb.rd("g_px"), gb.rd("g_py")
        g0 = gb.rd16("g_gold")
        t = min(gold, key=lambda i: abs(i["x"] - px) + abs(i["y"] - py))
        gb.expect(walk_to(gb, grid, (t["x"], t["y"])), f"could not reach gold {t}")
        gb.expect(gb.rd16("g_gold") == g0 + t["qty"] * 4,
                  f"gold {g0} -> {gb.rd16('g_gold')}, want +{t['qty']*4}")
        print(f"  gold picked: +{t['qty']*4}")

    # --- pack UI: open fully, close, world restored
    gb.press_until("select",
                   lambda rows: any("PACK" in r for r in rows) and "close" in rows[17])
    gb.shot("m6_02_pack")
    gb.press_until("b", lambda rows: any("@" in r for r in rows[:16]))

    # --- descend via stairs
    stairs = next((x, y) for y in range(MAP_H) for x in range(MAP_W)
                  if grid[y][x] == TI_STAIRS_DOWN)
    d0 = gb.rd("g_depth")
    # closed loop: late-latched inputs (or a teleport trap) can drift
    # the player off the stairs between "arrived" and the A tap — walk
    # back and retry until the descend actually registers
    for _ in range(6):
        gb.expect(walk_to(gb, grid, stairs), "could not reach stairs")
        gb.press("a", hold=8, settle=120)
        if gb.rd("g_depth") == d0 + 1:
            break
    gb.expect(gb.rd("g_depth") == d0 + 1, f"depth {d0} -> {gb.rd('g_depth')}")
    # (the "You descend" line can be overwritten at once by a monster
    #  attacking on arrival — assert the durable status row instead)
    gb.expect(gb.wait_screen(lambda rows: rows[17].startswith("B2")),
              f"status row never showed B2: {gb.status_row()!r}")
    gb.shot("m6_03_descended")
    print(f"  descended to depth {gb.rd('g_depth')}")

    gb.stop()
    print("verify_m6: all checks passed")
    return 0


if __name__ == "__main__":
    import sys
    try:
        sys.exit(main())
    except Failure as e:
        print(f"verify_m6 FAILED: {e}")
        sys.exit(1)
