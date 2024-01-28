/*
 * File: player-common.h
 * Purpose: Player interface
 */

#ifndef INCLUDED_PLAYER_COMMON_H
#define INCLUDED_PLAYER_COMMON_H

/* Maximum number of spells per page
   Note: this must be greater than the maximum number of spells in all books (currently 9)
   must be +1 from real number, eg in PWMA:
   MAX_SPELLS_PER_PAGE 10
   but actual max spells in-game are 9)
 */
#define MAX_SPELLS_PER_PAGE 25

/*
 * Player constants
 */
#define PY_MAX_EXP      99999999L   /* Maximum exp */
#define PY_MAX_GOLD     999999999L  /* Maximum gold */
#define PY_MAX_LEVEL    50          /* Maximum level */

/** Sexes **/

/*
 * Maximum number of player "sex" types (see "table.c", etc)
 */
#define MAX_SEXES   3

/*
 * Player sex constants (hard-coded by save-files, arrays, etc)
 */
#define SEX_MALE  0
#define SEX_FEMALE    1
#define SEX_NEUTER  2

/*
 * Timed effects
 */
enum
{
    #define TMD(a, b, c) TMD_##a,
    #include "list-player-timed.h"
    #undef TMD
    TMD_MAX
};

/*
 * List of resistances and abilities to display
 */
#define RES_PANELS  4
#define RES_ROWS    13

/*
 * Number of history flags
 */
#define N_HISTORY_FLAGS (1 + STAT_MAX + (RES_PANELS + 3) * RES_ROWS)

/*
 * Special values for the number of turns to rest, these need to be
 * negative numbers, as postive numbers are taken to be a turncount,
 * and zero means "not resting".
 */
enum
{
    REST_COMPLETE = -2,
    REST_ALL_POINTS = -1,
    REST_SOME_POINTS = -3,
    REST_MORNING = -4,
    REST_COMPLETE_NODISTURB = -5
};

/*
 * Maximum number of messages to keep in player message history
 */
#define MAX_MSG_HIST 60

/*
 * Maximum number of players playing at once.
 *
 * The number of connections is limited by the number of bases
 * and the max number of possible file descriptors to use in
 * the select(2) call minus those for stdin, stdout, stderr,
 * the contact socket, and the socket for the resolver library routines.
 *
 * This limit has never been stretched, and it would be interesting to see
 * what happens when 100 or so players play at once.
 */
#define MAX_PLAYERS 1018

/*
 * Maximum number of lines in 'special info'
 */
#define MAX_TXT_INFO    384

/* Constants for character history */
#define N_HIST_LINES    3
#define N_HIST_WRAP     73

/* Character rolling methods */
enum birth_rollers
{
    BR_DEFAULT = -3,
    BR_QDYNA = -2,
    BR_QUICK = -1,
    BR_POINTBASED = 0,
    BR_NORMAL,
    MAX_BIRTH_ROLLERS
};

/* Warlocks can turn into an undead being */
#define player_can_undead(P) \
    (player_has((P), PF_UNDEAD_POWERS) && ((P)->state.stat_use[STAT_INT] >= 18+70))

/* History message types */
enum
{
    #define HIST(a, b) HIST_##a,
    #include "list-history-types.h"
    #undef HIST

    HIST_MAX
};

#define HIST_SIZE                FLAG_SIZE(HIST_MAX)

#define hist_has(f, flag)        flag_has_dbg(f, HIST_SIZE, flag, #f, #flag)
#define hist_is_empty(f)         flag_is_empty(f, HIST_SIZE)
#define hist_on(f, flag)         flag_on_dbg(f, HIST_SIZE, flag, #f, #flag)
#define hist_off(f, flag)        flag_off(f, HIST_SIZE, flag)
#define hist_wipe(f)             flag_wipe(f, HIST_SIZE)
#define hist_copy(f1, f2)        flag_copy(f1, f2, HIST_SIZE)

struct timed_grade
{
    int grade;
    uint8_t color;
    int max;
    char *name;
    char *up_msg;
    char *down_msg;
    struct timed_grade *next;
};

/*
 * Player structures
 */

/*
 * Structure for the "quests"
 */
struct quest
{
    struct quest *next;
    uint8_t index;
    char *name;
    uint8_t level;              /* Dungeon level */
    struct monster_race *race;  /* Monster race */
    int16_t cur_num;            /* Number killed */
    int16_t max_num;            /* Number required */
    int16_t timer;              /* Time left before quest is over */
};

/*
 * A single equipment slot
 */
struct equip_slot
{
    struct equip_slot *next;

    int16_t type;
    char *name;
    struct object *obj;
};

/*
 * A player 'body'
 */
struct player_body
{
    struct player_body *next;
    char *name;
    int16_t count;
    struct equip_slot *slots;
};

struct brand_info
{
    bool brand;
    uint8_t minlvl;
    uint8_t maxlvl;
};

struct slay_info
{
    bool slay;
    uint8_t minlvl;
    uint8_t maxlvl;
};

struct modifier
{
    random_value value;
    uint8_t lvl;
};

struct player_shape
{
    struct player_shape *next;
    char *name;
    uint8_t lvl;
};


/* Barehanded attack */
struct barehanded_attack
{
    char *verb;         /* A verbose attack description */
    char *hit_extra;
    int min_level;      /* Minimum level to use */
    int chance;         /* Chance of failure vs player level */
    int effect;         /* Special effects */
    struct barehanded_attack *next;
};

/*
 * Racial gifts the player starts with. Used in player_race and specified in
 * p_race.txt.
 */
struct gift
{
    int tval;                   /* General object type (see TV_ macros) */
    int sval;                   /* Object sub-type */
    int min;                    /* Minimum starting amount */
    int max;                    /* Maximum starting amount */
    struct gift *next;
};

/*
 * Player race info
 */
struct player_race
{
    struct player_race *next;
    char *name;                 /* Name */
    unsigned int ridx;          /* Index */
    uint8_t r_mhp;              /* Hit-dice modifier */
    int16_t r_exp;              /* Experience factor */
    int b_age;                  /* Base age */
    int m_age;                  /* Mod age */
    int base_hgt;               /* Base height */
    int mod_hgt;                /* Mod height */
    int base_wgt;               /* Base weight */
    int mod_wgt;                /* Mod weight */
    int body;                   /* Race body */
    struct modifier modifiers[OBJ_MOD_MAX]; /* Modifiers */
    int16_t r_skills[SKILL_MAX];    /* Skills */
    bitflag flags[OF_SIZE];         /* Racial (object) flags */
    uint8_t flvl[OF_MAX];           /* Application level for racial (object) flags */
    struct brand_info *brands;      /* Racial brands */
    struct slay_info *slays;        /* Racial slays */
    bitflag pflags[PF_SIZE];        /* Racial (player) flags */
    uint8_t pflvl[PF__MAX];         /* Application level for racial (player) flags */
    struct history_chart *history;
    struct element_info el_info[ELEM_MAX];  /* Resists */
    struct player_shape *shapes;
    struct barehanded_attack *attacks;
    struct gift *gifts;         /* Racial gifts */
};

/*
 * Dragon breed info
 */
struct dragon_breed
{
    struct dragon_breed *next;
    char *d_name;               /* Dragon name */
    uint8_t d_fmt;              /* Dragon name format ("dragon" or "drake") */
    char *w_name;               /* Wyrm name */
    uint8_t w_fmt;              /* Wyrm name format ("xxx wyrm" or "wyrm of xxx") */
    uint8_t commonness;         /* Commonness of the breed */
    int16_t r_exp;              /* Experience factor */
    uint8_t immune;             /* Immunity to element? */
};

/*
 * Items the player starts with. Used in player_class and specified in
 * class.txt.
 */
struct start_item
{
    int tval;                   /* General object type (see TV_ macros) */
    int sval;                   /* Object sub-type */
    int min;                    /* Minimum starting amount */
    int max;                    /* Maximum starting amount */
    int *eopts;                 /* Indices (zero terminated array) for birth options which can exclude item */
    struct start_item *next;
};

/*
 * Structure for magic realms
 */
struct magic_realm
{
    struct magic_realm *next;
    char *name;
    int stat;
    char *verb;
    char *spell_noun;
    char *book_noun;
};

/*
 * A structure to hold class-dependent information on spells.
 */
struct class_spell
{
    char *name;
    char *text;
    struct effect *effect;              /* The spell's effect */
    const struct magic_realm *realm;    /* The magic realm of this spell */
    int sidx;                           /* The index of this spell for this class */
    int bidx;                           /* The index into the player's books array */
    int slevel;                         /* Required level (to learn) */
    int smana;                          /* Required mana (to cast) */
    int sfail;                          /* Minimum chance of failure */
    int sexp;                           /* Encoded experience bonus */
    int sproj;                          /* Can be projected */
    int cooldown;                       /* Cooldown */
};

/*
 * A structure to hold class-dependent information on spell books.
 */
struct class_book
{
    uint16_t tval;                      /* Item type of the book */
    int sval;                           /* Item sub-type for book (book number) */
    bool dungeon;                       /* Whether this is a dungeon book */
    const struct magic_realm *realm;    /* The magic realm of this book */
    int num_spells;                     /* Number of spells in this book */
    struct class_spell *spells;         /* Spells in the book */
};

/*
 * Information about class magic knowledge
 */
struct class_magic
{
    uint16_t spell_first;                   /* Level of first spell */
    int spell_weight;                       /* Max armor weight to avoid mana penalties */
    int num_books;                          /* Number of spellbooks */
    struct class_book *books;               /* Details of spellbooks */
    uint8_t total_spells;                   /* Number of spells for this class */

    /* PWMAngband: dummy placeholders to display fail rate of the first spell on the client */
    int sfail;
    int slevel;

    /* PWMAngband: dummy placeholder to hold the tval of the first book of the realm */
    uint16_t tval_first;
};

/*
 * Player class info
 */
struct player_class
{
    struct player_class *next;
    char *name;                     /* Name */
    unsigned int cidx;              /* Index */
    char *title[PY_MAX_LEVEL / 5];  /* Titles */
    struct modifier modifiers[OBJ_MOD_MAX]; /* Modifiers */
    int16_t c_skills[SKILL_MAX];    /* Class skills */
    int x_skills[SKILL_MAX];        /* Extra skills */
    uint8_t c_mhp;                  /* Hit-dice adjustment */
    bitflag flags[OF_SIZE];         /* (Object) flags */
    uint8_t flvl[OF_MAX];           /* Application level for (object) flags */
    struct brand_info *brands;      /* Class brands */
    struct slay_info *slays;        /* Class slays */
    bitflag pflags[PF_SIZE];        /* (Player) flags */
    uint8_t pflvl[PF__MAX];         /* Application level for racial (player) flags */
    struct element_info el_info[ELEM_MAX];  /* Resists */
    int max_attacks;                /* Maximum possible attacks */
    int min_weight;                 /* Minimum weapon weight for calculations */
    int att_multiply;               /* Multiplier for attack calculations */
    struct start_item *start_items; /* Starting inventory */
    struct class_magic magic;       /* Magic spells */
    uint8_t attr;                   /* Class color */
    struct player_shape *shapes;
    struct barehanded_attack *attacks;
};

/*
 * Info for player abilities
 */
struct player_ability
{
    struct player_ability *next;
    uint16_t index;                 /* PF_*, OF_* or element index */
    char *type;                     /* Ability type */
    char *name;                     /* Ability name */
    char *desc;                     /* Ability description */
    int group;                      /* Ability group (set locally when viewing) */
    int value;                      /* Resistance value for elements */
    int idx;                        /* Index value for elements */
};

/*  
 * Histories are a graph of charts; each chart contains a set of individual
 * entries for that chart, and each entry contains a text description and a
 * successor chart to move history generation to
 * For example:
 *   chart 1
 *   {
 *     entry
 *     {
 *       desc "You are the illegitimate and unacknowledged child";
 *       next 2;
 *     };
 *     entry
 *     {
 *       desc "You are the illegitimate but acknowledged child";
 *       next 2;
 *     };
 *     entry
 *     {
 *       desc "You are one of several children";
 *       next 3;
 *     };
 *   };
 *
 * History generation works by walking the graph from the starting chart for
 * each race, picking a random entry (with weighted probability) each time
 */
struct history_entry
{
    struct history_entry *next;
    struct history_chart *succ;
    int isucc;
    int roll;
    char *text;
};

struct history_chart
{
    struct history_chart *next;
    struct history_entry *entries;
    unsigned int idx;
};

/*
 * Player history table
 */
struct history_info
{
    bitflag type[HIST_SIZE];    /* Kind of history item */
    int16_t dlev;               /* Dungeon level when this item was recorded */
    int16_t clev;               /* Character level when this item was recorded */
    const struct artifact *art; /* Artifact this item relates to */
    char name[NORMAL_WID];      /* Artifact name */
    hturn turn;                 /* Turn this item was recorded on */
    char event[NORMAL_WID];     /* The text of the item */
};

/*
 * Player history information
 *
 * See player-history.c/.h
 */
struct player_history
{
    struct history_info *entries;   /* List of entries */
    int16_t next;                   /* First unused entry */
    int16_t length;                 /* Current length */
};

/*
 * An "actor race" structure defining either a monster race or a player ID
 */
struct actor_race
{
    struct player *player;
    struct monster_race *race;
};

#define ACTOR_RACE_NULL(A) \
    (((A) == NULL) || (((A)->player == NULL) && ((A)->race == NULL)))

#define ACTOR_RACE_EQUAL(A1, A2) \
    ((A1)->race && ((A1)->race == (A2)->race))

#define ACTOR_PLAYER_EQUAL(A1, A2) \
    ((A1)->player && ((A1)->player == (A2)->player))

/*
 * Temporary, derived, player-related variables used during play but not saved
 */
struct player_upkeep
{
    uint8_t new_level_method;       /* Climb up stairs, down, or teleport level? */
    bool funeral;                   /* True if player is leaving */
    bool energy_use;                /* Energy use this turn */
    int16_t new_spells;             /* Number of spells available */
    struct source health_who;       /* Who's shown on the health bar */
    struct actor_race monster_race; /* Monster race trackee */
    struct object *object;          /* Object trackee */
    uint32_t notice;                /* Bit flags for pending actions */
    uint32_t update;                /* Bit flags for recalculations needed */
    uint32_t redraw;                /* Bit flags for changes that need to be redrawn by the UI */
    int16_t resting;                /* Resting counter */
    bool running;                   /* Are we running? */
    bool running_firststep;         /* Is this our first step running? */
    struct object **quiver;         /* Quiver objects */
    struct object **inven;          /* Inventory objects */
    int16_t total_weight;           /* Total weight being carried */
    int16_t inven_cnt;              /* Number of items in inventory */
    int16_t equip_cnt;              /* Number of items in equipment */
    int16_t quiver_cnt;             /* Number of items in the quiver */
    int16_t recharge_pow;           /* Power of recharge effect */
    bool running_update;            /* True if updating monster/object lists while running */
    struct object *redraw_equip;    /* Single equipment object to redraw */
    bool skip_redraw_equip;         /* Skip redraw_equip object */
    struct object *redraw_inven;    /* Single inventory object to redraw */
    bool skip_redraw_inven;         /* Skip redraw_inven object */
};

/*
 * Player sex info
 */
typedef struct player_sex
{
    const char *title;      /* Type of sex */
    const char *winner;     /* Name of winner */
    const char *conqueror;  /* Name of conqueror of the Nether Realm */
    const char *killer;     /* Name of Melkor killer */
} player_sex;

extern player_sex sex_info[MAX_SEXES];

/*
 * Square flags
 */
enum
{
    #define SQUARE(a, b) SQUARE_##a,
    #include "list-square-flags.h"
    #undef SQUARE
    SQUARE_MAX
};

#define SQUARE_SIZE                FLAG_SIZE(SQUARE_MAX)

#define sqinfo_has(f, flag)        flag_has_dbg(f, SQUARE_SIZE, flag, #f, #flag)
#define sqinfo_next(f, flag)       flag_next(f, SQUARE_SIZE, flag)
#define sqinfo_is_empty(f)         flag_is_empty(f, SQUARE_SIZE)
#define sqinfo_is_full(f)          flag_is_full(f, SQUARE_SIZE)
#define sqinfo_is_inter(f1, f2)    flag_is_inter(f1, f2, SQUARE_SIZE)
#define sqinfo_is_subset(f1, f2)   flag_is_subset(f1, f2, SQUARE_SIZE)
#define sqinfo_is_equal(f1, f2)    flag_is_equal(f1, f2, SQUARE_SIZE)
#define sqinfo_on(f, flag)         flag_on_dbg(f, SQUARE_SIZE, flag, #f, #flag)
#define sqinfo_off(f, flag)        flag_off(f, SQUARE_SIZE, flag)
#define sqinfo_wipe(f)             flag_wipe(f, SQUARE_SIZE)
#define sqinfo_setall(f)           flag_setall(f, SQUARE_SIZE)
#define sqinfo_negate(f)           flag_negate(f, SQUARE_SIZE)
#define sqinfo_copy(f1, f2)        flag_copy(f1, f2, SQUARE_SIZE)
#define sqinfo_union(f1, f2)       flag_union(f1, f2, SQUARE_SIZE)
#define sqinfo_inter(f1, f2)       flag_inter(f1, f2, SQUARE_SIZE)
#define sqinfo_diff(f1, f2)        flag_diff(f1, f2, SQUARE_SIZE)

struct player_square
{
    uint16_t feat;
    bitflag *info;
    int light;
    struct object *obj;
    struct trap *trap;
};

struct heatmap
{
    uint16_t **grids;
};

struct player_cave
{
    uint16_t feeling_squares;   /* How many feeling squares the player has visited */
    int height;
    int width;
    struct player_square **squares;
    struct heatmap noise;
    struct heatmap scent;
    bool allocated;
};

/*
 * Player info recording the original (pre-ghost) cause of death
 */
struct player_death_info
{
    char title[NORMAL_WID];     /* Title */
    int16_t max_lev;            /* Max level */
    int16_t lev;                /* Level */
    int32_t max_exp;            /* Max experience */
    int32_t exp;                /* Experience */
    int32_t au;                 /* Gold */
    int16_t max_depth;          /* Max depth */
    struct worldpos wpos;       /* Position on the world map */
    char died_from[NORMAL_WID]; /* Cause of death */
    time_t time;                /* Time of death */
    char ctime[NORMAL_WID];
};

/* The information needed to show a single "grid" */
typedef struct
{
    uint16_t a; /* Color attribute */
    char c; /* ASCII character */
} cave_view_type;

/* Information about a "hostility" */
typedef struct _hostile_type
{
    int32_t id;                 /* ID of player we are hostile to */
    struct _hostile_type *next; /* Next in list */
} hostile_type;

/* Archer flags */
struct bow_brand
{
    bitflag type;
    bool blast;
    int dam;
};

/*
 * Special values for escape key
 */
enum
{
    ES_KEY = 0,
    ES_BEGIN_MACRO,
    ES_END_MACRO
};

/*
 * Most of the "player" information goes here.
 *
 * This stucture gives us a large collection of player variables.
 *
 * This entire structure is wiped when a new character is born.
 *
 * This structure is more or less laid out so that the information
 * which must be saved in the savefile precedes all the information
 * which can be recomputed as needed.
 */
struct player
{
    /*** Angband common fields ***/

    const struct player_race *race;
    const struct player_class *clazz;
    struct loc grid;                            /* Player location */
    uint8_t hitdie;                             /* Hit dice (sides) */
    int16_t expfact;                            /* Experience factor */
    int16_t age;                                /* Characters age */
    int16_t ht;                                 /* Height */
    int16_t wt;                                 /* Weight */
    int32_t au;                                 /* Current Gold */
    int16_t max_depth;                          /* Max depth */
    struct worldpos wpos;                       /* Current position on the world map */
    int16_t max_lev;                            /* Max level */
    int16_t lev;                                /* Cur level */
    int32_t max_exp;                            /* Max experience */
    int32_t exp;                                /* Cur experience */
    uint16_t exp_frac;                          /* Cur exp frac (times 2^16) */
    int16_t mhp;                                /* Max hit pts */
    int16_t chp;                                /* Cur hit pts */
    uint16_t chp_frac;                          /* Cur hit frac (times 2^16) */
    int16_t msp;                                /* Max mana pts */
    int16_t csp;                                /* Cur mana pts */
    uint16_t csp_frac;                          /* Cur mana frac (times 2^16) */
    int16_t stat_max[STAT_MAX];                 /* Current "maximal" stat values */
    int16_t stat_cur[STAT_MAX];                 /* Current "natural" stat values */
    int16_t stat_map[STAT_MAX];                 /* Tracks remapped stats from temp stat swap */
    int16_t *timed;                             /* Timed effects */
    int16_t word_recall;                        /* Word of recall counter */
    int16_t deep_descent;                       /* Deep Descent counter */
    int32_t energy;                             /* Current energy */
    uint8_t unignoring;                         /* Player doesn't hide ignored items */
    uint8_t *spell_flags;                       /* Spell flags */
    uint8_t *spell_order;                       /* Spell order */
    char full_name[NORMAL_WID];                 /* Full name */
    char died_from[NORMAL_WID];                 /* Cause of death */
    char history[N_HIST_LINES][N_HIST_WRAP];    /* Player history */
    uint16_t total_winner;                      /* Total winner */
    uint8_t noscore;                            /* Cheating flags */
    bool is_dead;                               /* Player is dead */
    int16_t player_hp[PY_MAX_LEVEL];            /* HP gained per level */

    /* Saved values for quickstart */
    int16_t stat_birth[STAT_MAX];               /* Birth "natural" stat values */

    struct player_options opts;                 /* Player options */
    struct player_history hist;                 /* Player history (see player-history.c) */

    struct player_body body;                    /* Equipment slots available */

    struct object *gear;                        /* Real gear */

    struct object *obj_k;                       /* Object knowledge ("runes") */
    struct player_cave *cave;                   /* Known version of current level */

    struct player_state state;                  /* Calculatable state */
    struct player_state known_state;            /* What the player can know of the above */
    struct player_upkeep *upkeep;               /* Temporary player-related values */

    /*** Angband global variables (tied to the player in MAngband) ***/

    uint8_t run_cur_dir;    /* Direction we are running */
    uint8_t run_old_dir;    /* Direction we came from */
    bool run_open_area;     /* Looking for an open area */
    bool run_break_right;   /* Looking for a break (right) */
    bool run_break_left;    /* Looking for a break (left) */

    int size_mon_hist;
    int size_mon_msg;
    struct monster_race_message *mon_msg;
    struct monster_message_history *mon_message_hist;

    /*** MAngband common fields ***/

    const struct player_sex *sex;
    uint8_t psex;                           /* Sex index */
    uint8_t stealthy;                       /* Stealth mode */
    hturn game_turn;                        /* Number of game turns */
    hturn player_turn;                      /* Number of player turns (including resting) */
    hturn active_turn;                      /* Number of active player turns */
    bool* kind_aware;                       /* Is the player aware of this obj kind? */
    bool* kind_tried;                       /* Has the player tried this obj kind? */
    char name[NORMAL_WID];                  /* Character nickname */
    char pass[NORMAL_WID];                  /* Password */
    int32_t id;                             /* Unique ID to each player character (not account!) */
    int16_t ghost;                          /* Are we a ghost */
    uint8_t lives;                          /* Number of times we have resurrected */
    uint8_t party;                          /* The party he belongs to (or 0 if neutral) */
    struct player_death_info death_info;    /* Original cause of death */
    uint16_t retire_timer;                  /* The number of minutes this guy can play until retired. */
    uint8_t **wild_map;                     /* The wilderness we have explored */
    uint8_t *art_info;                      /* Artifacts player has encountered */

    /*** MAngband temporary fields ***/

    int conn;                                       /* Connection number */
    char hostname[NORMAL_WID];                      /* His hostname */
    char addr[NORMAL_WID];                          /* His IP address */
    unsigned int version;                           /* His version */
    hostile_type *hostile;                          /* List of players we wish to attack */
    char savefile[MSG_LEN];                         /* Name of the savefile */
    char panicfile[MSG_LEN];                        /* Name of the panic save corresponding to savefile */
    bool alive;                                     /* Are we alive */
    struct worldpos recall_wpos;                    /* Where to recall */
    cave_view_type* hist_flags[N_HISTORY_FLAGS];    /* Player's sustains/resists/flags */
    struct source cursor_who;                       /* Who's tracked by cursor */
    uint8_t special_file_type;                      /* Type of info browsed by this player */
    bitflag (*mflag)[MFLAG_SIZE];                   /* Temporary monster flags */
    uint8_t *mon_det;                               /* Were these monsters detected by this player? */
    bitflag pflag[MAX_PLAYERS][MFLAG_SIZE];         /* Temporary monster flags (players) */
    uint8_t play_det[MAX_PLAYERS];                  /* Were these players detected by this player? */
    uint8_t *d_attr;
    char *d_char;
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
    uint8_t use_graphics;
    uint8_t screen_cols;
    uint8_t screen_rows;
    uint8_t tile_wid;
    uint8_t tile_hgt;
    bool tile_distorted;
    struct loc offset_grid;
    struct loc old_offset_grid;
    cave_view_type **scr_info;
    cave_view_type **trn_info;
    char msg_log[MAX_MSG_HIST][NORMAL_WID]; /* Message history log */
    int16_t msg_hist_ptr;                   /* Where will the next message be stored */
    uint8_t last_dir;                       /* Last direction moved (used for swapping places) */
    int16_t current_spell;                  /* Current values */
    int16_t current_item;
    int16_t current_action;
    int16_t current_value;
    int16_t current_selling;
    int16_t current_sell_amt;
    int current_sell_price;
    int current_house;                      /* Which house is he pointing */
    int store_num;                          /* What store this guy is in */
    int player_store_num;                   /* What player store this guy is in */
    int16_t delta_floor_item;               /* Player is standing on.. */
    int16_t msg_hist_dupe;                  /* Count duplicate messages for collapsing */
    uint32_t dm_flags;                      /* Dungeon Master Flags */
    uint16_t msg_last_type;                 /* Last message type sent */
    uint16_t main_channel;                  /* Main chat channel the player is in */
    char second_channel[NORMAL_WID];        /* Where his legacy 'privates' are sent */
    uint8_t *on_channel;                    /* Listening to what channels */
    cave_view_type info[MAX_TXT_INFO][NORMAL_WID];
    struct loc info_grid;
    int16_t last_info_line;
    uint8_t remote_term;
    bool bubble_checked;                    /* Have we been included in a time bubble check? */
    hturn bubble_change;                    /* Server turn we last changed colour */
    bool bubble_colour;                     /* Current warning colour for slow time bubbles */
    int bubble_speed;                       /* Current speed for slow time bubbles */
    uint32_t blink_speed;                   /* Current blink speed for slow time bubbles */
    int arena_num;                          /* What arena this guy is in */
    uint32_t window_flag;
    bool prevents[128];                     /* Cache of "^" inscriptions */
    int16_t feeling;                        /* Most recent feeling */
    int16_t interactive_line;               /* Which line is he on? */
    char *interactive_file;                 /* Which file is he reading? */
    int16_t interactive_next;               /* Which line is he on 'in the file' ? */
    int16_t interactive_size;               /* Total number of lines in file */
    char interactive_hook[26][32];          /* Sub-menu information */
    int set_value;                          /* Set value for a chain of effects */

    /* Targeting */
    struct target target;                   /* Player target */
    bool target_fixed;                      /* Is the target fixed (for the duration of a spell)? */
    struct target old_target;               /* Old player target */
    bool show_interesting;                  /* Interesting grids */
    int16_t target_index;                   /* Current index */
    struct loc tt_grid;                     /* Current location */
    struct object *tt_o;                    /* Current object */
    uint8_t tt_step;                        /* Current step */
    bool tt_help;                           /* Display info/help */

    /*** PWMAngband common fields ***/

    struct quest quest;                 /* Current quest */
    char died_flavor[160];              /* How this guy died */
    int16_t tim_mimic_what;             /* Rogue flag */
    struct monster_lore *lore;          /* Monster lore */
    struct monster_race *poly_race;     /* Monster race (mimic form) */
    int16_t k_idx;                      /* Object kind index (mimic form) */
    uint8_t *randart_info;              /* Randarts player has encountered */
    uint8_t *randart_created;           /* Randarts player has created */
    uint8_t *spell_power;               /* Spell power array */
    uint8_t *spell_cooldown;            /* Spell cooldown array */
    uint8_t *kind_ignore;               /* Ignore this object kind */
    uint8_t *kind_everseen;             /* Has the player seen this object kind? */
    uint8_t **ego_ignore_types;         /* Table for ignoring by ego and type */
    uint8_t *ego_everseen;              /* Has the player seen this ego type? */
    hturn quit_turn;                    /* Turn this player left the game */
    struct bow_brand brand;             /* Archer flags */
    struct store *home;                 /* Home inventory */

    /*** PWMAngband temporary fields ***/

    int32_t esp_link;                  /* Mind flags */
    uint8_t esp_link_type;
    int16_t spell_cost;                /* Total cost for spells */
    uint8_t ignore;                    /* Player has auto-ignore activated */
    struct monster_lore current_lore;
    bool fainting;                  /* True if player is fainting */
    uint8_t max_hgt;                /* Max client screen height */
    cave_view_type **info_icky;     /* Info is icky */
    int16_t last_info_line_icky;
    char *header_icky;
    bool mlist_icky;
    int16_t screen_save_depth;      /* Depth of the screen_save() stack */
    bool locating;                  /* Is the player looking around? */
    bool was_aware;                 /* Is the player aware of the current obj type? */
    int16_t current_sound;          /* Current sound */
    int32_t charge;                 /* Charging energy */
    bool has_energy;                /* Player has energy */
    hturn idle_turn;                /* Turn since last game command */
    bool full_refresh;              /* Full refresh (includes monster/object lists) */
    uint8_t digging_request;
    uint8_t digging_dir;
    bool firing_request;
    bool cancel_firing;
    bool shimmer;                   /* Hack -- optimize multi-hued code (players) */
    bool delayed_display;           /* Hack -- delay messages after character creation */
    bool did_visuals;               /* Hack -- projection indicator (visuals) */
    bool do_visuals;                /* Hack -- projection indicator (visuals) */
    struct loc old_grid;            /* Previous player location */
    bool path_drawn;                /* NPP's visible targeting */
    int path_n;
    struct loc path_g[256];
    bool can_study_book;            /* Player carries a book with spells they can study */
    uint8_t slaves;                 /* Number of controlled monsters */
    char tempbuf[NORMAL_WID];
    int16_t obj_feeling;            /* Object/monster feeling (for display) */
    int16_t mon_feeling;
    char depths[13];                /* Displayed coordinates */
    char locname[NORMAL_WID];       /* Location (short name) */
    int frac_blow;                  /* Blow frac (%) */
    int frac_shot;                  /* Shot frac (%) */
    int16_t square_light;           /* Square light (for display) */
    char terrain[40];               /* Displayed terrain */
    uint8_t flicker;                /* A counter to select the step color from the flicker table */
    bool no_disturb_icky;
    bool placed;                    /* Player is properly placed on the level */
    int monwidth;                   /* Monster list subwindow width */
    int32_t extra_energy;           /* Extra energy */
    bool first_escape;
    bool dump_gen;
    bool icy_aura;
    uint8_t cannot_cast;            /* Player cannot cast spells */
    uint8_t cannot_cast_mimic;      /* Player cannot cast mimic spells */

    /*
     * In order to prevent the regeneration bonus from the first few turns, we have
     * to store the number of turns the player has rested. Otherwise, the first
     * few turns will have the bonus and the last few will not.
     */
    int player_turns_rested;
    bool player_rest_disturb;

    /* Shared monster/object list instances */
    void *monster_list_subwindow;
    void *object_list_subwindow;

    quark_t* note_aware;    /* Autoinscription quark number */

    // Tangaria common fields
    uint32_t account_id;            // account id (needed for housing)
    char account_name[NORMAL_WID];  // account name (needed for housing)
    uint32_t account_score;         // account score (for ladder)
    uint8_t supporter;              // supporter's account lvl
    uint8_t y_cooldown;             // cooldown for racial abilities
    uint16_t iron_timer;             // time until next auto > for ironman

    // Weather
    uint8_t weather_type;           // type of weather
    uint8_t weather_wind;           // current gust of wind
    uint8_t weather_intensity;      // density of raindrops/snowflakes
};

extern struct player_body *bodies;
extern struct player_race *races;
extern struct dragon_breed *breeds;
extern struct player_class *classes;
extern struct start_item *dm_start_items;
extern struct player_ability *player_abilities;
extern struct magic_realm *realms;

#endif /* INCLUDED_PLAYER_COMMON_H */
