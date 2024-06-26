/*
 * File: house.h
 * Purpose: House code.
 */

#ifndef INCLUDED_HOUSE_H
#define INCLUDED_HOUSE_H

/* State of a house: 0 = unallocated, 1 = normal, 2 = extended, 3 = custom */
enum
{
    HOUSE_NORMAL = 1,
    HOUSE_EXTENDED,
    HOUSE_CUSTOM
};

/* Information about a "house" */
struct house_type
{
    struct loc grid_1;             /* Location of house */
    struct loc grid_2;
    struct loc door;               /* Location of door */
    struct worldpos wpos;          /* Position on the world map */
    int32_t price;                 /* Cost of buying */
    // as in T we bind houses to account: changed int32_t to uint32_t for ownerid (cause account is u)
    uint32_t ownerid;              // Owner account ID
    char ownername[NORMAL_WID];    // Owner account name
    time_t last_visit_time;        // When house was 'refreshed' (open the door) last time
    uint8_t color;                 /* Door color */
    uint8_t state;                 /* State */
    uint8_t free;                  /* House is free (bought with a Deed of Property) */
};

/* Initialize the house package */
extern void houses_init(void);

/* De-initialize the house package */
extern void houses_free(void);

/* Count the number of houses */
extern int houses_count(void);

// Count house area size (without outer walls)
extern int house_count_area_size(int house);

/* Determine if the player is inside the house */
extern bool house_inside(struct player *p, int house);

/* Determine if the player owns the house */
extern bool house_owned_by(struct player *p, int house);

/* Count the number of owned houses */
extern int houses_owned(struct player *p);

/* Return the index of a house given a coordinate pair */
extern int pick_house(struct worldpos *wpos, struct loc *grid);

/* Given coordinates return a house to which they belong */
extern int find_house(struct player *p, struct loc *grid, int offset);

/* Set house owner */
extern void set_house_owner(struct player *p, struct house_type *house);

/* Get an empty house slot */
extern int house_add(bool custom);

/* Set house */
extern void house_set(int slot, struct house_type *house);

/* List owned houses in a file */
extern void house_list(struct player *p, ang_file *fff);

/* Determine if the level contains owned houses */
extern bool level_has_owned_houses(struct worldpos *wpos);

/* Determine if the level contains any houses */
extern bool level_has_any_houses(struct worldpos *wpos);

/* Wipe old houses on a level */
extern void wipe_old_houses(struct worldpos *wpos);

/* Wipe custom houses on a level */
extern void wipe_custom_houses(struct worldpos *wpos);

/* Determine if the player has stored items in houses */
extern bool has_home_inventory(struct player *p);

/* Dump content of owned houses in a file */
extern void house_dump(struct player *p, ang_file *fp);

/* Determine if the location is inside a house */
extern bool location_in_house(struct worldpos *wpos, struct loc *grid);

/* Get house */
extern struct house_type *house_get(int house);

/* Reset house (only for character items, not for account) */
extern void reset_house_rip(int house);

/* Reset owned houses (only for character items, not for account) */
extern void reset_houses_rip(struct player *p);

/* Reset house */
extern void reset_house(int house);

/* Reset owned houses */
extern void reset_houses(struct player *p);

/* Know content of owned houses */
extern void know_houses(struct player *p);

/* Colorize house door */
extern void colorize_door(struct player *p, struct object_kind *kind, struct chunk *c,
    struct loc *grid);

/* Return the name of a player owned store */
extern bool get_player_store_name(int num, char *name, int len);

/* Return the index of a house near a location */
extern int house_near(struct player *p, struct loc *grid1, struct loc *grid2);

/* Extend house */
extern bool house_extend(void);

/* Memorize the content of owned houses */
extern void memorize_houses(struct player *p);

#endif /* INCLUDED_HOUSE_H */
