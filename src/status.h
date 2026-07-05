#ifndef STATUS_H
#define STATUS_H

#include <stdint.h>

/* When nonzero: SID shown bottom-right instead of the stairs hint
   (e.g. SID_HINT_CANCEL while aiming a wand). */
extern uint8_t g_status_hint;

/* Rebuild the bottom status row from world state. */
void status_update(void);

#endif
