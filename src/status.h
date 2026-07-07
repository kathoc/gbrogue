#ifndef STATUS_H
#define STATUS_H

#include <stdint.h>

/* When nonzero: SID shown bottom-right instead of the stairs hint
   (e.g. SID_HINT_CANCEL while aiming a wand). */
extern uint8_t g_status_hint;

/* Rebuild the bottom status row from world state. */
void status_update(void);

/* Drop the status-row compare cache so the next status_update() repaints
   every tile unconditionally. Call when the composed-tile pool is reset or
   the status row is otherwise torn down (e.g. returning from a modal). */
void status_invalidate(void);

#endif
