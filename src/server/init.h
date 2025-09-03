/*
 * File: init.h
 * Purpose: Initialization
 */

#ifndef INCLUDED_INIT_H
#define INCLUDED_INIT_H

struct init_module
{
    const char *name;
    void (*init)(void);
    void (*cleanup)(void);
};

extern bool cfg_report_to_meta;
extern bool cfg_mang_meta;
extern char *cfg_meta_address;
extern int32_t cfg_meta_port;
extern char *cfg_bind_name;
extern char *cfg_report_address;
extern char *cfg_console_password;
extern char *cfg_dungeon_master;
extern bool cfg_secret_dungeon_master;
extern uint32_t cfg_max_account_chars;
extern bool cfg_no_steal;
extern bool cfg_newbies_cannot_drop;
extern int32_t cfg_level_unstatic_chance;
extern int32_t cfg_retire_timer;
extern bool cfg_random_artifacts;
extern bool cfg_more_towns;
extern bool cfg_artifact_drop_shallow;
extern bool cfg_limit_player_connections;
extern int32_t cfg_tcp_port;
extern int16_t cfg_quit_timeout;
extern uint32_t cfg_disconnect_fainting;
extern bool cfg_lazy_connections;
extern bool cfg_chardump_color;
extern int16_t cfg_pvp_hostility;
extern bool cfg_base_monsters;
extern bool cfg_extra_monsters;
extern bool cfg_ghost_diving;
extern bool cfg_console_local_only;
extern char *cfg_load_pref_file;
extern char *cfg_chardump_label;
extern int16_t cfg_preserve_artifacts;
extern bool cfg_safe_recharge;
extern int16_t cfg_party_sharelevel;
extern bool cfg_instance_closed;
extern bool cfg_turn_based;
extern bool cfg_limited_esp;
extern bool cfg_double_purse;
extern bool cfg_level_req;
extern int16_t cfg_constant_time_factor;
extern bool cfg_classic_exp_factor;
extern int16_t cfg_house_floor_size;
extern int16_t cfg_limit_stairs;
extern int16_t cfg_diving_mode;
extern bool cfg_no_artifacts;
extern int16_t cfg_level_feelings;
extern int16_t cfg_limited_stores;
extern bool cfg_gold_drop_vanilla;
extern bool cfg_no_ghost;
extern bool cfg_ai_learn;
extern bool cfg_challenging_levels;

extern const char *list_obj_flag_names[];
extern const char *obj_mods[];
extern const char *list_element_names[];

extern errr grab_effect_data(struct parser *p, struct effect *effect);
extern void init_file_paths(const char *configpath, const char *libpath, const char *datapath);
extern void create_needed_dirs(void);
extern void init_angband(void);
extern void cleanup_angband(void);
extern void load_server_cfg(void);

#endif /* INCLUDED_INIT_H */
