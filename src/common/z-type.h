/*
 * File: z-type.h
 * Purpose: Helper classes for the display of typed data
 */

#ifndef INCLUDED_ZTYPE_H
#define INCLUDED_ZTYPE_H

/* Defines a rectangle on the screen that is bound to a Panel or subpanel */
typedef struct
{
    int col;        /* x-coordinate of upper right corner */
    int row;        /* y-coordinate of upper right corner */
    int width;      /* width of display area. 1 - use system default. */
                    /* non-positive - rel to right of screen */
    int page_rows;  /* non-positive value is relative to the bottom of the screen */
} region;

struct loc
{
    int x;
    int y;
};

struct loc_iterator
{
    struct loc begin;
    struct loc end;
    struct loc cur;
};

extern bool loc_is_zero(struct loc *grid);
extern void loc_init(struct loc *grid, int x, int y);
extern void loc_copy(struct loc *dest, struct loc *src);
extern bool loc_eq(struct loc *grid1, struct loc *grid2);
extern void loc_sum(struct loc *sum, struct loc *grid1, struct loc *grid2);
extern void loc_diff(struct loc *sum, struct loc *grid1, struct loc *grid2);
extern void rand_loc(struct loc *rand, struct loc *grid, int x_spread, int y_spread);
extern void loc_iterator_first(struct loc_iterator *iter, struct loc *begin, struct loc *end);
extern bool loc_iterator_next(struct loc_iterator *iter);
extern bool loc_iterator_next_strict(struct loc_iterator *iter);
extern bool loc_between(struct loc *grid, struct loc *grid1, struct loc *grid2);

struct cmp_loc
{
    struct loc grid;
    void *data;
};

/* Coordinates on the world map */
struct worldpos
{
    struct loc grid;    /* The wilderness coordinates */
    int16_t depth;         /* Cur depth */
    struct worldpos *next;
};

extern bool wpos_null(struct worldpos *wpos);
extern void wpos_init(struct worldpos *wpos, struct loc *grid, int depth);
extern bool wpos_eq(struct worldpos *wpos1, struct worldpos *wpos2);

/*
 * Defines a (value, name) pairing. Variable names used are historical.
 */
typedef struct
{
	uint16_t tval;
	const char *name;
} grouper;

/*
 * A set of points that can be constructed to apply a set of changes to
 */
struct point_set
{
    int n;
    int allocated;
    struct cmp_loc *pts;
};

extern struct point_set *point_set_new(int initial_size);
extern void point_set_dispose(struct point_set *ps);
extern void add_to_point_set(struct point_set *ps, void *data, struct loc *grid);
extern int point_set_size(struct point_set *ps);
extern int point_set_contains(struct point_set *ps, struct loc *grid);

/**** MAngband specific ****/

/*
 * Buffers for ".txt" text files
 */
#define MAX_TEXTFILES   3
#define TEXTFILE__WID   140
#define TEXTFILE__HGT   23
#define TEXTFILE_MOTD   0
#define TEXTFILE_TOMB   1
#define TEXTFILE_CRWN   2

/* The setup data that the server transmits to the client */
typedef struct
{
    int16_t frames_per_second;
    uint8_t min_col;
    uint8_t max_col;
    uint8_t min_row;
    uint8_t max_row;
    bool initialized;
    bool ready;

    /* Static arrays to hold text screen loaded from TEXTFILEs */
    char text_screen[MAX_TEXTFILES][TEXTFILE__WID * TEXTFILE__HGT];
} server_setup_t;


/* The setting data that the client transmits to the server */
enum
{
    SETTING_USE_GRAPHICS = 0,
    SETTING_SCREEN_COLS,
    SETTING_SCREEN_ROWS,
    SETTING_TILE_WID,
    SETTING_TILE_HGT,
    SETTING_TILE_DISTORTED,
    SETTING_MAX_HGT,
    SETTING_WINDOW_FLAG,
    SETTING_HITPOINT_WARN,

    SETTING_MAX
};

enum grid_light_level
{
    LIGHTING_LOS = 0,  /* line of sight */
    LIGHTING_TORCH,    /* torchlight */
    LIGHTING_LIT,      /* permanently lit (when not in line of sight) */
    LIGHTING_DARK,     /* dark */

    LIGHTING_MAX
};

typedef uint8_t byte_lit[LIGHTING_MAX];
typedef char char_lit[LIGHTING_MAX];

typedef char char_note[4];

/* The setup data that the client transmits to the server */
typedef struct
{
    int16_t settings[SETTING_MAX];
    uint8_t *flvr_x_attr;
    char *flvr_x_char;
    uint8_t (*f_attr)[LIGHTING_MAX];
    char (*f_char)[LIGHTING_MAX];
    uint8_t (*t_attr)[LIGHTING_MAX];
    char (*t_char)[LIGHTING_MAX];
    uint8_t *k_attr;
    char *k_char;
    uint8_t *r_attr;
    char *r_char;
    uint8_t proj_attr[PROJ_MAX][BOLT_MAX];
    char proj_char[PROJ_MAX][BOLT_MAX];
    char (*note_aware)[4];
} client_setup_t;

extern client_setup_t Client_setup;

/*
 * Maximum channel name length
 */
#define MAX_CHAN_LEN 12

/* Information about a "chat channel" */
typedef struct
{
    char name[MAX_CHAN_LEN];
    int32_t id;
    int32_t num;
    uint8_t mode;
} channel_type;

/*
 * Maximum number of channels.
 */
#define MAX_CHANNELS 255

extern channel_type channels[MAX_CHANNELS];

/* A structure to hold misc information on spells */
typedef struct
{
    int flag;       /* Actual spell flag */
    uint8_t line_attr; /* "Color" of the spell (learned, worked, forgotten) */
    uint8_t dir_attr;  /* Directional info */
    uint8_t proj_attr; /* Can be projected */
    int smana;      /* Mana cost */
    int page;       /* Spell page */
} spell_flags;

/*
 * Information about maximal indices of certain arrays.
 */
struct angband_constants
{
    /* Array bounds, set on parsing edit files */
    uint16_t f_max;                 /* Maximum number of terrain features */
    uint16_t trap_max;              /* Maximum number of trap kinds */
    uint16_t k_max;                 /* Maximum number of object base kinds */
    uint16_t a_max;                 /* Maximum number of artifact kinds */
    uint16_t e_max;                 /* Maximum number of ego item kinds */
    uint16_t r_max;                 /* Maximum number of monster races */
    uint16_t mp_max;                /* Maximum number of monster pain message sets */
    uint16_t s_max;                 /* Maximum number of magic spells */
    uint16_t pit_max;               /* Maximum number of monster pit types */
    uint16_t act_max;               /* Maximum number of activations */
    uint16_t curse_max;             /* Maximum number of curses */
    uint16_t slay_max;              /* Maximum number of slays */
    uint16_t brand_max;             /* Maximum number of brands */
    uint16_t mon_blows_max;         /* Maximum number of monster blows */
    uint16_t blow_methods_max;      /* Maximum number of monster blow methods */
    uint16_t blow_effects_max;      /* Maximum number of monster blow effects */
    uint16_t equip_slots_max;       /* Maximum number of player equipment slots */
    uint16_t profile_max;           /* Maximum number of cave_profiles */
    uint16_t quest_max;             /* Maximum number of quests */
    uint16_t projection_max;        /* Maximum number of projection types */
    uint16_t calculation_max;       /* Maximum number of object power calculations */
    uint16_t property_max;          /* Maximum number of object properties */
    uint16_t summon_max;            /* Maximum number of summon types */
    uint16_t soc_max;               /* Maximum number of socials */
    uint16_t wf_max;                /* Maximum number of wilderness terrain features */
    uint16_t tf_max;                /* Maximum number of town terrain features */
    uint16_t town_max;              /* Maximum number of towns */
    uint16_t dungeon_max;           /* Maximum number of dungeons */

    /* Maxima of things on a given level, read from constants.txt */
    uint16_t level_monster_max;     /* Maximum number of monsters on a given level */

    /* Monster generation constants, read from constants.txt */
    uint16_t alloc_monster_chance;  /* 1/per-turn-chance of generation */
    uint16_t level_monster_min;     /* Minimum number generated */
    uint16_t town_monsters_day;     /* Townsfolk generated - day */
    uint16_t town_monsters_night;   /* Townsfolk generated  - night */
    uint16_t repro_monster_max;     /* Maximum breeders on a level */
    uint16_t ood_monster_chance;    /* Chance of OoD monster is 1 in this */
    uint16_t ood_monster_amount;    /* Max number of levels OoD */
    uint16_t monster_group_max;     /* Maximum size of a group */
    uint16_t monster_group_dist;    /* Max dist of a group from a related group */

    /* Monster gameplay constants, read from constants.txt */
    uint16_t glyph_hardness;        /* How hard for a monster to break a glyph */
    uint16_t repro_monster_rate;    /* Monster reproduction rate-slower */
    uint16_t life_drain_percent;    /* Percent of player life drained */
    uint16_t flee_range;            /* Monsters run this many grids out of view */
    uint16_t turn_range;            /* Monsters turn to fight closer than this */

    /* Dungeon generation constants, read from constants.txt */
    uint16_t level_room_max;        /* Maximum number of rooms on a level */
    uint16_t level_door_max;        /* Maximum number of potential doors on a level */
    uint16_t wall_pierce_max;       /* Maximum number of potential wall piercings */
    uint16_t tunn_grid_max;         /* Maximum number of tunnel grids */
    uint16_t room_item_av;          /* Average number of items in rooms */
    uint16_t both_item_av;          /* Average number of items in random places */
    uint16_t both_gold_av;          /* Average number of money items */
    uint16_t level_pit_max;         /* Maximum number of pits on a level */
    uint16_t lab_depth_lit;         /* Depth where labyrinths start to be generated unlit */
    uint16_t lab_depth_known;       /* Depth where labyrinths start to be generated unknown */
    uint16_t lab_depth_soft;        /* Depth where labyrinths start to be generated with hard walls */

    /* World shape constants, read from constants.txt */
    uint16_t max_depth;             /* Maximum dungeon level */
    uint16_t day_length;            /* Number of turns from dawn to dawn */
    uint16_t dungeon_hgt;           /* Maximum number of vertical grids on a level */
    uint16_t dungeon_wid;           /* Maximum number of horizontal grids on a level */
    uint16_t town_hgt;              /* Number of features in the starting town (vertically) */
    uint16_t town_wid;              /* Number of features in the starting town (horizontally) */
    uint16_t feeling_total;         /* Total number of feeling squares per level */
    uint16_t feeling_need;          /* Squares needed to see to get first feeling */
    uint16_t stair_skip;            /* Number of levels to skip for each down stair */
    uint16_t move_energy;           /* Energy the player or monster needs to move */

    /* Carrying capacity constants, read from constants.txt */
    uint16_t pack_size;             /* Maximum number of pack slots */
    uint16_t quiver_size;           /* Maximum number of quiver slots */
    uint16_t quiver_slot_size;      /* Maximum number of missiles per quiver slot */
    uint16_t thrown_quiver_mult;    /* Size multiplier for non-ammo in quiver */
    uint16_t floor_size;            /* Maximum number of items per floor grid */

    /* Store parameters, read from constants.txt */
    uint16_t store_inven_max;       /* Maximum number of objects in store inventory */
    uint16_t home_inven_max;        /* Maximum number of objects in home inventory */
    uint16_t store_turns;           /* Number of turns between turnovers */
    uint16_t store_shuffle;         /* 1/per-day-chance of owner changing */
    uint16_t store_magic_level;     /* Level for apply_magic() in normal stores */

    /* Object creation constants, read from constants.txt */
    uint16_t max_obj_depth;         /* Maximum depth used in object allocation */
    uint16_t good_obj;              /* Chance of object being "good" */
    uint16_t ego_obj;               /* Chance of object being "great" */
    uint16_t great_obj;             /* 1/chance of inflating the requested object level */
    uint16_t great_ego;             /* 1/chance of inflating the requested ego item level */
    uint16_t fuel_torch;            /* Maximum amount of fuel in a torch */
    uint16_t fuel_lamp;             /* Maximum amount of fuel in a lamp */
    uint16_t default_lamp;          /* Default amount of fuel in a lamp */

    /* Player constants, read from constants.txt */
    uint16_t max_sight;             /* Maximum visual range */
    uint16_t max_range;             /* Maximum missile and spell range */
    uint16_t start_gold;            /* Amount of gold the player starts with */
    uint16_t food_value;            /* Number of turns 1% of food lasts */
};

extern struct angband_constants *z_info;

extern const char *ANGBAND_SYS;
extern const char *ANGBAND_FONTNAME;

extern char *ANGBAND_DIR_GAMEDATA;
extern char *ANGBAND_DIR_CUSTOMIZE;
extern char *ANGBAND_DIR_HELP;
extern char *ANGBAND_DIR_SCREENS;
extern char *ANGBAND_DIR_FONTS;
extern char *ANGBAND_DIR_TILES;
extern char *ANGBAND_DIR_SOUNDS;
extern char *ANGBAND_DIR_MUSIC;
extern char *ANGBAND_DIR_ICONS;
extern char *ANGBAND_DIR_USER;
extern char *ANGBAND_DIR_SAVE;
extern char *ANGBAND_DIR_PANIC;
extern char *ANGBAND_DIR_SCORES;

/*
 * Socials
 */
struct social
{
    char *name;         /* Name */
    char *text;         /* Text */
    unsigned int sidx;  /* Index */
    struct social *next;
    uint8_t target;        /* Target type (target/no target) */
    uint8_t max_dist;      /* Max distance of target allowed */
};

extern struct social *soc_info;

#endif /* INCLUDED_ZTYPE_H */
