#ifndef UI_MENU_H
#define UI_MENU_H

#include <stdint.h>

/* Main menu results. */
enum {
    MENU_CANCEL = 0,
    MENU_SUSPEND,     /* caller saves + exits to title */
};

/* START menu: Log / Display / Speed / Language / Save & quit. No entry
   consumes a turn; returns MENU_SUSPEND when the caller must save-and-quit,
   else MENU_CANCEL. */
uint8_t ui_menu_show(void);

#endif
