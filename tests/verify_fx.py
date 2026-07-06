#!/usr/bin/env python3
"""
Feature pass: START-hold diagonal lock (menu on clean release), damage
flashes (red = took damage / yellow = dealt damage, GBC attributes),
no sprite leak for unseen monsters during the move animation, and
distance-map chasing (doorway geometry included).
"""
from collections import deque

from gbtest import GB, Failure

MAP_W, MAP_H = 32, 28
MF_TERRAIN, MF_EXPLORED = 0x1F, 0x20
WALK = {1, 2, 5, 6, 7, 8}
TI_DOOR = 5
MON_STRIDE = 7
Z_KIND = 25  # zombie: mean, walks


def read_grid(gb):
    raw = gb.rdbuf("g_map", MAP_W * MAP_H)
    return [[raw[y * MAP_W + x] for x in range(MAP_W)] for y in range(MAP_H)]


def clear_mons(gb):
    base = gb.addr("g_mons")
    for i in range(12):
        gb.pb.memory[base + i * MON_STRIDE] = 0xFF


def put_mon(gb, slot, x, y, kind=Z_KIND, hp=200, awake=1):
    base = gb.addr("g_mons") + slot * MON_STRIDE
    mem = gb.pb.memory
    mem[base + 1] = x
    mem[base + 2] = y
    mem[base + 3] = hp
    mem[base + 4] = awake
    mem[base + 5] = 0
    mem[base + 6] = 0
    mem[base + 0] = kind
    gb.tick(2)


def bfs_dist(grid, start, goal):
    q = deque([(start, 0)])
    seen = {start}
    while q:
        (x, y), d = q.popleft()
        if (x, y) == goal:
            return d
        for dx in (-1, 0, 1):
            for dy in (-1, 0, 1):
                if not dx and not dy:
                    continue
                nx, ny = x + dx, y + dy
                if 0 <= nx < MAP_W and 0 <= ny < MAP_H and (nx, ny) not in seen \
                   and (grid[ny][nx] & MF_TERRAIN) in WALK:
                    seen.add((nx, ny))
                    q.append(((nx, ny), d + 1))
    return None


def rest(gb):
    gb.hold("b")
    gb.tick(4)
    gb.press("a", hold=8, settle=8)
    gb.release("b")
    gb.tick(30)


def main() -> int:
    gb = GB()
    gb.boot_game()
    grid = read_grid(gb)
    mem = gb.pb.memory
    gb.pb.memory[gb.addr("g_maxhp")] = 200
    gb.pb.memory[gb.addr("g_hp")] = 200

    # ---------------- 1. unseen-monster sprite leak during the glide
    px, py = gb.rd("g_px"), gb.rd("g_py")
    far = next(((x, y) for y in range(MAP_H) for x in range(MAP_W)
                if (grid[y][x] & MF_TERRAIN) in WALK
                and not (grid[y][x] & MF_EXPLORED)
                and abs(x - px) + abs(y - py) > 8), None)
    gb.expect(far is not None, "no unexplored walkable cell found")
    clear_mons(gb)
    put_mon(gb, 11, far[0], far[1], awake=0)
    oam = 0xFE00 + (1 + 11) * 4          # sprite slot SPR_MON0+11
    btn = None
    for d, (dx, dy) in {"right": (1, 0), "left": (-1, 0),
                        "down": (0, 1), "up": (0, -1)}.items():
        if (grid[py + dy][px + dx] & MF_TERRAIN) in WALK:
            btn = d
            break
    gb.pb.button_press(btn)
    leaked = False
    for _ in range(30):
        gb.tick(1)
        if mem[oam] != 0:
            leaked = True
    gb.pb.button_release(btn)
    gb.tick(20)
    gb.expect(not leaked, "unseen monster sprite appeared during the move")
    print("  no sprite leak for unseen monster")

    # ---------------- 2. chase via distance map (across rooms/doors)
    grid = read_grid(gb)
    px, py = gb.rd("g_px"), gb.rd("g_py")
    spot = None
    for y in range(MAP_H):
        for x in range(MAP_W):
            if (grid[y][x] & MF_TERRAIN) not in WALK:
                continue
            d = bfs_dist(grid, (x, y), (px, py))
            if d is not None and 5 <= d <= 8:
                spot = (x, y, d)
    gb.expect(spot is not None, "no cell at BFS distance 5..8")
    clear_mons(gb)
    put_mon(gb, 0, spot[0], spot[1], awake=1)
    d0 = spot[2]
    dmin = d0
    for t in range(16):
        rest(gb)
        mx, my = mem[gb.addr("g_mons") + 1], mem[gb.addr("g_mons") + 2]
        if mem[gb.addr("g_mons")] == 0xFF:
            break                        # it died? (can't with hp 200)
        d = bfs_dist(read_grid(gb), (mx, my),
                     (gb.rd("g_px"), gb.rd("g_py")))
        if d is not None:
            dmin = min(dmin, d)
        if d is not None and d <= 1:
            break
    gb.expect(dmin <= 1, f"monster never closed in (start {d0}, best {dmin})")
    print(f"  chase: distance {d0} -> {dmin}")

    # ---------------- 3. damage flash attributes (GBC)
    gb.expect(gb.rd("g_is_gbc") == 1, "expected CGB boot for attr flashes")
    # plant a tanky zombie on a cardinally-adjacent tile and trade blows,
    # watching the attribute map for the flash palettes
    grid = read_grid(gb)
    px, py = gb.rd("g_px"), gb.rd("g_py")
    btn = mx = my = None
    for d, (dx, dy) in {"right": (1, 0), "left": (-1, 0),
                        "down": (0, 1), "up": (0, -1)}.items():
        if (grid[py + dy][px + dx] & MF_TERRAIN) in WALK:
            btn, mx, my = d, px + dx, py + dy
            break
    gb.expect(btn is not None, "no adjacent walkable tile")
    clear_mons(gb)
    put_mon(gb, 0, mx, my, awake=1)
    seen_yellow = seen_red = seen_hp_red = False
    hp_col = gb.rd("g_hp_col0")
    for round_ in range(10):
        if seen_yellow and seen_red and seen_hp_red:
            break
        gb.pb.button_press(btn)
        for f in range(80):
            gb.tick(1)
            if f == 10:
                gb.pb.button_release(btn)
            if mem[1, 0x9800 + 32 * my + mx] == 5:
                seen_yellow = True
            if mem[1, 0x9800 + 32 * py + px] == 4:
                seen_red = True
            if mem[1, 0x9C00 + 32 + hp_col] == 4:
                seen_hp_red = True
        gb.tick(6)
        if gb.rd("g_hp") < 50:
            gb.pb.memory[gb.addr("g_hp")] = 200
    gb.expect(seen_yellow, "no yellow flash on the monster cell")
    gb.expect(seen_red, "no red flash on the player cell")
    gb.expect(seen_hp_red, "HP readout never blinked red")
    print("  damage flashes: yellow / red / HP readout observed")

    # ---------------- 3b. message band slides between lines
    # (battle chatter alternates lines, so slides are guaranteed —
    #  the zombie from 3 is still adjacent)
    tile0 = 0x8000 + 63 * 16          # first fixed message-row tile
    def tsnap():
        return bytes(mem[tile0 + i] for i in range(16))
    states = [tsnap()]
    for _ in range(3):
        gb.pb.button_press(btn)
        for f in range(70):
            gb.tick(1)
            if f == 10:
                gb.pb.button_release(btn)
            s = tsnap()
            if states[-1] != s:
                states.append(s)
        gb.tick(6)
    gb.expect(len(states) >= 4,
              f"message slide showed only {len(states)} tile states")
    print(f"  message band slides ({len(states)} intermediate states)")

    # ---------------- 4. START-hold diagonal lock
    clear_mons(gb)
    gb.tick(4)
    # relocate to the widest room's interior so diagonals exist
    rraw = gb.rdbuf("g_rooms", 9 * 5)
    rooms = [tuple(rraw[i*5:(i+1)*5]) for i in range(9)
             if not (rraw[i*5+4] & 1) and rraw[i*5+2] >= 6 and rraw[i*5+3] >= 5]
    gb.expect(rooms, "no roomy room for the diagonal test")
    rx, ry, rw, rh, _ = max(rooms, key=lambda r: r[2] * r[3])
    gb.pb.memory[gb.addr("g_px")] = rx + rw // 2
    gb.pb.memory[gb.addr("g_py")] = ry + rh // 2
    gb.press("right", hold=6, settle=30)   # resync view state
    grid = read_grid(gb)
    px, py = gb.rd("g_px"), gb.rd("g_py")
    diag = None
    for dx, dy, b1, b2 in ((1, 1, "right", "down"), (-1, 1, "left", "down"),
                           (1, -1, "right", "up"), (-1, -1, "left", "up")):
        tx, ty = px + dx, py + dy
        if not (0 <= tx < MAP_W and 0 <= ty < MAP_H):
            continue
        if (grid[ty][tx] & MF_TERRAIN) in WALK \
           and (grid[ty][tx] & MF_TERRAIN) != TI_DOOR \
           and (grid[py][px] & MF_TERRAIN) != TI_DOOR \
           and ((grid[py][tx] & MF_TERRAIN) in WALK
                or (grid[ty][px] & MF_TERRAIN) in WALK):
            diag = (dx, dy, b1, b2)
            break
    gb.expect(diag is not None, "no diagonal-walkable neighbor")
    dx, dy, b1, b2 = diag

    # cardinal press while START held: must NOT move, and releasing
    # START afterwards must NOT open the menu
    gb.hold("start")
    gb.tick(6)
    gb.press(b1, hold=10, settle=16)
    gb.release("start")
    gb.tick(30)
    gb.expect((gb.rd("g_px"), gb.rd("g_py")) == (px, py),
              "cardinal moved during START hold")
    gb.expect(not any("MENU" in r for r in gb.screen_rows()),
              "menu opened although the D-pad was used")

    # diagonal pair while START held: moves diagonally, still no menu
    gb.hold("start")
    gb.tick(6)
    gb.hold(b1, b2)
    gb.tick(14)
    gb.release(b1, b2)
    gb.tick(10)
    gb.release("start")
    gb.tick(40)
    gb.expect((gb.rd("g_px"), gb.rd("g_py")) == (px + dx, py + dy),
              f"START+diagonal failed: at {(gb.rd('g_px'), gb.rd('g_py'))}, "
              f"want {(px + dx, py + dy)}")
    gb.expect(not any("MENU" in r for r in gb.screen_rows()),
              "menu opened after a diagonal-locked move")
    print("  START-hold: diagonal locked, cardinal ignored, no menu")

    # plain START tap still opens the menu
    gb.press_until("start", lambda rows: any("MENU" in r for r in rows))
    gb.press_until("b", lambda rows: not any("MENU" in r for r in rows))
    print("  START tap still opens the menu")

    # ---------------- 5. A+B hold: delayed auto-repeat resting
    gb.tick(30)          # clear the modal-close swallow window first
    t0 = gb.rd16("g_turns")
    gb.hold("a", "b")
    gb.tick(240)
    gb.release("a", "b")
    gb.tick(30)
    dt = gb.rd16("g_turns") - t0
    gb.expect(dt >= 4, f"A+B hold only advanced {dt} turns")
    # a short hold (< 0.5s delay) must yield exactly one rest
    t0 = gb.rd16("g_turns")
    gb.hold("a", "b")
    gb.tick(16)
    gb.release("a", "b")
    gb.tick(40)
    dt = gb.rd16("g_turns") - t0
    gb.expect(dt == 1, f"short A+B tap advanced {dt} turns (want 1)")
    print("  A+B hold repeats after the 0.5s delay")

    # ---------------- 6. repeat-speed setting cycles in the menu
    menu_on = lambda rows: any("MENU" in r for r in rows)
    gb.press_until("start", menu_on)
    want = 2 + 5                          # Repeat speed row (index 5, after Log)
    for _ in range(20):
        cur = gb.cursor_row()
        if cur == want:
            break
        gb.press("down" if cur < want else "up", hold=6, settle=10)
    s0 = gb.rd("g_repeat_speed")
    gb.press("a", hold=8, settle=20)
    s1 = gb.rd("g_repeat_speed")
    gb.expect(s1 == (s0 + 1) % 3, f"speed setting {s0} -> {s1}")
    gb.press("a", hold=8, settle=20)
    gb.press("a", hold=8, settle=20)      # cycle back to the start value
    gb.expect(gb.rd("g_repeat_speed") == s0, "speed did not cycle around")
    gb.press_until("b", lambda rows: not any("MENU" in r for r in rows))
    print("  repeat speed setting cycles 1/2/3")

    # ---------------- 7. autosave: every step commits to SRAM
    gb.pb.memory[0x0000] = 0x0A           # enable cart RAM for peeking
    gb.tick(2)
    mem = gb.pb.memory
    def sram_head():
        return bytes(mem[0xA000 + i] for i in range(40))
    rest(gb)
    s_a = sram_head()
    gb.expect(s_a[0] == 0x47, "no autosave written after a turn")
    rest(gb)
    s_b = sram_head()
    gb.expect(s_a != s_b, "autosave did not update on the next turn")
    print("  autosave commits every turn")

    # ---------------- 8. wandering monsters appear over time
    clear_mons(gb)
    wt = gb.addr("g_wander_t")
    alive = lambda: sum(1 for i in range(12)
                        if mem[gb.addr("g_mons") + i * MON_STRIDE] != 0xFF)
    spawned = False
    # the spawn roll is ~6/256 per turn once the clock is ripe; 400
    # turns puts P(no spawn) below 1e-4 for any RNG stream
    for _ in range(400):
        gb.pb.memory[wt] = 200            # fast-forward the clock
        gb.pb.memory[wt + 1] = 0
        rest(gb)
        if alive() > 0:
            spawned = True
            break
    gb.expect(spawned, "no wandering monster spawned")
    print("  wandering monster spawned")

    # ---------------- 9. monsters obey the door/diagonal rules
    clear_mons(gb)
    mem[wt] = 0
    mem[wt + 1] = 0
    mem[gb.addr("g_hp")] = 200
    grid = read_grid(gb)
    geo = None
    for y in range(1, MAP_H - 1):
        for x in range(1, MAP_W - 1):
            if (grid[y][x] & MF_TERRAIN) != TI_DOOR:
                continue
            for ox, oy in ((0, 1), (0, -1), (1, 0), (-1, 0)):
                if (grid[y + oy][x + ox] & MF_TERRAIN) != 2:   # corridor out
                    continue
                if (grid[y - oy][x - ox] & MF_TERRAIN) != 1:   # floor in
                    continue
                for s in (1, -1):
                    zx, zy = (x + s, y - oy) if oy else (x - ox, y + s)
                    if (grid[zy][zx] & MF_TERRAIN) == 1:
                        geo = (x, y, ox, oy, zx, zy)
                        break
                if geo:
                    break
            if geo:
                break
        if geo:
            break
    gb.expect(geo is not None, "no door with a diagonal interior cell")
    dx_, dy_, ox, oy, zx, zy = geo
    inx, iny = dx_ - ox, dy_ - oy                      # floor inside the door

    # 9a. player steps onto the door; the diagonal zombie must neither
    # strike across the corner nor stay put — it has to walk around
    mem[gb.addr("g_px")] = dx_ + ox
    mem[gb.addr("g_py")] = dy_ + oy
    gb.tick(4)
    put_mon(gb, 0, zx, zy, awake=1)
    hp0 = gb.rd("g_hp")
    btn = {(0, 1): "up", (0, -1): "down",
           (1, 0): "left", (-1, 0): "right"}[(ox, oy)]
    gb.press(btn, hold=8, settle=70)
    gb.expect((gb.rd("g_px"), gb.rd("g_py")) == (dx_, dy_),
              "player did not step onto the door")
    gb.expect(gb.rd("g_hp") == hp0,
              "monster attacked diagonally across the door")
    mpos = (mem[gb.addr("g_mons") + 1], mem[gb.addr("g_mons") + 2])
    gb.expect(mpos == (inx, iny),
              f"zombie at {mpos}, expected the orthogonal detour {(inx, iny)}")
    print("  no diagonal strike across a door; monster detours")

    # 9b. chasing from the diagonal cell, it must round the corner
    # through the inside cell instead of slipping onto the door
    mem[gb.addr("g_px")] = dx_ + ox
    mem[gb.addr("g_py")] = dy_ + oy
    gb.tick(4)
    put_mon(gb, 0, zx, zy, awake=1)
    hp0 = gb.rd("g_hp")
    rest(gb)
    mpos = (mem[gb.addr("g_mons") + 1], mem[gb.addr("g_mons") + 2])
    gb.expect(mpos != (dx_, dy_), "zombie entered the door diagonally")
    gb.expect(mpos == (inx, iny),
              f"zombie at {mpos}, expected {(inx, iny)}")
    rest(gb)
    mpos = (mem[gb.addr("g_mons") + 1], mem[gb.addr("g_mons") + 2])
    gb.expect(mpos == (dx_, dy_), "zombie did not take the door frontally")
    gb.expect(gb.rd("g_hp") == hp0, "hp dropped before the zombie arrived")
    hit = False
    for _ in range(6):                    # control: it still fights fine
        rest(gb)
        if gb.rd("g_hp") < hp0:
            hit = True
            break
    gb.expect(hit, "zombie never attacked frontally (stuck?)")
    print("  door entered frontally only; frontal attacks still land")

    gb.stop()
    print("verify_fx: all checks passed")
    return 0


if __name__ == "__main__":
    import sys
    try:
        sys.exit(main())
    except Failure as e:
        print(f"verify_fx FAILED: {e}")
        sys.exit(1)
