/*
 * File: netserver.h
 * Purpose: The server side of the network stuff
 */

#ifndef __Netserver_h
#define __Netserver_h

#define FREE_TIMEOUT    15
#define SETUP_TIMEOUT   180
#define PLAY_TIMEOUT    30

/*
 * The types of communication that we send to the metaserver
 */
#define META_START  0x01
#define META_DIE    0x02
#define META_UPDATE 0x04

/*
 * Special codes corresponding to action request.
 */
#define ACTION_PICKUP   1
#define ACTION_GO_DOWN  2

/* Mental links */
#define LINK_NONE       0
#define LINK_DOMINANT   1
#define LINK_DOMINATED  2

typedef struct
{
    int             state;
    sockbuf_t       r;
    sockbuf_t       w;
    sockbuf_t       c;
    sockbuf_t       q;
    hturn           start;
    long            timeout;
    bool            has_setup;
    uint16_t            conntype;
    uint8_t            char_state;
    int             id;
    unsigned        version;
    char            *real;
    char            *nick_account;
    char            *nick;
    char            *addr;
    char            *host;
    char            *pass;
    uint8_t            ridx;
    uint8_t            cidx;
    uint8_t            psex;
    int16_t            stat_roll[STAT_MAX + 1];
    client_setup_t  Client_setup;
    bool            options[OPT_MAX];
    bool            console_authenticated;
    bool            console_listen;
    uint8_t            console_channels[MAX_CHANNELS];
    uint32_t            account;
    char            *quit_msg;
} connection_t;

struct birth_options
{
    bool ironman;
    bool force_descend;
    bool no_recall;
    bool no_artifacts;
    bool feelings;
    bool no_selling;
    bool start_kit;
    bool no_stores;
    bool no_ghost;
    bool fruit_bat;
    bool hardcore;
};

/*** Player connection/index wrappers ***/
extern connection_t *get_connection(long idx);
extern long get_player_index(connection_t *connp);
extern void set_player_index(connection_t *connp, long idx);

/*** General utilities ***/
extern int Setup_net_server(void);
extern void Conn_set_state(connection_t *connp, int state, long timeout);
extern void setup_contact_socket(void);
extern bool Report_to_meta(int flag);
extern void Destroy_connection(int ind, char *reason);
extern void Stop_net_server(void);
extern void* console_buffer(int ind, bool read);
extern bool Conn_is_alive(int ind);
extern void Conn_set_console_setting(int ind, int set, bool val);
extern bool Conn_get_console_setting(int ind, int set);
extern int Init_setup(void);
extern uint8_t *Conn_get_console_channels(int ind);

/*** Sending ***/
extern int Send_basic_info(int ind);
extern int Send_limits_struct_info(int ind);
extern int Send_race_struct_info(int ind);
extern int Send_class_struct_info(int ind);
extern int Send_body_struct_info(int ind);
extern int Send_socials_struct_info(int ind);
extern int Send_kind_struct_info(int ind);
extern int Send_ego_struct_info(int ind);
extern int Send_rinfo_struct_info(int ind);
extern int Send_rbinfo_struct_info(int ind);
extern int Send_curse_struct_info(int ind);
extern int Send_realm_struct_info(int ind);
extern int Send_feat_struct_info(int ind);
extern int Send_trap_struct_info(int ind);
extern int Send_timed_struct_info(int ind);
extern int Send_abilities_struct_info(int ind);
extern int Send_death_cause(struct player *p);
extern int Send_winner(struct player *p);
extern int Send_lvl(struct player *p, int lev, int mlev);
extern int Send_weight(struct player *p, int weight, int max_weight);
extern int Send_plusses(struct player *p, int dd, int ds, int mhit, int mdam, int shit,
    int sdam);
extern int Send_ac(struct player *p, int base, int plus);
extern int Send_exp(struct player *p, int32_t max, int32_t cur, int16_t expfact);
extern int Send_gold(struct player *p, int32_t gold);
extern int Send_hp(struct player *p, int mhp, int chp);
extern int Send_sp(struct player *p, int msp, int csp);
extern int Send_various(struct player *p, int height, int weight, int age);
extern int Send_stat(struct player *p, int stat, int stat_top, int stat_use, int stat_max,
    int stat_add, int stat_cur);
extern int Send_history(struct player *p, int line, const char *hist);
extern int Send_autoinscription(struct player *p, struct object_kind *kind);
extern int Send_index(struct player *p, int i, int index, uint8_t type);
extern int Send_item_request(struct player *p, uint8_t tester_hook, char *dice_string);
extern int Send_title(struct player *p, const char *title);
extern int Send_turn(struct player *p, uint32_t game_turn, uint32_t player_turn, uint32_t active_turn);
extern int Send_extra(struct player *p);
extern int Send_depth(struct player *p);
extern int Send_status(struct player *p, int16_t *effects);
extern int Send_recall(struct player *p, int16_t word_recall, int16_t deep_descent);
extern int Send_state(struct player *p, bool stealthy, bool resting, bool unignoring, bool afraid,
    const char *terrain);
extern int Send_line_info(struct player *p, int y);
extern int Send_remote_line(struct player *p, int y);
extern int Send_speed(struct player *p, int speed, int mult);
extern int Send_study(struct player *p, int study, bool can_study_book);
extern int Send_count(struct player *p, uint8_t type, int16_t count);
extern int Send_show_floor(struct player *p, uint8_t mode);
extern int Send_char(struct player *p, struct loc *grid, uint16_t a, char c, uint16_t ta, char tc);
extern int Send_spell_info(struct player *p, int book, int i, const char *out_val,
    spell_flags *flags, int smana);
extern int Send_book_info(struct player *p, int book, const char *name);
extern int Send_floor(struct player *p, uint8_t num, const struct object *obj,
    struct object_xtra *info_xtra, uint8_t force);
extern int Send_special_other(struct player *p, char *header, uint8_t peruse, bool protect);
extern int Send_store(struct player *p, char pos, uint8_t attr, int16_t wgt, uint8_t number, int16_t owned,
    int32_t price, uint16_t tval, uint8_t max, int16_t bidx, const char *name);
extern int Send_store_info(struct player *p, int num, char *name, char *owner, char *welcome,
    int items, int32_t purse);
extern int Send_target_info(struct player *p, int x, int y, bool dble, const char *buf);
extern int Send_sound(struct player *p, int sound);
extern int Send_mini_map(struct player *p, int y, int16_t w);
extern int Send_skills(struct player *p);
extern int Send_pause(struct player *p);
extern int Send_monster_health(struct player *p, int num, uint8_t attr);
extern int Send_aware(struct player *p, uint16_t num);
extern int Send_everseen(struct player *p, uint16_t num);
extern int Send_ego_everseen(struct player *p, uint16_t num);
extern int Send_cursor(struct player *p, char vis, char x, char y);
extern int Send_objflags(struct player *p, int line);
extern int Send_spell_desc(struct player *p, int book, int i, char *out_desc, char *out_name);
extern int Send_dtrap(struct player *p, uint8_t dtrap);
extern int Send_term_info(struct player *p, int mode, uint16_t arg);
extern int Send_player_pos(struct player *p);
extern int Send_minipos(struct player *p, int y, int x, bool self, int n);
extern int Send_play(int ind);
extern int Send_features(int ind, int lighting, int off);
extern int Send_text_screen(int ind, int type, int32_t offset);
extern int Send_char_info_conn(int ind);
extern int Send_char_info(struct player *p, uint8_t ridx, uint8_t cidx, uint8_t psex);
extern int Send_birth_options(int ind, struct birth_options *options);
extern bool Send_dump_character(connection_t *connp, const char *dumpname, int mode);
extern int Send_message(struct player *p, const char *msg, uint16_t typ);
extern int Send_item(struct player *p, const struct object *obj, int wgt, int32_t price,
    struct object_xtra *info_xtra);
extern int Send_store_sell(struct player *p, int32_t price, bool reset);
extern int Send_party(struct player *p);
extern int Send_special_line(struct player *p, int max, int last, int line, uint8_t attr,
    const char *buf);
extern int Send_fullmap(struct player *p, int y);
extern int Send_poly(struct player *p, int race);
extern int Send_poly_race(struct player *p);
extern int Send_store_leave(struct player *p);
extern int Send_ignore(struct player *p);
extern int Send_flush(struct player *p, bool fresh, char delay);
extern int Send_channel(struct player *p, uint8_t n, const char *virt);
extern int Send_weather(struct player *p, uint8_t weather_type, uint8_t weather_wind, uint8_t weather_intensity);
extern int Send_slash_fx(struct player *p, int y, int x, int dir);

/*** Commands ***/
extern int cmd_ignore_drop(struct player *p);
extern int cmd_run(struct player *p, int dir);
extern int cmd_rest(struct player *p, int16_t resting);
extern int cmd_tunnel(struct player *p);
extern int cmd_zap(struct player *p, int item, int dir);
extern int cmd_use(struct player *p, int item, int dir);
extern int cmd_aim_wand(struct player *p, int item, int dir);
extern int cmd_activate(struct player *p, int item, int dir);
extern int cmd_fire_at_nearest(struct player *p);
extern int cmd_cast(struct player *p, int16_t book, int16_t spell, int dir);

/*** General network functions ***/
extern bool process_pending_commands(int ind);
extern int Net_input(void);
extern int Net_output(void);
extern int Net_output_p(struct player *p);
extern bool process_turn_based(void);

/* account.c */
extern uint32_t get_account(const char *name, const char *pass);

/* control.c */
extern void console_print(char *msg, int chan);
extern void NewConsole(int fd, int arg);

#endif
