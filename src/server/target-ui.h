/*
 * File: target-ui.h
 * Purpose: UI for targeting code
 */

#ifndef TARGET_UI_H
#define TARGET_UI_H

extern int draw_path(struct player *p, u16b path_n, struct loc *path_g, struct loc *grid);
extern void load_path(struct player *p, u16b path_n, struct loc *path_g);
extern bool target_set_interactive(struct player *p, int mode, u32b press, int step);

#endif /* TARGET_UI_H */
