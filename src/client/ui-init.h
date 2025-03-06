/*
 * File: ui-init.h
 * Purpose: Various game initialisation routines
 */

#ifndef INCLUDED_UI_INIT_H
#define INCLUDED_UI_INIT_H

#define MAX_VERIFY  8

/*
 * Information about terrain features.
 *
 * Copied from cave.h and simplified for the client.
 */
struct feature
{
    char *name; /* Name */
    int fidx;   /* Index */
};

extern struct feature *f_info;

extern char meta_address[NORMAL_WID];
extern int meta_port;
extern char account[NORMAL_WID];
extern char nick[NORMAL_WID];
extern char pass[NORMAL_WID];
extern char stored_pass[NORMAL_WID];
extern char real_name[NORMAL_WID];
extern char server_name[NORMAL_WID];
extern int server_port;
extern bool play_again;
extern uint16_t max_account_chars;
extern uint16_t char_num;
extern char **char_name;
extern char *char_expiry;

extern void init_file_paths(const char *configpath, const char *libpath, const char *datapath);
extern void create_needed_dirs(void);
extern void client_init(bool new_game, int argc, char **argv);
extern void client_ready(bool newchar);
extern bool gather_settings(void);
extern void cleanup_floor(void);
extern void cleanup_angband(void);
extern void init_stuff(void);
extern void textui_cleanup(void);

#endif /* INCLUDED_UI_INIT_H */
