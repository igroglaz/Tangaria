/*
 * File: cave-square.c
 * Purpose: Functions for dealing with individual squares
 *
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
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
 * FEATURE PREDICATES
 *
 * These functions test a terrain feature index for the obviously described
 * type. They are used in the square feature predicates below, and
 * occasionally on their own
 */


/*
 * True if the square is a magma wall.
 */
bool feat_is_magma(int feat)
{
    return tf_has(f_info[feat].flags, TF_MAGMA);
}


/*
 * True if the square is a quartz wall.
 */
bool feat_is_quartz(int feat)
{
    return tf_has(f_info[feat].flags, TF_QUARTZ);
}


/*
 * True if the square is a mineral wall with treasure (magma/quartz).
 */
bool feat_is_treasure(int feat)
{
    return (tf_has(f_info[feat].flags, TF_GOLD) && tf_has(f_info[feat].flags, TF_INTERESTING));
}


/*
 * True is the feature is a solid wall (not rubble).
 */
bool feat_is_wall(int feat)
{
    return tf_has(f_info[feat].flags, TF_WALL);
}


/*
 * True is the feature is a floor.
 */
bool feat_is_floor(int feat)
{
    return tf_has(f_info[feat].flags, TF_FLOOR);
}


/*
 * True is the feature can hold a trap.
 */
bool feat_is_trap_holding(int feat)
{
    return tf_has(f_info[feat].flags, TF_TRAP);
}


/*
 * True is the feature can hold an object.
 */
bool feat_is_object_holding(int feat)
{
    return tf_has(f_info[feat].flags, TF_OBJECT);
}


/*
 * True if a monster can walk through the feature.
 */
bool feat_is_monster_walkable(int feat)
{
    return (tf_has(f_info[feat].flags, TF_PASSABLE) && !tf_has(f_info[feat].flags, TF_NO_GENERATION));
}


/*
 * True if the feature is a shop entrance.
 */
bool feat_is_shop(int feat)
{
    return tf_has(f_info[feat].flags, TF_SHOP);
}


/*
 * True if the feature allows line-of-sight.
 */
bool feat_is_los(int feat)
{
    return tf_has(f_info[feat].flags, TF_LOS);
}


/*
 * True if the feature is passable by the player.
 */
bool feat_is_passable(int feat)
{
    return tf_has(f_info[feat].flags, TF_PASSABLE);
}


/*
 * True if any projectable can pass through the feature.
 */
bool feat_is_projectable(int feat)
{
    return tf_has(f_info[feat].flags, TF_PROJECT);
}


/*
 * True if the feature can be lit by light sources.
 */
bool feat_is_torch(int feat)
{
    return tf_has(f_info[feat].flags, TF_TORCH);
}


/*
 * True if the feature is internally lit.
 */
bool feat_is_bright(int feat)
{
    return tf_has(f_info[feat].flags, TF_BRIGHT);
}


/*
 * True if the feature is fire-based.
 */
bool feat_is_fiery(int feat)
{
    return tf_has(f_info[feat].flags, TF_FIERY);
}


/*
 * True if the feature doesn't carry monster flow information.
 */
bool feat_is_no_flow(int feat)
{
    return tf_has(f_info[feat].flags, TF_NO_FLOW);
}


/*
 * True if the feature doesn't carry player scent.
 */
bool feat_is_no_scent(int feat)
{
    return tf_has(f_info[feat].flags, TF_NO_SCENT);
}


/*
 * True if the feature should have smooth boundaries (for dungeon generation).
 */
bool feat_is_smooth(int feat)
{
    return tf_has(f_info[feat].flags, TF_SMOOTH);
}


bool feat_issafefloor(int feat)
{
    return tf_has(f_info[feat].flags, TF_FLOOR_SAFE);
}


bool feat_isterrain(int feat)
{
    return !tf_has(f_info[feat].flags, TF_FLOOR);
}


int feat_order_special(int feat)
{
    if ((feat == FEAT_WATER) || (feat == FEAT_MUD) || (feat == FEAT_DRAWBRIDGE) ||
        (feat == FEAT_LOOSE_DIRT) || tf_has(f_info[feat].flags, TF_CROP) || (feat == FEAT_MOUNTAIN))
    {
        return 0;
    }

    if ((feat == FEAT_TREE) || (feat == FEAT_EVIL_TREE))
        return 5;

    if ((feat == FEAT_SWAMP) || (feat == FEAT_DEEP_WATER) || (feat == FEAT_HILL) ||
        (feat == FEAT_SHORE))
    {
        return 7;
    }

    return -1;
}


int feat_pseudo(char d_char)
{
    switch (d_char)
    {
        case '+': return FEAT_CLOSED;
        case '<': return FEAT_LESS;
        case '>': return FEAT_MORE;
    }

    return FEAT_FLOOR;
}


bool feat_ishomedoor(int feat)
{
    return tf_has(f_info[feat].flags, TF_DOOR_HOME);
}


bool feat_isperm(int feat)
{
    return (tf_has(f_info[feat].flags, TF_PERMANENT) && tf_has(f_info[feat].flags, TF_ROCK));
}


bool feat_ismetamap(int feat)
{
    return tf_has(f_info[feat].flags, TF_METAMAP);
}


/*
 * SQUARE FEATURE PREDICATES
 *
 * These functions are used to figure out what kind of square something is,
 * via c->squares[y][x].feat (preferably accessed via square(c, grid)).
 * All direct testing of square(c, grid).feat should be rewritten
 * in terms of these functions.
 *
 * It's often better to use square behavior predicates (written in terms of
 * these functions) instead of these functions directly. For instance,
 * square_isrock() will return false for a secret door, even though it will
 * behave like a rock wall until the player determines it's a door.
 *
 * Use functions like square_isdiggable, square_allowslos, etc. in these cases.
 */


/*
 * True if the square is normal open floor.
 */
bool square_isfloor(struct chunk *c, struct loc *grid)
{
    return feat_is_floor(square(c, grid)->feat);
}


/*
 * True if the square is safe floor.
 */
bool square_issafefloor(struct chunk *c, struct loc *grid)
{
    return feat_issafefloor(square(c, grid)->feat);
}


/*
 * True if the square is pit floor.
 */
bool square_ispitfloor(struct chunk *c, struct loc *grid)
{
    return (tf_has(f_info[square(c, grid)->feat].flags, TF_FLOOR) &&
        tf_has(f_info[square(c, grid)->feat].flags, TF_PIT));
}


/*
 * True if the square is other floor.
 */
bool square_isotherfloor(struct chunk *c, struct loc *grid)
{
    return tf_has(f_info[square(c, grid)->feat].flags, TF_FLOOR_OTHER);
}


bool square_isanyfloor(struct chunk *c, struct loc *grid)
{
    return (square_isfloor(c, grid) || square_issafefloor(c, grid) || square_isotherfloor(c, grid));
}


/*
 * True if the square can hold a trap.
 */
bool square_istrappable(struct chunk *c, struct loc *grid)
{
    return feat_is_trap_holding(square(c, grid)->feat);
}


/*
 * True if the square can hold an object.
 */
bool square_isobjectholding(struct chunk *c, struct loc *grid)
{
    return feat_is_object_holding(square(c, grid)->feat);
}


/*
 * True if the square is a normal granite rock wall.
 */
bool square_isrock(struct chunk *c, struct loc *grid)
{
    return (tf_has(f_info[square(c, grid)->feat].flags, TF_GRANITE) &&
        !tf_has(f_info[square(c, grid)->feat].flags, TF_DOOR_ANY));
}


/*
 * True if the square is a permanent wall.
 */
bool square_isperm(struct chunk *c, struct loc *grid)
{
    return (tf_has(f_info[square(c, grid)->feat].flags, TF_PERMANENT) &&
        tf_has(f_info[square(c, grid)->feat].flags, TF_ROCK));
}


/*
 * True if the square is a permanent wall.
 */
bool square_isperm_p(struct player *p, struct loc *grid)
{
    return (tf_has(f_info[square_p(p, grid)->feat].flags, TF_PERMANENT) &&
        tf_has(f_info[square_p(p, grid)->feat].flags, TF_ROCK));
}


bool square_isunpassable(struct chunk *c, struct loc *grid)
{
    return (square_isperm(c, grid) || square_ispermfake(c, grid));
}


bool square_isborder(struct chunk *c, struct loc *grid)
{
    return tf_has(f_info[square(c, grid)->feat].flags, TF_BORDER);
}


bool square_ispermborder(struct chunk *c, struct loc *grid)
{
    return (square_isunpassable(c, grid) || square_isborder(c, grid));
}


bool square_ispermarena(struct chunk *c, struct loc *grid)
{
    return tf_has(f_info[square(c, grid)->feat].flags, TF_ARENA);
}


bool square_ispermhouse(struct chunk *c, struct loc *grid)
{
    return (square(c, grid)->feat == FEAT_PERM_HOUSE);
}

bool square_is_new_permhouse(struct chunk *c, struct loc *grid)
{
    return tf_has(f_info[square(c, grid)->feat].flags, TF_HOUSE);
}


bool square_ispermstatic(struct chunk *c, struct loc *grid)
{
    return (square(c, grid)->feat == FEAT_PERM_STATIC);
}


bool square_ispermfake(struct chunk *c, struct loc *grid)
{
    my_assert(square_in_bounds(c, grid));

    return sqinfo_has(square(c, grid)->info, SQUARE_FAKE);
}


/*
 * True if the square is a magma wall.
 */
bool square_ismagma(struct chunk *c, struct loc *grid)
{
    return feat_is_magma(square(c, grid)->feat);
}


/*
 * True if the square is a quartz wall.
 */
bool square_isquartz(struct chunk *c, struct loc *grid)
{
    return feat_is_quartz(square(c, grid)->feat);
}


/*
 * True if the square is a mineral wall (other)
 */
bool square_ismineral_other(struct chunk *c, struct loc *grid)
{
    return (tf_has(f_info[square(c, grid)->feat].flags, TF_SAND) ||
        tf_has(f_info[square(c, grid)->feat].flags, TF_ICE) ||
        tf_has(f_info[square(c, grid)->feat].flags, TF_DARK));
}

// True if the square is sand
bool square_is_sand(struct chunk *c, struct loc *grid)
{
    return (tf_has(f_info[square(c, grid)->feat].flags, TF_SAND));
}

// True if the square is ice
bool square_is_ice(struct chunk *c, struct loc *grid)
{
    return (tf_has(f_info[square(c, grid)->feat].flags, TF_ICE));
}

/*
 * True if the square is a mineral wall.
 */
bool square_ismineral(struct chunk *c, struct loc *grid)
{
    return (square_isrock(c, grid) || square_ismagma(c, grid) || square_isquartz(c, grid) ||
        square_ismineral_other(c, grid));
}


bool square_hasgoldvein(struct chunk *c, struct loc *grid)
{
    return tf_has(f_info[square(c, grid)->feat].flags, TF_GOLD);
}


bool square_hasgoldvein_p(struct player *p, struct loc *grid)
{
    return tf_has(f_info[square_p(p, grid)->feat].flags, TF_GOLD);
}


/*
 * True if the square is rubble.
 */
bool square_isrubble(struct chunk *c, struct loc *grid)
{
    return (!tf_has(f_info[square(c, grid)->feat].flags, TF_WALL) &&
        tf_has(f_info[square(c, grid)->feat].flags, TF_ROCK));
}


/*
 * True if the square is rubble.
 */
bool square_isrubble_p(struct player *p, struct loc *grid)
{
    return (!tf_has(f_info[square_p(p, grid)->feat].flags, TF_WALL) &&
        tf_has(f_info[square_p(p, grid)->feat].flags, TF_ROCK));
}


/*
 * True if the square is hard rubble.
 */
bool square_ishardrubble(struct chunk *c, struct loc *grid)
{
    return ((square(c, grid)->feat == FEAT_HARD_RUBBLE) ||
        (square(c, grid)->feat == FEAT_HARD_PASS_RUBBLE));
}


/*
 * True if the square is a hidden secret door.
 *
 * These squares appear as if they were granite -- when detected a secret door
 * is replaced by a closed door.
 */
bool square_issecretdoor(struct chunk *c, struct loc *grid)
{
    return (tf_has(f_info[square(c, grid)->feat].flags, TF_DOOR_ANY) &&
        tf_has(f_info[square(c, grid)->feat].flags, TF_ROCK));
}


/*
 * True if the square is an open door.
 */
bool square_isopendoor(struct chunk *c, struct loc *grid)
{
    return (tf_has(f_info[square(c, grid)->feat].flags, TF_CLOSABLE));
}


/*
 * True if the square is an open door.
 */
bool square_isopendoor_p(struct player *p, struct loc *grid)
{
    return (tf_has(f_info[square_p(p, grid)->feat].flags, TF_CLOSABLE));
}


bool square_home_isopendoor(struct chunk *c, struct loc *grid)
{
    return (tf_has(f_info[square(c, grid)->feat].flags, TF_DOOR_HOME) &&
        tf_has(f_info[square(c, grid)->feat].flags, TF_CLOSABLE));
}


/*
 * True if the square is a closed door.
 */
bool square_iscloseddoor(struct chunk *c, struct loc *grid)
{
    return tf_has(f_info[square(c, grid)->feat].flags, TF_DOOR_CLOSED);
}


/*
 * True if the square is a closed door.
 */
bool square_iscloseddoor_p(struct player *p, struct loc *grid)
{
    return tf_has(f_info[square_p(p, grid)->feat].flags, TF_DOOR_CLOSED);
}


bool square_basic_iscloseddoor(struct chunk *c, struct loc *grid)
{
    return (tf_has(f_info[square(c, grid)->feat].flags, TF_DOOR_CLOSED) &&
        !tf_has(f_info[square(c, grid)->feat].flags, TF_DOOR_HOME));
}


bool square_home_iscloseddoor(struct chunk *c, struct loc *grid)
{
    return (tf_has(f_info[square(c, grid)->feat].flags, TF_DOOR_CLOSED) &&
        tf_has(f_info[square(c, grid)->feat].flags, TF_DOOR_HOME));
}


bool square_isbrokendoor(struct chunk *c, struct loc *grid)
{
    return (tf_has(f_info[square(c, grid)->feat].flags, TF_DOOR_ANY) &&
        tf_has(f_info[square(c, grid)->feat].flags, TF_PASSABLE) &&
        !tf_has(f_info[square(c, grid)->feat].flags, TF_CLOSABLE));
}


bool square_isbrokendoor_p(struct player *p, struct loc *grid)
{
    return (tf_has(f_info[square_p(p, grid)->feat].flags, TF_DOOR_ANY) &&
        tf_has(f_info[square_p(p, grid)->feat].flags, TF_PASSABLE) &&
        !tf_has(f_info[square_p(p, grid)->feat].flags, TF_CLOSABLE));
}


/*
 * True if the square is a door.
 *
 * This includes open, closed, broken, and hidden doors.
 */
bool square_isdoor(struct chunk *c, struct loc *grid)
{
    return tf_has(f_info[square(c, grid)->feat].flags, TF_DOOR_ANY);
}


/*
 * True if the square is a door.
 *
 * This includes open, closed, broken, and hidden doors.
 */
bool square_isdoor_p(struct player *p, struct loc *grid)
{
    return tf_has(f_info[square_p(p, grid)->feat].flags, TF_DOOR_ANY);
}


/*
 * True if cave is an up or down stair
 */
bool square_isstairs(struct chunk *c, struct loc *grid)
{
    return tf_has(f_info[square(c, grid)->feat].flags, TF_STAIR);
}


/*
 * True if cave is an up or down stair
 */
bool square_isstairs_p(struct player *p, struct loc *grid)
{
    return tf_has(f_info[square_p(p, grid)->feat].flags, TF_STAIR);
}


/*
 * True if cave is an up stair.
 */
bool square_isupstairs(struct chunk *c, struct loc *grid)
{
    return tf_has(f_info[square(c, grid)->feat].flags, TF_UPSTAIR);
}


/*
 * True if cave is a down stair.
 */
bool square_isdownstairs(struct chunk *c, struct loc *grid)
{
    return tf_has(f_info[square(c, grid)->feat].flags, TF_DOWNSTAIR);
}


/*
 * True if the square is a shop entrance.
 */
bool square_isshop(struct chunk *c, struct loc *grid)
{
    return feat_is_shop(square(c, grid)->feat);
}


bool square_noticeable(struct chunk *c, struct loc *grid)
{
    if (square_isvisibletrap(c, grid)) return true;

    return (tf_has(f_info[square(c, grid)->feat].flags, TF_INTERESTING) ||
        tf_has(f_info[square(c, grid)->feat].flags, TF_NOTICEABLE));
}


/*
 * True if the square contains a player
 */
bool square_isplayer(struct chunk *c, struct loc *grid)
{
    return ((square(c, grid)->mon < 0)? true: false);
}


/*
 * True if the square contains a player or a monster
 */
bool square_isoccupied(struct chunk *c, struct loc *grid)
{
    return ((square(c, grid)->mon != 0)? true: false);
}


/*
 * True if the player knows the terrain of the square
 */
bool square_isknown(struct player *p, struct loc *grid)
{
    return ((square_p(p, grid)->feat != FEAT_NONE) || (p->dm_flags & DM_SEE_LEVEL));
}


/*
 * True if the player's knowledge of the terrain of the square is wrong
 * or missing
 */
bool square_ismemorybad(struct player *p, struct chunk *c, struct loc *grid)
{
    return (!square_isknown(p, grid) || (square_p(p, grid)->feat != square(c, grid)->feat));
}


/*
 * True if the square is marked
 */
bool square_ismark(struct player *p, struct loc *grid)
{
    my_assert(player_square_in_bounds(p, grid));

    return sqinfo_has(square_p(p, grid)->info, SQUARE_MARK);
}


bool square_istree(struct chunk *c, struct loc *grid)
{
    return tf_has(f_info[square(c, grid)->feat].flags, TF_TREE);
}


bool square_istree_p(struct player *p, struct loc *grid)
{
    return tf_has(f_info[square_p(p, grid)->feat].flags, TF_TREE);
}


bool square_isstrongtree(struct chunk *c, struct loc *grid)
{
    return (tf_has(f_info[square(c, grid)->feat].flags, TF_TREE) &&
        !tf_has(f_info[square(c, grid)->feat].flags, TF_WITHERED));
}


bool square_iswitheredtree(struct chunk *c, struct loc *grid)
{
    return (tf_has(f_info[square(c, grid)->feat].flags, TF_TREE) &&
        tf_has(f_info[square(c, grid)->feat].flags, TF_WITHERED));
}


bool square_isdirt(struct chunk *c, struct loc *grid)
{
    return tf_has(f_info[square(c, grid)->feat].flags, TF_DIRT);
}


bool square_isgrass(struct chunk *c, struct loc *grid)
{
    return tf_has(f_info[square(c, grid)->feat].flags, TF_GRASS);
}


bool square_iscropbase(struct chunk *c, struct loc *grid)
{
    return tf_has(f_info[square(c, grid)->feat].flags, TF_CROP_BASE);
}


bool square_iscrop(struct chunk *c, struct loc *grid)
{
    return tf_has(f_info[square(c, grid)->feat].flags, TF_CROP);
}


bool square_iswater(struct chunk *c, struct loc *grid)
{
    return tf_has(f_info[square(c, grid)->feat].flags, TF_WATER);
}


bool square_islava(struct chunk *c, struct loc *grid)
{
    return tf_has(f_info[square(c, grid)->feat].flags, TF_LAVA);
}


bool square_isnether(struct chunk *c, struct loc *grid)
{
    return tf_has(f_info[square(c, grid)->feat].flags, TF_NETHER);
}


bool square_ismountain(struct chunk *c, struct loc *grid)
{
    return tf_has(f_info[square(c, grid)->feat].flags, TF_MOUNTAIN);
}


bool square_isdryfountain(struct chunk *c, struct loc *grid)
{
    return (tf_has(f_info[square(c, grid)->feat].flags, TF_FOUNTAIN) &&
        tf_has(f_info[square(c, grid)->feat].flags, TF_DRIED));
}


bool square_isfountain(struct chunk *c, struct loc *grid)
{
    return tf_has(f_info[square(c, grid)->feat].flags, TF_FOUNTAIN);
}


bool square_iswebbed(struct chunk *c, struct loc *grid)
{
    return tf_has(f_info[square(c, grid)->feat].flags, TF_WEB);
}


/*
 * SQUARE INFO PREDICATES
 *
 * These functions tell whether a square is marked with one of the SQUARE_*
 * flags. These flags are mostly used to mark a square with some information
 * about its location or status.
 */


/*
 * True if the square is lit
 */
bool square_isglow(struct chunk *c, struct loc *grid)
{
    my_assert(square_in_bounds(c, grid));

    return sqinfo_has(square(c, grid)->info, SQUARE_GLOW);
}


/*
 * True if the square is part of a vault.
 *
 * This doesn't say what kind of square it is, just that it is part of a vault.
 */
bool square_isvault(struct chunk *c, struct loc *grid)
{
    my_assert(square_in_bounds(c, grid));

    return sqinfo_has(square(c, grid)->info, SQUARE_VAULT);
}


/*
 * True if the square will be cleared of its trash on every dawn.
 */
bool square_notrash(struct chunk *c, struct loc *grid)
{
    my_assert(square_in_bounds(c, grid));

    return sqinfo_has(square(c, grid)->info, SQUARE_NOTRASH);
}


/*
 * True if the square is part of a room.
 */
bool square_isroom(struct chunk *c, struct loc *grid)
{
    my_assert(square_in_bounds(c, grid));

    return sqinfo_has(square(c, grid)->info, SQUARE_ROOM);
}


/*
 * True if the square has been seen by the player
 */
bool square_isseen(struct player *p, struct loc *grid)
{
    my_assert(player_square_in_bounds(p, grid));

    return sqinfo_has(square_p(p, grid)->info, SQUARE_SEEN);
}


/*
 * True if the square is currently viewable by the player
 */
bool square_isview(struct player *p, struct loc *grid)
{
    my_assert(player_square_in_bounds(p, grid));

    return sqinfo_has(square_p(p, grid)->info, SQUARE_VIEW);
}


/*
 * True if the square was seen before the current update
 */
bool square_wasseen(struct player *p, struct loc *grid)
{
    my_assert(player_square_in_bounds(p, grid));

    return sqinfo_has(square_p(p, grid)->info, SQUARE_WASSEEN);
}


/*
 * True if the square has been detected for traps
 */
bool square_isdtrap(struct player *p, struct loc *grid)
{
    my_assert(player_square_in_bounds(p, grid));

    return sqinfo_has(square_p(p, grid)->info, SQUARE_DTRAP);
}


/*
 * True if the square is a feeling trigger square
 */
bool square_isfeel(struct chunk *c, struct loc *grid)
{
    my_assert(square_in_bounds(c, grid));

    return sqinfo_has(square(c, grid)->info, SQUARE_FEEL);
}


/*
 * True if the square is a feeling trigger square for the player
 */
bool square_ispfeel(struct player *p, struct loc *grid)
{
    my_assert(player_square_in_bounds(p, grid));

    return sqinfo_has(square_p(p, grid)->info, SQUARE_FEEL);
}


/*
 * True if the square has a known trap
 */
bool square_istrap(struct chunk *c, struct loc *grid)
{
    my_assert(square_in_bounds(c, grid));

    return sqinfo_has(square(c, grid)->info, SQUARE_TRAP);
}


/*
 * True if the square is an inner wall (generation)
 */
bool square_iswall_inner(struct chunk *c, struct loc *grid)
{
    my_assert(square_in_bounds(c, grid));

    return sqinfo_has(square(c, grid)->info, SQUARE_WALL_INNER);
}


/*
 * True if the square is an outer wall (generation)
 */
bool square_iswall_outer(struct chunk *c, struct loc *grid)
{
    my_assert(square_in_bounds(c, grid));

    return sqinfo_has(square(c, grid)->info, SQUARE_WALL_OUTER);
}


/*
 * True if the square is a solid wall (generation)
 */
bool square_iswall_solid(struct chunk *c, struct loc *grid)
{
    my_assert(square_in_bounds(c, grid));

    return sqinfo_has(square(c, grid)->info, SQUARE_WALL_SOLID);
}


/*
 * True if the square has monster restrictions (generation)
 */
bool square_ismon_restrict(struct chunk *c, struct loc *grid)
{
    my_assert(square_in_bounds(c, grid));

    return sqinfo_has(square(c, grid)->info, SQUARE_MON_RESTRICT);
}


/*
 * True if the square is no-teleport.
 */
bool square_isno_teleport(struct chunk *c, struct loc *grid)
{
    my_assert(square_in_bounds(c, grid));

    return sqinfo_has(square(c, grid)->info, SQUARE_NO_TELEPORT);
}


/*
 * True if the square is no-teleport.
 */
bool square_limited_teleport(struct chunk *c, struct loc *grid)
{
    my_assert(square_in_bounds(c, grid));

    return sqinfo_has(square(c, grid)->info, SQUARE_LIMITED_TELE);
}


/*
 * True if the square is no-map.
 */
bool square_isno_map(struct chunk *c, struct loc *grid)
{
    my_assert(square_in_bounds(c, grid));

    return sqinfo_has(square(c, grid)->info, SQUARE_NO_MAP);
}


/*
 * True if the square is no-esp.
 */
bool square_isno_esp(struct chunk *c, struct loc *grid)
{
    my_assert(square_in_bounds(c, grid));

    return sqinfo_has(square(c, grid)->info, SQUARE_NO_ESP);
}


/*
 * True if the square is marked for projection processing
 */
bool square_isproject(struct chunk *c, struct loc *grid)
{
    my_assert(square_in_bounds(c, grid));

    return sqinfo_has(square(c, grid)->info, SQUARE_PROJECT);
}


/*
 * True if cave square is inappropriate to place stairs
 */
bool square_isno_stairs(struct chunk *c, struct loc *grid)
{
    my_assert(square_in_bounds(c, grid));

    return (sqinfo_has(square(c, grid)->info, SQUARE_NO_STAIRS) ||
        tf_has(f_info[square(c, grid)->feat].flags, TF_NO_STAIRS));
}


/*
 * SQUARE BEHAVIOR PREDICATES
 *
 * These functions define how a given square behaves, e.g. whether it is
 * passable by the player, whether it is diggable, contains items, etc.
 *
 * These functions use the FEATURE PREDICATES (as well as c->info) to make
 * the determination.
 */


/*
 * True if the square is open (a floor square not occupied by a monster/player).
 */
bool square_isopen(struct chunk *c, struct loc *grid)
{
    return (square_isanyfloor(c, grid) && !square(c, grid)->mon);
}


/*
 * True if the square is empty (an open square without any items).
 */
bool square_isempty(struct chunk *c, struct loc *grid)
{
    if (square_isplayertrap(c, grid)) return false;
    return (square_isopen(c, grid) && !square_object(c, grid));
}


/*
 * True if the square is an empty water square.
 */
bool square_isemptywater(struct chunk *c, struct loc *grid)
{
    if (square_isplayertrap(c, grid)) return false;
    return (square_iswater(c, grid) && !square(c, grid)->mon && !square_object(c, grid));
}


/*
 * True if the square is a training square.
 */
bool square_istraining(struct chunk *c, struct loc *grid)
{
    return (square(c, grid)->feat == FEAT_TRAINING);
}


/*
 * True if the square is an "empty" floor grid.
 */
bool square_isemptyfloor(struct chunk *c, struct loc *grid)
{
    /* Assume damaging squares are not valid */
    if (square_isdamaging(c, grid)) return false;

    return (square_ispassable(c, grid) && !square(c, grid)->mon);
}


/*
 * True if the square is an untrapped floor square without items.
 */
bool square_canputitem(struct chunk *c, struct loc *grid)
{
    if (!square_isanyfloor(c, grid)) return false;
    if (square_trap_flag(c, grid, TRF_GLYPH) || square_isplayertrap(c, grid))
        return false;
    return !square_object(c, grid);
}


/*
 * True if the square can be dug: this includes rubble and non-permanent walls.
 */
bool square_isdiggable(struct chunk *c, struct loc *grid)
{
    /* PWMAngband: also include trees and webs */
    return (square_ismineral(c, grid) || square_issecretdoor(c, grid) || square_isrubble(c, grid) ||
        square_istree(c, grid) || square_iswebbed(c, grid));
}


/*
 * True if a square seems diggable: this includes diggable squares as well as permanent walls,
 * closed doors and mountain tiles.
 */
bool square_seemsdiggable(struct chunk *c, struct loc *grid)
{
    return (square_isdiggable(c, grid) || square_basic_iscloseddoor(c, grid) ||
        square_isperm(c, grid) || square_ismountain(c, grid));
}


/*
 * True if a square seems diggable: this includes diggable squares as well as permanent walls,
 * closed doors and mountain tiles.
 */
bool square_seemsdiggable_p(struct player *p, struct loc *grid)
{
    int feat = square_p(p, grid)->feat;

    return ((tf_has(f_info[feat].flags, TF_GRANITE) && !tf_has(f_info[feat].flags, TF_DOOR_ANY)) ||
        feat_is_magma(feat) || feat_is_quartz(feat) || tf_has(f_info[feat].flags, TF_SAND) ||
        tf_has(f_info[feat].flags, TF_ICE) || tf_has(f_info[feat].flags, TF_DARK) ||
        (tf_has(f_info[feat].flags, TF_DOOR_ANY) && tf_has(f_info[feat].flags, TF_ROCK)) ||
        (!tf_has(f_info[feat].flags, TF_WALL) && tf_has(f_info[feat].flags, TF_ROCK)) ||
        tf_has(f_info[feat].flags, TF_TREE) || tf_has(f_info[feat].flags, TF_WEB) ||
        (tf_has(f_info[feat].flags, TF_DOOR_CLOSED) && !tf_has(f_info[feat].flags, TF_DOOR_HOME)) ||
        (tf_has(f_info[feat].flags, TF_PERMANENT) && tf_has(f_info[feat].flags, TF_ROCK)) ||
        tf_has(f_info[feat].flags, TF_MOUNTAIN));
}


/*
 * True if the square can be webbed.
 */
bool square_iswebbable(struct chunk *c, struct loc *grid)
{
    return square_isempty(c, grid);
}


/*
 * True if a monster can walk through the tile.
 *
 * This is needed for polymorphing. A monster may be on a feature that isn't
 * an empty space, causing problems when it is replaced with a new monster.
 */
bool square_is_monster_walkable(struct chunk *c, struct loc *grid)
{
    my_assert(square_in_bounds(c, grid));

    return feat_is_monster_walkable(square(c, grid)->feat);
}


/*
 * True if the square is passable by the player.
 */
bool square_ispassable(struct chunk *c, struct loc *grid)
{
    my_assert(square_in_bounds(c, grid));

    return feat_is_passable(square(c, grid)->feat);
}


/*
 * True if the square is seen as passable by the player.
 */
bool square_ispassable_p(struct player *p, struct loc *grid)
{
    my_assert(player_square_in_bounds(p, grid));

    return feat_is_passable(square_p(p, grid)->feat);
}


/*
 * True if any projectable can pass through the square.
 */
bool square_isprojectable(struct chunk *c, struct loc *grid)
{
    if (!square_in_bounds(c, grid)) return false;

    return feat_is_projectable(square(c, grid)->feat);
}


/*
 * True if any projectable can pass through the square.
 */
bool square_isprojectable_p(struct player *p, struct loc *grid)
{
    if (!player_square_in_bounds(p, grid)) return false;

    return feat_is_projectable(square_p(p, grid)->feat);
}


/*
 * True if the square could be used as a feeling square.
 */
bool square_allowsfeel(struct chunk *c, struct loc *grid)
{
    return square_ispassable(c, grid) && !square_isdamaging(c, grid);
}


/*
 * True if the square allows line-of-sight.
 */
bool square_allowslos(struct chunk *c, struct loc *grid)
{
    my_assert(square_in_bounds(c, grid));

    return feat_is_los(square(c, grid)->feat);
}


/*
 * True if the square is a permanent wall or one of the "stronger" walls.
 *
 * The stronger walls are granite, magma and quartz. This excludes things like
 * secret doors and rubble.
 */
bool square_isstrongwall(struct chunk *c, struct loc *grid)
{
    my_assert(square_in_bounds(c, grid));

    return (square_ismineral(c, grid) || square_isperm(c, grid));
}


/*
 * True if the cave square is internally lit.
 */
bool square_isbright(struct chunk *c, struct loc *grid)
{
    my_assert(square_in_bounds(c, grid));

    return feat_is_bright(square(c, grid)->feat);
}


/*
 * True if the cave square is fire-based.
 */
bool square_isfiery(struct chunk *c, struct loc *grid)
{
    my_assert(square_in_bounds(c, grid));

    return feat_is_fiery(square(c, grid)->feat);
}


/*
 * True if the cave square is lit.
 */
bool square_islit(struct player *p, struct loc *grid)
{
    my_assert(player_square_in_bounds(p, grid));

    return ((square_light(p, grid) > 0)? true: false);
}


/*
 * True if the cave square can damage the inhabitant
 */
bool square_isdamaging(struct chunk *c, struct loc *grid)
{
    my_assert(square_in_bounds(c, grid));

    return (square_iswater(c, grid) || square_islava(c, grid) || square_isfiery(c, grid) ||
        square_isnether(c, grid));
}


/*
 * True if the cave square doesn't allow monster flow information.
 */
bool square_isnoflow(struct chunk *c, struct loc *grid)
{
    my_assert(square_in_bounds(c, grid));

    return feat_is_no_flow(square(c, grid)->feat);
}


/*
 * True if the cave square doesn't carry player scent.
 */
bool square_isnoscent(struct chunk *c, struct loc *grid)
{
    my_assert(square_in_bounds(c, grid));

    return feat_is_no_scent(square(c, grid)->feat);
}


bool square_iswarded(struct chunk *c, struct loc *grid)
{
    struct trap_kind *rune = lookup_trap("glyph of warding");

    return square_trap_specific(c, grid, rune->tidx);
}


bool square_isdecoyed(struct chunk *c, struct loc *grid)
{
    struct trap_kind *glyph = lookup_trap("decoy");

    return square_trap_specific(c, grid, glyph->tidx);
}


bool square_seemslikewall(struct chunk *c, struct loc *grid)
{
    return (tf_has(f_info[square(c, grid)->feat].flags, TF_ROCK) ||
        sqinfo_has(square(c, grid)->info, SQUARE_CUSTOM_WALL));
}


bool square_isinteresting(struct chunk *c, struct loc *grid)
{
    return tf_has(f_info[square(c, grid)->feat].flags, TF_INTERESTING);
}


/*
 * True if the square is a closed, locked door.
 */
bool square_islockeddoor(struct chunk *c, struct loc *grid)
{
    return square_door_power(c, grid) > 0;
}


/*
 * True if the square is a closed, unlocked door.
 */
bool square_isunlockeddoor(struct chunk *c, struct loc *grid)
{
    return (square_basic_iscloseddoor(c, grid) && !square_islockeddoor(c, grid));
}


/*
 * True if there is a player trap (known or unknown) in this square.
 */
bool square_isplayertrap(struct chunk *c, struct loc *grid)
{
    return square_trap_flag(c, grid, TRF_TRAP);
}


/*
 * True if there is a visible trap in this square.
 */
bool square_isvisibletrap(struct chunk *c, struct loc *grid)
{
    return square_trap_flag(c, grid, TRF_VISIBLE);
}


/*
 * True if the square is an unknown player trap (it will appear as a floor tile).
 */
bool square_issecrettrap(struct chunk *c, struct loc *grid)
{
    return !square_isvisibletrap(c, grid) && square_isplayertrap(c, grid);
}


/*
 * True if the square is a known, disabled player trap.
 */
bool square_isdisabledtrap(struct chunk *c, struct loc *grid)
{
    return square_isvisibletrap(c, grid) && (square_trap_timeout(c, grid, 0) > 0);
}


/*
 * True if the square is a known, disarmable player trap.
 */
bool square_isdisarmabletrap(struct chunk *c, struct loc *grid)
{
    if (square_isdisabledtrap(c, grid)) return false;
    return square_isvisibletrap(c, grid) && square_isplayertrap(c, grid);
}


/*
 * Checks if a square is at the (inner) edge of a trap detect area
 */
bool square_dtrap_edge(struct player *p, struct chunk *c, struct loc *grid)
{
    struct loc next;

    /* Only on random levels */
    if (!random_level(&p->wpos)) return false;

    /* Check if the square is a dtrap in the first place */
    if (!square_isdtrap(p, grid)) return false;

    /* Check for non-dtrap adjacent grids */
    next_grid(&next, grid, DIR_S);
    if (square_in_bounds_fully(c, &next) && !square_isdtrap(p, &next))
        return true;
    next_grid(&next, grid, DIR_E);
    if (square_in_bounds_fully(c, &next) && !square_isdtrap(p, &next))
        return true;
    next_grid(&next, grid, DIR_N);
    if (square_in_bounds_fully(c, &next) && !square_isdtrap(p, &next))
        return true;
    next_grid(&next, grid, DIR_W);
    if (square_in_bounds_fully(c, &next) && !square_isdtrap(p, &next))
        return true;

    return false;
}


/*
 * Determine if a given location may be "destroyed"
 *
 * Used by destruction spells, and for placing stairs, etc.
 */
bool square_changeable(struct chunk *c, struct loc *grid)
{
    struct object *obj;

    /* Forbid perma-grids */
    if (square_isunpassable(c, grid) || square_isstairs(c, grid) || square_isshop(c, grid) ||
        square_home_iscloseddoor(c, grid))
    {
        return false;
    }

    /* Check objects */
    for (obj = square_object(c, grid); obj; obj = obj->next)
    {
        /* Forbid artifact grids */
        if (obj->artifact) return false;
    }

    /* Accept */
    return true;
}


bool square_in_bounds(struct chunk *c, struct loc *grid)
{
    my_assert(c);

    return ((grid->x >= 0) && (grid->x < c->width) && (grid->y >= 0) && (grid->y < c->height));
}


bool square_in_bounds_fully(struct chunk *c, struct loc *grid)
{
    my_assert(c);

    return ((grid->x > 0) && (grid->x < c->width - 1) && (grid->y > 0) && (grid->y < c->height - 1));
}


/*
 * Checks if a square is thought by the player to block projections
 */
bool square_isbelievedwall(struct player *p, struct chunk *c, struct loc *grid)
{
    /* The edge of the world is definitely gonna block things */
    if (!square_in_bounds_fully(c, grid)) return true;

    /* If we dont know assume its projectable */
    if (!square_isknown(p, grid)) return false;

    /* Report what we think (we may be wrong) */
    return !feat_is_projectable(square_p(p, grid)->feat);
}


/*
 * Checks if a square is appropriate for placing a summoned creature.
 */
bool square_allows_summon(struct chunk *c, struct loc *grid)
{
    return (square_isemptyfloor(c, grid) && !square_trap_flag(c, grid, TRF_GLYPH));
}


/*
 * OTHER SQUARE FUNCTIONS
 *
 * Below are various square-specific functions which are not predicates
 */


struct square *square(struct chunk *c, struct loc *grid)
{
    my_assert(square_in_bounds(c, grid));
    return &c->squares[grid->y][grid->x];
}


struct player_square *square_p(struct player *p, struct loc *grid)
{
    my_assert(player_square_in_bounds(p, grid));
    return &p->cave->squares[grid->y][grid->x];
}


struct feature *square_feat(struct chunk *c, struct loc *grid)
{
    my_assert(square_in_bounds(c, grid));
    return &f_info[square(c, grid)->feat];
}


int square_light(struct player *p, struct loc *grid)
{
    my_assert(player_square_in_bounds(p, grid));
    return square_p(p, grid)->light;
}


/*
 * Get a monster on the current level by its position.
 */
struct monster *square_monster(struct chunk *c, struct loc *grid)
{
    if (!square_in_bounds(c, grid)) return NULL;
    if (square(c, grid)->mon > 0)
    {
        struct monster *mon = cave_monster(c, square(c, grid)->mon);

        return ((mon && mon->race)? mon: NULL);
    }

    return NULL;
}


/*
 * Get the top object of a pile on the current level by its position.
 */
struct object *square_object(struct chunk *c, struct loc *grid)
{
    if (!square_in_bounds(c, grid)) return NULL;
    return square(c, grid)->obj;
}


/*
 * Get the first (and currently only) trap in a position on the current level.
 */
struct trap *square_trap(struct chunk *c, struct loc *grid)
{
    if (!square_in_bounds(c, grid)) return NULL;
    return square(c, grid)->trap;
}


/*
 * Return true if the given object is on the floor at this grid
 */
bool square_holds_object(struct chunk *c, struct loc *grid, struct object *obj)
{
    my_assert(square_in_bounds(c, grid));
    return pile_contains(square_object(c, grid), obj);
}


/*
 * Remove an object from a floor pile, leaving it unattached
 */
void square_excise_object(struct chunk *c, struct loc *grid, struct object *obj)
{
    my_assert(square_in_bounds(c, grid));
    pile_excise(&square(c, grid)->obj, obj);

    /* Hack -- excise object index */
    c->o_gen[0 - (obj->oidx + 1)] = false;
    obj->oidx = 0;

    /* Delete the mimicking monster if necessary */
    if (obj->mimicking_m_idx)
    {
        struct monster *mon = cave_monster(c, obj->mimicking_m_idx);

        /* Clear the mimicry */
        mon->mimicked_obj = NULL;
        mflag_off(mon->mflag, MFLAG_CAMOUFLAGE);
    }

    /* Redraw */
    redraw_floor(&c->wpos, grid, NULL);

    /* Visual update */
    square_light_spot(c, grid);
}


/*
 * Excise an entire floor pile.
 */
void square_excise_pile(struct chunk *c, struct loc *grid)
{
    struct object *obj;

    my_assert(c);
    my_assert(square_in_bounds(c, grid));

    for (obj = square_object(c, grid); obj; obj = obj->next)
    {
        /* Preserve unseen artifacts */
        preserve_artifact(obj);

        /* Hack -- excise object index */
        c->o_gen[0 - (obj->oidx + 1)] = false;
        obj->oidx = 0;

        /* Delete the mimicking monster if necessary */
        if (obj->mimicking_m_idx)
        {
            struct monster *mon = cave_monster(c, obj->mimicking_m_idx);

            /* Clear the mimicry */
            mon->mimicked_obj = NULL;

            delete_monster_idx(c, obj->mimicking_m_idx);
        }
    }

    object_pile_free(square_object(c, grid));
    square_set_obj(c, grid, NULL);

    /* Redraw */
    redraw_floor(&c->wpos, grid, NULL);

    /* Visual update */
    square_light_spot(c, grid);
}


/*
 * Excise an object from a floor pile and delete it while doing the other
 * necessary bookkeeping. Normally, this is only called for the chunk
 * representing the true nature of the environment and not the one
 * representing the player's view of it. If do_note is true, call
 * square_note_spot(). If do_light is true, call square_light_spot().
 * Unless calling this on the player's view, those both would be true
 * except as an optimization/simplification when the caller would call
 * square_note_spot()/square_light_spot() anyways or knows that those aren't
 * necessary.
 */
void square_delete_object(struct chunk *c, struct loc *grid, struct object *obj, bool do_note,
    bool do_light)
{
    square_excise_object(c, grid, obj);
    object_delete(&obj);
    if (do_note) square_note_spot(c, grid);
    if (do_light) square_light_spot(c, grid);
}


/*
 * Sense the existence of objects on a grid in the current level
 */
void square_sense_pile(struct player *p, struct chunk *c, struct loc *grid)
{
    struct object *obj;

    if (!wpos_eq(&p->wpos, &c->wpos)) return;

    /* Make new sensed objects where necessary */
    if (square_p(p, grid)->obj) return;

    /* Sense every item on this grid */
    for (obj = square_object(c, grid); obj; obj = obj->next)
        object_sense(p, obj);
}


static bool object_equals(const struct object *obj1, const struct object *obj2)
{
    struct object *test;

    /* Objects are strictly equal */
    if (obj1 == obj2) return true;

    /* Objects are strictly different */
    if (!(obj1 && obj2)) return false;

    /* Make a writable identical copy of the second object */
    test = object_new();
    memcpy(test, obj2, sizeof(struct object));

    /* Make prev and next strictly equal since they are irrelevant */
    test->prev = obj1->prev;
    test->next = obj1->next;

    /* Known part must be equal */
    if (!object_equals(obj1->known, test->known))
    {
        mem_free(test);
        return false;
    }

    /* Make known strictly equal since they are now irrelevant */
    test->known = obj1->known;

    /* Brands must be equal */
    if (!brands_are_equal(obj1, test))
    {
        mem_free(test);
        return false;
    }

    /* Make brands strictly equal since they are now irrelevant */
    test->brands = obj1->brands;

    /* Slays must be equal */
    if (!slays_are_equal(obj1, test))
    {
        mem_free(test);
        return false;
    }

    /* Make slays strictly equal since they are now irrelevant */
    test->slays = obj1->slays;

    /* Make attr strictly equal since they are irrelevant */
    test->attr = obj1->attr;

    /* All other fields must be equal */
    if (memcmp(obj1, test, sizeof(struct object)) != 0)
    {
        mem_free(test);
        return false;
    }

    /* Success */
    mem_free(test);
    return true;
}


static void square_update_pile(struct player *p, struct chunk *c, struct loc *grid)
{
    struct object *obj;

    /* Know every item on this grid */
    for (obj = square_object(c, grid); obj; obj = obj->next)
    {
        /* Make the new object */
        struct object *new_obj = object_new();

        object_copy(new_obj, obj);

        /* Attach it to the current floor pile */
        pile_insert_end(&square_p(p, grid)->obj, new_obj);
    }
}


/*
 * Update the player's knowledge of the objects on a grid in the current level
 */
void square_know_pile(struct player *p, struct chunk *c, struct loc *grid)
{
    struct object *obj, *known_obj;

    obj = square_object(c, grid);
    known_obj = square_p(p, grid)->obj;

    if (!wpos_eq(&p->wpos, &c->wpos)) return;

    /* Object is not known: update knowledge */
    if (!known_obj)
    {
        square_update_pile(p, c, grid);
        return;
    }

    /* Object is absent: wipe knowledge */
    if (!obj)
    {
        square_forget_pile(p, grid);
        return;
    }

    /* Object is known: wipe and update knowledge if something changed */
    while (obj || known_obj)
    {
        if (!object_equals(obj, known_obj))
        {
            square_forget_pile(p, grid);
            square_update_pile(p, c, grid);
            return;
        }
        if (obj) obj = obj->next;
        if (known_obj) known_obj = known_obj->next;
    }
}


void square_forget_pile(struct player *p, struct loc *grid)
{
    struct object *current, *next;

    current = square_p(p, grid)->obj;
    while (current)
    {
        next = current->next;

        /* Stop tracking item */
        if (tracked_object_is(p->upkeep, current)) track_object(p->upkeep, NULL);

        object_delete(&current);
        current = next;
    }
    square_p(p, grid)->obj = NULL;
}


struct object *square_known_pile(struct player *p, struct chunk *c, struct loc *grid)
{
    /* Hack -- DM has full knowledge */
    if (p->dm_flags & DM_SEE_LEVEL) return square_object(c, grid);

    return square_p(p, grid)->obj;
}


/*
 * Return how many cardinal directions around (x, y) contain (real) walls.
 *
 * c current chunk
 * y, x co-ordinates
 *
 * Returns the number of walls
 */
int square_num_walls_adjacent(struct chunk *c, struct loc *grid)
{
    int k = 0;
    struct loc next;

    my_assert(square_in_bounds_fully(c, grid));

    next_grid(&next, grid, DIR_S);
    if (feat_is_wall(square(c, &next)->feat)) k++;
    next_grid(&next, grid, DIR_N);
    if (feat_is_wall(square(c, &next)->feat)) k++;
    next_grid(&next, grid, DIR_E);
    if (feat_is_wall(square(c, &next)->feat)) k++;
    next_grid(&next, grid, DIR_W);
    if (feat_is_wall(square(c, &next)->feat)) k++;

    return k;
}


/*
 * Set the terrain type for a square.
 *
 * This should be the only function that sets terrain, apart from the savefile
 * loading code.
 */
void square_set_feat(struct chunk *c, struct loc *grid, int feat)
{
    int current_feat;

    my_assert(square_in_bounds(c, grid));
    current_feat = square(c, grid)->feat;

    /* Track changes */
    if (current_feat) c->feat_count[current_feat]--;
    if (feat) c->feat_count[feat]++;

    /* Make the change */
    square(c, grid)->feat = feat;

    /* Light bright terrain */
    if (feat_is_bright(feat)) sqinfo_on(square(c, grid)->info, SQUARE_GLOW);

    /* Make the new terrain feel at home */
    if (!ht_zero(&c->generated))
    {
        /* Remove traps if necessary */
        if (!square_player_trap_allowed(c, grid))
            square_destroy_trap(c, grid);

        square_note_spot(c, grid);
        square_light_spot(c, grid);
    }

    /* Make sure no incorrect wall flags set for dungeon generation */
    else
    {
        sqinfo_off(square(c, grid)->info, SQUARE_WALL_INNER);
        sqinfo_off(square(c, grid)->info, SQUARE_WALL_OUTER);
        sqinfo_off(square(c, grid)->info, SQUARE_WALL_SOLID);
    }
}


/*
 * Set the player-"known" terrain type for a square.
 */
static void square_set_known_feat(struct player *p, struct loc *grid, int feat)
{
    square_p(p, grid)->feat = feat;
}


/*
 * Set the occupying monster for a square.
 */
void square_set_mon(struct chunk *c, struct loc *grid, int midx)
{
    square(c, grid)->mon = midx;
}


/*
 * Set the (first) object for a square.
 */
void square_set_obj(struct chunk *c, struct loc *grid, struct object *obj)
{
    square(c, grid)->obj = obj;
}


/*
 * Set the (first) trap for a square.
 */
void square_set_trap(struct chunk *c, struct loc *grid, struct trap *trap)
{
    square(c, grid)->trap = trap;
}


void square_add_trap(struct chunk *c, struct loc *grid)
{
    my_assert(square_in_bounds_fully(c, grid));
    place_trap(c, grid, -1, c->wpos.depth);
}


void square_add_glyph(struct chunk *c, struct loc *grid, int type)
{
    struct trap_kind *glyph = NULL;

    switch (type)
    {
        case GLYPH_WARDING: glyph = lookup_trap("glyph of warding"); break;
        case GLYPH_DECOY: glyph = lookup_trap("decoy"); loc_copy(&c->decoy, grid); break;
        default: return;
    }

    place_trap(c, grid, glyph->tidx, 0);
}


void square_add_web(struct chunk *c, struct loc *grid)
{
    square_set_feat(c, grid, FEAT_WEB);
}


static void square_set_stairs(struct chunk *c, struct loc *grid, int feat)
{
    struct worldpos dpos;
    struct location *dungeon;

    /* Get the dungeon */
    wpos_init(&dpos, &c->wpos.grid, 0);
    dungeon = get_dungeon(&dpos);
    if (dungeon && c->wpos.depth)
    {
        int i, chance;

        /* Basic chance */
        chance = randint0(10000);

        /* Get a random stair tile */
        for (i = 0; i < dungeon->n_stairs; i++)
        {
            struct dun_feature *feature = &dungeon->stairs[i];

            if (feature->chance > chance)
            {
                if (feat == FEAT_MORE) feat = feature->feat;
                else feat = feature->feat2;
                break;
            }

            chance -= feature->chance;
        }
    }

    square_set_feat(c, grid, feat);
}


void square_add_stairs(struct chunk *c, struct loc *grid, int feat_stairs)
{
    static uint8_t count = 0xFF;
    static int feat = 0;
    int desired_feat;

    if (!feat) feat = FEAT_MORE;

    /* Choose staircase direction */
    if  ((c->wpos.grid.x == 0 && c->wpos.grid.y == 6) || // zeitnot dungeon stairs all go down
        (c->wpos.grid.x == 0 && c->wpos.grid.y == -6))   // ironman dungeon too
    {
        desired_feat = FEAT_MORE; // down
    }
    else if (feat_stairs != FEAT_NONE)
        desired_feat = feat_stairs;
    else if (cfg_limit_stairs >= 2)
    {
        /* Always down */
        desired_feat = FEAT_MORE;
    }
    else if (count == 0)
    {
        /* Un-bias the RNG: no more creation of 10 up staircases in a row... */
        count = rand_range(3, 5);
        feat = ((feat == FEAT_MORE)? FEAT_LESS: FEAT_MORE);
        desired_feat = feat;
    }
    else
    {
        /* Random choice */
        desired_feat = (magik(50)? FEAT_MORE: FEAT_LESS);

        /* Check current feature */
        if ((count == 0xFF) || (desired_feat != feat)) count = rand_range(3, 5);
        if (desired_feat == feat) count--;
    }

    /* Create a staircase */
    square_set_stairs(c, grid, desired_feat);
}


void square_open_door(struct chunk *c, struct loc *grid)
{
    struct trap_kind *lock = lookup_trap("door lock");

    my_assert(square_iscloseddoor(c, grid) || square_issecretdoor(c, grid));
    my_assert(lock);
    square_remove_all_traps_of_type(c, grid, lock->tidx);

    square_create_open_door(c, grid);
}


void square_create_open_door(struct chunk *c, struct loc *grid)
{
    int feat = FEAT_OPEN;
    struct worldpos dpos;
    struct location *dungeon;

    /* Get the dungeon */
    wpos_init(&dpos, &c->wpos.grid, 0);
    dungeon = get_dungeon(&dpos);
    if (dungeon && c->wpos.depth)
    {
        int i;

        /* Use the corresponding open door instead */
        for (i = 0; i < dungeon->n_doors; i++)
        {
            struct dun_feature *feature = &dungeon->doors[i];

            if (square(c, grid)->feat == feature->feat)
            {
                feat = feature->feat2;
                break;
            }
        }
    }

    square_set_feat(c, grid, feat);
}


void square_open_homedoor(struct chunk *c, struct loc *grid)
{
    square_set_feat(c, grid, FEAT_HOME_OPEN);
}


void square_close_door(struct chunk *c, struct loc *grid)
{
    my_assert(square_isopendoor(c, grid));

    square_create_closed_door(c, grid);
}


void square_create_closed_door(struct chunk *c, struct loc *grid)
{
    int feat = FEAT_CLOSED;
    struct worldpos dpos;
    struct location *dungeon;

    /* Get the dungeon */
    wpos_init(&dpos, &c->wpos.grid, 0);
    dungeon = get_dungeon(&dpos);
    if (dungeon && c->wpos.depth)
    {
        int i, chance;

        /* Basic chance */
        chance = randint0(10000);

        /* Get a random closed door (for mimics) */
        for (i = 0; i < dungeon->n_doors; i++)
        {
            struct dun_feature *feature = &dungeon->doors[i];

            if (feature->chance > chance)
            {
                feat = feature->feat;
                break;
            }

            chance -= feature->chance;
        }

        /* If we close a specific open door, use that instead */
        for (i = 0; i < dungeon->n_doors; i++)
        {
            struct dun_feature *feature = &dungeon->doors[i];

            if (square(c, grid)->feat == feature->feat2)
            {
                feat = feature->feat;
                break;
            }
        }
    }

    square_set_feat(c, grid, feat);
}


void square_smash_door(struct chunk *c, struct loc *grid)
{
    struct trap_kind *lock = lookup_trap("door lock");

    my_assert(square_isdoor(c, grid));
    my_assert(lock);
    square_remove_all_traps_of_type(c, grid, lock->tidx);

    square_create_smashed_door(c, grid);
}


void square_create_smashed_door(struct chunk *c, struct loc *grid)
{
    int feat = FEAT_BROKEN;
    struct worldpos dpos;
    struct location *dungeon;

    /* Get the dungeon */
    wpos_init(&dpos, &c->wpos.grid, 0);
    dungeon = get_dungeon(&dpos);
    if (dungeon && c->wpos.depth)
    {
        int i;

        /* Use the corresponding broken door instead */
        for (i = 0; i < dungeon->n_doors; i++)
        {
            struct dun_feature *feature = &dungeon->doors[i];

            if (square(c, grid)->feat == feature->feat)
            {
                feat = feature->feat3;
                break;
            }
        }
    }

    square_set_feat(c, grid, feat);
}


void square_unlock_door(struct chunk *c, struct loc *grid)
{
    square_set_door_lock(c, grid, 0);
}


static bool square_set_floor_valid(struct chunk *c, struct loc *grid)
{
    /* Need to be passable */
    if (!square_ispassable(c, grid)) return false;

    /* Floor can't hold objects */
    if (square_isanyfloor(c, grid) && !square_isobjectholding(c, grid)) return false;

    return true;
}


void square_set_floor(struct chunk *c, struct loc *grid, int feat)
{
    struct worldpos dpos;
    struct location *dungeon;
    bool feat_ispitfloor = (tf_has(f_info[feat].flags, TF_FLOOR) &&
        tf_has(f_info[feat].flags, TF_PIT));

    /* Get the dungeon */
    wpos_init(&dpos, &c->wpos.grid, 0);
    dungeon = get_dungeon(&dpos);

    /* Get a random custom floor */
    if (dungeon && c->wpos.depth && !feat_ispitfloor)
    {
        customize_feature(c, grid, dungeon->floors, dungeon->n_floors, square_set_floor_valid, NULL,
            &feat);
    }

    square_set_feat(c, grid, feat);
    sqinfo_off(square(c, grid)->info, SQUARE_CUSTOM_WALL);
}


void square_destroy_door(struct chunk *c, struct loc *grid)
{
    int feat = ((c->wpos.depth > 0)? FEAT_FLOOR: FEAT_DIRT);

    struct trap_kind *lock = lookup_trap("door lock");

    my_assert(square_isdoor(c, grid));
    my_assert(lock);
    square_remove_all_traps_of_type(c, grid, lock->tidx);
    square_set_floor(c, grid, feat);
}


void square_destroy_trap(struct chunk *c, struct loc *grid)
{
    square_remove_all_traps(c, grid);
}


void square_disable_trap(struct player *p, struct chunk *c, struct loc *grid)
{
    if (!square_isplayertrap(c, grid)) return;
    square_set_trap_timeout(p, c, grid, false, 0, 10);
}


void square_destroy_decoy(struct player *p, struct chunk *c, struct loc *grid)
{
    struct trap_kind *decoy_kind = lookup_trap("decoy");

    my_assert(decoy_kind);
    square_remove_all_traps_of_type(c, grid, decoy_kind->tidx);
    loc_init(&c->decoy, 0, 0);

    if (!p) return;
    if (los(c, &p->grid, grid) && !p->timed[TMD_BLIND] && !p->timed[TMD_BLIND_REAL])
        msg(p, "The decoy is destroyed!");
}


void square_tunnel_wall(struct chunk *c, struct loc *grid)
{
    square_set_feat(c, grid, FEAT_DIRT);
    sqinfo_off(square(c, grid)->info, SQUARE_CUSTOM_WALL);
}


void square_destroy_wall(struct chunk *c, struct loc *grid)
{
    int feat = ((c->wpos.depth > 0)? FEAT_FLOOR: FEAT_MUD);

    square_set_floor(c, grid, feat);
}


void square_smash_wall(struct player *p, struct chunk *c, struct loc *grid)
{
    int i;

    square_set_floor(c, grid, FEAT_FLOOR);

    for (i = 0; i < 8; i++)
    {
        struct loc adj_grid;

        /* Extract adjacent location */
        loc_sum(&adj_grid, grid, &ddgrid_ddd[i]);

        /* Check legality */
        if (!square_in_bounds_fully(c, &adj_grid)) continue;

        /* Ignore permanent grids */
        if (square_isunpassable(c, &adj_grid)) continue;

        /* Ignore floors, but destroy decoys */
        if (square_isanyfloor(c, &adj_grid))
        {
            if (square_isdecoyed(c, &adj_grid)) square_destroy_decoy(p, c, &adj_grid);
            continue;
        }

        /* Give this grid a chance to survive */
        if ((square_isrock(c, &adj_grid) && one_in_(4)) ||
            (square_isquartz(c, &adj_grid) && one_in_(10)) ||
            (square_ismagma(c, &adj_grid) && one_in_(20)) ||
            (square_ismineral_other(c, &adj_grid) && one_in_(40)))
        {
            continue;
        }

        /* Remove it */
        square_set_floor(c, &adj_grid, FEAT_FLOOR);
    }
}


static bool square_set_wall_valid(struct chunk *c, struct loc *grid)
{
    /* Floor can't hold objects */
    if (square_isanyfloor(c, grid) && !square_isobjectholding(c, grid))
        return false;

    return true;
}


static void square_set_wall(struct chunk *c, struct loc *grid, int feat)
{
    struct worldpos dpos;
    struct location *dungeon;

    /* Get the dungeon */
    wpos_init(&dpos, &c->wpos.grid, 0);
    dungeon = get_dungeon(&dpos);
    if (dungeon && c->wpos.depth)
    {
        int feat = 0;

        /* Get a random wall tile */
        if (customize_feature(c, grid, dungeon->walls, dungeon->n_walls, square_set_wall_valid,
            NULL, &feat))
        {
            sqinfo_on(square(c, grid)->info, SQUARE_CUSTOM_WALL);
        }
    }

    square_set_feat(c, grid, feat);
}


void square_destroy(struct chunk *c, struct loc *grid)
{
    int r = randint0(200);

    if (r < 20)
        square_set_wall(c, grid, FEAT_GRANITE);
    else if (r < 70)
        square_set_feat(c, grid, FEAT_QUARTZ);
    else if (r < 100)
        square_set_feat(c, grid, FEAT_MAGMA);
    else
        square_set_floor(c, grid, FEAT_FLOOR);
}


void square_earthquake(struct chunk *c, struct loc *grid)
{
    int t = randint0(100);

    if (!square_ispassable(c, grid))
    {
        square_clear_feat(c, grid);
        return;
    }

    if (t < 20)
        square_set_wall(c, grid, FEAT_GRANITE);
    else if (t < 70)
        square_set_feat(c, grid, FEAT_QUARTZ);
    else
        square_set_feat(c, grid, FEAT_MAGMA);
}


/*
 * Add visible treasure to a mineral square.
 */
void square_upgrade_mineral(struct chunk *c, struct loc *grid)
{
    if (square(c, grid)->feat == FEAT_MAGMA)
        square_set_feat(c, grid, FEAT_MAGMA_K);
    if (square(c, grid)->feat == FEAT_QUARTZ)
        square_set_feat(c, grid, FEAT_QUARTZ_K);
}


void square_destroy_rubble(struct chunk *c, struct loc *grid)
{
    int feat = ((c->wpos.depth > 0)? FEAT_FLOOR: FEAT_MUD);

    square_set_floor(c, grid, feat);
}


int feat_shopnum(int feat)
{
    return f_info[feat].shopnum - 1;
}


/* Note that this returns the STORE_ index, which is one less than shopnum */
int square_shopnum(struct chunk *c, struct loc *grid)
{
    if (square_isshop(c, grid)) return feat_shopnum(square(c, grid)->feat);
    return -1;
}


int square_digging(struct chunk *c, struct loc *grid)
{
    return f_info[square(c, grid)->feat].dig;
}


static bool square_apparent_feat_valid(struct chunk *c, struct loc *grid)
{
    if (square_ispassable(c, grid)) return false;
    return true;
}


int square_apparent_feat(struct player *p, struct chunk *c, struct loc *grid)
{
    int actual = square_known_feat(p, c, grid);

    if (f_info[actual].mimic)
    {
        actual = f_info[actual].mimic->fidx;

        /* Use custom feature for secret doors to avoid leaking info */
        if (actual == FEAT_GRANITE)
        {
            struct worldpos dpos;
            struct location *dungeon;

            /* Get the dungeon */
            wpos_init(&dpos, &c->wpos.grid, 0);
            dungeon = get_dungeon(&dpos);
            if (dungeon && c->wpos.depth)
            {
                uint32_t tmp_seed = Rand_value;
                bool rand_old = Rand_quick;

                /* Fixed seed for consistence */
                Rand_quick = true;
                Rand_value = seed_wild + world_index(&c->wpos) * 600 + c->wpos.depth * 37;

                /* Get a random wall tile */
                customize_feature(c, grid, dungeon->walls, dungeon->n_walls,
                    square_apparent_feat_valid, NULL, &actual);

                Rand_value = tmp_seed;
                Rand_quick = rand_old;
            }
        }
    }

    return actual;
}


/*
 * Return the name for the terrain in a grid. Accounts for the fact that
 * some terrain mimics another terrain.
 *
 * c Is the chunk to use. Usually it is the player's version of the chunk.
 * grid Is the grid to use.
 */
const char *square_apparent_name(struct player *p, struct chunk *c, struct loc *grid)
{
    struct feature *f = &f_info[square_apparent_feat(p, c, grid)];

    if (f->shortdesc) return f->shortdesc;
    return f->name;
}


/*
 * Return the prefix, appropriate for describing looking at the grid in
 * question, for the name returned by square_name().
 *
 * c Is the chunk to use. Usually it is the player's version of the chunk.
 * grid Is the grid to use.
 *
 * The prefix is usually an indefinite article. It may be an empty string.
 */
const char *square_apparent_look_prefix(struct player *p, struct chunk *c, struct loc *grid)
{
    struct feature *f = &f_info[square_apparent_feat(p, c, grid)];

    if (f->look_prefix) return f->look_prefix;
    return (is_a_vowel(f->name[0])? "an ": "a ");
}


/*
 * Return a preposition, appropriate for describing the grid the viewer is on,
 * for the name returned by square_name(). May return an empty string when
 * the name doesn't require a preposition.
 *
 * c Is the chunk to use. Usually it is the player's version of the chunk.
 * grid Is the grid to use.
 */
const char *square_apparent_look_in_preposition(struct player *p, struct chunk *c, struct loc *grid)
{
    struct feature *f = &f_info[square_apparent_feat(p, c, grid)];

    if (f->look_in_preposition) return f->look_in_preposition;
    return "on ";
}


/* Memorize the terrain */
void square_memorize(struct player *p, struct chunk *c, struct loc *grid)
{
    if (!wpos_eq(&p->wpos, &c->wpos)) return;
    square_set_known_feat(p, grid, square(c, grid)->feat);
}


/* Forget the terrain */
void square_forget(struct player *p, struct loc *grid)
{
    square_set_known_feat(p, grid, FEAT_NONE);
}


void square_mark(struct player *p, struct loc *grid)
{
    sqinfo_on(square_p(p, grid)->info, SQUARE_MARK);
}


void square_unmark(struct player *p, struct loc *grid)
{
    sqinfo_off(square_p(p, grid)->info, SQUARE_MARK);
}


void square_glow(struct chunk *c, struct loc *grid)
{
    sqinfo_on(square(c, grid)->info, SQUARE_GLOW);
}


void square_unglow(struct chunk *c, struct loc *grid)
{
    /* Bright tiles are always lit */
    if (square_isbright(c, grid)) return;

    sqinfo_off(square(c, grid)->info, SQUARE_GLOW);
}


bool square_isnormal(struct chunk *c, struct loc *grid)
{
    if (square_isvisibletrap(c, grid)) return true;
    return (!tf_has(f_info[square(c, grid)->feat].flags, TF_FLOOR) &&
        !tf_has(f_info[square(c, grid)->feat].flags, TF_BORING));
}


void square_destroy_tree(struct chunk *c, struct loc *grid)
{
    square_set_feat(c, grid, FEAT_DIRT);
    sqinfo_off(square(c, grid)->info, SQUARE_CUSTOM_WALL);
}


void square_burn_tree(struct chunk *c, struct loc *grid)
{
    square_set_feat(c, grid, FEAT_EVIL_TREE);
}


void square_burn_grass(struct chunk *c, struct loc *grid)
{
    square_set_feat(c, grid, FEAT_DIRT);
}


void square_colorize_door(struct chunk *c, struct loc *grid, int power)
{
    square_set_feat(c, grid, FEAT_HOME_CLOSED + power);
}


// get random wall feat for house building
void square_build_new_permhouse(struct chunk *c, struct loc *grid, char wall_type, int wall_id)
{
    char wall[20] = "house wall ";       // first part of the wall name
 // wall_type - (given to this fuction) second part of the wall name
 // wall_id - specific wall (when we have several different walls in one row)
    int rng = 0;                              // preliminary third part of wall name
    char wall_glyph = '\0';                   // third part of wall name
    int house_wall = 0;                       // result: index of terrain feature
    int len; // measure length of a string

    // random choice of wall number
    rng = randint0(63);

    /* getting wall type from function */
    if (wall_type == 'a')      // B7 B8   wood
    {
        if ((rng == 5) || (rng == 6) ||       // door tiles.. bullutin boards and fire -> rare
        (rng == 32) || (rng == 33) || (rng == 34)) rng = randint0(63);
        if ((rng == 5) || (rng == 6) ||       // very rare
        (rng == 32) || (rng == 33) || (rng == 34)) rng = randint0(63);
        if ((rng == 5) || (rng == 6) ||       // really rare :)
        (rng == 32) || (rng == 33) || (rng == 34)) rng = randint0(63);  
    }

    else if (wall_type == 'b') // B9 BA   black small bricks
    {
        // bullutin boards and fire + stairs -> rare
        if ((rng == 32) || (rng == 33) || (rng == 34) ||
        (rng == 24) || (rng == 25)) rng = randint0(63);
        if ((rng == 32) || (rng == 33) || (rng == 34) ||
        (rng == 24) || (rng == 25)) rng = randint0(63);
        if ((rng == 32) || (rng == 33) || (rng == 34) ||
        (rng == 24) || (rng == 25)) rng = randint0(63);
        
        if      (one_in_(5))  {wall_type = 'h'; rng = randint0(31);}       // paintings DC
        else if (one_in_(15)) {wall_type = 'g'; rng = rand_range(44, 51);} // small black holes AA
        else if (one_in_(10)) {wall_type = 'f'; rng = 34;}                 // moss small 98
        else if (one_in_(20)) {wall_type = 'f'; rng = rand_range(35, 36);} // moss small holes 98
        else if (one_in_(90)) {wall_type = 'g'; rng = rand_range(58, 60);} // bloody AA
        else    { /* pick generated rng */ }
    }

    else if (wall_type == 'c') // BB BC   big white
    {
        // door tiles.. bullutin boards and fire -> rare
        if ((rng == 4) || (rng == 32) || (rng == 33) || (rng == 34)) rng = randint0(63);
        if ((rng == 4) || (rng == 32) || (rng == 33) || (rng == 34)) rng = randint0(63);
        if ((rng == 4) || (rng == 32) || (rng == 33) || (rng == 34)) rng = randint0(63);        
        if (rng == 28) rng = 0; // no need big empty window

        if      (one_in_(3))   rng = 0;                                    // common big white
        else if (one_in_(25)) {wall_type = 'f'; rng = 19;}                 // grey wall 96
        else if (one_in_(50)) {wall_type = 'f'; rng = 29;}                 // metallic wall 96
        else if (one_in_(90)) {wall_type = 'g'; rng = rand_range(61, 63);} // big bloody AA
        else    { /* pick generated rng */ }
    }

    else if (wall_type == 'd') // BD BE   big black
    {

        if ((rng >= 9) && (rng <= 11))rng = randint0(63); // big black windows moss rare
        if ((rng >= 9) && (rng <= 11))rng = randint0(63);
        if ((rng == 5) || (rng == 7)) rng = randint0(63); // doors rare
        if ((rng == 5) || (rng == 7)) rng = randint0(63);
        if ((rng == 6) || (rng == 8)) rng = 1; // no need open doors (ugly)

        if (wall_id > 2) wall_id = randint1(2);  // cause 'd' got 2 subwalls

        if (wall_id == 1) // big black
        {
            if      (one_in_(3))   rng = 0;
            else if (one_in_(10)) {wall_type = 'g'; rng = rand_range(12, 19);} // big holes AA
            else if (one_in_(150)){wall_type = 'g'; rng = rand_range(61, 63);} // big bloody AA
            else if (one_in_(75)) {wall_type = 'f'; rng = 14;}                 // deep black 96
            else if (one_in_(30)) {wall_type = 'f'; rng = rand_range(16, 18);} // cracked earthy wall 96
            else if (one_in_(60)) {wall_type = 'f'; rng = rand_range(19, 28);} // etc 96
            else    { /* pick generated rng */ }
        }
        if (wall_id == 2) // big black mossy
        {
            if (one_in_(2)) rng = 1;
            else if (one_in_(25))  rng = rand_range(9,11);    // big black windows moss BD
            else if (one_in_(15)) {wall_type = 'f'; rng = rand_range(32, 33);} // moss big holes 98
            else if (one_in_(15)) {wall_type = 'f'; rng = 34;}                 // moss small 98
            else if (one_in_(25)) {wall_type = 'f'; rng = rand_range(35, 36);} // moss small holes 98
            else if (one_in_(15)) {wall_type = 'f'; rng = rand_range(37, 38);} // moss big holes 98
            else if (one_in_(25)) {wall_type = 'f'; rng = rand_range(39, 40);} // big black holes 98
            else if (one_in_(15)) {wall_type = 'f'; rng = rand_range(41, 43);} // moss big holes 98
            else if (one_in_(50)) {wall_type = 'f'; rng = 44;}                 // moss big white 98
            else if (one_in_(20)) {wall_type = 'f'; rng = 45;}                 // moss big grey 98
            else if (one_in_(90)) {wall_type = 'f'; rng = rand_range(32, 46);} // etc 98
            else if (one_in_(50)) {wall_type = 'f'; rng = 14;}                 // deep black 96
            else if (one_in_(50)) {wall_type = 'f'; rng = rand_range(16, 18);} // cracked earthy wall 96
            else if (one_in_(90)) {wall_type = 'f'; rng = rand_range(19, 28);} // etc 96
            else if (one_in_(150)){wall_type = 'g'; rng = rand_range(61, 63);} // big bloody AA
            else    { /* pick generated rng */ }
        }
    }

    else if (wall_type == 'e') // BF C0   white small bricks
    {
        if ((rng == 4) || (rng == 8) ||       // door tiles.. bullutin boards and fire -> rare
        (rng == 32) || (rng == 33) || (rng == 34)) rng = randint0(63);
        if ((rng == 4) || (rng == 8) ||       // very rare
        (rng == 32) || (rng == 33) || (rng == 34)) rng = randint0(63);
        if ((rng == 4) || (rng == 8) ||       // really rare :)
        (rng == 32) || (rng == 33) || (rng == 34)) rng = randint0(63);  
        if (rng == 9) rng = 1; // no need open doors (ugly)
        /* pick generated rng */
    }

    else if (wall_type == 'f') // 96 98
    {   /* 96 */
        if (wall_id > 12) wall_id = randint1(12);  // cause 'f' got 9 subwalls

        while ((rng == 0) || (rng == 1) || (rng == 2) || // exclude bad looking tiles
               (rng == 7) || (rng == 8) || (rng == 9) || // eg ice, lave, nether walls
               (rng == 11)|| (rng == 13)|| (rng == 30)||
               (rng == 31)) rng = randint0(63);

        if (wall_id == 1) // brown concrete
        {
            if      (one_in_(2))   rng = 3;
            else if (one_in_(5))   rng = 4;         // grey concrete
            else if (one_in_(15)) {wall_type = 'h'; rng = randint0(31);}       // paintings DC
            else if (one_in_(10))  wall_type = 'b'; // small black
            else if (one_in_(50))  wall_type = 'c'; // big white
            else if (one_in_(4))   wall_type = 'd'; // big black
            else if (one_in_(40))  wall_type = 'e'; // small white
            else if (one_in_(150)){wall_type = 'g'; rng = rand_range(61, 63);} // big bloody AA
            else    { /* pick generated rng */ }
        }
        if (wall_id == 2) // grey concrete
        {
            if      (one_in_(2))   rng = 4;
            else if (one_in_(5))   rng = 3;         // brown concrete
            else if (one_in_(15)) {wall_type = 'h'; rng = randint0(31);}       // paintings DC
            else if (one_in_(3))   wall_type = 'b'; // small black
            else if (one_in_(50))  wall_type = 'c'; // big white
            else if (one_in_(15))  wall_type = 'd'; // big black
            else if (one_in_(25))  wall_type = 'e'; // small white
            else if (one_in_(150)){wall_type = 'g'; rng = rand_range(61, 63);} // big bloody AA
            else if (one_in_(10)) {wall_type = 'f'; rng = 34;}                 // moss small 98
            else if (one_in_(25)) {wall_type = 'f'; rng = rand_range(35, 36);} // moss small holes 98
            else    { /* pick generated rng */ }
        }
        if (wall_id == 3) // brown sandstone
        {
            if      (one_in_(2))   rng = 10;
            else if (one_in_(50))  wall_type = 'a'; // wood
            else if (one_in_(10))  wall_type = 'b'; // small black
            else if (one_in_(15))  wall_type = 'c'; // big white
            else if (one_in_(10))  wall_type = 'd'; // big black
            else if (one_in_(15))  wall_type = 'e'; // small white
            else if (one_in_(50)) {wall_type = 'h'; rng = randint0(31);}       // paintings DC
            else if (one_in_(150)){wall_type = 'g'; rng = rand_range(61, 63);} // big bloody AA
            else if (one_in_(12)) {wall_type = 'g'; rng = rand_range(38, 39);} // greenish brown moss big AA
            else if (one_in_(22)) {wall_type = 'g'; rng = 40;}                 // dark brown moss big AA
            else if (one_in_(10)) {wall_type = 'g'; rng = rand_range(41, 43);} // brown moss AA
            else if (one_in_(15)) {wall_type = 'g'; rng = rand_range(52, 55);} // brown small AA
            else if (one_in_(5))  {wall_type = 'g'; rng = rand_range(56, 57);} // 2x brown big AA
            else    { /* pick generated rng */ }
        }
        if (wall_id == 4) // deep black wall
        {
            if      (one_in_(2))   rng = 14;
            else if (one_in_(50))  wall_type = 'a'; // wood
            else if (one_in_(5))   wall_type = 'b'; // small black
            else if (one_in_(50))  wall_type = 'c'; // big white
            else if (one_in_(3))   wall_type = 'd'; // big black
            else if (one_in_(25))  wall_type = 'e'; // small white
            else if (one_in_(150)){wall_type = 'g'; rng = rand_range(61, 63);} // big bloody AA
            else if (one_in_(15)) {wall_type = 'g'; rng = rand_range(38, 39);} // greenish brown moss big AA
            else if (one_in_(25)) {wall_type = 'g'; rng = 40;}                 // dark brown moss big AA
            else if (one_in_(10)) {wall_type = 'h'; rng = randint0(31);}       // paintings DC
            else    { /* pick generated rng */ }
        }
        if (wall_id == 5) // 3x cracked earthy wall
        {
            if      (one_in_(2))   rng = rand_range(16, 18);
            else if (one_in_(50))  wall_type = 'a'; // wood
            else if (one_in_(15))  wall_type = 'b'; // small black
            else if (one_in_(50))  wall_type = 'c'; // big white
            else if (one_in_(3))   wall_type = 'd'; // big black
            else if (one_in_(25))  wall_type = 'e'; // small white
            else if (one_in_(150)){wall_type = 'g'; rng = rand_range(61, 63);} // big bloody AA
            else if (one_in_(15)) {wall_type = 'g'; rng = rand_range(38, 39);} // greenish brown moss big AA
            else if (one_in_(25)) {wall_type = 'g'; rng = 40;}                 // dark brown moss big AA
            else if (one_in_(15)) {wall_type = 'h'; rng = randint0(31);}       // paintings DC
            else    { /* pick generated rng */ }
        }
        if (wall_id == 6) // 4x grey walls
        {
            if      (one_in_(2))   rng = rand_range(19, 22);
            else if (one_in_(25))  wall_type = 'a'; // wood
            else if (one_in_(15))  wall_type = 'b'; // small black
            else if (one_in_(3))   wall_type = 'c'; // big white
            else if (one_in_(15))  wall_type = 'd'; // big black
            else if (one_in_(5))   wall_type = 'e'; // small white
            else if (one_in_(150)){wall_type = 'g'; rng = rand_range(61, 63);} // big bloody AA
            else if (one_in_(30)) {wall_type = 'g'; rng = rand_range(38, 39);} // greenish brown moss big AA
            else if (one_in_(40)) {wall_type = 'g'; rng = 40;}                 // dark brown moss big AA
            else if (one_in_(20)) {wall_type = 'h'; rng = randint0(31);}       // paintings DC
            else    { /* pick generated rng */ }
        }
        if (wall_id == 7) // 3x cracked grey walls
        {
            if      (one_in_(2))   rng = rand_range(23, 25);
            else if (one_in_(70))  wall_type = 'a'; // wood
            else if (one_in_(10))  wall_type = 'b'; // small black
            else if (one_in_(50))  wall_type = 'c'; // big white
            else if (one_in_(2))   wall_type = 'd'; // big black
            else if (one_in_(50))  wall_type = 'e'; // small white
            else if (one_in_(150)){wall_type = 'g'; rng = rand_range(61, 63);} // big bloody AA
            else if (one_in_(30)) {wall_type = 'g'; rng = rand_range(38, 39);} // greenish brown moss big AA
            else if (one_in_(40)) {wall_type = 'g'; rng = 40;}                 // dark brown moss big AA
            else if (one_in_(15)) {wall_type = 'h'; rng = randint0(31);}       // paintings DC
            else    { /* pick generated rng */ }
        }
        if (wall_id == 8) // 2x muddy walls
        {
            if      (one_in_(2))   rng = rand_range(26, 27);
            else if (one_in_(150)) wall_type = 'a'; // wood
            else if (one_in_(5))   wall_type = 'b'; // small black
            else if (one_in_(150)) wall_type = 'c'; // big white
            else if (one_in_(3))   wall_type = 'd'; // big black
            else if (one_in_(150)) wall_type = 'e'; // small white
            else if (one_in_(150)){wall_type = 'g'; rng = rand_range(61, 63);} // big bloody AA
            else if (one_in_(90)) {wall_type = 'g'; rng = rand_range(38, 39);} // greenish brown moss big AA
            else if (one_in_(90)) {wall_type = 'g'; rng = 40;}                 // dark brown moss big AA
            else if (one_in_(15)) {wall_type = 'h'; rng = randint0(31);}       // paintings DC
            else    { /* pick generated rng */ }
        }
        if (wall_id == 9) // metallic walls
        {
            if      (one_in_(2))   rng = 29;
            else if (one_in_(50))  wall_type = 'a'; // wood
            else if (one_in_(150)) wall_type = 'b'; // small black
            else if (one_in_(150)) wall_type = 'd'; // big black
            else if (one_in_(3))   wall_type = 'e'; // small white
            else                   wall_type = 'c'; // big white;
        }

        /* 98 */

        if (wall_id == 10) // moss small
        {
            if      (one_in_(2))  rng = 34;
            else if (one_in_(15)) rng = rand_range(35, 36);   // moss small holes 98
            else if (one_in_(3)) {wall_type = 'd'; rng = 1;}  // big black moss BD
            else if (one_in_(15)){wall_type = 'd'; rng = rand_range(9,11);} // big black windows moss BD
            else if (one_in_(10)) rng = 45;                   // moss big grey 98
            else if (one_in_(20)) rng = 44;                   // moss big white 98
            else if (one_in_(5))  rng = rand_range(40, 42);
            else if (one_in_(20)) rng = rand_range(32, 33);   // big common moss holes
            else if (one_in_(6))  rng = rand_range(36, 37);
            else if (one_in_(7))  rng = rand_range(32, 45);   // etc 98
            else if (one_in_(7))  wall_type = 'd';            // big black
            else if (one_in_(3))  wall_type = 'b';            // small black
            else if (one_in_(150)){wall_type = 'g'; rng = rand_range(61, 63);} // big bloody AA
            else if (one_in_(50)) {wall_type = 'g'; rng = rand_range(38, 39);} // greenish brown moss big AA
            else if (one_in_(50)) {wall_type = 'g'; rng = 40;}                 // dark brown moss big AA
            else if (one_in_(50)) {wall_type = 'g'; rng = rand_range(41, 43);} // brown moss AA
            else if (one_in_(50)) {wall_type = 'g'; rng = rand_range(52, 55);} // brown small AA
            else if (one_in_(50)) {wall_type = 'g'; rng = rand_range(56, 57);} // 2x brown big AA
            else if (one_in_(2))  {wall_type = 'h'; rng = randint0(31);}       // paintings DC
            else    { /* pick generated rng */ }
        }
        if (wall_id == 11) // moss big white
        {
            if      (one_in_(2))   rng = 44;
            else if (one_in_(25))  rng = 45;        // moss big grey 98
            else if (one_in_(150)) wall_type = 'b'; // small black
            else if (one_in_(150)) wall_type = 'd'; // big black
            else if (one_in_(3))   wall_type = 'e'; // small white
            else                   wall_type = 'c'; // big white;
        }
        if (wall_id == 12) // moss big grey
        {
            if      (one_in_(2))   rng = 45;
            else if (one_in_(5))   rng = 44;                  // moss big grey 98
            else if (one_in_(50))  rng = 34;                  // moss small 98
            else if (one_in_(75)) {wall_type = 'd'; rng = 1;}// moss big black BD
            else if (one_in_(50))  rng = rand_range(35, 36);  // moss small holes 98
            else if (one_in_(40)) {wall_type = 'd'; rng = rand_range(9,11);} // big black windows moss BD
            else if (one_in_(30))  rng = rand_range(37, 38);  // moss big blocked walls 98
            else if (one_in_(125)) rng = rand_range(41, 43);  // moss big blocked walls   98
            else                   wall_type = 'c';           // big white
        }
    }

    else if (wall_type == 'g') // A3 AA
    {   /* A3 */

        if (wall_id > 6) wall_id = randint1(6); // cause 'g' got 6 subwalls

        if (wall_id == 1) // sewers A3 full + 6 AA
        {
            if (rng > 37) rng = randint0(37); // AA got got only 6 sewers tiles
        }

        /* AA */

        if (wall_id == 2) // x6 AA big walls
        {
            if      (one_in_(2))  rng = randint0(6);
            else if (one_in_(25)) wall_type = 'b';  // small black
            else if (one_in_(10)) wall_type = 'd';  // big black
            else if (one_in_(200)) wall_type = 'e';  // small white
            else    { /* pick generated rng */ }
        }
        if (wall_id == 3) // x6 AA mossy brown walls
        {
            if      (one_in_(3))   rng = rand_range(38, 39);
            else if (one_in_(3))   rng = rand_range(41, 43);
            else if (one_in_(50))  rng = 40; // too dark
            else if (one_in_(15)) {wall_type = 'd'; rng = 1;} // common big black mossy
            else if (one_in_(20))  wall_type = 'b'; // small black
            else if (one_in_(35)) {wall_type = 'f'; rng = rand_range(32, 33);} // moss big holes 98
            else if (one_in_(15)) {wall_type = 'f'; rng = 34;}                 // moss small 98
            else if (one_in_(65)) {wall_type = 'f'; rng = rand_range(35, 36);} // moss small holes 98
            else if (one_in_(45)) {wall_type = 'f'; rng = rand_range(37, 38);} // moss big holes 98
            else if (one_in_(65)) {wall_type = 'f'; rng = rand_range(39, 40);} // big black holes 98
            else if (one_in_(35)) {wall_type = 'f'; rng = rand_range(41, 43);} // moss big holes 98
            else if (one_in_(250)){wall_type = 'f'; rng = 44;}                 // moss big white 98
            else if (one_in_(190)){wall_type = 'f'; rng = 45;}                 // moss big grey 98
            else if (one_in_(350)){wall_type = 'f'; rng = rand_range(32, 46);} // etc 98
            else if (one_in_(50)) {wall_type = 'f'; rng = 14;}                 // deep black 96
            else if (one_in_(50)) {wall_type = 'f'; rng = rand_range(16, 18);} // cracked earthy wall 96
            else if (one_in_(90)) {wall_type = 'f'; rng = rand_range(19, 28);} // etc 96
            else if (one_in_(150)){wall_type = 'g'; rng = rand_range(61, 63);} // big bloody AA
            else                   wall_type = 'd';  // big black
        }
        if (wall_id == 4) // x4 AA brown small brick
        {
            if      (one_in_(2))  rng = rand_range(52, 55);
            else if (one_in_(5))  rng = 56;         // brown big AA
            else if (one_in_(7))  rng = 57;         // light brown big AA
            else if (one_in_(15)){wall_type = 'h'; rng = randint0(31);}       // paintings DC
            else                  wall_type = 'a';  // wood
        }
        if (wall_id == 5) // AA brown big wall
        {
            if      (one_in_(2))  rng = 56;
            else if (one_in_(7))  rng = rand_range(52, 55); // brown small AA
            else if (one_in_(5))  rng = 57;                 // light brown big AA
            else                  wall_type = 'a';  // wood
        }
        if (wall_id == 6) // AA light brown big wall
        {
            if      (one_in_(2))  rng = 57;
            else if (one_in_(8))  rng = rand_range(52, 55); // brown small AA
            else if (one_in_(5))  rng = 56;                 // brown big AA
            else                  wall_type = 'a';  // wood
        }
    }

    else if (wall_type == 'h') // DC E1... but we use only E1
    {
        if (rng < 32) rng = rand_range(32, 63);  // we don't want pictures there
        if ((rng >= 52) && (rng <= 55)) rng = rand_range(32, 63); // lights reroll
    }

    else if (wall_type == 'i') // E2 E3 separately
    {

        if (wall_id > 6) wall_id = randint1(6); // cause 'i' got 2 subwalls

        if (wall_id == 1) // E2
        {
            if (rng > 31) rng = randint0(31);
        }

        if (wall_id == 2) // E3
        {
            if (rng < 32) rng = rand_range(32, 63);
        }

        if (wall_id == 3) // E2+E3
        {
            /* pick generated rng */
        }

        if (wall_id == 4) // E1+E2
        {
            if (rng > 31)    rng = randint0(31);; // E2
            if (one_in_(2)) {wall_type = 'h'; rng = rand_range(32, 63);} // E1
        }

        if (wall_id == 5) // E1+E3
        {
            if (rng < 32) rng = rand_range(32, 63); //E3
            if (one_in_(2)) {wall_type = 'h'; rng = rand_range(32, 63);} // E1
        }

        if (wall_id == 6) // E1+E2+E3
        {
            if (one_in_(3)) {wall_type = 'h'; rng = rand_range(32, 63);} // E1
        }
    }

    else                       // E2 E3
    {
        wall_type = 'i';
    }

    // Add wall_type (e.g. 'a', 'b', ...) to the name
    len = strlen(wall);
    wall[len] = wall_type;
    wall[len+1] = '\0';

    // some walls not really good to be house-ones
    switch(rng)
    {   // 1st stroke in tileset
        case 0: break;
        case 1: break;
        case 2: break;
        case 3: break;
        case 4: break;
        case 5: break;
        case 6: break;
        case 7: break;
        case 8: break;
        case 9: break;
        case 10: wall_glyph = 'a'; break;
        case 11: wall_glyph = 'b'; break;
        case 12: wall_glyph = 'c'; break;
        case 13: wall_glyph = 'd'; break;
        case 14: wall_glyph = 'e'; break;
        case 15: wall_glyph = 'f'; break;
        case 16: wall_glyph = 'g'; break;
        case 17: wall_glyph = 'h'; break;
        case 18: wall_glyph = 'i'; break;
        case 19: wall_glyph = 'j'; break;
        case 20: wall_glyph = 'k'; break;
        case 21: wall_glyph = 'l'; break;
        case 22: wall_glyph = 'm'; break;
        case 23: wall_glyph = 'n'; break;
        case 24: wall_glyph = 'o'; break;
        case 25: wall_glyph = 'p'; break;
        case 26: wall_glyph = 'q'; break;
        case 27: wall_glyph = 'r'; break;
        case 28: wall_glyph = 's'; break;
        case 29: wall_glyph = 't'; break;
        case 30: wall_glyph = 'u'; break;
        case 31: wall_glyph = 'v'; break;
        // 2nd stroke in tileset
        case 32: wall_glyph = 'w'; break;
        case 33: wall_glyph = 'x'; break;
        case 34: wall_glyph = 'y'; break;
        case 35: wall_glyph = 'z'; break;
        case 36: wall_glyph = 'A'; break;
        case 37: wall_glyph = 'B'; break;
        case 38: wall_glyph = 'C'; break;
        case 39: wall_glyph = 'D'; break;
        case 40: wall_glyph = 'E'; break;
        case 41: wall_glyph = 'F'; break;
        case 42: wall_glyph = 'G'; break;
        case 43: wall_glyph = 'H'; break;
        case 44: wall_glyph = 'I'; break;
        case 45: wall_glyph = 'J'; break;
        case 46: wall_glyph = 'K'; break;
        case 47: wall_glyph = 'L'; break;
        case 48: wall_glyph = 'M'; break;
        case 49: wall_glyph = 'N'; break;
        case 50: wall_glyph = 'O'; break;
        case 51: wall_glyph = 'P'; break;
        case 52: wall_glyph = 'Q'; break;
        case 53: wall_glyph = 'R'; break;
        case 54: wall_glyph = 'S'; break;
        case 55: wall_glyph = 'T'; break;
        case 56: wall_glyph = 'U'; break;
        case 57: wall_glyph = 'V'; break;
        case 58: wall_glyph = 'W'; break;
        case 59: wall_glyph = 'X'; break;
        case 60: wall_glyph = 'Y'; break;
        case 61: wall_glyph = 'Z'; break;
        case 62: wall_glyph = '~'; break;
        case 63: wall_glyph = '`'; break;
        default: rng = 0;
    }

    // Convert rng to character (digit or tile glyph)
    len = strlen(wall);

    if (rng <= 9)
        wall[len] = '0' + rng;   // 0..9 as '0'..'9'
    else
        wall[len] = wall_glyph; // use preselected glyph
    wall[len + 1] = '\0';

    // look for the feat
    house_wall = lookup_feat_code(wall);

    square_set_feat(c, grid, house_wall);
}

// not using in new house building
void square_build_permhouse(struct chunk *c, struct loc *grid)
{
    square_set_feat(c, grid, FEAT_PERM_HOUSE);
}


void square_dry_fountain(struct chunk *c, struct loc *grid)
{
    int feat = FEAT_FNT_DRIED;
    struct worldpos dpos;
    struct location *dungeon;

    /* Get the dungeon */
    wpos_init(&dpos, &c->wpos.grid, 0);
    dungeon = get_dungeon(&dpos);
    if (dungeon && c->wpos.depth)
    {
        int i;

        /* Use the corresponding dried out fountain instead */
        for (i = 0; i < dungeon->n_fountains; i++)
        {
            struct dun_feature *feature = &dungeon->fountains[i];

            if (square(c, grid)->feat == feature->feat)
            {
                feat = feature->feat2;
                break;
            }
        }
    }

    square_set_feat(c, grid, feat);
}


void square_clear_feat(struct chunk *c, struct loc *grid)
{
    square_set_floor(c, grid, FEAT_FLOOR);
}


void square_add_wall(struct chunk *c, struct loc *grid)
{
    square_set_wall(c, grid, FEAT_GRANITE);
}


void square_add_tree(struct chunk *c, struct loc *grid)
{
    char tr33[30] = "";  // tree terrain feature
    int rng = 0;
    int tree_index = 0;         // result: index of terrain feature

    // random choice of tree
    rng = randint0(18);

    switch(rng)
    {
        case 0: strncpy(tr33, "tree 1", 7); break;
        case 1: strncpy(tr33, "tree 3", 7); break;
        case 2: strncpy(tr33, "tree 4 maple", 13); break;
        case 3: strncpy(tr33, "tree 5 maple", 13); break;
        case 4: strncpy(tr33, "tree 6 coniferous", 18); break;
        case 5: strncpy(tr33, "tree 8 coniferous", 18); break;
        case 6: strncpy(tr33, "tree 9 willow", 14); break;
        case 7: strncpy(tr33, "tree d birch", 13); break;
        case 8: strncpy(tr33, "tree e linden", 14); break;
        case 9: strncpy(tr33, "tree g larch", 13); break;
        case 10:strncpy(tr33, "tree n dead tree", 17); break;
        case 11:strncpy(tr33, "tree o dead tree", 17); break;
        case 12:strncpy(tr33, "tree D tree", 12); break;
        case 13:strncpy(tr33, "tree E tree", 12); break;
        case 14:strncpy(tr33, "tree F tree", 12); break;
        case 15:strncpy(tr33, "tree V", 7); break;
        case 16:strncpy(tr33, "tree W", 7); break;
        case 17:strncpy(tr33, "tree X", 7); break;
        case 18:strncpy(tr33, "tree Y", 7); break;
        default:strncpy(tr33, "tree 1", 7);
    }
    
    // look for the feat
    tree_index = lookup_feat_code(tr33);

    square_set_feat(c, grid, tree_index);
}


void square_add_dirt(struct chunk *c, struct loc *grid)
{
    square_set_feat(c, grid, FEAT_DIRT);
}


void square_add_grass(struct chunk *c, struct loc *grid)
{
    square_set_feat(c, grid, FEAT_GRASS);
}

/* get random floor feat for house building */
void square_add_new_safe(struct chunk *c, struct loc *grid)
{
    char floor[20] = "house floor "; // 1st part of floor name
    char floor_type = '\0';     // second part of floor name (type)
    int rng = 0;                // third part of the floor name
    char floor_glyph = '\0';
    int house_floor = 0;         // result: index of terrain feature
    int len; // measure length of a string

    rng = randint0(63); // random choice of floor number

    if (one_in_(9))
    {
        floor_type = 'a'; // AD AE
        if ((rng >= 35) && (rng <= 43)) rng = 45; // if roll NPC - safe floor
        if (one_in_(3)) rng = randint0(63); // but sometimes give them chance
        if (rng == 44) rng = 45; // if roll door - safe floor
    }

    else if (one_in_(9))
    {
        floor_type = 'b'; // AF B0
    }

    else if (one_in_(9))
    {
        floor_type = 'c'; // D1 D2
    }

    else if (one_in_(9))
    {
        floor_type = 'd'; // D3 D4
    }

    else if (one_in_(9))
    {
        floor_type = 'e'; // D5 D6
    }

    else if (one_in_(9))
    {
        floor_type = 'f'; // D7 D8
    }

    else if (one_in_(9))
    {
        floor_type = 'g'; // D9 DA
    }

    else if (one_in_(9))
    {
        floor_type = 'h'; // DB B4
        if (rng > 43)     rng = randint0(43); // after carpets and pentagrams there are bad tiles
        if (one_in_(50))  rng = randint0(46); // but rarely generate them too: stone plates
        if (one_in_(500)) rng = randint0(63); // and very rare dark floors
    }

    else if (one_in_(9))
    {
        floor_type = 'i'; // B5 A4
        if (rng > 31) rng = 0; // safe floor as second part of 'i' is grassy A4 terrain which looks bad inside atm..
    }

    else
    {
        floor_type = 'i';
        rng = 0; // add some more common safe floors
    }

    // combine 1st ("house floor ") and 2nd (type "a-i") part of the wall name
    len = strlen(floor);
    floor[len] = floor_type;
    floor[len + 1] = '\0';

    switch(rng)
    {   // 1st stroke in tileset
        case 0: break;
        case 1: break;
        case 2: break;
        case 3: break;
        case 4: break;
        case 5: break;
        case 6: break;
        case 7: break;
        case 8: break;
        case 9: break;
        case 10: floor_glyph = 'a'; break;
        case 11: floor_glyph = 'b'; break;
        case 12: floor_glyph = 'c'; break;
        case 13: floor_glyph = 'd'; break;
        case 14: floor_glyph = 'e'; break;
        case 15: floor_glyph = 'f'; break;
        case 16: floor_glyph = 'g'; break;
        case 17: floor_glyph = 'h'; break;
        case 18: floor_glyph = 'i'; break;
        case 19: floor_glyph = 'j'; break;
        case 20: floor_glyph = 'k'; break;
        case 21: floor_glyph = 'l'; break;
        case 22: floor_glyph = 'm'; break;
        case 23: floor_glyph = 'n'; break;
        case 24: floor_glyph = 'o'; break;
        case 25: floor_glyph = 'p'; break;
        case 26: floor_glyph = 'q'; break;
        case 27: floor_glyph = 'r'; break;
        case 28: floor_glyph = 's'; break;
        case 29: floor_glyph = 't'; break;
        case 30: floor_glyph = 'u'; break;
        case 31: floor_glyph = 'v'; break;
        // 1st stroke in tileset
        case 32: floor_glyph = 'w'; break;
        case 33: floor_glyph = 'x'; break;
        case 34: floor_glyph = 'y'; break;
        case 35: floor_glyph = 'z'; break;
        case 36: floor_glyph = 'A'; break;
        case 37: floor_glyph = 'B'; break;
        case 38: floor_glyph = 'C'; break;
        case 39: floor_glyph = 'D'; break;
        case 40: floor_glyph = 'E'; break;
        case 41: floor_glyph = 'F'; break;
        case 42: floor_glyph = 'G'; break;
        case 43: floor_glyph = 'H'; break;
        case 44: floor_glyph = 'I'; break;
        case 45: floor_glyph = 'J'; break;
        case 46: floor_glyph = 'K'; break;
        case 47: floor_glyph = 'L'; break;
        case 48: floor_glyph = 'M'; break;
        case 49: floor_glyph = 'N'; break;
        case 50: floor_glyph = 'O'; break;
        case 51: floor_glyph = 'P'; break;
        case 52: floor_glyph = 'Q'; break;
        case 53: floor_glyph = 'R'; break;
        case 54: floor_glyph = 'S'; break;
        case 55: floor_glyph = 'T'; break;
        case 56: floor_glyph = 'U'; break;
        case 57: floor_glyph = 'V'; break;
        case 58: floor_glyph = 'W'; break;
        case 59: floor_glyph = 'X'; break;
        case 60: floor_glyph = 'Y'; break;
        case 61: floor_glyph = 'Z'; break;
        case 62: floor_glyph = '~'; break;
        case 63: floor_glyph = '`'; break;
        default: rng = 0;
    }

    // Convert rng to character (digit or tile glyph)
    len = strlen(floor);

    if (rng <= 9)
        floor[len] = '0' + rng; // 0..9 as '0'..'9'
    else
        floor[len] = floor_glyph; // use preselected glyph
    floor[len + 1] = '\0';

    // look for the feat
    house_floor = lookup_feat_code(floor);

    square_set_feat(c, grid, house_floor);
}

void square_add_safe(struct chunk *c, struct loc *grid)
{
    square_set_feat(c, grid, FEAT_FLOOR_SAFE);
}


bool square_isplot(struct chunk *c, struct loc *grid)
{
    return tf_has(f_info[square(c, grid)->feat].flags, TF_PLOT);
}

bool square_is_no_house(struct chunk *c, struct loc *grid)
{
    return tf_has(f_info[square(c, grid)->feat].flags, TF_NO_HOUSE);
}

bool square_is_window(struct chunk *c, struct loc *grid)
{
    return tf_has(f_info[square(c, grid)->feat].flags, TF_WINDOW);
}

void square_actor(struct chunk *c, struct loc *grid, struct source *who)
{
    int m_idx;

    memset(who, 0, sizeof(struct source));
    if (!square_in_bounds(c, grid)) return;

    m_idx = square(c, grid)->mon;
    if (!m_idx) return;

    who->idx = abs(m_idx);
    who->player = ((m_idx < 0)? player_get(0 - m_idx): NULL);
    who->monster = ((m_idx > 0)? cave_monster(c, m_idx): NULL);
}


int square_known_feat(struct player *p, struct chunk *c, struct loc *grid)
{
    if (p->dm_flags & DM_SEE_LEVEL) return square(c, grid)->feat;
    return square_p(p, grid)->feat;
}


static bool normal_grid(struct chunk *c, struct loc *grid)
{
    return ((((c->wpos.depth > 0) || town_area(&c->wpos)) && square_isnormal(c, grid)) ||
        square_isroom(c, grid));
}


/*
 * Light or darken a square
 * Also applied for wilderness and special levels
 */
void square_illuminate(struct player *p, struct chunk *c, struct loc *grid, bool daytime, bool light)
{
    /* Only interesting grids at night */
    if (normal_grid(c, grid) || daytime || (c->wpos.depth > 0))
    {
        sqinfo_on(square(c, grid)->info, SQUARE_GLOW);
        if (p && light) square_memorize(p, c, grid);
    }
    else
    {
        square_unglow(c, grid);

        /* Hack -- like cave_unlight(), forget "boring" grids */
        if (p && square_isview(p, grid) && !square_isnormal(c, grid))
            square_forget(p, grid);
    }
}


struct trap *square_top_trap(struct chunk *c, struct loc *grid)
{
    struct trap *trap = NULL;

    if (square_istrap(c, grid))
    {
        trap = square(c, grid)->trap;

        /* Scan the square trap list */
        while (trap)
        {
            if ((trf_has(trap->flags, TRF_TRAP) && trf_has(trap->flags, TRF_VISIBLE)) ||
                trf_has(trap->flags, TRF_GLYPH))
            {
                /* Accept the trap -- only if not disabled */
                if (!trap->timeout) break;
            }
            trap = trap->next;
        }
    }

    return trap;
}


void square_memorize_trap(struct player *p, struct chunk *c, struct loc *grid)
{
    struct trap *trap;

    if (!wpos_eq(&p->wpos, &c->wpos)) return;

    /* Remove current knowledge */
    square_forget_trap(p, grid);

    /* Memorize first visible trap */
    trap = square_top_trap(c, grid);
    if (trap)
    {
        square_p(p, grid)->trap = mem_zalloc(sizeof(struct trap));
        square_p(p, grid)->trap->kind = trap->kind;
        loc_copy(&square_p(p, grid)->trap->grid, &trap->grid);
        trf_copy(square_p(p, grid)->trap->flags, trap->flags);
    }
}


struct trap *square_known_trap(struct player *p, struct chunk *c, struct loc *grid)
{
    /* Hack -- DM has full knowledge */
    if (p->dm_flags & DM_SEE_LEVEL) return square_top_trap(c, grid);

    return square_p(p, grid)->trap;
}


void square_forget_trap(struct player *p, struct loc *grid)
{
    if (square_p(p, grid)->trap)
    {
        mem_free(square_p(p, grid)->trap);
        square_p(p, grid)->trap = NULL;
    }
}


void square_init_join_up(struct chunk *c)
{
    loc_init(&c->join->up, 0, 0);
}


void square_set_join_up(struct chunk *c, struct loc *grid)
{
    loc_copy(&c->join->up, grid);
}


void square_init_join_down(struct chunk *c)
{
    loc_init(&c->join->down, 0, 0);
}


void square_set_join_down(struct chunk *c, struct loc *grid)
{
    loc_copy(&c->join->down, grid);
}


void square_init_join_rand(struct chunk *c)
{
    loc_init(&c->join->rand, 0, 0);
}


void square_set_join_rand(struct chunk *c, struct loc *grid)
{
    loc_copy(&c->join->rand, grid);
}


/*
 * Set an up staircase at position (y,x)
 */
void square_set_upstairs(struct chunk *c, struct loc *grid)
{
    /* Clear previous contents, add up stairs */
    if (cfg_limit_stairs < 2) square_set_stairs(c, grid, FEAT_LESS);

    /* Set this to be the starting location for people going down */
    square_set_join_down(c, grid);
}


/*
 * Set a down staircase at position (y,x)
 */
void square_set_downstairs(struct chunk *c, struct loc *grid, int feat)
{
    /* Clear previous contents, add down stairs */
    square_set_stairs(c, grid, feat);

    /* Set this to be the starting location for people going up */
    square_set_join_up(c, grid);
}


void square_set_rubble(struct chunk *c, struct loc *grid, int feat)
{
    struct worldpos dpos;
    struct location *dungeon;

    /* Get the dungeon */
    wpos_init(&dpos, &c->wpos.grid, 0);
    dungeon = get_dungeon(&dpos);
    if (dungeon && c->wpos.depth)
    {
        int i, chance;

        /* Basic chance */
        chance = randint0(10000);

        /* Get a random rubble tile */
        for (i = 0; i < dungeon->n_rubbles; i++)
        {
            struct dun_feature *feature = &dungeon->rubbles[i];

            if (feature->chance > chance)
            {
                if (feat == FEAT_RUBBLE) feat = feature->feat;
                else feat = feature->feat2;
                break;
            }

            chance -= feature->chance;
        }
    }

    square_set_feat(c, grid, feat);
}
