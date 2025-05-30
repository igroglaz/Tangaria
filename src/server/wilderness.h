/*
 * File: wilderness.h
 * Purpose: Wilderness interface
 */

#ifndef WILDERNESS_H
#define WILDERNESS_H

/*
 * Used for wilderness generation
 */
#define DIR_NORTH   0
#define DIR_EAST    1
#define DIR_SOUTH   2
#define DIR_WEST    3

/*
 * Max number of wilderness terrain features per wilderness sector
 */
#define TERRAIN_TYPE_MAX    30

/*
 * Chance of generating this terrain feature on a given wilderness sector
 */
struct wf_chance
{
    int feat;
    int chance;
};

/*
 * Information about wilderness terrain features.
 */
struct wild_feat
{
    char *name;                                 /* Name */
    int feat_lvl;                               /* Level of wilderness terrain feature */
    int feat_idx;                               /* Index of wilderness terrain feature */
    char symbol;                                /* Symbol */
    struct wf_chance chance[TERRAIN_TYPE_MAX];  /* Chance of generating this terrain feature */
    int sound_idx;                              /* Index of ambient sound (MSG_XXX) */
    struct wild_feat *next;
};

extern struct wild_feat *wf_info;
extern struct file_parser wild_feat_parser;

/*
 * Information about wilderness.
 */
struct wilderness
{
    char *text;     /* Text */
};

extern struct wilderness *wild_info;
extern struct file_parser wild_info_parser;

/*
 * Information about town terrain features.
 */
struct town_feat
{
    char symbol;            /* Symbol */
    char special;           /* Special symbol */
    int feat_idx;           /* Index of wilderness terrain feature */
    struct town_feat *next;
};

extern struct town_feat *tf_info;
extern struct file_parser town_feat_parser;

/*
 * Dungeon flags
 */
enum
{
    #define DF(a, b) DF_##a,
    #include "list-dungeon-flags.h"
    #undef DF
    DF_MAX
};

#define DF_SIZE                 FLAG_SIZE(DF_MAX)

#define df_has(f, flag) flag_has_dbg(f, DF_SIZE, flag, #f, #flag)

/*
 * Dungeon rule
 */
struct dun_rule
{
    int chance;                     /* 1/10000 chance of monsters to obey this rule */
    uint8_t all;                    /* Rule applies to all monsters */
    bitflag flags[RF_SIZE];         /* Flags */
	bitflag spell_flags[RSF_SIZE];  /* Spell flags */
    char sym[5];                    /* Symbols */
    struct dun_rule *next;
};

/*
 * Dungeon feature
 */
struct dun_feature
{
    int feat;               /* Feature (floor, wall, permanent wall, closed door, stairs down) */
    int feat2;              /* Feature (open door, stairs up, passable rubble) */
    int feat3;              /* Feature (broken door) */
    int chance;             /* 1/10000 chance of floors/walls/doors replaced by that feature */
    struct dun_feature *next;
};

/*
 * A location (town or dungeon)
 */
struct location
{
    char *name;                     /* Name */
    char *shortname;                /* Short name */
    struct worldpos wpos;           /* Position on the world map */
    int min_depth;                  /* Min depth */
    int max_depth;                  /* Max depth */
    int max_level;                  /* Max character level allowed */
    int max_townies;                /* Max townies */
    int feat;                       /* Symbol on minimap */
    bitflag flags[DF_SIZE];         /* Flags */
    struct dun_feature *floors;     /* Unusual dungeon floors */
    int n_floors;
    struct dun_feature *walls;      /* Unusual dungeon walls */
    int n_walls;
    struct dun_feature *fills;      /* Unusual dungeon filler walls */
    int n_fills;
    struct dun_feature *permas;     /* Unusual dungeon permawalls */
    int n_permas;
    struct dun_feature *doors;      /* Unusual dungeon doors */
    int n_doors;
    struct dun_feature *stairs;     /* Unusual dungeon stairs */
    int n_stairs;
    struct dun_feature *rubbles;    /* Unusual dungeon rubbles */
    int n_rubbles;
    struct dun_feature *fountains;  /* Unusual dungeon fountains */
    int n_fountains;
    struct dun_rule *rules;         /* Rules */
    int n_rules;
    random_value up;
    random_value down;
    struct location *next;
};

/*
 * Information about towns.
 */
extern struct location *towns;
extern struct file_parser town_info_parser;

/*
 * Information about dungeons.
 */
extern struct location *dungeons;
extern struct file_parser dungeon_info_parser;

extern uint16_t radius_wild;
extern uint32_t seed_wild;

/*
 * Information about "Arena" (special building for PvP)
 */
struct arena_type
{
    struct loc grid_1;
    struct loc grid_2;
    struct worldpos wpos;
    int player1;
    int player2;
};

/*
 * Total number of arenas
 */
#define MAX_ARENAS  10

extern struct arena_type arenas[MAX_ARENAS];
extern uint16_t num_arenas;

/*
 * Different types of terrain, used for the wilderness.
 */
enum
{
    #define WILD(type) WILD_##type,
    #include "list-wild-feats.h"
    #undef WILD

    WILD_MAX
};

/*
 * Different buildings
 */
#define WILD_LOG_CABIN      0
#define WILD_TOWN_HOME      1
#define WILD_ARENA          2

/*
 * Types of crops
 */
#define WILD_CROP_POTATO    0
#define WILD_CROP_CABBAGE   1
#define WILD_CROP_CARROT    2
#define WILD_CROP_BEET      3
#define WILD_CROP_SQUASH    4
#define WILD_CROP_CORN      5
#define WILD_CROP_MUSHROOM  6

enum wild_gen
{
    WILD_NONE = 0,  /* Not generated */
    WILD_GENERATED, /* Generated without dwellings */
    WILD_FURNISHED, /* Generated with furnished dwellings */
    WILD_DESERTED   /* Generated with empty dwellings */
};

/*
 * Information about wilderness levels
 */
struct wild_type
{
    struct worldpos wpos;       /* Position on the world map (transient) */
    int min_depth;              /* Min depth (transient) */
    int max_depth;              /* Max depth (transient) */

    struct chunk **chunk_list;  /* List of pointers to saved chunks */
    int16_t *players_on_depth;     /* How many players are at each depth */

    int type;                   /* What kind of terrain we are in (transient) */
    int distance;               /* Distance from towns (transient) */
    enum wild_gen generated;    /* Level is generated */
};

extern struct wild_type *get_wt_info_at(struct loc *grid);
extern int monster_level(struct worldpos *wpos);
extern int object_level(struct worldpos *wpos);
extern bool surface_of_dungeon(struct worldpos *wpos);
extern struct worldpos *start_wpos(void);
extern bool in_start_town(struct worldpos *wpos);
extern struct worldpos *base_wpos(void);
extern bool in_base_town(struct worldpos *wpos);
extern struct worldpos *zeitnot_wpos(void);
extern bool in_zeitnot_town(struct worldpos *wpos);
extern struct worldpos *deeptown_wpos(void);
extern bool in_deeptown_town(struct worldpos *wpos);
extern struct location *get_town(struct worldpos *wpos);
extern bool in_town(struct worldpos *wpos);
extern bool in_wild(struct worldpos *wpos);
extern bool town_suburb(struct worldpos *wpos);
extern bool town_area(struct worldpos *wpos);
extern struct worldpos *restrict_locations(const char *locations);
extern struct location *get_dungeon(struct worldpos *wpos);
extern void dungeon_list(struct player *p, ang_file *fff);
extern void init_wild_info(void);
extern void free_wild_info(void);
extern void wild_cat_depth(struct worldpos *wpos, char *buf, int len);
extern bool wild_is_explored(struct player *p, struct worldpos *wpos);
extern void wild_set_explored(struct player *p, struct worldpos *wpos);
extern void wild_deserted_message(struct player *p);
extern void wild_add_monster(struct player *p, struct chunk *c);
extern void wild_grow_crops(struct chunk *c, struct loc *grid1, struct loc *grid2, bool regen);
extern void do_cmd_plant_seed(struct player *p, struct object *obj);
extern struct wild_type *get_neighbor(struct wild_type *origin, char dir);
extern int world_index(struct worldpos *wpos);
extern int house_price(int area, bool town);
extern void get_town_file(char *buf, size_t len, const char *name);
extern bool customize_feature(struct chunk *c, struct loc *grid, struct dun_feature *dun_feats,
    int size, bool (*test)(struct chunk *, struct loc *),
    bool (*post_test)(struct chunk *, struct loc *, int), int *feat);

extern struct chunk *wilderness_gen(struct player *p, struct worldpos *wpos, int min_height,
    int min_width, const char **p_error);
extern void wilderness_gen_basic_layout(struct chunk *c);

#endif /* WILDERNESS_H */
