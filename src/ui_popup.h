#ifndef UI_POPUP_H
#define UI_POPUP_H

/* Modal 2-4 line message box for important events. Pass NULL for
   unused lines. Waits for A or B, then the caller repaints. */
void ui_popup(const char *l1, const char *l2, const char *l3);

#endif
