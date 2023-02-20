/*
 * File: cave.c
 * Purpose: Chunk allocation and utility functions
 *
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 * Copyright (c) 2022 MAngband and PWMAngband Developers
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


struct feature *f_info;


/*
 * Global array for looping through the "keypad directions"
 */
int16_t ddd[9] =
{ 2, 8, 6, 4, 3, 1, 9, 7, 5 };


/*
 * Global array for converting "keypad direction" into "offsets".
 */
struct loc ddgrid[10] =
{{0, 0}, {-1, 1}, {0, 1}, {1, 1}, {-1, 0}, {0, 0}, {1, 0}, {-1, -1}, {0, -1}, {1, -1}};


/*
 * Global arrays for optimizing "ddx[ddd[i]]", "ddy[ddd[i]]" and
 * "loc(ddx[ddd[i]], ddy[ddd[i]])".
 *
 * This means that each entry in this array corresponds to the direction
 * with the same array index in ddd[].
 */
int16_t ddx_ddd[9] =
{ 0, 0, 1, -1, 1, -1, 1, -1, 0 };

int16_t ddy_ddd[9] =
{ 1, -1, 0, 0, 1, 1, -1, -1, 0 };

struct loc ddgrid_ddd[9] =
{{0, 1}, {0, -1}, {1, 0}, {-1, 0}, {1, 1}, {-1, 1}, {1, -1}, {-1, -1}, {0, 0}};


/*
 * Hack -- precompute a bunch of calls to distance().
 *
 * The pair of arrays dist_offsets_y[n] and dist_offsets_x[n] contain the
 * offsets of all the locations with a distance of n from a central point,
 * with an offset of (0,0) indicating no more offsets at this distance.
 *
 * This is, of course, fairly unreadable, but it eliminates multiple loops
 * from the previous version.
 *
 * It is probably better to replace these arrays with code to compute
 * the relevant arrays, even if the storage is pre-allocated in hard
 * coded sizes.  At the very least, code should be included which is
 * able to generate and dump these arrays (ala "los()").  XXX XXX XXX
 */

static const int d_off_y_0[] =
{ 0 };

static const int d_off_x_0[] =
{ 0 };

static const int d_off_y_1[] =
{ -1, -1, -1, 0, 0, 1, 1, 1, 0 };

static const int d_off_x_1[] =
{ -1, 0, 1, -1, 1, -1, 0, 1, 0 };

static const int d_off_y_2[] =
{ -1, -1, -2, -2, -2, 0, 0, 1, 1, 2, 2, 2, 0 };

static const int d_off_x_2[] =
{ -2, 2, -1, 0, 1, -2, 2, -2, 2, -1, 0, 1, 0 };

static const int d_off_y_3[] =
{ -1, -1, -2, -2, -3, -3, -3, 0, 0, 1, 1, 2, 2,
  3, 3, 3, 0 };

static const int d_off_x_3[] =
{ -3, 3, -2, 2, -1, 0, 1, -3, 3, -3, 3, -2, 2,
  -1, 0, 1, 0 };

static const int d_off_y_4[] =
{ -1, -1, -2, -2, -3, -3, -3, -3, -4, -4, -4, 0,
  0, 1, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 0 };

static const int d_off_x_4[] =
{ -4, 4, -3, 3, -2, -3, 2, 3, -1, 0, 1, -4, 4,
  -4, 4, -3, 3, -2, -3, 2, 3, -1, 0, 1, 0 };

static const int d_off_y_5[] =
{ -1, -1, -2, -2, -3, -3, -4, -4, -4, -4, -5, -5,
  -5, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 4, 4, 5, 5,
  5, 0 };

static const int d_off_x_5[] =
{ -5, 5, -4, 4, -4, 4, -2, -3, 2, 3, -1, 0, 1,
  -5, 5, -5, 5, -4, 4, -4, 4, -2, -3, 2, 3, -1,
  0, 1, 0 };

static const int d_off_y_6[] =
{ -1, -1, -2, -2, -3, -3, -4, -4, -5, -5, -5, -5,
  -6, -6, -6, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5,
  5, 5, 6, 6, 6, 0 };

static const int d_off_x_6[] =
{ -6, 6, -5, 5, -5, 5, -4, 4, -2, -3, 2, 3, -1,
  0, 1, -6, 6, -6, 6, -5, 5, -5, 5, -4, 4, -2,
  -3, 2, 3, -1, 0, 1, 0 };

static const int d_off_y_7[] =
{ -1, -1, -2, -2, -3, -3, -4, -4, -5, -5, -5, -5,
  -6, -6, -6, -6, -7, -7, -7, 0, 0, 1, 1, 2, 2, 3,
  3, 4, 4, 5, 5, 5, 5, 6, 6, 6, 6, 7, 7, 7, 0 };

static const int d_off_x_7[] =
{ -7, 7, -6, 6, -6, 6, -5, 5, -4, -5, 4, 5, -2,
  -3, 2, 3, -1, 0, 1, -7, 7, -7, 7, -6, 6, -6,
  6, -5, 5, -4, -5, 4, 5, -2, -3, 2, 3, -1, 0,
  1, 0 };

static const int d_off_y_8[] =
{ -1, -1, -2, -2, -3, -3, -4, -4, -5, -5, -6, -6,
  -6, -6, -7, -7, -7, -7, -8, -8, -8, 0, 0, 1, 1,
  2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 6, 6, 7, 7, 7, 7,
  8, 8, 8, 0 };

static const int d_off_x_8[] =
{ -8, 8, -7, 7, -7, 7, -6, 6, -6, 6, -4, -5, 4,
  5, -2, -3, 2, 3, -1, 0, 1, -8, 8, -8, 8, -7,
  7, -7, 7, -6, 6, -6, 6, -4, -5, 4, 5, -2, -3,
  2, 3, -1, 0, 1, 0 };

static const int d_off_y_9[] =
{ -1, -1, -2, -2, -3, -3, -4, -4, -5, -5, -6, -6,
  -7, -7, -7, -7, -8, -8, -8, -8, -9, -9, -9, 0,
  0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 7,
  7, 8, 8, 8, 8, 9, 9, 9, 0 };

static const int d_off_x_9[] =
{ -9, 9, -8, 8, -8, 8, -7, 7, -7, 7, -6, 6, -4,
  -5, 4, 5, -2, -3, 2, 3, -1, 0, 1, -9, 9, -9,
  9, -8, 8, -8, 8, -7, 7, -7, 7, -6, 6, -4, -5,
  4, 5, -2, -3, 2, 3, -1, 0, 1, 0 };

const int *dist_offsets_y[10] =
{
    d_off_y_0, d_off_y_1, d_off_y_2, d_off_y_3, d_off_y_4,
    d_off_y_5, d_off_y_6, d_off_y_7, d_off_y_8, d_off_y_9
};

const int *dist_offsets_x[10] =
{
    d_off_x_0, d_off_x_1, d_off_x_2, d_off_x_3, d_off_x_4,
    d_off_x_5, d_off_x_6, d_off_x_7, d_off_x_8, d_off_x_9
};


/*
 * Given a central direction at position [dir #][0], return a series
 * of directions radiating out on both sides from the central direction
 * all the way back to its rear.
 *
 * Side directions come in pairs; for example, directions '1' and '3'
 * flank direction '2'. The code should know which side to consider
 * first. If the left, it must add 10 to the central direction to
 * access the second part of the table.
 */
const uint8_t side_dirs[20][8] =
{
    /* bias right */
    {0, 0, 0, 0, 0, 0, 0, 0},
    {1, 4, 2, 7, 3, 8, 6, 9},
    {2, 1, 3, 4, 6, 7, 9, 8},
    {3, 2, 6, 1, 9, 4, 8, 7},
    {4, 7, 1, 8, 2, 9, 3, 6},
    {5, 5, 5, 5, 5, 5, 5, 5},
    {6, 3, 9, 2, 8, 1, 7, 4},
    {7, 8, 4, 9, 1, 6, 2, 3},
    {8, 9, 7, 6, 4, 3, 1, 2},
    {9, 6, 8, 3, 7, 2, 4, 1},

    /* bias left */
    {0, 0, 0, 0, 0, 0, 0, 0},
    {1, 2, 4, 3, 7, 6, 8, 9},
    {2, 3, 1, 6, 4, 9, 7, 8},
    {3, 6, 2, 9, 1, 8, 4, 7},
    {4, 1, 7, 2, 8, 3, 9, 6},
    {5, 5, 5, 5, 5, 5, 5, 5},
    {6, 9, 3, 8, 2, 7, 1, 4},
    {7, 4, 8, 1, 9, 2, 6, 3},
    {8, 7, 9, 4, 6, 1, 3, 2},
    {9, 8, 6, 7, 3, 4, 2, 1}
};


/*
 * Given a "start" and "finish" location, extract a "direction",
 * which will move one step from the "start" towards the "finish".
 *
 * Note that we use "diagonal" motion whenever possible.
 *
 * We return "5" if no motion is needed.
 */
int motion_dir(struct loc *start, struct loc *finish)
{
    /* No movement required */
    if (loc_eq(start, finish)) return 5;

    /* South or North */
    if (start->x == finish->x) return ((start->y < finish->y)? 2: 8);

    /* East or West */
    if (start->y == finish->y) return ((start->x < finish->x)? 6: 4);

    /* South-east or South-west */
    if (start->y < finish->y) return ((start->x < finish->x)? 3: 1);

    /* North-east or North-west */
    if (start->y > finish->y) return ((start->x < finish->x)? 9: 7);

    /* Paranoia */
    return 5;
}


/*
 * Given a grid and a direction, extract the adjacent grid in that direction
 */
void next_grid(struct loc *next, struct loc *grid, int dir)
{
    loc_sum(next, grid, &ddgrid[dir]);
}


static const char *feat_code_list[] =
{
	#define FEAT(x) #x,
	#include "../common/list-terrain.h"
	#undef FEAT
	NULL
};


/*
 * Find a terrain feature by its code name.
 */
int lookup_feat_code(const char *code)
{
    int i;

    for (i = 0; feat_code_list[i]; i++)
    {
        if (streq(code, feat_code_list[i])) return i;
    }

    /* Non-feature: placeholder for player stores */
    if (streq(code, "STORE_PLAYER")) return FEAT_STORE_PLAYER;

    /* Backwards compatibility: find a terrain feature by its name. */
    for (i = 0; i < FEAT_MAX; i++)
    {
        struct feature *feat = &f_info[i];

        if (!feat->name) continue;
        if (streq(code, feat->name)) return i;
    }
    if (streq(code, "Player shop")) return FEAT_STORE_PLAYER;

    quit_fmt("Failed to find terrain feature %s", code);
    return -1;
}


/*
 * Allocate a new chunk of the world
 */
struct chunk *cave_new(int height, int width)
{
    struct loc grid;
    struct chunk *c = mem_zalloc(sizeof(*c));

    c->height = height;
    c->width = width;

    c->feat_count = mem_zalloc(FEAT_MAX * sizeof(int));

    c->squares = mem_zalloc(c->height * sizeof(struct square*));
    for (grid.y = 0; grid.y < c->height; grid.y++)
    {
        c->squares[grid.y] = mem_zalloc(c->width * sizeof(struct square));
        for (grid.x = 0; grid.x < c->width; grid.x++)
            square(c, &grid)->info = mem_zalloc(SQUARE_SIZE * sizeof(bitflag));
    }

    c->monsters = mem_zalloc(z_info->level_monster_max * sizeof(struct monster));
    c->mon_max = 1;

    c->monster_groups = mem_zalloc(z_info->level_monster_max * sizeof(struct monster_group*));

    c->o_gen = mem_zalloc(MAX_OBJECTS * sizeof(bool));
    c->join = mem_zalloc(sizeof(struct connector));

    return c;
}


/*
 * Free a chunk
 */
void cave_free(struct chunk *c)
{
    struct loc grid;

    for (grid.y = 0; grid.y < c->height; grid.y++)
    {
        for (grid.x = 0; grid.x < c->width; grid.x++)
        {
            mem_free(square(c, &grid)->info);
            if (square(c, &grid)->trap)
                square_free_trap(c, &grid);
            if (square(c, &grid)->obj)
                object_pile_free(square(c, &grid)->obj);
        }
        mem_free(c->squares[grid.y]);
    }
    mem_free(c->squares);

    mem_free(c->feat_count);
    mem_free(c->monsters);
    mem_free(c->monster_groups);
    mem_free(c->o_gen);
    mem_free(c->join);
    mem_free(c);
}


/*
 * Standard "find me a location" function, now with all legal outputs!
 *
 * Obtains a legal location within the given distance of the initial
 * location, and with "los()" from the source to destination location.
 *
 * This function is often called from inside a loop which searches for
 * locations while increasing the "d" distance.
 *
 * need_los determines whether line of sight is needed
 */
bool scatter(struct chunk *c, struct loc *place, struct loc *grid, int d, bool need_los)
{
    return (scatter_ext(c, place, 1, grid, d, need_los, NULL) != 0);
}


/*
 * Try to find a given number of distinct, randomly selected, locations that
 * are within a given distance of a grid, fully in bounds, and, optionally,
 * are in the line of sight of the given grid and satisfy an additional
 * condition.
 *
 * c Is the chunk to search.
 * places Points to the storage for the locations found. That storage
 * must have space for at least n grids.
 * n Is the number of locations to find.
 * grid Is the location to use as the origin for the search.
 * d Is the maximum distance, in grids, that a location can be from
 * grid and still be accepted.
 * need_los If true, any locations found will also be in the line of
 * sight from grid.
 * pred If not NULL, evaluating that function at a found location, lct,
 * will return true, i.e. (*pred)(c, lct) will be true.
 *
 * Returns the number of locations found. That number will be less
 * than or equal to n if n is not negative and will be zero if n is negative.
 */
int scatter_ext(struct chunk *c, struct loc *places, int n, struct loc *grid, int d, bool need_los,
    bool (*pred)(struct chunk *, struct loc *))
{
    int result = 0;

    /* Stores feasible locations. */
    struct loc *feas = mem_alloc(MIN(c->width, (1 + 2 * MAX(0, d))) *
        (size_t)MIN(c->height, (1 + 2 * MAX(0, d))) * sizeof(*feas));
    int nfeas = 0;
    struct loc g;

    /* Get the feasible locations. */
    for (g.y = grid->y - d; g.y <= grid->y + d; ++g.y)
    {
        for (g.x = grid->x - d; g.x <= grid->x + d; ++g.x)
        {
            if (!square_in_bounds_fully(c, &g)) continue;
            if ((d > 1) && (distance(grid, &g) > d)) continue;
            if (need_los && !los(c, grid, &g)) continue;
            if (pred && !(*pred)(c, &g)) continue;
            loc_copy(&feas[nfeas], &g);
            ++nfeas;
        }
    }

    /* Assemble the result. */
    while (result < n && nfeas > 0)
    {
        /* Choose one at random and append it to the outgoing list. */
        int choice = randint0(nfeas);

        loc_copy(&places[result], &feas[choice]);
        ++result;

        /* Shift the last feasible one to replace the one selected. */
        --nfeas;
        loc_copy(&feas[choice], &feas[nfeas]);
    }

    mem_free(feas);
    return result;
}


/*
 * Get a monster on the current level by its index.
 */
struct monster *cave_monster(struct chunk *c, int idx)
{
    /* Index MUST be valid */
    my_assert((idx >= 0) && (idx < c->mon_max));

    return &c->monsters[idx];
}


/*
 * The maximum number of monsters allowed in the level.
 */
int cave_monster_max(struct chunk *c)
{
    return c->mon_max;
}


/*
 * The current number of monsters present on the level.
 */
int cave_monster_count(struct chunk *c)
{
    return c->mon_cnt;
}


/*
 * Return the number of matching grids around (or under) the character.
 *
 * grid If not NULL, *grid is set to the location of the last match.
 * test Is the predicate to use when testing for a match.
 * under If true, the character's grid is tested as well.
 *
 * Only tests grids that are known and fully in bounds.
 */
int count_feats(struct player *p, struct chunk *c, struct loc *grid,
    bool (*test)(struct chunk *c, struct loc *grid), bool under)
{
    int d;

    /* Count how many matches */
    int count = 0;

    /* Check around (and under) the character */
    for (d = 0; d < 9; d++)
    {
        struct loc adjacent;

        /* If not searching under player continue */
        if ((d == 8) && !under) continue;

        /* Extract adjacent (legal) location */
        loc_sum(&adjacent, &p->grid, &ddgrid_ddd[d]);

        /* Paranoia */
        if (!square_in_bounds_fully(c, &adjacent)) continue;

        /* Must have knowledge */
        if (!square_isknown(p, &adjacent)) continue;

        /* Not looking for this feature */
        if (!((*test)(c, &adjacent))) continue;

        /* Count it */
        ++count;

        /* Remember the location of the last match */
        if (grid) loc_copy(grid, &adjacent);
    }

    /* All done */
    return count;
}


/*
 * Return the number of matching grids around a location.
 *
 * match If not NULL, *match is set to the location of the last match.
 * grid Is the location whose neighbors will be tested.
 * test Is the predicate to use when testing for a match.
 * under If true, grid is tested as well.
 */
int count_neighbors(struct loc *match, struct chunk *c, struct loc *grid,
    bool (*test)(struct chunk *c, struct loc *grid), bool under)
{
    int dlim = (under? 9: 8);
    int d;

    /* Count how many matches */
    int count = 0;

    /* Check the grid's neighbors and, if under is true, grid */
    for (d = 0; d < dlim; d++)
    {
        struct loc adjacent;

        /* Extract adjacent (legal) location */
        loc_sum(&adjacent, grid, &ddgrid_ddd[d]);
        if (!square_in_bounds(c, &adjacent)) continue;

        /* Reject those that don't match */
        if (!((*test)(c, &adjacent))) continue;

        /* Count it */
        ++count;

        /* Remember the location of the last match */
        if (match) loc_copy(match, &adjacent);
    }

    /* All done */
    return count;
}


struct loc *cave_find_decoy(struct chunk *c)
{
    return &c->decoy;
}


/*
 * Update the visuals
 */
void update_visuals(struct worldpos *wpos)
{
    int i;

    /* Check everyone */
    for (i = 1; i <= NumPlayers; i++)
    {
        struct player *p = player_get(i);

        /* If he's not here, skip him */
        if (!wpos_eq(&p->wpos, wpos)) continue;

        /* Update the visuals */
        p->upkeep->update |= (PU_UPDATE_VIEW | PU_MONSTERS);
    }
}


/*
 * Note changes to viewable region
 */
void note_viewable_changes(struct worldpos *wpos, struct loc *grid)
{
    int i;

    /* Check everyone */
    for (i = 1; i <= NumPlayers; i++)
    {
        struct player *p = player_get(i);

        /* If he's not here, skip him */
        if (!wpos_eq(&p->wpos, wpos)) continue;

        /* Note changes to viewable region */
        if (square_isview(p, grid)) p->upkeep->update |= (PU_UPDATE_VIEW | PU_MONSTERS);
    }
}


/*
 * Fully update the flow
 */
void fully_update_flow(struct worldpos *wpos)
{
    int i;

    /* Check everyone */
    for (i = 1; i <= NumPlayers; i++)
    {
        struct player *p = player_get(i);

        /* If he's not here, skip him */
        if (!wpos_eq(&p->wpos, wpos)) continue;
    }
}


/*
 * Display the full map of the dungeon in the active Term.
 */
void display_fullmap(struct player *p)
{
    struct loc grid;
    struct chunk *cv = chunk_get(&p->wpos);

    /* Dump the map */
    for (grid.y = 0; grid.y < z_info->dungeon_hgt; grid.y++)
    {
        /* First clear the old stuff */
        for (grid.x = 0; grid.x < z_info->dungeon_wid; grid.x++)
        {
            p->scr_info[grid.y][grid.x].c = 0;
            p->scr_info[grid.y][grid.x].a = 0;
            p->trn_info[grid.y][grid.x].c = 0;
            p->trn_info[grid.y][grid.x].a = 0;
        }

        /* Scan the columns of row "y" */
        for (grid.x = 0; grid.x < z_info->dungeon_wid; grid.x++)
        {
            uint16_t a, ta;
            char c, tc;
            struct grid_data g;

            /* Check bounds */
            if (!square_in_bounds(cv, &grid)) continue;

            /* Determine what is there */
            map_info(p, cv, &grid, &g);
            grid_data_as_text(p, cv, false, &g, &a, &c, &ta, &tc);

            p->scr_info[grid.y][grid.x].c = c;
            p->scr_info[grid.y][grid.x].a = a;
            p->trn_info[grid.y][grid.x].c = tc;
            p->trn_info[grid.y][grid.x].a = ta;
        }

        /* Send that line of info */
        Send_fullmap(p, grid.y);
    }

    /* Reset the line counter */
    Send_fullmap(p, -1);
}


/*  
 * Update the cursors for anyone tracking a monster (or player)
 */
void update_cursor(struct source *who)
{
    int i;

    /* Each player */
    for (i = 1; i <= NumPlayers; i++)
    {
        struct player *p = player_get(i);
        struct source *cursor_who = &p->cursor_who;

        /* See if he is tracking this monster (or player) */
        if (source_equal(cursor_who, who))
        {
            /* Redraw */
            p->upkeep->redraw |= (PR_CURSOR);
        }
    }
}


/*
 * Update the health bars for anyone tracking a monster (or player)
 */
void update_health(struct source *who)
{
    int i;

    /* Each player */
    for (i = 1; i <= NumPlayers; i++)
    {
        struct player *p = player_get(i);
        struct source *health_who = &p->upkeep->health_who;

        /* See if he is tracking this monster (or player) */
        if (source_equal_player_or_monster(health_who, who))
        {
            /* Redraw */
            p->upkeep->redraw |= (PR_HEALTH);
        }
    }
}


static void place_feature(struct player *p, struct chunk *c, int cur_feat)
{
    /* Can only place a staircase once */
    if ((cur_feat == FEAT_LESS) && (c->join->down.y || c->join->down.x))
    {
        msg(p, "There is already an up staircase on this level!");
        return;
    }
    if ((cur_feat == FEAT_MORE) && (c->join->up.y || c->join->up.x))
    {
        msg(p, "There is already a down staircase on this level!");
        return;
    }

    /* Remove a staircase */
    if (square_isupstairs(c, &p->grid))
        square_init_join_down(c);
    if (square_isdownstairs(c, &p->grid))
        square_init_join_up(c);

    /* Place a staircase */
    if (cur_feat == FEAT_LESS) square_set_upstairs(c, &p->grid);
    else if (cur_feat == FEAT_MORE) square_set_downstairs(c, &p->grid, FEAT_MORE);

    /* Place any other feature */
    else square_set_feat(c, &p->grid, cur_feat);
}


static void get_rectangle(struct chunk *c, struct loc *grid0, struct loc *gridmax)
{
    int y, x;

    /* Find the width of the rectangle to fill */
    for (x = grid0->x; x < gridmax->x; x++)
    {
        struct loc grid;

        loc_init(&grid, x, grid0->y);

        /* Require a "clean" floor grid */
        if (!square_canputitem(c, &grid))
        {
            if (x < gridmax->x) gridmax->x = x;
            break;
        }
    }

    /* Find the height of the rectangle to fill */
    for (y = grid0->y; y < gridmax->y; y++)
    {
        struct loc grid;

        loc_init(&grid, grid0->x, y);

        /* Require a "clean" floor grid */
        if (!square_canputitem(c, &grid))
        {
            if (y < gridmax->y) gridmax->y = y;
            break;
        }
    }
}


/*
 * The dungeon master movement hook, called whenever he moves
 * (to make building large buildings / summoning hoards of monsters easier)
 */
void (*master_move_hook)(struct player *p, char *args) = NULL;


static int get_feat_byfuzzyname(char *name)
{
    int i;
    char buf[NORMAL_WID];
    char *str;

    /* Lowercase our search string */
    for (str = name; *str; str++) *str = tolower((unsigned char)*str);

    for (i = 0; i < FEAT_MAX; i++)
    {
        struct feature *feat = &f_info[i];

        /* Clean up name */
        clean_name(buf, feat->name);

        /* If cleaned name matches our search string, return it */
        if (strstr(buf, name)) return i;
    }

    return -1;
}


/*
 * Build walls and such
 */
void master_build(struct player *p, char *parms)
{
    static int cur_feat = 0;
    struct chunk *c = chunk_get(&p->wpos);

    /* Paranoia -- make sure the player is on a valid level */
    if (!c) return;

    if (!cur_feat) cur_feat = FEAT_FLOOR;

    /* Place a feature at the player's location */
    if (!parms)
    {
        place_feature(p, c, cur_feat);
        return;
    }

    switch (parms[0])
    {
        /* Set Feature */
        case 'i':
        {
            int feat = get_feat_byfuzzyname(&parms[1]);

            /* Unknown or unauthorized features */
            if (feat == -1) break;
            if (feat_ismetamap(feat)) break;

            cur_feat = feat;
            break;
        }

        /* Place Feature */
        case 'f':
        {
            place_feature(p, c, cur_feat);
            break;
        }

        /* Draw Line */
        case 'l':
        {
            int dir = (int)parms[1];

            /* No lines of staircases */
            if ((cur_feat == FEAT_LESS) || (cur_feat == FEAT_MORE)) break;

            /* No lines of shops */
            if (feat_is_shop(cur_feat)) break;

            /* No lines of house doors */
            if (feat_ishomedoor(cur_feat)) break;

            /* Draw a line if we have a valid direction */
            if (dir && (dir != 5) && VALID_DIR(dir))
            {
                struct loc grid;

                loc_copy(&grid, &p->grid);

                /* Require a "clean" floor grid */
                while (square_canputitem(c, &grid))
                {
                    /* Set feature */
                    square_set_feat(c, &grid, cur_feat);

                    /* Update the visuals */
                    update_visuals(&p->wpos);

                    /* Use the given direction */
                    grid.x += ddx[dir];
                    grid.y += ddy[dir];
                }
            }

            break;
        }

        /* Fill Rectangle */
        case 'r':
        {
            struct loc begin, end;
            struct loc_iterator iter;

            /* No rectangles of staircases */
            if ((cur_feat == FEAT_LESS) || (cur_feat == FEAT_MORE)) break;

            /* No rectangles of shops */
            if (feat_is_shop(cur_feat)) break;

            /* No rectangles of house doors */
            if (feat_ishomedoor(cur_feat)) break;

            /* Find the width and height of the rectangle to fill */
            loc_copy(&begin, &p->grid);
            loc_init(&end, c->width - 1, c->height - 1);
            while ((begin.y < end.y) && (begin.x < end.x))
            {
                get_rectangle(c, &begin, &end);
                begin.y++; begin.x++;
            }

            loc_iterator_first(&iter, &p->grid, &end);

            /* Fill rectangle */
            do
            {
                /* Set feature */
                square_set_feat(c, &iter.cur, cur_feat);

                /* Update the visuals */
                update_visuals(&p->wpos);
            }
            while (loc_iterator_next_strict(&iter));

            break;
        }

        /* Build mode on */
        case 'm':
        {
            master_move_hook = master_build;
            break;
        }

        /* Build mode off */
        case 'x':
        {
            master_move_hook = NULL;
            break;
        }
        
        /* Wipe unowned custom houses */
        case 'w':
        {
            wipe_custom_houses(&p->wpos);
            break;
        }        
    }
}


void fill_dirt(struct chunk *c, struct loc *grid1, struct loc *grid2)
{
    struct loc_iterator iter;

    loc_iterator_first(&iter, grid1, grid2);

    do
    {
        square_set_feat(c, &iter.cur, FEAT_LOOSE_DIRT);
    }
    while (loc_iterator_next(&iter));
}


void add_crop(struct chunk *c, struct loc *grid1, struct loc *grid2, int orientation)
{
    struct loc_iterator iter;

    loc_iterator_first(&iter, grid1, grid2);

    do
    {
        /* Different orientations */
        if ((!orientation && (iter.cur.y % 2)) || (orientation && (iter.cur.x % 2)))
        {
            /* Set to crop */
            square_set_feat(c, &iter.cur, FEAT_CROP);
        }
    }
    while (loc_iterator_next(&iter));
}


int add_building(struct chunk *c, struct loc *grid1, struct loc *grid2, int type)
{
    int floor_feature = FEAT_FLOOR, wall_feature = 0, door_feature = 0;
    bool lit_room = true;
    struct loc_iterator iter;
    char wall_type = '\0';  // second part of floor name (type) for rng walls
    int wall_id = 0;  // special wall (when we have several different walls in one row)

    /* Select features */
    switch (type)
    {
        case WILD_LOG_CABIN:
        {
            wall_feature = FEAT_LOGS;
            door_feature = FEAT_CLOSED;
            lit_room = false;

            break;
        }

        case WILD_TOWN_HOME:
        {
            wall_feature = FEAT_PERM_HOUSE;
            door_feature = FEAT_HOME_CLOSED;
            floor_feature = FEAT_FLOOR_SAFE;

            break;
        }

        case WILD_ARENA:
        {
            wall_feature = FEAT_PERM_ARENA;
            door_feature = FEAT_PERM_ARENA;

            break;
        }
    }

    loc_iterator_first(&iter, grid1, grid2);
    
    /* Wall type for building rng walls */

    if      (one_in_(9)) wall_type = 'a';  // B7 B8 wood
    else if (one_in_(9)) wall_type = 'b';  // B9 BA small black
    else if (one_in_(9)) wall_type = 'c';  // BB BC big white
    else if (one_in_(9)) wall_type = 'd';  // BD BE big black
    else if (one_in_(9)) wall_type = 'e';  // BF C0 small white
    else if (one_in_(2)) wall_type = 'f';  // 96 98
    else if (one_in_(2)) wall_type = 'g';  // A3 AA
    else if (one_in_(9)) wall_type = 'h';  // DC E1
    else if (one_in_(9)) wall_type = 'i';  // E2 E3
    else wall_type = 'b';

    /* Generate special wall id: starting from 1!  */
    wall_id = randint1(12);
    
    /* Build a rectangular building */
    do
    {
        /* Clear previous contents, add "basic" wall */
        square_build_new_permhouse(c, &iter.cur, wall_type, wall_id);
    ///////// old: square_set_feat(c, &iter.cur, wall_feature);
    }
    while (loc_iterator_next(&iter));

    grid1->x++;
    grid1->y++;
    loc_iterator_first(&iter, grid1, grid2);

    /* Make it hollow */
    do
    {
        /* Fill with floor */
        square_add_new_safe(c, &iter.cur);
    ///////// square_set_feat(c, &iter.cur, floor_feature);

        /* Make it "icky" */
        sqinfo_on(square(c, &iter.cur)->info, SQUARE_VAULT);

        /* Make it glowing */
        if (lit_room)
        {
            sqinfo_on(square(c, &iter.cur)->info, SQUARE_ROOM);
            sqinfo_on(square(c, &iter.cur)->info, SQUARE_GLOW);
        }
    }
    while (loc_iterator_next_strict(&iter));

    grid1->x--;
    grid1->y--;

    return door_feature;
}


void add_moat(struct chunk *c, struct loc *grid1, struct loc *grid2, struct loc drawbridge[3])
{
    int y, x;
    struct loc grid;

    /* North / South */
    for (x = grid1->x - 2; x <= grid2->x + 2; x++)
    {
        loc_init(&grid, x, grid1->y - 2);
        square_set_feat(c, &grid, FEAT_WATER);
        sqinfo_on(square(c, &grid)->info, SQUARE_VAULT);
        loc_init(&grid, x, grid1->y - 3);
        square_set_feat(c, &grid, FEAT_WATER);
        sqinfo_on(square(c, &grid)->info, SQUARE_VAULT);
        loc_init(&grid, x, grid2->y + 2);
        square_set_feat(c, &grid, FEAT_WATER);
        sqinfo_on(square(c, &grid)->info, SQUARE_VAULT);
        loc_init(&grid, x, grid2->y + 3);
        square_set_feat(c, &grid, FEAT_WATER);
        sqinfo_on(square(c, &grid)->info, SQUARE_VAULT);
    }

    /* East / West */
    for (y = grid1->y - 2; y <= grid2->y + 2; y++)
    {
        loc_init(&grid, grid1->x - 2, y);
        square_set_feat(c, &grid, FEAT_WATER);
        sqinfo_on(square(c, &grid)->info, SQUARE_VAULT);
        loc_init(&grid, grid1->x - 3, y);
        square_set_feat(c, &grid, FEAT_WATER);
        sqinfo_on(square(c, &grid)->info, SQUARE_VAULT);
        loc_init(&grid, grid2->x + 2, y);
        square_set_feat(c, &grid, FEAT_WATER);
        sqinfo_on(square(c, &grid)->info, SQUARE_VAULT);
        loc_init(&grid, grid2->x + 3, y);
        square_set_feat(c, &grid, FEAT_WATER);
        sqinfo_on(square(c, &grid)->info, SQUARE_VAULT);
    }
    square_set_feat(c, &drawbridge[0], FEAT_DRAWBRIDGE);
    sqinfo_on(square(c, &drawbridge[0])->info, SQUARE_VAULT);
    square_set_feat(c, &drawbridge[1], FEAT_DRAWBRIDGE);
    sqinfo_on(square(c, &drawbridge[1])->info, SQUARE_VAULT);
    square_set_feat(c, &drawbridge[2], FEAT_DRAWBRIDGE);
    sqinfo_on(square(c, &drawbridge[2])->info, SQUARE_VAULT);
}


/* Player images for graphic mode */
struct preset *presets;
int presets_count;
