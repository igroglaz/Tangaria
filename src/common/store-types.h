/*
 * File: store-types.h
 * Purpose: Global store type declarations
 */

#ifndef INCLUDED_STORE_TYPES_H
#define INCLUDED_STORE_TYPES_H

/*** Constants ***/

/* Number of welcome messages (1 for every 5 clvl starting at clvl 6) */
#define N_WELCOME   9

/*** Types ***/

struct object_buy
{
    struct object_buy *next;
    uint16_t tval;
    size_t flag;
};

/*
 * A store owner
 */
struct owner
{
    char *name;         /* Name */
    unsigned int oidx;  /* Index */
    struct owner *next;
    int32_t max_cost;      /* Purse limit */
};

struct normal_entry
{
    struct object_kind *kind;
    int16_t rarity;
    int16_t factor;
};

/*
 * A store, with an owner, various state flags, a current stock
 * of items, and a table of items that are often purchased.
 */
struct store
{
    struct owner *owners;   /* Owners */
    struct owner *owner;    /* Current owner */
    int feat;               /* Index for entrance's terrain */
    int16_t stock_num;      /* Stock -- number of entries */
    int16_t stock_size;     /* Stock -- size of array (transient) */
    struct object *stock;   /* Stock -- actual stock items */

    /* Always stock these items */
    size_t always_size;
    size_t always_num;
    struct object_kind **always_table;

    /* Select a number of these items to stock */
    size_t normal_size;
    size_t normal_num;
    struct normal_entry *normal_table;

    /* Buy these items */
    struct object_buy *buy;

    int turnover;
    int normal_stock_min;
    int normal_stock_max;

    int16_t max_depth;             /* Max level of last customer */

    char comment_welcome[N_WELCOME][NORMAL_WID];
};

#endif /* INCLUDED_STORE_TYPES_H */
