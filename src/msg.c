#include "msg.h"
#include "render.h"
#include "lang.h"

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

/* --- deferred message queue --------------------------------------------
   Enqueue is pure RAM (no render/lang), so banked logic may call it. The
   single arg slot is enough because at most one arg-bearing message is
   queued per flush cycle (only identify posts one). msgq_flush lives with
   msg_post* in the fixed bank and does the actual rendering. */
#define MSGQ_MAX 4
static uint8_t mq_sid[MSGQ_MAX];
static uint8_t mq_hasarg[MSGQ_MAX];
static uint8_t mq_n;
static char    mq_arg[LOG_COLS + 1];

void msgq_id(uint8_t sid) {
    if (mq_n >= MSGQ_MAX) return;          /* overflow guard (never hit) */
    mq_sid[mq_n] = sid;
    mq_hasarg[mq_n] = 0;
    mq_n++;
}

void msgq_arg(uint8_t sid, const char *arg) {
    uint8_t i = 0;
    if (mq_n >= MSGQ_MAX) return;
    while (arg[i] && i < LOG_COLS) { mq_arg[i] = arg[i]; i++; }
    mq_arg[i] = 0;
    mq_sid[mq_n] = sid;
    mq_hasarg[mq_n] = 1;
    mq_n++;
}

void msgq_flush(void) {
    uint8_t i;
    for (i = 0; i < mq_n; i++) {
        if (mq_hasarg[i]) msg_postf(mq_sid[i], mq_arg);
        else              msg_post_id(mq_sid[i]);
    }
    mq_n = 0;
}

const char *msg_log_line(uint8_t idx) {
    uint8_t slot;
    if (idx >= LOG_LINES) return "";
    slot = (uint8_t)((head + LOG_LINES - 1u - idx) % LOG_LINES);
    return ring[slot];
}
