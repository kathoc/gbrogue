#!/usr/bin/env python3
"""
Sound effects: every action posts its SFX id to g_sfx_last (footsteps
alternate, wall bump, gold/item pickup, hit/miss, getting hit, traps),
the wall bump leaves the camera where it was, and the APU is audibly
on (NR52 channel-active bits) while a sound plays.
"""
from gbtest import GB, Failure

MAP_W, MAP_H = 32, 28
MF_TERRAIN = 0x1F
WALK = {1, 2, 5, 6, 7, 8}
MON_STRIDE = 7
Z_KIND = 25          # zombie: mean, walks
IK_POTION, IK_GOLD = 1, 7
TR_BEAR = 1
TI_TRAP = 8
NR52 = 0xFF26

SFX_STEPS = (1, 2)
SFX_MISS, SFX_BUMP, SFX_ITEM, SFX_GOLD = 3, 4, 5, 6
SFX_HIT, SFX_HURT, SFX_TRAP = 7, 8, 9
SFX_MENU, SFX_STAIRS, SFX_LVLUP = 10, 11, 12

BTN = {(1, 0): "right", (-1, 0): "left", (0, 1): "down", (0, -1): "up"}


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


def main() -> int:
    gb = GB(sound=True)
    gb.boot_game()
    mem = gb.pb.memory
    mem[gb.addr("g_maxhp")] = 200
    mem[gb.addr("g_hp")] = 200
    clear_mons(gb)
    wt = gb.addr("g_wander_t")
    mem[wt] = 0
    mem[wt + 1] = 0
    last_a = gb.addr("g_sfx_last")

    def last():
        return mem[last_a]

    def reset_last():
        mem[last_a] = 0

    def pos():
        return gb.rd("g_px"), gb.rd("g_py")

    def stabilize():
        """Long sections accumulate turns: hold back the wandering-
        monster clock and shake off any status a stray hit left."""
        mem[wt] = 0
        mem[wt + 1] = 0
        mem[gb.addr("g_sleep_t")] = 0
        mem[gb.addr("g_held_t")] = 0
        mem[gb.addr("g_conf_t")] = 0

    def dirs(pred):
        grid = read_grid(gb)
        px, py = pos()
        return [(dx, dy) for (dx, dy) in BTN
                if pred(grid[py + dy][px + dx] & MF_TERRAIN)]

    def act(btn, done, tries=4):
        """Press until done() — a damage flash from the previous turn
        can swallow a single tap (same retry idiom as verify_m6/m7)."""
        for _ in range(tries):
            gb.press(btn, hold=8, settle=40)
            if done():
                return True
        return False

    gb.expect(mem[NR52] & 0x80, "APU is off (NR52 bit 7 clear)")

    # ---------------- 1. footsteps alternate "zu, za"
    def floor_item_at(x, y):
        raw = gb.rdbuf("g_floor", 24 * 8)   # item_t stride = 8
        return any(raw[i*8] != 9 and raw[i*8+2] == x and raw[i*8+3] == y
                   for i in range(24))

    def clean_step_dir():
        """A walkable neighbor without an item (a pickup sound would
        overwrite the footstep in g_sfx_last)."""
        cand = [d for d in dirs(lambda t: t in WALK)
                if not floor_item_at(pos()[0] + d[0], pos()[1] + d[1])]
        return (cand or dirs(lambda t: t in WALK))[0]

    d1 = clean_step_dir()
    reset_last()
    gb.expect(act(BTN[d1], lambda: last() != 0), "first step swallowed")
    first = last()
    gb.expect(first in SFX_STEPS, f"step sound missing (got {first})")
    d2 = clean_step_dir()
    reset_last()
    gb.expect(act(BTN[d2], lambda: last() != 0), "second step swallowed")
    second = last()
    gb.expect(second in SFX_STEPS and second != first,
              f"steps must alternate: {first} then {second}")
    print("  footsteps alternate")

    # ---------------- 2. wall bump: sound, no move, camera restored
    walls = dirs(lambda t: t not in WALK)
    if not walls:                       # room center: walk until a wall
        for _ in range(6):
            d = dirs(lambda t: t in WALK)[0]
            gb.press(BTN[d], hold=8, settle=30)
            walls = dirs(lambda t: t not in WALK)
            if walls:
                break
    gb.expect(bool(walls), "no wall neighbor found")
    p0 = pos()
    scx0, scy0 = mem[0xFF43], mem[0xFF42]
    reset_last()
    gb.expect(act(BTN[walls[0]], lambda: last() != 0), "bump swallowed")
    gb.expect(last() == SFX_BUMP, f"bump sound missing (got {last()})")
    gb.expect(pos() == p0, "bump moved the player")
    gb.expect((mem[0xFF43], mem[0xFF42]) == (scx0, scy0),
              "camera not restored after the bump jolt")
    print("  wall bump: sound + jolt restored")

    # ---------------- 3. gold pickup rings, and the APU really sounds
    d = dirs(lambda t: t == 1)          # onto plain floor, no trap there
    if not d:
        d = dirs(lambda t: t in WALK)
    tx, ty = pos()[0] + d[0][0], pos()[1] + d[0][1]
    floor0 = gb.addr("g_floor")
    for off, v in enumerate((IK_GOLD, 0, tx, ty, 5, 0, 0, 0)):  # +sench
        mem[floor0 + off] = v
    g0 = gb.rd16("g_gold")
    reset_last()
    heard = False
    for _ in range(4):                  # single taps: no held-repeat
        gb.hold(BTN[d[0]])
        for _ in range(8):
            gb.tick(1)
            heard |= bool(mem[NR52] & 0x0F)
        gb.release(BTN[d[0]])
        for _ in range(40):             # the chime rings on after release
            gb.tick(1)
            heard |= bool(mem[NR52] & 0x0F)
        if gb.rd16("g_gold") != g0:
            break
    gb.expect(last() == SFX_GOLD, f"gold chime missing (got {last()})")
    gb.expect(gb.rd16("g_gold") == g0 + 20, "gold not credited")
    gb.expect(heard, "NR52 never showed an active channel")
    print("  gold: chime requested and APU audibly active")

    # ---------------- 4. item pickup blips
    d = dirs(lambda t: t == 1) or dirs(lambda t: t in WALK)
    tx, ty = pos()[0] + d[0][0], pos()[1] + d[0][1]
    for off, v in enumerate((IK_POTION, 0, tx, ty, 1, 0, 0, 0)):  # +sench
        mem[floor0 + off] = v
    reset_last()
    gb.expect(act(BTN[d[0]], lambda: pos() == (tx, ty)),
              "never stepped onto the item")
    gb.expect(last() == SFX_ITEM, f"item blip missing (got {last()})")
    print("  item pickup blips")

    # ---------------- 5. your hit crunches (monster forced to whiff)
    d = dirs(lambda t: t in WALK)[0]
    p0 = pos()
    mx, my = p0[0] + d[0], p0[1] + d[1]
    put_mon(gb, 0, mx, my, awake=1)
    mem[gb.addr("g_level")] = 19        # player ~always hits
    mem[gb.addr("g_ac")] = 0x9C         # -100: monster always misses
    hit_seen = False
    for _ in range(10):
        stabilize()
        reset_last()
        mem[gb.addr("g_mons") + 3] = 200        # keep the zombie alive
        gb.press(BTN[d], hold=8, settle=40)
        if last() == SFX_HIT:
            hit_seen = True
            break
        # 0 = the tap was swallowed by a lingering flash; just retry
        gb.expect(last() in (0, SFX_MISS),
                  f"unexpected sfx {last()} attacking")
    gb.expect(hit_seen, "hit crunch never heard")
    gb.expect(pos() == p0, "attacking moved the player")
    print("  your hit crunches; monster kept whiffing silently")

    # ---------------- 6. your whiff swishes (low level, keep swinging)
    mem[gb.addr("g_level")] = 1
    miss_seen = False
    for _ in range(60):
        stabilize()
        reset_last()
        mem[gb.addr("g_mons") + 3] = 200
        gb.press(BTN[d], hold=8, settle=40)
        if last() == SFX_MISS:
            miss_seen = True
            break
        gb.expect(last() in (0, SFX_HIT),
                  f"unexpected sfx {last()} attacking")
    gb.expect(miss_seen, "whiff sound never heard in 60 swings")
    print("  your whiff swishes")

    # ---------------- 7. taking a hit thuds
    mem[gb.addr("g_ac")] = 30           # monster always hits now
    stabilize()
    hp0 = gb.rd("g_hp")
    reset_last()
    # search in place until the turn registers and the zombie connects
    gb.expect(act("a", lambda: gb.rd("g_hp") < hp0),
              "hp did not drop on the hit")
    gb.expect(last() == SFX_HURT, f"hurt thud missing (got {last()})")
    print("  taking a hit thuds")

    # ---------------- 8. traps warble
    clear_mons(gb)
    stabilize()
    mem[gb.addr("g_hp")] = 200
    d = dirs(lambda t: t == 1) or dirs(lambda t: t == 2)
    gb.expect(bool(d), "no plain cell to plant a trap on")
    tx, ty = pos()[0] + d[0][0], pos()[1] + d[0][1]
    traps0 = gb.addr("g_traps")
    mem[traps0], mem[traps0 + 1], mem[traps0 + 2] = tx, ty, TR_BEAR
    mem[gb.addr("g_trap_count")] = 1
    cell_a = gb.addr("g_map") + ty * MAP_W + tx
    mem[cell_a] = (mem[cell_a] & ~MF_TERRAIN) | TI_TRAP
    reset_last()
    gb.expect(act(BTN[d[0]], lambda: pos() == (tx, ty)),
              "never stepped onto the trap")
    gb.expect(last() == SFX_TRAP, f"trap warble missing (got {last()})")
    gb.expect(gb.rd("g_held_t") > 0, "bear trap did not hold")
    print("  trap warbles (bear trap holds)")

    # ---------------- 9. level up: rising arpeggio on the ding
    stabilize()
    mem[gb.addr("g_level")] = 1         # level-up path needs level < 14
    xp_a = gb.addr("g_xp")
    mem[xp_a], mem[xp_a + 1] = 9, 0     # one xp short of level 2
    d = dirs(lambda t: t in WALK)[0]
    put_mon(gb, 0, pos()[0] + d[0], pos()[1] + d[1], hp=1)  # dies to a hit
    reset_last()
    gb.expect(act(BTN[d], lambda: gb.rd("g_level") == 2, tries=12),
              "kill never leveled us up")
    gb.expect(last() == SFX_LVLUP, f"level-up jingle missing (got {last()})")
    # ordering: the level-up line posts AFTER the kill line, so it is
    # the latest message on the band
    msg = bytes(gb.rdbuf("g_last_msg", 40)).split(b"\0")[0]
    gb.expect(b"level" in msg,
              f"level-up line should follow the kill line (got {msg!r})")
    print("  level-up jingle plays; line lands after the kill")

    # ---------------- 10. stairs rumble on the way down
    clear_mons(gb)
    stabilize()
    grid = read_grid(gb)
    stairs = next(((x, y) for y in range(MAP_H) for x in range(MAP_W)
                   if (grid[y][x] & MF_TERRAIN) == 6), None)
    gb.expect(stairs is not None, "no stairs on level")
    mem[gb.addr("g_px")], mem[gb.addr("g_py")] = stairs
    gb.tick(4)
    d0 = gb.rd("g_depth")
    reset_last()
    gb.expect(act("a", lambda: gb.rd("g_depth") == d0 + 1),
              "descend never registered")
    gb.expect(last() == SFX_STAIRS, f"stairs sound missing (got {last()})")
    print("  stairs go da-da-da")

    # ---------------- 10b. BGM: melody advances, deeper = slower;
    #     the wave channel (CH3, one octave down at 50%) is active
    gb.expect(gb.wait_screen(lambda rows: gb.rd16("g_bgm_len") > 240, 240),
              f"depth-2 note length never exceeded 240")
    i0 = gb.rd("g_bgm_idx")
    ch3 = False
    for _ in range(300):
        gb.tick(1)
        if mem[NR52] & 0x04:
            ch3 = True
    gb.expect(gb.rd("g_bgm_idx") != i0, "BGM melody never advanced")
    gb.expect(ch3, "BGM wave channel never active")
    print("  BGM plays on the wave channel (slower on depth 2)")

    # ---------------- 11. menus blip on open and cursor moves
    gb.tick(120)                # ride out the stair fade + queued taps
    reset_last()
    pack_open = lambda: any("PACK" in r for r in gb.screen_rows())
    gb.expect(act("select", pack_open, tries=6), "pack never opened")
    gb.expect(last() == SFX_MENU, f"pack-open blip missing (got {last()})")
    reset_last()
    gb.expect(act("down", lambda: last() != 0), "cursor tap swallowed")
    gb.expect(last() == SFX_MENU, f"cursor blip missing (got {last()})")
    gb.press_until("b", lambda rows: rows[17].startswith("B"))
    print("  menu blips on open and cursor")

    gb.stop()
    print("verify_sfx: all checks passed")
    return 0


if __name__ == "__main__":
    import sys
    try:
        sys.exit(main())
    except Failure as e:
        print(f"verify_sfx FAILED: {e}")
        sys.exit(1)
