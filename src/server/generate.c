/*
 * File: generate.c
 * Purpose: Dungeon generation
 *
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 * Copyright (c) 2013 Erik Osheim, Nick McConnell
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
#include <math.h>


/*
 * This is the top level dungeon generation file, which contains room profiles
 * (for determining what rooms are available and their parameters), cave
 * profiles (for determining the level generation function and parameters for
 * different styles of levels), initialisation functions for template rooms and
 * vaults, and the main level generation function (which calls the level
 * builders from gen-cave.c).
 *
 * See the "vault.txt" file for more on vault generation.
 * See the "room_template.txt" file for more room templates.
 */


struct pit_profile *pit_info;
struct vault *vaults;
static struct cave_profile *cave_profiles;


/*
 * This is the global structure representing dungeon generation info.
 */
struct dun_data *dun;
struct room_template *room_templates;


static const struct
{
    const char *name;
    cave_builder builder;
} cave_builders[] =
{
    #define DUN(a, b) {a, b##_gen},
    #include "list-dun-profiles.h"
    #undef DUN
    {NULL, NULL}
};


static const struct
{
    const char *name;
    int max_height;
    int max_width;
    room_builder builder;
} room_builders[] =
{
    #define ROOM(a, b, c, d) {a, b, c, build_##d},
    #include "list-rooms.h"
    #undef ROOM
    {NULL, 0, 0, NULL}
};


static const char *room_flags[] =
{
    #define ROOMF(a, b) #a,
    #include "list-room-flags.h"
    #undef ROOMF
    NULL
};


/*
 * Parsing functions for dungeon_profile.txt
 */
static enum parser_error parse_profile_name(struct parser *p)
{
    struct cave_profile *h = parser_priv(p);
    struct cave_profile *c = mem_zalloc(sizeof(*c));
    size_t i;

    c->name = string_make(parser_getstr(p, "name"));
    for (i = 0; cave_builders[i].name; i++)
    {
        if (streq(c->name, cave_builders[i].name))
            break;
    }
    if (!cave_builders[i].name) return PARSE_ERROR_NO_BUILDER_FOUND;
    c->builder = cave_builders[i].builder;
    c->next = h;
    parser_setpriv(p, c);

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_profile_params(struct parser *p)
{
    struct cave_profile *c = parser_priv(p);

    if (!c) return PARSE_ERROR_MISSING_RECORD_HEADER;
    c->block_size = parser_getint(p, "block");
    c->dun_rooms = parser_getint(p, "rooms");
    c->dun_unusual = parser_getint(p, "unusual");
    c->max_rarity = parser_getint(p, "rarity");

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_profile_tunnel(struct parser *p)
{
    struct cave_profile *c = parser_priv(p);

    if (!c) return PARSE_ERROR_MISSING_RECORD_HEADER;
    c->tun.rnd = parser_getint(p, "rnd");
    c->tun.chg = parser_getint(p, "chg");
    c->tun.con = parser_getint(p, "con");
    c->tun.pen = parser_getint(p, "pen");
    c->tun.jct = parser_getint(p, "jct");

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_profile_streamer(struct parser *p)
{
    struct cave_profile *c = parser_priv(p);

    if (!c) return PARSE_ERROR_MISSING_RECORD_HEADER;
    c->str.den = parser_getint(p, "den");
    c->str.rng = parser_getint(p, "rng");
    c->str.mag = parser_getint(p, "mag");
    c->str.mc  = parser_getint(p, "mc");
    c->str.qua = parser_getint(p, "qua");
    c->str.qc  = parser_getint(p, "qc");

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_profile_stairs_up(struct parser *p)
{
    struct cave_profile *c = parser_priv(p);
    dice_t *dice;

    if (!c) return PARSE_ERROR_MISSING_RECORD_HEADER;
    dice = dice_new();
    if (!dice_parse_string(dice, parser_getstr(p, "up")))
    {
        dice_free(dice);
        return PARSE_ERROR_NOT_RANDOM;
    }
    dice_random_value(dice, NULL, &c->up);
    dice_free(dice);

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_profile_stairs_down(struct parser *p)
{
    struct cave_profile *c = parser_priv(p);
    dice_t *dice;

    if (!c) return PARSE_ERROR_MISSING_RECORD_HEADER;
    dice = dice_new();
    if (!dice_parse_string(dice, parser_getstr(p, "down")))
    {
        dice_free(dice);
        return PARSE_ERROR_NOT_RANDOM;
    }
    dice_random_value(dice, NULL, &c->down);
    dice_free(dice);

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_profile_room(struct parser *p)
{
    struct cave_profile *c = parser_priv(p);
    struct room_profile *r = c->room_profiles;
    size_t i;

    if (!c) return PARSE_ERROR_MISSING_RECORD_HEADER;

    /* Go to the last valid room profile, then allocate a new one */
    if (!r)
    {
        c->room_profiles = mem_zalloc(sizeof(struct room_profile));
        r = c->room_profiles;
    }
    else
    {
        while (r->next) r = r->next;
        r->next = mem_zalloc(sizeof(struct room_profile));
        r = r->next;
    }

    /* Now read the data */
    r->name = string_make(parser_getsym(p, "name"));
    for (i = 0; room_builders[i].name; i++)
    {
        if (streq(r->name, room_builders[i].name))
            break;
    }
    if (!room_builders[i].name) return PARSE_ERROR_NO_ROOM_FOUND;
    r->builder = room_builders[i].builder;
    r->rating = parser_getint(p, "rating");
    r->height = parser_getint(p, "height");
    r->width = parser_getint(p, "width");
    r->level = parser_getint(p, "level");
    r->pit = (parser_getint(p, "pit") == 1);
    r->rarity = parser_getint(p, "rarity");
    r->cutoff = parser_getint(p, "cutoff");

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_profile_min_level(struct parser *p)
{
    struct cave_profile *c = parser_priv(p);

    if (!c) return PARSE_ERROR_MISSING_RECORD_HEADER;
    c->min_level = parser_getint(p, "min");

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_profile_alloc(struct parser *p)
{
    struct cave_profile *c = parser_priv(p);

    if (!c) return PARSE_ERROR_MISSING_RECORD_HEADER;
    c->alloc = parser_getint(p, "alloc");

    return PARSE_ERROR_NONE;
}


static struct parser *init_parse_profile(void)
{
    struct parser *p = parser_new();

    parser_setpriv(p, NULL);
    parser_reg(p, "name str name", parse_profile_name);
    parser_reg(p, "params int block int rooms int unusual int rarity", parse_profile_params);
    parser_reg(p, "tunnel int rnd int chg int con int pen int jct", parse_profile_tunnel);
    parser_reg(p, "streamer int den int rng int mag int mc int qua int qc", parse_profile_streamer);
    parser_reg(p, "up str up", parse_profile_stairs_up);
    parser_reg(p, "down str down", parse_profile_stairs_down);
    parser_reg(p,
        "room sym name int rating int height int width int level int pit int rarity int cutoff",
        parse_profile_room);
    parser_reg(p, "alloc int alloc", parse_profile_alloc);
    parser_reg(p, "min-level int min", parse_profile_min_level);

    return p;
}


static errr run_parse_profile(struct parser *p)
{
    return parse_file_quit_not_found(p, "dungeon_profile");
}


static errr finish_parse_profile(struct parser *p)
{
    struct cave_profile *n, *c = parser_priv(p);
    int i, num;

    /* Scan the list */
    z_info->profile_max = 0;
    while (c)
    {
        struct room_profile *r = c->room_profiles;

        /* Count the room profiles */
        c->n_room_profiles = 0;
        while (r)
        {
            c->n_room_profiles++;
            r = r->next;
        }

        z_info->profile_max++;
        c = c->next;
    }

    /* Allocate the array and copy the records to it */
    cave_profiles = mem_zalloc(z_info->profile_max * sizeof(*c));
    num = z_info->profile_max - 1;
    for (c = parser_priv(p); c; c = n)
    {
        /* Main record */
        memcpy(&cave_profiles[num], c, sizeof(*c));
        n = c->next;
        if (num < z_info->profile_max - 1) cave_profiles[num].next = &cave_profiles[num + 1];
        else cave_profiles[num].next = NULL;

        /* Now allocate the room profile array */
        if (c->room_profiles)
        {
            struct room_profile *r, *rn = NULL;

            cave_profiles[num].room_profiles = mem_zalloc(c->n_room_profiles *
                sizeof(struct room_profile));
            for (i = 0, r = c->room_profiles; r; i++, r = rn)
            {
                memcpy(&cave_profiles[num].room_profiles[i], r, sizeof(*r));
                cave_profiles[num].room_profiles[i].next = NULL;
                rn = r->next;
                mem_free(r);
            }
        }

        mem_free(c);
        num--;
    }

    parser_destroy(p);
    return 0;
}


static void cleanup_profile(void)
{
    int i, j;

    /* Paranoia */
    if (!cave_profiles) return;

    for (i = 0; i < z_info->profile_max; i++)
    {
        for (j = 0; j < cave_profiles[i].n_room_profiles; j++)
            string_free(cave_profiles[i].room_profiles[j].name);
        mem_free(cave_profiles[i].room_profiles);
        string_free(cave_profiles[i].name);
    }
    mem_free(cave_profiles);
    cave_profiles = NULL;
}


static struct file_parser profile_parser =
{
    "dungeon_profile",
    init_parse_profile,
    run_parse_profile,
    finish_parse_profile,
    cleanup_profile
};


/*
 * Parsing functions for room_template.txt
 */
static enum parser_error parse_room_name(struct parser *p)
{
    struct room_template *h = parser_priv(p);
    struct room_template *t = mem_zalloc(sizeof(*t));

    t->name = string_make(parser_getstr(p, "name"));
    t->next = h;
    parser_setpriv(p, t);

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_room_type(struct parser *p)
{
    struct room_template *t = parser_priv(p);

    if (!t) return PARSE_ERROR_MISSING_RECORD_HEADER;
    t->typ = parser_getuint(p, "type");

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_room_rating(struct parser *p)
{
    struct room_template *t = parser_priv(p);

    if (!t) return PARSE_ERROR_MISSING_RECORD_HEADER;
    t->rat = parser_getint(p, "rating");

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_room_height(struct parser *p)
{
    struct room_template *t = parser_priv(p);
    size_t i;

    if (!t) return PARSE_ERROR_MISSING_RECORD_HEADER;
    t->hgt = parser_getuint(p, "height");

    /* Make sure rooms are no higher than the room profiles allow. */
    for (i = 0; i < N_ELEMENTS(room_builders); i++)
    {
        if (streq("room template", room_builders[i].name)) break;
    }
    if (i == N_ELEMENTS(room_builders)) return PARSE_ERROR_NO_ROOM_FOUND;
    if (t->hgt > room_builders[i].max_height)
        return PARSE_ERROR_VAULT_TOO_BIG;

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_room_width(struct parser *p)
{
    struct room_template *t = parser_priv(p);
    size_t i;

    if (!t) return PARSE_ERROR_MISSING_RECORD_HEADER;
    t->wid = parser_getuint(p, "width");

    /* Make sure rooms are no wider than the room profiles allow. */
    for (i = 0; i < N_ELEMENTS(room_builders); i++)
    {
        if (streq("room template", room_builders[i].name)) break;
    }
    if (i == N_ELEMENTS(room_builders)) return PARSE_ERROR_NO_ROOM_FOUND;
    if (t->wid > room_builders[i].max_width)
        return PARSE_ERROR_VAULT_TOO_BIG;

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_room_doors(struct parser *p)
{
    struct room_template *t = parser_priv(p);

    if (!t) return PARSE_ERROR_MISSING_RECORD_HEADER;
    t->dor = parser_getuint(p, "doors");

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_room_tval(struct parser *p)
{
    struct room_template *t = parser_priv(p);
    int tval;

    if (!t) return PARSE_ERROR_MISSING_RECORD_HEADER;
    tval = tval_find_idx(parser_getsym(p, "tval"));
    if (tval < 0) return PARSE_ERROR_UNRECOGNISED_TVAL;
    t->tval = tval;

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_room_flags(struct parser *p)
{
    struct room_template *t = parser_priv(p);
    char *s, *st;

    if (!t) return PARSE_ERROR_MISSING_RECORD_HEADER;
    s = string_make(parser_getstr(p, "flags"));
    st = strtok(s, " |");
    while (st && !grab_flag(t->flags, ROOMF_SIZE, room_flags, st))
        st = strtok(NULL, " |");
    mem_free(s);

    return (st? PARSE_ERROR_INVALID_FLAG: PARSE_ERROR_NONE);
}


static enum parser_error parse_room_d(struct parser *p)
{
    struct room_template *t = parser_priv(p);

    if (!t) return PARSE_ERROR_MISSING_RECORD_HEADER;
    t->text = string_append(t->text, parser_getstr(p, "text"));

    return PARSE_ERROR_NONE;
}


static struct parser *init_parse_room(void)
{
    struct parser *p = parser_new();

    parser_setpriv(p, NULL);
    parser_reg(p, "name str name", parse_room_name);
    parser_reg(p, "type uint type", parse_room_type);
    parser_reg(p, "rating int rating", parse_room_rating);
    parser_reg(p, "rows uint height", parse_room_height);
    parser_reg(p, "columns uint width", parse_room_width);
    parser_reg(p, "doors uint doors", parse_room_doors);
    parser_reg(p, "tval sym tval", parse_room_tval);
    parser_reg(p, "flags str flags", parse_room_flags);
    parser_reg(p, "D str text", parse_room_d);

    return p;
}


static errr run_parse_room(struct parser *p)
{
    return parse_file_quit_not_found(p, "room_template");
}


static errr finish_parse_room(struct parser *p)
{
    room_templates = parser_priv(p);
    parser_destroy(p);
    return 0;
}


static void cleanup_room(void)
{
    struct room_template *t, *next;

    for (t = room_templates; t; t = next)
    {
        next = t->next;
        string_free(t->name);
        string_free(t->text);
        mem_free(t);
    }
}


static struct file_parser room_parser =
{
    "room_template",
    init_parse_room,
    run_parse_room,
    finish_parse_room,
    cleanup_room
};


/*
 * Parsing functions for vault.txt
 */
static enum parser_error parse_vault_name(struct parser *p)
{
    struct vault *h = parser_priv(p);
    struct vault *v = mem_zalloc(sizeof(*v));

    v->name = string_make(parser_getstr(p, "name"));
    v->next = h;
    parser_setpriv(p, v);

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_vault_type(struct parser *p)
{
    struct vault *v = parser_priv(p);

    if (!v) return PARSE_ERROR_MISSING_RECORD_HEADER;
    v->typ = string_make(parser_getstr(p, "type"));
    return PARSE_ERROR_NONE;
}


static enum parser_error parse_vault_rating(struct parser *p)
{
    struct vault *v = parser_priv(p);

    if (!v) return PARSE_ERROR_MISSING_RECORD_HEADER;
    v->rat = parser_getint(p, "rating");
    return PARSE_ERROR_NONE;
}


static enum parser_error parse_vault_rows(struct parser *p)
{
    struct vault *v = parser_priv(p);
    size_t i;

    if (!v) return PARSE_ERROR_MISSING_RECORD_HEADER;
    v->hgt = parser_getuint(p, "height");

    /* Make sure vaults are no bigger than the room profiles allow. */
    for (i = 0; i < N_ELEMENTS(room_builders); i++)
    {
        if (streq(v->typ, room_builders[i].name)) break;
    }
    if (i == N_ELEMENTS(room_builders)) return PARSE_ERROR_NO_ROOM_FOUND;
    if (v->hgt > room_builders[i].max_height)
        return PARSE_ERROR_VAULT_TOO_BIG;

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_vault_columns(struct parser *p)
{
    struct vault *v = parser_priv(p);
    size_t i;

    if (!v) return PARSE_ERROR_MISSING_RECORD_HEADER;
    v->wid = parser_getuint(p, "width");

    /* Make sure vaults are no bigger than the room profiles allow. */
    for (i = 0; i < N_ELEMENTS(room_builders); i++)
    {
        if (streq(v->typ, room_builders[i].name)) break;
    }
    if (i == N_ELEMENTS(room_builders)) return PARSE_ERROR_NO_ROOM_FOUND;
    if (v->wid > room_builders[i].max_width)
        return PARSE_ERROR_VAULT_TOO_BIG;

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_vault_min_depth(struct parser *p)
{
    struct vault *v = parser_priv(p);

    if (!v) return PARSE_ERROR_MISSING_RECORD_HEADER;
    v->min_lev = parser_getuint(p, "min_lev");
    return PARSE_ERROR_NONE;
}


static enum parser_error parse_vault_max_depth(struct parser *p)
{
    struct vault *v = parser_priv(p);
    int max_lev;

    if (!v) return PARSE_ERROR_MISSING_RECORD_HEADER;
    max_lev = parser_getuint(p, "max_lev");
    v->max_lev = (max_lev? max_lev: z_info->max_depth);
    return PARSE_ERROR_NONE;
}


static enum parser_error parse_vault_flags(struct parser *p)
{
    struct vault *v = parser_priv(p);
    char *s, *st;

    if (!v) return PARSE_ERROR_MISSING_RECORD_HEADER;
    s = string_make(parser_getstr(p, "flags"));
    st = strtok(s, " |");
    while (st && !grab_flag(v->flags, ROOMF_SIZE, room_flags, st))
        st = strtok(NULL, " |");
    mem_free(s);

    return (st? PARSE_ERROR_INVALID_FLAG: PARSE_ERROR_NONE);
}


static enum parser_error parse_vault_d(struct parser *p)
{
    struct vault *v = parser_priv(p);
    const char *desc;

    if (!v) return PARSE_ERROR_MISSING_RECORD_HEADER;
    desc = parser_getstr(p, "text");
    if (strlen(desc) != (size_t)v->wid) return PARSE_ERROR_VAULT_DESC_WRONG_LENGTH;
    v->text = string_append(v->text, desc);

    return PARSE_ERROR_NONE;
}


static struct parser *init_parse_vault(void)
{
    struct parser *p = parser_new();

    parser_setpriv(p, NULL);
    parser_reg(p, "name str name", parse_vault_name);
    parser_reg(p, "type str type", parse_vault_type);
    parser_reg(p, "rating int rating", parse_vault_rating);
    parser_reg(p, "rows uint height", parse_vault_rows);
    parser_reg(p, "columns uint width", parse_vault_columns);
    parser_reg(p, "min-depth uint min_lev", parse_vault_min_depth);
    parser_reg(p, "max-depth uint max_lev", parse_vault_max_depth);
    parser_reg(p, "flags str flags", parse_vault_flags);
    parser_reg(p, "D str text", parse_vault_d);

    return p;
}


static errr run_parse_vault(struct parser *p)
{
    return parse_file_quit_not_found(p, "vault");
}


static errr finish_parse_vault(struct parser *p)
{
    vaults = parser_priv(p);
    parser_destroy(p);
    return 0;
}


static void cleanup_vault(void)
{
    struct vault *v, *next;

    for (v = vaults; v; v = next)
    {
        next = v->next;
        string_free(v->name);
        string_free(v->text);
        string_free(v->typ);
        mem_free(v);
    }
}


static struct file_parser vault_parser =
{
    "vault",
    init_parse_vault,
    run_parse_vault,
    finish_parse_vault,
    cleanup_vault
};


static void run_template_parser(void)
{
    /* Initialize dungeon profile info */
    plog("Initializing dungeon profiles...");
    if (run_parser(&profile_parser)) quit("Cannot initialize dungeon profiles.");

    /* Initialize room template info */
    plog("Initializing room templates...");
    if (run_parser(&room_parser)) quit("Cannot initialize room templates.");

    /* Initialize vault info */
    plog("Initializing vaults...");
    if (run_parser(&vault_parser)) quit("Cannot initialize vaults.");
}


/*
 * Free the template arrays
 */
static void cleanup_template_parser(void)
{
    cleanup_parser(&profile_parser);
    cleanup_parser(&room_parser);
    cleanup_parser(&vault_parser);
}


/*
 * Place hidden squares that will be used to generate feeling
 *
 * p is the player
 * c is the cave struct the feeling squares are being placed in
 */
static void place_feeling(struct player *p, struct chunk *c)
{
    int i, j;
    int tries = 500;

    for (i = 0; i < z_info->feeling_total; i++)
    {
        for (j = 0; j < tries; j++)
        {
            struct loc grid;

            /* Pick a random dungeon coordinate */
            loc_init(&grid, randint0(c->width), randint0(c->height));

            /* Check to see if it can be used as a feeling square */
            if (!square_allowsfeel(c, &grid)) continue;

            /* Check to see if it is already marked */
            if (square_isfeel(c, &grid)) continue;

            /* Set the cave square appropriately */
            sqinfo_on(square(c, &grid)->info, SQUARE_FEEL);
            if (p) sqinfo_on(square_p(p, &grid)->info, SQUARE_FEEL);

            break;
        }
    }
}


/*
 * Calculate the level feeling for objects.
 *
 * c is the cave where the feeling is being measured
 */
static int calc_obj_feeling(struct chunk *c)
{
    uint32_t x;

    /* Non-random level gets no feeling */
    if (!random_level(&c->wpos)) return 0;

    /* Artifacts trigger a special feeling when preserve=no */
    if (c->good_item && !cfg_preserve_artifacts) return 10;

    /* Check the loot adjusted for depth */
    x = c->obj_rating / c->wpos.depth;

    /* Apply a minimum feeling if there's an artifact on the level */
    if (c->good_item && (x < 641)) return 60;

    if (x > 160000) return 20;
    if (x > 40000) return 30;
    if (x > 10000) return 40;
    if (x > 2500) return 50;
    if (x > 640) return 60;
    if (x > 160) return 70;
    if (x > 40) return 80;
    if (x > 10) return 90;
    return 100;
}


/*
 * Calculate the level feeling for monsters.
 *
 * c is the cave where the feeling is being measured
 */
static int calc_mon_feeling(struct chunk *c)
{
    uint32_t x;

    /* Non-random level gets no feeling */
    if (!random_level(&c->wpos)) return 0;

    /* Check the monster power adjusted for depth */
    x = c->mon_rating / c->wpos.depth;

    if (x > 7000) return 1;
    if (x > 4500) return 2;
    if (x > 2500) return 3;
    if (x > 1500) return 4;
    if (x > 800) return 5;
    if (x > 400) return 6;
    if (x > 150) return 7;
    if (x > 50) return 8;
    return 9;
}


/*
 * Find a cave_profile by name
 *
 * name is the name of the cave_profile being looked for
 */
static const struct cave_profile *find_cave_profile(char *name)
{
    int i;

    for (i = 0; i < z_info->profile_max; i++)
    {
        const struct cave_profile *profile;

        profile = &cave_profiles[i];
        if (streq(name, profile->name))
            return profile;
    }

    /* Not there */
    return NULL;
}


/*
 * Do prime check for labyrinths
 *
 * depth is the depth where we're trying to generate a labyrinth
 */
static bool labyrinth_check(struct worldpos *wpos)
{
    /* There's a base 2 in 100 to accept the labyrinth */
    int chance = 2;

    struct worldpos dpos;
    struct location *dungeon;

    /* Get the dungeon */
    wpos_init(&dpos, &wpos->grid, 0);
    dungeon = get_dungeon(&dpos);

    /* Some dungeons are real mazes */
    if (dungeon && wpos->depth && df_has(dungeon->flags, DF_MAZE))
        return true;

    /* If we're too shallow then don't do it */
    if (wpos->depth < 13) return false;

    /* Certain numbers increase the chance of having a labyrinth */
    if (wpos->depth % 3 == 0) chance += 1;
    if (wpos->depth % 5 == 0) chance += 1;
    if (wpos->depth % 7 == 0) chance += 1;
    if (wpos->depth % 11 == 0) chance += 1;
    if (wpos->depth % 13 == 0) chance += 1;

    /* Only generate the level if we pass a check */
    if (randint0(100) >= chance) return false;

    /* Successfully ran the gauntlet! */
    return true;
}


/*
 * Do prime check for caverns
 *
 * wpos is the location where we're trying to generate a cavern
 */
static bool cavern_check(struct worldpos *wpos)
{
    struct worldpos dpos;
    struct location *dungeon;

    /* Get the dungeon */
    wpos_init(&dpos, &wpos->grid, 0);
    dungeon = get_dungeon(&dpos);

    /* Some dungeons have an extra chance at generating caverns */
    if (dungeon && wpos->depth && df_has(dungeon->flags, DF_CAVERN) && one_in_(20))
        return true;

    return false;
}


/*
 * Do prime check for arenas
 *
 * wpos is the location where we're trying to generate an arena
 */
static bool arena_check(struct worldpos *wpos)
{
    struct worldpos dpos;
    struct location *dungeon;

    /* Get the dungeon */
    wpos_init(&dpos, &wpos->grid, 0);
    dungeon = get_dungeon(&dpos);

    /* Some dungeons have a chance at generating arenas */
    if (dungeon && wpos->depth && df_has(dungeon->flags, DF_EMPTY) && one_in_(15))
        return true;

    return false;
}


/*
 * Choose a cave profile
 *
 * wpos is the coordinates of the cave the profile will be used to generate
 */
static const struct cave_profile *choose_profile(struct worldpos *wpos)
{
    const struct cave_profile *profile = NULL;
    int moria_alloc = find_cave_profile("moria")->alloc;
    int labyrinth_alloc = find_cave_profile("labyrinth")->alloc;

    /* Make the profile choice */
    if (wpos->depth > 0)
    {
        if (dynamic_town(wpos))
            profile = find_cave_profile("town");
        else if (is_quest(wpos->depth) || !random_level(wpos))
            profile = find_cave_profile("classic");
        else if (arena_check(wpos))
            profile = find_cave_profile("arena");
        else if (cavern_check(wpos))
            profile = find_cave_profile("cavern");
        else if (labyrinth_check(wpos) && (labyrinth_alloc > 0 || labyrinth_alloc == -1))
            profile = find_cave_profile("labyrinth");
        else if ((wpos->depth >= 10) && (wpos->depth < 40) && one_in_(40) &&
            (moria_alloc > 0 || moria_alloc == -1))
        {
            profile = find_cave_profile("moria");
        }
        // t_modified
        else if (wpos->depth < 10 && one_in_(2))
            profile = find_cave_profile("t_modified");
        else if (wpos->depth > 10 && one_in_(1 + (wpos->depth / 10)))
            profile = find_cave_profile("t_modified");
        // "modified" and others from dungeon_profile.txt
        else
        {
            int total_alloc = 0;
            int i;

            for (i = 0; i < z_info->profile_max; i++)
            {
                struct cave_profile *test_profile = &cave_profiles[i];

                if (test_profile->alloc <= 0 || wpos->depth < test_profile->min_level) continue;
                total_alloc += test_profile->alloc;
                if (randint0(total_alloc) < test_profile->alloc) profile = test_profile;
            }

            if (!profile) profile = find_cave_profile("classic");
        }
    }
    else if (in_base_town(wpos))
        profile = find_cave_profile("town");
    else if (in_town(wpos))
    {
        /* Get town info */
        struct location *town = get_town(wpos);

        char town_file[20];
        char path[MSG_LEN];

        /* Get town file */
        get_town_file(town_file, sizeof(town_file), town->shortname);
        path_build(path, sizeof(path), ANGBAND_DIR_GAMEDATA, format("%s.txt", town_file));

        /*
         * If there's a town file, use the profile inside to create the town.
         * Otherwise, create a default MAngband-style town.
         */
        if (file_exists(path))
            profile = find_cave_profile("wilderness");
        else
            profile = find_cave_profile("mang_town");
    }
    else
        profile = find_cave_profile("wilderness");

    /* Add message showing which profile was selected */
    if (profile)
        plog_fmt("Dungeon profile: ___ %s ___ @ %d depth)", 
                profile->name, wpos->depth);
    else
        plog_fmt("choose_profile(): Failed to select profile (depth: %d, location: %d,%d)",
                wpos->depth, wpos->grid.x, wpos->grid.y);

    /* Return the profile or fail horribly */
    if (profile) return profile;

    quit("Failed to find cave profile!");
    return NULL;
}


/*
 * Mark artifacts as "generated" when the dungeon is ready.
 *
 * This is called during cave generation, so that unpreserved artifacts are lost when
 * leaving real levels, but not when abandoning levels through errors in generation.
 */
static void set_artifacts_generated(struct player *p, struct chunk *c)
{
    struct loc begin, end;
    struct loc_iterator iter;
    struct object *obj;

    loc_init(&begin, 0, 0);
    loc_init(&end, c->width, c->height);
    loc_iterator_first(&iter, &begin, &end);

    do
    {
        for (obj = square_object(c, &iter.cur); obj; obj = obj->next)
        {
            uint8_t *pinfo;

            /* Skip non artifacts */
            if (!obj->artifact) continue;

            /* True artifacts */
            if (true_artifact_p(obj)) pinfo = p->art_info;
            else pinfo = p->randart_info;

            /* Preserve artifacts from dungeon generation errors */
            if (pinfo[obj->artifact->aidx] >= ARTS_CREATED)
            {
                pinfo[obj->artifact->aidx] -= ARTS_CREATED;

                /* Mark the artifact as "generated" if dungeon is ready */
                if (!ht_zero(&c->generated) && !pinfo[obj->artifact->aidx])
                    set_artifact_info(p, obj, ARTS_GENERATED);
            }
        }
    }
    while (loc_iterator_next_strict(&iter));
}


/*
 * Clear the dungeon, ready for generation to begin.
 */
static void cave_clear(struct player *p, struct chunk *c)
{
    /* Clear the monsters */
    wipe_mon_list(c);

    /* Deal with artifacts */
    if (p) set_artifacts_generated(p, c);

    /* Free the chunk */
    cave_free(c);
}


/*
 * Release the dynamically allocated resources in a dun_data structure.
 */
static void cleanup_dun_data(struct dun_data *dd)
{
    int i;

    mem_free(dun->cent);
    dun->cent = NULL;
    mem_free(dun->ent_n);
    dun->ent_n = NULL;
    for (i = 0; i < z_info->level_room_max; ++i) mem_free(dun->ent[i]);
    mem_free(dun->ent);
    dun->ent = NULL;
    if (dun->ent2room)
    {
        for (i = 0; dun->ent2room[i]; ++i) mem_free(dun->ent2room[i]);
        mem_free(dun->ent2room);
        dun->ent2room = NULL;
    }
    mem_free(dun->door);
    dun->door = NULL;
    mem_free(dun->wall);
    dun->wall = NULL;
    mem_free(dun->tunn);
    dun->tunn = NULL;
    mem_free(dun->tunn_flag);
    dun->tunn_flag = NULL;
}


/*
 * Wipe the dungeon.
 *
 * Same as cave_clear, but for real levels (all unpreserved artifacts are lost).
 */
void cave_wipe(struct chunk *c)
{
    int i;
    struct loc begin, end;
    struct loc_iterator iter;

    /* Clear the monsters */
    wipe_mon_list(c);

    loc_init(&begin, 0, 0);
    loc_init(&end, c->width, c->height);
    loc_iterator_first(&iter, &begin, &end);

    /* Deal with artifacts */
    do
    {
        struct object *obj = square_object(c, &iter.cur);

        /* Preserve unseen artifacts */
        while (obj)
        {
            preserve_artifact(obj);
            obj = obj->next;
        }
    }
    while (loc_iterator_next_strict(&iter));

    /* Cancel tracking for all players */
    for (i = 1; i <= NumPlayers; i++)
    {
        struct player *p = player_get(i);

        if (p->upkeep && p->upkeep->object && !object_is_carried(p, p->upkeep->object))
            track_object(p->upkeep, NULL);
    }

    /* Free the chunk */
    cave_free(c);
}


bool allow_location(struct monster_race *race, struct worldpos *wpos)
{
    if ((cfg_diving_mode < 2) && race->locations)
    {
        bool found = false;
        struct worldpos *location = race->locations;

        while (location && !found)
        {
            if (loc_eq(&location->grid, &wpos->grid)) found = true;
            else location = location->next;
        }

        if (!found) return false;
    }

    return true;
}


/*
 * Generate a random level.
 *
 * Confusingly, this function also generates the surface levels (wilderness and towns).
 *
 * p is the current player struct
 * wpos is the location where we're trying to generate a level
 *
 * Return a pointer to the new level
 */
static struct chunk *cave_generate(struct player *p, struct worldpos *wpos, int height, int width)
{
    const char *error = "no generation";
    int tries = 0, i;
    struct chunk *chunk = NULL;

    /* Generate */
    for (tries = 0; (tries < 100) && error; tries++)
    {
        struct loc begin, end;
        struct loc_iterator iter;
        struct dun_data dun_body;

        error = NULL;

        /* Allocate global data (will be freed when we leave the loop) */
        dun = &dun_body;
        dun->cent = mem_zalloc(z_info->level_room_max * sizeof(struct loc));
        dun->ent_n = mem_zalloc(z_info->level_room_max * sizeof(*dun->ent_n));
        dun->ent = mem_zalloc(z_info->level_room_max * sizeof(*dun->ent));
        dun->ent2room = NULL;
        dun->door = mem_zalloc(z_info->level_door_max * sizeof(struct loc));
        dun->wall = mem_zalloc(z_info->wall_pierce_max * sizeof(struct loc));
        dun->tunn = mem_zalloc(z_info->tunn_grid_max * sizeof(struct loc));
        dun->tunn_flag = mem_zalloc(z_info->tunn_grid_max * sizeof(uint8_t));
        dun->quest = is_quest(wpos->depth);

        /* Choose a profile and build the level */
        dun->profile = choose_profile(wpos);
        chunk = dun->profile->builder(p, wpos, height, width, &error);
        if (!chunk)
        {
            if (!error) error = "unspecified level builder failure";
            plog_fmt("Generation restarted: %s.", error);
            cleanup_dun_data(dun);
            continue;
        }

        /* Ensure quest monsters and fixed encounters (wilderness) */
        // dungeon boss spawning (if not killed yet)
        if (p)
        {
            for (i = 1; i < z_info->r_max; i++)
            {
                struct monster_race *race = &r_info[i];
                bool quest_monster = (is_quest_active(p, chunk->wpos.depth) &&
                    rf_has(race->flags, RF_QUESTOR));
                bool fixed_encounter = (rf_has(race->flags, RF_PWMANG_FIXED) &&
                    (cfg_diving_mode < 2));
                struct monster_group_info info = {0, 0};
                struct loc grid;
                bool found = false;
                int tries = 50;

                /* The monster must be an unseen quest monster/fixed encounter of this depth. */
                if (race->lore.spawned) continue;
                if (!quest_monster && !fixed_encounter) continue;
                if (race->level != chunk->wpos.depth) continue;
                if (!allow_location(race, &chunk->wpos)) continue;

                /* Pick a location and place the monster */
                while (tries-- && !found)
                {
                    if (rf_has(race->flags, RF_AQUATIC)) found = find_emptywater(chunk, &grid);
                    else if (rf_has(race->flags, RF_NO_DEATH)) found = find_training(chunk, &grid);
                    else found = (find_empty(chunk, &grid) && square_is_monster_walkable(chunk, &grid));
                }
                if (found)
                {
                    // 1) custom messages-sounds
                    struct monster_lore *lore = get_lore(p, race);
                    if (!lore->pkills) // only if not killed this boss yet
                    {
                        msgt(p, MSG_BROADCAST_DIED, "This place belongs to someone...");

                        if (p->wpos.depth == 5)
                            sound(p, MSG_AMBIENT_VOICE); // hi from Yaga
                        else if (p->wpos.depth == 12)
                            sound(p, MSG_ORC_CAVES); // hi from Solovei
                        else if (p->wpos.depth == 19)
                            sound(p, MSG_KIKIMORA); // hi from Kikimora
                        else if (p->wpos.depth == 27)
                            sound(p, MSG_MANOR); // hi from Koschei
                        else if (p->wpos.depth == 32 && rf_has(race->flags, RF_FEMALE)) // Sandworm Queen
                        {
                            msgt(p, MSG_BROADCAST_LEVEL, "Ecch.. You feel poisonous smell there!");
                            msgt(p, MSG_BROADCAST_LEVEL, "You may want to ensure that you've got poison resistance...");
                        }
                        else if (p->wpos.depth == 36)
                            sound(p, MSG_ENTER_BARROW); // hi from Wight-King
                    }

                    // 2) ok, place now.
                    place_new_monster(p, chunk, &grid, race, MON_ASLEEP | MON_GROUP, &info,
                        ORIGIN_DROP);
                }
                else
                    plog_fmt("Unable to place monster of race %s", race->name);
            }
        }

        loc_init(&begin, 0, 0);
        loc_init(&end, chunk->width, chunk->height);
        loc_iterator_first(&iter, &begin, &end);

        /* Clear generation flags. */
        do
        {
            sqinfo_off(square(chunk, &iter.cur)->info, SQUARE_WALL_INNER);
            sqinfo_off(square(chunk, &iter.cur)->info, SQUARE_WALL_OUTER);
            sqinfo_off(square(chunk, &iter.cur)->info, SQUARE_WALL_SOLID);
            sqinfo_off(square(chunk, &iter.cur)->info, SQUARE_MON_RESTRICT);
        }
        while (loc_iterator_next_strict(&iter));

        /* Regenerate levels that overflow their maxima */
        if (cave_monster_max(chunk) >= z_info->level_monster_max) error = "too many monsters";

        if (error)
        {
            plog_fmt("Generation restarted: %s.", error);
            uncreate_artifacts(chunk);
            cave_clear(p, chunk);
            chunk = NULL;
        }

        cleanup_dun_data(dun);
    }

    if (error) quit("cave_generate() failed 100 times!");

    if (random_level(&chunk->wpos))
    {
        plog_fmt("New Level %dft at (%d, %d) Ratings obj:%lu/mon:%lu", 
            chunk->wpos.depth * 50,
            chunk->wpos.grid.x, chunk->wpos.grid.y, 
            chunk->obj_rating / chunk->wpos.depth,
            chunk->mon_rating / chunk->wpos.depth);
    }
    else
    {
        plog_fmt("New Level %dft at (%d, %d)", 
            chunk->wpos.depth * 50, 
            chunk->wpos.grid.x, chunk->wpos.grid.y);
    }

    /* Place dungeon squares to trigger feeling (not on the surface) */
    if (chunk->wpos.depth > 0) place_feeling(p, chunk);

    /* Get a feeling */
    if (p) p->feeling = calc_obj_feeling(chunk) + calc_mon_feeling(chunk);

    /* Validate the dungeon (we could use more checks here) */
    chunk_validate_objects(chunk);

    /* Allocate new known level, light it if requested */
    chunk_list_add(chunk);
    if (p && chunk->light_level) wiz_light(p, chunk, 0);

    /* Mark artifacts as "generated" */
    if (p) set_artifacts_generated(p, chunk);

    return chunk;
}


static void check_level_size(struct worldpos *wpos, int n, int *min_height, int *min_width)
{
    struct worldpos check;

    wpos_init(&check, &wpos->grid, n);
    if (random_level(&check))
    {
        struct chunk *c = chunk_get(&check);

        if (c)
        {
            *min_height = MAX(*min_height, c->height);
            *min_width = MAX(*min_width, c->width);
        }
    }
}


/*
 * Prepare the level the player is about to enter
 */
struct chunk *prepare_next_level(struct player *p)
{
    struct worldpos *wpos = &p->wpos;
    int min_height = 0, min_width = 0;
    struct chunk *c;

    /* Determine level size requirements (only for random levels) */
    if (random_level(wpos))
    {
        struct wild_type *w_ptr = get_wt_info_at(&wpos->grid);
        int n;

        /* Check level above (must be a valid random level) */
        /* We simply get the first level n where dungeon_get_next_level(n, 1) = wpos->depth */
        for (n = wpos->depth - 1; n >= w_ptr->min_depth; n--)
        {
            if (dungeon_get_next_level(p, n, 1) == wpos->depth)
            {
                check_level_size(wpos, n, &min_height, &min_width);
                break;
            }
        }

        /* Check level below (must be a valid random level) */
        /* We simply get the first level n where dungeon_get_next_level(n, -1) = wpos->depth */
        for (n = wpos->depth + 1; n < w_ptr->max_depth; n++)
        {
            if (dungeon_get_next_level(p, n, -1) == wpos->depth)
            {
                check_level_size(wpos, n, &min_height, &min_width);
                break;
            }
        }
    }

    /* Generate a new level */
    c = cave_generate(p, wpos, min_height, min_width);

    /* The dungeon is ready */
    ht_copy(&c->generated, &turn);

    return c;
}


void player_place_feeling(struct player *p, struct chunk *c)
{
    struct loc begin, end;
    struct loc_iterator iter;

    loc_init(&begin, 0, 0);
    loc_init(&end, c->width, c->height);
    loc_iterator_first(&iter, &begin, &end);

    do
    {
        if (square_isfeel(c, &iter.cur))
            sqinfo_on(square_p(p, &iter.cur)->info, SQUARE_FEEL);
    }
    while (loc_iterator_next_strict(&iter));
}


/*
 * The generate module, which initialises template rooms and vaults
 */
struct init_module generate_module =
{
    "generate",
    run_template_parser,
    cleanup_template_parser
};

