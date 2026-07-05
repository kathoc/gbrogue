#ifndef UI_MENU_H
#define UI_MENU_H

#include <stdint.h>

/* Main menu results. */
enum {
    MENU_CANCEL = 0,
    MENU_REST,        /* handled inside (turn consumed) */
    MENU_SUSPEND,     /* caller saves + exits to title */
};

/* START menu: Rest / Search / Throw / Log / Display / Save & quit.
   Most entries resolve internally; returns MENU_SUSPEND when the
   caller must save-and-quit, MENU_REST when a turn was consumed,
   else MENU_CANCEL. */
uint8_t ui_menu_show(void);

#endif
