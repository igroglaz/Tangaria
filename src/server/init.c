/*
 * File: init.c
 * Purpose: Various game initialisation routines
 *
 * Copyright (c) 1997 Ben Harrison
 * Copyright (c) 2025 MAngband and PWMAngband Developers
 *
 * This work is free software; you can redistribute it and/or modify it
 * under the terms of either:
 *
 * a) the GNU General Public License as published by the Free Software
 *    Foundation, version 2, or
 *
 * b) the "Angband licence":
 *    This software may be copied and distributed for educational, research,
 *    and not for profit purposes provided that this copyright and statement
 *    are included in all such copies.  Other copyrights may also apply.
 */


#include "s-angband.h"


/*
 * This file is used to initialize various variables and arrays for the
 * Angband game.
 *
 * Several of the arrays for Angband are built from data files in the
 * "lib/gamedata" directory.
 */


/*
 * Server options, set in mangband.cfg
 */
bool cfg_report_to_meta = false;
bool cfg_mang_meta = true;
char *cfg_meta_address = NULL;
int32_t cfg_meta_port = 8800;
char *cfg_bind_name = NULL;
char *cfg_report_address = NULL;
char *cfg_console_password = NULL;
char *cfg_dungeon_master = NULL;
bool cfg_secret_dungeon_master = true;
uint32_t cfg_max_account_chars = 12;
bool cfg_no_steal = true;
bool cfg_newbies_cannot_drop = true;
int32_t cfg_level_unstatic_chance = 60;
bool cfg_random_artifacts = false;
int32_t cfg_retire_timer = -1;
bool cfg_more_towns = false;
bool cfg_artifact_drop_shallow = true;
bool cfg_limit_player_connections = true;
int32_t cfg_tcp_port = 18346;
int16_t cfg_quit_timeout = 5;
uint32_t cfg_disconnect_fainting = 180;
bool cfg_chardump_color = false;
int16_t cfg_pvp_hostility = PVP_SAFE;
bool cfg_base_monsters = true;
bool cfg_extra_monsters = false;
bool cfg_ghost_diving = false;
bool cfg_console_local_only = false;
char *cfg_load_pref_file = NULL;
char *cfg_chardump_label = NULL;
int16_t cfg_preserve_artifacts = 3;
bool cfg_safe_recharge = false;
int16_t cfg_party_sharelevel = -1;
bool cfg_instance_closed = false;
bool cfg_turn_based = false;
bool cfg_limited_esp = false;
bool cfg_double_purse = false;
bool cfg_level_req = true;
int16_t cfg_constant_time_factor = 5;
bool cfg_classic_exp_factor = true;
int16_t cfg_house_floor_size = 1;
int16_t cfg_limit_stairs = 0;
int16_t cfg_diving_mode = 0;
bool cfg_no_artifacts = false;
int16_t cfg_level_feelings = 3;
int16_t cfg_limited_stores = 1;
bool cfg_gold_drop_vanilla = true;
bool cfg_no_ghost = false;
bool cfg_ai_learn = true;
bool cfg_challenging_levels = false;


static const char *slots[] =
{
    #define EQUIP(a, b, c, d, e, f) #a,
    #include "../common/list-equip-slots.h"
    #undef EQUIP
    NULL
};


const char *list_obj_flag_names[] =
{
    #define OF(a, b) #a,
    #include "../common/list-object-flags.h"
    #undef OF
    NULL
};


const char *obj_mods[] =
{
    #define STAT(a, b, c) #a,
    #include "../common/list-stats.h"
    #undef STAT
    #define OBJ_MOD(a, b, c) #a,
    #include "../common/list-object-modifiers.h"
    #undef OBJ_MOD
    NULL
};


const char *list_element_names[] =
{
    #define ELEM(a, b, c, d) #a,
    #include "../common/list-elements.h"
    #undef ELEM
    NULL
};


static const char *effect_list[] = {
    NULL,
    #define EFFECT(x, a, b, c, d, e) #x,
    #include "list-effects.h"
    #undef EFFECT
    NULL
};


static const char *trap_flags[] =
{
    #define TRF(a, b) #a,
    #include "../common/list-trap-flags.h"
    #undef TRF
    NULL
};


static const char *terrain_flags[] =
{
    #define TF(a, b) #a,
    #include "list-terrain-flags.h"
    #undef TF
    NULL
};


static const char *player_info_flags[] =
{
    #define PF(a) #a,
    #include "../common/list-player-flags.h"
    #undef PF
    NULL
};


static const char *attack_effects[] = {
    #define MA(a) #a,
    #include "list-attack-effects.h"
    #undef MA
    NULL
};


static errr grab_barehanded_attack(struct parser *p, struct barehanded_attack *attack)
{
    const char *extra;
    int val;

    attack->verb = string_make(parser_getsym(p, "verb"));
    extra = parser_getsym(p, "extra");
    if (streq(extra, "none")) extra = "";
    attack->hit_extra = string_make(extra);
    attack->min_level = parser_getint(p, "level");
    attack->chance = parser_getint(p, "chance");
    if (grab_name("effect", parser_getstr(p, "effect"), attack_effects, N_ELEMENTS(attack_effects),
        &val))
    {
        return PARSE_ERROR_INVALID_EFFECT;
    }
    attack->effect = val;
    return PARSE_ERROR_NONE;
}


errr grab_effect_data(struct parser *p, struct effect *effect)
{
    const char *type;
    int val;

    if (grab_name("effect", parser_getsym(p, "eff"), effect_list, N_ELEMENTS(effect_list), &val))
        return PARSE_ERROR_INVALID_EFFECT;
    effect->index = val;

    if (parser_hasval(p, "type"))
    {
        type = parser_getsym(p, "type");

        if (type == NULL) return PARSE_ERROR_UNRECOGNISED_PARAMETER;

        /* Check for a value */
        val = effect_subtype(effect->index, type);
        if (val < 0) return PARSE_ERROR_INVALID_VALUE;
        effect->subtype = val;
    }

    if (parser_hasval(p, "radius"))
        effect->radius = parser_getint(p, "radius");

    if (parser_hasval(p, "other"))
        effect->other = parser_getint(p, "other");

    return PARSE_ERROR_NONE;
}


static enum parser_error write_book_kind(struct class_book *book, const char *name)
{
    struct object_kind *temp, *kind;
    int i;

    /* Check we haven't already made this book */
    for (i = 0; i < z_info->k_max; i++)
    {
        if (k_info[i].name && streq(name, k_info[i].name))
        {
            book->sval = k_info[i].sval;
            return PARSE_ERROR_NONE;
        }
    }

    /* Extend by 1 and realloc */
    z_info->k_max++;
    temp = mem_realloc(k_info, z_info->k_max * sizeof(*temp));

    /* Copy if no errors */
    if (!temp) return PARSE_ERROR_INTERNAL;
    k_info = temp;

    /* Add this entry at the end */
    kind = &k_info[z_info->k_max - 1];
    memset(kind, 0, sizeof(*kind));

    /* Copy the tval and base */
    kind->tval = book->tval;
    kind->base = &kb_info[kind->tval];

    /* Make the name and index */
    kind->name = string_make(name);
    kind->kidx = z_info->k_max - 1;

    /* Increase the sval count for this tval, set the new one to the max */
    for (i = 0; i < TV_MAX; i++)
    {
        if (kb_info[i].tval == kind->tval)
        {
            kb_info[i].num_svals++;
            kind->sval = kb_info[i].num_svals;
            break;
        }
    }
    if (i == TV_MAX) return PARSE_ERROR_INTERNAL;

    /* Copy the sval */
    book->sval = kind->sval;

    /* Set object defaults (graphics should be overwritten) */
    kind->d_char = '*';
    kind->d_attr = COLOUR_RED;
    kind->dd = 1;
    kind->ds = 1;

    /* Dungeon books get extra properties */
    if (book->dungeon)
    {
        for (i = ELEM_BASE_MIN; i < ELEM_BASE_MAX; i++)
            kind->el_info[i].flags |= EL_INFO_IGNORE;
        kf_on(kind->kind_flags, KF_GOOD);
    }

    /* Inherit base flags. */
    kf_union(kind->kind_flags, kb_info[kind->tval].kind_flags);

    return PARSE_ERROR_NONE;
}


/* Free the sub-paths */
static void free_file_paths(void)
{
    string_free(ANGBAND_DIR_GAMEDATA);
    ANGBAND_DIR_GAMEDATA = NULL;
    string_free(ANGBAND_DIR_CUSTOMIZE);
    ANGBAND_DIR_CUSTOMIZE = NULL;
    string_free(ANGBAND_DIR_HELP);
    ANGBAND_DIR_HELP = NULL;
    string_free(ANGBAND_DIR_SCREENS);
    ANGBAND_DIR_SCREENS = NULL;
    string_free(ANGBAND_DIR_TILES);
    ANGBAND_DIR_TILES = NULL;
    string_free(ANGBAND_DIR_USER);
    ANGBAND_DIR_USER = NULL;
    string_free(ANGBAND_DIR_SAVE);
    ANGBAND_DIR_SAVE = NULL;
    string_free(ANGBAND_DIR_PANIC);
    ANGBAND_DIR_PANIC = NULL;
    string_free(ANGBAND_DIR_SCORES);
    ANGBAND_DIR_SCORES = NULL;
}


/*
 * Find the default paths to all of our important sub-directories.
 *
 * All of the sub-directories should, for a single-user install, be
 * located inside the main directory, whose location is very system-dependent.
 * For shared installations, typically on Unix or Linux systems, the
 * directories may be scattered - see config.h for more info.
 *
 * This function takes buffers, holding the paths to the "config", "lib",
 * and "data" directories (for example, those could be "/etc/angband/",
 * "/usr/share/angband", and "/var/games/angband"). Some system-dependent
 * expansion/substitution may be done when copying those base paths to the
 * paths Angband uses: see path_process() in z-file.c for details (Unix
 * implementations, for instance, try to replace a leading ~ or ~username with
 * the path to a home directory).
 *
 * Various command line options may allow some of the important
 * directories to be changed to user-specified directories, most
 * importantly, the "scores" and "user" and "save" directories,
 * but this is done after this function, see "main.c".
 *
 * In general, the initial path should end in the appropriate "PATH_SEP"
 * string.  All of the "sub-directory" paths (created below or supplied
 * by the user) will NOT end in the "PATH_SEP" string, see the special
 * "path_build()" function in "util.c" for more information.
 *
 * First we free all the strings, since this is known
 * to succeed even if the strings have not been allocated yet,
 * as long as the variables start out as "NULL".  This allows
 * this function to be called multiple times, for example, to
 * try several base "path" values until a good one is found.
 */
void init_file_paths(const char *configpath, const char *libpath, const char *datapath)
{
    char buf[MSG_LEN];
    char *userpath = NULL;

    /*** Free everything ***/

    /* Free the sub-paths */
    free_file_paths();

    /*** Prepare the paths ***/

#define BUILD_DIRECTORY_PATH(dest, basepath, dirname) \
    path_build(buf, sizeof(buf), (basepath), (dirname)); \
    dest = string_make(buf);

    /* Paths generally containing configuration data for Angband. */
#ifdef GAMEDATA_IN_LIB
    BUILD_DIRECTORY_PATH(ANGBAND_DIR_GAMEDATA, libpath, "gamedata");
#else
    BUILD_DIRECTORY_PATH(ANGBAND_DIR_GAMEDATA, configpath, "gamedata");
#endif
    BUILD_DIRECTORY_PATH(ANGBAND_DIR_CUSTOMIZE, configpath, "customize");
    BUILD_DIRECTORY_PATH(ANGBAND_DIR_HELP, libpath, "help");
    BUILD_DIRECTORY_PATH(ANGBAND_DIR_SCREENS, libpath, "screens");
    BUILD_DIRECTORY_PATH(ANGBAND_DIR_TILES, libpath, "tiles");

#ifdef PRIVATE_USER_PATH
	/* Build the path to the user specific directory */
	if (strncmp(ANGBAND_SYS, "test", 4) == 0)
		path_build(buf, sizeof(buf), PRIVATE_USER_PATH, "Test");
	else
		path_build(buf, sizeof(buf), PRIVATE_USER_PATH, VERSION_NAME);
	ANGBAND_DIR_USER = string_make(buf);
#else /* !PRIVATE_USER_PATH */
#ifdef MACH_O_CARBON
	/* Remove any trailing separators, since some deeper path creation functions
	 * don't like directories with trailing slashes. */
	if (suffix(datapath, PATH_SEP)) {
		/* Hacky way to trim the separator. Since this is just for OS X, we can
		 * assume a one char separator. */
		int last_char_index = strlen(datapath) - 1;
		my_strcpy(buf, datapath, sizeof(buf));
		buf[last_char_index] = '\0';
		ANGBAND_DIR_USER = string_make(buf);
	}
	else {
		ANGBAND_DIR_USER = string_make(datapath);
	}
#else /* !MACH_O_CARBON */
	BUILD_DIRECTORY_PATH(ANGBAND_DIR_USER, datapath, "user");
#endif /* MACH_O_CARBON */
#endif /* PRIVATE_USER_PATH */

#ifdef USE_PRIVATE_PATHS
    userpath = ANGBAND_DIR_USER;
#else /* !USE_PRIVATE_PATHS */
    userpath = (char *)datapath;
#endif /* USE_PRIVATE_PATHS */

    /* Build the path to the score and save directories */
    BUILD_DIRECTORY_PATH(ANGBAND_DIR_SCORES, userpath, "scores");
    BUILD_DIRECTORY_PATH(ANGBAND_DIR_SAVE, userpath, "save"); 
    BUILD_DIRECTORY_PATH(ANGBAND_DIR_PANIC, userpath, "panic");

#undef BUILD_DIRECTORY_PATH
}


/*
 * Create any missing directories.
 *
 * We create only those dirs which may be empty. The others are assumed
 * to contain required files and therefore must exist at startup.
 */
void create_needed_dirs(void)
{
    char dirpath[MSG_LEN];

    path_build(dirpath, sizeof(dirpath), ANGBAND_DIR_USER, "");
    if (!dir_create(dirpath)) quit_fmt("Cannot create '%s'", dirpath);

    path_build(dirpath, sizeof(dirpath), ANGBAND_DIR_SAVE, "");
    if (!dir_create(dirpath)) quit_fmt("Cannot create '%s'", dirpath);

    path_build(dirpath, sizeof(dirpath), ANGBAND_DIR_PANIC, "");
    if (!dir_create(dirpath)) quit_fmt("Cannot create '%s'", dirpath);

    path_build(dirpath, sizeof(dirpath), ANGBAND_DIR_SCORES, "");
    if (!dir_create(dirpath)) quit_fmt("Cannot create '%s'", dirpath);
}


/*
 * Initialize game constants
 */


static enum parser_error parse_constants_level_max(struct parser *p)
{
    struct angband_constants *z;
    const char *label;
    int value;

    z = parser_priv(p);
    label = parser_getsym(p, "label");
    value = parser_getint(p, "value");

    if (value < 0) return PARSE_ERROR_INVALID_VALUE;

    if (streq(label, "monsters"))
        z->level_monster_max = value;
    else
        return PARSE_ERROR_UNDEFINED_DIRECTIVE;

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_constants_mon_gen(struct parser *p)
{
    struct angband_constants *z;
    const char *label;
    int value;

    z = parser_priv(p);
    label = parser_getsym(p, "label");
    value = parser_getint(p, "value");

    if (value < 0) return PARSE_ERROR_INVALID_VALUE;

    if (streq(label, "chance"))
        z->alloc_monster_chance = value;
    else if (streq(label, "level-min"))
        z->level_monster_min = value;
    else if (streq(label, "town-day"))
        z->town_monsters_day = value;
    else if (streq(label, "town-night"))
        z->town_monsters_night = value;
    else if (streq(label, "repro-max"))
        z->repro_monster_max = value;
    else if (streq(label, "ood-chance"))
        z->ood_monster_chance = value;
    else if (streq(label, "ood-amount"))
        z->ood_monster_amount = value;
    else if (streq(label, "group-max"))
        z->monster_group_max = value;
    else if (streq(label, "group-dist"))
        z->monster_group_dist = value;
    else
        return PARSE_ERROR_UNDEFINED_DIRECTIVE;

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_constants_mon_play(struct parser *p)
{
    struct angband_constants *z;
    const char *label;
    int value;

    z = parser_priv(p);
    label = parser_getsym(p, "label");
    value = parser_getint(p, "value");

    if (value < 0) return PARSE_ERROR_INVALID_VALUE;

    if (streq(label, "break-glyph"))
        z->glyph_hardness = value;
    else if (streq(label, "mult-rate"))
        z->repro_monster_rate = value;
    else if (streq(label, "life-drain"))
        z->life_drain_percent = value;
    else if (streq(label, "flee-range"))
        z->flee_range = value;
    else if (streq(label, "turn-range"))
        z->turn_range = value;
    else
        return PARSE_ERROR_UNDEFINED_DIRECTIVE;

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_constants_dun_gen(struct parser *p)
{
    struct angband_constants *z;
    const char *label;
    int value;

    z = parser_priv(p);
    label = parser_getsym(p, "label");
    value = parser_getint(p, "value");

    if (value < 0) return PARSE_ERROR_INVALID_VALUE;

    if (streq(label, "cent-max"))
        z->level_room_max = value;
    else if (streq(label, "door-max"))
        z->level_door_max = value;
    else if (streq(label, "wall-max"))
        z->wall_pierce_max = value;
    else if (streq(label, "tunn-max"))
        z->tunn_grid_max = value;
    else if (streq(label, "amt-room"))
        z->room_item_av = value;
    else if (streq(label, "amt-item"))
        z->both_item_av = value;
    else if (streq(label, "amt-gold"))
        z->both_gold_av = value;
    else if (streq(label, "pit-max"))
        z->level_pit_max = value;
    else if (streq(label, "lab-depth-lit"))
        z->lab_depth_lit = value;
    else if (streq(label, "lab-depth-known"))
        z->lab_depth_known = value;
    else if (streq(label, "lab-depth-soft"))
        z->lab_depth_soft = value;
    else
        return PARSE_ERROR_UNDEFINED_DIRECTIVE;

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_constants_world(struct parser *p)
{
    struct angband_constants *z;
    const char *label;
    int value;

    z = parser_priv(p);
    label = parser_getsym(p, "label");
    value = parser_getint(p, "value");

    if (value < 0) return PARSE_ERROR_INVALID_VALUE;

    if (streq(label, "max-depth"))
        z->max_depth = value;
    else if (streq(label, "day-length"))
        z->day_length = value;
    else if (streq(label, "dungeon-hgt"))
        z->dungeon_hgt = value;
    else if (streq(label, "dungeon-wid"))
        z->dungeon_wid = value;
    else if (streq(label, "town-hgt"))
        z->town_hgt = value;
    else if (streq(label, "town-wid"))
        z->town_wid = value;
    else if (streq(label, "feeling-total"))
        z->feeling_total = value;
    else if (streq(label, "feeling-need"))
        z->feeling_need = value;
    else if (streq(label, "stair-skip"))
        z->stair_skip = value;
    else if (streq(label, "move-energy"))
        z->move_energy = value;
    else
        return PARSE_ERROR_UNDEFINED_DIRECTIVE;

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_constants_carry_cap(struct parser *p)
{
    struct angband_constants *z;
    const char *label;
    int value;

    z = parser_priv(p);
    label = parser_getsym(p, "label");
    value = parser_getint(p, "value");

    if (value < 0) return PARSE_ERROR_INVALID_VALUE;

    if (streq(label, "pack-size"))
        z->pack_size = value;
    else if (streq(label, "quiver-size"))
        z->quiver_size = value;
    else if (streq(label, "quiver-slot-size"))
        z->quiver_slot_size = value;
    else if (streq(label, "thrown-quiver-mult"))
        z->thrown_quiver_mult = value;
    else if (streq(label, "floor-size"))
    {
        z->floor_size = value;
        if (cfg_house_floor_size > z->floor_size) cfg_house_floor_size = z->floor_size;
    }
    else
        return PARSE_ERROR_UNDEFINED_DIRECTIVE;

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_constants_store(struct parser *p)
{
    struct angband_constants *z;
    const char *label;
    int value;

    z = parser_priv(p);
    label = parser_getsym(p, "label");
    value = parser_getint(p, "value");

    if (value < 0) return PARSE_ERROR_INVALID_VALUE;

    if (streq(label, "inven-max"))
        z->store_inven_max = value;
    else if (streq(label, "home-inven-max"))
        z->home_inven_max = value;
    else if (streq(label, "turns"))
        z->store_turns = value;
    else if (streq(label, "shuffle"))
        z->store_shuffle = value;
    else if (streq(label, "magic-level"))
        z->store_magic_level = value;
    else
        return PARSE_ERROR_UNDEFINED_DIRECTIVE;

    /* Sanity checks */

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_constants_obj_make(struct parser *p)
{
    struct angband_constants *z;
    const char *label;
    int value;

    z = parser_priv(p);
    label = parser_getsym(p, "label");
    value = parser_getint(p, "value");

    if (value < 0) return PARSE_ERROR_INVALID_VALUE;

    if (streq(label, "max-depth"))
        z->max_obj_depth = value;
    else if (streq(label, "good-obj"))
        z->good_obj = value;
    else if (streq(label, "ego-obj"))
        z->ego_obj = value;
    else if (streq(label, "great-obj"))
        z->great_obj = value;
    else if (streq(label, "great-ego"))
        z->great_ego = value;
    else if (streq(label, "fuel-torch"))
        z->fuel_torch = value;
    else if (streq(label, "fuel-lamp"))
        z->fuel_lamp = value;
    else if (streq(label, "default-lamp"))
        z->default_lamp = value;
    else
        return PARSE_ERROR_UNDEFINED_DIRECTIVE;

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_constants_player(struct parser *p)
{
    struct angband_constants *z;
    const char *label;
    int value;

    z = parser_priv(p);
    label = parser_getsym(p, "label");
    value = parser_getint(p, "value");

    if (value < 0) return PARSE_ERROR_INVALID_VALUE;

    if (streq(label, "max-sight"))
        z->max_sight = value;
    else if (streq(label, "max-range"))
        z->max_range = value;
    else if (streq(label, "start-gold"))
        z->start_gold = value;
    else if (streq(label, "food-value"))
        z->food_value = value;
    else
        return PARSE_ERROR_UNDEFINED_DIRECTIVE;

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_constants_melee_critical(struct parser *p)
{
    struct angband_constants *z = parser_priv(p);
    const char *label = parser_getsym(p, "label");
    int value = parser_getint(p, "value");

    if (streq(label, "debuff-toh"))
        z->m_crit_debuff_toh = value;
    else if (streq(label, "chance-weight-scale"))
        z->m_crit_chance_weight_scl = value;
    else if (streq(label, "chance-toh-scale"))
        z->m_crit_chance_toh_scl = value;
    else if (streq(label, "chance-level-scale"))
        z->m_crit_chance_level_scl = value;
    else if (streq(label, "chance-toh-skill-scale"))
        z->m_crit_chance_toh_skill_scl = value;
    else if (streq(label, "chance-offset"))
        z->m_crit_chance_offset = value;
    else if (streq(label, "chance-range"))
        z->m_crit_chance_range = value;
    else if (streq(label, "power-weight-scale"))
        z->m_crit_power_weight_scl = value;
    else if (streq(label, "power-random"))
        z->m_crit_power_random = value;
    else
        return PARSE_ERROR_UNDEFINED_DIRECTIVE;

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_constants_melee_critical_level(struct parser *p)
{
    struct angband_constants *z = parser_priv(p);
    struct critical_level *new_level;
    const char *msgt_str = parser_getstr(p, "msg");
    int msgt = message_lookup_by_name(msgt_str);

    if (msgt < 0) return PARSE_ERROR_INVALID_MESSAGE;
    new_level = mem_alloc(sizeof(*new_level));
    new_level->next = NULL;
    new_level->cutoff = parser_getint(p, "cutoff");
    new_level->mult = parser_getint(p, "mult");
    new_level->add = parser_getint(p, "add");
    new_level->msgt = msgt;

    /* Add it to the end of the linked list. */
    if (z->m_crit_level_head)
    {
        struct critical_level *cursor = z->m_crit_level_head;

        while (cursor->next) cursor = cursor->next;
        cursor->next = new_level;
    }
    else
        z->m_crit_level_head = new_level;

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_constants_ranged_critical(struct parser *p)
{
    struct angband_constants *z = parser_priv(p);
    const char *label = parser_getsym(p, "label");
    int value = parser_getint(p, "value");

    if (streq(label, "debuff-toh"))
        z->r_crit_debuff_toh = value;
    else if (streq(label, "chance-weight-scale"))
        z->r_crit_chance_weight_scl = value;
    else if (streq(label, "chance-toh-scale"))
        z->r_crit_chance_toh_scl = value;
    else if (streq(label, "chance-level-scale"))
        z->r_crit_chance_level_scl = value;
    else if (streq(label, "chance-launched-toh-skill-scale"))
        z->r_crit_chance_launched_toh_skill_scl = value;
    else if (streq(label, "chance-thrown-toh-skill-scale"))
        z->r_crit_chance_thrown_toh_skill_scl = value;
    else if (streq(label, "chance-offset"))
        z->r_crit_chance_offset = value;
    else if (streq(label, "chance-range"))
        z->r_crit_chance_range = value;
    else if (streq(label, "power-weight-scale"))
        z->r_crit_power_weight_scl = value;
    else if (streq(label, "power-random"))
        z->r_crit_power_random = value;
    else
        return PARSE_ERROR_UNDEFINED_DIRECTIVE;

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_constants_ranged_critical_level(struct parser *p)
{
    struct angband_constants *z = parser_priv(p);
    struct critical_level *new_level;
    const char *msgt_str = parser_getstr(p, "msg");
    int msgt = message_lookup_by_name(msgt_str);

    if (msgt < 0) return PARSE_ERROR_INVALID_MESSAGE;
    new_level = mem_alloc(sizeof(*new_level));
    new_level->next = NULL;
    new_level->cutoff = parser_getint(p, "cutoff");
    new_level->mult = parser_getint(p, "mult");
    new_level->add = parser_getint(p, "add");
    new_level->msgt = msgt;

    /* Add it to the end of the linked list. */
    if (z->r_crit_level_head)
    {
        struct critical_level *cursor = z->r_crit_level_head;

        while (cursor->next) cursor = cursor->next;
        cursor->next = new_level;
    }
    else
        z->r_crit_level_head = new_level;

    return PARSE_ERROR_NONE;
}


static struct parser *init_parse_constants(void)
{
    struct angband_constants *z = mem_zalloc(sizeof(*z));
    struct parser *p = parser_new();

    parser_setpriv(p, z);
    parser_reg(p, "level-max sym label int value", parse_constants_level_max);
    parser_reg(p, "mon-gen sym label int value", parse_constants_mon_gen);
    parser_reg(p, "mon-play sym label int value", parse_constants_mon_play);
    parser_reg(p, "dun-gen sym label int value", parse_constants_dun_gen);
    parser_reg(p, "world sym label int value", parse_constants_world);
    parser_reg(p, "carry-cap sym label int value", parse_constants_carry_cap);
    parser_reg(p, "store sym label int value", parse_constants_store);
    parser_reg(p, "obj-make sym label int value", parse_constants_obj_make);
    parser_reg(p, "player sym label int value", parse_constants_player);
    parser_reg(p, "melee-critical sym label int value", parse_constants_melee_critical);
    parser_reg(p, "melee-critical-level int cutoff int mult int add str msg",
        parse_constants_melee_critical_level);
    parser_reg(p, "ranged-critical sym label int value", parse_constants_ranged_critical);
    parser_reg(p, "ranged-critical-level int cutoff int mult int add str msg",
        parse_constants_ranged_critical_level);

    return p;
}


static errr run_parse_constants(struct parser *p)
{
    return parse_file_quit_not_found(p, "constants");
}


static int check_critical_levels(const struct critical_level *head)
{
    /* Reject if the cutoffs, except for the last one which is unused, do not strictly increase. */
    if (!head) return 0;
    while (head->next)
    {
        int prev_cutoff = head->cutoff;

        head = head->next;
        if (head->next && head->cutoff <= prev_cutoff) return 1;
    }
    return 0;
}


static errr finish_parse_constants(struct parser *p)
{
    z_info = parser_priv(p);
    parser_destroy(p);
    if (check_critical_levels(z_info->m_crit_level_head))
    {
        plog("The cutoffs for melee criticals in constants.txt are not strictly increasing.");
        return PARSE_ERROR_NON_SEQUENTIAL_RECORDS;
    }
    if (check_critical_levels(z_info->r_crit_level_head))
    {
        plog("The cutoffs for ranged criticals in constants.txt are not strictly increasing.");
        return PARSE_ERROR_NON_SEQUENTIAL_RECORDS;
    }

    return 0;
}


static void cleanup_critical_levels(struct critical_level *head)
{
    while (head)
    {
        struct critical_level *target = head;

        head = head->next;
        mem_free(target);
    }
}


static void cleanup_constants(void)
{
    cleanup_critical_levels(z_info->m_crit_level_head);
    cleanup_critical_levels(z_info->r_crit_level_head);
    mem_free(z_info);
    z_info = NULL;
}


static struct file_parser constants_parser =
{
    "constants",
    init_parse_constants,
    run_parse_constants,
    finish_parse_constants,
    cleanup_constants
};


/*
 * Initialize game constants.
 *
 * Assumption: Paths are set up correctly before calling this function.
 */
static void init_game_constants(void)
{
    plog("Initializing constants...");
    if (run_parser(&constants_parser)) quit("Cannot initialize constants");
}


/*
 * Free the game constants
 */
static void cleanup_game_constants(void)
{
    cleanup_parser(&constants_parser);
}


/*
 * Initialize random names
 */


struct name
{
    struct name *next;
    char *str;
};


struct names_parse
{
    unsigned int section;
    unsigned int nnames[RANDNAME_NUM_TYPES];
    struct name *names[RANDNAME_NUM_TYPES];
};


static enum parser_error parse_names_section(struct parser *p)
{
    unsigned int section = parser_getuint(p, "section");
    struct names_parse *s = parser_priv(p);

    if (section >= RANDNAME_NUM_TYPES) return PARSE_ERROR_OUT_OF_BOUNDS;
    s->section = section;

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_names_word(struct parser *p)
{
    const char *name = parser_getstr(p, "name");
    struct names_parse *s = parser_priv(p);
    struct name *ns = mem_zalloc(sizeof(*ns));

    s->nnames[s->section]++;
    ns->next = s->names[s->section];
    ns->str = string_make(name);
    s->names[s->section] = ns;

    return PARSE_ERROR_NONE;
}


static struct parser *init_parse_names(void)
{
    struct parser *p = parser_new();
    struct names_parse *n = mem_zalloc(sizeof(*n));

    n->section = 0;
    parser_setpriv(p, n);
    parser_reg(p, "section uint section", parse_names_section);
    parser_reg(p, "word str name", parse_names_word);

    return p;
}


static errr run_parse_names(struct parser *p)
{
    return parse_file_quit_not_found(p, "names");
}


static errr finish_parse_names(struct parser *p)
{
    int i;
    unsigned int j;
    struct names_parse *n = parser_priv(p);
    struct name *nm;

    num_names = mem_zalloc(RANDNAME_NUM_TYPES * sizeof(uint32_t));
    name_sections = mem_zalloc(sizeof(char**) * RANDNAME_NUM_TYPES);
    for (i = 0; i < RANDNAME_NUM_TYPES; i++)
    {
        num_names[i] = n->nnames[i];
        name_sections[i] = mem_alloc(sizeof(char*) * (n->nnames[i] + 1));
        for (nm = n->names[i], j = 0; nm && j < n->nnames[i]; nm = nm->next, j++)
            name_sections[i][j] = nm->str;
        name_sections[i][n->nnames[i]] = NULL;
        while (n->names[i])
        {
            nm = n->names[i]->next;
            mem_free(n->names[i]);
            n->names[i] = nm;
        }
    }
    mem_free(n);
    parser_destroy(p);

    return 0;
}


static void cleanup_names(void)
{
    strings_free(name_sections, num_names, RANDNAME_NUM_TYPES);
    name_sections = NULL;
    num_names = NULL;
}


static struct file_parser names_parser =
{
    "names",
    init_parse_names,
    run_parse_names,
    finish_parse_names,
    cleanup_names
};


/*
 * Initialize traps
 */


static enum parser_error parse_trap_name(struct parser *p)
{
    const char *name = parser_getsym(p, "name");
    const char *desc = parser_getstr(p, "desc");
    struct trap_kind *h = parser_priv(p);
    struct trap_kind *t = mem_zalloc(sizeof(*t));

    t->next = h;
    t->name = string_make(name);
    t->desc = string_make(desc);
    parser_setpriv(p, t);

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_trap_graphics(struct parser *p)
{
    char glyph = parser_getchar(p, "glyph");
    const char *color = parser_getsym(p, "color");
    int attr = 0;
    struct trap_kind *t = parser_priv(p);

    if (!t) return PARSE_ERROR_MISSING_RECORD_HEADER;
    t->d_char = glyph;
    if (strlen(color) > 1)
		attr = color_text_to_attr(color);
    else
		attr = color_char_to_attr(color[0]);
    if (attr < 0) return PARSE_ERROR_INVALID_COLOR;
    t->d_attr = attr;

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_trap_appear(struct parser *p)
{
    struct trap_kind *t = parser_priv(p);

    if (!t) return PARSE_ERROR_MISSING_RECORD_HEADER;
    t->rarity = parser_getuint(p, "rarity");
    t->min_depth = parser_getuint(p, "mindepth");
    t->max_num = parser_getuint(p, "maxnum");

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_trap_visibility(struct parser *p)
{
    struct trap_kind *t = parser_priv(p);
    dice_t *dice = NULL;

    if (!t) return PARSE_ERROR_MISSING_RECORD_HEADER;
    dice = dice_new();
    if (dice == NULL) return PARSE_ERROR_INVALID_DICE;
    if (!dice_parse_string(dice, parser_getstr(p, "visibility")))
    {
        dice_free(dice);
        return PARSE_ERROR_INVALID_DICE;
    }
    dice_random_value(dice, NULL, &t->power);
    dice_free(dice);

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_trap_flags(struct parser *p)
{
    struct trap_kind *t = parser_priv(p);
    char *flags, *s;

    if (!t) return PARSE_ERROR_MISSING_RECORD_HEADER;
    if (!parser_hasval(p, "flags")) return PARSE_ERROR_NONE;
    flags = string_make(parser_getstr(p, "flags"));
    s = strtok(flags, " |");
    while (s)
    {
		if (grab_flag(t->flags, TRF_SIZE, trap_flags, s)) break;
		s = strtok(NULL, " |");
    }

    string_free(flags);
    return (s? PARSE_ERROR_INVALID_FLAG: PARSE_ERROR_NONE);
}


static enum parser_error parse_trap_effect(struct parser *p)
{
	struct trap_kind *t = parser_priv(p);
    struct effect *new_effect;
    errr ret;

	if (!t) return PARSE_ERROR_MISSING_RECORD_HEADER;
    new_effect = mem_zalloc(sizeof(*new_effect));

    /* Fill in the detail */
    ret = grab_effect_data(p, new_effect);
    if (ret) return ret;

    new_effect->next = t->effect;
    t->effect = new_effect;

	return PARSE_ERROR_NONE;
}


static enum parser_error parse_trap_effect_yx(struct parser *p)
{
    struct trap_kind *t = parser_priv(p);

    if (!t) return PARSE_ERROR_MISSING_RECORD_HEADER;

    /* If there is no effect, assume that this is human and not parser error. */
    if (t->effect == NULL) return PARSE_ERROR_NONE;

    t->effect->y = parser_getint(p, "y");
    t->effect->x = parser_getint(p, "x");

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_trap_dice(struct parser *p)
{
    struct trap_kind *t = parser_priv(p);
    dice_t *dice;
    const char *string;

    if (!t) return PARSE_ERROR_MISSING_RECORD_HEADER;

    /* If there is no effect, assume that this is human and not parser error. */
    if (t->effect == NULL) return PARSE_ERROR_NONE;

    dice = dice_new();
    if (dice == NULL) return PARSE_ERROR_INVALID_DICE;

    string = parser_getstr(p, "dice");
    if (dice_parse_string(dice, string))
    {
        dice_free(t->effect->dice);
        t->effect->dice = dice;
    }
    else
    {
        dice_free(dice);
        return PARSE_ERROR_INVALID_DICE;
    }

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_trap_expr(struct parser *p)
{
    struct trap_kind *t = parser_priv(p);
    expression_t *expression;
    expression_base_value_f function;
    const char *name;
    const char *base;
    const char *expr;
    enum parser_error result;

    if (!t) return PARSE_ERROR_MISSING_RECORD_HEADER;

    /* If there is no effect, assume that this is human and not parser error. */
    if (t->effect == NULL) return PARSE_ERROR_NONE;

    /* If there are no dice, assume that this is human and not parser error. */
    if (t->effect->dice == NULL) return PARSE_ERROR_NONE;

    name = parser_getsym(p, "name");
    base = parser_getsym(p, "base");
    expr = parser_getstr(p, "expr");
    expression = expression_new();
    if (expression == NULL) return PARSE_ERROR_INVALID_EXPRESSION;

    function = effect_value_base_by_name(base);
    expression_set_base_value(expression, function);
    if (expression_add_operations_string(expression, expr) < 0)
        result = PARSE_ERROR_BAD_EXPRESSION_STRING;
    else if (dice_bind_expression(t->effect->dice, name, expression) < 0)
        result = PARSE_ERROR_UNBOUND_EXPRESSION;
    else
        result = PARSE_ERROR_NONE;

    /* The dice object makes a deep copy of the expression, so we can free it */
    expression_free(expression);

    return result;
}


static enum parser_error parse_trap_effect_xtra(struct parser *p)
{
	struct trap_kind *t = parser_priv(p);
    struct effect *new_effect;
    errr ret;

	if (!t) return PARSE_ERROR_MISSING_RECORD_HEADER;

    new_effect = mem_zalloc(sizeof(*new_effect));

    /* Fill in the detail */
    ret = grab_effect_data(p, new_effect);
    if (ret) return ret;

    new_effect->next = t->effect_xtra;
    t->effect_xtra = new_effect;

	return PARSE_ERROR_NONE;
}


static enum parser_error parse_trap_effect_yx_xtra(struct parser *p)
{
    struct trap_kind *t = parser_priv(p);

    if (!t) return PARSE_ERROR_MISSING_RECORD_HEADER;

    /* If there is no effect, assume that this is human and not parser error. */
    if (t->effect_xtra == NULL) return PARSE_ERROR_NONE;

    t->effect_xtra->y = parser_getint(p, "y");
    t->effect_xtra->x = parser_getint(p, "x");

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_trap_dice_xtra(struct parser *p)
{
    struct trap_kind *t = parser_priv(p);
    dice_t *dice;
    const char *string;

    if (!t) return PARSE_ERROR_MISSING_RECORD_HEADER;

    /* If there is no effect, assume that this is human and not parser error. */
    if (t->effect_xtra == NULL) return PARSE_ERROR_NONE;

    dice = dice_new();
    if (dice == NULL) return PARSE_ERROR_INVALID_DICE;

    string = parser_getstr(p, "dice");
    if (dice_parse_string(dice, string))
    {
        dice_free(t->effect_xtra->dice);
        t->effect_xtra->dice = dice;
    }
    else
    {
        dice_free(dice);
        return PARSE_ERROR_INVALID_DICE;
    }

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_trap_expr_xtra(struct parser *p)
{
    struct trap_kind *t = parser_priv(p);
    expression_t *expression;
    expression_base_value_f function;
    const char *name;
    const char *base;
    const char *expr;
    enum parser_error result;

    if (!t) return PARSE_ERROR_MISSING_RECORD_HEADER;

    /* If there is no effect, assume that this is human and not parser error. */
    if (t->effect_xtra == NULL) return PARSE_ERROR_NONE;

    /* If there are no dice, assume that this is human and not parser error. */
    if (t->effect_xtra->dice == NULL) return PARSE_ERROR_NONE;

    name = parser_getsym(p, "name");
    base = parser_getsym(p, "base");
    expr = parser_getstr(p, "expr");
    expression = expression_new();
    if (expression == NULL) return PARSE_ERROR_INVALID_EXPRESSION;

    function = effect_value_base_by_name(base);
    expression_set_base_value(expression, function);
    if (expression_add_operations_string(expression, expr) < 0)
        result = PARSE_ERROR_BAD_EXPRESSION_STRING;
    else if (dice_bind_expression(t->effect_xtra->dice, name, expression) < 0)
        result = PARSE_ERROR_UNBOUND_EXPRESSION;
    else
        result = PARSE_ERROR_NONE;

    /* The dice object makes a deep copy of the expression, so we can free it */
    expression_free(expression);

    return result;
}


static enum parser_error parse_trap_save_flags(struct parser *p)
{
    struct trap_kind *t = parser_priv(p);
    char *s, *u;

    if (!t) return PARSE_ERROR_MISSING_RECORD_HEADER;
    s = string_make(parser_getstr(p, "flags"));
    u = strtok(s, " |");
    while (u)
    {
        if (grab_flag(t->save_flags, OF_SIZE, list_obj_flag_names, u)) break;
        u = strtok(NULL, " |");
    }

    string_free(s);
    return (u? PARSE_ERROR_INVALID_FLAG: PARSE_ERROR_NONE);
}


static enum parser_error parse_trap_desc(struct parser *p)
{
    struct trap_kind *t = parser_priv(p);

    if (!t) return PARSE_ERROR_MISSING_RECORD_HEADER;
    t->text = string_append(t->text, parser_getstr(p, "text"));

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_trap_msg(struct parser *p)
{
    struct trap_kind *t = parser_priv(p);

    if (!t) return PARSE_ERROR_MISSING_RECORD_HEADER;
    t->msg = string_append(t->msg, parser_getstr(p, "text"));

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_trap_msg_good(struct parser *p)
{
    struct trap_kind *t = parser_priv(p);

    if (!t) return PARSE_ERROR_MISSING_RECORD_HEADER;
    t->msg_good = string_append(t->msg_good, parser_getstr(p, "text"));

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_trap_msg_bad(struct parser *p)
{
    struct trap_kind *t = parser_priv(p);

    if (!t) return PARSE_ERROR_MISSING_RECORD_HEADER;
    t->msg_bad = string_append(t->msg_bad, parser_getstr(p, "text"));

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_trap_msg_xtra(struct parser *p)
{
    struct trap_kind *t = parser_priv(p);

    if (!t) return PARSE_ERROR_MISSING_RECORD_HEADER;
    t->msg_xtra = string_append(t->msg_xtra, parser_getstr(p, "text"));

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_trap_msg_death(struct parser *p)
{
    struct trap_kind *t = parser_priv(p);

    if (!t) return PARSE_ERROR_MISSING_RECORD_HEADER;
    t->msg_death_type = parser_getint(p, "type");
    t->msg_death = string_append(t->msg_death, parser_getstr(p, "text"));

    return PARSE_ERROR_NONE;
}


static struct parser *init_parse_trap(void)
{
    struct parser *p = parser_new();

    parser_setpriv(p, NULL);
    parser_reg(p, "name sym name str desc", parse_trap_name);
    parser_reg(p, "graphics char glyph sym color", parse_trap_graphics);
    parser_reg(p, "appear uint rarity uint mindepth uint maxnum", parse_trap_appear);
    parser_reg(p, "visibility str visibility", parse_trap_visibility);
    parser_reg(p, "flags ?str flags", parse_trap_flags);
	parser_reg(p, "effect sym eff ?sym type ?int radius ?int other", parse_trap_effect);
    parser_reg(p, "effect-yx int y int x", parse_trap_effect_yx);
    parser_reg(p, "dice str dice", parse_trap_dice);
    parser_reg(p, "expr sym name sym base str expr", parse_trap_expr);
    parser_reg(p, "effect-xtra sym eff ?sym type ?int radius ?int other", parse_trap_effect_xtra);
    parser_reg(p, "effect-yx-xtra int y int x", parse_trap_effect_yx_xtra);
    parser_reg(p, "dice-xtra str dice", parse_trap_dice_xtra);
    parser_reg(p, "expr-xtra sym name sym base str expr", parse_trap_expr_xtra);
    parser_reg(p, "save str flags", parse_trap_save_flags);
    parser_reg(p, "desc str text", parse_trap_desc);
    parser_reg(p, "msg str text", parse_trap_msg);
    parser_reg(p, "msg-good str text", parse_trap_msg_good);
    parser_reg(p, "msg-bad str text", parse_trap_msg_bad);
    parser_reg(p, "msg-xtra str text", parse_trap_msg_xtra);
    parser_reg(p, "msg-death int type str text", parse_trap_msg_death);

    return p;
}


static errr run_parse_trap(struct parser *p)
{
    return parse_file_quit_not_found(p, "trap");
}


static errr finish_parse_trap(struct parser *p)
{
	struct trap_kind *t, *n;
    int tidx;

	/* Scan the list for the max id */
	z_info->trap_max = 0;
	t = parser_priv(p);
	while (t)
    {
		z_info->trap_max++;
		t = t->next;
	}

	/* Allocate the direct access list and copy the data to it */
    trap_info = mem_zalloc(z_info->trap_max * sizeof(*t));
    tidx = z_info->trap_max - 1;
    for (t = parser_priv(p); t; t = n, tidx--)
    {
		memcpy(&trap_info[tidx], t, sizeof(*t));
        trap_info[tidx].tidx = tidx;
        n = t->next;
        if (tidx < z_info->trap_max - 1) trap_info[tidx].next = &trap_info[tidx + 1];
        else trap_info[tidx].next = NULL;
        mem_free(t);
    }

    parser_destroy(p);
    return 0;
}


static void cleanup_trap(void)
{
	int i;

    /* Paranoia */
    if (!trap_info) return;

	for (i = 0; i < z_info->trap_max; i++)
    {
		string_free(trap_info[i].name);
		string_free(trap_info[i].text);
        string_free(trap_info[i].desc);
        string_free(trap_info[i].msg);
        string_free(trap_info[i].msg_good);
        string_free(trap_info[i].msg_bad);
        string_free(trap_info[i].msg_xtra);
        string_free(trap_info[i].msg_death);
        free_effect(trap_info[i].effect);
        free_effect(trap_info[i].effect_xtra);
	}
	mem_free(trap_info);
    trap_info = NULL;
}


static struct file_parser trap_parser =
{
    "trap",
    init_parse_trap,
    run_parse_trap,
    finish_parse_trap,
    cleanup_trap
};


/*
 * Initialize terrain
 */


static enum parser_error parse_feat_code(struct parser *p)
{
    const char *code = parser_getstr(p, "code");
    int idx = lookup_feat_code(code);
    struct feature *f;

    if (idx < 0 || idx >= FEAT_MAX) return PARSE_ERROR_OUT_OF_BOUNDS;
    f = &f_info[idx];
    f->fidx = idx;
    parser_setpriv(p, f);
    return PARSE_ERROR_NONE;
}


static enum parser_error parse_feat_name(struct parser *p)
{
    const char *name = parser_getstr(p, "name");
    struct feature *f = parser_priv(p);

    if (!f) return PARSE_ERROR_MISSING_RECORD_HEADER;
    if (f->name) return PARSE_ERROR_REPEATED_DIRECTIVE;
    f->name = string_make(name);

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_feat_graphics(struct parser *p)
{
    char glyph = parser_getchar(p, "glyph");
    const char *color = parser_getsym(p, "color");
    int attr = 0;
    struct feature *f = parser_priv(p);

    if (!f) return PARSE_ERROR_MISSING_RECORD_HEADER;
    f->d_char = glyph;
    if (strlen(color) > 1)
        attr = color_text_to_attr(color);
    else
        attr = color_char_to_attr(color[0]);
    if (attr < 0) return PARSE_ERROR_INVALID_COLOR;
    f->d_attr = attr;

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_feat_mimic(struct parser *p)
{
    const char *mimic_name = parser_getstr(p, "feat");
    struct feature *f = parser_priv(p);
    int mimic_idx;

    if (!f) return PARSE_ERROR_MISSING_RECORD_HEADER;

    /* Verify that it refers to a valid feature. */
    mimic_idx = lookup_feat_code(mimic_name);
    if (mimic_idx < 0 || mimic_idx >= FEAT_MAX) return PARSE_ERROR_OUT_OF_BOUNDS;

    f->mimic = &f_info[mimic_idx];

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_feat_priority(struct parser *p)
{
    unsigned int priority = parser_getuint(p, "priority");
    struct feature *f = parser_priv(p);

    if (!f) return PARSE_ERROR_MISSING_RECORD_HEADER;
    f->priority = priority;

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_feat_flags(struct parser *p)
{
    struct feature *f = parser_priv(p);
    char *flags, *s;

    if (!f) return PARSE_ERROR_MISSING_RECORD_HEADER;
    if (!parser_hasval(p, "flags")) return PARSE_ERROR_NONE;
    flags = string_make(parser_getstr(p, "flags"));
    s = strtok(flags, " |");
    while (s)
    {
        if (grab_flag(f->flags, TF_SIZE, terrain_flags, s)) break;
        s = strtok(NULL, " |");
    }

    string_free(flags);
    return (s? PARSE_ERROR_INVALID_FLAG: PARSE_ERROR_NONE);
}


static enum parser_error parse_feat_digging(struct parser *p)
{
    struct feature *f = parser_priv(p);
    int dig_idx = parser_getint(p, "dig");

    if (!f) return PARSE_ERROR_MISSING_RECORD_HEADER;
    if (dig_idx < DIGGING_TREE + 1 || dig_idx >= DIGGING_MAX + 1)
        return PARSE_ERROR_OUT_OF_BOUNDS;
    f->dig = dig_idx;

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_feat_shortdesc(struct parser *p)
{
    struct feature *f = parser_priv(p);

    if (!f) return PARSE_ERROR_MISSING_RECORD_HEADER;
    f->shortdesc = string_make(parser_getstr(p, "text"));

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_feat_desc(struct parser *p)
{
    struct feature *f = parser_priv(p);

    if (!f) return PARSE_ERROR_MISSING_RECORD_HEADER;
    f->desc = string_append(f->desc, parser_getstr(p, "text"));

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_feat_hurt_msg(struct parser *p)
{
    struct feature *f = parser_priv(p);

    if (!f) return PARSE_ERROR_MISSING_RECORD_HEADER;
    f->hurt_msg = string_append(f->hurt_msg, parser_getstr(p, "text"));

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_feat_died_flavor(struct parser *p)
{
    struct feature *f = parser_priv(p);

    if (!f) return PARSE_ERROR_MISSING_RECORD_HEADER;
    f->died_flavor = string_append(f->died_flavor, parser_getstr(p, "text"));

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_feat_die_msg(struct parser *p)
{
    struct feature *f = parser_priv(p);

    if (!f) return PARSE_ERROR_MISSING_RECORD_HEADER;
    f->die_msg = string_append(f->die_msg, parser_getstr(p, "text"));

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_feat_confused_msg(struct parser *p)
{
    struct feature *f = parser_priv(p);

    if (!f) return PARSE_ERROR_MISSING_RECORD_HEADER;
    f->confused_msg = string_append(f->confused_msg, parser_getstr(p, "text"));

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_feat_look_prefix(struct parser *p)
{
    struct feature *f = parser_priv(p);

    if (!f) return PARSE_ERROR_MISSING_RECORD_HEADER;
    f->look_prefix = string_append(f->look_prefix, parser_getstr(p, "text"));

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_feat_look_in_preposition(struct parser *p)
{
    struct feature *f = parser_priv(p);

    if (!f) return PARSE_ERROR_MISSING_RECORD_HEADER;
    f->look_in_preposition = string_append(f->look_in_preposition, parser_getstr(p, "text"));

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_feat_resist_flag(struct parser *p)
{
    struct feature *f = parser_priv(p);
    int flag = lookup_flag(r_info_flags, parser_getsym(p, "flag"));

    if (!f) return PARSE_ERROR_MISSING_RECORD_HEADER;
    if (flag == FLAG_END) return PARSE_ERROR_INVALID_FLAG;
    f->resist_flag = flag;

    return PARSE_ERROR_NONE;
}


static struct parser *init_parse_feat(void)
{
    struct parser *p = parser_new();

    parser_setpriv(p, NULL);
    parser_reg(p, "code str code", parse_feat_code);
    parser_reg(p, "name str name", parse_feat_name);
    parser_reg(p, "graphics char glyph sym color", parse_feat_graphics);
    parser_reg(p, "mimic str feat", parse_feat_mimic);
    parser_reg(p, "priority uint priority", parse_feat_priority);
    parser_reg(p, "flags ?str flags", parse_feat_flags);
    parser_reg(p, "digging int dig", parse_feat_digging);
    parser_reg(p, "short-desc str text", parse_feat_shortdesc);
    parser_reg(p, "desc str text", parse_feat_desc);
    parser_reg(p, "hurt-msg str text", parse_feat_hurt_msg);
    parser_reg(p, "died-flavor str text", parse_feat_died_flavor);
    parser_reg(p, "die-msg str text", parse_feat_die_msg);
    parser_reg(p, "confused-msg str text", parse_feat_confused_msg);
    parser_reg(p, "look-prefix str text", parse_feat_look_prefix);
    parser_reg(p, "look-in-preposition str text", parse_feat_look_in_preposition);
    parser_reg(p, "resist-flag sym flag", parse_feat_resist_flag);

    /*
     * Since the layout of the terrain array is fixed by list-terrain.h,
     * allocate it now and fill in the customizable parts when parsing.
     */
    f_info = mem_zalloc(FEAT_MAX * sizeof(*f_info));

    return p;
}


static errr run_parse_feat(struct parser *p)
{
    return parse_file_quit_not_found(p, "terrain");
}


static errr finish_parse_feat(struct parser *p)
{
    int shop_idx = 0, fidx;

    for (fidx = 0; fidx < FEAT_MAX; ++fidx)
    {
        /* Assign shop index based on the order within the other terrain. */
        if (tf_has(f_info[fidx].flags, TF_SHOP)) f_info[fidx].shopnum = ++shop_idx;

        /*
         * Ensure the prefixes and prepositions end with a space for
         * ease of use with the targeting code.
         */
        if (f_info[fidx].look_prefix && !suffix(f_info[fidx].look_prefix, " "))
            f_info[fidx].look_prefix = string_append(f_info[fidx].look_prefix, " ");
        if (f_info[fidx].look_in_preposition && !suffix(f_info[fidx].look_in_preposition, " "))
            f_info[fidx].look_in_preposition = string_append(f_info[fidx].look_in_preposition, " ");
    }

    z_info->store_max = shop_idx;

    /* Non-feature: placeholder for player stores */
    z_info->store_max++;

    parser_destroy(p);
    return 0;
}


static void cleanup_feat(void)
{
    int i;

    /* Paranoia */
    if (!f_info) return;

    for (i = 0; i < FEAT_MAX; i++)
    {
        string_free(f_info[i].look_in_preposition);
        string_free(f_info[i].look_prefix);
        string_free(f_info[i].confused_msg);
        string_free(f_info[i].die_msg);
        string_free(f_info[i].died_flavor);
        string_free(f_info[i].hurt_msg);
        string_free(f_info[i].desc);
        string_free(f_info[i].shortdesc);
        string_free(f_info[i].name);
    }
    mem_free(f_info);
    f_info = NULL;
}


static struct file_parser feat_parser =
{
    "terrain",
    init_parse_feat,
    run_parse_feat,
    finish_parse_feat,
    cleanup_feat
};


/*
 * Initialize player bodies
 */


static enum parser_error parse_body_body(struct parser *p)
{
    struct player_body *h = parser_priv(p);
    struct player_body *b = mem_zalloc(sizeof(*b));

    b->next = h;
    b->name = string_make(parser_getstr(p, "name"));
    parser_setpriv(p, b);

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_body_slot(struct parser *p)
{
    struct player_body *b = parser_priv(p);
    struct equip_slot *slot;
    int n;

    if (!b) return PARSE_ERROR_MISSING_RECORD_HEADER;

    /* Go to the last valid slot, then allocate a new one */
    slot = b->slots;
    if (!slot)
    {
        b->slots = mem_zalloc(sizeof(struct equip_slot));
        slot = b->slots;
    }
    else
    {
        while (slot->next) slot = slot->next;
        slot->next = mem_zalloc(sizeof(struct equip_slot));
        slot = slot->next;
    }

    n = lookup_flag(slots, parser_getsym(p, "slot"));
    if (!n) return PARSE_ERROR_INVALID_FLAG;
    slot->type = n;
    slot->name = string_make(parser_getsym(p, "name"));
    b->count++;

    return PARSE_ERROR_NONE;
}


static struct parser *init_parse_body(void)
{
    struct parser *p = parser_new();

    parser_setpriv(p, NULL);
    parser_reg(p, "body str name", parse_body_body);
    parser_reg(p, "slot sym slot sym name", parse_body_slot);

    return p;
}


static errr run_parse_body(struct parser *p)
{
    return parse_file_quit_not_found(p, "body");
}


static errr finish_parse_body(struct parser *p)
{
    struct player_body *b;
    int i;

    /* Scan the list for the max slots */
    z_info->equip_slots_max = 0;
    bodies = parser_priv(p);
    for (b = bodies; b; b = b->next)
    {
        if (b->count > z_info->equip_slots_max)
            z_info->equip_slots_max = b->count;
    }

    /* Allocate the slot list and copy */
    for (b = bodies; b; b = b->next)
    {
        struct equip_slot *s_new, *s, *sn = NULL;

        s_new = mem_zalloc(z_info->equip_slots_max * sizeof(struct equip_slot));
        for (i = 0, s = b->slots; s; i++, s = sn)
        {
            memcpy(&s_new[i], s, sizeof(*s));
            s_new[i].next = NULL;
            sn = s->next;
            mem_free(s);
        }
        b->slots = s_new;
    }

    parser_destroy(p);
    return 0;
}


static struct file_parser body_parser =
{
    "body",
    init_parse_body,
    run_parse_body,
    finish_parse_body,
    cleanup_body
};


/*
 * Initialize player histories
 */


static struct history_chart *histories;


static struct history_chart *findchart(struct history_chart *hs, unsigned int idx)
{
    for (; hs; hs = hs->next)
    {
        if (hs->idx == idx) break;
    }
    return hs;
}


static enum parser_error parse_history_chart(struct parser *p)
{
    struct history_chart *oc = parser_priv(p);
    struct history_chart *c;
    struct history_entry *e = mem_zalloc(sizeof(*e));
    unsigned int idx = parser_getuint(p, "chart");

    c = findchart(oc, idx);
    if (!c)
    {
        c = mem_zalloc(sizeof(*c));
        c->next = oc;
        c->idx = idx;
        parser_setpriv(p, c);
    }

    e->isucc = parser_getint(p, "next");
    e->roll = parser_getint(p, "roll");

    e->next = c->entries;
    c->entries = e;

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_history_phrase(struct parser *p)
{
    struct history_chart *h = parser_priv(p);

    if (!h) return PARSE_ERROR_MISSING_RECORD_HEADER;
    my_assert(h->entries);
    h->entries->text = string_append(h->entries->text, parser_getstr(p, "text"));

    return PARSE_ERROR_NONE;
}


static struct parser *init_parse_history(void)
{
    struct parser *p = parser_new();

    parser_setpriv(p, NULL);
    parser_reg(p, "chart uint chart int next int roll", parse_history_chart);
    parser_reg(p, "phrase str text", parse_history_phrase);

    return p;
}


static errr run_parse_history(struct parser *p)
{
    return parse_file_quit_not_found(p, "history");
}


static errr finish_parse_history(struct parser *p)
{
    struct history_chart *c;
    struct history_entry *e, *prev, *next;

    histories = parser_priv(p);

    /*
     * Go fix up the entry successor pointers. We can't compute them at
     * load-time since we may not have seen the successor history yet. Also,
     * we need to put the entries in the right order; the parser actually
     * stores them backwards, which is not desirable.
     */
    for (c = histories; c; c = c->next)
    {
        e = c->entries;
        prev = NULL;
        while (e)
        {
            next = e->next;
            e->next = prev;
            prev = e;
            e = next;
        }
        c->entries = prev;
        for (e = c->entries; e; e = e->next)
        {
            if (!e->isucc) continue;
            e->succ = findchart(histories, e->isucc);
            if (!e->succ) return -1;
        }
    }

    parser_destroy(p);
    return 0;
}


static void cleanup_history(void)
{
    struct history_chart *c, *next_c;
    struct history_entry *e, *next_e;

    c = histories;
    while (c)
    {
        next_c = c->next;
        e = c->entries;
        while (e)
        {
            next_e = e->next;
            string_free(e->text);
            mem_free(e);
            e = next_e;
        }
        mem_free(c);
        c = next_c;
    }
}


static struct file_parser history_parser =
{
    "history",
    init_parse_history,
    run_parse_history,
    finish_parse_history,
    cleanup_history
};


/*
 * Initialize player races
 */


static enum parser_error parse_p_race_name(struct parser *p)
{
    struct player_race *h = parser_priv(p);
    struct player_race *r = mem_zalloc(sizeof(*r));

    r->next = h;
    r->name = string_make(parser_getstr(p, "name"));

    /* Default body is humanoid */
    r->body = 0;

    parser_setpriv(p, r);

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_p_race_skill_disarm_phys(struct parser *p)
{
    struct player_race *r = parser_priv(p);

    if (!r) return PARSE_ERROR_MISSING_RECORD_HEADER;
    r->r_skills[SKILL_DISARM_PHYS] = parser_getint(p, "disarm");

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_p_race_skill_disarm_magic(struct parser *p)
{
    struct player_race *r = parser_priv(p);

    if (!r) return PARSE_ERROR_MISSING_RECORD_HEADER;
    r->r_skills[SKILL_DISARM_MAGIC] = parser_getint(p, "disarm");

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_p_race_skill_device(struct parser *p)
{
    struct player_race *r = parser_priv(p);

    if (!r) return PARSE_ERROR_MISSING_RECORD_HEADER;
    r->r_skills[SKILL_DEVICE] = parser_getint(p, "device");

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_p_race_skill_save(struct parser *p)
{
    struct player_race *r = parser_priv(p);

    if (!r) return PARSE_ERROR_MISSING_RECORD_HEADER;
    r->r_skills[SKILL_SAVE] = parser_getint(p, "save");

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_p_race_skill_stealth(struct parser *p)
{
    struct player_race *r = parser_priv(p);

    if (!r) return PARSE_ERROR_MISSING_RECORD_HEADER;
    r->r_skills[SKILL_STEALTH] = parser_getint(p, "stealth");

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_p_race_skill_search(struct parser *p)
{
    struct player_race *r = parser_priv(p);

    if (!r) return PARSE_ERROR_MISSING_RECORD_HEADER;
    r->r_skills[SKILL_SEARCH] = parser_getint(p, "search");

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_p_race_skill_melee(struct parser *p)
{
    struct player_race *r = parser_priv(p);

    if (!r) return PARSE_ERROR_MISSING_RECORD_HEADER;
    r->r_skills[SKILL_TO_HIT_MELEE] = parser_getint(p, "melee");

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_p_race_skill_shoot(struct parser *p)
{
    struct player_race *r = parser_priv(p);

    if (!r) return PARSE_ERROR_MISSING_RECORD_HEADER;
    r->r_skills[SKILL_TO_HIT_BOW] = parser_getint(p, "shoot");

    /* Default value for SKILL_TO_HIT_THROW */
    r->r_skills[SKILL_TO_HIT_THROW] = r->r_skills[SKILL_TO_HIT_BOW];

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_p_race_skill_throw(struct parser *p)
{
    struct player_race *r = parser_priv(p);

    if (!r) return PARSE_ERROR_MISSING_RECORD_HEADER;
    r->r_skills[SKILL_TO_HIT_THROW] = parser_getint(p, "throw");

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_p_race_skill_dig(struct parser *p)
{
    struct player_race *r = parser_priv(p);

    if (!r) return PARSE_ERROR_MISSING_RECORD_HEADER;
    r->r_skills[SKILL_DIGGING] = parser_getint(p, "dig");

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_p_race_hitdie(struct parser *p)
{
    struct player_race *r = parser_priv(p);

    if (!r) return PARSE_ERROR_MISSING_RECORD_HEADER;
    r->r_mhp = parser_getint(p, "mhp");

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_p_race_exp(struct parser *p)
{
    struct player_race *r = parser_priv(p);
    int exp = parser_getint(p, "exp");
    int cexp = parser_getint(p, "cexp");

    if (!r) return PARSE_ERROR_MISSING_RECORD_HEADER;
    if (cfg_classic_exp_factor) r->r_exp = cexp;
    else r->r_exp = exp;

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_p_race_history(struct parser *p)
{
    struct player_race *r = parser_priv(p);

    if (!r) return PARSE_ERROR_MISSING_RECORD_HEADER;
    r->history = findchart(histories, parser_getuint(p, "hist"));

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_p_race_age(struct parser *p)
{
    struct player_race *r = parser_priv(p);

    if (!r) return PARSE_ERROR_MISSING_RECORD_HEADER;
    r->b_age = parser_getint(p, "base_age");
    r->m_age = parser_getint(p, "mod_age");

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_p_race_height(struct parser *p)
{
    struct player_race *r = parser_priv(p);

    if (!r) return PARSE_ERROR_MISSING_RECORD_HEADER;
    r->base_hgt = parser_getint(p, "base_hgt");
    r->mod_hgt = parser_getint(p, "mod_hgt");

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_p_race_weight(struct parser *p)
{
    struct player_race *r = parser_priv(p);

    if (!r) return PARSE_ERROR_MISSING_RECORD_HEADER;
    r->base_wgt = parser_getint(p, "base_wgt");
    r->mod_wgt = parser_getint(p, "mod_wgt");

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_p_race_obj_flag(struct parser *p)
{
    struct player_race *r = parser_priv(p);
    uint8_t level;
    int flag;

    if (!r) return PARSE_ERROR_MISSING_RECORD_HEADER;

    level = (uint8_t)parser_getuint(p, "level");
    flag = lookup_flag(list_obj_flag_names, parser_getstr(p, "flag"));
    if (flag == FLAG_END) return PARSE_ERROR_INVALID_FLAG;
    of_on(r->flags, flag);
    r->flvl[flag] = level;

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_p_race_obj_brand(struct parser *p)
{
    struct player_race *r = parser_priv(p);
    uint8_t minlvl;
    uint8_t maxlvl;
    const char *s;
    int i;

    if (!r) return PARSE_ERROR_MISSING_RECORD_HEADER;

    minlvl = (uint8_t)parser_getuint(p, "minlvl");
    maxlvl = (uint8_t)parser_getuint(p, "maxlvl");
    s = parser_getstr(p, "code");
    for (i = 0; i < z_info->brand_max; i++)
    {
        if (streq(s, brands[i].code)) break;
    }
    if (i == z_info->brand_max)
        return PARSE_ERROR_UNRECOGNISED_BRAND;

    if (!r->brands)
        r->brands = mem_zalloc(z_info->brand_max * sizeof(struct brand_info));

    r->brands[i].brand = true;
    r->brands[i].minlvl = minlvl;
    r->brands[i].maxlvl = maxlvl;

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_p_race_obj_slay(struct parser *p)
{
    struct player_race *r = parser_priv(p);
    uint8_t minlvl;
    uint8_t maxlvl;
    const char *s;
    int i;

    if (!r) return PARSE_ERROR_MISSING_RECORD_HEADER;

    minlvl = (uint8_t)parser_getuint(p, "minlvl");
    maxlvl = (uint8_t)parser_getuint(p, "maxlvl");
    s = parser_getstr(p, "code");
    for (i = 0; i < z_info->slay_max; i++)
    {
        if (streq(s, slays[i].code)) break;
    }
    if (i == z_info->slay_max)
        return PARSE_ERROR_UNRECOGNISED_SLAY;

    if (!r->slays)
        r->slays = mem_zalloc(z_info->slay_max * sizeof(struct slay_info));

    r->slays[i].slay = true;
    r->slays[i].minlvl = minlvl;
    r->slays[i].maxlvl = maxlvl;

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_p_race_play_flags(struct parser *p)
{
    struct player_race *r = parser_priv(p);
    char *flags;
    char *s;
    uint8_t level;
    int flag;

    if (!r) return PARSE_ERROR_MISSING_RECORD_HEADER;

    level = (uint8_t)parser_getuint(p, "level");
    if (!parser_hasval(p, "flags")) return PARSE_ERROR_NONE;
    flags = string_make(parser_getstr(p, "flags"));
    s = strtok(flags, " |");
    while (s)
    {
        if (grab_flag_aux(r->pflags, PF_SIZE, player_info_flags, s, &flag)) break;
        r->pflvl[flag] = level;
        s = strtok(NULL, " |");
    }
    string_free(flags);

    return (s? PARSE_ERROR_INVALID_FLAG: PARSE_ERROR_NONE);
}


static enum parser_error parse_p_race_value(struct parser *p)
{
    struct player_race *r = parser_priv(p);
    uint8_t level;
    const char *name_and_value;
    random_value rvalue;
    int value = 0;
    int index = 0;
    bool found = false;

    if (!r) return PARSE_ERROR_MISSING_RECORD_HEADER;

    level = (uint8_t)parser_getuint(p, "level");
    name_and_value = parser_getstr(p, "value");

    if (!grab_index_and_rand(&rvalue, &index, obj_mods, name_and_value))
    {
        found = true;
        r->modifiers[index].value.base = rvalue.base;
        r->modifiers[index].value.dice = rvalue.dice;
        r->modifiers[index].value.sides = rvalue.sides;
        r->modifiers[index].value.m_bonus = rvalue.m_bonus;
        r->modifiers[index].lvl = level;
    }
    if (!grab_index_and_int(&value, &index, list_element_names, "RES_", name_and_value))
    {
        found = true;
        if (r->el_info[index].idx == MAX_EL_INFO) return PARSE_ERROR_TOO_MANY_ENTRIES;
        r->el_info[index].res_level[r->el_info[index].idx] = value;
        r->el_info[index].lvl[r->el_info[index].idx] = level;
        r->el_info[index].idx++;
    }
    if (!found) return PARSE_ERROR_INVALID_VALUE;

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_p_race_shape(struct parser *p)
{
    struct player_race *r = parser_priv(p);
    struct player_shape *shape;

    if (!r) return PARSE_ERROR_MISSING_RECORD_HEADER;

    shape = mem_zalloc(sizeof(*shape));
    shape->next = r->shapes;
    shape->name = string_make(parser_getstr(p, "name"));
    shape->lvl = (uint8_t)parser_getuint(p, "level");
    r->shapes = shape;

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_p_race_attack(struct parser *p)
{
    struct player_race *r = parser_priv(p);
    struct barehanded_attack *attack;
    errr ret;

    if (!r) return PARSE_ERROR_MISSING_RECORD_HEADER;

    attack = mem_zalloc(sizeof(*attack));

    /* Fill in the detail */
    ret = grab_barehanded_attack(p, attack);
    if (ret) return ret;

    attack->next = r->attacks;
    r->attacks = attack;

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_p_race_gift(struct parser *p)
{
    struct player_race *r = parser_priv(p);
    struct gift *g;
    int tval, sval;
    struct object_kind *kind;

    if (!r) return PARSE_ERROR_MISSING_RECORD_HEADER;

    tval = tval_find_idx(parser_getsym(p, "tval"));
    if (tval < 0) return PARSE_ERROR_UNRECOGNISED_TVAL;

    sval = lookup_sval(tval, parser_getsym(p, "sval"));
    if (sval < 0) return PARSE_ERROR_UNRECOGNISED_SVAL;

    g = mem_zalloc(sizeof(*g));
    g->tval = tval;
    g->sval = sval;
    g->min = parser_getuint(p, "min");
    g->max = parser_getuint(p, "max");

    kind = lookup_kind(g->tval, g->sval);
    if (tval_is_money_k(kind))
    {
        /* Not an actual object, don't check max stack */
    }
    else if ((g->min > kind->base->max_stack) || (g->max > kind->base->max_stack))
    {
        mem_free(g);
        return PARSE_ERROR_INVALID_ITEM_NUMBER;
    }

    g->next = r->gifts;
    r->gifts = g;

    return PARSE_ERROR_NONE;
}


static struct parser *init_parse_p_race(void)
{
    struct parser *p = parser_new();

    parser_setpriv(p, NULL);
    parser_reg(p, "name str name", parse_p_race_name);
    parser_reg(p, "skill-disarm-phys int disarm", parse_p_race_skill_disarm_phys);
    parser_reg(p, "skill-disarm-magic int disarm", parse_p_race_skill_disarm_magic);
    parser_reg(p, "skill-device int device", parse_p_race_skill_device);
    parser_reg(p, "skill-save int save", parse_p_race_skill_save);
    parser_reg(p, "skill-stealth int stealth", parse_p_race_skill_stealth);
    parser_reg(p, "skill-search int search", parse_p_race_skill_search);
    parser_reg(p, "skill-melee int melee", parse_p_race_skill_melee);
    parser_reg(p, "skill-shoot int shoot", parse_p_race_skill_shoot);
    parser_reg(p, "skill-throw int throw", parse_p_race_skill_throw);
    parser_reg(p, "skill-dig int dig", parse_p_race_skill_dig);
    parser_reg(p, "hitdie int mhp", parse_p_race_hitdie);
    parser_reg(p, "exp int exp int cexp", parse_p_race_exp);
    parser_reg(p, "history uint hist", parse_p_race_history);
    parser_reg(p, "age int base_age int mod_age", parse_p_race_age);
    parser_reg(p, "height int base_hgt int mod_hgt", parse_p_race_height);
    parser_reg(p, "weight int base_wgt int mod_wgt", parse_p_race_weight);
    parser_reg(p, "obj-flag uint level str flag", parse_p_race_obj_flag);
    parser_reg(p, "brand uint minlvl uint maxlvl str code", parse_p_race_obj_brand);
    parser_reg(p, "slay uint minlvl uint maxlvl str code", parse_p_race_obj_slay);
    parser_reg(p, "player-flags uint level ?str flags", parse_p_race_play_flags);
    parser_reg(p, "value uint level str value", parse_p_race_value);
    parser_reg(p, "shape uint level str name", parse_p_race_shape);
    parser_reg(p, "attack sym verb sym extra int level int chance str effect", parse_p_race_attack);
    parser_reg(p, "gift sym tval sym sval uint min uint max", parse_p_race_gift);

    return p;
}


static errr run_parse_p_race(struct parser *p)
{
    return parse_file_quit_not_found(p, "p_race");
}


static errr finish_parse_p_race(struct parser *p)
{
    struct player_race *r;
    int num = 0;

    races = parser_priv(p);
    for (r = races; r; r = r->next) num++;
    for (r = races; r; r = r->next, num--) r->ridx = num - 1;
    parser_destroy(p);
    return 0;
}


static struct file_parser p_race_parser =
{
    "p_race",
    init_parse_p_race,
    run_parse_p_race,
    finish_parse_p_race,
    cleanup_p_race
};


/*
 * Initialize dragon breeds
 */


static enum parser_error parse_dragon_breed_dragon(struct parser *p)
{
    struct dragon_breed *h = parser_priv(p);
    struct dragon_breed *r = mem_zalloc(sizeof(*r));

    r->next = h;
    r->d_name = string_make(parser_getsym(p, "name"));
    r->d_fmt = (uint8_t)parser_getuint(p, "format");

    parser_setpriv(p, r);

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_dragon_breed_wyrm(struct parser *p)
{
    struct dragon_breed *r = parser_priv(p);

    if (!r) return PARSE_ERROR_MISSING_RECORD_HEADER;
    r->w_name = string_make(parser_getsym(p, "name"));
    r->w_fmt = (uint8_t)parser_getuint(p, "format");

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_dragon_breed_info(struct parser *p)
{
    struct dragon_breed *r = parser_priv(p);

    if (!r) return PARSE_ERROR_MISSING_RECORD_HEADER;
    r->commonness = (uint8_t)parser_getuint(p, "commonness");
    r->r_exp = (int16_t)parser_getint(p, "r_exp");
    r->immune = (uint8_t)parser_getuint(p, "immune");

    return PARSE_ERROR_NONE;
}


static struct parser *init_parse_dragon_breed(void)
{
    struct parser *p = parser_new();

    parser_setpriv(p, NULL);
    parser_reg(p, "dragon sym name uint format", parse_dragon_breed_dragon);
    parser_reg(p, "wyrm sym name uint format", parse_dragon_breed_wyrm);
    parser_reg(p, "info uint commonness int r_exp uint immune", parse_dragon_breed_info);

    return p;
}


static errr run_parse_dragon_breed(struct parser *p)
{
    return parse_file_quit_not_found(p, "dragon_breed");
}


static errr finish_parse_dragon_breed(struct parser *p)
{
    breeds = parser_priv(p);
    parser_destroy(p);
    return 0;
}


static void cleanup_dragon_breed(void)
{
    struct dragon_breed *p = breeds;
    struct dragon_breed *next;

    while (p)
    {
        next = p->next;
        string_free(p->d_name);
        string_free(p->w_name);
        mem_free(p);
        p = next;
    }
}


static struct file_parser dragon_breed_parser =
{
    "dragon_breed",
    init_parse_dragon_breed,
    run_parse_dragon_breed,
    finish_parse_dragon_breed,
    cleanup_dragon_breed
};


/*
 * Initialize player magic realms
 */


static enum parser_error parse_realm_name(struct parser *p)
{
    struct magic_realm *h = parser_priv(p);
    struct magic_realm *realm = mem_zalloc(sizeof(*realm));
    const char *name = parser_getstr(p, "name");

    realm->next = h;
    realm->name = string_make(name);

    parser_setpriv(p, realm);

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_realm_stat(struct parser *p)
{
    struct magic_realm *realm = parser_priv(p);

    if (!realm) return PARSE_ERROR_MISSING_RECORD_HEADER;
    realm->stat = stat_name_to_idx(parser_getsym(p, "stat"));
    if (realm->stat < 0) return PARSE_ERROR_INVALID_SPELL_STAT;

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_realm_verb(struct parser *p)
{
    struct magic_realm *realm = parser_priv(p);
    const char *verb = parser_getstr(p, "verb");

    if (!realm) return PARSE_ERROR_MISSING_RECORD_HEADER;
    string_free(realm->verb);
    realm->verb = string_make(verb);

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_realm_spell_noun(struct parser *p)
{
    struct magic_realm *realm = parser_priv(p);
    const char *spell = parser_getstr(p, "spell");

    if (!realm) return PARSE_ERROR_MISSING_RECORD_HEADER;
    string_free(realm->spell_noun);
    realm->spell_noun = string_make(spell);

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_realm_book_noun(struct parser *p)
{
    struct magic_realm *realm = parser_priv(p);
    const char *book = parser_getstr(p, "book");

    if (!realm) return PARSE_ERROR_MISSING_RECORD_HEADER;
    string_free(realm->book_noun);
    realm->book_noun = string_make(book);

    return PARSE_ERROR_NONE;
}


static struct parser *init_parse_realm(void)
{
    struct parser *p = parser_new();

    parser_setpriv(p, NULL);
    parser_reg(p, "name str name", parse_realm_name);
    parser_reg(p, "stat sym stat", parse_realm_stat);
    parser_reg(p, "verb str verb", parse_realm_verb);
    parser_reg(p, "spell-noun str spell", parse_realm_spell_noun);
    parser_reg(p, "book-noun str book", parse_realm_book_noun);

    return p;
}


static errr run_parse_realm(struct parser *p)
{
    return parse_file_quit_not_found(p, "realm");
}


static errr finish_parse_realm(struct parser *p)
{
    realms = parser_priv(p);
    parser_destroy(p);
    return 0;
}


static struct file_parser realm_parser =
{
    "realm",
    init_parse_realm,
    run_parse_realm,
    finish_parse_realm,
    cleanup_realm
};


/*
 * Initialize player classes
 */


/*
 * Used to remember the maximum number of books for the current class and the
 * maximum number of spells in the current book while parsing so bounds
 * checking can be done.
 */
static int class_max_books = 0;
static int book_max_spells = 0;


static enum parser_error parse_class_name(struct parser *p)
{
    struct player_class *h = parser_priv(p);
    struct player_class *c = mem_zalloc(sizeof(*c));

    c->name = string_make(parser_getstr(p, "name"));
    c->next = h;
    parser_setpriv(p, c);

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_class_color(struct parser *p)
{
    const char *color = parser_getsym(p, "color");
    struct player_class *c = parser_priv(p);

    if (!c) return PARSE_ERROR_MISSING_RECORD_HEADER;
    c->attr = color_char_to_attr(color[0]);

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_class_skill_disarm_phys(struct parser *p)
{
    struct player_class *c = parser_priv(p);

    if (!c) return PARSE_ERROR_MISSING_RECORD_HEADER;
    c->c_skills[SKILL_DISARM_PHYS] = parser_getint(p, "base");
    c->x_skills[SKILL_DISARM_PHYS] = parser_getint(p, "incr");

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_class_skill_disarm_magic(struct parser *p)
{
    struct player_class *c = parser_priv(p);

    if (!c) return PARSE_ERROR_MISSING_RECORD_HEADER;
    c->c_skills[SKILL_DISARM_MAGIC] = parser_getint(p, "base");
    c->x_skills[SKILL_DISARM_MAGIC] = parser_getint(p, "incr");

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_class_skill_device(struct parser *p)
{
    struct player_class *c = parser_priv(p);

    if (!c) return PARSE_ERROR_MISSING_RECORD_HEADER;
    c->c_skills[SKILL_DEVICE] = parser_getint(p, "base");
    c->x_skills[SKILL_DEVICE] = parser_getint(p, "incr");

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_class_skill_save(struct parser *p)
{
    struct player_class *c = parser_priv(p);

    if (!c) return PARSE_ERROR_MISSING_RECORD_HEADER;
    c->c_skills[SKILL_SAVE] = parser_getint(p, "base");
    c->x_skills[SKILL_SAVE] = parser_getint(p, "incr");

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_class_skill_stealth(struct parser *p)
{
    struct player_class *c = parser_priv(p);

    if (!c) return PARSE_ERROR_MISSING_RECORD_HEADER;
    c->c_skills[SKILL_STEALTH] = parser_getint(p, "base");
    c->x_skills[SKILL_STEALTH] = parser_getint(p, "incr");

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_class_skill_search(struct parser *p)
{
    struct player_class *c = parser_priv(p);

    if (!c) return PARSE_ERROR_MISSING_RECORD_HEADER;
    c->c_skills[SKILL_SEARCH] = parser_getint(p, "base");
    c->x_skills[SKILL_SEARCH] = parser_getint(p, "incr");

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_class_skill_melee(struct parser *p)
{
    struct player_class *c = parser_priv(p);

    if (!c) return PARSE_ERROR_MISSING_RECORD_HEADER;
    c->c_skills[SKILL_TO_HIT_MELEE] = parser_getint(p, "base");
    c->x_skills[SKILL_TO_HIT_MELEE] = parser_getint(p, "incr");

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_class_skill_shoot(struct parser *p)
{
    struct player_class *c = parser_priv(p);

    if (!c) return PARSE_ERROR_MISSING_RECORD_HEADER;
    c->c_skills[SKILL_TO_HIT_BOW] = parser_getint(p, "base");
    c->x_skills[SKILL_TO_HIT_BOW] = parser_getint(p, "incr");

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_class_skill_throw(struct parser *p)
{
    struct player_class *c = parser_priv(p);

    if (!c) return PARSE_ERROR_MISSING_RECORD_HEADER;
    c->c_skills[SKILL_TO_HIT_THROW] = parser_getint(p, "base");
    c->x_skills[SKILL_TO_HIT_THROW] = parser_getint(p, "incr");

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_class_skill_dig(struct parser *p)
{
    struct player_class *c = parser_priv(p);

    if (!c) return PARSE_ERROR_MISSING_RECORD_HEADER;
    c->c_skills[SKILL_DIGGING] = parser_getint(p, "base");
    c->x_skills[SKILL_DIGGING] = parser_getint(p, "incr");

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_class_hitdie(struct parser *p)
{
    struct player_class *c = parser_priv(p);

    if (!c) return PARSE_ERROR_MISSING_RECORD_HEADER;
    c->c_mhp = parser_getint(p, "mhp");

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_class_max_attacks(struct parser *p)
{
    struct player_class *c = parser_priv(p);

    if (!c) return PARSE_ERROR_MISSING_RECORD_HEADER;
    c->max_attacks = parser_getint(p, "max-attacks");

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_class_min_weight(struct parser *p)
{
    struct player_class *c = parser_priv(p);

    if (!c) return PARSE_ERROR_MISSING_RECORD_HEADER;
    c->min_weight = parser_getint(p, "min-weight");

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_class_str_mult(struct parser *p)
{
    struct player_class *c = parser_priv(p);

    if (!c) return PARSE_ERROR_MISSING_RECORD_HEADER;
    c->att_multiply = parser_getint(p, "att-multiply");

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_class_title(struct parser *p)
{
    struct player_class *c = parser_priv(p);
    int n, i;

    if (!c) return PARSE_ERROR_MISSING_RECORD_HEADER;
    n = (int)N_ELEMENTS(c->title);
    for (i = 0; i < n; i++)
    {
        if (!c->title[i])
        {
            c->title[i] = string_make(parser_getstr(p, "title"));
            break;
        }
    }

    return ((i >= n)? PARSE_ERROR_TOO_MANY_ENTRIES: PARSE_ERROR_NONE);
}


static int lookup_option(const char *name)
{
    int result = 1;

    while (1)
    {
        if (result >= OPT_MAX) return 0;
        if (streq(option_name(result), name)) return result;
        ++result;
    }
}


static enum parser_error parse_class_equip(struct parser *p)
{
    struct player_class *c = parser_priv(p);
    struct start_item *si;
    int tval, sval;
    char *eopts;
    char *s;
    int *einds;
    int nind, nalloc;
    struct object_kind *kind;

    if (!c) return PARSE_ERROR_MISSING_RECORD_HEADER;

    tval = tval_find_idx(parser_getsym(p, "tval"));
    if (tval < 0) return PARSE_ERROR_UNRECOGNISED_TVAL;

    sval = lookup_sval(tval, parser_getsym(p, "sval"));
    if (sval < 0) return PARSE_ERROR_UNRECOGNISED_SVAL;

    eopts = string_make(parser_getsym(p, "eopts"));
    einds = NULL;
    nind = 0;
    nalloc = 0;
    s = strtok(eopts, " |");
    while (s)
    {
        bool negated = false;
        int ind;

        if (prefix(s, "NOT-"))
        {
            negated = true;
            s += 4;
        }
        ind = lookup_option(s);
        if (ind > 0 && option_type(ind) == OP_BIRTH)
        {
            if (nind >= nalloc - 2)
            {
                if (nalloc == 0) nalloc = 2;
                else nalloc *= 2;
                einds = mem_realloc(einds, nalloc * sizeof(*einds));
            }
            einds[nind] = (negated? -ind: ind);
            einds[nind + 1] = 0;
            ++nind;
        }
        else if (!streq(s, "none"))
        {
            mem_free(einds);
            string_free(eopts);
            return PARSE_ERROR_INVALID_OPTION;
        }
        s = strtok(NULL, " |");
    }
    string_free(eopts);

    si = mem_zalloc(sizeof(*si));
    si->tval = tval;
    si->sval = sval;
    si->min = parser_getuint(p, "min");
    si->max = parser_getuint(p, "max");
    si->eopts = einds;

    kind = lookup_kind(si->tval, si->sval);
    if ((si->min > kind->base->max_stack) || (si->max > kind->base->max_stack))
    {
        mem_free(si->eopts);
        mem_free(si);
        return PARSE_ERROR_INVALID_ITEM_NUMBER;
    }

    si->next = c->start_items;
    c->start_items = si;

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_class_obj_flag(struct parser *p)
{
    struct player_class *c = parser_priv(p);
    uint8_t level;
    int flag;

    if (!c) return PARSE_ERROR_MISSING_RECORD_HEADER;

    level = (uint8_t)parser_getuint(p, "level");
    flag = lookup_flag(list_obj_flag_names, parser_getstr(p, "flag"));
    if (flag == FLAG_END) return PARSE_ERROR_INVALID_FLAG;
    of_on(c->flags, flag);
    c->flvl[flag] = level;

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_class_obj_brand(struct parser *p)
{
    struct player_class *c = parser_priv(p);
    uint8_t minlvl, maxlvl;
    const char *s;
    int i;

    if (!c) return PARSE_ERROR_MISSING_RECORD_HEADER;

    minlvl = (uint8_t)parser_getuint(p, "minlvl");
    maxlvl = (uint8_t)parser_getuint(p, "maxlvl");
    s = parser_getstr(p, "code");
    for (i = 0; i < z_info->brand_max; i++)
    {
        if (streq(s, brands[i].code)) break;
    }
    if (i == z_info->brand_max)
        return PARSE_ERROR_UNRECOGNISED_BRAND;

    if (!c->brands)
        c->brands = mem_zalloc(z_info->brand_max * sizeof(struct brand_info));

    c->brands[i].brand = true;
    c->brands[i].minlvl = minlvl;
    c->brands[i].maxlvl = maxlvl;

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_class_obj_slay(struct parser *p)
{
    struct player_class *c = parser_priv(p);
    uint8_t minlvl;
    uint8_t maxlvl;
    const char *s;
    int i;

    if (!c) return PARSE_ERROR_MISSING_RECORD_HEADER;

    minlvl = (uint8_t)parser_getuint(p, "minlvl");
    maxlvl = (uint8_t)parser_getuint(p, "maxlvl");
    s = parser_getstr(p, "code");
    for (i = 0; i < z_info->slay_max; i++)
    {
        if (streq(s, slays[i].code)) break;
    }
    if (i == z_info->slay_max)
        return PARSE_ERROR_UNRECOGNISED_SLAY;

    if (!c->slays)
        c->slays = mem_zalloc(z_info->slay_max * sizeof(struct slay_info));

    c->slays[i].slay = true;
    c->slays[i].minlvl = minlvl;
    c->slays[i].maxlvl = maxlvl;

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_class_play_flags(struct parser *p)
{
    struct player_class *c = parser_priv(p);
    char *flags;
    char *s;
    uint8_t level;
    int flag;

    if (!c) return PARSE_ERROR_MISSING_RECORD_HEADER;

    level = (uint8_t)parser_getuint(p, "level");
    if (!parser_hasval(p, "flags")) return PARSE_ERROR_NONE;
    flags = string_make(parser_getstr(p, "flags"));
    s = strtok(flags, " |");
    while (s)
    {
        if (grab_flag_aux(c->pflags, PF_SIZE, player_info_flags, s, &flag)) break;
        c->pflvl[flag] = level;
        s = strtok(NULL, " |");
    }

    string_free(flags);
    return (s? PARSE_ERROR_INVALID_FLAG: PARSE_ERROR_NONE);
}


static enum parser_error parse_p_class_value(struct parser *p)
{
    struct player_class *c = parser_priv(p);
    uint8_t level;
    const char *name_and_value;
    random_value rvalue;
    int value = 0;
    int index = 0;
    bool found = false;

    if (!c) return PARSE_ERROR_MISSING_RECORD_HEADER;

    level = (uint8_t)parser_getuint(p, "level");
    name_and_value = parser_getstr(p, "value");

    if (!grab_index_and_rand(&rvalue, &index, obj_mods, name_and_value))
    {
        found = true;
        c->modifiers[index].value.base = rvalue.base;
        c->modifiers[index].value.dice = rvalue.dice;
        c->modifiers[index].value.sides = rvalue.sides;
        c->modifiers[index].value.m_bonus = rvalue.m_bonus;
        c->modifiers[index].lvl = level;
    }
    if (!grab_index_and_int(&value, &index, list_element_names, "RES_", name_and_value))
    {
        found = true;
        if (c->el_info[index].idx == MAX_EL_INFO) return PARSE_ERROR_TOO_MANY_ENTRIES;
        c->el_info[index].res_level[c->el_info[index].idx] = value;
        c->el_info[index].lvl[c->el_info[index].idx] = level;
        c->el_info[index].idx++;
    }
    if (!found) return PARSE_ERROR_INVALID_VALUE;

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_p_class_shape(struct parser *p)
{
    struct player_class *c = parser_priv(p);
    struct player_shape *shape;

    if (!c) return PARSE_ERROR_MISSING_RECORD_HEADER;

    shape = mem_zalloc(sizeof(*shape));
    shape->next = c->shapes;
    shape->name = string_make(parser_getstr(p, "name"));
    shape->lvl = (uint8_t)parser_getuint(p, "level");
    c->shapes = shape;

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_class_attack(struct parser *p)
{
    struct player_class *c = parser_priv(p);
    struct barehanded_attack *attack;
    errr ret;

    if (!c) return PARSE_ERROR_MISSING_RECORD_HEADER;

    attack = mem_zalloc(sizeof(*attack));

    /* Fill in the detail */
    ret = grab_barehanded_attack(p, attack);
    if (ret) return ret;

    attack->next = c->attacks;
    c->attacks = attack;

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_class_magic(struct parser *p)
{
    struct player_class *c = parser_priv(p);
    int num_books;

    if (!c) return PARSE_ERROR_MISSING_RECORD_HEADER;
    if (c->magic.books)
    {
        /* There's more than one magic directive for this class. */
        return PARSE_ERROR_REPEATED_DIRECTIVE;
    }
    c->magic.spell_first = parser_getuint(p, "first");
    c->magic.spell_weight = parser_getint(p, "weight");
    num_books = parser_getint(p, "books");
    c->magic.books = mem_zalloc(num_books * sizeof(struct class_book));
    class_max_books = num_books;

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_class_book(struct parser *p)
{
    struct player_class *c = parser_priv(p);
    int tval, spells;
    const char *name, *quality;
    struct class_book *b;

    if (!c) return PARSE_ERROR_MISSING_RECORD_HEADER;

    tval = tval_find_idx(parser_getsym(p, "tval"));
    if (tval < 0) return PARSE_ERROR_UNRECOGNISED_TVAL;
    if (!c->magic.books || c->magic.num_books >= class_max_books)
        return PARSE_ERROR_TOO_MANY_ENTRIES;
    my_assert(c->magic.num_books >= 0);
    b = &c->magic.books[c->magic.num_books];
    b->tval = tval;

    quality = parser_getsym(p, "quality");
    if (streq(quality, "dungeon")) b->dungeon = true;
    name = parser_getsym(p, "name");

    /* Ghost/mimic spells have no sval */
    if ((tval != TV_GHOST_REALM) && (tval != TV_MIMIC_REALM))
        write_book_kind(b, name);

    spells = parser_getuint(p, "spells");
    b->spells = mem_zalloc(spells * sizeof(struct class_spell));
    book_max_spells = spells;
    b->realm = lookup_realm(parser_getstr(p, "realm"));
    ++c->magic.num_books;

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_class_book_graphics(struct parser *p)
{
    struct player_class *c = parser_priv(p);
    char glyph = parser_getchar(p, "glyph");
    const char *color = parser_getsym(p, "color");
    struct class_book *b;
    struct object_kind *k;

    if (!c) return PARSE_ERROR_MISSING_RECORD_HEADER;
    if (c->magic.num_books < 1)
    {
        /*
         * Either missing a magic directive for the class or didn't
         * have a book directive after the magic directive.
         */
        return PARSE_ERROR_MISSING_RECORD_HEADER;
    }
    my_assert(c->magic.books && c->magic.num_books <= class_max_books);
    b = &c->magic.books[c->magic.num_books - 1];
    k = lookup_kind(b->tval, b->sval);
    my_assert(k);
    k->d_char = glyph;
    if (strlen(color) > 1)
        k->d_attr = color_text_to_attr(color);
    else
        k->d_attr = color_char_to_attr(color[0]);

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_class_book_properties(struct parser *p)
{
    struct player_class *c = parser_priv(p);
    struct class_book *b;
    struct object_kind *k;

    if (!c) return PARSE_ERROR_MISSING_RECORD_HEADER;
    if (c->magic.num_books < 1)
    {
        /*
         * Either missing a magic directive for the class or didn't
         * have a book directive after the magic directive.
         */
        return PARSE_ERROR_MISSING_RECORD_HEADER;
    }
    my_assert(c->magic.books && c->magic.num_books <= class_max_books);
    b = &c->magic.books[c->magic.num_books - 1];
    k = lookup_kind(b->tval, b->sval);
    my_assert(k);
    k->level = parser_getint(p, "level");
    k->weight = parser_getint(p, "weight");
    k->cost = parser_getint(p, "cost");

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_class_book_alloc(struct parser *p)
{
    struct player_class *c = parser_priv(p);
    struct class_book *b;
    struct object_kind *k;
    const char *tmp = parser_getstr(p, "minmax");
    int amin, amax;

    if (!c) return PARSE_ERROR_MISSING_RECORD_HEADER;
    if (c->magic.num_books < 1)
    {
        /*
         * Either missing a magic directive for the class or didn't
         * have a book directive after the magic directive.
         */
        return PARSE_ERROR_MISSING_RECORD_HEADER;
    }
    my_assert(c->magic.books && c->magic.num_books <= class_max_books);
    b = &c->magic.books[c->magic.num_books - 1];
    k = lookup_kind(b->tval, b->sval);
    my_assert(k);
    k->alloc_prob = parser_getint(p, "common");
    if (grab_int_range(&amin, &amax, tmp, "to")) return PARSE_ERROR_INVALID_ALLOCATION;

    k->alloc_min = amin;
    k->alloc_max = amax;

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_class_book_desc(struct parser *p)
{
    struct player_class *c = parser_priv(p);
    struct class_book *b;
    struct object_kind *k;

    if (!c) return PARSE_ERROR_MISSING_RECORD_HEADER;
    if (c->magic.num_books < 1)
    {
        /*
         * Either missing a magic directive for the class or didn't
         * have a book directive after the magic directive.
         */
        return PARSE_ERROR_MISSING_RECORD_HEADER;
    }
    my_assert(c->magic.books && c->magic.num_books <= class_max_books);
    b = &c->magic.books[c->magic.num_books - 1];
    k = lookup_kind(b->tval, b->sval);
    my_assert(k);
    k->text = string_append(k->text, parser_getstr(p, "text"));

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_class_spell(struct parser *p)
{
    struct player_class *c = parser_priv(p);
    struct class_book *book;
    struct class_spell *spell;

    if (!c) return PARSE_ERROR_MISSING_RECORD_HEADER;
    if (c->magic.num_books < 1)
    {
        /*
         * Either missing a magic directive for the class or didn't
         * have a book directive after the magic directive. Use
         * this under the assumption that without those, the maximum
         * number of spells is zero.
         */
        return PARSE_ERROR_TOO_MANY_ENTRIES;
    }
    my_assert(c->magic.books && c->magic.num_books <= class_max_books);
    book = &c->magic.books[c->magic.num_books - 1];
    if (book->num_spells >= book_max_spells) return PARSE_ERROR_TOO_MANY_ENTRIES;
    my_assert(book->spells && book->num_spells >= 0);
    spell = &book->spells[book->num_spells];
    spell->realm = book->realm;
    spell->name = string_make(parser_getsym(p, "name"));
    spell->sidx = c->magic.total_spells;
    c->magic.total_spells++;
    spell->bidx = c->magic.num_books - 1;
    spell->slevel = parser_getint(p, "level");
    spell->smana = parser_getint(p, "mana");
    spell->sfail = parser_getint(p, "fail");
    spell->sexp = parser_getint(p, "exp");
    spell->sproj = parser_getuint(p, "sproj");
    ++book->num_spells;

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_class_cooldown(struct parser *p)
{
    struct player_class *c = parser_priv(p);
    struct class_book *book;
    struct class_spell *spell;

    if (!c) return PARSE_ERROR_MISSING_RECORD_HEADER;
    if (c->magic.num_books < 1)
    {
        /*
         * Either missing a magic directive for the class or didn't
         * have a book directive after the magic directive.
         */
        return PARSE_ERROR_MISSING_RECORD_HEADER;
    }
    my_assert(c->magic.books && c->magic.num_books <= class_max_books);
    book = &c->magic.books[c->magic.num_books - 1];
    if (book->num_spells < 1)
    {
        /* Missing a spell directive after the book directive. */
        return PARSE_ERROR_MISSING_RECORD_HEADER;
    }
    my_assert(book->spells && book->num_spells <= book_max_spells);
    spell = &book->spells[book->num_spells - 1];

    spell->cooldown = parser_getint(p, "cooldown");

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_class_effect(struct parser *p)
{
    struct player_class *c = parser_priv(p);
    struct class_book *book;
    struct class_spell *spell;
    struct effect *new_effect;
    errr ret;

    if (!c) return PARSE_ERROR_MISSING_RECORD_HEADER;
    if (c->magic.num_books < 1)
    {
        /*
         * Either missing a magic directive for the class or didn't
         * have a book directive after the magic directive.
         */
        return PARSE_ERROR_MISSING_RECORD_HEADER;
    }
    my_assert(c->magic.books && c->magic.num_books <= class_max_books);
    book = &c->magic.books[c->magic.num_books - 1];
    if (book->num_spells < 1)
    {
        /* Missing a spell directive after the book directive. */
        return PARSE_ERROR_MISSING_RECORD_HEADER;
    }
    my_assert(book->spells && book->num_spells <= book_max_spells);
    spell = &book->spells[book->num_spells - 1];
    new_effect = mem_zalloc(sizeof(*new_effect));

    /* Fill in the detail */
    ret = grab_effect_data(p, new_effect);
    if (ret) return ret;

    new_effect->next = spell->effect;
    spell->effect = new_effect;

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_class_effect_yx(struct parser *p)
{
    struct player_class *c = parser_priv(p);
    struct class_book *book;
    struct class_spell *spell;

    if (!c) return PARSE_ERROR_MISSING_RECORD_HEADER;
    if (c->magic.num_books < 1)
    {
        /*
         * Either missing a magic directive for the class or didn't
         * have a book directive after the magic directive.
         */
        return PARSE_ERROR_MISSING_RECORD_HEADER;
    }
    my_assert(c->magic.books && c->magic.num_books <= class_max_books);
    book = &c->magic.books[c->magic.num_books - 1];
    if (book->num_spells < 1)
    {
        /* Missing a spell directive after the book directive. */
        return PARSE_ERROR_MISSING_RECORD_HEADER;
    }
    my_assert(book->spells && book->num_spells <= book_max_spells);
    spell = &book->spells[book->num_spells - 1];

    /* If there is no effect, assume that this is human and not parser error. */
    if (spell->effect == NULL) return PARSE_ERROR_NONE;

    spell->effect->y = parser_getint(p, "y");
    spell->effect->x = parser_getint(p, "x");

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_class_flag(struct parser *p)
{
    struct player_class *c = parser_priv(p);
    struct class_book *book;
    struct class_spell *spell;
    int flag;

    if (!c) return PARSE_ERROR_MISSING_RECORD_HEADER;
    if (c->magic.num_books < 1)
    {
        /*
         * Either missing a magic directive for the class or didn't
         * have a book directive after the magic directive.
         */
        return PARSE_ERROR_MISSING_RECORD_HEADER;
    }
    my_assert(c->magic.books && c->magic.num_books <= class_max_books);
    book = &c->magic.books[c->magic.num_books - 1];
    if (book->num_spells < 1)
    {
        /* Missing a spell directive after the book directive. */
        return PARSE_ERROR_MISSING_RECORD_HEADER;
    }
    my_assert(book->spells && book->num_spells <= book_max_spells);
    spell = &book->spells[book->num_spells - 1];

    /* If there is no effect, assume that this is human and not parser error. */
    if (spell->effect == NULL) return PARSE_ERROR_NONE;

    /* Mimic spells are defined by their RSF_XXX flag */
    if (grab_name("flag", parser_getsym(p, "flag"), r_info_spell_flags, RSF_MAX, &flag))
        return PARSE_ERROR_INVALID_FLAG;
    spell->effect->flag = flag;

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_class_dice(struct parser *p)
{
    struct player_class *c = parser_priv(p);
    struct class_book *book;
    struct class_spell *spell;
    dice_t *dice;
    const char *string;

    if (!c) return PARSE_ERROR_MISSING_RECORD_HEADER;
    if (c->magic.num_books < 1)
    {
        /*
         * Either missing a magic directive for the class or didn't
         * have a book directive after the magic directive.
         */
        return PARSE_ERROR_MISSING_RECORD_HEADER;
    }
    my_assert(c->magic.books && c->magic.num_books <= class_max_books);
    book = &c->magic.books[c->magic.num_books - 1];
    if (book->num_spells < 1)
    {
        /* Missing a spell directive after the book directive. */
        return PARSE_ERROR_MISSING_RECORD_HEADER;
    }
    my_assert(book->spells && book->num_spells <= book_max_spells);
    spell = &book->spells[book->num_spells - 1];

    /* If there is no effect, assume that this is human and not parser error. */
    if (spell->effect == NULL) return PARSE_ERROR_NONE;

    dice = dice_new();
    if (dice == NULL) return PARSE_ERROR_INVALID_DICE;

    string = parser_getstr(p, "dice");

    if (dice_parse_string(dice, string))
    {
        dice_free(spell->effect->dice);
        spell->effect->dice = dice;
    }
    else
    {
        dice_free(dice);
        return PARSE_ERROR_INVALID_DICE;
    }

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_class_expr(struct parser *p)
{
    struct player_class *c = parser_priv(p);
    struct class_book *book;
    struct class_spell *spell;
    expression_t *expression;
    expression_base_value_f function;
    const char *name;
    const char *base;
    const char *expr;
    enum parser_error result;

    if (!c) return PARSE_ERROR_MISSING_RECORD_HEADER;
    if (c->magic.num_books < 1)
    {
        /*
         * Either missing a magic directive for the class or didn't
         * have a book directive after the magic directive.
         */
        return PARSE_ERROR_MISSING_RECORD_HEADER;
    }
    my_assert(c->magic.books && c->magic.num_books <= class_max_books);
    book = &c->magic.books[c->magic.num_books - 1];
    if (book->num_spells < 1)
    {
        /* Missing a spell directive after the book directive. */
        return PARSE_ERROR_MISSING_RECORD_HEADER;
    }
    my_assert(book->spells && book->num_spells <= book_max_spells);
    spell = &book->spells[book->num_spells - 1];

    /* If there is no effect, assume that this is human and not parser error. */
    if (spell->effect == NULL) return PARSE_ERROR_NONE;

    /* If there are no dice, assume that this is human and not parser error. */
    if (spell->effect->dice == NULL) return PARSE_ERROR_NONE;

    name = parser_getsym(p, "name");
    base = parser_getsym(p, "base");
    expr = parser_getstr(p, "expr");
    expression = expression_new();
    if (expression == NULL) return PARSE_ERROR_INVALID_EXPRESSION;

    function = effect_value_base_by_name(base);
    expression_set_base_value(expression, function);
    if (expression_add_operations_string(expression, expr) < 0)
        result = PARSE_ERROR_BAD_EXPRESSION_STRING;
    else if (dice_bind_expression(spell->effect->dice, name, expression) < 0)
        result = PARSE_ERROR_UNBOUND_EXPRESSION;
    else
        result = PARSE_ERROR_NONE;

    /* The dice object makes a deep copy of the expression, so we can free it */
    expression_free(expression);

    return result;
}


static enum parser_error parse_class_msg_self(struct parser *p)
{
    struct player_class *c = parser_priv(p);
    struct class_book *book;
    struct class_spell *spell;

    if (!c) return PARSE_ERROR_MISSING_RECORD_HEADER;
    if (c->magic.num_books < 1)
    {
        /*
         * Either missing a magic directive for the class or didn't
         * have a book directive after the magic directive.
         */
        return PARSE_ERROR_MISSING_RECORD_HEADER;
    }
    my_assert(c->magic.books && c->magic.num_books <= class_max_books);
    book = &c->magic.books[c->magic.num_books - 1];
    if (book->num_spells < 1)
    {
        /* Missing a spell directive after the book directive. */
        return PARSE_ERROR_MISSING_RECORD_HEADER;
    }
    my_assert(book->spells && book->num_spells <= book_max_spells);
    spell = &book->spells[book->num_spells - 1];

    /* If there is no effect, assume that this is human and not parser error. */
    if (spell->effect == NULL) return PARSE_ERROR_NONE;

    spell->effect->self_msg = string_make(parser_getstr(p, "msg_self"));

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_class_msg_other(struct parser *p)
{
    struct player_class *c = parser_priv(p);
    struct class_book *book;
    struct class_spell *spell;

    if (!c) return PARSE_ERROR_MISSING_RECORD_HEADER;
    if (c->magic.num_books < 1)
    {
        /*
         * Either missing a magic directive for the class or didn't
         * have a book directive after the magic directive.
         */
        return PARSE_ERROR_MISSING_RECORD_HEADER;
    }
    my_assert(c->magic.books && c->magic.num_books <= class_max_books);
    book = &c->magic.books[c->magic.num_books - 1];
    if (book->num_spells < 1)
    {
        /* Missing a spell directive after the book directive. */
        return PARSE_ERROR_MISSING_RECORD_HEADER;
    }
    my_assert(book->spells && book->num_spells <= book_max_spells);
    spell = &book->spells[book->num_spells - 1];

    /* If there is no effect, assume that this is human and not parser error. */
    if (spell->effect == NULL) return PARSE_ERROR_NONE;

    spell->effect->other_msg = string_make(parser_getstr(p, "msg_other"));

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_class_desc(struct parser *p)
{
    struct player_class *c = parser_priv(p);
    struct class_book *book;
    struct class_spell *spell;

    if (!c) return PARSE_ERROR_MISSING_RECORD_HEADER;
    if (c->magic.num_books < 1)
    {
        /*
         * Either missing a magic directive for the class or didn't
         * have a book directive after the magic directive.
         */
        return PARSE_ERROR_MISSING_RECORD_HEADER;
    }
    my_assert(c->magic.books && c->magic.num_books <= class_max_books);
    book = &c->magic.books[c->magic.num_books - 1];
    if (book->num_spells < 1)
    {
        /* Missing a spell directive after the book directive. */
        return PARSE_ERROR_MISSING_RECORD_HEADER;
    }
    my_assert(book->spells && book->num_spells <= book_max_spells);
    spell = &book->spells[book->num_spells - 1];

    spell->text = string_append(spell->text, parser_getstr(p, "desc"));

    return PARSE_ERROR_NONE;
}


static struct parser *init_parse_class(void)
{
    struct parser *p = parser_new();

    parser_setpriv(p, NULL);
    parser_reg(p, "name str name", parse_class_name);
    parser_reg(p, "color sym color", parse_class_color);
    parser_reg(p, "skill-disarm-phys int base int incr", parse_class_skill_disarm_phys);
    parser_reg(p, "skill-disarm-magic int base int incr", parse_class_skill_disarm_magic);
    parser_reg(p, "skill-device int base int incr", parse_class_skill_device);
    parser_reg(p, "skill-save int base int incr", parse_class_skill_save);
    parser_reg(p, "skill-stealth int base int incr", parse_class_skill_stealth);
    parser_reg(p, "skill-search int base int incr", parse_class_skill_search);
    parser_reg(p, "skill-melee int base int incr", parse_class_skill_melee);
    parser_reg(p, "skill-shoot int base int incr", parse_class_skill_shoot);
    parser_reg(p, "skill-throw int base int incr", parse_class_skill_throw);
    parser_reg(p, "skill-dig int base int incr", parse_class_skill_dig);
    parser_reg(p, "hitdie int mhp", parse_class_hitdie);
    parser_reg(p, "max-attacks int max-attacks", parse_class_max_attacks);
    parser_reg(p, "min-weight int min-weight", parse_class_min_weight);
    parser_reg(p, "strength-multiplier int att-multiply", parse_class_str_mult);
    parser_reg(p, "equip sym tval sym sval uint min uint max sym eopts", parse_class_equip);
    parser_reg(p, "obj-flag uint level str flag", parse_class_obj_flag);
    parser_reg(p, "brand uint minlvl uint maxlvl str code", parse_class_obj_brand);
    parser_reg(p, "slay uint minlvl uint maxlvl str code", parse_class_obj_slay);
    parser_reg(p, "player-flags uint level ?str flags", parse_class_play_flags);
    parser_reg(p, "value uint level str value", parse_p_class_value);
    parser_reg(p, "shape uint level str name", parse_p_class_shape);
    parser_reg(p, "title str title", parse_class_title);
    parser_reg(p, "attack sym verb sym extra int level int chance str effect", parse_class_attack);
    parser_reg(p, "magic uint first int weight int books", parse_class_magic);
    parser_reg(p, "book sym tval sym quality sym name uint spells str realm", parse_class_book);
    parser_reg(p, "book-graphics char glyph sym color", parse_class_book_graphics);
    parser_reg(p, "book-properties int level int weight int cost", parse_class_book_properties);
    parser_reg(p, "book-alloc int common str minmax", parse_class_book_alloc);
    parser_reg(p, "book-desc str text", parse_class_book_desc);
    parser_reg(p, "spell sym name int level int mana int fail int exp uint sproj",
        parse_class_spell);
    parser_reg(p, "cooldown int cooldown", parse_class_cooldown);
    parser_reg(p, "effect sym eff ?sym type ?int radius ?int other", parse_class_effect);
    parser_reg(p, "effect-yx int y int x", parse_class_effect_yx);
    parser_reg(p, "flag sym flag", parse_class_flag);
    parser_reg(p, "dice str dice", parse_class_dice);
    parser_reg(p, "expr sym name sym base str expr", parse_class_expr);
    parser_reg(p, "msg_self str msg_self", parse_class_msg_self);
    parser_reg(p, "msg_other str msg_other", parse_class_msg_other);
    parser_reg(p, "desc str desc", parse_class_desc);

    return p;
}


static errr run_parse_class(struct parser *p)
{
    return parse_file_quit_not_found(p, "class");
}


static errr finish_parse_class(struct parser *p)
{
    struct player_class *c;
    int num = 0;

    classes = parser_priv(p);
    for (c = classes; c; c = c->next) num++;
    for (c = classes; c; c = c->next, num--) c->cidx = num - 1;
    parser_destroy(p);
    return 0;
}


static struct file_parser class_parser =
{
    "class",
    init_parse_class,
    run_parse_class,
    finish_parse_class,
    cleanup_class
};


/*
 * Initialize DM starting items
 */


static enum parser_error parse_dm_start_items(struct parser *p)
{
    struct start_item *h = parser_priv(p);
    struct start_item *si;
    int tval, sval;
    struct object_kind *kind;

    tval = tval_find_idx(parser_getsym(p, "tval"));
    if (tval < 0) return PARSE_ERROR_UNRECOGNISED_TVAL;

    sval = lookup_sval(tval, parser_getsym(p, "sval"));
    if (sval < 0) return PARSE_ERROR_UNRECOGNISED_SVAL;

    si = mem_zalloc(sizeof(*si));
    si->tval = tval;
    si->sval = sval;
    si->min = parser_getuint(p, "amount");

    kind = lookup_kind(si->tval, si->sval);
    if (si->min > kind->base->max_stack)
    {
        mem_free(si);
        return PARSE_ERROR_INVALID_ITEM_NUMBER;
    }

    si->next = h;
    parser_setpriv(p, si);

    return PARSE_ERROR_NONE;
}


static struct parser *init_parse_dm_start_items(void)
{
    struct parser *p = parser_new();

    parser_setpriv(p, NULL);
    parser_reg(p, "equip sym tval sym sval uint amount", parse_dm_start_items);

    return p;
}


static errr run_parse_dm_start_items(struct parser *p)
{
    return parse_file_quit_not_found(p, "admin_stuff");
}


static errr finish_parse_dm_start_items(struct parser *p)
{
    dm_start_items = parser_priv(p);
    parser_destroy(p);
    return 0;
}


static struct file_parser dm_start_items_parser =
{
    "admin_stuff",
    init_parse_dm_start_items,
    run_parse_dm_start_items,
    finish_parse_dm_start_items,
    cleanup_dm_start_items
};


/*
 * Initialize player properties
 */


static enum parser_error parse_player_prop_type(struct parser *p)
{
	const char *type = parser_getstr(p, "type");
	struct player_ability *h = parser_priv(p);
	struct player_ability *ability = mem_zalloc(sizeof(*ability));

	if (h) h->next = ability;
	else player_abilities = ability;
	parser_setpriv(p, ability);
	ability->type = string_make(type);
	return PARSE_ERROR_NONE;
}


static enum parser_error parse_player_prop_code(struct parser *p)
{
	const char *code = parser_getstr(p, "code");
	struct player_ability *ability = parser_priv(p);
    int index = -1;

	if (!ability) return PARSE_ERROR_MISSING_RECORD_HEADER;
    if (!ability->type) return PARSE_ERROR_MISSING_PLAY_PROP_TYPE;

	if (streq(ability->type, "player")) index = code_index_in_array(player_info_flags, code);
    else if (streq(ability->type, "object")) index = code_index_in_array(list_obj_flag_names, code);
    if (index >= 0) ability->index = index;
    else return PARSE_ERROR_INVALID_PLAY_PROP_CODE; 
	return PARSE_ERROR_NONE;
}


static enum parser_error parse_player_prop_desc(struct parser *p)
{
	struct player_ability *ability = parser_priv(p);

	if (!ability) return PARSE_ERROR_MISSING_RECORD_HEADER;

	ability->desc = string_append(ability->desc, parser_getstr(p, "desc"));
	return PARSE_ERROR_NONE;
}


static enum parser_error parse_player_prop_name(struct parser *p)
{
	const char *desc = parser_getstr(p, "desc");
	struct player_ability *ability = parser_priv(p);

	if (!ability) return PARSE_ERROR_MISSING_RECORD_HEADER;

	string_free(ability->name);
    ability->name = string_make(desc);
	return PARSE_ERROR_NONE;
}


static enum parser_error parse_player_prop_value(struct parser *p)
{
	struct player_ability *ability = parser_priv(p);

	if (!ability) return PARSE_ERROR_MISSING_RECORD_HEADER;

	ability->value = parser_getint(p, "value");
	return PARSE_ERROR_NONE;
}


static struct parser *init_parse_player_prop(void)
{
	struct parser *p = parser_new();

	parser_setpriv(p, NULL);
	parser_reg(p, "type str type", parse_player_prop_type);
	parser_reg(p, "code str code", parse_player_prop_code);
	parser_reg(p, "desc str desc", parse_player_prop_desc);
	parser_reg(p, "name str desc", parse_player_prop_name);
    parser_reg(p, "value int value", parse_player_prop_value);
	return p;
}


static errr run_parse_player_prop(struct parser *p)
{
	return parse_file_quit_not_found(p, "player_property");
}


static errr finish_parse_player_prop(struct parser *p)
{
	struct player_ability *ability = player_abilities;
	struct player_ability *new_ability, *previous = NULL;

	/* Copy abilities over, making multiple copies for element types */
    player_abilities = mem_zalloc(sizeof(*player_abilities));
	new_ability = player_abilities;
	while (ability)
    {
		if (streq(ability->type, "element"))
        {
            uint16_t i, n;

            my_assert(N_ELEMENTS(list_element_names) < 65536);
            n = (uint16_t)N_ELEMENTS(list_element_names);

            for (i = 0; i < n - 1; i++)
            {
                char *name = string_make(projections[i].name);

				new_ability->index = i;
				new_ability->type = string_make(ability->type);
				new_ability->desc = string_make(format("%s %s.", ability->desc, name));
                my_strcap(name);
				new_ability->name = string_make(format("%s %s", name, ability->name));
                string_free(name);
                new_ability->value = ability->value;
				if ((i != n - 2) || ability->next)
                {
					previous = new_ability;
					new_ability = mem_zalloc(sizeof(*new_ability));
					previous->next = new_ability;
				}
			}
			string_free(ability->type);
			string_free(ability->desc);
			string_free(ability->name);
			previous = ability;
			ability = ability->next;
			mem_free(previous);
		}
        else
        {
			new_ability->type = ability->type;
			new_ability->index = ability->index;
			new_ability->desc = ability->desc;
			new_ability->name = ability->name;
			if (ability->next)
            {
				previous = new_ability;
				new_ability = mem_zalloc(sizeof(*new_ability));
				previous->next = new_ability;
			}
			previous = ability;
			ability = ability->next;
			mem_free(previous);
		}
	}
	parser_destroy(p);
	return 0;
}


static void cleanup_player_prop(void)
{
	struct player_ability *ability = player_abilities;

	while (ability)
    {
		struct player_ability *totrash = ability;

        ability = ability->next;
		string_free(totrash->type);
		string_free(totrash->desc);
		string_free(totrash->name);
        mem_free(totrash);
	}
}


static struct file_parser player_property_parser =
{
	"player_property",
	init_parse_player_prop,
	run_parse_player_prop,
	finish_parse_player_prop,
	cleanup_player_prop
};


/*
 * Initialize flavors
 */


static char flavor_glyph;
static int flavor_tval;


static enum parser_error parse_flavor_flavor(struct parser *p)
{
    struct flavor *h = parser_priv(p);
    struct flavor *f = mem_zalloc(sizeof(*f));
    const char *attr;
    int d_attr;

    f->next = h;

    f->fidx = parser_getuint(p, "index");
    f->tval = flavor_tval;
    f->d_char = flavor_glyph;

    if (parser_hasval(p, "sval"))
        f->sval = lookup_sval(f->tval, parser_getsym(p, "sval"));
    else
        f->sval = SV_UNKNOWN;

    attr = parser_getsym(p, "attr");
    if (strlen(attr) == 1)
        d_attr = color_char_to_attr(attr[0]);
    else
        d_attr = color_text_to_attr(attr);
    if (d_attr < 0) return PARSE_ERROR_INVALID_COLOR;
    f->d_attr = d_attr;

    if (parser_hasval(p, "desc"))
        f->text = string_append(f->text, parser_getstr(p, "desc"));

    parser_setpriv(p, f);

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_flavor_kind(struct parser *p)
{
    int tval = tval_find_idx(parser_getsym(p, "tval"));

    if (tval < 0) return PARSE_ERROR_UNRECOGNISED_TVAL;

    flavor_glyph = parser_getchar(p, "glyph");
    flavor_tval = (unsigned int)tval;

    return PARSE_ERROR_NONE;
}


static struct parser *init_parse_flavor(void)
{
    struct parser *p = parser_new();

    parser_setpriv(p, NULL);

    parser_reg(p, "kind sym tval char glyph", parse_flavor_kind);
    parser_reg(p, "flavor uint index sym attr ?str desc", parse_flavor_flavor);
    parser_reg(p, "fixed uint index sym sval sym attr ?str desc", parse_flavor_flavor);

    return p;
}


static errr run_parse_flavor(struct parser *p)
{
    return parse_file_quit_not_found(p, "flavor");
}


static errr finish_parse_flavor(struct parser *p)
{
    flavors = parser_priv(p);
    parser_destroy(p);
    return 0;
}


static void cleanup_flavor(void)
{
    struct flavor *f, *next;

    f = flavors;
    while (f)
    {
        next = f->next;
        string_free(f->text);
        mem_free(f);
        f = next;
    }
}


static struct file_parser flavor_parser =
{
    "flavor",
    init_parse_flavor,
    run_parse_flavor,
    finish_parse_flavor,
    cleanup_flavor
};


/*
 * Initialize socials
 */


static enum parser_error parse_soc_n(struct parser *p)
{
    struct social *s = mem_zalloc(sizeof(*s));

    s->next = parser_priv(p);
    s->name = string_make(parser_getstr(p, "name"));
    parser_setpriv(p, s);

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_soc_i(struct parser *p)
{
    struct social *s = parser_priv(p);

    if (!s) return PARSE_ERROR_MISSING_RECORD_HEADER;

    s->target = parser_getuint(p, "target");
    s->max_dist = parser_getuint(p, "max-dist");

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_soc_d(struct parser *p)
{
    struct social *s = parser_priv(p);

    if (!s) return PARSE_ERROR_MISSING_RECORD_HEADER;

    s->text = string_append(s->text, parser_getstr(p, "desc"));

    return PARSE_ERROR_NONE;
}


static struct parser *init_parse_soc(void)
{
    struct parser *p = parser_new();

    parser_setpriv(p, NULL);
    parser_reg(p, "N str name", parse_soc_n);
    parser_reg(p, "I uint target uint max-dist", parse_soc_i);
    parser_reg(p, "D str desc", parse_soc_d);

    return p;
}


static errr run_parse_soc(struct parser *p)
{
    return parse_file_quit_not_found(p, "socials");
}


static errr finish_parse_soc(struct parser *p)
{
    struct social *s, *n;
    int sidx;

    /* Scan the list for the max id */
    z_info->soc_max = 0;
    s = parser_priv(p);
    while (s)
    {
        z_info->soc_max++;
        s = s->next;
    }

    /* Allocate the direct access list and copy the data to it */
    soc_info = mem_zalloc(z_info->soc_max * sizeof(*s));
    sidx = z_info->soc_max - 1;
    for (s = parser_priv(p); s; s = n, sidx--)
    {
        memcpy(&soc_info[sidx], s, sizeof(*s));
        soc_info[sidx].sidx = sidx;
        n = s->next;
        if (sidx < z_info->soc_max - 1) soc_info[sidx].next = &soc_info[sidx + 1];
        else soc_info[sidx].next = NULL;
        mem_free(s);
    }

    parser_destroy(p);
    return 0;
}


static void cleanup_soc(void)
{
    int i;

    /* Paranoia */
    if (!soc_info) return;

    for (i = 0; i < z_info->soc_max; i++)
    {
        string_free(soc_info[i].name);
        string_free(soc_info[i].text);
    }
    mem_free(soc_info);
    soc_info = NULL;
}


static struct file_parser soc_parser =
{
    "socials",
    init_parse_soc,
    run_parse_soc,
    finish_parse_soc,
    cleanup_soc
};


/*
 * Initialize hints
 */


static enum parser_error parse_hint(struct parser *p)
{
    struct hint *h = parser_priv(p);
    struct hint *n = mem_zalloc(sizeof(*n));

    n->hint = string_make(parser_getstr(p, "text"));
    n->next = h;

    parser_setpriv(p, n);
    return PARSE_ERROR_NONE;
}


static struct parser *init_parse_hints(void)
{
    struct parser *p = parser_new();

    parser_reg(p, "H str text", parse_hint);
    return p;
}


static errr run_parse_hints(struct parser *p)
{
    return parse_file_quit_not_found(p, "hints");
}


static errr finish_parse_hints(struct parser *p)
{
    hints = parser_priv(p);
    parser_destroy(p);
    return 0;
}


static void cleanup_hints(void)
{
    struct hint *h, *next;

    h = hints;
    while (h)
    {
        next = h->next;
        string_free(h->hint);
        mem_free(h);
        h = next;
    }
}


static struct file_parser hints_parser =
{
    "hints",
    init_parse_hints,
    run_parse_hints,
    finish_parse_hints,
    cleanup_hints
};


/*
 * Initialize swearwords
 */


static struct parser *init_parse_swear(void)
{
    struct parser *p = parser_new();

    parser_reg(p, "W str text", parse_hint);
    return p;
}


static errr run_parse_swear(struct parser *p)
{
    return parse_file_quit_not_found(p, "swear");
}


static errr finish_parse_swear(struct parser *p)
{
    swear = parser_priv(p);
    parser_destroy(p);
    return 0;
}


static void cleanup_swear(void)
{
    struct hint *h, *next;

    h = swear;
    while (h)
    {
        next = h->next;
        string_free(h->hint);
        mem_free(h);
        h = next;
    }
}


static struct file_parser swear_parser =
{
    "swear",
    init_parse_swear,
    run_parse_swear,
    finish_parse_swear,
    cleanup_swear
};


/*
 * Initialize level_golds
 */


static enum parser_error parse_level_golds(struct parser *p)
{
    int depth = parser_getint(p, "depth");
    uint16_t rate = parser_getuint(p, "rate");

    if ((depth < 0) || (depth > 127))
        return PARSE_ERROR_INVALID_VALUE;
    if ((rate < 10) || (rate > 100))
        return PARSE_ERROR_INVALID_VALUE;

    level_golds[depth] = rate;

    return PARSE_ERROR_NONE;
}


static struct parser *init_parse_level_golds(void)
{
    struct parser *p = parser_new();

    parser_reg(p, "rate int depth uint rate", parse_level_golds);
    return p;
}


static errr run_parse_level_golds(struct parser *p)
{
    return parse_file_quit_not_found(p, "level_golds");
}


static errr finish_parse_level_golds(struct parser *p)
{
    parser_destroy(p);
    return 0;
}


static void cleanup_level_golds(void)
{
}


static struct file_parser level_golds_parser =
{
    "level_golds",
    init_parse_level_golds,
    run_parse_level_golds,
    finish_parse_level_golds,
    cleanup_level_golds
};


/*
 * Game data initialization
 */


/*
 * A list of all the above parsers, plus those found in mon-init.c and
 * obj-init.c
 */
static struct
{
    const char *name;
    struct file_parser *parser;
} pl[] =
{
    {"projections", &projection_parser},
    {"player properties", &player_property_parser},
    {"features", &feat_parser},
    {"wilderness feats", &wild_feat_parser},
    {"wilderness info", &wild_info_parser},
    {"town feats", &town_feat_parser},
    {"towns", &town_info_parser},
    {"dungeons", &dungeon_info_parser},
    {"object bases", &object_base_parser},
    {"slays", &slay_parser},
    {"brands", &brand_parser},
    {"monster pain messages", &pain_parser},
    {"monster bases", &mon_base_parser},
    {"summons", &summon_parser},
    {"curses", &curse_parser},
    {"activations", &act_parser},
    {"objects", &object_parser},
    {"ego-items", &ego_parser},
    {"history charts", &history_parser},
    {"bodies", &body_parser},
    {"player races", &p_race_parser},
    {"dragon breeds", &dragon_breed_parser},
    {"magic realms", &realm_parser},
    {"player classes", &class_parser},
    {"DM starting items", &dm_start_items_parser},
    {"artifacts", &artifact_parser},
    {"object properties", &object_property_parser},
    {"timed effects", &player_timed_parser},
    {"object power calculations", &object_power_parser},
    {"blow methods", &meth_parser},
    {"blow effects", &eff_parser},
    {"monster spells", &mon_spell_parser},
    {"monsters", &monster_parser},
    {"monster pits", &pit_parser},
    {"traps", &trap_parser},
    {"chest_traps", &chest_trap_parser},
    {"quests", &quests_parser},
    {"flavours", &flavor_parser},
    {"socials", &soc_parser},
    {"hints", &hints_parser},
    {"swear", &swear_parser},
    {"level_golds", &level_golds_parser},
    {"random names", &names_parser},
    {"ui knowledge", &ui_knowledge_parser}
};


/*
 * Initialize just the internal arrays.
 */
static void init_arrays(void)
{
    unsigned i;

    for (i = 0; i < N_ELEMENTS(pl); i++)
    {
        plog_fmt("Initializing %s...", pl[i].name);
        if (run_parser(pl[i].parser)) quit_fmt("Cannot initialize %s.", pl[i].name);
    }
}


/*
 * Free all the internal arrays
 */
static void cleanup_arrays(void)
{
    int i;

    for (i = N_ELEMENTS(pl) - 1; i >= 0; i--)
        cleanup_parser(pl[i].parser);
}


static struct init_module arrays_module =
{
    "arrays",
    init_arrays,
    cleanup_arrays
};


extern struct init_module z_quark_module;
extern struct init_module generate_module;
extern struct init_module rune_module;
extern struct init_module mon_make_module;
extern struct init_module obj_make_module;
extern struct init_module ignore_module;
extern struct init_module store_module;
extern struct init_module ui_visuals_module;


static struct init_module *modules[] =
{
    &z_quark_module,
    &ui_visuals_module, /* This needs to load before monsters and objects. */
    &arrays_module,
    &generate_module,
    &rune_module,
    &mon_make_module,
    &obj_make_module,
    &ignore_module,
    &store_module,
    NULL
};


/*
 * Initialize Angband's data stores and allocate memory for structures,
 * etc, so that the game can get started.
 */
void init_angband(void)
{
    int i;

    init_game_constants();

    /* Initialize modules */
    for (i = 0; modules[i]; i++)
    {
        if (modules[i]->init) modules[i]->init();
    }

    /* Initialize some other things */
    plog("Initializing other stuff...");

    /* Allocate space for houses */
    houses_init();

    /* Initialize the wilderness info */
    init_wild_info();

    /* Prepare chat channels */
    for (i = 0; i < MAX_CHANNELS; i++)
    {
        channels[i].name[0] = '\0';
        channels[i].id = 0;
        channels[i].num = 0;
        channels[i].mode = 0;
        chan_audit = chan_debug = chan_cheat = 0;
    }
    my_strcpy(channels[0].name, DEFAULT_CHANNEL, sizeof(channels[0].name));
    channels[0].mode = CM_SERVICE | CM_PLOG;
    for (i = 1; i < 4; i++)
    {
        channels[i].id = i;
        channels[i].mode = (CM_SECRET | CM_KEYLOCK | CM_SERVICE);
        switch (i)
        {
            case 1:
                chan_audit = i;
                my_strcpy(channels[i].name, "#audit", sizeof(channels[0].name));
                break;
            case 2:
                chan_debug = i;
                my_strcpy(channels[i].name, "#debug", sizeof(channels[0].name));
                break;
            case 3:
                chan_cheat = i;
                my_strcpy(channels[i].name, "#cheat", sizeof(channels[0].name));
                break;
        }
    }

    /* Initialize randart info */
    if (cfg_random_artifacts)
    {
        plog("Initializing randarts...");
        init_randart_generator();
    }

    /* Initialize RNG */
    plog("Initializing RNG...");
    Rand_init();

    /* Done */
    plog("Initialization complete");
}


/*
 * Dirty hack -- unset server options
 */
static void unload_server_cfg(void)
{
    string_free(cfg_meta_address);
    cfg_meta_address = NULL;
    string_free(cfg_bind_name);
    cfg_bind_name = NULL;
    string_free(cfg_report_address);
    cfg_report_address = NULL;
    string_free(cfg_console_password);
    cfg_console_password = NULL;
    string_free(cfg_dungeon_master);
    cfg_dungeon_master = NULL;
    string_free(cfg_load_pref_file);
    cfg_load_pref_file = NULL;
    string_free(cfg_chardump_label);
    cfg_chardump_label = NULL;
}


/*
 * Free all the stuff initialised in init_angband()
 */
void cleanup_angband(void)
{
    int i;
    static uint8_t done = 0;

    /* Don't re-enter */
    if (done) return;
    done++;

    /* Stop the main loop */
    remove_timer_tick();

    /* Free wilderness info */
    free_wild_info();

    /* Free options from mangband.cfg */
    unload_server_cfg();

    /* Misc */
    wipe_player_names();

    /* Free the allocation tables */
    for (i = 0; modules[i]; i++)
    {
        if (modules[i]->cleanup) modules[i]->cleanup();
    }

    cleanup_game_constants();

    /* Free attr/chars used for dumps */
    textui_prefs_free();

    /* Free the houses */
    houses_free();

    /* Free the format() buffer */
    vformat_kill();

    /* Free the directories */
    free_file_paths();

    /* Stop the network server */
    Stop_net_server();
}


static bool str_to_boolean(char * str)
{
    /* false by default */
    return !(my_stricmp(str, "true"));
}


/*
 * Try to set a server option.  This is handled very sloppily right now,
 * since server options are manually mapped to global variables.  Maybe
 * the handeling of this will be unified in the future with some sort of
 * options structure.
 */
static void set_server_option(const char *option, char *value)
{
    /* Due to the lame way that C handles strings, we can't use a switch statement */
    if (streq(option, "REPORT_TO_METASERVER"))
        cfg_report_to_meta = str_to_boolean(value);
    else if (streq(option, "MANGBAND_METASERVER"))
        cfg_mang_meta = str_to_boolean(value);
    else if (streq(option, "META_ADDRESS"))
    {
        string_free(cfg_meta_address);
        cfg_meta_address = string_make(value);
    }
    else if (streq(option, "META_PORT"))
    {
        cfg_meta_port = atoi(value);

        /* We probably ought to do some sanity check here */
        if ((cfg_meta_port > 65535) || (cfg_meta_port < 1))
            cfg_meta_port = 8800;
    }
    else if (streq(option, "BIND_NAME"))
    {
        string_free(cfg_bind_name);
        cfg_bind_name = string_make(value);
    }
    else if (streq(option, "REPORT_ADDRESS"))
    {
        string_free(cfg_report_address);
        cfg_report_address = string_make(value);
    }
    else if (streq(option, "CONSOLE_PASSWORD"))
    {
        string_free(cfg_console_password);
        cfg_console_password = string_make(value);
    }
    else if (streq(option, "DUNGEON_MASTER_NAME"))
    {
        string_free(cfg_dungeon_master);
        cfg_dungeon_master = string_make(value);
        plog_fmt("Dungeon Master Set as '%s'", cfg_dungeon_master);
    }
    else if (streq(option, "SECRET_DUNGEON_MASTER"))
        cfg_secret_dungeon_master = str_to_boolean(value);
    else if (streq(option, "FPS"))
    {
        cfg_fps = atoi(value);

        /* Reinstall the timer handler to match the new FPS */
        install_timer_tick(run_game_loop, cfg_fps);
    }
    else if (streq(option, "MAX_ACCOUNT_CHARS"))
    {
        cfg_max_account_chars = atoi(value);
        if ((cfg_max_account_chars < 1) || (cfg_max_account_chars > 12))
            cfg_max_account_chars = 12;
    }
    else if (streq(option, "NO_STEAL"))
        cfg_no_steal = str_to_boolean(value);
    else if (streq(option, "NEWBIES_CANNOT_DROP"))
        cfg_newbies_cannot_drop = str_to_boolean(value);
    else if (streq(option, "LEVEL_UNSTATIC_CHANCE"))
        cfg_level_unstatic_chance = atoi(value);
    else if (streq(option, "RETIRE_TIMER"))
        cfg_retire_timer = atoi(value);
    else if (streq(option, "ALLOW_RANDOM_ARTIFACTS"))
        cfg_random_artifacts = str_to_boolean(value);
    else if (streq(option, "MORE_TOWNS"))
        cfg_more_towns = str_to_boolean(value);
    else if (streq(option, "ARTIFACT_DROP_SHALLOW"))
        cfg_artifact_drop_shallow = str_to_boolean(value);
    else if (streq(option, "LIMIT_PLAYER_CONNECTIONS"))
        cfg_limit_player_connections = str_to_boolean(value);
    else if (streq(option, "TCP_PORT"))
    {
        cfg_tcp_port = atoi(value);

        /* We probably ought to do some sanity check here */
        if (cfg_tcp_port & 0x01) /* Odd number */
            cfg_tcp_port++;
        if ((cfg_tcp_port > 65535) || (cfg_tcp_port < 1))
            cfg_tcp_port = 18346;
    }
    else if (streq(option, "QUIT_TIMEOUT"))
    {
        cfg_quit_timeout = atoi(value);

        /* Sanity checks */
        if (cfg_quit_timeout < 0) cfg_quit_timeout = 0;
        if (cfg_quit_timeout > 60) cfg_quit_timeout = 60;
    }
    else if (streq(option, "DISCONNECT_FAINTING"))
        cfg_disconnect_fainting = atoi(value);
    else if (streq(option, "CHARACTER_DUMP_COLOR"))
        cfg_chardump_color = str_to_boolean(value);
    else if (streq(option, "PVP_HOSTILITY"))
    {
        cfg_pvp_hostility = atoi(value);

        /* Sanity checks */
        if (cfg_pvp_hostility < 0) cfg_pvp_hostility = 0;
        if (cfg_pvp_hostility > 4) cfg_pvp_hostility = 4;
    }
    else if (streq(option, "BASE_MONSTERS"))
        cfg_base_monsters = str_to_boolean(value);
    else if (streq(option, "EXTRA_MONSTERS"))
        cfg_extra_monsters = str_to_boolean(value);
    else if (streq(option, "GHOST_DIVING"))
        cfg_ghost_diving = str_to_boolean(value);
    else if (streq(option, "CONSOLE_LOCAL_ONLY"))
        cfg_console_local_only = str_to_boolean(value);
    else if (streq(option, "LOAD_PREF_FILE"))
    {
        string_free(cfg_load_pref_file);
        cfg_load_pref_file = string_make(value);
    }
    else if (streq(option, "CHARDUMP_LABEL"))
    {
        string_free(cfg_chardump_label);
        cfg_chardump_label = string_make(value);
    }
    else if (streq(option, "PRESERVE_ARTIFACTS"))
    {
        cfg_preserve_artifacts = atoi(value);

        /* Sanity checks */
        if (cfg_preserve_artifacts < 0) cfg_preserve_artifacts = 0;
        if (cfg_preserve_artifacts > 4) cfg_preserve_artifacts = 4;
    }
    else if (streq(option, "SAFE_RECHARGE"))
        cfg_safe_recharge = str_to_boolean(value);
    else if (streq(option, "PARTY_SHARELEVEL"))
        cfg_party_sharelevel = atoi(value);
    else if (streq(option, "INSTANCE_CLOSED"))
        cfg_instance_closed = str_to_boolean(value);
    else if (streq(option, "TURN_BASED"))
        cfg_turn_based = str_to_boolean(value);
    else if (streq(option, "LIMITED_ESP"))
        cfg_limited_esp = str_to_boolean(value);
    else if (streq(option, "DOUBLE_PURSE"))
        cfg_double_purse = str_to_boolean(value);
    else if (streq(option, "LEVEL_REQUIREMENT"))
        cfg_level_req = str_to_boolean(value);
    else if (streq(option, "CONSTANT_TIME_FACTOR"))
    {
        cfg_constant_time_factor = atoi(value);

        /* Sanity checks */
        if (cfg_constant_time_factor < 0) cfg_constant_time_factor = 0;
        if (cfg_constant_time_factor > MIN_TIME_SCALE) cfg_constant_time_factor = MIN_TIME_SCALE;
    }
    else if (streq(option, "CLASSIC_EXP_FACTOR"))
        cfg_classic_exp_factor = str_to_boolean(value);
    else if (streq(option, "HOUSE_FLOOR_SIZE"))
    {
        cfg_house_floor_size = atoi(value);

        /* Sanity checks */
        if (cfg_house_floor_size < 1) cfg_house_floor_size = 1;
    }
    else if (streq(option, "LIMIT_STAIRS"))
    {
        cfg_limit_stairs = atoi(value);

        /* Sanity checks */
        if (cfg_limit_stairs < 0) cfg_limit_stairs = 0;
        if (cfg_limit_stairs > 3) cfg_limit_stairs = 3;
    }
    else if (streq(option, "DIVING_MODE"))
    {
        cfg_diving_mode = atoi(value);

        /* Sanity checks */
        if (cfg_diving_mode < 0) cfg_diving_mode = 0;
        if (cfg_diving_mode > 3) cfg_diving_mode = 3;
    }
    else if (streq(option, "NO_ARTIFACTS"))
        cfg_no_artifacts = str_to_boolean(value);
    else if (streq(option, "LEVEL_FEELINGS"))
    {
        cfg_level_feelings = atoi(value);

        /* Sanity checks */
        if (cfg_level_feelings < 0) cfg_level_feelings = 0;
        if (cfg_level_feelings > 3) cfg_level_feelings = 3;
    }
    else if (streq(option, "LIMITED_STORES"))
    {
        cfg_limited_stores = atoi(value);

        /* Sanity checks */
        if (cfg_limited_stores < 0) cfg_limited_stores = 0;
        if (cfg_limited_stores > 3) cfg_limited_stores = 3;
    }
    else if (streq(option, "GOLD_DROP_VANILLA"))
        cfg_gold_drop_vanilla = str_to_boolean(value);
    else if (streq(option, "NO_GHOST"))
        cfg_no_ghost = str_to_boolean(value);
    else if (streq(option, "AI_LEARN"))
        cfg_ai_learn = str_to_boolean(value);
    else if (streq(option, "CHALLENGING_LEVELS"))
        cfg_challenging_levels = str_to_boolean(value);
    else plog_fmt("Error : unrecognized mangband.cfg option %s", option);
}


/*
 * Parse the loaded mangband.cfg file, and if a valid expression was found
 * try to set it using set_server_option.
 */
static void load_server_cfg_aux(ang_file *cfg)
{
    char line[256];
    char *newword;
    char *option;
    char *value;
    bool first_token;

    /* Read in lines until we hit EOF */
    while (file_getl(cfg, line, sizeof(line)))
    {
        /* Parse the line that has been read in */
        /* If the line begins with a # or is empty, ignore it */
        if ((line[0] == '#') || (line[0] == '\0')) continue;

        /* Reset option and value */
        option = NULL;
        value = NULL;

        /*
         * Split the line up into words
         * We pass the string to be parsed to strsep on the first call,
         * and subsequently pass it null.
         */
        first_token = 1;
        newword = strtok(first_token ? line : NULL, " ");
        while (newword)
        {
            first_token = 0;

            /* Set the option or value */
            if (!option) option = newword;
            else if ((!value) && (newword[0] != '='))
            {
                value = newword;

                /* Ignore "" around values */
                if (value[0] == '"') value++;
                if (value[strlen(value) - 1] == '"')
                    value[strlen(value) - 1] = '\0';
            }

            /* If we have a completed option and value, then try to set it */
            if (option && value)
            {
                set_server_option(option, value);
                break;
            }

            newword = strtok(first_token? line: NULL, " ");
        }
    }
}


/*
 * Load in the mangband.cfg file.  This is a file that holds many
 * options that have historically been #defined in config.h.
 */
void load_server_cfg(void)
{
    ang_file *cfg;

    /* Attempt to open the file */
    cfg = file_open("mangband.cfg", MODE_READ, FTYPE_TEXT);

    /* Failure */
    if (!cfg)
    {
        plog("Error : cannot open file mangband.cfg");
        return;
    }

    /* Default */
    cfg_fps = 12;

    /* Actually parse the file */
    load_server_cfg_aux(cfg);

    /* Close it */
    file_close(cfg);
}
