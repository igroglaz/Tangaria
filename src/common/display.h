/*
 * File: display.h
 * Purpose: Display the character on the screen or in a file
 */

#ifndef INCLUDED_DISPLAY_H
#define INCLUDED_DISPLAY_H

extern int16_t cfg_fps;

/*** Display hooks ***/
extern errr (*clear_hook)(void);
extern void (*region_erase_hook)(const region *loc);
extern errr (*put_ch_hook)(int x, int y, uint16_t a, char c);
extern errr (*put_str_hook)(int x, int y, int n, uint16_t a, const char *s);
extern bool use_bigtile_hook;

/*** Buffer access functions ***/
extern errr buffer_clear(void);
extern errr buffer_put_ch(int x, int y, uint16_t a, char c);
extern errr buffer_put_str(int x, int y, int n, uint16_t a, const char *s);
extern char *buffer_line(int row);

/*** Utility display functions ***/
extern void display_player(struct player *pplayer, uint8_t mode);

/*** Status line display functions ***/
extern size_t display_depth(struct player *p, int row, int col);
extern void display_statusline(struct player *p, int row, int col);
extern void display_status_subwindow(struct player *p, int row, int col);

#endif /* INCLUDED_DISPLAY_H */
