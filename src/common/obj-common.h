/*
 * File: obj-common.h
 * Purpose: Structures and functions for objects
 */

#ifndef INCLUDED_OBJECT_COMMON_H
#define INCLUDED_OBJECT_COMMON_H

/*** Constants ***/

/*
 * Spell types used by project(), needed for object resistances.
 */
enum
{
    #define ELEM(a, b, c, d) ELEM_##a,
    #include "list-elements.h"
    #undef ELEM
    ELEM_MAX
};

#define ELEM_BASE_MIN   ELEM_ACID
#define ELEM_BASE_MAX   ELEM_COLD
#define ELEM_HIGH_MIN   ELEM_POIS
#define ELEM_HIGH_MAX   ELEM_DISEN
#define ELEM_XHIGH_MAX  ELEM_WATER

/* The object flags */
enum
{
    #define OF(a, b) OF_##a,
    #include "list-object-flags.h"
    #undef OF
    OF_MAX
};

#define OF_SIZE                FLAG_SIZE(OF_MAX)

#define of_has(f, flag)        flag_has_dbg(f, OF_SIZE, flag, #f, #flag)
#define of_next(f, flag)       flag_next(f, OF_SIZE, flag)
#define of_count(f)            flag_count(f, OF_SIZE)
#define of_is_empty(f)         flag_is_empty(f, OF_SIZE)
#define of_is_full(f)          flag_is_full(f, OF_SIZE)
#define of_is_inter(f1, f2)    flag_is_inter(f1, f2, OF_SIZE)
#define of_is_subset(f1, f2)   flag_is_subset(f1, f2, OF_SIZE)
#define of_is_equal(f1, f2)    flag_is_equal(f1, f2, OF_SIZE)
#define of_on(f, flag)         flag_on_dbg(f, OF_SIZE, flag, #f, #flag)
#define of_off(f, flag)        flag_off(f, OF_SIZE, flag)
#define of_wipe(f)             flag_wipe(f, OF_SIZE)
#define of_setall(f)           flag_setall(f, OF_SIZE)
#define of_negate(f)           flag_negate(f, OF_SIZE)
#define of_copy(f1, f2)        flag_copy(f1, f2, OF_SIZE)
#define of_union(f1, f2)       flag_union(f1, f2, OF_SIZE)
#define of_inter(f1, f2)       flag_inter(f1, f2, OF_SIZE)
#define of_diff(f1, f2)        flag_diff(f1, f2, OF_SIZE)

#define of_has_unique(f, flag) \
    ((of_next(f, FLAG_START) == flag) && (of_next(f, flag + 1) == FLAG_END))

/* The object kind flags */
enum
{
    #define KF(a, b) KF_##a,
    #include "list-kind-flags.h"
    #undef KF
    KF_MAX
};

#define KF_SIZE                FLAG_SIZE(KF_MAX)

#define kf_has(f, flag)        flag_has_dbg(f, KF_SIZE, flag, #f, #flag)
#define kf_next(f, flag)       flag_next(f, KF_SIZE, flag)
#define kf_is_empty(f)         flag_is_empty(f, KF_SIZE)
#define kf_is_full(f)          flag_is_full(f, KF_SIZE)
#define kf_is_inter(f1, f2)    flag_is_inter(f1, f2, KF_SIZE)
#define kf_is_subset(f1, f2)   flag_is_subset(f1, f2, KF_SIZE)
#define kf_is_equal(f1, f2)    flag_is_equal(f1, f2, KF_SIZE)
#define kf_on(f, flag)         flag_on_dbg(f, KF_SIZE, flag, #f, #flag)
#define kf_off(f, flag)        flag_off(f, KF_SIZE, flag)
#define kf_wipe(f)             flag_wipe(f, KF_SIZE)
#define kf_setall(f)           flag_setall(f, KF_SIZE)
#define kf_negate(f)           flag_negate(f, KF_SIZE)
#define kf_copy(f1, f2)        flag_copy(f1, f2, KF_SIZE)
#define kf_union(f1, f2)       flag_union(f1, f2, KF_SIZE)
#define kf_inter(f1, f2)       flag_inter(f1, f2, KF_SIZE)
#define kf_diff(f1, f2)        flag_diff(f1, f2, KF_SIZE)

/* The object modifiers */
enum
{
    #define STAT(a, b, c) OBJ_MOD_##a,
    #include "list-stats.h"
    #undef STAT
    #define OBJ_MOD(a, b, c) OBJ_MOD_##a,
    #include "list-object-modifiers.h"
    #undef OBJ_MOD
    OBJ_MOD_MAX
};

/*
 * The different kinds of quality ignoring
 */
enum
{
    IGNORE_NONE,
    IGNORE_BAD,
    IGNORE_AVERAGE,
    IGNORE_GOOD,
    IGNORE_ALL,

    IGNORE_MAX
};

/*** Structures ***/

/*
 * Effect
 */
struct effect
{
    struct effect *next;
    uint16_t index;     /* The effect index */
    dice_t *dice;       /* Dice expression used in the effect */
    int subtype;        /* Projection type, timed effect type, etc. */
    int radius;         /* Radius of the effect (if it has one) */
    int other;          /* Extra parameter to be passed to the handler */
    int y;              /* Y coordinate or distance */
    int x;              /* X coordinate or distance */
    int flag;           /* Hack -- flag for mimic spells */
    char *self_msg;     /* Message for affected player */
    char *other_msg;    /* Message for other players */
};

/*
 * Chests
 */
struct chest_trap
{
    struct chest_trap *next;
    char *name;
    char *code;
    int level;
    struct effect *effect;
    int pval;
    bool destroy;
    bool magic;
    char *msg;
    char *msg_death;
};

/*
 * Object flavors
 */
struct flavor
{
    char *text;         /* Text */
    unsigned int fidx;  /* Index */
    struct flavor *next;
    uint16_t tval;      /* Associated object type */
    uint16_t sval;      /* Associated object sub-type */
    uint8_t d_attr;     /* Default flavor attribute */
    char d_char;        /* Default flavor character */
};

/*
 * Brand type
 */
struct brand
{
    char *code;
    char *name;
    char *verb;
    int resist_flag;
    int multiplier;
    int power;
    char *active_verb;
    char *active_verb_plural;
    char *desc_adjective;
    struct brand *next;
};

/*
 * Slay type
 */
struct slay
{
    char *code;
    char *name;
    char *base;
    char *melee_verb;
    char *range_verb;
    int race_flag;
    int multiplier;
    int power;
    int esp_chance;
    int esp_flag;
    struct slay *next;
};

/*
 * Curse type
 */
struct curse
{
    struct curse *next;
    char *name;
    bool *poss;
    struct object *obj;
    char *conflict;
    bitflag conflict_flags[OF_SIZE];
    char *desc;
};

extern struct curse *curses;

enum
{
    EL_INFO_HATES = 0x01,
    EL_INFO_IGNORE = 0x02,
    EL_INFO_RANDOM = 0x04
};

/*
 * Maximum number of element info types depending on level
 */
#define MAX_EL_INFO   3

/*
 * Element info type
 */
struct element_info
{
    int16_t res_level[MAX_EL_INFO];
    uint8_t lvl[MAX_EL_INFO];
    bitflag flags;
    uint8_t idx;
};

/*
 * Activation structure
 */
struct activation
{
    struct activation *next;
    char *name;
    unsigned int index;
    bool aim;
    int power;
    struct effect *effect;
    char *message;
    char *desc;
};

extern struct activation *activations;

/*
 * Information about object types, like rods, wands, etc.
 */
struct object_base
{
    char *name;
    int tval;
    struct object_base *next;
    int attr;
    bitflag flags[OF_SIZE];
    bitflag kind_flags[KF_SIZE];    /* Kind flags */
    struct element_info el_info[ELEM_MAX];
    int break_perc;
    int max_stack;
    int num_svals;
};

/*
 * Information about object kinds, including player knowledge.
 *
 * TODO: split out the user-changeable bits into a separate struct so this
 * one can be read-only.
 */
struct object_kind
{
    char *name;                     /* Name */
    char *text;                     /* Description */
    struct object_base *base;
    uint32_t kidx;                  /* Index */
    struct object_kind *next;
    uint16_t tval;                  /* General object type (see TV_ macros) */
    uint16_t sval;                  /* Object sub-type */
    random_value pval;              /* Item extra-parameter */
    random_value to_h;              /* Bonus to hit */
    random_value to_d;              /* Bonus to damage */
    random_value to_a;              /* Bonus to armor */
    int ac;                         /* Base armor */
    uint8_t dd, ds;                 /* Damage dice/sides */
    int weight;                     /* Weight, in 1/10lbs */
    int cost;                       /* Object base cost */
    bitflag flags[OF_SIZE];         /* Flags (all) */
    bitflag kind_flags[KF_SIZE];    /* Kind flags */
    random_value modifiers[OBJ_MOD_MAX];
    struct element_info el_info[ELEM_MAX];
    bool *brands;
    bool *slays;
    int *curses;                    /* Array of curse powers */
    uint8_t d_attr;                 /* Default object attribute */
    char d_char;                    /* Default object character */
    int alloc_prob;                 /* Allocation: commonness */
    int alloc_min;                  /* Highest normal dungeon level */
    int alloc_max;                  /* Lowest normal dungeon level */
    int level;                      /* Level */
    int difficulty;                 /* Difficulty level for activation */
    struct effect *effect;          /* Effect this item produces (effects.c) */
    struct activation *activation;  /* Activation */
    random_value time;              /* Recharge time (if appropriate) */
    random_value charge;            /* Number of charges (staves/wands) */
    int gen_mult_prob;              /* Probability of generating more than one */
    random_value stack_size;        /* Number to generate */
    struct flavor *flavor;          /* Special object flavor (or zero) */
};

extern struct object_kind *k_info;

/*
 * Unchanging information about artifacts.
 */
struct artifact
{
    char *name;                     /* Name */
    char *text;                     /* Description */
    uint32_t aidx;                  /* Index */
    struct artifact *next;
    int tval;                       /* General artifact type (see TV_ macros) */
    int sval;                       /* Artifact sub-type */
    int to_h;                       /* Bonus to hit */
    int to_d;                       /* Bonus to damage */
    int to_a;                       /* Bonus to armor */
    int ac;                         /* Base armor */
    int dd, ds;                     /* Base damage dice/sides */
    int weight;                     /* Weight in 1/10lbs */
    bitflag flags[OF_SIZE];         /* Flags (all) */
    int modifiers[OBJ_MOD_MAX];
    struct element_info el_info[ELEM_MAX];
    bool *brands;
    bool *slays;
    int *curses;                    /* Array of curse powers */
    int level;                      /* Level */
    int difficulty;                 /* Difficulty level for activation */
    int alloc_prob;                 /* Chance of being generated (i.e. rarity) */
    int alloc_min;                  /* Minimum depth (can appear earlier) */
    int alloc_max;                  /* Maximum depth (will NEVER appear deeper) */
    struct activation *activation;  /* Artifact activation */
    char *alt_msg;
    random_value time;              /* Recharge time (if appropriate) */
    bool negative_power;            /* Negative power (for randarts) */
};

/*
 * Information about artifacts that changes during the course of play;
 * except for aidx, saved to the save file
 */
struct artifact_upkeep
{
    uint32_t aidx;  /* For cross-indexing with struct artifact */
    bool created;   /* Artifact is created */
    int32_t owner;  /* Artifact owner (if any) */
};

/*
 * Structure for possible object kinds for an ego item
 */
struct poss_item
{
    uint32_t kidx;
    struct poss_item *next;
};

/*
 * Information about ego-items.
 */
struct ego_item
{
    char *name;                     /* Name */
    char *text;                     /* Description */
    uint32_t eidx;                  /* Index */
    struct ego_item *next;
    bitflag flags[OF_SIZE];         /* Flags (all) */
    bitflag kind_flags[KF_SIZE];    /* Kind flags */
    random_value modifiers[OBJ_MOD_MAX];
    int min_modifiers[OBJ_MOD_MAX];
    struct element_info el_info[ELEM_MAX];
    bool *brands;
    bool *slays;
    int *curses;                    /* Array of curse powers */
    int rating;                     /* Level rating boost */
    int level;                      /* Level */
    int difficulty;                 /* Difficulty level for activation */
    int alloc_prob;                 /* Chance of being generated (i.e. rarity) */
    int alloc_min;                  /* Minimum depth (can appear earlier) */
    int alloc_max;                  /* Maximum depth (will NEVER appear deeper) */
    struct poss_item *poss_items;
    random_value to_h;              /* Extra to-hit bonus */
    random_value to_d;              /* Extra to-dam bonus */
    random_value to_a;              /* Extra to-ac bonus */
    int min_to_h;                   /* Minimum to-hit value */
    int min_to_d;                   /* Minimum to-dam value */
    int min_to_a;                   /* Minimum to-ac value */
    struct activation *activation;  /* Activation */
    random_value time;              /* Recharge time (rods/activation) */
};

extern struct ego_item *e_info;

/*
 * State of activatable items (including rods)
 */
enum
{
    ACT_NONE,       /* Not activatable */
    ACT_TIMEOUT,    /* Charging */
    ACT_NORMAL      /* Activatable */
};

/*
 * Direction choice
 */
enum
{
    AIM_NONE,   /* None */
    AIM_RANDOM, /* Random */
    AIM_NORMAL  /* Normal */
};

/*
 * Hack -- extra information used by the client
 */
struct object_xtra
{
    uint8_t attr;              /* Color */
    uint8_t act;               /* Activation flag */
    uint8_t aim;               /* Aiming flag */
    uint8_t fuel;              /* Fuelable flag */
    uint8_t fail;              /* Fail flag */
    int16_t slot;              /* Slot flag */
    uint8_t max;               /* Max amount */
    int16_t owned;             /* Owned amount */
    uint8_t stuck;             /* Stuck flag */
    uint8_t known;             /* Known flag */
    uint8_t known_effect;      /* Known effect flag */
    uint8_t identified;        /* Identified flag */
    uint8_t sellable;          /* Sellable flag */
    uint8_t carry;             /* Carry flag */
    uint8_t quality_ignore;    /* Quality ignoring */
    uint8_t ignored;           /* Ignored flag */
    int16_t eidx;              /* Ego index (or -1 if no ego) */
    uint8_t equipped;          /* Equipped flag */
    uint8_t magic;             /* Magic flag */
    int16_t bidx;              /* Book index */
    uint8_t throwable;         /* Throwable flag */
    char name[NORMAL_WID];
    char name_terse[NORMAL_WID];
    char name_base[NORMAL_WID];
    char name_curse[NORMAL_WID];
    char name_power[NORMAL_WID];
};

/*
 * Flags for the obj->notice field
 */
enum
{
    OBJ_NOTICE_WORN = 0x01,
    OBJ_NOTICE_ASSESSED = 0x02,
    OBJ_NOTICE_IGNORE = 0x04
};

struct curse_data
{
    int power;
    int timeout;
    int to_a;
    int to_h;
    int to_d;
    int modifiers[OBJ_MOD_MAX];
};

/*
 * Object information, for a specific object.
 *
 * Note that inscriptions are now handled via the "quark_str()" function
 * applied to the "note" field, which will return NULL if "note" is zero.
 *
 * Each cave grid points to one (or zero) objects via the "obj" field in
 * its "squares" struct.  Each object then points to one (or zero) objects
 * via the "next" field, and (aside from the first) back via its "prev"
 * field, forming a doubly linked list, which in game terms represents a
 * stack of objects in the same grid.
 *
 * Each monster points to one (or zero) objects via the "held_obj"
 * field (see monster.h).  Each object then points to one (or zero) objects
 * and back to previous objects by its own "next" and "prev" fields,
 * forming a doubly linked list, which in game terms represents the
 * monster's inventory.
 *
 * The "held_m_idx" field is used to indicate which monster, if any,
 * is holding the object.  Objects being held have (0, 0) as a grid.
 *
 * Note that object records are not now copied, but allocated on object
 * creation and freed on object destruction.  These records are handed
 * around between player and monster inventories and the floor on a fairly
 * regular basis, and care must be taken when handling such objects.
 */
struct object
{
    struct object_kind *kind;           /* Kind of the object */
    struct ego_item *ego;               /* Ego item info of the object, if any */
    const struct artifact *artifact;    /* Artifact info of the object, if any */

    struct object *prev;                /* Previous object in a pile */
    struct object *next;                /* Next object in a pile */
    struct object *known;               /* Known version of this object */

    int16_t oidx;                       /* Item list index, if any */

    struct loc grid;                    /* Position on map, or (0, 0) */

    uint16_t tval;                      /* Item type (from kind) */
    uint16_t sval;                      /* Item sub-type (from kind) */

    int32_t pval;                       /* Item extra-parameter */

    int16_t weight;                     /* Item weight */

    uint8_t dd;                         /* Number of damage dice */
    uint8_t ds;                         /* Number of sides on each damage die */
    int16_t ac;                         /* Normal AC */
    int16_t to_a;                       /* Plusses to AC */
    int16_t to_h;                       /* Plusses to hit */
    int16_t to_d;                       /* Plusses to damage */

    bitflag flags[OF_SIZE];             /* Object flags */
    int32_t modifiers[OBJ_MOD_MAX];     /* Object modifiers */
    struct element_info el_info[ELEM_MAX];  /* Object element info */
    bool *brands;                       /* Flag absence/presence of each brand */
    bool *slays;                        /* Flag absence/presence of each slay */
    struct curse_data *curses;          /* Array of curse powers and timeouts */

    struct effect *effect;              /* Effect this item produces (effects.c) */
    struct activation *activation;      /* Activation */
    random_value time;                  /* Recharge time (rods/activation) */
    int16_t timeout;                    /* Timeout Counter */

    uint8_t number;                     /* Number of items */
    bitflag notice;                     /* Attention paid to the object */

    int16_t held_m_idx;                 /* Monster holding us (if any) */
    int16_t mimicking_m_idx;            /* Monster mimicking us (if any) */

    uint8_t origin;                     /* How this item was found */
    int16_t origin_depth;               /* What depth the item was found at */
    const struct monster_race *origin_race; /* Monster race that dropped it */

    quark_t note;                       /* Inscription index */

    /* MAngband & PWMAngband fields */

    struct worldpos wpos;               /* Position on the world map */
    int32_t randart_seed;               /* Randart seed, if any */
    int32_t askprice;                   /* Item sale price (transient) */
    int32_t creator;                    /* Item creator (if any) */
    int32_t owner;                      /* Item owner (if any) */
    uint8_t level_req;                  /* Level requirement */
    uint8_t ignore_protect;             /* Bypass auto-ignore */
    uint8_t ordered;                    /* Item has been ordered */
    struct object_xtra info_xtra;       /* Extra information used by the client */
    uint8_t attr;                       /* "attr" last used for drawing object */
    int16_t decay;                      /* Decay timeout for corpses */
    uint8_t bypass_aware;               /* Bypasses the "aware" flag */
    uint8_t soulbound;                  /* Can't drop this item */    
    quark_t origin_player;              /* Original owner */
    uint16_t worn_turn;                 // Number of turns object has been worn (in the dungeon)
};

typedef bool (*item_tester)(struct player *, const struct object *);

#endif /* INCLUDED_OBJECT_COMMON_H */
