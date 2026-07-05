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

/* Log ring access for the M9 log viewer. idx 0 = newest. Returns "" for
   empty slots. */
const char *msg_log_line(uint8_t idx);

#endif
