#ifndef MSG_H
#define MSG_H

#include <stdint.h>

#define LOG_LINES 16
#define LOG_COLS  40          /* half-width text: 40 chars per row */

/* Natural-language cause of death ("Slain by the bat"); set at the
   moment HP hits zero, shown on the game-over screen. */
extern char g_death_cause[];
void msg_death(uint8_t sid, const char *arg);

/* Show s on the message band and archive it in the log ring. */
void msg_post(const char *s);
/* Post the message SID (fetched in the active language). */
void msg_post_id(uint8_t sid);
/* Post the pattern SID with its %s slot (byte 0x01) replaced by arg. */
void msg_postf(uint8_t sid, const char *arg);
void msg_clear(void);
/* Redraw the newest logged line (after a full-screen UI repaint). */
void msg_refresh(void);

/* --- deferred message queue (items->BANK2 orchestration) -----------------
   Banked item/effect logic must not render mid-processing (it would call
   render/lang in another bank). Instead it records messages here with the
   enqueue calls below — pure RAM writes, safe from any bank — and the
   BANK0 orchestrator drains them with msgq_flush() once the banked work
   returns. Order is preserved (FIFO), so deferring is behaviour-neutral:
   nothing reads the log ring mid-processing. */
void msgq_id(uint8_t sid);                    /* enqueue msg_post_id(sid) */
void msgq_arg(uint8_t sid, const char *arg);  /* enqueue msg_postf(sid,arg) */
void msgq_flush(void);                        /* post all queued, then clear */

/* Log ring access for the M9 log viewer. idx 0 = newest. Returns "" for
   empty slots. */
const char *msg_log_line(uint8_t idx);

#endif
