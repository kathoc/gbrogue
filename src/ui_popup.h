#ifndef UI_POPUP_H
#define UI_POPUP_H

/* Modal 2-4 line message box for important events. Pass NULL for
   unused lines. Waits for A or B, then the caller repaints. */
void ui_popup(const char *l1, const char *l2, const char *l3);

/* Trap-door plunge screen: fast-fades the world to black, shows the
   plunge message centered with an A-button icon below, waits for A, then
   fades out again (leaving the screen black for the caller to build and
   fade in the new floor). */
void ui_trapdoor(void);

#endif
