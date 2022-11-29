/*
 * File: ui-output.h
 * Purpose: Putting text on the screen, screen saving and loading, panel handling
 */

#ifndef INCLUDED_UI_OUTPUT_H
#define INCLUDED_UI_OUTPUT_H

extern bool full_icky_screen;
extern bool target_icky_screen;

/*
 * Regions
 */

/* Region that defines the full screen */
extern const region SCREEN_REGION;

/* Erase the contents of a region */
extern void region_erase(const region *loc);

/* Erase the contents of a region + 1 char each way */
extern void region_erase_bordered(const region *loc);

/* Given a region with relative values, make them absolute */
extern region region_calculate(region loc);

/*
 * Screen loading/saving
 */
extern void screen_save(void);
extern void screen_load(bool flush);

/*
 * text_out hook for screen display
 */
extern void text_out_to_screen(uint8_t a, const char *str);

/*
 * Simple text display
 */
extern void c_put_str(uint8_t attr, const char *str, int row, int col);
extern void put_str(const char *str, int row, int col);
extern void c_prt(uint8_t attr, const char *str, int row, int col);
extern void prt(const char *str, int row, int col);
extern int prt_multi(int sx, int sy, int n, int m, int attr, const char *msg, bool ex);

/*
 * Miscellaneous things
 */

extern void window_make(int origin_x, int origin_y, int end_x, int end_y);
extern void text_out_e(const char *buf, int y, int xoffset);

/* set_focus.c */
extern void set_chat_focus(void);
extern void unset_chat_focus(void);
extern void stretch_chat_ctrl(void);

#endif /* INCLUDED_UI_OUTPUT_H */



