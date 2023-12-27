/*
 * File: ui-display.h
 * Purpose: Handles the setting up updating, and cleaning up of the game display.
 */

#ifndef INCLUDED_UI_DISPLAY_H
#define INCLUDED_UI_DISPLAY_H

extern int16_t max_line;
extern int16_t cur_line;

extern int health_amt;
extern uint8_t health_attr;
extern int lag_mark;
extern int16_t view_channel;
extern cave_view_type remote_info[ANGBAND_TERM_MAX][MAX_TXT_INFO][NORMAL_WID];
extern int16_t last_remote_line[ANGBAND_TERM_MAX];

extern const char *window_flag_desc[PW_MAX_FLAGS];

extern bool sfx_effect;
extern uint8_t sfx_dir;
extern uint16_t sfx_info_a[256][256];
extern char sfx_info_c[256][256];
extern uint8_t sfx_info_d[256][256];
extern uint8_t sfx_move[256][256];

extern void display_player_screen(uint8_t mode);
extern void toggle_inven_equip(void);
extern void redraw_stuff(void);
extern int find_whisper_tab(const char *msg, char *text, size_t len);
extern void message_color_hack(const char *msg, uint8_t *ap);
extern void subwindows_set_flags(uint32_t *new_flags, size_t n_subwindows);
extern void subwindows_init_flags(void);
extern void subwindows_reinit_flags(void);
extern void init_display(void);
extern void show_splashscreen(void);
extern bool peruse_file(void);
extern void do_weather(void);
extern void do_animations(void);
extern void do_slashfx(void);
extern void slashfx_refresh_char(int x, int y);

#endif /* INCLUDED_UI_DISPLAY_H */
