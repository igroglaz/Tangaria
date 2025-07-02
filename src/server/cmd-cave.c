/*
 * File: cmd-cave.c
 * Purpose: Chest and door opening/closing, disarming, running, resting, ...
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


static const char *get_house_type_desc(int area_size)
{
    if (area_size >= 324) return "castle";
    else if (area_size >= 256) return "keep";
    else if (area_size >= 144) return "tower";
    else if (area_size >= 100) return "estate";
    else if (area_size >= 64) return "mansion";
    else if (area_size >= 44) return "great villa";
    else if (area_size >= 32) return "villa";
    else if (area_size >= 25) return "luxury house";
    else if (area_size >= 20) return "spacious house";
    else if (area_size >= 15) return "big house";
    else if (area_size >= 9)  return "cozy house";
    else if (area_size >= 8)  return "wide house";
    else if (area_size >= 7)  return "small house";
    else if (area_size >= 6)  return "compact house";
    else if (area_size >= 5)  return "very small house";
    else if (area_size >= 4)  return "tiny house";
    else if (area_size >= 3)  return "modest cabin";
    else if (area_size >= 2)  return "small cabin";
    else return "tiny cabin";
}


int set_zeitnot_timer(int depth) {
    int timer;

    if (depth <= 20) {
        timer = 53 * depth + 947;  // 1=1000, 10=1500, 20=2000
    } else if (depth <= 30) {
        timer = 100 * depth;       // 21=2100, 25=2500, 30=3000
    } else if (depth <= 50) {
        timer = 100 * depth;       // 31=3100, 40=4000, 50=5000
    } else if (depth <= 100) {
        timer = 80 * depth + 1000; // 51=5100, 75=7000, 100=9000
    } else {
        timer = 222 * depth - 13222; // 101=9500, 110=11200, 127=15000
    }

    return timer;
}


// get the minimum required depth for a given player level in deeptown mode
static int get_deeptown_min_depth(int player_level)
{
    // Lookup table for levels 1-50 (index 0 unused for clarity)
    static const int depth_table[] = {
        0,  // index 0 (unused)
        1,  // level 1 -> depth 1
        1,  // level 2 -> depth 1  
        2,  // level 3 -> depth 2
        2,  // level 4 -> depth 2
        3,  // level 5 -> depth 3
        3,  // level 6 -> depth 3
        4,  // level 7 -> depth 4
        4,  // level 8 -> depth 4
        5,  // level 9 -> depth 5
        5,  // level 10 -> depth 5
        6, 6, 7, 7, 8,              // levels 11-15
        9, 10, 11, 12, 13,          // levels 16-20
        14, 15, 16, 17, 18,         // levels 21-25
        19, 20, 21, 22, 23,         // levels 26-30
        24, 25, 26, 27, 28,         // levels 31-35
        30, 32, 34, 36, 38,         // levels 36-40
        41, 44, 47, 50, 53,         // levels 41-45
        57, 61, 65, 70, 75          // levels 46-50
    };
    
    if (player_level >= 1 && player_level <= 50)
        return depth_table[player_level];
    
    return 1; // fallback for invalid levels
}


/*
 * Go up one level
 */
void do_cmd_go_up(struct player *p)
{
    int ascend_to;
    uint8_t new_level_method;
    struct chunk *c = chunk_get(&p->wpos);
    struct worldpos wpos;

    /* Can't go up on the surface */
    if (p->wpos.depth == 0)
    {
        msg(p, "There is nothing above you.");
        return;
    }

    /* Check preventive inscription '^<' */
    if (check_prevent_inscription(p, INSCRIPTION_UP))
    {
        msg(p, "The item's inscription prevents it.");
        return;
    }

    /* Verify stairs */
    if (p->timed[TMD_PROBTRAVEL] && !streq(p->clazz->name, "Assassin"))
        ;
    else if (!square_isupstairs(c, &p->grid) && !p->ghost)
    {
        msg(p, "I see no up staircase here.");
        return;
    }

    /* Force down */
    if (OPT(p, birth_zeitnot) && !is_dm_p(p))
    {
        msg(p, "Zeitnot! You can not go back. Can not.");
        return;
    } else if (player_force_descend(p, 2))
    {
        /* Going up is forbidden (except ghosts) */
        if (!p->ghost)
        {
            msg(p, "Morgoth awaits you in the darkness below.");
            return;
        }
    }

    /* No going up from quest levels with force_descend while the quest is active */
    if (player_force_descend(p, 3) && is_quest_active(p, p->wpos.depth))
    {
        msg(p, "Something prevents you from going up...");
        return;
    }

    // deeptown: special going up rules based on player level
    // allow going up to 1 level above minimum required depth, or directly to surface
    if (OPT(p, birth_deeptown) && !is_dm_p(p))
    {
        int allowed_shallow_depth;
        int min_depth_for_level = get_deeptown_min_depth(p->max_lev);

        allowed_shallow_depth = min_depth_for_level - 1;
        if (allowed_shallow_depth < 1) allowed_shallow_depth = 1;

        // if player is deeper than allowed shallow depth, can go up normally
        if (p->wpos.depth > allowed_shallow_depth)
        {
            // normal ascension
        }
        // if player is at or below allowed shallow depth, teleport to surface
        else if (p->wpos.depth <= allowed_shallow_depth)
        {
            ascend_to = 0; // go directly to surface
        }
        // fallback (shouldn't be reachable)
        else
        {
            msg(p, "You must delve deeper into the darkness.");
            return;
        }
    }

    // calculate ascend_to after deeptown logic
    if (OPT(p, birth_deeptown) && ascend_to == 0)
    {
        // already set to surface by deeptown logic
    }
    else
    {
        ascend_to = dungeon_get_next_level(p, p->wpos.depth, -1);
    }

    wpos_init(&wpos, &p->wpos.grid, ascend_to);

    /* Hack -- DM redesigning the level */
    if (chunk_inhibit_players(&wpos))
    {
        msg(p, "Something prevents you from going up...");
        return;
    }

    /* Take a turn */
    use_energy(p);

    /* Success */
    if (square_isupstairs(c, &p->grid))
    {
        msgt(p, MSG_STAIRS_UP, "You enter a maze of up staircases.");
        new_level_method = LEVEL_UP;
    }
    else
    {
        msg(p, "You float upwards.");
        new_level_method = LEVEL_GHOST;
    }

    /* Change level */
    dungeon_change_level(p, c, &wpos, new_level_method);
}


/*
 * Go down one level
 */
void do_cmd_go_down(struct player *p)
{
    int descend_to;
    uint8_t new_level_method;
    struct chunk *c = chunk_get(&p->wpos);
    struct wild_type *w_ptr = get_wt_info_at(&p->wpos.grid);
    struct worldpos wpos;

    /* Can't go down if no dungeon */
    if (w_ptr->max_depth == 1)
    {
        msg(p, "There is nothing below you.");
        return;
    }

    /* Ghosts who are not dungeon masters can't go down if ghost_diving is off */
    if (p->ghost && !cfg_ghost_diving && !is_dm_p(p))
    {
        msg(p, "You seem unable to go down. Try going up.");
        return;
    }

    /* Check preventive inscription '^>' */
    if (check_prevent_inscription(p, INSCRIPTION_DOWN))
    {
        msg(p, "The item's inscription prevents it.");
        return;
    }

    /* Verify stairs */
    if (p->timed[TMD_PROBTRAVEL] && !streq(p->clazz->name, "Timeturner"))
        ;
    else if (!square_isdownstairs(c, &p->grid) && !p->ghost)
    {
        msg(p, "I see no down staircase here.");
        return;
    }

    /* Paranoia, no descent from max_depth - 1 */
    if (p->wpos.depth == w_ptr->max_depth - 1)
    {
        msg(p, "The dungeon does not appear to extend deeper.");
        return;
    }

    /* Verify basic quests */
    if (is_quest_active(p, p->wpos.depth))
    {
        msg(p, "Something prevents you from going down...");
        return;
    }

    /* Winner-only dungeons */
    if (forbid_entrance_weak(p))
    {
        msg(p, "You're not powerful enough to enter!");
        return;
    }

    /* Shallow dungeons */
    if (forbid_entrance_strong(p))
    {
        msg(p, "You're too powerful to enter!");
        return;
    }

    // deeptown: from surface you can go down only on certain lvls
    // (prevent low lvl farm)
    if (OPT(p, birth_deeptown) && p->wpos.depth == 0 && !is_dm_p(p))
    {
        descend_to = get_deeptown_min_depth(p->max_lev);
        
        // ensure descend_to doesn't exceed player's maximum depth
        if (descend_to > p->max_depth)
            descend_to = p->max_depth;

        // but if player hasn't explored any dungeons yet, allow level 1
        if (descend_to == 0)
            descend_to = 1;
    }
    // regular PWMA case
    else
        descend_to = dungeon_get_next_level(p, p->wpos.depth, 1);

    /* Warn a force_descend player if they're going to a quest level */
    if (player_force_descend(p, 3))
    {
        descend_to = dungeon_get_next_level(p, p->max_depth, 1);

        /* Ask for confirmation */
        if (is_quest_active(p, descend_to) && !p->current_action)
        {
            p->current_action = ACTION_GO_DOWN;
            get_item(p, HOOK_DOWN, "");
            return;
        }
    }

    wpos_init(&wpos, &p->wpos.grid, descend_to);

    /* Hack -- DM redesigning the level */
    if (chunk_inhibit_players(&wpos))
    {
        msg(p, "Something prevents you from going down...");
        return;
    }

    /* Take a turn */
    use_energy(p);

    /* Success */
    if (square_isdownstairs(c, &p->grid))
    {
        // exception: entering Old Ruins. We don't wanna play stair sound there
        // as we have there sound with a sliding door: sound(p, MSG_ENTER_RUINS)
        if (p->wpos.depth != 1 && p->exp != 0)
            msgt(p, MSG_STAIRS_DOWN, "You enter a maze of down staircases.");
        new_level_method = LEVEL_DOWN;
    }
    else
    {
        msg(p, "You float downwards.");
        new_level_method = LEVEL_GHOST;
    }

    // when zeitnot use stairs down - it resets zeitnot_timer (auto > player)
    if (OPT(p, birth_zeitnot)) {
        p->zeitnot_timer = set_zeitnot_timer(p->wpos.depth);
        plog_fmt("do_cmd_go_down(): Zeitnot '%s' resets iron_time by using stairs", p->name);
    }

    /* Change level */
    dungeon_change_level(p, c, &wpos, new_level_method);
}


/*
 * Toggle stealth mode
 */
void do_cmd_toggle_stealth(struct player *p)
{
    /* Stop stealth mode */
    if (p->stealthy)
    {
        p->stealthy = false;
        p->upkeep->update |= (PU_BONUS);
        p->upkeep->redraw |= (PR_STATE);
    }

    /* Start stealth mode */
    else
    {
        p->stealthy = true;
        p->upkeep->update |= (PU_BONUS);
        p->upkeep->redraw |= (PR_STATE | PR_SPEED);
    }
}


/*
 * Determine if a given grid may be "opened"
 */
static bool do_cmd_open_test(struct player *p, struct chunk *c, struct loc *grid)
{
    /* Ghosts cannot open things */
    if ((p->ghost && !(p->dm_flags & DM_GHOST_HANDS)) || p->timed[TMD_WRAITHFORM])
    {
        msg(p, "You cannot open things!");
        return false;
    }

    /* Must have knowledge */
    if (!square_isknown(p, grid))
    {
        msg(p, "You see nothing there.");
        return false;
    }

    /* Must be a closed door */
    if (!square_iscloseddoor(c, grid))
    {
        msgt(p, MSG_NOTHING_TO_OPEN, "You see nothing there to open.");
        if (square_iscloseddoor_p(p, grid))
        {
            square_forget(p, grid);
            square_light_spot_aux(p, c, grid);
        }
        return false;
    }

    /* Handle polymorphed players */
    
    // can't open only if player is inside dungeon (to have access to houses)
    if (p->poly_race && !OPT(p, birth_fruit_bat) && p->wpos.depth)
    {
        bool can_open = rf_has(p->poly_race->flags, RF_OPEN_DOOR);
        bool can_bash = rf_has(p->poly_race->flags, RF_BASH_DOOR);

        if (!can_open && !can_bash)
        {
            msg(p, "You cannot open things!");
            return false;
        }
    }

    return true;
}


/*
 * Perform the basic "open" command on doors
 *
 * Assume there is no monster blocking the destination
 *
 * Returns true if repeated commands may continue
 */
static bool do_cmd_open_aux(struct player *p, struct chunk *c, struct loc *grid)
{
    bool more = false;
    struct house_type *house;
    bool can_open = (OPT(p, birth_fruit_bat) || !p->poly_race ||
        rf_has(p->poly_race->flags, RF_OPEN_DOOR));
    bool can_bash = (p->poly_race && !OPT(p, birth_fruit_bat) &&
        rf_has(p->poly_race->flags, RF_BASH_DOOR));

    /* Verify legality */
    if (!do_cmd_open_test(p, c, grid)) return false;

    /* Player Houses */
    if (square_home_iscloseddoor(c, grid))
    {
        int i, k;

        i = pick_house(&p->wpos, grid);
        if (i == -1)
        {
            plog_fmt("No house found at W (%d, %d), X=%d, Y=%d !", p->wpos.grid.x, p->wpos.grid.y,
                grid->x, grid->y);
            return false;
        }

        house = house_get(i);

        /* Tell the DM who owns the house */
        if (p->dm_flags & DM_HOUSE_CONTROL)
        {
            // This message contain players account, not character name!
            if (house->ownerid > 0)
                msg(p, "This house belongs to account: %s.", house->ownername);
            else
                msg(p, "This house is not owned.");
        }

        /* Do we own this house? */
        else if (house_owned_by(p, i))
        {
            /* If someone is in our store, we eject them (anti-exploit) */
            for (k = 1; k <= NumPlayers; k++)
            {
                struct player *q = player_get(k);

                if (p == q) continue;

                /* Eject any shopper */
                if (q->player_store_num == i)
                {
                    struct store *s = store_at(q);

                    if (s && (s->feat == FEAT_STORE_PLAYER))
                    {
                        msg(q, "The shopkeeper locks the doors.");
                        Send_store_leave(q);
                    }
                }
            }

            /* Open the door */
            square_open_homedoor(c, grid);

            // refresh house (update when we last time opened door)
            time(&house->last_visit_time);

            /* Update the visuals */
            square_memorize(p, c, grid);
            square_light_spot_aux(p, c, grid);
            update_visuals(&p->wpos);
        }

        /* He's not the owner, check if owned */
        else if (house->ownerid > 0)
        {
            /* Player owned store! */

            /* Disturb */
            disturb(p, 0);

            /* Hack -- enter store */
            do_cmd_store(p, i);
        }

        /* Not owned */
        else
        {
            /* Tell him the price */
            msg(p, "This house costs %ld gold.", house->price);
        }
    }

    /* Locked door */
    else if (square_islockeddoor(c, grid))
    {
        int chance = calc_unlocking_chance(p, square_door_power(c, grid), no_light(p));

        /* Success */
        if (magik(chance))
        {
            if (can_bash && !can_open)
            {
                msgt(p, MSG_DOOR_BROKEN, "You have bashed down the door.");
                square_smash_door(c, grid);
            }
            else
            {
                /* Message */
                msgt(p, MSG_LOCKPICK, "You have picked the lock.");

                /* Open the door */
                square_open_door(c, grid);
            }

            /* Update the visuals */
            square_memorize(p, c, grid);
            square_light_spot_aux(p, c, grid);
            update_visuals(&p->wpos);
        }

        /* Failure */
        else
        {
            /* Message */
            if (can_bash && !can_open)
                msgt(p, MSG_MISS, "You failed to bash down the door.");
            else
                msgt(p, MSG_LOCKPICK_FAIL, "You failed to pick the lock.");

            /* We may keep trying */
            more = true;
        }
    }

    /* Closed door */
    else
    {
        /* Open the door */
        if (can_bash && !can_open)
        {
            msgt(p, MSG_DOOR_BROKEN, "Can't open.. You have bashed down the door.");
            square_smash_door(c, grid);
        }
        else
            square_open_door(c, grid);

        /* Update the visuals */
        square_memorize(p, c, grid);
        square_light_spot_aux(p, c, grid);
        update_visuals(&p->wpos);

        /* Sound */
        if (can_bash && !can_open) sound(p, MSG_HIT);
        else sound(p, MSG_OPENDOOR);
    }

    /* Result */
    return (more);
}


/*
 * Monster or player in the way
 */
static bool is_blocker(struct player *p, struct chunk *c, struct loc *grid, bool allow_5)
{
    if (!square(c, grid)->mon) return false;
    if (!player_is_at(p, grid)) return true;
    return !allow_5;
}


/*
 * Attack monster or player in the way
 */
static void attack_blocker(struct player *p, struct chunk *c, struct loc *grid)
{
    struct source who_body;
    struct source *who = &who_body;

    square_actor(c, grid, who);

    /* Monster in the way */
    if (who->monster)
    {
        /* Camouflaged monsters surprise the player */
        if (monster_is_camouflaged(who->monster))
        {
            become_aware(p, c, who->monster);

            /* Camouflaged monster wakes up and becomes aware */
            if (pvm_check(p, who->monster))
                monster_wake(p, who->monster, false, 100);
        }
        else
        {
            /* Message */
            msg(p, "There is a monster in the way!");

            /* Attack */
            if (pvm_check(p, who->monster)) py_attack(p, c, grid);
        }
    }

    /* Player in the way */
    else
    {
        /* Reveal mimics */
        if (who->player->k_idx)
            aware_player(p, who->player);

        /* Message */
        msg(p, "There is a player in the way!");

        /* Attack */
        if (pvp_check(p, who->player, PVP_DIRECT, true, square(c, grid)->feat))
            py_attack(p, c, grid);
    }
}


/*
 * Open a closed/locked door or a closed/locked chest.
 *
 * Unlocking a locked chest is worth one experience point; since doors are
 * player lockable, there is no experience for unlocking doors.
 */
void do_cmd_open(struct player *p, int dir, bool easy)
{
    struct loc grid;
    struct object *obj;
    bool more = false, allow_5 = false;
    struct chunk *c = chunk_get(&p->wpos);

    /* Easy Open */
    if (easy)
    {
        int n_closed_doors, n_locked_chests;

        /* Count closed doors */
        n_closed_doors = count_feats(p, c, &grid, square_iscloseddoor, false);

        /* Count chests (locked) */
        n_locked_chests = count_chests(p, c, &grid, CHEST_OPENABLE);

        /* Use the last target found */
        if ((n_closed_doors + n_locked_chests) >= 1)
            dir = motion_dir(&p->grid, &grid);

        /* If there are chests to open, allow 5 as a direction */
        if (n_locked_chests) allow_5 = true;
    }

    /* Get a direction (or abort) */
    if (!VALID_DIR(dir)) return;

    // Golem. Move. Only. Straight. Movement. Denied.
    if (streq(p->race->name, "Golem") || streq(p->race->name, "Homunculus"))
        if (dir == 1 || dir == 3 || dir == 7 || dir == 9)
            return;

    /* Get location */
    next_grid(&grid, &p->grid, dir);

    /* Check for chest */
    obj = chest_check(p, c, &grid, CHEST_OPENABLE);

    /* Check for door */
    if (!obj && !do_cmd_open_test(p, c, &grid))
    {
        /* Cancel repeat */
        disturb(p, 1);
        return;
    }

    /* Take a turn */
    use_energy(p);

    /* Apply confusion */
    if (player_confuse_dir(p, &dir))
    {
        /* Get location */
        next_grid(&grid, &p->grid, dir);

        /* Check for chests */
        obj = chest_check(p, c, &grid, CHEST_OPENABLE);
    }

    /* Something in the way */
    if (is_blocker(p, c, &grid, allow_5))
        attack_blocker(p, c, &grid);

    /* Chest */
    else if (obj)
        more = do_cmd_open_chest(p, c, &grid, obj);

    /* Door */
    else
        more = do_cmd_open_aux(p, c, &grid);

    /* Cancel repeat unless we may continue */
    if (!more) disturb(p, 1);
}


/*
 * Determine if a given grid may be "closed"
 */
static bool do_cmd_close_test(struct player *p, struct chunk *c, struct loc *grid)
{
    /* Ghosts cannot close things */
    if ((p->ghost && !(p->dm_flags & DM_GHOST_HANDS)) || p->timed[TMD_WRAITHFORM])
    {
        /* Message */
        msg(p, "You cannot close things!");

        /* Nope */
        return false;
    }

    /* Check preventive inscription '^c' */
    if (check_prevent_inscription(p, INSCRIPTION_CLOSE))
    {
        msg(p, "The item's inscription prevents it.");
        return false;
    }

    /* Must have knowledge */
    if (!square_isknown(p, grid))
    {
        /* Message */
        msg(p, "You see nothing there.");

        /* Nope */
        return false;
    }

    /* Require open/broken door */
    if (!square_isopendoor(c, grid) && !square_isbrokendoor(c, grid))
    {
        /* Message */
        msg(p, "You see nothing there to close.");

        if (square_isopendoor_p(p, grid) || square_isbrokendoor_p(p, grid))
        {
            square_forget(p, grid);
            square_light_spot_aux(p, c, grid);
        }

        /* Nope */
        return false;
    }

    /* Handle polymorphed players */
    
    // can't close only if player is inside dungeon (to have access to houses)
    if (p->poly_race && !OPT(p, birth_fruit_bat) && p->wpos.depth)
    {
        bool can_open = rf_has(p->poly_race->flags, RF_OPEN_DOOR);
        bool can_bash = rf_has(p->poly_race->flags, RF_BASH_DOOR);

        if (!can_open && !can_bash)
        {
            /* Message */
            msg(p, "You cannot close things!");

            /* Nope */
            return false;
        }
    }

    /* Okay */
    return true;
}


/*
 * Perform the basic "close" command
 *
 * Assume there is no monster blocking the destination
 *
 * Returns true if repeated commands may continue
 */
static bool do_cmd_close_aux(struct player *p, struct chunk *c, struct loc *grid)
{
    bool more = false;
    int i;
    bool can_open = (OPT(p, birth_fruit_bat) || !p->poly_race ||
        rf_has(p->poly_race->flags, RF_OPEN_DOOR));
    bool can_bash = (p->poly_race && !OPT(p, birth_fruit_bat) &&
        rf_has(p->poly_race->flags, RF_BASH_DOOR));

    /* Verify legality */
    if (!do_cmd_close_test(p, c, grid)) return false;

    /* Broken door */
    if (square_isbrokendoor(c, grid))
        msg(p, "The door appears to be broken.");

    /* House door, close it */
    else if (square_home_isopendoor(c, grid))
    {
        /* Find this house */
        i = pick_house(&p->wpos, grid);

        /* Close the door */
        square_colorize_door(c, grid, house_get(i)->color);

        /* Update the visuals */
        square_memorize(p, c, grid);
        square_light_spot_aux(p, c, grid);
        update_visuals(&p->wpos);

        /* Sound */
        sound(p, MSG_SHUTDOOR);
    }

    else if (can_bash && !can_open)
        msg(p, "You cannot close things!");

    /* Close the door */
    else
    {
        /* Close the door */
        square_close_door(c, grid);

        /* Update the visuals */
        square_memorize(p, c, grid);
        square_light_spot_aux(p, c, grid);
        update_visuals(&p->wpos);

        /* Sound */
        sound(p, MSG_SHUTDOOR);
    }

    /* Result */
    return (more);
}


/*
 * Close an open door.
 */
void do_cmd_close(struct player *p, int dir, bool easy)
{
    bool more = false;
    struct chunk *c = chunk_get(&p->wpos);
    struct loc grid;

    /* Easy Close */
    if (easy)
    {
        /* Count open doors */
        if (count_feats(p, c, &grid, square_isopendoor, false) >= 1)
            dir = motion_dir(&p->grid, &grid);
    }

    /* Get a direction (or abort) */
    if (!VALID_DIR(dir)) return;

    // Golem. Move. Only. Straight. Movement. Denied.
    if (streq(p->race->name, "Golem") || streq(p->race->name, "Homunculus"))
        if (dir == 1 || dir == 3 || dir == 7 || dir == 9)
            return;

    /* Get location */
    next_grid(&grid, &p->grid, dir);

    /* Verify legality */
    if (!do_cmd_close_test(p, c, &grid))
    {
        /* Cancel repeat */
        disturb(p, 1);
        return;
    }

    /* Take a turn */
    use_energy(p);

    /* Apply confusion */
    if (player_confuse_dir(p, &dir))
    {
        /* Get location */
        next_grid(&grid, &p->grid, dir);
    }

    /* Something in the way */
    if (square(c, &grid)->mon != 0)
        attack_blocker(p, c, &grid);

    /* Door - close it */
    else
        more = do_cmd_close_aux(p, c, &grid);

    /* Cancel repeat unless we may continue */
    if (!more) disturb(p, 1);
}


/*
 * Determine if a given grid may be "tunneled"
 */
static bool do_cmd_tunnel_test(struct player *p, struct chunk *c, struct loc *grid)
{
    /* Ghosts cannot tunnel */
    // wraithform players too (it includes not only mages, but also eg Shapechangers)
    if ((p->ghost && !(p->dm_flags & DM_GHOST_HANDS)) || p->timed[TMD_WRAITHFORM])
    {
        msg(p, "You cannot tunnel.");
        return false;
    }

    /* Check preventive inscription '^T' */
    if (check_prevent_inscription(p, INSCRIPTION_TUNNEL))
    {
        msg(p, "The item's inscription prevents it.");
        return false;
    }

    /* Must have knowledge */
    if (!square_isknown(p, grid))
    {
        msg(p, "You see nothing there.");
        return false;
    }

    /* Must be a wall/door/etc */
    // 1) DM need possibility to 'dig' doors to remove wiped houses
    // 2) allow dig fountains
    if (!square_seemsdiggable(c, grid) && (!(p->dm_flags & DM_HOUSE_CONTROL)) &&
        !square_isfountain(c, grid))
    {
        msg(p, "You see nothing there to tunnel.");
        if (square_seemsdiggable_p(p, grid))
        {
            square_forget(p, grid);
            square_light_spot_aux(p, c, grid);
        }
        return false;
    }

    /* No tunneling on special levels and towns */
    if (special_level(&p->wpos)) /// REMOVED in_town(&p->wpos)
    {
        msg(p, "Nothing happens.");
        return false;
    }

    /* Okay */
    return true;
}


/*
 * Tunnel through wall. Assumes valid location.
 *
 * Note that it is impossible to "extend" rooms past their
 * outer walls (which are actually part of the room).
 *
 * Attempting to do so will produce floor grids which are not part
 * of the room, and whose "illumination" status do not change with
 * the rest of the room.
 */
static bool twall(struct player *p, struct chunk *c, struct loc *grid)
{
    /* Paranoia -- require a wall or door or some such */
    if (!square_seemsdiggable(c, grid)) return false;

    /* Remove the feature */
    square_tunnel_wall(c, grid);

    /* Update the visuals */
    update_visuals(&p->wpos);

    /* Fully update the flow */
    fully_update_flow(&p->wpos);

    /* Result */
    return true;
}


/* Forward declaration */
static bool create_house_door(struct player *p, struct chunk *c, struct loc *grid);


/*
 * Perform the basic "tunnel" command
 *
 * Assumes that no monster is blocking the destination.
 * Uses "twall" (above) to do all "terrain feature changing".
 * Returns true if repeated commands may continue.
 */
static bool do_cmd_tunnel_aux(struct player *p, struct chunk *c, struct loc *grid)
{
    bool more = false;
    int digging_chances[DIGGING_MAX], chance = 0, dig_idx;
    bool okay = false;
    bool gold, rubble, tree, web, door, ice, sand;
    int weapon_slot = slot_by_name(p, "weapon");
    struct object *current_weapon = slot_object(p, weapon_slot);
    struct object *current_tool = equipped_item_by_slot_name(p, "tool");
    const char *with_clause = ((current_weapon == NULL)? "with your hands": "with your weapon");

    if ((current_tool != NULL) && tval_is_digger(current_tool)) with_clause = "with your digger";

    gold = square_hasgoldvein(c, grid);
    rubble = square_isrubble(c, grid);
    tree = square_istree(c, grid);
    web = square_iswebbed(c, grid);
    door = square_isdoor(c, grid);
    ice = square_is_ice(c, grid);
    sand = square_is_sand(c, grid);

    /* Verify legality */
    if (!do_cmd_tunnel_test(p, c, grid)) return false;
    
    if (in_town(&p->wpos) && p->timed[TMD_FOOD] < 2000)
    {
        msg(p, "You are too tired and hungry for mining. Eat some food.");
        return false;        
    }
        

    /* Mountain in town */
    if (square_ismountain(c, grid) && in_town(&p->wpos) && p->lev > 7)
    {

        // digging in town makes you more hungry (first was '50', too ezpz.. making more harsh)
        player_dec_timed(p, TMD_FOOD, 200, false);
            if (one_in_(2))
            {
                struct object *dig_stone;
                /* Make house stone */
                dig_stone = make_object(p, c, 1, false, false, false, NULL, TV_STONE);
                if (dig_stone)
                {
                    sound(p, MSG_DIG);
                    set_origin(dig_stone, ORIGIN_FLOOR, p->wpos.depth, NULL);
                    /* Drop house stone */
                    drop_near(p, c, &dig_stone, 0, &p->grid, true, DROP_FADE, false);
                }
            }
        return false;
    }

    calc_digging_chances(p, &p->state, digging_chances);

    /* Do we succeed? */
    dig_idx = square_digging(c, grid);
    if (dig_idx > 0 && dig_idx <= DIGGING_MAX) chance = digging_chances[dig_idx - 1];
    okay = CHANCE(chance, 1600);

    /* Hack -- house walls */
    if (square_is_new_permhouse(c, grid))
    {
        /* Either the player has lost his mind or he is trying to create a door! */
        create_house_door(p, c, grid);
    }

    /* Hack -- DM can remove wiped unowned custom houses */
    // ...to prevent case when admin can accidently destroy NPC stores' doors
    // (I did it several times already) - destroy only whe drink ?Heroism
    if ((p->dm_flags & DM_HOUSE_CONTROL) && (find_house(p, grid, 0) == -1) &&
       ((square_is_new_permhouse(c, grid)) || (square_home_iscloseddoor(c, grid)) ||
         square_issafefloor(c, grid)) && p->timed[TMD_HERO])
    {
        /* Either the player has lost his mind or he is trying to create a door! */
        square_burn_grass(c, grid);
    }

    /* Permanent */
    else if (square_isperm(c, grid))
    {
        /* Hack -- logs */
        if (!feat_is_wall(square(c, grid)->feat))
            msg(p, "You cannot tunnel through that.");
        else
            msg(p, "This seems to be permanent rock.");

        if (!square_isperm_p(p, grid))
        {
            square_memorize(p, c, grid);
            square_light_spot_aux(p, c, grid);
        }
    }
    
    /* No diggins walls in towns */
    else if (in_town(&p->wpos) && ((feat_is_wall(square(c, grid)->feat)) ||
            (square_isdoor(c, grid))))
        msg(p, "It's pointless to dig that there.");

    /* Mountain */
    else if (square_ismountain(c, grid))
        msg(p, "Digging a tunnel into that mountain would take forever!");

    /* Hack -- pit walls (to fool the player) */
    else if (square_ispermfake(c, grid))
    {
        msg(p, "You tunnel into the %s %s.", square_apparent_name(p, c, grid), with_clause);
        more = true;
    }

////////// DIG DRY FOUNTAIN:
    else if (okay && square_isdryfountain(c, grid))
    {
        /* Message */
        msg(p, "You have split the dry fountain.");
        
        if (one_in_(4))
        {
            msg(p, "Water gushes forth from the hole in fountain!");
            square_set_feat(c, grid, FEAT_WATER);
        }
        else if (one_in_(7))
        {   /* pit trap */
            square_tunnel_wall(c, grid);
            msg(p, "Suddenly you hear that some floor under ruined fountain fall!");
            place_trap(c, grid, 5, c->wpos.depth);
        }
        else if (one_in_(7) && c->wpos.depth > 1) // no trap pit on 1st lvl
        {   /* trap door */
            square_tunnel_wall(c, grid);
            msg(p, "Suddenly you hear that some floor under ruined fountain fall!");
            place_trap(c, grid, 4, c->wpos.depth);
        }
        else
            /* Remove the feature */
            square_tunnel_wall(c, grid);

        /* Place an object */
        if (magik(10))
        {
            struct object *dig_dry;
            /* Create a simple object */
            dig_dry = make_object(p, c, object_level(&p->wpos), false, false, false, NULL, 0);

                if (dig_dry) // to prevent obj null in case of `water` tile which is not `floor`
                {
                    set_origin(dig_dry, ORIGIN_FLOOR, p->wpos.depth, NULL);
                    /* Drop house stone */
                    drop_near(p, c, &dig_dry, 0, &p->grid, true, DROP_FADE, false);
                }
        }        
        /* Summon creature */
        else if (one_in_(4))
        {
            static const struct summon_chance_t
            {
                const char *race;
                uint8_t minlev;
                uint8_t chance;
            } summon_chance[] =
            {
                {"creeping copper coins", 0, 100},
                {"clay golem", 7, 90},
                {"stone golem", 15, 80},
                {"livingstone", 17, 70},
                {"lesser wall monster", 23, 60},
                {"gargoyle", 28, 50},
                {"silent watcher", 30, 40},
                {"malicious leprechaun", 33, 30}
            };
            int i;

            msg(p, "Something crawls out of the fountain rubbles!");
            do {i = randint0(N_ELEMENTS(summon_chance));}
            while ((p->wpos.depth < summon_chance[i].minlev) || !magik(summon_chance[i].chance));
            summon_specific_race(p, c, &p->grid, get_race(summon_chance[i].race), 1);
        }        

        /* Sound */
        sound(p, MSG_DIG);

        /* Update the visuals */
        update_visuals(&p->wpos);

        /* Fully update the flow */
        fully_update_flow(&p->wpos);
    }

////////// DIG FOUNTAIN WITH WATER:
    else if (okay && square_isfountain(c, grid))
    {
        /* Message */
        msg(p, "Oops. You have split the fountain.");
        
        /* Fall in */
        if (one_in_(15))
        {
            msg(p, "You slip and fall in the water.");
            if (!player_passwall(p) && !can_swim(p))
                take_hit(p, damroll(4, 5), "drowning", "slipped and fell in a water");
            return false;
        }        
        else if (one_in_(2))
        {
            msg(p, "Water gushes forth from the overflowing ruined fountain!");
            square_set_feat(c, grid, FEAT_WATER);
            // add PROJ SHALLOW WATER fire_ball(context->origin->player, PROJ_SH_WATER, 0, 1, 2, false, false);
        }            
        else
        {
            /* Remove the feature */
            square_tunnel_wall(c, grid);        
            msg(p, "Water suddenly disappears. You hear bubbling sound.");    
        }

        /* Summon water creature */
        if (one_in_(2))
        {
            static const struct summon_chance_t
            {
                const char *race;
                uint8_t minlev;
                uint8_t chance;
            } summon_chance[] =
            {
                {"giant green frog", 0, 100},
                {"giant red frog", 3, 90},
                {"water spirit", 8, 80},
                {"water vortex", 11, 70},
                {"water elemental", 16, 60},
                {"water hound", 21, 50},
                {"seahorse", 30, 40},                
                {"Djinn", 45, 30}
            };
            int i;

            msg(p, "Caramba! Something pops out of the fountain depths!");
            do {i = randint0(N_ELEMENTS(summon_chance));}
            while ((p->wpos.depth < summon_chance[i].minlev) || !magik(summon_chance[i].chance));
            summon_specific_race(p, c, &p->grid, get_race(summon_chance[i].race), 1);
        }
        
        /* Place an object */
        else if (magik(10))
        {
            struct object *dig_fountain;
            /* Create a simple object */
            dig_fountain = make_object(p, c, object_level(&p->wpos), false, false, false, NULL, 0);

                if (dig_fountain) // to prevent obj null in case of `water` tile which is not `floor`
                {
                    set_origin(dig_fountain, ORIGIN_FLOOR, p->wpos.depth, NULL);

                    /* Drop treasure */
                    drop_near(p, c, &dig_fountain, 0, &p->grid, true, DROP_FADE, false);
                }
        }        

        sound(p, MSG_DIG_TREASURE);

        /* Update the visuals */
        update_visuals(&p->wpos);

        /* Fully update the flow */
        fully_update_flow(&p->wpos);
    }

////////// REGULAR DIGGING:
    /* Success */
    else if (okay && twall(p, c, grid))
    {
        // hack doors
        if (door)
        {
            sound(p, MSG_DOOR_BROKEN);
            msg(p, "Wham! You bashed through the door.");
            square_set_feat(c, grid, FEAT_BROKEN);
        }
        /* Mow down the vegetation */
        else if (tree) 
        { 
            sound(p, MSG_CHOP_TREE_FALL); 
            msg(p, "You hack your way through the vegetation %s.", with_clause); 
         
            // CLASS lumber item generation (Rare Herb, Sprig of Athelas etc)
            if (p->wpos.depth) 
            { 
                struct object *dig_reagent = NULL;
                
                if (streq(p->clazz->name, "Alchemist") && one_in_(5))
                {
                    dig_reagent = object_new();
                    object_prep(p, c, dig_reagent, lookup_kind_by_name(TV_REAGENT, "Rare Herb"), 0, MINIMISE);
                }
                else if (streq(p->race->name, "Dunadan") && magik(1)) // 1%
                {
                    dig_reagent = object_new();
                    object_prep(p, c, dig_reagent, lookup_kind_by_name(TV_FOOD, "Sprig of Athelas"), 0, MINIMISE);
                }

                // player found reagent after chopping...
                if (dig_reagent)
                {
                    /* Pack is too full or heavy */
                    if (!inven_carry_okay(p, dig_reagent))
                    {
                        object_delete(&dig_reagent);
                        msg(p, "Your backpack is too full to add something there!");
                        return false;
                    }
                    
                    if (!weight_okay(p, dig_reagent))
                    {
                        object_delete(&dig_reagent);
                        msg(p, "Your backpack is too heavy to add something there!");
                        return false;
                    }
                    
                    set_origin(dig_reagent, ORIGIN_ACQUIRE, p->wpos.depth, NULL);
                    dig_reagent->soulbound = true;
                    inven_carry(p, dig_reagent, true, true);
                    handle_stuff(p);
                }
            }
        }

        /* Clear some web */
        else if (web)
        {
            sound(p, MSG_DIG);
            msg(p, "You hack your way through the web %s.", with_clause);
        }

        /* Remove the rubble */
        else if (rubble)
        {
            msg(p, "You have removed the rubble %s.", with_clause);

            /* Place an object */
            // not on the surface! (ironman town cheeze.. a lot of rubble there)
            if (p->wpos.depth && !dynamic_town(&p->wpos) &&
                magik(10) && !square_ishardrubble(c, grid))
            {
                struct object *new_obj;

                /* Create a simple object */
                place_object(p, c, grid, object_level(&p->wpos), false, false, ORIGIN_RUBBLE, 0);

                /* Observe the new object */
                new_obj = square_object(c, grid);
                if (new_obj && !ignore_item_ok(p, new_obj) && square_isseen(p, grid))
                {
                    sound(p, MSG_DIG_TREASURE);
                    msg(p, "You have found something!");
                }
            }
        }

        /* Found treasure */
        else if (gold)
        {
            // make Crafting Material
            if (streq(p->clazz->name, "Crafter") && one_in_(2) && p->wpos.depth)
            {
                struct object *dig_reagent;

                dig_reagent = object_new();

                object_prep(p, c, dig_reagent, lookup_kind_by_name(TV_REAGENT, "Crafting Material"), 0, MINIMISE);

                /* Pack is too full */
                if (!inven_carry_okay(p, dig_reagent))
                {
                    object_delete(&dig_reagent);
                    msg(p, "Your backpack is too full to add something there!");
                    return false;
                }

                /* Pack is too heavy */
                if (!weight_okay(p, dig_reagent))
                {
                    object_delete(&dig_reagent);
                    msg(p, "Your backpack is too heavy to add something there!");
                    return false;
                }
                set_origin(dig_reagent, ORIGIN_ACQUIRE, p->wpos.depth, NULL);
                dig_reagent->soulbound = true;

                /* Give it to the player */
                inven_carry(p, dig_reagent, true, true);

                /* Handle stuff */
                handle_stuff(p);
            }
            // regular case when digging treasure
            else
            {
                /* Place some gold */
                place_gold(p, c, grid, object_level(&p->wpos), ORIGIN_FLOOR);
                sound(p, MSG_DIG_TREASURE);
                msg(p, "You have found something digging %s!", with_clause);
            }
        }

        /* No cobbles and stones to find in town */
        else if (in_town(&p->wpos))
        {
            sound(p, MSG_DIG);
            msg(p, "You have finished the tunnel.");
        }

        // make Rare Mineral
        else if (streq(p->clazz->name, "Alchemist") && one_in_(5) && p->wpos.depth)
        {
            struct object *dig_reagent;

            dig_reagent = object_new();

            object_prep(p, c, dig_reagent, lookup_kind_by_name(TV_REAGENT, "Rare Mineral"), 0, MINIMISE);

            /* Pack is too full */
            if (!inven_carry_okay(p, dig_reagent))
            {
                object_delete(&dig_reagent);
                msg(p, "Your backpack is too full to add something there!");
                return false;
            }

            /* Pack is too heavy */
            if (!weight_okay(p, dig_reagent))
            {
                object_delete(&dig_reagent);
                msg(p, "Your backpack is too heavy to add something there!");
                return false;
            }
            set_origin(dig_reagent, ORIGIN_ACQUIRE, p->wpos.depth, NULL);
            dig_reagent->soulbound = true;

            /* Give it to the player */
            inven_carry(p, dig_reagent, true, true);

            /* Handle stuff */
            handle_stuff(p);
        }

        /* Dig cobbles (simple non-magical rocks) */
        else if (one_in_(2) && !ice && !sand)
        {
            struct object *dig_cobble;
            /* Make cobble */
            dig_cobble = make_object(p, c, object_level(&p->wpos), false, false, false, NULL, TV_COBBLE);

            if (dig_cobble) // check in case of NULL
            {
                set_origin(dig_cobble, ORIGIN_FLOOR, p->wpos.depth, NULL);
                sound(p, MSG_DIG);

                /* Drop the cobble (place_object won't work as it generates
                object at wall position which is no drop) */
                drop_near(p, c, &dig_cobble, 0, &p->grid, true, DROP_FADE, true);
            }
        }

        /* Dig house Foundation Stone */
        else if (one_in_(2) && p->lev > 7 && !ice && !sand)
        {
            struct object *dig_stone;
            /* Make house stone */
            dig_stone = make_object(p, c, object_level(&p->wpos), false, false, false, NULL, TV_STONE);

            if (dig_stone)
            {
                set_origin(dig_stone, ORIGIN_FLOOR, p->wpos.depth, NULL);
                sound(p, MSG_DIG);
                /* Drop house stone */
                drop_near(p, c, &dig_stone, 0, &p->grid, true, DROP_FADE, false);
            }
        }

        /* You may dig out monster... */

        // sand monsters
        else if (one_in_(20) && square_is_sand(c, grid))
        {
            static const struct summon_chance_t
            {
                const char *race;
                uint8_t minlev;
                uint8_t chance;
            } summon_chance[] =
            {
                {"soldier ant", 0, 100},
                {"viper", 3, 50},
                {"giant salamander", 8, 50},
                {"moaning spirit", 12, 50},
                {"earth spirit", 16, 50},
                {"earth hound", 19, 50},
                {"sandworm", 20, 90},
                {"adult sandworm", 25, 60},
                {"earth elemental", 34, 40},
                {"sand giant", 40, 30},
                {"great earth elemental", 60, 5}
            };
            int i;

            msg(p, "Caramba! Something appears after you digged the sand!");
            do {i = randint0(N_ELEMENTS(summon_chance));}
            while ((p->wpos.depth < summon_chance[i].minlev) || !magik(summon_chance[i].chance));
            summon_specific_race(p, c, &p->grid, get_race(summon_chance[i].race), 1);
        }

        // ice monsters
        else if (one_in_(20) && square_is_ice(c, grid))
        {
            static const struct summon_chance_t
            {
                const char *race;
                uint8_t minlev;
                uint8_t chance;
            } summon_chance[] =
            {
                {"grey mold", 0, 100},
                {"green glutton ghost", 4, 5},
                {"creeping copper coins", 5, 10},
                {"undead mass", 10, 80},
                {"moaning spirit", 12, 70},
                {"freezing sphere", 17, 60},
                {"ancient obelisk", 20, 5},
                {"cold vortex", 21, 50},
                {"ice skeleton", 23, 50},
                {"statue of ancient god", 25, 5},
                {"ice troll", 28, 10},
                {"frost giant", 29, 10},
                {"mithril golem", 30, 5},
                {"ice elemental", 37, 50},
                {"death knight", 38, 10},
                {"great ice wyrm", 60, 1}
            };
            int i;

            msg(p, "Caramba! Something appears after you digged ice!");
            do {i = randint0(N_ELEMENTS(summon_chance));}
            while ((p->wpos.depth < summon_chance[i].minlev) || !magik(summon_chance[i].chance));
            summon_specific_race(p, c, &p->grid, get_race(summon_chance[i].race), 1);
        }

        // other monsters in walls/rocks
        else if (one_in_(20) && !square_is_ice(c, grid) && !square_is_sand(c, grid))
        {
            static const struct summon_chance_t
            {
                const char *race;
                uint8_t minlev;
                uint8_t chance;
            } summon_chance[] =
            {
                {"soldier ant", 0, 100},
                {"Weaver spider", 1, 50},
                {"giant cockroach", 2, 50},
                {"viper", 3, 50},
                {"cave lizard", 4, 50},
                {"brown mold", 6, 50},
                {"crypt creep", 7, 20},
                {"giant salamander", 8, 20},
                {"giant red ant", 9, 50},
                {"hairy mold", 10, 40},
                {"moaning spirit", 12, 30},
                {"earth spirit", 16, 60},
                {"earth hound", 20, 30},
                {"pukelman", 25, 10},
                {"rusty golem", 27, 10},
                {"earth elemental", 33, 60},
                {"xaren", 40, 40},
                {"Boggart", 43, 10},
                {"prizrak", 44, 5},
                {"shadow", 50, 10},
                {"great earth elemental", 60, 5}
            };
            int i;

            msg(p, "Caramba! Something appears after you digged the wall!");
            do {i = randint0(N_ELEMENTS(summon_chance));}
            while ((p->wpos.depth < summon_chance[i].minlev) || !magik(summon_chance[i].chance));
            summon_specific_race(p, c, &p->grid, get_race(summon_chance[i].race), 1);
        }


        /* Found nothing */
        else
            msg(p, "You have finished the tunnel %s.", with_clause);

        // golem restores LOW satiation by digging (especially Zeitnot)
        if (streq(p->race->name, "Golem") && !in_town(&p->wpos)) {
            if (OPT(p, birth_zeitnot) ||
               (OPT(p, birth_no_recall) && OPT(p, birth_force_descend))) {
                if (p->timed[TMD_FOOD] < 1000) // at 10%
                    player_inc_timed(p, TMD_FOOD, 100, false, false);
            } else if (p->timed[TMD_FOOD] < 500) { // at 5%
                player_inc_timed(p, TMD_FOOD, 30, false, false);
            } else { // in other cases restores 1 (makes success digging turn free)
                player_inc_timed(p, TMD_FOOD, 2, false, false);
            }
        }

        /* On the surface, new terrain may be exposed to the sun. */
        if (c->wpos.depth == 0) expose_to_sun(c, grid, is_daytime());

        /* Update the visuals. */
        square_memorize(p, c, grid);
        square_light_spot_aux(p, c, grid);
        update_visuals(&p->wpos);
    }

    /* Failure, continue digging */
    else if (chance > 0)
    {

    // Tunnel sound frequency: taking last number from turns counter and
    // checking is it equal to zero (odd/even number method).
    // For slow chars sounds should be more often.

    int turn_last_digit = p->active_turn.turn % 10;
    int sound_freq = 2 + ((p->state.speed - 110) / 10);
    if (sound_freq < 1) // if speed is very low we will have 0
        sound_freq = 2;

        if (tree || web)
        {
            if (turn_last_digit % sound_freq == 0)
                sound(p, MSG_CHOP_TREE);
            msg(p, "You attempt to clear a path %s.", with_clause);
        }
        else if (rubble)
        {
            if (turn_last_digit % sound_freq == 0)
                sound(p, MSG_TUNNEL_WALL);
            msg(p, "You dig in the rubble %s.", with_clause);
        }
        else
        {
            if (turn_last_digit % sound_freq == 0)
                sound(p, MSG_TUNNEL_WALL);
            msg(p, "You tunnel into the %s %s.", square_apparent_name(p, c, grid), with_clause);
        }

         more = true;
    }

    /* Don't automatically repeat if there's no hope. */
    else
    {
        if (tree || web)
            msg(p, "You fail to clear a path %s.", with_clause);
        else if (rubble)
            msg(p, "You dig in the rubble %s with little effect.", with_clause);
        else
        {
            msg(p, "You chip away futilely %s at the %s.", with_clause,
                square_apparent_name(p, c, grid));
        }
    }

    /* Result */
    return (more);
}


/*
 * Tunnel through "walls" (including rubble and doors, secret or otherwise)
 *
 * Digging is very difficult without a "digger" weapon, but can be
 * accomplished by strong players using heavy weapons.
 */
bool do_cmd_tunnel(struct player *p)
{
    int dir = (int)p->digging_dir;
    bool more = false;
    struct chunk *c = chunk_get(&p->wpos);
    struct loc grid;

    /* Cancel repeat */
    if (!p->digging_request) return true;

    /* Check energy */
    if (!has_energy(p, true)) return false;

    /* Get a direction (or abort) */
    if (!dir || !VALID_DIR(dir))
    {
        /* Cancel repeat */
        disturb(p, 1);
        return true;
    }

    // Golem. Move. Only. Straight. Movement. Denied.
    if (streq(p->race->name, "Golem") || streq(p->race->name, "Homunculus"))
        if (dir == 1 || dir == 3 || dir == 7 || dir == 9)
            return false;

    /* Get location */
    next_grid(&grid, &p->grid, dir);

    /* Oops */
    if (!do_cmd_tunnel_test(p, c, &grid))
    {
        /* Cancel repeat */
        disturb(p, 1);
        return true;
    }

    /* Take a turn */
    use_energy(p);


    /* Apply confusion */
    if (player_confuse_dir(p, &dir))
    {
        /* Get location */
        next_grid(&grid, &p->grid, dir);
    }

    /* Attack anything we run into */
    if (square(c, &grid)->mon != 0)
        attack_blocker(p, c, &grid);

    /* Tunnel through walls */
    else
        more = do_cmd_tunnel_aux(p, c, &grid);

    /* Cancel repetition unless we can continue */
    if (!more) disturb(p, 1);

    /* Repeat */
    if (p->digging_request > 0) p->digging_request--;
    if (p->digging_request > 0) cmd_tunnel(p);
    return true;
}


/*
 * Determine if a given grid may be "disarmed"
 */
static bool do_cmd_disarm_test(struct player *p, struct chunk *c, struct loc *grid)
{
    /* Ghosts cannot disarm */
    if ((p->ghost && !(p->dm_flags & DM_GHOST_HANDS)) || p->timed[TMD_WRAITHFORM])
    {
        msg(p, "You cannot disarm things!");
        return false;
    }

    /* Check preventive inscription '^D' */
    if (check_prevent_inscription(p, INSCRIPTION_DISARM))
    {
        msg(p, "The item's inscription prevents it.");
        return false;
    }

    /* Must have knowledge */
    if (!square_isknown(p, grid))
    {
        msg(p, "You see nothing there.");
        return false;
    }

    /* Look for a closed, unlocked door to lock */
    if (square_basic_iscloseddoor(c, grid) && !square_islockeddoor(c, grid))
        return true;

    /* Look for a trap */
    if (!square_isdisarmabletrap(c, grid))
    {
        msg(p, "You see nothing there to disarm.");
        return false;
    }

    /* Okay */
    return true;
}


/*
 * Perform the command "lock door"
 *
 * Assume there is no monster blocking the destination
 *
 * Returns true if repeated commands may continue
 */
static bool do_cmd_lock_door(struct player *p, struct chunk *c, struct loc *grid)
{
    int i, j, power;
    bool more = false;

    /* Verify legality */
    if (!do_cmd_disarm_test(p, c, grid)) return false;

    /* Get the "disarm" factor */
    i = p->state.skills[SKILL_DISARM_PHYS];

    /* Calculate lock "power" */
    power = m_bonus(7, p->wpos.depth);

    /* Extract the percentage success */
    j = calc_skill(p, i, power, no_light(p));

    /* Success */
    if (magik(j))
    {
        msg(p, "You lock the door.");
        square_set_door_lock(c, grid, power);
    }

    /* Failure -- keep trying */
    else if (magik(j))
    {
        msg(p, "You failed to lock the door.");

        /* We may keep trying */
        more = true;
    }

    /* Failure */
    else
        msg(p, "You failed to lock the door.");

    /* Result */
    return more;
}


/*
 * Perform the basic "disarm" command
 *
 * Assume there is no monster blocking the destination
 *
 * Returns true if repeated commands may continue
 */
static bool do_cmd_disarm_aux(struct player *p, struct chunk *c, struct loc *grid, int dir)
{
    int skill, power, chance;
    struct trap *trap = square(c, grid)->trap;
    bool more = false;

    /* Verify legality */
    if (!do_cmd_disarm_test(p, c, grid)) return false;

    /* Choose first player trap */
    while (trap)
    {
        if (trf_has(trap->flags, TRF_TRAP)) break;
        trap = trap->next;
    }
    if (!trap) return false;

    /* Get the base disarming skill */
    if (trf_has(trap->flags, TRF_MAGICAL))
        skill = p->state.skills[SKILL_DISARM_MAGIC];
    else
        skill = p->state.skills[SKILL_DISARM_PHYS];

    /* Extract trap power */
    power = MAX(c->wpos.depth, 0) / 5;

    /* Extract the percentage success */
    chance = calc_skill(p, skill, power, no_light(p));

    /* Two chances - one to disarm, one not to set the trap off */
    if (magik(chance))
    {
        msgt(p, MSG_DISARM, "You have disarmed the %s.", trap->kind->name);
        player_exp_gain(p, 1 + power);

        /* Trap is gone */
        if (!square_remove_trap(c, grid, trap, true))
        {
            my_assert(0);
        }
    }
    else if (magik(chance))
    {
        msg(p, "You failed to disarm the %s.", trap->kind->name);

        /* Player can try again */
        more = true;
    }
    else
    {
        msg(p, "You set off the %s!", trap->kind->name);
        move_player(p, c, dir, false, false, true, -1, true);
    }

    /* Result */
    return (more);
}


/*
 * Disarms a trap, or chest
 *
 * Traps must be visible, chests must be known trapped
 */
void do_cmd_disarm(struct player *p, int dir, bool easy)
{
    struct object *obj;
    bool more = false, allow_5 = false;
    struct chunk *c = chunk_get(&p->wpos);
    struct loc grid;

    /* Easy Disarm */
    if (easy)
    {
        int n_traps, n_chests, n_unldoor;

        /* Count visible traps */
        n_traps = count_feats(p, c, &grid, square_isdisarmabletrap, false);

        /* Count chests (trapped) */
        n_chests = count_chests(p, c, &grid, CHEST_TRAPPED);

        /* Count closed doors (for door locking) */
        n_unldoor = count_feats(p, c, &grid, square_isunlockeddoor, false);

        /* Use the last target found */
        if ((n_traps + n_chests + n_unldoor) >= 1)
            dir = motion_dir(&p->grid, &grid);

        /* If there are trap or chests to open, allow 5 as a direction */
        if (n_traps || n_chests) allow_5 = true;
    }

    /* Get a direction (or abort) */
    if (!VALID_DIR(dir)) return;

    // Golem. Move. Only. Straight. Movement. Denied.
    if (streq(p->race->name, "Golem") || streq(p->race->name, "Homunculus"))
        if (dir == 1 || dir == 3 || dir == 7 || dir == 9)
            return;

    /* Get location */
    next_grid(&grid, &p->grid, dir);

    /* Check for chests */
    obj = chest_check(p, c, &grid, CHEST_TRAPPED);

    /* Verify legality */
    if (!obj && !do_cmd_disarm_test(p, c, &grid))
    {
        /* Cancel repeat */
        disturb(p, 1);
        return;
    }

    /* Take a turn */
    use_energy(p);

    /* Apply confusion */
    if (player_confuse_dir(p, &dir))
    {
        /* Get location */
        next_grid(&grid, &p->grid, dir);

        /* Check for chests */
        obj = chest_check(p, c, &grid, CHEST_TRAPPED);
    }

    /* Something in the way */
    if (is_blocker(p, c, &grid, allow_5))
        attack_blocker(p, c, &grid);

    /* Chest */
    else if (obj)
        more = do_cmd_disarm_chest(p, c, &grid, obj);

    /* Door to lock */
    else if (square_basic_iscloseddoor(c, &grid))
        more = do_cmd_lock_door(p, c, &grid);

    /* Disarm trap */
    else
        more = do_cmd_disarm_aux(p, c, &grid, dir);

    /* Cancel repeat unless told not to */
    if (!more) disturb(p, 1);
}     


/*
 * Manipulate an adjacent grid in some way
 *
 * Attack monsters, tunnel through walls, disarm traps, open doors.
 *
 * This command must always take energy, to prevent free detection
 * of invisible monsters.
 * REVISED FOR MAngband-specific reasons: we don't care if someone
 * detects a monster by tunneling into it, and treat "tunnel air" as an
 * error, which DOES NOT spend player's energy.
 *
 * The "semantics" of this command must be chosen before the player
 * is confused, and it must be verified against the new grid.
 */
void do_cmd_alter(struct player *p, int dir)
{
    struct loc grid;
    bool more = false;
    struct object *o_chest_closed;
    struct object *o_chest_trapped;
    bool spend = true;
    struct chunk *c = chunk_get(&p->wpos);

    /* Get a direction (or abort) */
    if (!dir || !VALID_DIR(dir)) return;

    // Golem. Move. Only. Straight. Movement. Denied.
    if (streq(p->race->name, "Golem") || streq(p->race->name, "Homunculus"))
        if (dir == 1 || dir == 3 || dir == 7 || dir == 9)
            return;

    /* Check preventive inscription '^+' */
    if (check_prevent_inscription(p, INSCRIPTION_ALTER))
    {
        msg(p, "The item's inscription prevents it.");
        return;
    }

    /* Get location */
    next_grid(&grid, &p->grid, dir);

    /* Apply confusion */
    if (player_confuse_dir(p, &dir))
    {
        /* Get location */
        next_grid(&grid, &p->grid, dir);
    }

    /* Check for closed chest */
    o_chest_closed = chest_check(p, c, &grid, CHEST_OPENABLE);

    /* Check for trapped chest */
    o_chest_trapped = chest_check(p, c, &grid, CHEST_TRAPPED);

    /* Something in the way */
    if (square(c, &grid)->mon != 0)
    {
        /* Attack */
        attack_blocker(p, c, &grid);
    }

    /* MAngband-specific: open house walls (House Creation) */
    else if (square_is_new_permhouse(c, &grid))
        more = do_cmd_tunnel_aux(p, c, &grid);

    /* Tunnel through walls, trees and rubble (allow pit walls to fool the player) */
    else if (square_isdiggable(c, &grid))
        more = do_cmd_tunnel_aux(p, c, &grid);

    /* Open closed doors */
    else if (square_iscloseddoor(c, &grid))
        more = do_cmd_open_aux(p, c, &grid);

    /* Disarm traps */
    else if (square_isdisarmabletrap(c, &grid))
        more = do_cmd_disarm_aux(p, c, &grid, dir);

    /* Trapped chest */
    else if (o_chest_trapped)
        more = do_cmd_disarm_chest(p, c, &grid, o_chest_trapped);

    /* Open chest */
    else if (o_chest_closed)
        more = do_cmd_open_chest(p, c, &grid, o_chest_closed);

    /* Close door */
    else if (square_isopendoor(c, &grid))
        more = do_cmd_close_aux(p, c, &grid);

    /* Make farm --- comment out in T for now as we don't have wilderness 
    // and Villager class can steal from fields
    else if (in_wild(&p->wpos) && !special_level(&p->wpos) && !town_area(&p->wpos) &&
        square_isgrass(c, &grid) && !square_object(c, &grid))
    {
        square_set_feat(c, &grid, FEAT_CROP);
    }
    */

    /* Oops */
    else
    {
        msg(p, "You spin around.");

        /* Do not spend energy. */
        spend = false;
    }

    /* Take a turn */
    if (spend) use_energy(p);

    /* Cancel repetition unless we can continue */
    if (!more) disturb(p, 1);
}


static const char *comment_zeitnot[] =
{
    "You don't feel like going to pick flowers right now.",
    "Where do you think you are going?",
    "Morgoth the potato farmer? - get real!",
    "Morgoth awaits you in the depths not in the fields.",
    "Something draws your attention back to the stairs."
};


/* Do a probability travel in a wall */
static void do_prob_travel(struct player *p, struct chunk *c, int dir)
{
    bool do_move = true;
    struct loc grid, next;

    loc_copy(&grid, &p->grid);

    /* Paranoia */
    if ((dir == DIR_TARGET) || !dir) return;

    // Golem. Move. Only. Straight. Movement. Denied.
    if (streq(p->race->name, "Golem") || streq(p->race->name, "Homunculus"))
        if (dir == 1 || dir == 3 || dir == 7 || dir == 9)
            return;

    /* Ensure "dir" is in ddx/ddy array bounds */
    if (!VALID_DIR(dir)) return;

    next_grid(&next, &grid, dir);
    loc_copy(&grid, &next);

    while (true)
    {
        /* Do not get out of the level */
        if (!square_in_bounds_fully(c, &grid))
        {
            do_move = false;
            break;
        }

        /* Require a "naked" floor grid */
        if (!square_isempty(c, &grid) || square_isvault(c, &grid))
        {
            next_grid(&next, &grid, dir);
            loc_copy(&grid, &next);
            continue;
        }

        /* Everything is ok */
        do_move = true;
        break;
    }

    if (do_move)
    {
        monster_swap(c, &p->grid, &grid);
        player_handle_post_move(p, c, true, true, 0, true);
    }
}


/*
 * Move player in the given direction.
 *
 * This routine should only be called when energy has been expended.
 *
 * Note that this routine handles monsters in the destination grid,
 * and also handles attempting to move into walls/doors/rubble/etc.
 */
void move_player(struct player *p, struct chunk *c, int dir, bool disarm, bool check_pickup,
    bool force, int delayed, bool can_attack)
{
    bool old_dtrap, new_dtrap, old_pit, new_pit;
    bool do_move = true;
    struct source who_body;
    struct source *who = &who_body;
    bool trap, door, switched = false;
    struct loc grid;

    /* Ensure "dir" is in ddx/ddy array bounds */
    if (!VALID_DIR(dir)) return;

    // Golem. Move. Only. Straight. Movement. Denied.
    // (but can walk any direction in town)
    if ((streq(p->race->name, "Golem") || streq(p->race->name, "Homunculus")) &&
        p->wpos.depth > 0)
            if (dir == 1 || dir == 3 || dir == 7 || dir == 9)
                return;

    next_grid(&grid, &p->grid, dir);
    trap = square_isdisarmabletrap(c, &grid);
    door = square_iscloseddoor(c, &grid);

    /* Handle polymorphed players */
    if (p->poly_race)
    {
        if (rf_has(p->poly_race->flags, RF_NEVER_MOVE)) do_move = false;

        /* Unaware players trying to move reveal themselves */
        if (p->k_idx) aware_player(p, p);
    }

    /* New player location on the world map */
    if ((p->wpos.depth == 0) && !square_in_bounds_fully(c, &grid))
    {
        struct wild_type *w_ptr;
        struct loc new_world_grid, new_grid;

        loc_copy(&new_world_grid, &p->wpos.grid);
        loc_copy(&new_grid, &p->grid);

        /* Handle polymorphed players */
        if (!do_move)
        {
            msg(p, "You cannot move!");
            return;
        }

        // forbid leaving zeitnot "town"
        if (in_zeitnot_town(&p->wpos))
        {
            if (OPT(p, birth_zeitnot))
            {
                msg(p, ONE_OF(comment_zeitnot));
                disturb(p, 0);
                return;
            }
        /* Leaving base town */
        } else if (in_base_town(&p->wpos)) // there 'base' means 'ironman'
        {
            // Forbid for ironman
            if (cfg_diving_mode > 1 || OPT(p, birth_ironman))
            {
                if (cfg_diving_mode > 1)
                    msg(p, "There is a wall blocking your way.");
                else
                    msg(p, ONE_OF(comment_zeitnot));
                disturb(p, 0);
                return;
            }

            /* Warn */
            if (p->lev == 1)
                msg(p, "Really enter the wilderness? The dungeon entrance is in the town!");
        }

        /* Find his new location */
        if (grid.y <= 0)
        {
            new_world_grid.y++;
            new_grid.y = c->height - 2;
        }
        if (grid.y >= c->height - 1)
        {
            new_world_grid.y--;
            new_grid.y = 1;
        }
        if (grid.x <= 0)
        {
            new_world_grid.x--;
            new_grid.x = c->width - 2;
        }
        if (grid.x >= c->width - 1)
        {
            new_world_grid.x++;
            new_grid.x = 1;
        }


        if (
            !is_dm_p(p) && (
                // don't allow regular chars visit Deeptown/Zeitnot/Ironman locs
                (new_world_grid.x == -6 && new_world_grid.y == 0 && !OPT(p, birth_deeptown)) ||
                (new_world_grid.x == 0 && new_world_grid.y == 6 && !OPT(p, birth_zeitnot)) ||
                (new_world_grid.x == 0 && new_world_grid.y == -6 && !OPT(p, birth_ironman)) ||
                // and prevent mod heroes from leaving their designated locations
                (!(new_world_grid.x == -6 && new_world_grid.y == 0) && OPT(p, birth_deeptown)) ||
                (!(new_world_grid.x == 0 && new_world_grid.y == 6) && OPT(p, birth_zeitnot)) ||
                (!(new_world_grid.x == 0 && new_world_grid.y == -6) && OPT(p, birth_ironman))
            )
        )
        {
            if (p->wpos.grid.x == -6 && p->wpos.grid.y == 0)
                msg(p,
                "You hear a whisper: 'Do not leave.. there is still much to be done here...'");
            else
                msg(p, "You shall not pass!");
            return;
        }

        /* New location */
        w_ptr = get_wt_info_at(&new_world_grid);

        /* Check to make sure he hasn't hit the edge of the world */
        if (!w_ptr)
        {
            switch (randint0(2))
            {
                case 0: msg(p, "You have reached the Walls of the World. You can not pass."); break;
                case 1: msg(p, "You cannot go beyond the Walls of the World."); break;
            }

            disturb(p, 0);
            return;
        }

        /* Hack -- DM redesigning the level */
        if (chunk_inhibit_players(&w_ptr->wpos))
        {
            msg(p, "Something prevents you from going this way...");
            return;
        }

        /* Change location */
        dungeon_change_level(p, c, &w_ptr->wpos, LEVEL_OUTSIDE);

        /* Hack -- replace the player */
        loc_copy(&p->old_grid, &new_grid);
        loc_copy(&p->grid, &new_grid);

        /* Update the wilderness map */
        wild_set_explored(p, &w_ptr->wpos);

        /* Disturb if necessary */
        if (OPT(p, disturb_panel)) disturb(p, 0);

        return;
    }

    /* Save "last direction moved" */
    p->last_dir = dir;

    square_actor(c, &grid, who);

    /* Bump into other players */
    if (who->player)
    {
        /* Don't bump into self! */
        if (who->player != p)
        {
            /*
             * Players can switch places if:
             * - none of them is wraithed
             * - they're moving toward each other
             * - player initiating the switch doesn't stand on potentially harmful terrain
             */
            bool can_switch = (!player_passwall(p) && !player_passwall(who->player) &&
                (ddy[who->player->last_dir] == (0 - ddy[dir])) &&
                (ddx[who->player->last_dir] == (0 - ddx[dir])) &&
                square_ispassable(c, &p->grid) && !square_isplayertrap(c, &p->grid) &&
                !square_isdamaging(c, &p->grid));

            /* Reveal mimics */
            if (who->player->k_idx)
                aware_player(p, who->player);

            /* Check for an attack */
            if (pvp_check(p, who->player, PVP_DIRECT, true, square(c, &grid)->feat))
            {
                if (can_attack) py_attack(p, c, &grid);
                else p->upkeep->energy_use = false;
            }

            /* Handle polymorphed players */
            else if (!do_move)
                msg(p, "You cannot move!");

            /* Switch places */
            else if (can_switch || (who->player->dm_flags & DM_SECRET_PRESENCE))
                switched = true;

            /* Bump into other players */
            else if (!(p->dm_flags & DM_SECRET_PRESENCE))
            {
                char p_name[NORMAL_WID], q_name[NORMAL_WID];

                player_desc(p, q_name, sizeof(q_name), who->player, false);
                player_desc(who->player, p_name, sizeof(p_name), p, true);

                /* Tell both about it */
                msg(p, "You bump into %s.", q_name);
                msg(who->player, "%s bumps into you.", p_name);

                /* Disturb both parties */
                disturb(p, 0);
                disturb(who->player, 0);
            }
        }

        if (!switched) return;
    }

    /* Bump into monsters */
    if (who->monster)
    {
        bool swap = false;

        /* Check for an attack */
        if (pvm_check(p, who->monster))
        {
            /* Attack monsters */
            if (monster_is_camouflaged(who->monster))
            {
                become_aware(p, c, who->monster);

                /* Camouflaged monster wakes up and becomes aware */
                monster_wake(p, who->monster, false, 100);
            }
            else
            {
                if (can_attack) py_attack(p, c, &grid);
                else p->upkeep->energy_use = false;
            }
        }

        /* Handle polymorphed players */
        else if (!do_move)
            msg(p, "You cannot move!");

        /* Reveal mimics */
        else if (monster_is_camouflaged(who->monster))
            become_aware(p, c, who->monster);

        /* Switch places */
        else
            swap = true;

        if (!swap) return;
    }

    /* Arena */
    if (square_ispermarena(c, &grid))
    {
        access_arena(p, &grid);
        return;
    }

    /* Prob travel */
    if (p->timed[TMD_PROBTRAVEL] && !square_ispassable(c, &grid) &&
        !streq(p->clazz->name, "Assassin") && !streq(p->clazz->name, "Timeturner"))
    {
        do_prob_travel(p, c, dir);
        return;
    }

    /* Optionally alter traps/doors on movement */
    // exception: vampire race 'mist' form etc
    if (door && ((p->poly_race && streq(p->poly_race->name, "vampiric mist_")) || streq(p->race->name, "Ooze")))
        ;
    else if (((trap && disarm) || door) && square_isknown(p, &grid))
    {
        do_cmd_alter(p, dir);
        return;
    }

    /* Stop running before known traps */
    if (trap && p->upkeep->running && !player_is_trapsafe(p))
    {
        disturb(p, 0);

        /* No move made so no energy spent. */
        p->upkeep->energy_use = false;

        return;
    }

    // Slippery grounds
    if (streq(p->terrain, "\tIce\0") && !streq(p->clazz->name, "Cryokinetic") && one_in_(6))
    {
        msgt(p, MSG_TERRAIN_SLIP, "You slip on the icy floor!");
        return;
    }

    // Normal players can not walk through "walls", but some can pass trees and other terrain...
    if (!player_passwall(p) && !square_ispassable(c, &grid))
    {
        // vampire race 'mist' form can pass all except walls
        if (p->poly_race && streq(p->poly_race->name, "vampiric mist_"))
        {
            if (square_ispassable(c, &grid) || square_istree(c, &grid) || square_isrubble(c, &grid) ||
            (square_iscloseddoor(c, &grid) && !square_home_iscloseddoor(c, &grid)))
                ;
            else
            {
                msgt(p, MSG_HITWALL, "Way is blocked.");
                return;
            }
        }
        else if (square_istree(c, &grid))
        {
            // allow pass trees in town by running OR for druid bird/rat form
            if (p->wpos.depth == 0 || (p->poly_race && (streq(p->poly_race->name, "bird-form") ||
                streq(p->poly_race->name, "rat-form")))) ;
            // other cases
            else if ((streq(p->clazz->name, "Druid") || streq(p->race->name, "Ent")) &&
                magik(p->lev + 50)) ;
            else if ((streq(p->clazz->name, "Shaman") || streq(p->clazz->name, "Ranger")) &&
                magik(p->lev)) ;
            else if (player_of_has(p, OF_FLYING) && !player_of_has(p, OF_CANT_FLY)) ;
            else return;
        }
        // druid class 'rat' form can pass walls sometimes (except permawalls)
        else if (p->poly_race && streq(p->poly_race->name, "rat-form") &&
                !square_isperm(c, &grid) && magik(p->lev / 5)) ;
        // ooze can pass doors
        else if (square_iscloseddoor(c, &grid) && !square_home_iscloseddoor(c, &grid))
        {
            if (streq(p->race->name, "Ooze")) ;
            else
            {
                msgt(p, MSG_HITWALL, "There is a door blocking your way.");
                return;
            }
        }
        else // regular PWMA case
        {
            disturb(p, 0);

            /* Notice unknown obstacles */
            if (!square_isknown(p, &grid))
            {
                /* Rubble */
                if (square_isrubble(c, &grid))
                {
                    msgt(p, MSG_HITWALL, "There is a pile of rubble blocking your way.");
                    if (!square_isrubble_p(p, &grid))
                    {
                        square_memorize(p, c, &grid);
                        square_light_spot_aux(p, c, &grid);
                    }
                }

                /* Closed doors */
                else if (square_iscloseddoor(c, &grid))
                {
                    msgt(p, MSG_HITWALL, "There is a door blocking your way.");
                    if (!square_iscloseddoor_p(p, &grid))
                    {
                        square_memorize(p, c, &grid);
                        square_light_spot_aux(p, c, &grid);
                    }
                }

                /* Tree */
                else if (square_istree(c, &grid))
                {
                    msgt(p, MSG_HITWALL, "There is a tree blocking your way.");
                    if (!square_istree_p(p, &grid))
                    {
                        square_memorize(p, c, &grid);
                        square_light_spot_aux(p, c, &grid);
                    }
                }

                /* Wall (or secret door) */
                else
                {
                    msgt(p, MSG_HITWALL, "There is a wall blocking your way.");
                    if (square_ispassable_p(p, &grid))
                    {
                        square_forget(p, &grid);
                        square_light_spot_aux(p, c, &grid);
                    }
                }
            }

            /* No move but do not refund energy */
            return;
        }
    }

    // Paranoia to prevent go out of dungeon borders
    if (square_isunpassable(c, &grid) && !square_in_bounds_fully(c, &grid))
    {
            msg(p, "The wall blocks your movement.");
            disturb(p, 0);
            return;
    }

    /* Permanent walls */
    if (player_passwall(p) && square_isunpassable(c, &grid))
    {
        /* Forbid in most cases */
        if (p->timed[TMD_WRAITHFORM] || player_can_undead(p) || !square_in_bounds_fully(c, &grid))
        {
            /* Message */
            msg(p, "The wall blocks your movement.");

            disturb(p, 0);
            return;
        }
    }

    /* Wraith trying to run inside a house */
    if (p->timed[TMD_WRAITHFORM] && square_home_iscloseddoor(c, &grid))
    {
        do_cmd_open(p, dir, false);
        return;
    }

    /* Handle polymorphed players */
    if (!do_move && !force)
    {
        msg(p, "You cannot move!");
        return;
    }

    /* See if trap detection status will change */
    old_dtrap = square_isdtrap(p, &p->grid);
    new_dtrap = square_isdtrap(p, &grid);

    /* Note the change in the detect status */
    if (old_dtrap != new_dtrap) p->upkeep->redraw |= (PR_DTRAP);

    /* Disturb if the player is about to leave the area */
    if (p->upkeep->running && !p->upkeep->running_firststep && old_dtrap && !new_dtrap &&
        random_level(&p->wpos))
    {
        disturb(p, 0);

        /* No move made so no energy spent. */
        p->upkeep->energy_use = false;

        return;
    }

    /* PWMAngband: display a message when entering a pit */
    old_pit = square_ispitfloor(c, &p->grid);
    new_pit = square_ispitfloor(c, &grid);
    if (new_pit && !old_pit)
        msgt(p, MSG_ENTER_PIT, "The floor is very dusty and the air feels very still!");

    /* Trap immune player learns that they are */
    if (trap && player_of_has(p, OF_TRAP_IMMUNE))
        equip_learn_flag(p, OF_TRAP_IMMUNE);

    /* Move player */
    monster_swap(c, &p->grid, &grid);
    p->upkeep->redraw |= PR_STATE;

    /* Switch places */
    if (switched)
    {
        /* Don't tell people they bumped into the Dungeon Master */
        if (!(who->player->dm_flags & DM_SECRET_PRESENCE))
        {
            char p_name[NORMAL_WID], q_name[NORMAL_WID];

            player_desc(p, q_name, sizeof(q_name), who->player, false);
            player_desc(who->player, p_name, sizeof(p_name), p, false);

            /* Tell both of them */
            msg(p, "You switch places with %s.", q_name);
            msg(who->player, "You switch places with %s.", p_name);

            /* Disturb both of them */
            disturb(p, 0);
            disturb(who->player, 0);
        }

        /* Unhack both of them */
        who->player->last_dir = p->last_dir = 5;

        /* No checks for the player that got moved, but at least notice objects */
        player_know_floor(who->player, c);
    }

    player_handle_post_move(p, c, true, check_pickup, delayed, true);

    p->upkeep->running_firststep = false;

    // each step decrease zeitnot_timer by -2 (in addition to -1 by turn pass)
    if (OPT(p, birth_zeitnot) && !p->is_dead && p->chp >= 0 &&
        (p->wpos.depth != 20 && p->wpos.depth != 40 && // except dungeon-towns
         p->wpos.depth != 60 && p->wpos.depth != 80)) {
        p->zeitnot_timer -= 2;
    }

    /* Hack -- we're done if player is gone (trap door) */
    if (p->upkeep->new_level_method) return;

    /*
     * Hack -- if we are the dungeon master, and our movement hook
     * is set, call it.  This is used to make things like building walls
     * and summoning monster armies easier.
     */
    if (is_dm_p(p) && master_move_hook)
        master_move_hook(p, NULL);
}


/*
 * Determine if a given grid may be "walked"
 */
static bool do_cmd_walk_test(struct player *p)
{
    /* Check preventive inscription '^;' */
    if (check_prevent_inscription(p, INSCRIPTION_WALK))
    {
        msg(p, "The item's inscription prevents it.");
        return false;
    }

    /* Tests are done in move_player() */

    /* Okay */
    return true;
}


/*
 * Apply erratic movement, if needed, to a direction
 *
 * Return true if direction changes.
 */
static bool erratic_dir(struct player *p, int *dp)
{
    int dir;
    int erratic = 0;

    /* Default */
    dir = (*dp);

    /* Handle polymorphed players */
    if (p->poly_race)
    {
        if (rf_has(p->poly_race->flags, RF_RAND_25)) erratic += 10;
        if (rf_has(p->poly_race->flags, RF_RAND_50)) erratic += 20;
        if (rf_has(p->poly_race->flags, RF_RAND_100)) erratic = 40;
    }

    /* Apply "erratic movement" */
    if (magik(erratic))
    {
        /* Random direction */
        dir = ddd[randint0(8)];
    }

    /* Notice erratic movement */
    if ((*dp) != dir)
    {
        /* Save direction */
        (*dp) = dir;

        /* Erratic movement */
        return true;
    }

    /* Normal movement */
    return false;
}


static void clear_web(struct player *p, struct chunk *c)
{
    msg(p, "You clear the web.");
    square_clear_feat(c, &p->grid);
    update_visuals(&p->wpos);
    fully_update_flow(&p->wpos);
    use_energy(p);
}


static bool player_confuse_dir_nomsg(struct player *p, int *dp)
{
    int dir = *dp;

    /* Random direction */
    if ((p->timed[TMD_CONFUSED] || p->timed[TMD_CONFUSED_REAL]) &&
        ((dir == DIR_TARGET) || magik(75)))
            dir = ddd[randint0(8)];

    if (*dp != dir)
    {
        *dp = dir;
        return true;
    }

    return false;
}


/*
 * Walk in the given direction.
 */
bool do_cmd_walk(struct player *p, int dir)
{
    struct chunk *c = chunk_get(&p->wpos);
    struct loc grid;

    /* Check energy */
    if (!has_energy_per_move(p)) return false;

    /* Get a direction (or abort) */
    if (!dir) return true;

    // Golem. Move. Only. Straight. Movement. Denied.
    // (but can walk any direction in town)
    if ((streq(p->race->name, "Golem") || streq(p->race->name, "Homunculus")) &&
        p->wpos.depth > 0)
            if (dir == 1 || dir == 3 || dir == 7 || dir == 9)
                return false;

    /* If we're in a web, deal with that */
    if (square_iswebbed(c, &p->grid))
    {
        // spider/homi race pass web
		if (streq(p->race->name, "Spider") || streq(p->race->name, "Homunculus"))
            ;
		/* Handle polymorphed players */
        else if (p->poly_race)
        {
            /* If we can pass, no need to clear */
            if (!rf_has(p->poly_race->flags, RF_PASS_WEB))
            {
                /* Insubstantial monsters go right through */
                if (rf_has(p->poly_race->flags, RF_PASS_WALL)) {}

                /* If you can pass through walls, you can destroy a web */
                else if (monster_passes_walls(p->poly_race))
                {
                    /* Check energy */
                    if (has_energy(p, false))
                    {
                        clear_web(p, c);
                        return true;
                    }

                    return false;
                }

                /* Clearing costs a turn */
                else if (rf_has(p->poly_race->flags, RF_CLEAR_WEB))
                {
                    /* Check energy */
                    if (has_energy(p, false))
                    {
                        clear_web(p, c);
                        return true;
                    }

                    return false;
                }

                /* Stuck */
                else return true;
            }
        }

        /* Clear the web, finish turn */
        else
        {
            /* Check energy */
            if (has_energy(p, false))
            {
                clear_web(p, c);
                return true;
            }

            return false;
        }
    }

    /* Verify legality */
    if (!do_cmd_walk_test(p)) return true;

    /* Apply confusion if necessary */
    if (player_confuse_dir_nomsg(p, &dir))
    {
        /* Confused movements use energy no matter what */
        if (!has_energy(p, false)) return false;
        msg(p, "You are confused.");
    }

    /* Apply erratic movement if necessary */
    if (erratic_dir(p, &dir))
    {
        /* Erratic movements use energy no matter what */
        if (!has_energy(p, false)) return false;
    }

    /* Ensure "dir" is in ddx/ddy array bounds */
    if (!VALID_DIR(dir)) return true;

    next_grid(&grid, &p->grid, dir);

    /* Paranoia */
    if (!square_in_bounds(c, &grid))
        quit("Trying to walk out of bounds, please report this bug.");

    /* Attempt to disarm unless it's a trap and we're trapsafe */
    p->upkeep->energy_use = true;
    move_player(p, c, dir, !(square_isdisarmabletrap(c, &grid) && player_is_trapsafe(p)), true,
        false, 0, has_energy(p, false));

    /* Take a turn */
    if (!p->upkeep->energy_use) return false;
    use_energy(p);
    return true;
}


/*
 * Walk into a trap
 */
bool do_cmd_jump(struct player *p, int dir)
{
    struct chunk *c = chunk_get(&p->wpos);

    /* Check energy */
    if (!has_energy_per_move(p)) return false;

    /* Get a direction (or abort) */
    if (!dir) return true;

    // Golem. Move. Only. Straight. Movement. Denied.
    if (streq(p->race->name, "Golem") || streq(p->race->name, "Homunculus"))
        if (dir == 1 || dir == 3 || dir == 7 || dir == 9)
            return false;

    /* If we're in a web, deal with that */
    if (square_iswebbed(c, &p->grid))
    {
        /* Handle polymorphed players */
        if (p->poly_race)
        {
            /* If we can pass, no need to clear */
            if (!rf_has(p->poly_race->flags, RF_PASS_WEB))
            {
                /* Insubstantial monsters go right through */
                if (rf_has(p->poly_race->flags, RF_PASS_WALL)) {}

                /* If you can pass through walls, you can destroy a web */
                else if (monster_passes_walls(p->poly_race))
                {
                    /* Check energy */
                    if (has_energy(p, false))
                    {
                        clear_web(p, c);
                        return true;
                    }

                    return false;
                }

                /* Clearing costs a turn */
                else if (rf_has(p->poly_race->flags, RF_CLEAR_WEB))
                {
                    /* Check energy */
                    if (has_energy(p, false))
                    {
                        clear_web(p, c);
                        return true;
                    }

                    return false;
                }

                /* Stuck */
                else return true;
            }
        }

        /* Clear the web, finish turn */
        else
        {
            /* Check energy */
            if (has_energy(p, false))
            {
                clear_web(p, c);
                return true;
            }

            return false;
        }
    }

    /* Verify legality */
    if (!do_cmd_walk_test(p)) return true;

    /* Apply confusion if necessary */
    if (player_confuse_dir_nomsg(p, &dir))
    {
        /* Confused movements use energy no matter what */
        if (!has_energy(p, false)) return false;
        msg(p, "You are confused.");
    }

    /* Apply erratic movement if necessary */
    if (erratic_dir(p, &dir))
    {
        /* Erratic movements use energy no matter what */
        if (!has_energy(p, false)) return false;
    }

    /* Ensure "dir" is in ddx/ddy array bounds */
    if (!VALID_DIR(dir)) return true;

    /* Move the player */
    p->upkeep->energy_use = true;
    move_player(p, c, dir, false, true, false, 0, has_energy(p, false));

    /* Take a turn */
    if (!p->upkeep->energy_use) return false;
    use_energy(p);
    return true;
}


/*
 * Determine if a given grid may be "run"
 */
static bool do_cmd_run_test(struct player *p, struct loc *grid)
{
    struct chunk *c = chunk_get(&p->wpos);

    /* Check preventive inscription '^.' */
    if (check_prevent_inscription(p, INSCRIPTION_RUN))
    {
        msg(p, "The item's inscription prevents it.");
        return false;
    }

    /* Ghosts run right through everything */
    if (player_passwall(p)) return true;

    /* Do wilderness hack, keep running from one outside level to another */
    if (!square_in_bounds_fully(c, grid) && (p->wpos.depth == 0)) return true;

    /* Illegal grids are not known walls XXX XXX XXX */
    if (!square_in_bounds(c, grid)) return true;

    /* Hack -- walking obtains knowledge XXX XXX */
    if (!square_isknown(p, grid)) return true;

    // allow pass trees in town by running
    if (square_istree(c, grid) && (p->wpos.depth == 0))
        return true;

    /* Require open space */
    if (!square_ispassable(c, grid))
    {
        /* Rubble */
        if (square_isrubble(c, grid))
        {
            msgt(p, MSG_HITWALL, "There is a pile of rubble in the way!");
            if (!square_isrubble_p(p, grid))
            {
                square_memorize(p, c, grid);
                square_light_spot_aux(p, c, grid);
            }
        }

        /* Door */
        else if (square_iscloseddoor(c, grid))
            return true;

        /* Tree */
        else if (square_istree(c, grid))
        {
            msgt(p, MSG_HITWALL, "There is a tree in the way!");
            if (!square_istree_p(p, grid))
            {
                square_memorize(p, c, grid);
                square_light_spot_aux(p, c, grid);
            }
        }

        /* Wall */
        else
        {
            msgt(p, MSG_HITWALL, "There is a wall in the way!");
            if (square_ispassable_p(p, grid))
            {
                square_forget(p, grid);
                square_light_spot_aux(p, c, grid);
            }
        }

        /* Cancel repeat */
        disturb(p, 1);

        /* Nope */
        return false;
    }

    /* Okay */
    return true;
}


/*
 * Start running.
 *
 * Note that running while confused is not allowed
 */
bool do_cmd_run(struct player *p, int dir)
{
    struct chunk *c = chunk_get(&p->wpos);
    struct loc grid;

    /* Check energy */
    if (!has_energy_per_move(p)) return false;

    /* Not while confused */
    if (p->timed[TMD_CONFUSED] || p->timed[TMD_CONFUSED_REAL])
    {
        msg(p, "You are too confused!");
        return true;
    }

    /* Handle polymorphed players */
    if (p->poly_race)
    {
        if (rf_has(p->poly_race->flags, RF_RAND_25) || rf_has(p->poly_race->flags, RF_RAND_50) ||
            rf_has(p->poly_race->flags, RF_RAND_100))
        {
            msg(p, "Your nature prevents you from running straight.");
            return true;
        }
    }

    /* Ignore invalid directions */
    if ((dir == DIR_TARGET) || !VALID_DIR(dir)) return true;

    // Golem. Move. Only. Straight. Movement. Denied.
    if (streq(p->race->name, "Golem") || streq(p->race->name, "Homunculus"))
        if (dir == 1 || dir == 3 || dir == 7 || dir == 9)
            return false;

    /* Ignore non-direction if we are not running */
    if (!p->upkeep->running && !dir) return true;

    /* If we're in a web, deal with that */
    if (square_iswebbed(c, &p->grid))
    {
        /* Handle polymorphed players */
        if (p->poly_race)
        {
            /* If we can pass, no need to clear */
            if (!rf_has(p->poly_race->flags, RF_PASS_WEB))
            {
                /* Insubstantial monsters go right through */
                if (rf_has(p->poly_race->flags, RF_PASS_WALL)) {}

                /* If you can pass through walls, you can destroy a web */
                else if (monster_passes_walls(p->poly_race))
                {
                    /* Check energy */
                    if (has_energy(p, false))
                    {
                        clear_web(p, c);
                        return true;
                    }

                    return false;
                }

                /* Clearing costs a turn */
                else if (rf_has(p->poly_race->flags, RF_CLEAR_WEB))
                {
                    /* Check energy */
                    if (has_energy(p, false))
                    {
                        clear_web(p, c);
                        return true;
                    }

                    return false;
                }

                /* Stuck */
                else return true;
            }
        }

        /* Clear the web, finish turn */
        else
        {
            /* Check energy */
            if (has_energy(p, false))
            {
                clear_web(p, c);
                return true;
            }

            return false;
        }
    }

    /* Continue running if we are already running in this direction */
    if (p->upkeep->running && (dir == p->run_cur_dir)) dir = 0;

    /* Get location */
    if (dir)
    {
        next_grid(&grid, &p->grid, dir);

        /* Verify legality */
        if (!do_cmd_run_test(p, &grid)) return true;
    }

    /* Start run */
    return run_step(p, dir);
}


/*
 * Rest (restores hit points and mana and such)
 */
bool do_cmd_rest(struct player *p, int16_t resting)
{
    /*
     * A little sanity checking on the input - only the specified negative
     * values are valid.
     */
    if ((resting < 0) && !player_resting_is_special(resting))
        return true;

    /* Check energy */
    if (!has_energy(p, true)) return false;

    /* Do some upkeep on the first turn of rest */
    if (!player_is_resting(p))
    {
        /* Disturb us: reset running if needed */
        disturb(p, 1);

        /* Redraw the state */
        p->upkeep->redraw |= (PR_STATE);
    }

    /* Set the counter, and stop if told to */
    player_resting_set_count(p, resting);
    if (!player_is_resting(p)) return true;

    /* Take a turn */
    player_resting_step_turn(p);

    /* Redraw the state if requested */
    handle_stuff(p);

    /* Prepare to continue, or cancel and clean up */
    if (player_resting_count(p) > 0)
        cmd_rest(p, resting - 1);
    else if (player_resting_is_special(resting))
        cmd_rest(p, resting);
    else
        player_resting_cancel(p, false);

    return true;
}


/*
 * Spend a turn doing nothing
 */
void do_cmd_sleep(struct player *p)
{
    /* Take a turn */
    use_energy(p);
}


/*
 * Array of feeling strings for object feelings.
 * Keep strings at 36 or less characters to keep the
 * combined feeling on one row.
 */
static const char *obj_feeling_text[][2] =
{
    {"this looks like any other level.", "this place looks familiar."},
    {"you sense an item of wondrous power!", "there was an item of wondrous power here!"},
    {"there are superb treasures here.", "there were superb treasures here."},
    {"there are excellent treasures here.", "there were excellent treasures here."},
    {"there are very good treasures here.", "there were very good treasures here."},
    {"there are good treasures here.", "there were good treasures here."},
    {"there may be something worthwhile here.", "there was something worthwhile here."},
    {"there may not be much interesting here.", "there was nothing interesting here."},
    {"there aren't many treasures here.", "there weren't many treasures here."},
    {"there are only scraps of junk here.", "there were only scraps of junk here."},
    {"there is naught but cobwebs here.", "there was naught but cobwebs here."}
};


/*
 * Array of feeling strings for monster feelings.
 * Keep strings at 36 or less characters to keep the
 * combined feeling on one row.
 */
static const char *mon_feeling_text[][2] =
{
    {"You are still uncertain about this place", "You are still uncertain about this place"},
    {"Omens of death haunt this place", "Omens of death haunted this place"},
    {"This place seems murderous", "This place seemed murderous"},
    {"This place seems terribly dangerous", "This place seemed terribly dangerous"},
    {"You feel anxious about this place", "You were anxious about this place"},
    {"You feel nervous about this place", "You were nervous about this place"},
    {"This place does not seem too risky", "This place did not seem too risky"},
    {"This place seems reasonably safe", "This place seemed reasonably safe"},
    {"This seems a tame, sheltered place", "This seemed a tame, sheltered place"},
    {"This seems a quiet, peaceful place", "This seemed a quiet, peaceful place"}
};


/*
 * Display the feeling. Players always get a monster feeling.
 * Object feelings are delayed until the player has explored some
 * of the level.
 *
 * Players entering a static level will get a different message, since
 * the feeling may not be accurate anymore.
 */
void display_feeling(struct player *p, bool obj_only)
{
    int16_t obj_feeling;
    int16_t mon_feeling;
    const char *join;
    uint8_t set = 0;
    int n_obj_feelings = N_ELEMENTS(obj_feeling_text);
    int n_mon_feelings = N_ELEMENTS(mon_feeling_text);

    // Don't show feelings for some races
    if (!cfg_level_feelings || !OPT(p, birth_feelings) ||
        streq(p->race->name, "Frostmen"))
        return;

    /* No feeling in towns */
    if (forbid_town(&p->wpos))
    {
        msg(p, "Looks like a typical town.");
        return;
    }

    /* Hack -- special levels */
    if (special_level(&p->wpos))
    {
        msg(p, "Looks like a special level.");
        return;
    }

    /* No feeling in the wilderness */
    if (p->wpos.depth == 0)
    {
        msg(p, "Looks like a typical wilderness level.");
        return;
    }

    /* PWMAngband: monster feeling on exploration + no object feeling at level 1 */
    if ((p->cave->feeling_squares < z_info->feeling_need) && (cfg_level_feelings == 1)) return;

    /* Hack -- player entering a static level */
    if (p->feeling < 0)
    {
        int i, obj_f, mon_f;

        obj_feeling = 0;
        mon_feeling = 0;

        /* Get a feeling from other players */
        for (i = 1; i <= NumPlayers; i++)
        {
            struct player *q = player_get(i);

            if (q == p) continue;
            if (!wpos_eq(&q->wpos, &p->wpos)) continue;
            if (q->feeling < 0) continue;

            obj_f = q->feeling / 10;
            mon_f = q->feeling - (10 * obj_f);

            if (obj_f > obj_feeling) obj_feeling = obj_f;
            if (mon_f > mon_feeling) mon_feeling = mon_f;
        }

        /* Display a different message */
        set = 1;
    }
    else
    {
        obj_feeling = p->feeling / 10;
        mon_feeling = p->feeling - (10 * obj_feeling);
    }

    /* Verify the feeling */
    if (obj_feeling >= n_obj_feelings) obj_feeling = n_obj_feelings - 1;

    /* Display only the object feeling when it's first discovered. */
    if (obj_only && (cfg_level_feelings == 3))
    {
        disturb(p, 0);
        msg(p, "You feel that %s", obj_feeling_text[obj_feeling][set]);
        p->obj_feeling = obj_feeling;
        return;
    }

    /* PWMAngband: no object feeling at level 2 */
    if (obj_only && (cfg_level_feelings == 2)) return;

    /* Verify the feeling */
    if (mon_feeling >= n_mon_feelings) mon_feeling = n_mon_feelings - 1;

    /* Players automatically get a monster feeling. */
    if (((p->cave->feeling_squares < z_info->feeling_need) && (cfg_level_feelings == 3)) ||
        (cfg_level_feelings == 1) || (cfg_level_feelings == 2))
    {
        msg(p, "%s.", mon_feeling_text[mon_feeling][set]);
        p->mon_feeling = mon_feeling;
        return;
    }

    /* Decide the conjunction */
    if (((mon_feeling <= 5) && (obj_feeling > 6)) || ((mon_feeling > 5) && (obj_feeling <= 6)))
        join = ", yet";
    else
        join = ", and";

    /* Display the feeling */
    msg(p, "%s%s %s", mon_feeling_text[mon_feeling][set], join, obj_feeling_text[obj_feeling][set]);
    p->obj_feeling = obj_feeling;
    p->mon_feeling = mon_feeling;

    // sound for 9 danger
    if (mon_feeling >= 9)
        sound(p, MSG_DANGER_9);
}


/*
 * Check if the player has enough funds to buy a house.
 * The player is allowed to buy a small house (value < 10000 au) with a deed of property.
 */
static int can_buy_house(struct player *p, int price)
{
    int i;
    
    if (p->lev < 8)
    {
        msg(p, "You must be level 8 to buy a house.");
        return false;
    }    

    /* Check for deeds of property */
    if (price < 10000)
    {
        for (i = 0; i < z_info->pack_size; i++)
        {
            struct object *obj = p->upkeep->inven[i];

            if (!obj) continue;

            if (tval_is_deed(obj))
            {
                use_object(p, obj, 1, true);

                /* Used a deed of property */
                return 1;
            }
        }
    }

    /* Check for enough funds */
    if (price > p->au)
    {
        /* Not enough money, message */
        msg(p, "You do not have enough money.");
        return 0;
    }

    /* Actually bought the house */
    return 2;
}


/*
 * Buy a house. It is assumed that the player already knows the price.
 * Hacked to sell houses for half price.
 */
void do_cmd_purchase_house(struct player *p, int dir)
{
    int i, n, check;
    int house_area_size = 0;
    struct chunk *c = chunk_get(&p->wpos);
    const char *house_type_desc; // house name for msg

    if (p->account_score < 10) {
        msg(p, "To buy house you need at least 10 account points (press Ctrl+r to check score).");
        return;
    }

    /* Ghosts cannot buy houses */
    if (p->ghost && !(p->dm_flags & DM_HOUSE_CONTROL))
    {
        /* Message */
        msg(p, "You cannot buy a house.");
        return;
    }

    /* Restricted by choice */
    if ((cfg_limited_stores == 3) || OPT(p, birth_no_stores))
    {
        /* Message */
        msg(p, "You cannot buy a house.");
        return;
    }

    /* Check preventive inscription '^h' */
    if (check_prevent_inscription(p, INSCRIPTION_HOUSE))
    {
        msg(p, "The item's inscription prevents it.");
        return;
    }

    /* Check for no-direction -- confirmation (when selling house) */
    if (!dir)
    {
        struct house_type *house;

        i = p->current_house;
        p->current_house = -1;

        if (i == -1)
        {
            /* No house, message */
            msg(p, "You see nothing to sell there.");
            return;
        }

        house = house_get(i);

        /* Check for already-owned house */
        if (house->ownerid > 0)
        {
            /* See if he owns the house */
            if (house_owned_by(p, i))
            {
                /* Get the money */
                if (house->free)
                    msg(p, "You reset your house.");
                else
                {
                    // ! Temporary disabling getting gold for selling a house,
                    // as to fix selling house properly we need to store unique player ID
                    // (not player name... which anyway now stores account name) into house struct;
                    // as it will be only fair to sell home for full price to hero who
                    // built it.. not other heroes (now it's potential cheeze, see below)
                    ////////////////////////////////////////////////////////////////////////
                    // to prevent cheezing when player sell account's big house for new character
                    // we make possibility to sell it only for a very little if you are on low lvls
                    /* -----commented out------
                    int price_h = house->price;

                    // as we are trying to fight high-lvl cheating with very expansive houses,
                    // not low lvl ones which matters not
                    if (price_h < 25000)
                        price_h /= 7 - (p->lev / 10);
                    else
                        price_h /= (52 - p->lev);

                    // min price for 1 tile houses shouldn't be way too low
                    if (price_h < 1000)
                        price_h = 1000;

                    msg(p, "You sell your house for %ld gold.", price_h);
                    p->au += price_h;
                       -----commented out------*/
                    msg(p, "You no longer own this house");
                }

                /* House is no longer owned */
                reset_house(i);

                // drink Heroism potions to wipe house right on
                if (p->timed[TMD_HERO])
                    wipe_old_houses(&p->wpos);

                /* Redraw */
                p->upkeep->redraw |= (PR_GOLD);
                set_redraw_inven(p, NULL);

                /* Done */
                return;
            }

            /* The DM can reset a house */
            if (p->dm_flags & DM_HOUSE_CONTROL)
            {
                /* House is no longer owned */
                reset_house(i);
                msg(p, "The house has been reset.");

                /* Done */
                return;
            }
        }

        /* Message */
        msg(p, "You don't own this house.");

        /* No sale */
        return;
    }

    /* Be sure we have a direction */
    if (VALID_DIR(dir))
    {
        struct house_type *house;
        struct loc grid;

        /* Get requested direction */
        next_grid(&grid, &p->grid, dir);

        /* Check for a house */
        if ((i = pick_house(&p->wpos, &grid)) == -1)
        {
            /* No house, message */
            msg(p, "You see nothing to buy there.");
            return;
        }

        /* The door needs to be closed! */
        if (square_home_isopendoor(c, &grid))
        {
            /* Open door, message */
            msg(p, "You need to close the door first...");
            return;
        }

        /* Not inside the house! */
        if (house_inside(p, i))
        {
            /* Instead we allow players to look inside their own stores */
            if (house_owned_by(p, i))
                do_cmd_store(p, i);
            else
                msg(p, "You need to stand outside of the house first...");
            return;
        }

        house = house_get(i);

        /* Check for already-owned house */
        if (house->ownerid > 0)
        {
            /* See if he owns the house */
            if (house_owned_by(p, i))
            {
                /* Delay house transaction */
                p->current_house = i;

                /* Tell the client about the price */
                Send_store_sell(p, (house->free? 0: house->price / 2), false);

                /* Done */
                return;
            }

            /* The DM can reset a house */
            if (p->dm_flags & DM_HOUSE_CONTROL)
            {
                /* Delay house transaction */
                p->current_house = i;

                /* Tell the client about the reset */
                Send_store_sell(p, 0, true);

                /* Done */
                return;
            }

            /* Message */
            msg(p, "That house is already owned.");

            /* No sale */
            return;
        }

        /* The DM cannot buy houses! */
        if (p->dm_flags & DM_HOUSE_CONTROL)
        {
            /* Message */
            msg(p, "You cannot buy a house.");
            return;
        }

        /* Check already owned houses */
        n = houses_owned(p);

        /* Can we buy a house? */
        if (n == 2)
        {
            /* Too many houses, message */
            msg(p, "You cannot buy more houses.");
            return;
        }

        // check house area_size and find out do we have enough account points to buy it
        house_area_size = house_count_area_size(i);

        /* Check account score requirements */
        if      (house_area_size >= 324 && p->account_score < 10000) {
            msg(p, "To buy this house you need at least 10000 account points.");
            return;
        } else if (house_area_size >= 256 && p->account_score < 5000) {
            msg(p, "To buy this house you need at least 5000 account points.");
            return;
        } else if (house_area_size >= 144 && p->account_score < 3500) {
            msg(p, "To buy this house you need at least 3500 account points.");
            return;
        } else if (house_area_size >= 100 && p->account_score < 2500) {
            msg(p, "To buy this house you need at least 2500 account points.");
            return;
        } else if (house_area_size >= 64 && p->account_score < 2000) {
            msg(p, "To buy this house you need at least 2000 account points.");
            return;
        } else if (house_area_size >= 44 && p->account_score < 1500) {
            msg(p, "To buy this house you need at least 1500 account points.");
            return;
        } else if (house_area_size >= 32 && p->account_score < 1250) {
            msg(p, "To buy this house you need at least 1250 account points.");
            return;
        } else if (house_area_size >= 25 && p->account_score < 1000) {
            msg(p, "To buy this house you need at least 1000 account points.");
            return;
        } else if (house_area_size >= 20 && p->account_score < 800) {
            msg(p, "To buy this house you need at least 800 account points.");
            return;
        } else if (house_area_size >= 15 && p->account_score < 650) {
            msg(p, "To buy this house you need at least 650 account points.");
            return;
        } else if (house_area_size >= 9 && p->account_score < 500) {
            msg(p, "To buy this house you need at least 500 account points.");
            return;
        } else if (house_area_size >= 8 && p->account_score < 400) {
            msg(p, "To buy this house you need at least 400 account points.");
            return;
        } else if (house_area_size >= 7 && p->account_score < 300) {
            msg(p, "To buy this house you need at least 300 account points.");
            return;
        } else if (house_area_size >= 6 && p->account_score < 225) {
            msg(p, "To buy this house you need at least 225 account points.");
            return;
        } else if (house_area_size >= 5 && p->account_score < 150) {
            msg(p, "To buy this house you need at least 150 account points.");
            return;
        } else if (house_area_size >= 4 && p->account_score < 100) {
            msg(p, "To buy this house you need at least 100 account points.");
            return;
        } else if (house_area_size >= 3 && p->account_score < 50) {
            msg(p, "To buy this house you need at least 50 account points.");
            return;
        } else if (house_area_size >= 2 && p->account_score < 25) {
            msg(p, "To buy this house you need at least 25 account points.");
            return;
        }

        /* Check for funds or deed of property */
        check = can_buy_house(p, house->price);
        if (!check) return;

        /* Open the door */
        square_open_homedoor(c, &grid);

        /* Take some of the player's money (if the house was bought) */
        if (check == 2) p->au -= house->price;
        else house->free = 1;

        /* The house is now owned */
        set_house_owner(p, house);

        // get name of house for msg
        house_type_desc = get_house_type_desc(house_area_size);
        // message
        msg(p, "# %s bought a %s (%d tiles) for %d gold at (%d, %d).",
            p->name, house_type_desc, house_area_size,
            (check == 2 ? house->price : 0),
            p->wpos.grid.x, p->wpos.grid.y);

        /* Redraw */
        if (check == 2) p->upkeep->redraw |= (PR_GOLD);
    }
}


/*
 * "Cutting" bonus based on weapon efficiency against trees
 */
int wielding_cut(struct player *p)
{
    struct object *obj = equipped_item_by_slot_name(p, "weapon");

    /* Skip empty weapon slot */
    if (!obj) return 0;

    /* Weapon type */
    return tval_wielding_cut(obj);
}


/*
 * MAngband house creation code
 *
 * Disabled for now, as it is incomplete:
 *   - confuses the player owned shop mechanism
 *   - allows awkward door placements
 *   - doesn't check for other houses proximity (especially house doors!)
 *   - sets a price of 0 on new houses
 */


/*  
 * Create a new house door at the given location.
 * Due to the fact that houses can overlap (i.e. share a common wall) it
 * may not be possible to identify the house to which the door should belong.
 *
 * For example, on the left below, we have two houses overlapping, neither
 * have doors. On the right the player creates a door, but to which house does
 * it belong?
 *
 *   ####                        ####
 *   #  #@                       #  #@
 *   #  ###                      #  +##
 *   #### #                      #### #
 *      # #                         # #
 *      ###                         ###
 *
 * It is therefore possible to create a complex of houses such that the player
 * owned shop mechanism becomes confused.  When a player bumps one door they
 * see the contents of a different room listed.
 *
 * FIXME Therefore the player owned shop mechanism should treat overlapping
 * player created houses as a *single* house and present all goods in all
 * attached houses.
 */
static bool create_house_door(struct player *p, struct chunk *c, struct loc *grid)
{
    int house, lastmatch;

    /* Which house is the given location part of? */
    lastmatch = 0;
    house = find_house(p, grid, lastmatch);
    while (house >= 0)
    {
        struct house_type *h_ptr = house_get(house);

        /* Do we own this house? */
        if (!house_owned_by(p, house))
        {
            /* If we don't own this one, we can't own any overlapping ones */
            msg(p, "You do not own this house.");

            return false;
        }

        /* Does it already have a door? */
        if (loc_is_zero(&h_ptr->door))
        {
            /* No door, so create one! */
            loc_copy(&h_ptr->door, grid);
            square_colorize_door(c, grid, 0);
            msg(p, "You create a door for your house!");

            return true;
        }

        /* Check next house */
        lastmatch = house + 1;
        house = find_house(p, grid, lastmatch);
    }

    /* We searched all matching houses and none needed a door */
    msg(p, "You can't do that here.");

    return false;
}

/*
 * Check if the area around house foundation is allowed.
 */
static bool check_around_foundation (struct player *p, struct chunk *c, struct loc *begin,
    struct loc *end)
{
    int x, y;
    int house;
    struct loc grid1, grid2, grid3, grid4;

    loc_init(&grid1, begin->x - 1, begin->y - 1);
    loc_init(&grid2, end->x + 1, begin->y - 1);
    loc_init(&grid3, begin->x - 1, end->y + 1);
    loc_init(&grid4, end->x + 1, end->y + 1);

    /* Check bounds (fully) */
    if (!square_in_bounds_fully(c, &grid1) || !square_in_bounds_fully(c, &grid2) ||
        !square_in_bounds_fully(c, &grid3) || !square_in_bounds_fully(c, &grid4))
    {
        msg(p, "You cannot build house near the location border.");
        return false;
    }

    /* Check north and south */
    for (x = begin->x; x <= end->x; x++)
    {
        loc_init(&grid1, x, begin->y - 1);
        loc_init(&grid2, x, end->y + 1);

        /* Check is this square allowed to have a house
        (terrain where housing not allowed - roads, NPC stores, dungeons etc) */
        if (square_is_no_house(c, &grid1) || square_is_no_house(c, &grid2))
        {
            msg(p, "You cannot build house there.");
            return false;
        }

        house = 0;

        /* Grid 1 : given coordinates return a house to which they belong. */
        house = find_house(p, &grid1, 0);
        if (house >= 0)
        {
            /* Determine if the player owns the house */
            if (!(house_owned_by(p, house)))
            {
                /* Check for house doors */
                if (square_home_iscloseddoor(c, &grid1))
                {
                    msg(p, "You cannot build house near other's players house doors.");
                    return false;
                }
            }
        }

        house = 0;

        /* Grind 2 : given coordinates return a house to which they belong. */
        house = find_house(p, &grid2, 0);
        if (house >= 0)
        {
            /* Determine if the player owns the house */
            if (!(house_owned_by(p, house)))
            {
                /* Check for house doors */
                if (square_home_iscloseddoor(c, &grid2))
                {
                    msg(p, "You cannot build house near other's players house doors.");
                    return false;
                }
            }
        }

    }

    /* Check east and west */
    for (y = begin->y; y <= end->y; y++)
    {
        loc_init(&grid1, begin->x - 1, y);
        loc_init(&grid2, end->x + 1, y);

        /* Check is this square allowed to have a house
        (terrain where housing not allowed - roads, NPC stores, dungeons etc) */
        if (square_is_no_house(c, &grid1) || square_is_no_house(c, &grid2))
        {
            msg(p, "You cannot build house there.");
            return false;
        }

        house = 0;

        /* Grid 1 : given coordinates return a house to which they belong. */
        house = find_house(p, &grid1, 0);
        if (house >= 0)
        {
            /* Determine if the player owns the house */
            if (!(house_owned_by(p, house)))
            {
                /* Check for house doors */
                if (square_home_iscloseddoor(c, &grid1))
                {
                    msg(p, "You cannot build house near other's players house doors.");
                    return false;
                }
            }
        }

        house = 0;

        /* Grind 2 : given coordinates return a house to which they belong. */
        house = find_house(p, &grid2, 0);
        if (house >= 0)
        {
            /* Determine if the player owns the house */
            if (!(house_owned_by(p, house)))
            {
                /* Check for house doors */
                if (square_home_iscloseddoor(c, &grid2))
                {
                    msg(p, "You cannot build house near other's players house doors.");
                    return false;
                }
            }
        }

    }

    return true;
}


/*
 * Determine if the given location is ok to use as part of the foundation
 * of a house.
 *
 * This function called by get_house_foundation() when there is
 * a need to check particular square (tile or grid) with certain x,y coords.
 * This fuctions called a lot of times; each time when we need to check
 * "Could we expand to certain direction?". Each time function checks:
 * 1) is there a foundation stone lying at this square?
 * 2) is this square is a part of a house? if so - is it our house?
 *
 */

static bool is_valid_foundation(struct player *p, struct chunk *c, struct loc *grid)
{
    struct object *obj = square_object(c, grid);

    /* Foundation stones are always valid */
    if (obj)
    {
        if (tval_is_stone(obj) && !obj->next) return true;
        return false;
    }

    /*
     * Perma walls and doors are valid if they are part of a house owned
     * by this player
     */
    if (square_is_new_permhouse(c, grid) || square_home_iscloseddoor(c, grid))
    {
        int house;

        /* Looks like part of a house, which house?
        Given coordinates return a house to which they belong. */
        house = find_house(p, grid, 0);
        if (house >= 0)
        {
            /* Do we own this house? */
            if (house_owned_by(p, house))
            {
                /* Valid, a wall or door in our own house. */
                return true;
            }
        }
    }

    return false;
}


/*  
 * Determine the area for a house foundation.
 *
 * Although an individual house must be rectangular, a foundation
 * can be non-rectangular.  This is because we allow existing walls to
 * form part of our foundation, and therefore allow complex shaped houses
 * to be constructed.
 *                                              ~~~
 * For example this is a legal foundation:   ~~~~~~
 *                                           ~~~~~~
 * In this situation:
 *
 *   #####                               #####
 *   #   #                               #   #
 *   #   #      Forming a final shape:   #   #
 *   #####~~~                            ###+####
 *     ~~~~~~                              #    #
 *     ~~~~~~                              ######
 *
 * This function is also responsible for rejecting illegal shapes and sizes.
 *
 * We start from the player location (who must be stood on a foundation stone)
 * and work our way outwards to find the bounding rectangle of the foundation.
 * Conceptually imagine a box radiating out from the player, we keep extending
 * the box in each dimension for as long as all points on the perimeter are
 * either foundation stones or walls of houses the player owns.
 *
 * CHANGED BOOL TO INT to return area size
 */
static long int get_house_foundation(struct player *p, struct chunk *c, struct loc *grid1,
    struct loc *grid2)
{
    int x, y;
    bool done = false;
    bool n, s, e, w, ne, nw, se, sw;

    /* We must be standing on a house foundation */
    if (!is_valid_foundation(p, c, &p->grid))
    {
        msg(p, "There is no house foundation here.");
        return false;
    }

    /* Start from the players position */
    loc_copy(grid1, &p->grid);
    loc_copy(grid2, &p->grid);

    while (!done)
    {
        struct loc grid;

        n = s = e = w = ne = nw = se = sw = false;

        /* Could we expand north? */
        n = true;
        for (x = grid1->x; x <= grid2->x; x++)
        {
            /* Is this a valid location for part of our house? */
            loc_init(&grid, x, grid1->y - 1);
            if (!is_valid_foundation(p, c, &grid))
            {
                /* Not a valid perimeter */
                n = false;

                break;
            }
        }

        /* Could we expand east? */
        e = true;
        for (y = grid1->y; y <= grid2->y; y++)
        {
            /* Is this a valid location for part of our house? */
            loc_init(&grid, grid2->x + 1, y);
            if (!is_valid_foundation(p, c, &grid))
            {
                /* Not a valid perimeter */
                e = false;

                break;
            }
        }

        /* Could we expand south? */
        s = true;
        for (x = grid1->x; x <= grid2->x; x++)
        {
            /* Is this a valid location for part of our house? */
            loc_init(&grid, x, grid2->y + 1);
            if (!is_valid_foundation(p, c, &grid))
            {
                /* Not a valid perimeter */
                s = false;

                break;
            }
        }

        /* Could we expand west? */
        w = true;
        for (y = grid1->y; y <= grid2->y; y++)
        {
            /* Is this a valid location for part of our house? */
            loc_init(&grid, grid1->x - 1, y);
            if (!is_valid_foundation(p, c, &grid))
            {
                /* Not a valid perimeter */
                w = false;

                break;
            }
        }

        /* Could we expand the corners? */
        loc_init(&grid, grid2->x + 1, grid1->y - 1);
        ne = is_valid_foundation(p, c, &grid);
        loc_init(&grid, grid1->x - 1, grid1->y - 1);
        nw = is_valid_foundation(p, c, &grid);
        loc_init(&grid, grid2->x + 1, grid2->y + 1);
        se = is_valid_foundation(p, c, &grid);
        loc_init(&grid, grid1->x - 1, grid2->y + 1);
        sw = is_valid_foundation(p, c, &grid);

        /*
         * Only permit expansion in a way that maintains a rectangle, we don't
         * want to create fancy polygons.
         */
        if (n) n = (!e && !w) || (e && ne) || (w && nw);
        if (e) e = (!n && !s) || (n && ne) || (s && se);
        if (s) s = (!e && !w) || (e && se) || (w && sw);
        if (w) w = (!n && !s) || (n && nw) || (s && sw);

        /* Actually expand the boundary */
        if (n) grid1->y--;
        if (s) grid2->y++;
        if (w) grid1->x--;
        if (e) grid2->x++;

        /* Stop if we couldn't expand */
        done = !(n || s || w || e);
    }

// ATTENTION!
// location coordinates starts on top left corner of the _map_! so Y coord is vice versa
// see pics at https://tangaria.com/dev-notes/

    /* Paranoia: checks is foundation from one corner to another
    got at least 1 tile in between. So it's always a square;
    not possible to make it unusual form */
    x = grid2->x - grid1->x - 1;
    y = grid2->y - grid1->y - 1;
    if ((x <= 0) || (y <= 0))
    {
        msg(p, "The foundation should have positive dimensions!");
        return false;
    }

    /* House foundation size check.
    Smallest house (1 floor tile) got 2 in this calculations */
    if ((x + y) < 2) // 3->2 TANGARIA
    {
        msg(p, "The foundation is too small!");
        return false;
    }

    /* Calculare area size */
    // ! This returns only floor tiles! Not real house square with walls.
    // Note: we store real house square with walls later on for houses,
    // this 'area size' is only for pricing and house scrolls checks
    return (x * y);
}


/*
 * Create a new house.
 * The creating player owns the house.
 */
bool create_house(struct player *p, int house_variant)
{
    int house;
    struct house_type h_local;
    struct chunk *c = chunk_get(&p->wpos);
    struct loc begin, end;
    struct loc_iterator iter;
    int area_size;
    long int price;
    int i; // to find house deed for <10k houses
    char wall_type = '\0';  // second part of floor name (type) for rng walls
    int wall_id = 0;  // specific wall (when we have several different walls in one row)
    const char *house_type_desc; // house name for msg

    if (p->lev < 8)
    {
        msg(p, "You must be level 8 to build a house.");
        return false;
    }
    
    if (p->account_score < 10)
    {
        msg(p, "You need at least 10 account points to build a house (press Ctrl+r to check it).");
        return false;
    }

#ifndef TEST_MODE
    /* The DM cannot create houses! */
    if (p->dm_flags & DM_HOUSE_CONTROL)
    {
        msg(p, "You cannot create or extend houses.");
        return false;
    }
#endif

    /* Restricted by choice */
    if ((cfg_limited_stores == 3) || OPT(p, birth_no_stores))
    {
        msg(p, "You cannot create or extend houses.");
        return false;
    }

    /* Can't have more than 2 houses */
    if (houses_owned(p) >= 2)
    {
        msg(p, "You cannot have more than 2 houses.");
        return false;
    }

//  /* Houses can only be created in the wilderness */
//    if (!in_wild(&p->wpos))
//   {
//        msg(p, "This location is not suited for a house.");
//        return false;
//   }

    /* Determine the area of the house foundation AND calculate price */
    area_size = get_house_foundation(p, c, &begin, &end);

    if (area_size <= 0)
    {
        msg(p, "Non-valid house foundation size.");
        return false;
    }

    // Compare planned area_size and house variant (depends on type of house)
    if (house_variant == 1) // cabin
    {
        if      (area_size == 1 && p->account_score >= 10) ;
        else if (area_size == 2 && p->account_score >= 25) ;
        else if (area_size == 3 && p->account_score >= 50) ;
        else if (area_size >= 1 && area_size <= 3)
        {
            msg(p, "You need more account points to build house of such size.");
            return false;
        }
        else
        {
            msg(p, "This house deed cannot be used for a house of this size. Cabin deeds are for houses of 1-3 tiles.");
            return false;
        }
    }
    else if (house_variant == 2) // small house
    {
        if      (area_size == 4 && p->account_score >= 100) ;
        else if (area_size == 5 && p->account_score >= 150) ;
        else if (area_size == 6 && p->account_score >= 225) ;
        else if (area_size >= 4 && area_size <= 6)
        {
            msg(p, "You need more account points to build house of such size.");
            return false;
        }
        else
        {
            msg(p, "This house deed cannot be used for a house of this size. Small house deeds are for houses of 4-6 tiles.");
            return false;
        }
    }
    else if (house_variant == 3) // medium house
    {
        if      (area_size == 7 && p->account_score >= 300) ;
        else if (area_size == 8 && p->account_score >= 400) ;
        else if (area_size == 9 && p->account_score >= 500) ;
        else if (area_size >= 7 && area_size <= 9)
        {
            msg(p, "You need more account points to build house of such size.");
            return false;
        }
        else
        {
            msg(p, "This house deed cannot be used for a house of this size. Medium house deeds are for houses of 7-9 tiles.");
            return false;
        }
    }
    else if (house_variant == 4) // big house
    {
        if      (area_size <= 15 && p->account_score >= 650) ;
        else if (area_size <= 20 && p->account_score >= 800) ;
        else if (area_size <= 25 && p->account_score >= 1000) ;
        else if (area_size <= 25)
        {
            msg(p, "You need more account points to build house of such size.");
            return false;
        }
        else
        {
            msg(p, "This house deed cannot be used for a house of this size. Big house deeds are for houses of up to 25 tiles.");
            return false;
        }
    }
    else if (house_variant == 5) // villa
    {
        if      (area_size <= 32 && p->account_score >= 1250) ;
        else if (area_size <= 44 && p->account_score >= 1500) ;
        else if (area_size <= 64 && p->account_score >= 2000) ;
        else if (area_size <= 64)
        {
            msg(p, "You need more account points to build house of such size.");
            return false;
        }
        else
        {
            msg(p, "This house deed cannot be used for a house of this size. Villa deeds are for houses of up to 64 tiles.");
            return false;
        }
    }
    else if (house_variant == 6) // estate
    {
        if (area_size <= 100 && p->account_score >= 2500) ;
        else if (area_size <= 100)
        {
            msg(p, "You need at least 2500 account points to build such house.");
            return false;
        }
        else
        {
            msg(p, "This house deed cannot be used for a house of this size. Estate deeds are for houses of up to 100 tiles.");
            return false;
        }
    }
    else if (house_variant == 7) // tower
    {
        if (area_size <= 144 && p->account_score >= 3500) ;
        else if (area_size <= 144)
        {
            msg(p, "You need at least 3500 account points to build such house.");
            return false;
        }
        else
        {
            msg(p, "This house deed cannot be used for a house of this size. Tower deeds are for houses of up to 144 tiles.");
            return false;
        }
    }
    else if (house_variant == 8) // keep
    {
        if (area_size <= 256 && p->account_score >= 5000) ;
        else if (area_size <= 256)
        {
            msg(p, "You need at least 5000 account points to build such house.");
            return false;
        }
        else
        {
            msg(p, "This house deed cannot be used for a house of this size. Keep deeds are for houses of up to 256 tiles.");
            return false;
        }
    }
    else if (house_variant == 9) // castle
    {
        if (area_size <= 324 && p->account_score >= 10000) ;
        else if (area_size <= 324)
        {
            msg(p, "You need at least 10000 account points to build such house.");
            return false;
        }
        else
        {
            msg(p, "This house deed cannot be used for a house of this size. Castle deeds are for houses of up to 324 tiles.");
            return false;
        }
    }

    /* Calculate price.
    More houses you have - more expensive will be next one */
    price = house_price(area_size, false);

    /* Check for deeds of property */
    // There will be yet another check of such kind - when we pay the gold,
    // but for now it's commented out. We use Deed only for discount atm.
    // Look in backpack for Deed of Property
    for (i = 0; i < z_info->pack_size; i++)
    {
        struct object *obj = p->upkeep->inven[i];

        if (!obj) continue;

        if (tval_is_deed(obj))
        {
            price = (price * 9) / 10;
            break;
        }
    }

    // experienced adventurers can build small (1-2-3 tile) house for free (only deed cost)
    if (price < 13000 && houses_owned(p) < 1)
    {
        if ((p->account_score >= 70 && area_size == 3) ||
            (p->account_score >= 50 && area_size == 2) ||
            (p->account_score >= 30 && area_size == 1))
                price = 1;
    }

    // check for enough funds:
    if (price > p->au)
    {
        msg(p, "You do not have enough money to pay the price of %ld gold.", price);
        return false;
    }

    /* Is the location allowed? */
    /* XXX We should check if too near other houses, roads, level edges, etc */
    if (!check_around_foundation(p, c, &begin, &end)) return false;

    /* Get an empty house slot */
    house = house_add(true);

    /* Check maximum number of houses */
    if (house == -1)
    {
        msg(p, "The maximum number of houses has been reached.");
        return false;
    }

    /* NOT USED for now...
    // it means that h_local.free also not used atm.
    //////
    // Check for deeds of property
    // if you don't have any house, building 1st small one will be free
    if ((price < 10000) && ((houses_owned(p)) < 1))
    {
        for (i = 0; i < z_info->pack_size; i++)
        {
            struct object *obj = p->upkeep->inven[i];

            if (!obj) continue;

            if (tval_is_deed(obj))
            {
                use_object(p, obj, 1, true);
                h_local.free = 1;
            }
        }
        if (h_local.free != 1) p->au -= price; // didn't use house deed used - pay
    }
    else
        p->au -= price; // big house: take some of the player's money
    //////
    */

    // pay for house
    p->au -= price;

    /* Redraw */
    p->upkeep->redraw |= (PR_GOLD);

    /* Setup house info */
    // if in local.grid will be PWMA +1/-1 stuff -- walls won't be
    // counted (so house will be marked only on grids with floor).
    // So there +1/-1 are removed.
    loc_init(&h_local.grid_1, begin.x, begin.y);
    loc_init(&h_local.grid_2, end.x, end.y);
    loc_init(&h_local.door, 0, 0);
    // copy coordinates of world location
    memcpy(&h_local.wpos, &p->wpos, sizeof(struct worldpos));
    h_local.price = price;
    set_house_owner(p, &h_local);
    h_local.state = HOUSE_CUSTOM;

    /* Add a house to our houses list */
    house_set(house, &h_local);

    loc_iterator_first(&iter, &begin, &end);

    /* Wall type for building rng walls */
    if      (one_in_(9)) wall_type = 'a';  // B7 B8 wood
    else if (one_in_(9)) wall_type = 'b';  // B9 BA small black
    else if (one_in_(9)) wall_type = 'c';  // BB BC big white
    else if (one_in_(9)) wall_type = 'd';  // BD BE big black
    else if (one_in_(9)) wall_type = 'e';  // BF C0 small white
    else if (one_in_(9)) wall_type = 'f';  // 96 98
    else if (one_in_(9)) wall_type = 'g';  // A3 AA
    else if (one_in_(9)) wall_type = 'h';  // DC E1
    else if (one_in_(9)) wall_type = 'i';  // E2 E3
    else wall_type = 'b';

    /* Generate special wall id: starting from 1! */
    wall_id = randint1(12);

    /* Render into the terrain */
    do
    {
        /* Delete any object */
        square_excise_pile(c, &iter.cur);

        /* Build a wall, but don't destroy any existing door */
        if (!square_home_iscloseddoor(c, &iter.cur))
            square_build_new_permhouse(c, &iter.cur, wall_type, wall_id);
    }
    while (loc_iterator_next(&iter));

    begin.x++;
    begin.y++;
    loc_iterator_first(&iter, &begin, &end);

    do
    {
        /* Fill with safe floor */
        square_add_new_safe(c, &iter.cur);

        /* Declare this to be a room */
        sqinfo_on(square(c, &iter.cur)->info, SQUARE_VAULT);
        sqinfo_on(square(c, &iter.cur)->info, SQUARE_ROOM);
    }
    while (loc_iterator_next_strict(&iter));

    // get house name
    house_type_desc = get_house_type_desc(area_size);
    // msg
    msg(p, "# %s built a %s (%d tiles) at %s.",
        p->name, house_type_desc, area_size,
        (p->wpos.grid.x == -6 && p->wpos.grid.y == 0) ? "Deeptown" :
        (p->wpos.grid.x == 0 && p->wpos.grid.y == 1) ? "Farfest" :
        (p->wpos.grid.x == -1 && p->wpos.grid.y == 1) ? "Suburbs" :
        "Unknown Location");

    msg(p, "To create a door - 'T'unnel the wall in desirable place.");

    return true;
}


/*
 * PWMAngband house creation code
 *
 * A simplified version of the MAngband house creation code:
 *   - only rectangular houses allowed with automatic (random) door placement
 *   - any foundation near an existing owned house extends that house automatically
 *   - house creation/extension forbidden near other houses
 *   - price set (or adjusted) according to house dimensions
 *   - player pays local tax equal to this price minus the amount already paid
 */


/*
 * Determine if the given location contains a house foundation stone.
 */
static bool is_foundation(struct player *p, struct chunk *c, struct loc *grid)
{
    struct object *obj;

    /* Paranoia */
    if (!square_in_bounds(c, grid)) return false;

    obj = square_object(c, grid);

    if (obj && tval_is_stone(obj) && !obj->next) return true;
    return false;
}


/*
 * Determine the area for a house foundation.
 */
static bool get_foundation_area(struct player *p, struct chunk *c, struct loc *begin,
    struct loc *end)
{
    int x, y, x1, x2, y1, y2, d;
    bool done = true;
    bool n, s, e, w, ne, nw, se, sw;

    /* We must NOT be standing on a house foundation stone */
    if (is_foundation(p, c, &p->grid))
    {
        msg(p, "You must stand outside the foundation perimeter.");
        return false;
    }

    /* Find a house foundation stone next to the player */
    for (d = 0; d < 8; d++)
    {
        struct loc grid;

        loc_sum(&grid, &p->grid, &ddgrid_ddd[d]);

        /* Oops */
        if (!square_in_bounds(c, &grid)) continue;

        if (is_foundation(p, c, &grid))
        {
            x1 = grid.x;
            x2 = grid.x;
            y1 = grid.y;
            y2 = grid.y;
            done = false;
            break;
        }
    }

    /* Didn't find any foundation stone next to the player */
    if (done)
    {
        msg(p, "You must stand next to the foundation perimeter.");
        return false;
    }

    do
    {
        struct loc grid;

        /* Could we expand north? */
        n = true;
        for (x = x1; x <= x2; x++)
        {
            loc_init(&grid, x, y1 - 1);

            /* Is this a valid location for part of our house? */
            if (!is_foundation(p, c, &grid))
            {
                /* Not a valid perimeter */
                n = false;

                break;
            }
        }

        /* Could we expand east? */
        e = true;
        for (y = y1; y <= y2; y++)
        {
            loc_init(&grid, x2 + 1, y);

            /* Is this a valid location for part of our house? */
            if (!is_foundation(p, c, &grid))
            {
                /* Not a valid perimeter */
                e = false;

                break;
            }
        }

        /* Could we expand south? */
        s = true;
        for (x = x1; x <= x2; x++)
        {
            loc_init(&grid, x, y2 + 1);

            /* Is this a valid location for part of our house? */
            if (!is_foundation(p, c, &grid))
            {
                /* Not a valid perimeter */
                s = false;

                break;
            }
        }

        /* Could we expand west? */
        w = true;
        for (y = y1; y <= y2; y++)
        {
            loc_init(&grid, x1 - 1, y);

            /* Is this a valid location for part of our house? */
            if (!is_foundation(p, c, &grid))
            {
                /* Not a valid perimeter */
                w = false;

                break;
            }
        }

        /* Could we expand the corners? */
        loc_init(&grid, x2 + 1, y1 - 1);
        ne = is_foundation(p, c, &grid);
        loc_init(&grid, x1 - 1, y1 - 1);
        nw = is_foundation(p, c, &grid);
        loc_init(&grid, x2 + 1, y2 + 1);
        se = is_foundation(p, c, &grid);
        loc_init(&grid, x1 - 1, y2 + 1);
        sw = is_foundation(p, c, &grid);

        /*
         * Only permit expansion in a way that maintains a rectangle, we don't
         * want to create fancy polygons.
         */
        if (n) n = (!e && !w) || (e && ne) || (w && nw);
        if (e) e = (!n && !s) || (n && ne) || (s && se);
        if (s) s = (!e && !w) || (e && se) || (w && sw);
        if (w) w = (!n && !s) || (n && nw) || (s && sw);

        /* Actually expand the boundary */
        if (n) y1--;
        if (s) y2++;
        if (w) x1--;
        if (e) x2++;

        /* Stop if we couldn't expand */
        done = !(n || s || w || e);
    }
    while (!done);

    /* Paranoia */
    x = x2 - x1 - 1;
    y = y2 - y1 - 1;
    if ((x <= 0) || (y <= 0))
    {
        msg(p, "The foundation should have positive dimensions!");
        return false;
    }

    /* No 1x1 house foundation */
    if ((x + y) < 2)  ////////////// 3 -> TANGARIA
    {
        msg(p, "The foundation is too small!");
        return false;
    }

    /* Return the area */
    loc_init(begin, x1, y1);
    loc_init(end, x2, y2);

    return true;
}


/*
 * Determine if the area for a house foundation is allowed.
 */
static bool allowed_foundation_area(struct player *p, struct chunk *c, struct loc *begin,
    struct loc *end)
{
    int x, y;
    struct loc grid1, grid2, grid3, grid4;

    loc_init(&grid1, begin->x - 1, begin->y - 1);
    loc_init(&grid2, end->x + 1, begin->y - 1);
    loc_init(&grid3, begin->x - 1, end->y + 1);
    loc_init(&grid4, end->x + 1, end->y + 1);

    /* Check bounds (fully) */
    if (!square_in_bounds_fully(c, &grid1) || !square_in_bounds_fully(c, &grid2) ||
        !square_in_bounds_fully(c, &grid3) || !square_in_bounds_fully(c, &grid4))
    {
        msg(p, "You cannot create or extend houses near the level border.");
        return false;
    }

    /* Check north and south */
    for (x = begin->x; x <= end->x; x++)
    {
        loc_init(&grid1, x, begin->y - 1);
        loc_init(&grid2, x, end->y + 1);

        /* Check for house doors */
        if (square_home_iscloseddoor(c, &grid1) || square_home_iscloseddoor(c, &grid2))
        {
            msg(p, "You cannot create or extend houses near other house doors.");
            return false;
        }
    }

    /* Check east and west */
    for (y = begin->y; y <= end->y; y++)
    {
        loc_init(&grid1, begin->x - 1, y);
        loc_init(&grid2, end->x + 1, y);

        /* Check for house doors */
        if (square_home_iscloseddoor(c, &grid1) || square_home_iscloseddoor(c, &grid2))
        {
            msg(p, "You cannot create or extend houses near other house doors.");
            return false;
        }
    }

    return true;
}


/*
 * Create or extend a house.
 */
// TURNED OFF TEMPORARY - use create_house() instead (though this PWMA version might be
// better as it gives possibility to expand houses.. need to check)
bool build_house(struct player *p)
{
    int x1, x2, y1, y2, house, area, price, tax;
    struct house_type *h_ptr = NULL;
    struct chunk *c = chunk_get(&p->wpos);
    struct loc begin, end;
    struct loc_iterator iter;

#ifndef TEST_MODE
    /* The DM cannot create or extend houses! */
    if (p->dm_flags & DM_HOUSE_CONTROL)
    {
        msg(p, "You cannot create or extend houses.");
        return false;
    }
#endif

    /* Restricted by choice */
    if ((cfg_limited_stores == 3) || OPT(p, birth_no_stores))
    {
        msg(p, "You cannot create or extend houses.");
        return false;
    }

//    /* Houses can only be created in the wilderness */
//    if (!in_wild(&p->wpos))
//    {
//        msg(p, "This location is not suited for a house.");
//        return false;
//    }

/// Tangaria comment out:
    /* PWMAngband: no house expansion in immediate suburbs 
    if (town_suburb(&p->wpos))
    {
        msg(p, "This location is not suited for a house.");
        return false;
    }*/

    /* Determine the area of the house foundation */
    if (!get_foundation_area(p, c, &begin, &end)) return false;

    /* Is the location allowed? */
    if (!allowed_foundation_area(p, c, &begin, &end)) return false;

    /* Is it near a house we own? */
    house = house_near(p, &begin, &end);
    switch (house)
    {
        /* Invalid dimensions */
        case -3:
        {
            msg(p, "You need a foundation with proper dimensions to extend that house.");
            return false;
        }

        /* House not owned */
        case -2:
        {
            msg(p, "You cannot create or extend houses near houses you don't own.");
            return false;
        }

        /* No house near: we can create one */
        case -1: break;

        /* A house has been found: we can extend it */
        default: h_ptr = house_get(house);
    }

    x1 = begin.x;
    x2 = end.x;
    y1 = begin.y;
    y2 = end.y;

    /* Local tax: we already paid this amount (cost of foundation) */
    tax = 0 - 1000 * (x2 - x1 + 1) * (y2 - y1 + 1);

    /* New dimensions */
    if (h_ptr)
    {
        x1 = MIN(x1, h_ptr->grid_1.x - 1);
        x2 = MAX(x2, h_ptr->grid_2.x + 1);
        y1 = MIN(y1, h_ptr->grid_1.y - 1);
        y2 = MAX(y2, h_ptr->grid_2.y + 1);
    }

    /* Remember price */
    area = (x2 - x1 - 1) * (y2 - y1 - 1);
    price = house_price(area, false);

    /* Local tax: price minus amount already paid */
    tax += price;
    if (h_ptr) tax -= h_ptr->price;
    if (tax < 0) tax = 0;

    /* Check for enough funds */
    if (tax > p->au)
    {
        /* Not enough money, message */
        msg(p, "You do not have enough money to pay the local tax of %ld gold.", tax);
        return false;
    }

    /* Extend an existing house */
    if (h_ptr)
    {
        /* Check maximum number of houses */
        if (!house_extend())
        {
            msg(p, "The maximum number of houses has been reached.");
            return false;
        }
    }

    /* Create a new house */
    else
    {
        /* Get an empty house slot */
        house = house_add(true);

        /* Check maximum number of houses */
        if (house == -1)
        {
            msg(p, "The maximum number of houses has been reached.");
            return false;
        }
    }

    /* Take some of the player's money */
    p->au -= tax;

    /* Redraw */
    p->upkeep->redraw |= (PR_GOLD);

    loc_init(&begin, x1, y1);
    loc_init(&end, x2, y2);
    loc_iterator_first(&iter, &begin, &end);

    /* Build a rectangular building */
    do
    {
        /* Delete any object */
        square_excise_pile(c, &iter.cur);

        /* Build a wall, but don't destroy any existing door */
        if (!square_home_iscloseddoor(c, &iter.cur))
            square_build_permhouse(c, &iter.cur);
    }
    while (loc_iterator_next(&iter));

    loc_init(&begin, x1 + 1, y1 + 1);
    loc_iterator_first(&iter, &begin, &end);

    /* Make it hollow */
    do
    {
        /* Fill with safe floor */
        square_add_safe(c, &iter.cur);

        /* Declare this to be a room */
        sqinfo_on(square(c, &iter.cur)->info, SQUARE_VAULT);
        sqinfo_on(square(c, &iter.cur)->info, SQUARE_ROOM);
    }
    while (loc_iterator_next_strict(&iter));

    /* Finish house creation */
    if (!h_ptr)
    {
        struct house_type h_local;
        int tmp;
        struct loc door;

        /* Pick a door direction (S,N,E,W) */
        tmp = randint0(4);

        /* Extract a "door location" */
        switch (tmp)
        {
            /* Bottom side */
            case DIR_SOUTH:
            {
                door.y = y2;
                door.x = rand_range(x1, x2);
                break;
            }

            /* Top side */
            case DIR_NORTH:
            {
                door.y = y1;
                door.x = rand_range(x1, x2);
                break;
            }

            /* Right side */
            case DIR_EAST:
            {
                door.y = rand_range(y1, y2);
                door.x = x2;
                break;
            }

            /* Left side */
            default:
            {
                door.y = rand_range(y1, y2);
                door.x = x1;
                break;
            }
        }

        /* Add the door */
        square_colorize_door(c, &door, 0);

        /* Setup house info */
        loc_init(&h_local.grid_1, x1 + 1, y1 + 1);
        loc_init(&h_local.grid_2, x2 - 1, y2 - 1);
        loc_copy(&h_local.door, &door);
        memcpy(&h_local.wpos, &p->wpos, sizeof(struct worldpos));
        h_local.price = price;
        set_house_owner(p, &h_local);
        h_local.state = HOUSE_CUSTOM;

        /* Add a house to our houses list */
        house_set(house, &h_local);

        /* Update the visuals */
        update_visuals(&p->wpos);

        return true;
    }

    /* Adjust some house info */
    loc_init(&h_ptr->grid_1, x1 + 1, y1 + 1);
    loc_init(&h_ptr->grid_2, x2 - 1, y2 - 1);
    h_ptr->price = price;
    h_ptr->state = HOUSE_EXTENDED;

    /* Update the visuals */
    update_visuals(&p->wpos);

    return true;
}


/*
 * Display current time (of the day).
 */
void display_time(struct player *p)
{
    int day = 5 * z_info->day_length;
    int hour = 1 + (turn.turn % day) * 12 / day;
    const char *suffix = "th";

    if (hour == 1) suffix = "st";
    else if (hour == 2) suffix = "nd";
    else if (hour == 3) suffix = "rd";

    msg(p, "It is the %d%s hour of the %s. Acc.points: %d. Lives: %d.", hour, suffix, (is_daytime()? "day": "night"), p->account_score, p->lives);
}
