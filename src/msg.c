#include "msg.h"
#include "render.h"
#include "lang.h"

/* The deferred msgq journal lives in bank0_msgq.c (fixed bank) so banked
   item logic can enqueue into it; see there. */

static char    ring[LOG_LINES][LOG_COLS + 1];
static uint8_t head;                    /* next slot to write */

/* The game-over screen shows this instead of the raw last log line. */
char g_death_cause[LOG_COLS + 1];
/* Compact form of the same cause, persisted into the ranking: the fatal
   SID (SID_DEATH_*) plus the monster kind when it was a monster kill. */
uint8_t g_death_sid;
uint8_t g_death_mon;
void msg_death(uint8_t sid, const char *arg) {
    const char *p = lang_str(sid);
    uint8_t o = 0;
    g_death_sid = sid;
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

const char *msg_log_line(uint8_t idx) {
    uint8_t slot;
    if (idx >= LOG_LINES) return "";
    slot = (uint8_t)((head + LOG_LINES - 1u - idx) % LOG_LINES);
    return ring[slot];
}
