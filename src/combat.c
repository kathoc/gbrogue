#include "combat.h"
#include "monsters.h"
#include "world.h"
#include "rng.h"
#include "msg.h"
#include "lang.h"
#include "util.h"
#include "inventory.h"
#include "effects.h"
#include "render.h"
#include "sfx.h"
#include "worldview.h"

/* sign() for the attack-lunge direction */
static int8_t sgn8(int8_t v) {
    return (int8_t)(v > 0 ? 1 : (v < 0 ? -1 : 0));
}

/*
 * Rogue 5.4 swing(): a d20 hits when roll >= (20 - attacker_level) -
 * defender_armor. Lower armor = harder to hit, exactly like the
 * original. Player melee is bare-handed 1d4 until M6 equips a mace.
 */

/* Flat to-hit bonus over Rogue's table for a brisker melee pace
   (applies to both sides — swing() is shared). */
#define HIT_BONUS 3

static uint8_t swing(uint8_t atk_lvl, int8_t def_arm) {
    int8_t need = (int8_t)(20 - HIT_BONUS - (int8_t)atk_lvl - def_arm);
    return (int8_t)rng_range(20) >= need;
}

/* e_levels: xp needed to reach level idx+2. */
static const uint16_t E_LEVELS[] = {
    10u, 20u, 40u, 80u, 160u, 320u, 640u, 1300u, 2600u, 5200u,
    13000u, 26000u, 50000u,
};
#define E_LEVEL_COUNT (sizeof(E_LEVELS) / sizeof(E_LEVELS[0]))

/* xp threshold to *hold* the given level (E_LEVELS[level-2]). */
static uint16_t xp_for_level(uint8_t lvl) {
    if (lvl < 2u) return 0;
    return E_LEVELS[lvl - 2u];
}

void combat_gain_xp(uint16_t xp) {
    uint16_t total = (uint16_t)(g_xp + xp);
    if (total < g_xp) total = 0xFFFFu;             /* saturate */
    g_xp = total;
    while (g_level - 1u < E_LEVEL_COUNT && g_xp >= E_LEVELS[g_level - 1u]) {
        char buf[32];
        char *p;
        sfx_play(SFX_LVLUP);
        g_level++;
        g_maxhp = (uint8_t)(g_maxhp + rng_dice(1, 10));
        g_hp = g_maxhp;                            /* Rogue tops you up */
        p = fmt_str(buf, lang_str(SID_WELCOME_LVL));
        p = fmt_u16(p, g_level);
        *p = 0;
        msg_post(buf);
    }
}

/*
 * Rogue's strength adjustments (hlist): STR shifts both the to-hit roll
 * and the damage. STR 16 (the start) gives +0 to-hit, +1 damage — so a
 * fresh game is unchanged, but drinking gain-strength (or losing it to a
 * rattlesnake) now actually matters. Applied to the player only; the
 * monster table already bakes its own numbers in.
 */
static void str_bonus(uint8_t str, int8_t *hit, int8_t *dam) {
    if (str <= 7u)       { *hit = -1; *dam = -1; }
    else if (str <= 15u) { *hit =  0; *dam =  0; }
    else if (str <= 17u) { *hit =  0; *dam =  1; }
    else if (str == 18u) { *hit =  1; *dam =  1; }
    else if (str == 19u) { *hit =  1; *dam =  2; }
    else if (str == 20u) { *hit =  1; *dam =  3; }
    else if (str == 21u) { *hit =  2; *dam =  3; }
    else                 { *hit =  2; *dam =  4; }
}

static uint8_t confuse_hit;

void combat_set_confuse_hit(void) {
    confuse_hit = 1;
}

void combat_report_kill(uint8_t kind) {
    msg_postf(SID_YOU_KILLED, lang_name(LT_MNAME, kind));
}

void combat_player_attack(monster_t *m) {
    const mkind_t *k = mkind(m->kind);
    int8_t hit_lvl = (int8_t)g_level;
    int8_t str_hit, str_dam;
    int8_t dex = effects_ring_ench_sum(7u);   /* ring of dexterity */

    str_bonus(g_str, &str_hit, &str_dam);
    hit_lvl = (int8_t)(hit_lvl + str_hit);
    if (dex > 0) hit_lvl = (int8_t)(hit_lvl + dex);
    if (hit_lvl < 0) hit_lvl = 0;

    m->state |= MST_AWAKE;
    view_lunge_add(SPR_PLAYER, sgn8((int8_t)(m->x - g_px)),
                   sgn8((int8_t)(m->y - g_py)));
    if (!swing((uint8_t)hit_lvl, k->arm)) {
        sfx_play(SFX_MISS);
        msg_postf(SID_YOU_MISS, lang_name(LT_MNAME, m->kind));
        return;
    }
    sfx_play(SFX_HIT);

    {
        uint8_t dmg;
        uint8_t kind = m->kind;
        int8_t d;
        if (g_wield != SLOT_NONE) {
            const item_t *w = &g_pack[g_wield];
            d = (int8_t)(rng_dice(WEAPON_DICE[w->sub][0],
                                  WEAPON_DICE[w->sub][1]) + w->ench);
        } else {
            d = (int8_t)rng_dice(1, 4);     /* bare hands */
        }
        d = (int8_t)(d + str_dam);
        dmg = d > 0 ? (uint8_t)d : 1u;
        {
            int8_t rdmg = effects_ring_ench_sum(8u);   /* damage ring */
            if (rdmg > 0) dmg = (uint8_t)(dmg + rdmg);
        }
        if (confuse_hit) {
            confuse_hit = 0;
            m->eff |= MEF_CONF;
            if (m->eff_t < 12u) m->eff_t = 12u;
        }
        render_flash_add(m->x, m->y, FLASH_HIT,
                         (uint8_t)(SPR_MON0 + (m - g_mons)));
        if (mon_damage(m, dmg)) {
            combat_report_kill(kind);
            combat_gain_xp(k->exp);   /* level-up line lands after it */
        } else {
            msg_postf(SID_YOU_HIT, lang_name(LT_MNAME, kind));
        }
    }
}

void combat_monster_attack(monster_t *m) {
    const mkind_t *k = mkind(m->kind);
    uint8_t a, hit_any = 0, dmg = 0;

    m->state |= MST_AWAKE;
    view_lunge_add((uint8_t)(SPR_MON0 + (m - g_mons)),
                   sgn8((int8_t)(g_px - m->x)),
                   sgn8((int8_t)(g_py - m->y)));
    for (a = 0; a < 6; a += 2) {
        uint8_t cnt = k->d[a];
        if (!cnt) break;
        if (swing((uint8_t)k->lvl, (int8_t)g_ac)) {
            hit_any = 1;
            dmg = (uint8_t)(dmg + rng_dice(cnt, k->d[a + 1u]));
        }
    }

    if (!hit_any) {
        msg_postf(SID_MON_MISSES, lang_name(LT_MNAME, m->kind));
        return;
    }
    msg_postf(SID_MON_HITS, lang_name(LT_MNAME, m->kind));

    sfx_play(SFX_HURT);
    render_flash_add(g_px, g_py, FLASH_HURT, SPR_PLAYER);
    if (dmg >= g_hp) {
        g_hp = 0;
        g_death_mon = m->kind;   /* remembered for the ranking cause line */
        msg_death(SID_DEATH_MON, lang_name(LT_MNAME, m->kind));
        return;
    }
    g_hp -= dmg;

    /* Signature on-hit abilities (Rogue 5.4). */
    switch (m->kind) {
    case 0:   /* Aquator: rusts worn armor */
        if (g_worn != SLOT_NONE && !effects_ring_worn(13u) &&
            g_pack[g_worn].sub != 0u) {        /* leather doesn't rust */
            g_pack[g_worn].ench--;
            g_ac = inv_player_ac();
            msg_post_id(SID_A_RUST);
        }
        break;
    case 8:   /* Ice monster: freezes you */
        g_sleep_t = (uint8_t)(g_sleep_t + 1u + rng_range(2));
        msg_post_id(SID_A_FROZEN);
        break;
    case 11:  /* Leprechaun: steals gold, vanishes */
        if (g_gold) {
            uint16_t take = (uint16_t)(rng_dice(5, 10) * (uint8_t)g_depth);
            if (take >= g_gold) g_gold = 0;
            else g_gold = (uint16_t)(g_gold - take);
            msg_post_id(SID_A_PURSE);
            m->kind = MON_NONE;
        }
        break;
    case 13:  /* Nymph: steals an item, vanishes */
        {
            uint8_t i;
            for (i = 0; i < PACK_SLOTS; i++) {
                if (g_pack[i].kind != ITEM_NONE &&
                    !(g_pack[i].flags & IF_WORN)) {
                    g_pack[i].kind = ITEM_NONE;
                    g_pack[i].flags = 0;
                    inv_compact();
                    msg_post_id(SID_A_STOLE);
                            m->kind = MON_NONE;
                    break;
                }
            }
        }
        break;
    case 17:  /* Rattlesnake: saps strength */
        if (!effects_ring_worn(2u) && g_str > 3u) {
            g_str--;
            msg_post_id(SID_A_WEAKER);
        }
        break;
    case 21:  /* Vampire: drains max HP */
        if (g_maxhp > 5u) {
            g_maxhp--;
            if (g_hp > g_maxhp) g_hp = g_maxhp;
            msg_post_id(SID_A_LIFE);
        }
        break;
    case 22:  /* Wraith: drains experience */
        if (rng_byte() < 100u && g_xp) {
            g_xp = (uint16_t)(g_xp / 2u);
            while (g_level > 1u && g_xp < xp_for_level(g_level)) {
                g_level--;
                if (g_maxhp > 8u) g_maxhp = (uint8_t)(g_maxhp - 4u);
                if (g_hp > g_maxhp) g_hp = g_maxhp;
            }
            msg_post_id(SID_A_DRAINED);
        }
        break;
    case 5:   /* Venus flytrap: holds you fast */
        g_held_t = (uint8_t)(3u + rng_range(3));
        msg_post_id(SID_A_HELD);
        break;
    default:
        break;
    }
}
