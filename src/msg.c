#include "msg.h"
#include "render.h"
#include "lang.h"
#include "combat.h"      /* msgq_flush replays kill/xp events here */
#include "monsters.h"    /* mkind(kind)->exp for KILL replay */

static char    ring[LOG_LINES][LOG_COLS + 1];
static uint8_t head;                    /* next slot to write */

/* The game-over screen shows this instead of the raw last log line. */
char g_death_cause[LOG_COLS + 1];
void msg_death(uint8_t sid, const char *arg) {
    const char *p = lang_str(sid);
    uint8_t o = 0;
    while (*p && o < LOG_COLS) {
        if ((uint8_t)*p == 0x01u && arg) {
            const char *a = arg;
            while (*a && o < LOG_COLS) g_death_cause[o++] = *a++;
            p++;
        } else {
            g_death_cause[o++] = *p++;
        }
    }
    g_death_cause[o] = 0;
}

void msg_clear(void) {
    uint8_t i;
    for (i = 0; i < LOG_LINES; i++) ring[i][0] = 0;
    head = 0;
    render_message(0);
}

void msg_post(const char *s) {
    uint8_t i = 0;
    while (s[i] && i < LOG_COLS) {
        ring[head][i] = s[i];
        i++;
    }
    ring[head][i] = 0;
    head = (uint8_t)((head + 1u) % LOG_LINES);
    render_message(s);
}

void msg_post_id(uint8_t sid) {
    msg_post(lang_str(sid));
}

void msg_postf(uint8_t sid, const char *arg) {
    char out[LOG_COLS + 1];
    const char *p = lang_str(sid);
    uint8_t o = 0;
    while (*p && o < LOG_COLS) {
        if ((uint8_t)*p == 0x01u) {
            const char *a = arg;
            while (*a && o < LOG_COLS) out[o++] = *a++;
            p++;
        } else {
            out[o++] = *p++;
        }
    }
    out[o] = 0;
    msg_post(out);
}

void msg_refresh(void) {
    const char *last = msg_log_line(0);
    if (last[0]) render_message(last);
}

/* --- deferred turn-effect journal --------------------------------------
   Banked item logic (items_use/zap/throw, once in BANK2) must not render or
   grant xp mid-processing: msg_post touches render, and combat_gain_xp both
   renders and would run under call_bank's interrupt-masked __critical. So
   the logic only records effects here — pure RAM writes, safe from any bank
   — and the BANK0 orchestrator replays them with msgq_flush() after the
   banked work returns (in HOME, interrupts live, rendering normal).

   Messages AND kill/xp events share ONE ordered list so their relative
   order is preserved (a wand bolt's line precedes its kill; a drain wand's
   kills precede its summary line). KILL replays combat_report_kill +
   combat_gain_xp; XP replays a raw combat_gain_xp (raise-level potion).

   The single text slot (mq_txt) is enough: at most one text-bearing entry
   (identify's ARG, or a bolt's STR) is queued per flush cycle, and those
   paths carry no other text. Sized for the worst case: drain-life can kill
   all MAX_MONSTERS in one zap (that many KILL entries) plus its summary. */
#define MQ_ID   0u   /* msg_post_id(val)          */
#define MQ_ARG  1u   /* msg_postf(val, mq_txt)    */
#define MQ_STR  2u   /* msg_post(mq_txt)          */
#define MQ_KILL 3u   /* combat_report_kill(val) + combat_gain_xp(mkind exp) */
#define MQ_XP   4u   /* combat_gain_xp(mq_xp)     */
#define MSGQ_MAX 16
static uint8_t  mq_type[MSGQ_MAX];
static uint8_t  mq_val[MSGQ_MAX];
static uint8_t  mq_n;
static uint16_t mq_xp;                  /* one raw-xp grant per cycle */
static char     mq_txt[LOG_COLS + 1];   /* one text entry per cycle   */

static void mq_settxt(const char *s) {
    uint8_t i = 0;
    while (s[i] && i < LOG_COLS) { mq_txt[i] = s[i]; i++; }
    mq_txt[i] = 0;
}

void msgq_id(uint8_t sid) {
    if (mq_n >= MSGQ_MAX) return;          /* overflow guard (never hit) */
    mq_type[mq_n] = MQ_ID;
    mq_val[mq_n] = sid;
    mq_n++;
}

void msgq_arg(uint8_t sid, const char *arg) {
    if (mq_n >= MSGQ_MAX) return;
    mq_settxt(arg);
    mq_type[mq_n] = MQ_ARG;
    mq_val[mq_n] = sid;
    mq_n++;
}

void msgq_str(const char *s) {
    if (mq_n >= MSGQ_MAX) return;
    mq_settxt(s);
    mq_type[mq_n] = MQ_STR;
    mq_n++;
}

void msgq_kill(uint8_t kind) {
    if (mq_n >= MSGQ_MAX) return;
    mq_type[mq_n] = MQ_KILL;
    mq_val[mq_n] = kind;
    mq_n++;
}

void msgq_xp(uint16_t xp) {
    if (mq_n >= MSGQ_MAX) return;
    mq_type[mq_n] = MQ_XP;
    mq_xp = xp;
    mq_n++;
}

void msgq_flush(void) {
    uint8_t i;
    for (i = 0; i < mq_n; i++) {
        switch (mq_type[i]) {
        case MQ_ID:   msg_post_id(mq_val[i]);            break;
        case MQ_ARG:  msg_postf(mq_val[i], mq_txt);      break;
        case MQ_STR:  msg_post(mq_txt);                  break;
        case MQ_KILL: combat_report_kill(mq_val[i]);
                      combat_gain_xp(mkind(mq_val[i])->exp); break;
        default:      combat_gain_xp(mq_xp);             break;  /* MQ_XP */
        }
    }
    mq_n = 0;
}

const char *msg_log_line(uint8_t idx) {
    uint8_t slot;
    if (idx >= LOG_LINES) return "";
    slot = (uint8_t)((head + LOG_LINES - 1u - idx) % LOG_LINES);
    return ring[slot];
}
