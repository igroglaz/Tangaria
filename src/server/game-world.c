/*
 * File: game-world.c
 * Purpose: Game core management of the game world
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


bool server_generated;      /* The server exists */
bool server_state_loaded;   /* The server state was loaded from a savefile */
uint32_t seed_flavor;       /* Hack -- consistent object colors */
hturn turn;                 /* Current game turn */


/*
 * This table allows quick conversion from "speed" to "energy"
 * The basic function WAS ((S>=110) ? (S-110) : (100 / (120-S)))
 * Note that table access is *much* quicker than computation.
 *
 * Note that the table has been changed at high speeds.  From
 * "Slow (-40)" to "Fast (+30)" is pretty much unchanged, but
 * at speeds above "Fast (+30)", one approaches an asymptotic
 * effective limit of 50 energy per turn.  This means that it
 * is relatively easy to reach "Fast (+30)" and get about 40
 * energy per turn, but then speed becomes very "expensive",
 * and you must get all the way to "Fast (+50)" to reach the
 * point of getting 45 energy per turn.  After that point,
 * further increases in speed are more or less pointless,
 * except to balance out heavy inventory.
 *
 * Note that currently the fastest monster is "Fast (+30)".
 */
static const uint8_t extract_energy[200] =
{
    /* Slow */     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
    /* Slow */     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
    /* Slow */     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
    /* Slow */     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
    /* Slow */     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
    /* Slow */     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
    /* S-50 */     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
    /* S-40 */     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
    /* S-30 */     2,  2,  2,  2,  2,  2,  2,  3,  3,  3,
    /* S-20 */     3,  3,  3,  3,  3,  4,  4,  4,  4,  4,
    /* S-10 */     5,  5,  5,  5,  6,  6,  7,  7,  8,  9,
    /* Norm */    10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
    /* F+10 */    20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
    /* F+20 */    30, 31, 32, 33, 34, 35, 36, 36, 37, 37,
    /* F+30 */    38, 38, 39, 39, 40, 40, 40, 41, 41, 41,
    /* F+40 */    42, 42, 42, 43, 43, 43, 44, 44, 44, 44,
    /* F+50 */    45, 45, 45, 45, 45, 46, 46, 46, 46, 46,
    /* F+60 */    47, 47, 47, 47, 47, 48, 48, 48, 48, 48,
    /* F+70 */    49, 49, 49, 49, 49, 49, 49, 49, 49, 49,
    /* Fast */    49, 49, 49, 49, 49, 49, 49, 49, 49, 49
};


/*
 * Say whether a turn is during day or not
// Note: number of turns from dawn to dawn world:day-length:10000
 */
bool is_daytime_turn(hturn *ht_ptr)
{
    return ((ht_ptr->turn % (10L * z_info->day_length)) < (uint32_t)((10L * z_info->day_length) / 2));
}


/*
 * Say whether it's daytime or not
 */
bool is_daytime(void)
{
    return is_daytime_turn(&turn);
}


void dusk_or_dawn(struct player *p, struct chunk *c, bool dawn)
{
    /* Day breaks */
    if (dawn) msg(p, "The sun has risen.");

    /* Night falls */
    else msg(p, "The sun has fallen.");

    /* Clear the flags for each cave grid */
    // only at daylight surface and mid-game+
    // as we don't wanna wipe grind 'info' and 'feat' in dungeon cause
    // of OPEN_SKY wiz_lit() (or it will erase all memorized squares).
    // but at 25+ lvl it suits well - to make them more dangerous
    if (p->wpos.depth == 0 || p->wpos.depth > 25)
        player_cave_clear(p, false);

    /* Illuminate */
    cave_illuminate(p, c, dawn);

    // also we will check for old and unowned custom houses
    wipe_old_houses(&p->wpos);
}


/*
 * The amount of energy gained in a turn by a player or monster
 */
int turn_energy(int speed)
{
    int n_energy = N_ELEMENTS(extract_energy);

    return extract_energy[MIN(speed, n_energy - 1)] * z_info->move_energy / 100;
}


/*
 * The amount of energy gained in a frame by a player or monster
 *
 * PWMAngband: the energy is multiplied by 100 to ensure that slow players will always gain at
 * least 1 energy (energy = energy * bubble speed factor / 100) when locked in a slow time bubble.
 */
int frame_energy(int speed)
{
    int n_energy = N_ELEMENTS(extract_energy);

    return extract_energy[MIN(speed, n_energy - 1)] * 100;
}


/*
 * Let the player know when an object is recharged.
 * Also inform player when first item of a stack has recharged.
 */
static void recharged_notice(struct player *p, const struct object *obj, bool all)
{
    char o_name[NORMAL_WID];

    if (!OPT(p, notify_recharge)) return;

    /* Describe (briefly) */
    object_desc(p, o_name, sizeof(o_name), obj, ODESC_BASE);

    /* Disturb the player */
    disturb(p, 0);

    /* Notify the player */
    if (obj->number > 1)
    {
        if (all) msg(p, "Your %s have recharged.", o_name);
        else msg(p, "One of your %s has recharged.", o_name);
    }

    /* Artifacts */
    else if (obj->artifact)
        msg(p, "The %s has recharged.", o_name);

    /* Single, non-artifact items */
    else msg(p, "Your %s has recharged.", o_name);
}


/*
 * Recharge activatable objects in the player's equipment
 * and rods in the inventory. Decompose carried corpses slowly.
 */
static void recharge_objects(struct player *p)
{
    bool discharged_stack;
    struct object *obj = p->gear, *next;

    /* Recharge carried gear */
    while (obj)
    {
        next = obj->next;

        /* Recharge equipment */
        if (object_is_equipped(p->body, obj))
        {
            /* Recharge activatable objects */
            if (recharge_timeout(obj))
            {
                /* Message if an item recharged */
                recharged_notice(p, obj, true);

                /* Redraw */
                set_redraw_equip(p, obj);
            }
        }

        /* Recharge the inventory */
        /* Check if we are shopping (fixes stacking exploits) */
        else if (!in_store(p))
        {
            discharged_stack = (number_charging(obj) == obj->number)? true: false;

            /* Recharge rods, and update if any rods are recharged */
            if (tval_can_have_timeout(obj) && recharge_timeout(obj))
            {
                /* Entire stack is recharged */
                if (!obj->timeout) recharged_notice(p, obj, true);

                /* Previously exhausted stack has acquired a charge */
                else if (discharged_stack) recharged_notice(p, obj, false);

                /* Combine pack */
                p->upkeep->notice |= (PN_COMBINE);

                /* Redraw */
                set_redraw_inven(p, obj);
            }

            /* Handle corpse decay */
            if (tval_is_corpse(obj))
            {
                char o_name[NORMAL_WID];
                object_desc(p, o_name, sizeof(o_name), obj, ODESC_BASE);

                /* Corpses slowly decompose */
                obj->decay--;

                /* Notice changes */
                if (obj->decay == obj->timeout / 5)
                {
                    /* Rotten corpse */
                    if (obj->number > 1)
                        msg(p, "Your %s slowly rot away.", o_name);
                    else
                        msg(p, "Your %s slowly rots away.", o_name);

                    /* Combine pack */
                    p->upkeep->notice |= (PN_COMBINE);

                    /* Redraw */
                    set_redraw_inven(p, obj);
                }
                else if (!obj->decay)
                {
                    /* No more corpse... */
                    if (obj->number > 1)
                        msg(p, "Your %s rot away.", o_name);
                    else
                        msg(p, "Your %s rots away.", o_name);
                    gear_excise_object(p, obj);
                    object_delete(&obj);

                    /* Combine pack */
                    p->upkeep->notice |= (PN_COMBINE);

                    /* Redraw */
                    set_redraw_inven(p, NULL);
                }
            }
        }

        obj = next;
    }
}


/*
 * Play an ambient sound dependent on dungeon level, and day or night in towns
 */
// later: add regular sound events which played more often than ambient sounds below
static void play_ambient_sound(struct player *p)
{
    if (p->wpos.depth == 0)
    {
        if (in_town(&p->wpos))
        {
            if ((p->wpos.grid.x ==  0 && p->wpos.grid.y ==  1) && one_in_(3))
            {
                if (one_in_(6))
                    sound(p, MSG_TOWN_RARE);
                else
                    sound(p, MSG_TOWN);
            }
            else if ((p->wpos.grid.x ==  0 && p->wpos.grid.y ==  0) ||
                (p->wpos.grid.x == -1 && p->wpos.grid.y ==  1) ||
                (p->wpos.grid.x ==  0 && p->wpos.grid.y == -1) ||
                (p->wpos.grid.x ==  0 && p->wpos.grid.y == -2) ||
                (p->wpos.grid.x ==  0 && p->wpos.grid.y == -3) ||
                (p->wpos.grid.x ==  0 && p->wpos.grid.y == -4)) // free spot. add there dungeon..
                        sound(p, MSG_WILD_WOOD);
            else if((p->wpos.grid.x ==  1 && p->wpos.grid.y == 0) ||
                    (p->wpos.grid.x ==  1 && p->wpos.grid.y == 2) ||
                    (p->wpos.grid.x ==  2 && p->wpos.grid.y == 1) ||
                    (p->wpos.grid.x ==  2 && p->wpos.grid.y == 2))
                    {
                        if (one_in_(5)) sound(p, MSG_WILD_GRASS); // bird of prey
                        else if (one_in_(3)) sound(p, MSG_WILD_MOUNTAIN); // wolves
                    }
            else if((p->wpos.grid.x == -1 && p->wpos.grid.y == 0) ||
                    (p->wpos.grid.x == -1 && p->wpos.grid.y == -1))
                        sound(p, MSG_WILD_SWAMP);
            else if((p->wpos.grid.x ==  0 && p->wpos.grid.y == 2) || 
                    (p->wpos.grid.x == -1 && p->wpos.grid.y == 2) ||
                    (p->wpos.grid.x ==  1 && p->wpos.grid.y == 3) ||
                    (p->wpos.grid.x == -2 && p->wpos.grid.y == 2) ||
                    (p->wpos.grid.x == -2 && p->wpos.grid.y == 1) ||
                    (p->wpos.grid.x == -2 && p->wpos.grid.y == 0) ||
                    (p->wpos.grid.x == -2 && p->wpos.grid.y == -1)||
                    (p->wpos.grid.x ==  0 && p->wpos.grid.y == -5)||
                    (p->wpos.grid.x ==  1 && p->wpos.grid.y == -5))
                    {
                        if (one_in_(5)) sound(p, MSG_WILD_SHORE); // seagulls. !together with waves!
                        if (one_in_(3)) sound(p, MSG_WILD_WAVES); // 1 minute sea
                        else sound(p, MSG_WILD_DEEPWATER); // short sea
                    }
            else if (p->wpos.grid.x == -2 && p->wpos.grid.y == 2 && one_in_(3))
                        sound(p, MSG_WILD_DESERT);
            else if (p->wpos.grid.x == -1 && p->wpos.grid.y == -2 && one_in_(2))
                        sound(p, MSG_WILD_HILL);
            else if (p->wpos.grid.x == 1 && p->wpos.grid.y == -1)
                        sound(p, MSG_WILD_WASTE);
            // WILD_DEEPWATER already there as T surrounded by it.. but some locs need it
            else if((p->wpos.grid.x ==  0 && p->wpos.grid.y == 3) || 
                    (p->wpos.grid.x == -1 && p->wpos.grid.y == 3) ||
                    (p->wpos.grid.x == -2 && p->wpos.grid.y == 3) ||
                    (p->wpos.grid.x == -3 && p->wpos.grid.y == 0) ||
                    (p->wpos.grid.x == -5 && p->wpos.grid.y == 0) ||
                    (p->wpos.grid.x ==  0 && p->wpos.grid.y == 5) ||
                    (p->wpos.grid.x ==  2 && p->wpos.grid.y == 3))
                        sound(p, MSG_WILD_DEEPWATER);
            else if (is_daytime())
                sound(p, MSG_AMBIENT_DAY);
            else
                sound(p, MSG_AMBIENT_NITE);

            //// Weather
            if (p->wpos.grid.x == 0 && p->wpos.grid.y == 4) // Helcaraxe
            {
                // Snow
                Send_weather(p, 2, randint1(4), randint1(3));
                sound(p, MSG_WILD_GLACIER);

                if (one_in_(5))
                {
                    // Stop weather
                    Send_weather(p, 255, 0, 0);
                }
            }
            // all other locations
            else if (p->store_num == -1)
            {
                if ((p->weather_type == 0) || (p->weather_type == 255))
                {
                    if (one_in_(5))
                    {
                        // Rain
                        Send_weather(p, 1, randint1(4), randint1(3));
                        sound(p, MSG_WILD_RAIN);
                    }
                }
                else
                {
                    if (one_in_(3))
                    {
                        // empty sound to break sound loop .ogg.0
                        sound(p, MSG_SILENT0);
                        // Stop weather
                        Send_weather(p, 255, 0, 0);
                    }
                    // Weather sound
                    else if (p->weather_intensity == 3)
                    {
                        // thunder
                        sound(p, MSG_WILD_THUNDER);
                    }
                }
            }
        }
        else
            sound(p, wf_info[get_wt_info_at(&p->wpos.grid)->type].sound_idx);
    }
    else if (p->wpos.depth == 1)
        ; // seems these sounds played every time when you enter level.
          // At 1st dlvl we have ironman, old ruins etc dungeons and
          // they have their own entrance sounds... so this one we turn off.
          // Not sure, maybe we need fix all these dungeon "ambient" sounds..
    else if (p->wpos.depth <= 20)
        sound(p, MSG_AMBIENT_DNG1);
    else if (p->wpos.depth <= 40)
        sound(p, MSG_AMBIENT_DNG2);
    else if (p->wpos.depth <= 60)
        sound(p, MSG_AMBIENT_DNG3);
    // paths of the dead
    else if (p->wpos.grid.x == -1 && p->wpos.grid.y == -1 && p->wpos.depth >= 66)
    {
        if (one_in_(2))
            sound(p, MSG_AMBIENT_DNG4);
        else
            sound(p, MSG_PATHS_OF_THE_DEAD);
    }
    else if (p->wpos.depth <= 80)
        sound(p, MSG_AMBIENT_DNG4);
    else if (p->wpos.depth <= 98)
        sound(p, MSG_AMBIENT_DNG5);
    else if (p->wpos.depth == 99)
        sound(p, MSG_AMBIENT_SAURON);
    else if (p->wpos.depth == 100)
        sound(p, MSG_AMBIENT_MORGOTH);
    else if (p->wpos.depth <= 124)
        sound(p, MSG_AMBIENT_DNG6);
    else if (p->wpos.depth == 125)
        sound(p, MSG_AMBIENT_SENYA);
    else if (p->wpos.depth == 126)
        sound(p, MSG_AMBIENT_XAKAZE);
    else
        sound(p, MSG_AMBIENT_MELKOR);
}


/*
 * Play an ambient sound dependent on location
 */
static void play_ambient_sound_location(struct player *p)
{
    // custom depth music-ambience for Sewers
    if (streq(p->locname, "Sewers"))
    {
        if (p->wpos.depth == 8) sound(p, MSG_SEWERS1);
        else if (p->wpos.depth == 9) sound(p, MSG_SEWERS2);
        else if (p->wpos.depth == 10) sound(p, MSG_SEWERS3);
        else if (p->wpos.depth == 11) sound(p, MSG_SEWERS4);
        else if (p->wpos.depth == 12) sound(p, MSG_SEWERS3);
        else if (p->wpos.depth == 13) sound(p, MSG_SEWERS2);
        else if (p->wpos.depth == 14) sound(p, MSG_SEWERS5);
        else if (p->wpos.depth == 15) sound(p, MSG_SEWERS6);
    }
}


/*
 * Helper for process_player -- decrement p->timed[] and curse effect fields.
 */
static void decrease_timeouts(struct player *p, struct chunk *c)
{
    int adjust = (adj_con_fix[p->state.stat_ind[STAT_CON]] + 1);
    int i;

    /* Most timed effects decrement by 1 */
    for (i = 0; i < TMD_MAX; i++)
    {
        int decr = 1;
        if (!p->timed[i]) continue;

        /* Special case */
        if (i == TMD_SAFELOGIN)
        {
            p->timed[i]--;
            if (!p->timed[i]) p->upkeep->redraw |= PR_STATUS;
            continue;
        }

        /* Special cases */
        switch (i)
        {
            case TMD_CUT:
            {
                /* Check for truly "mortal" wound */
                if (player_timed_grade_eq(p, i, "Mortal Wound")) decr = 0;
                else decr = adjust;

                /* Biofeedback always helps */
                if (p->timed[TMD_BIOFEEDBACK]) decr += decr + 10;
                break;
            }

            case TMD_POISONED:
            case TMD_STUN:
            {
                decr = adjust;
                break;
            }

            case TMD_WRAITHFORM:
            {
                /* Hack -- must be in bounds */
                if (!chunk_has_players(&c->wpos) || !square_in_bounds_fully(c, &p->grid))
                    decr = 0;
                break;
            }

            case TMD_FOOD:
            {
                /* Already handled in digest_food() */
                decr = 0;
                break;
            }
        }

        /* Hack -- make -1 permanent */
        if (p->timed[i] == -1) decr = 0;

        /* Decrement the effect */
        if (decr > 0)
        {
            p->no_disturb_icky = true;
            player_dec_timed(p, i, decr, false);
            p->no_disturb_icky = false;
        }
    }

    /* Curse effects always decrement by 1 */
    for (i = 0; i < p->body.count; i++)
    {
        struct curse_data *curse = NULL;
        int j;

        if (p->body.slots[i].obj == NULL) continue;
        curse = p->body.slots[i].obj->curses;

        if (streq(p->clazz->name, "Unbeliever") && one_in_(2))
            curse = 0;

        for (j = 0; curse && (j < z_info->curse_max); j++)
        {
            if (curse[j].power == 0) continue;
            if (curse[j].timeout == 0) continue;
            curse[j].timeout--;

            // hardcode cursed Ring of Teleportation for more often tp
            if (streq(p->body.slots[i].obj->kind->name, "Teleportation") && one_in_(4))
                curse[j].timeout = 0;

            if (!curse[j].timeout)
            {
                do_curse_effect(p, j);
                curse[j].timeout = randcalc(curses[j].obj->time, 0, RANDOMISE);
            }
        }
    }

    /* Spell cooldown */
    for (i = 0; i < p->clazz->magic.total_spells; i++)
    {
        // cooldown restores only inside in the dungeon!
        if (p->spell_cooldown[i] && p->wpos.depth)
        {
            p->spell_cooldown[i]--;
            if (!p->spell_cooldown[i]) p->upkeep->redraw |= (PR_SPELL);
        }
    }
    
    // decrease racial ability cooldown (only in dungeon)
    if (p->y_cooldown && p->wpos.depth)
        p->y_cooldown--;
}


/*
 * Handle things that need updating (mostly once every 10 game turns)
 */
static void process_world(struct player *p, struct chunk *c)
{
    struct loc begin, end;
    struct loc_iterator iter;
    struct worldpos dpos;
    struct location *dungeon;
    int respawn_rate;

    if (!p)
    {
        /* Compact the monster list if we're approaching the limit */
        if (cave_monster_count(c) + 32 > z_info->level_monster_max)
            compact_monsters(c, 64);

        /* Too many holes in the monster list - compress */
        if (cave_monster_count(c) + 32 < cave_monster_max(c))
            compact_monsters(c, 0);

        loc_init(&begin, 0, 0);
        loc_init(&end, c->width, c->height);
        loc_iterator_first(&iter, &begin, &end);

        /* Decrease trap timeouts */
        do
        {
            struct trap *trap = square(c, &iter.cur)->trap;
            while (trap)
            {
                if (trap->timeout)
                {
                    trap->timeout--;
                    if (!trap->timeout) square_light_spot(c, &iter.cur);
                }
                trap = trap->next;
            }
        }
        while (loc_iterator_next_strict(&iter));

        return;
    }

    /* Play an ambient sound at regular intervals. */
    // instead of '1000' there was z_'info->day_length' (10.000)
    if (!(turn.turn % ((10L * 1000) / 4))) play_ambient_sound(p);

/// T: moved this up for day/night change in the OPEN_SKY dungeons
    /* Get the dungeon */
    wpos_init(&dpos, &c->wpos.grid, 0);
    dungeon = get_dungeon(&dpos);
///

    /*** Check the Time ***/
    /* Daybreak/Nighfall in towns or wilderness */
    // T: added check for OPEN_SKY dungeons
    if (((p->wpos.depth == 0) || (dungeon && df_has(dungeon->flags, DF_OPEN_SKY))) &&
         !(turn.turn % ((10L * z_info->day_length) / 2)))
    {
        /* Check for dawn */
        bool dawn = (!(turn.turn % (10L * z_info->day_length)));

        dusk_or_dawn(p, c, dawn);
    }

    /* Hack -- DM redesigning the level */
    if (chunk_inhibit_players(&p->wpos)) return;

    /* Every ten turns */
    if (turn.turn % 10) return;

    /*
     * Note : since monsters are added at a constant rate in real time,
     * this corresponds in game time to them appearing at faster rates
     * deeper in the dungeon.
     */

    respawn_rate = 1;
    /* Hack -- increase respawn rate on no_recall server */
    // if (cfg_diving_mode == 3) respawn_rate = 4;

    // Monster respawn rate based on number of players at the level
    if (dungeon && c->wpos.depth)
    {
        int num_on_depth = 0, i;

        /* Count the number of players actually in game on this level */
        for (i = 1; i <= NumPlayers; i++)
        {
            struct player *q = player_get(i);

            if (!q->upkeep->funeral && wpos_eq(&q->wpos, &c->wpos))
                num_on_depth++;
        }

        // newbie dungeons with multiple players should have especially
        // fast respawn to avoid boredom
        if (c->wpos.depth == 1)
            respawn_rate *= num_on_depth;
        // moar monsters for big crowds
        else if (num_on_depth > 9)
            respawn_rate *= 5;
        else if (num_on_depth > 6)
            respawn_rate *= 4;
        else if (num_on_depth > 3)
            respawn_rate *= 3;
        else if (num_on_depth > 1)
            respawn_rate *= 2;

        // some dungeons might have even faster spawn
        if (df_has(dungeon->flags, DF_FAST_SPAWN)) respawn_rate *= 2;
    }

    /* Check for creature generation */
    if (one_in_(z_info->alloc_monster_chance / respawn_rate))
    {
        if (in_wild(&p->wpos))
        {
            /* Respawn residents in the wilderness outside of town areas */
            if (!town_area(&p->wpos)) wild_add_monster(p, c);
        }
        else
            pick_and_place_distant_monster(p, c, z_info->max_sight + 5, MON_ASLEEP);
    }
}


/*
 * Digest some food
 */
static void digest_food(struct player *p)
{
    /* Ghosts don't need food */
    if (p->ghost) return;

    /* Don't use food in towns */
    if (forbid_town(&p->wpos)) return;

    /* Don't use food near towns (to avoid starving in one's own house) */
    if (town_area(&p->wpos)) return;

    // slow down digestion on low satiation
    if (p->timed[TMD_FOOD] < 100 && !magik(10)) { // 90% to skip digestion
        return;
    } else if (p->timed[TMD_FOOD] < 400 && !magik(30)) { // 70% to skip digestion
        return;
    } else if (p->timed[TMD_FOOD] < 800 && magik(50)) { // 50% to skip digestion
        return;
    }

    /* Digest some food */
    player_dec_timed(p, TMD_FOOD, player_digest(p), false);
}


/*
 * Every turn, the character makes enough noise that nearby monsters can use
 * it to home in.
 *
 * This function actually just computes distance from the player; this is
 * used in combination with the player's stealth value to determine what
 * monsters can hear. We mark the player's grid with 0, then fill in the noise
 * field of every grid that the player can reach with that "noise"
 * (actually distance) plus the number of steps needed to reach that grid,
 * so higher values mean further from the player.
 *
 * Monsters use this information by moving to adjacent grids with lower noise
 * values, thereby homing in on the player even though twisty tunnels and
 * mazes. Monsters have a hearing value, which is the largest sound value
 * they can detect.
 */
static void make_noise(struct player *p)
{
    struct loc next;
    int y, x, d;
    int noise = 0;
    int noise_increment = (p->timed[TMD_COVERTRACKS]? 4: 1);
    struct queue *queue = q_new(p->cave->height * p->cave->width);
    struct chunk *c = chunk_get(&p->wpos);

    loc_copy(&next, &p->grid);

    /* Set all the grids to silence */
    for (y = 1; y < p->cave->height - 1; y++)
        for (x = 1; x < p->cave->width - 1; x++)
            p->cave->noise.grids[y][x] = 0;

    /* Player makes noise */
    p->cave->noise.grids[next.y][next.x] = noise;
    q_push_int(queue, grid_to_i(&next, p->cave->width));
    noise += noise_increment;

    if (streq(p->race->name, "Troglodyte"))
        noise++;

    /* Propagate noise */
    while (q_len(queue) > 0)
    {
        /* Get the next grid */
        i_to_grid(q_pop_int(queue), p->cave->width, &next);

        /* If we've reached the current noise level, put it back and step */
        if (p->cave->noise.grids[next.y][next.x] == noise)
        {
            q_push_int(queue, grid_to_i(&next, p->cave->width));
            noise += noise_increment;
            continue;
        }

        /* Assign noise to the children and enqueue them */
        for (d = 0; d < 8; d++)
        {
            struct loc child;

            /* Child location */
            loc_sum(&child, &next, &ddgrid_ddd[d]);
            if (!player_square_in_bounds(p, &child)) continue;

            /* Ignore features that don't transmit sound */
            if (square_isnoflow(c, &child)) continue;

            /* Skip grids that already have noise */
            if (p->cave->noise.grids[child.y][child.x] != 0) continue;

            /* Skip the player grid */
            if (loc_eq(&child, &p->grid)) continue;

            /* Save the noise */
            p->cave->noise.grids[child.y][child.x] = noise;

            /* Enqueue that entry */
            q_push_int(queue, grid_to_i(&child, p->cave->width));
        }
    }

    q_free(queue);
}


/*
 * Characters leave scent trails for perceptive monsters to track.
 *
 * Scent is rather more limited than sound. Many creatures cannot use
 * it at all, it doesn't extend very far outwards from the character's
 * current position, and monsters can use it to home in the character,
 * but not to run away.
 *
 * Scent is valued according to age. When a character takes his turn,
 * scent is aged by one, and new scent is laid down. Monsters have a smell
 * value which indicates the oldest scent they can detect. Grids where the
 * player has never been will have scent 0. The player's grid will also have
 * scent 0, but this is OK as no monster will ever be smelling it.
 */
static void update_scent(struct player *p)
{
    int y, x;
    int scent_strength[5][5] =
    {
        {2, 2, 2, 2, 2},
        {2, 1, 1, 1, 2},
        {2, 1, 0, 1, 2},
        {2, 1, 1, 1, 2},
        {2, 2, 2, 2, 2},
    };
    struct chunk *c = chunk_get(&p->wpos);

    /* Update scent for all grids */
    for (y = 1; y < p->cave->height - 1; y++)
    {
        for (x = 1; x < p->cave->width - 1; x++)
        {
            // troglodytes leaves stench which stay strong for long time
            // (so scent won't get old in half cases)
            if (streq(p->race->name, "Troglodyte") && one_in_(2))
                continue;
            if (p->cave->scent.grids[y][x] > 0)
                p->cave->scent.grids[y][x]++;
        }
    }

    /* Scentless player */
    if (p->timed[TMD_COVERTRACKS]) return;

    /* Lay down new scent around the player */
    for (y = 0; y < 5; y++)
    {
        for (x = 0; x < 5; x++)
        {
            struct loc scent;
            int new_scent = scent_strength[y][x];
            int d;
            bool add_scent = false;

            /* Initialize */
            loc_init(&scent, x + p->grid.x - 2, y + p->grid.y - 2);

            /* Ignore invalid or non-scent-carrying grids */
            if (!player_square_in_bounds(p, &scent)) continue;
            if (square_isnoscent(c, &scent)) continue;

            /* Check scent is spreading on floors, not going through walls */
            for (d = 0; d < 8; d++)
            {
                struct loc adj;

                loc_sum(&adj, &scent, &ddgrid_ddd[d]);
                if (!player_square_in_bounds(p, &adj)) continue;

                /* Player grid is always valid */
                if ((x == 2) && (y == 2)) add_scent = true;

                /* Adjacent to a closer grid, so valid */
                if (p->cave->scent.grids[adj.y][adj.x] == new_scent - 1) add_scent = true;
            }

            /* Not valid */
            if (!add_scent) continue;

            /* Mark the scent */
            p->cave->scent.grids[scent.y][scent.x] = new_scent;
        }
    }
}


/*
 * Handle things that need updating once every 10 "scaled" game turns
 */
static void process_player_world(struct player *p, struct chunk *c)
{
    int i;

    /* Hack -- fade monster detect over time (unless looking around) */
    for (i = 1; i < cave_monster_max(c); i++)
    {
        if (p->locating) continue;
        if (!p->mon_det[i]) continue;
        p->mon_det[i]--;
        if (!p->mon_det[i])
        {
            struct monster *mon = cave_monster(c, i);

            /* Paranoia -- skip dead monsters */
            if (!mon->race) continue;

            update_mon(mon, c, false);
        }
    }

    /* Hack -- fade player detect over time (unless looking around) */
    for (i = 1; i <= NumPlayers; i++)
    {
        if (p->locating) continue;
        if (!p->play_det[i]) continue;
        p->play_det[i]--;
        if (!p->play_det[i])
        {
            struct player *q = player_get(i);

            /* Paranoia -- skip dead players */
            if (q->upkeep->new_level_method || q->upkeep->funeral) continue;

            update_player(q);
        }
    }

    /* Hack -- semi-constant hallucination (but not in stores) */
    if (p->timed[TMD_IMAGE] && !in_store(p)) p->upkeep->redraw |= (PR_MAP);

    /*** Damage (or healing) over Time ***/

    /* Take damage from permanent wraithform while inside walls */
    if ((p->timed[TMD_WRAITHFORM] == -1) && !square_ispassable(c, &p->grid))
    {
        take_hit(p, player_apply_damage_reduction(p, 1, false, "hypoxia"), "hypoxia",
            "was entombed into solid terrain");
        if (p->is_dead) return;
    }

    /* Take damage from Undead Form */
    if (player_undead(p))
    {
        take_hit(p, player_apply_damage_reduction(p, 1, false, "fading"), "fading", "faded away");
        if (p->is_dead) return;
    }

    /* Take damage from poison */
    if (p->timed[TMD_POISONED])
    {
    
        if (p->is_dead) return;
    }

    /* Take damage from cuts, worse from serious cuts */
    if (p->timed[TMD_CUT])
    {
        if (player_timed_grade_eq(p, TMD_CUT, "Mortal Wound") ||
            player_timed_grade_eq(p, TMD_CUT, "Deep Gash"))
        {
            i = 3;
        }
        else if (player_timed_grade_eq(p, TMD_CUT, "Severe Cut")) i = 2;
        else i = 1;

        /* Take damage */
        take_hit(p, player_apply_damage_reduction(p, i, false, "a fatal wound"), "a fatal wound", "bled to death");
        if (p->is_dead) return;
    }

    /* Side effects of diminishing bloodlust */
    if (p->timed[TMD_BLOODLUST])
    {
        player_over_exert(p, PY_EXERT_HP | PY_EXERT_CUT | PY_EXERT_SLOW,
            MAX(0, 10 - p->timed[TMD_BLOODLUST]), p->chp / 10);
        if (p->is_dead) return;
    }

    /* Timed healing */
    if (p->timed[TMD_HEAL])
    {
        bool ident = false;
        struct source who_body;
        struct source *who = &who_body;

        source_player(who, get_player_index(get_connection(p->conn)), p);
        effect_simple(EF_HEAL_HP, who, "30", 0, 0, 0, 0, 0, &ident);
    }

    /* Player can be damaged by terrain */
    player_take_terrain_damage(p, c);
    if (p->is_dead) return;

    /* Effects of Black Breath */
    if (p->timed[TMD_BLACKBREATH])
    {
        if (one_in_(2))
        {
            msg(p, "The Black Breath sickens you.");
            player_stat_dec(p, STAT_CON, false);
        }
        if (one_in_(2))
        {
            msg(p, "The Black Breath saps your strength.");
            player_stat_dec(p, STAT_STR, false);
        }
        if (one_in_(2))
        {
            int drain = 100 + (p->exp / 100) * z_info->life_drain_percent;

            msg(p, "The Black Breath dims your life force.");
            player_exp_lose(p, drain, false);
            // makes you additionally hungry (+10 on regular exp drain)
            player_dec_timed(p, TMD_FOOD, 25, false);
        }
    }

    /*** Check the Food, and Regenerate ***/

    /* Digest */
    if (!player_timed_grade_eq(p, TMD_FOOD, "Full"))
    {
        /* Every 100 "scaled" turns */
        int time = move_energy(p->wpos.depth) / time_factor(p, c);

        /* Digest normally */
        if (!(turn.turn % time)) digest_food(p);

        /* Fast metabolism */
        if (p->timed[TMD_HEAL] && !p->ghost)
        {
            player_dec_timed(p, TMD_FOOD, 8 * z_info->food_value, false);
            if (p->timed[TMD_FOOD] < PY_FOOD_HUNGRY)
                player_set_timed(p, TMD_HEAL, 0, true);
        }
    }
    else
    {
        /* Digest quickly when gorged */
        player_dec_timed(p, TMD_FOOD, 1000 / z_info->food_value, false);
    }

    /* Faint or starving */
    if (player_timed_grade_eq(p, TMD_FOOD, "Faint"))
    {
        /* If the player is idle and fainting, destroy his connection */
        if (cfg_disconnect_fainting && !ht_zero(&p->idle_turn) &&
            (ht_diff(&turn, &p->idle_turn) > (uint32_t)(cfg_disconnect_fainting * cfg_fps)))
        {
            p->fainting = true;
        }

        /* Faint occasionally */
        if (!p->timed[TMD_PARALYZED] && magik(10))
        {
            /* Message */
            msg(p, "You faint from the lack of food.");
            disturb(p, 0);

            /* Hack -- faint (bypass free action) */
            player_inc_timed(p, TMD_PARALYZED, 1 + randint0(5), true, false);
        }
    }
    else if (player_timed_grade_eq(p, TMD_FOOD, "Starving"))
    {
        /* Calculate damage */
        i = (PY_FOOD_STARVE - p->timed[TMD_FOOD]) / 10;

        /* Take damage */
        take_hit(p, player_apply_damage_reduction(p, i, false, "starvation"), "starvation", "starved to death");
        if (p->is_dead) return;
    }

    /* Regenerate Hit Points if needed */
    if (p->chp < p->mhp) player_regen_hp(p, c);

    /* Regenerate or lose mana */
    player_regen_mana(p);

    /* Check for interrupts */
    player_resting_complete_special(p);

    /* Dwarves detect treasure */
    if (player_has(p, PF_SEE_ORE))
    {
        /* Only if they are in good shape */
        if (!p->timed[TMD_IMAGE] && !p->timed[TMD_CONFUSED] &&
            !p->timed[TMD_AMNESIA] && !p->timed[TMD_STUN] &&
            !p->timed[TMD_PARALYZED] && !p->timed[TMD_TERROR] &&
            !p->timed[TMD_AFRAID])
        {
            struct source who_body;
            struct source *who = &who_body;

            source_player(who, get_player_index(get_connection(p->conn)), p);
            effect_simple(EF_DETECT_ORE, who, "0", 0, 0, 0, 3, 3, NULL);
        }
    }

    /* Quest */
    process_quest(p);

    /* Process light */
    player_update_light(p);

    /* Update noise and scent (not if resting) */
    if (!player_is_resting(p))
    {
        make_noise(p);
        update_scent(p);
    }

    /*** Process Inventory ***/

    /* Handle experience draining */
    if (player_of_has(p, OF_DRAIN_EXP) && ht_zero(&p->idle_turn))
    {
        if (magik(10) && (p->exp > 0))
        {
            int32_t d = damroll(10, 6) + (p->exp / 100) * z_info->life_drain_percent;

            player_exp_lose(p, d / 10, false);
        }

        equip_learn_flag(p, OF_DRAIN_EXP);
    }

    /* Recharge activatable objects and rods */
    recharge_objects(p);

    /* Notice things after time */
    if (!(turn.turn % 100))
        equip_learn_after_time(p);

// T stuff
////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////

    // polymorphed into 'fruit bats' can run.. but sometimes get side effects
    if (p->poly_race) {
        // to check bat we need get_race 1st
        struct monster_race *race_fruit_bat = get_race("fruit bat");
        if (p->poly_race == race_fruit_bat) {
            if (p->wpos.depth && !OPT(p, birth_fruit_bat)) {
                // ~ chance for both is 13
                if (one_in_(40))
                    player_inc_timed(p, TMD_CONFUSED, randint1(3), false, false);
                else if (one_in_(20))
                    player_inc_timed(p, TMD_IMAGE, randint1(3), false, false);
            }
        }
    }

    /////////////////////////////////////
    //////// RACIAL MISC EFFECTS ////////
    /////////////////////////////////////
    // Imp got 'perma-curse' - rng teleport
    // at first it's more often and at bigger distance, but later
    // it becomes more stable
    if (streq(p->race->name, "Imp") && p->wpos.depth)
    {
        int tele_chance = 200 + (p->lev * 2);
        if (one_in_(tele_chance))
        {
            char dice[5];
            int tele_dist = (200 - (p->lev * 4)) + 2;
            int msg_imp = randint1(13);
            struct source who_body;
            struct source *who = &who_body;
            
            // learn black speech.. more or less canonic:
            if (msg_imp == 1) msgt(p, MSG_IMP, "How do you like it?");
            else if (msg_imp == 2) msgt(p, MSG_IMP, "Lat brogb za amol?");
            else if (msg_imp == 3) msgt(p, MSG_IMP, "Brogbzalat amol?");
            else if (msg_imp == 4) msgt(p, MSG_IMP, "Guz latur za?");
            else if (msg_imp == 5) msgt(p, MSG_IMP, "Gur latur za?");
            else if (msg_imp == 6) msgt(p, MSG_IMP, "Mor kiyu moz?");
            else if (msg_imp == 7) msgt(p, MSG_IMP, "Amol latu za?");
            // mixed:
            else if (msg_imp == 8) msgt(p, MSG_IMP, "Amol kiyu ajog?");
            else if (msg_imp == 9) msgt(p, MSG_IMP, "Arz lat alag?");
            else if (msg_imp == 10) msgt(p, MSG_IMP, "Gur sun za?");
            else if (msg_imp == 11) msgt(p, MSG_IMP, "Guz kiyu zab?");
            else if (msg_imp == 12) msgt(p, MSG_IMP, "Narz lat zalo?");
            else if (msg_imp == 13) msgt(p, MSG_IMP, "Amol sun zaurz?");

            source_player(who, get_player_index(get_connection(p->conn)), p);
            strnfmt(dice, sizeof(dice), "%d", tele_dist);
            effect_simple(EF_TELEPORT, who, dice, 0, 0, 0, 0, 0, NULL);
        }
    }

    // Werewolves howl from time to time at night waking everyone :D
    else if (streq(p->race->name, "Werewolf"))
    {
        // Base howl chance - include level bonus for both day and night
        int howl_chance = is_daytime() ? (2000 - (p->lev * 2)) : (200 + (p->lev * 2));
        
        // Ensure minimum chance even at high levels
        if (is_daytime() && howl_chance < 1000) howl_chance = 1000;
        
        if (one_in_(howl_chance))
        {
            struct source who_body;
            struct source *who = &who_body;
            
            // Different message based on time of day
            if (is_daytime()) {
                msgt(p, MSG_HOWL, "Despite the daylight, you feel a primal urge and "
                "let out an unexpected howl!");
            } else {
                msgt(p, MSG_HOWL, "You can't handle your animal nature and "
                "howl on top of your lungs!");
            }
            
            source_player(who, get_player_index(get_connection(p->conn)), p);
            effect_simple(EF_WAKE, who, 0, 0, 0, 0, 0, 0, NULL);
        }
    }
    // Beholders may hallucinate from time to time
    else if (streq(p->race->name, "Beholder") && one_in_(200 + (p->lev * 15)))
        player_inc_timed(p, TMD_IMAGE, randint1(10), true, false); 
    /* Damned constantly hunted by monsters */
    else if (streq(p->race->name, "Damned") && p->wpos.depth &&
        one_in_(200 + (p->lev * 10)))
    {
        struct source who_body;
        struct source *who = &who_body;

        msgt(p, MSG_VERSION, "Gods sent another emissary to deal with you...");
        source_player(who, get_player_index(get_connection(p->conn)), p);
        effect_simple(EF_SUMMON, who, "1", 0, 0, 0, 0, 0, NULL);
    }   

/////////////////////////////////////
///////// SUMMONING EFFECTS /////////
/////////////////////////////////////
    /* Traveller's dog */
    if (streq(p->clazz->name, "Traveller") && p->wpos.depth && p->slaves < 1)
    {
        if (p->lev < 20)
            summon_specific_race_aux(p, c, &p->grid, get_race("cub"), 1, true);
        else if (p->lev < 30)
            summon_specific_race_aux(p, c, &p->grid, get_race("puppy"), 1, true);
        else if (p->lev < 50)
            summon_specific_race_aux(p, c, &p->grid, get_race("dog"), 1, true);
        else if (p->lev > 49)
            summon_specific_race_aux(p, c, &p->grid, get_race("hound"), 1, true);
    }
    /* Trader's cat */
    else if (streq(p->clazz->name, "Trader") && p->wpos.depth && p->slaves < 1)
    {
        if (p->lev < 20)
            summon_specific_race_aux(p, c, &p->grid, get_race("kitten"), 1, true);
        else if (p->lev < 30)
            summon_specific_race_aux(p, c, &p->grid, get_race("cat"), 1, true);
        else if (p->lev < 50)
            summon_specific_race_aux(p, c, &p->grid, get_race("housecat"), 1, true);
        else if (p->lev > 49)
            summon_specific_race_aux(p, c, &p->grid, get_race("big cat"), 1, true);
    }
    /* Scavenger's rat */
    else if (streq(p->clazz->name, "Scavenger") && p->wpos.depth && p->slaves < 1)
    {
        if (p->lev > 9 && p->lev < 32)
            summon_specific_race_aux(p, c, &p->grid, get_race("baby rat"), 1, true);
        else if (p->lev < 44)
            summon_specific_race_aux(p, c, &p->grid, get_race("rat"), 1, true);
        else if (p->lev > 43)
            summon_specific_race_aux(p, c, &p->grid, get_race("fancy rat"), 1, true);
    }
    /* Tamer class: pets */
    else if (streq(p->clazz->name, "Tamer") && p->wpos.depth && p->slaves < 1)
    {
        if (p->lev < 5)
            summon_specific_race_aux(p, c, &p->grid, get_race("tamed frog"), 1, true);
        else if (p->lev < 10)
            summon_specific_race_aux(p, c, &p->grid, get_race("tamed snake"), 1, true);
        else if (p->lev < 15)
            summon_specific_race_aux(p, c, &p->grid, get_race("tamed hawk"), 1, true);
        else if (p->lev < 20)
            summon_specific_race_aux(p, c, &p->grid, get_race("tamed boar"), 1, true);
        else if (p->lev < 25)
            summon_specific_race_aux(p, c, &p->grid, get_race("tamed wolf"), 1, true);
        else if (p->lev < 30)
            summon_specific_race_aux(p, c, &p->grid, get_race("tamed bear"), 1, true);
        else if (p->lev < 35)
            summon_specific_race_aux(p, c, &p->grid, get_race("tamed reptile"), 1, true);
        else if (p->lev < 40)
            summon_specific_race_aux(p, c, &p->grid, get_race("tamed hellcat"), 1, true);
        else if (p->lev < 45)
            summon_specific_race_aux(p, c, &p->grid, get_race("tamed young unicorn"), 1, true);
        else if (p->lev < 50)
            summon_specific_race_aux(p, c, &p->grid, get_race("tamed drake"), 1, true);
        else if (p->lev > 49)
            summon_specific_race_aux(p, c, &p->grid, get_race("tamed dragon"), 1, true);
    }
    /* Necromancer class golem */
    else if (p->timed[TMD_GOLEM] && p->wpos.depth && (p->slaves < (p->lev / 10) + 1))
    {
        for (i = 1; i < cave_monster_max(c); i++)
        {
            struct monster *mon = cave_monster(c, i);

            if (mon->race)
            {
                // if we already got golem - no need to summon another one
                if (streq(mon->race->name, "clay_golem")) break;
                if (streq(mon->race->name, "stone_golem")) break;
                if (streq(mon->race->name, "iron_golem")) break;
                if (streq(mon->race->name, "fire_golem")) break;
                if (streq(mon->race->name, "drolem_")) break;
            }

            // if we checked all monsters and no golem among them: summon one!
            if (i+1 == cave_monster_max(c))
            {
                if (p->lev < 20 && p->lev > 9)
                    summon_specific_race_aux(p, c, &p->grid, get_race("clay_golem"), 1, true);
                else if (p->lev < 30)
                    summon_specific_race_aux(p, c, &p->grid, get_race("stone_golem"), 1, true);
                else if (p->lev < 40)
                    summon_specific_race_aux(p, c, &p->grid, get_race("iron_golem"), 1, true);
                else if (p->lev < 50)
                    summon_specific_race_aux(p, c, &p->grid, get_race("fire_golem"), 1, true);
                else if (p->lev > 49)
                    summon_specific_race_aux(p, c, &p->grid, get_race("drolem_"), 1, true);
            }
        }
    }
    /* Assassins class sentry */
    else if (p->timed[TMD_SENTRY] && p->wpos.depth)
    {
        for (i = 1; i < cave_monster_max(c); i++)
        {
            struct monster *mon = cave_monster(c, i);

            if (mon->race)
            {
                // if we already got sentry - no need to summon another one
                if (streq(mon->race->name, "blade sentry")) break;
                if (streq(mon->race->name, "dart sentry")) break;
                if (streq(mon->race->name, "spear sentry")) break;
                if (streq(mon->race->name, "acid sentry")) break;
                if (streq(mon->race->name, "fire sentry")) break;
                if (streq(mon->race->name, "lightning sentry")) break;
            }

            // if we checked all monsters and no sentry among them: summon one!
            if (i+1 == cave_monster_max(c))
            {
                if (p->lev < 10)
                    summon_specific_race_aux(p, c, &p->grid, get_race("blade sentry"), 1, true);
                else if (p->lev < 20)
                    summon_specific_race_aux(p, c, &p->grid, get_race("dart sentry"), 1, true);
                else if (p->lev < 30)
                    summon_specific_race_aux(p, c, &p->grid, get_race("spear sentry"), 1, true);
                else if (p->lev < 40)
                    summon_specific_race_aux(p, c, &p->grid, get_race("acid sentry"), 1, true);
                else if (p->lev < 50)
                    summon_specific_race_aux(p, c, &p->grid, get_race("fire sentry"), 1, true);
                else if (p->lev > 49)
                    summon_specific_race_aux(p, c, &p->grid, get_race("lightning sentry"), 1, true);
                sound(p, MSG_SENTRY);
            }
        }
    }
    /* Thunderlord race: eagle-companion */
    else if (streq(p->race->name, "Thunderlord") && p->wpos.depth && p->slaves < 1)
    {
        if (p->lev < 20)
            summon_specific_race_aux(p, c, &p->grid, get_race("tamed young eagle"), 1, true);
        else if (p->lev < 30) // else if, so no need to set lower border req.
            summon_specific_race_aux(p, c, &p->grid, get_race("tamed eagle"), 1, true);
        else if (p->lev < 50)
            summon_specific_race_aux(p, c, &p->grid, get_race("tamed great eagle"), 1, true);
        else if (p->lev > 49)
            summon_specific_race_aux(p, c, &p->grid, get_race("tamed giant eagle"), 1, true);
    }

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
// END T stuff

    /*** Involuntary Movement ***/

    /* Delayed Word-of-Recall */
    if (p->word_recall)
    {
        /* Count down towards recall */
        p->word_recall--;

        /* Activate the recall */
        if (!p->word_recall)
        {
            /* Hack -- no recall if in a shop, or under the influence of space/time anchor */
            if (in_store(p) || check_st_anchor(&p->wpos, &p->grid))
                p->word_recall++;

            /* Hack -- no recall if waiting for confirmation */
            else if (p->current_value == ITEM_PENDING)
                p->word_recall++;

            /* Activate the recall */
            else
                recall_player(p, c);
        }
    }

    /* Delayed Deep Descent */
    if (p->deep_descent)
    {
        /* Count down towards descent */
        p->deep_descent--;

        /* Activate the descent */
        if (!p->deep_descent)
        {
            /* Hack -- not if in a shop, or under the influence of space/time anchor */
            if (in_store(p) || check_st_anchor(&p->wpos, &p->grid))
                p->deep_descent++;
            else
            {
                struct source who_body;
                struct source *who = &who_body;

                source_player(who, get_player_index(get_connection(p->conn)), p);
                if (effect_simple(EF_DEEP_DESCENT, who, "0", 0, 1, 0, 0, 0, NULL)) return;

                /* Failure */
                p->deep_descent++;
            }
        }
    }

    // decrease ironman timer till next auto >
    if (OPT(p, birth_ironman) && !p->is_dead && p->chp >= 0) {
        p->iron_timer--;

        // if timer is gone - move ironman player down
        // (timer can have negative value cause we decrease it on movement too)
        if (p->iron_timer < 0 && p->wpos.depth < 126) {
            
            plog_fmt("process_player_world(): [%s] iron_timer < 0", p->name);
            
            // no > if in a shop OR if waiting for confirmation
            if (in_store(p) || (p->current_value == ITEM_PENDING)) {
                p->iron_timer++;
            } else {
                struct source who_body;
                struct source *who = &who_body;

                source_player(who, get_player_index(get_connection(p->conn)), p);
                
                plog_fmt("process_player_world(): [%s] starting attempt to EF_IRONMAN_DESCENT", p->name);

                if (effect_simple(EF_IRONMAN_DESCENT, who, "0", 0, 1, 0, 0, 0, NULL)) {
                    p->iron_timer = set_ironman_timer(p->wpos.depth);
                    // Cancel any WOR spells
                    p->word_recall = 0;
                    p->deep_descent = 0;
                    
                    plog_fmt("process_player_world(): [%s] finishes EF_IRONMAN_DESCENT, returning", p->name);
                    
                    return; // success
                }

                // if fail (not sure how it can happen, but..) - try again next turn
                p->iron_timer++;
            }
        }
    }
}


/*
 * Housekeeping after the processing of a player command
 */
static void process_player_cleanup(struct player *p)
{
    int timefactor, time;
    struct chunk *c = chunk_get(&p->wpos);
    int mode = ((get_connection(p->conn)->state == CONN_QUIT)? AR_QUIT: AR_NORMAL);

    /* If we are in a slow time condition, give visual warning */
    timefactor = time_factor(p, c);
    if (timefactor < NORMAL_TIME)
        square_light_spot_aux(p, c, &p->grid);

    /* Check for auto-retaliate */
    if (has_energy(p, true)) auto_retaliate(p, c, mode);

    /* Notice stuff */
    notice_stuff(p);

    /* Hack -- pack overflow */
    pack_overflow(p, c, NULL);

    /* Determine basic frequency of regen in game turns, then scale by players local time bubble */
    time = move_energy(p->wpos.depth) / (10 * timefactor);

    /* Process the world of that player every ten "scaled" turns */
    if (!(turn.turn % time)) process_player_world(p, c);

    /* Only when needed, every five game turns */
    if (!(turn.turn % 5))
    {
        /* Flicker self if multi-hued */
        if (p->poly_race && monster_shimmer(p->poly_race) && monster_allow_shimmer(p))
            square_light_spot_aux(p, c, &p->grid);

        /* Flicker multi-hued players, party leaders and elementalists */
        if (p->shimmer)
        {
            int j;

            /* Check everyone */
            for (j = 1; j <= NumPlayers; j++)
            {
                struct player *q = player_get(j);

                /* Ignore the player that we're updating */
                if (q == p) continue;

                /* If he's not here, skip him */
                if (!wpos_eq(&q->wpos, &p->wpos)) continue;

                /* If he's not here YET, also skip him */
                if (q->upkeep->new_level_method) continue;

                /* Flicker multi-hued players */
                if (p->poly_race && monster_shimmer(p->poly_race) && monster_allow_shimmer(q))
                    square_light_spot_aux(q, c, &p->grid);

                /* Flicker party leaders */
                if (is_party_owner(q, p) && OPT(q, highlight_leader))
                    square_light_spot_aux(q, c, &p->grid);

                /* Flicker elementalists */
                if (player_has(p, PF_ELEMENTAL_SPELLS) && allow_shimmer(q))
                    square_light_spot_aux(q, c, &p->grid);
            }
        }
    }

    /* Check monster recall */
    if (p->upkeep->monster_race.race)
    {
        struct monster_lore lore;
        uint8_t *blows, *current_blows;
        bool *blow_known, *current_blow_known;

        /* Get the lores (player + global) */
        get_global_lore(p, p->upkeep->monster_race.race, &lore);

        /* Check for change in blows */
        if (0 != memcmp(p->current_lore.blows, lore.blows, z_info->mon_blows_max * sizeof(uint8_t)))
        {
            memmove(p->current_lore.blows, lore.blows, z_info->mon_blows_max * sizeof(uint8_t));
            p->upkeep->redraw |= (PR_MONSTER);
        }

        /* Check for change in known blows */
        if (0 != memcmp(p->current_lore.blow_known, lore.blow_known,
            z_info->mon_blows_max * sizeof(bool)))
        {
            memmove(p->current_lore.blow_known, lore.blow_known,
                z_info->mon_blows_max * sizeof(bool));
            p->upkeep->redraw |= (PR_MONSTER);
        }

        /* Hack -- save blow states, nullify */
        blows = lore.blows;
        current_blows = p->current_lore.blows;
        blow_known = lore.blow_known;
        current_blow_known = p->current_lore.blow_known;
        lore.blows = NULL;
        p->current_lore.blows = NULL;
        lore.blow_known = NULL;
        p->current_lore.blow_known = NULL;

        /* Check for change of any kind (except blow states) */
        if (0 != memcmp(&p->current_lore, &lore, sizeof(struct monster_lore)))
        {
            memmove(&p->current_lore, &lore, sizeof(struct monster_lore));
            p->upkeep->redraw |= (PR_MONSTER);
        }

        /* Hack -- restore blow states */
        lore.blows = blows;
        p->current_lore.blows = current_blows;
        lore.blow_known = blow_known;
        p->current_lore.blow_known = current_blow_known;

        mem_free(lore.blows);
        mem_free(lore.blow_known);
    }

    /* Refresh stuff */
    refresh_stuff(p);
}


/*
 * Process player commands from the command queue, finishing when there is a
 * command using energy (any regular game command), or we run out of commands
 * and need another from the user, or the character changes level or dies, or
 * the game is stopped.
 */
static void process_player(struct player *p)
{
    int time = move_energy(p->wpos.depth) / (10 * time_factor(p, chunk_get(&p->wpos)));

    /* Timeout various things */
    if (!(turn.turn % time)) decrease_timeouts(p, chunk_get(&p->wpos));

    /* Try to execute any commands on the command queue. */
    /* NB: process_pending_commands may have deleted the connection! */
    if (process_pending_commands(p->conn)) return;

    if (!p->upkeep->new_level_method && !p->upkeep->funeral)
        process_player_cleanup(p);
}


/*
 * Housekeeping on arriving on a new level
 */
static void on_new_level(void)
{
    int i;

    /* Hack -- reset current sound */
    for (i = 1; i <= NumPlayers; i++) player_get(i)->current_sound = -1;

    /*
     * The number of game turns per player turn can be calculated as the energy
     * required to act at the current depth (energy per player turn) divided by
     * the energy given per game turn given the current player speed.
     */

    /* Hack -- reset projection indicator every player turn */
    for (i = 1; i <= NumPlayers; i++)
    {
        struct player *p = player_get(i);

        /*
         * For projection indicator, we will consider the shortest number of game turns
         * possible -- the one obtained at max speed.
         */
        int player_turn = move_energy(p->wpos.depth) / frame_energy(199);

        /* Reset projection indicator */
        if (!(turn.turn % player_turn)) p->did_visuals = false;
    }
}


/*
 * Housekeeping on leaving a level
 */
static void on_leave_level(void)
{
    int i;
    struct loc grid;

    /* Deallocate any unused levels */
    for (grid.y = radius_wild; grid.y >= 0 - radius_wild; grid.y--)
    {
        for (grid.x = 0 - radius_wild; grid.x <= radius_wild; grid.x++)
        {
            struct wild_type *w_ptr = get_wt_info_at(&grid);

            /* Caves */
            for (i = 0; i <= w_ptr->max_depth - w_ptr->min_depth; i++)
            {
                if (!w_ptr->chunk_list[i]) continue;

                /* Don't deallocate special levels */
                // note: it doesn't count DM! When testing - use regular char
                if (level_keep_allocated(w_ptr->chunk_list[i])) continue;

                // also exist in admin menu
                /* Hack -- deallocate custom houses */
                // ..in T we also do this at dusk_or_dawn() in wipe_old_houses()
                wipe_custom_houses(&w_ptr->chunk_list[i]->wpos);

                /* Deallocate the level */
                cave_wipe(w_ptr->chunk_list[i]);
                w_ptr->chunk_list[i] = NULL;
            }
        }
    }
}


static struct chunk *get_location(struct monster_race *race)
{
    struct loc grid;

    /* Get location */
    for (grid.y = radius_wild; grid.y >= 0 - radius_wild; grid.y--)
    {
        for (grid.x = 0 - radius_wild; grid.x <= radius_wild; grid.x++)
        {
            struct wild_type *w_ptr = get_wt_info_at(&grid);
            struct chunk *c = w_ptr->chunk_list[0];

            if (c && allow_location(race, &c->wpos)) return c;
        }
    }

    return NULL;
}


/*
 * Handles "global" things on the server
 */
static void process_various(void)
{
    /* Purge the player database occasionally */
    if (!(turn.turn % (cfg_fps * 60 * 60 * SERVER_PURGE)))
        purge_player_names();

    /* Save the server state occasionally */
    if (!(turn.turn % (cfg_fps * 60 * SERVER_SAVE)))
    {
        int i;

        /* Save server state + player names */
        save_server_info(false);
        save_account_info(false);

        /* Save each player */
        for (i = 1; i <= NumPlayers; i++)
        {
            struct player *p = player_get(i);

            /* Save this player */
            if (!p->upkeep->funeral) save_player(p, false);
        }
    }

    /* Handle certain things once a minute */
    if (!(turn.turn % (cfg_fps * 60)))
    {
        int i;

        /* Update the player retirement timers */
        for (i = 1; i <= NumPlayers; i++)
        {
            struct player *p = player_get(i);

            /* If our retirement timer is set */
            if (!p->upkeep->funeral && (p->retire_timer > 0))
            {
                /* Decrement our retire timer */
                p->retire_timer--;

                /* If the timer runs out, forcibly retire this character */
                if (!p->retire_timer) do_cmd_retire(p);
            }
        }

        /* If the level unstaticer is not disabled */
        if (cfg_level_unstatic_chance >= 0)
        {
            int depth;
            struct loc grid;

            /* For each dungeon level */
            for (grid.y = radius_wild; grid.y >= 0 - radius_wild; grid.y--)
            {
                for (grid.x = 0 - radius_wild; grid.x <= radius_wild; grid.x++)
                {
                    struct wild_type *w_ptr = get_wt_info_at(&grid);

                    for (depth = w_ptr->min_depth; depth < w_ptr->max_depth; depth++)
                    {
                        int num_on_depth = 0;
                        struct worldpos wpos;

                        /* Random chance of the level unstaticing */
                        /* The chance is one in (base_chance * depth) / 250 feet */
                        int chance = (cfg_level_unstatic_chance * (depth + 5) / 5) - 1;

                        /* Full unstaticer */
                        if (!cfg_level_unstatic_chance) chance = 0;

                        wpos_init(&wpos, &grid, depth);

                        /* If this depth is static */
                        if (!chunk_has_players(&wpos)) continue;

                        /* Count the number of players actually in game on this level */
                        for (i = 1; i <= NumPlayers; i++)
                        {
                            struct player *p = player_get(i);

                            if (!p->upkeep->funeral && wpos_eq(&p->wpos, &wpos))
                                num_on_depth++;
                        }

                        /* Unstatic the level if this level is static and no one is actually on it */
                        if (!num_on_depth && one_in_(chance))
                            chunk_set_player_count(&wpos, 0);
                    }
                }
            }
        }
    }

    /* Grow crops very occasionally */
    if (!(turn.turn % (10L * GROW_CROPS)))
    {
        struct loc grid;

        /* For each wilderness level */
        for (grid.y = radius_wild; grid.y >= 0 - radius_wild; grid.y--)
        {
            for (grid.x = 0 - radius_wild; grid.x <= radius_wild; grid.x++)
            {
                struct wild_type *w_ptr = get_wt_info_at(&grid);
                struct chunk *c = w_ptr->chunk_list[0];
                struct loc begin, end;

                /* Must exist */
                if (!c) continue;

                loc_init(&begin, 0, 0);
                loc_init(&end, c->width, c->height);

                wild_grow_crops(c, &begin, &end, true);
            }
        }
    }
    
    /* Grow trees very occasionally */
    if (!(turn.turn % (10L * GROW_TREE)))
    {
        struct loc grid;

        /* Find a suitable location */
        for (grid.y = radius_wild; grid.y >= 0 - radius_wild; grid.y--)
        {
            for (grid.x = 0 - radius_wild; grid.x <= radius_wild; grid.x++)
            {
                struct wild_type *w_ptr = get_wt_info_at(&grid);
                struct chunk *c = w_ptr->chunk_list[0];
                struct loc begin, end;
                struct loc_iterator iter;

                /* Must exist */
                if (!c) continue;

                /* Only town */
                if(!in_town(&c->wpos)) continue;

                loc_init(&begin, 0, 0);
                loc_init(&end, c->width, c->height);
                loc_iterator_first(&iter, &begin, &end);

                do
                {
                    /* Only allow "dirt" */
                    if (!square_isdirt(c, &iter.cur)) continue;

                    /* Never grow on top of objects or monsters/players */
                    if (square_object(c, &iter.cur)) continue;
                    if (square(c, &iter.cur)->mon) continue;
                    if (one_in_(2)) continue;

                    /* Grow a tree here */
                    square_add_tree(c, &iter.cur);

                    /* Done */
                    break;
                }
                while (loc_iterator_next_strict(&iter));
            }
        }
    }    

    /* Update the stores */
    store_update();

    /* Hack -- prevent wilderness monster "buildup" */
    if (!(turn.turn % (10L * z_info->day_length)))
    {
        struct loc grid;

        for (grid.y = radius_wild; grid.y >= 0 - radius_wild; grid.y--)
        {
            for (grid.x = 0 - radius_wild; grid.x <= radius_wild; grid.x++)
            {
                struct wild_type *w_ptr = get_wt_info_at(&grid);
                struct chunk *c = w_ptr->chunk_list[0];
                int m_idx, i, num_on_depth = 0;
                struct worldpos wpos;

                /* Must exist */
                if (!c) continue;

                wpos_init(&wpos, &grid, 0);

                /* Count the number of players actually in game on this level */
                for (i = 1; i <= NumPlayers; i++)
                {
                    struct player *p = player_get(i);

                    if (!p->upkeep->funeral && wpos_eq(&p->wpos, &wpos))
                        num_on_depth++;
                }

                /* Only if no one is actually on this level */
                if (num_on_depth) continue;

                /* Count the number of townies actually on this level if this is a town */
                if (in_town(&c->wpos))
                {
                    int max_townies = get_town(&c->wpos)->max_townies;

                    /* Only if max number of townies is reached */
                    if ((max_townies == -1) || (cave_monster_count(c) < max_townies)) continue;
                }

                /* Mimic stuff */
                for (m_idx = cave_monster_max(c) - 1; m_idx >= 1; m_idx--)
                {
                    struct monster *mon = cave_monster(c, m_idx);
                    struct object *obj = mon->mimicked_obj;

                    /* Delete mimicked objects */
                    if (obj) square_delete_object(c, &mon->grid, obj, false, false);

                    /* Paranoia */
                    if (!mon || !mon->race) continue;

                    /* Delete mimicked features */
                    if (mon->race->base == lookup_monster_base("feature mimic"))
                        square_set_floor(c, &mon->grid, mon->feat);
                }

                /* Wipe the monster list */
                wipe_mon_list(c);
            }
        }
    }

    /* Hack -- prevent surface levels from becoming a "trash dump" */
    if (!(turn.turn % (10L * z_info->day_length)))
    {
        struct loc grid;

        for (grid.y = radius_wild; grid.y >= 0 - radius_wild; grid.y--)
        {
            for (grid.x = 0 - radius_wild; grid.x <= radius_wild; grid.x++)
            {
                struct wild_type *w_ptr = get_wt_info_at(&grid);
                struct chunk *c = w_ptr->chunk_list[0];
                struct object *obj, *next;
                struct loc begin, end;
                struct loc_iterator iter;
                int i, num_on_depth = 0;
                struct worldpos wpos;

                /* Must exist */
                if (!c) continue;

                wpos_init(&wpos, &grid, 0);

                /* Count the number of players actually in game on this level */
                for (i = 1; i <= NumPlayers; i++)
                {
                    struct player *p = player_get(i);

                    if (!p->upkeep->funeral && wpos_eq(&p->wpos, &wpos))
                        num_on_depth++;
                }

                /* Only if no one is actually on this level */
                if (num_on_depth) continue;

                loc_init(&begin, 0, 0);
                loc_init(&end, c->width, c->height);
                loc_iterator_first(&iter, &begin, &end);

                do
                {
                    /* Hack -- skip objects in houses */
                    if (square_isvault(c, &iter.cur) && !square_notrash(c, &iter.cur)) continue;

                    obj = square_object(c, &iter.cur);

                    while (obj)
                    {
                        next = obj->next;

                        /* Nuke object (unless it's a mimic) */
                        if (!obj->mimicking_m_idx)
                            square_delete_object(c, &iter.cur, obj, false, false);

                        obj = next;
                    }
                }
                while (loc_iterator_next_strict(&iter));
            }
        }
    }

    /* Paranoia -- respawn NO_DEATH monsters */
    if (!(turn.turn % (10L * z_info->day_length)))
    {
        int i;

        for (i = 1; i < z_info->r_max; i++)
        {
            struct monster_race *race = &r_info[i];
            struct chunk *c;
            struct loc grid;
            bool found = false;
            int tries = 50;
            struct monster_group_info info = {0, 0};

            if (!rf_has(race->flags, RF_NO_DEATH)) continue;
            if (race->lore.spawned) continue;

            /* Get location */
            c = get_location(race);
            if (!c) continue;

            /* Pick a location and place the monster */
            while (tries-- && !found) found = find_training(c, &grid);
            if (found)
                place_new_monster(NULL, c, &grid, race, MON_ASLEEP | MON_GROUP, &info, ORIGIN_DROP);
        }
    }
}


static void process_death(void)
{
    int i;

    /* Check for death */
    for (i = 1; i <= NumPlayers; i++)
    {
        struct player *p = player_get(i);

        /* Check for death (postpone death if no level) */
        if (p->is_dead && !p->upkeep->new_level_method && !p->upkeep->funeral)
        {
            /* Kill him */
            player_death(p);
        }
    }
}


static void remove_hounds(struct player *p, struct chunk *c)
{
    int j, d;

    /* Remove nearby hounds */
    for (j = 1; j < cave_monster_max(c); j++)
    {
        struct monster *mon = cave_monster(c, j);

        /* Paranoia -- skip dead monsters */
        if (!mon->race) continue;

        /* Hack -- skip unique monsters */
        if (monster_is_unique(mon)) continue;

        /* Skip monsters other than hounds */
        if (mon->race->base != lookup_monster_base("zephyr hound")) continue;

        /* Skip distant monsters */
        d = distance(&p->grid, &mon->grid);
        if (d > z_info->max_sight) continue;

        /* Delete the monster */
        delete_monster_idx(c, j);
    }
}


static void place_player(struct player *p, struct chunk *c, struct loc *grid)
{
    int d, j;

    /* Try to find an empty space */
    for (j = 0; j < 1500; ++j)
    {
        struct loc new_grid;

        /* Increasing distance */
        d = (j + 149) / 150;

        /* Pick a location (skip LOS test) */
        if (!scatter(c, &new_grid, grid, d, false)) continue;

        /* Must have an "empty" grid */
        if (!square_isemptyfloor(c, &new_grid)) continue;

        /* Not allowed to go onto a icky location (house) */
        if ((p->wpos.depth == 0) && square_isvault(c, &new_grid)) continue;

        /* Place the player */
        loc_copy(&p->old_grid, &new_grid);
        loc_copy(&p->grid, &new_grid);

        return;
    }

    /* Try to find an occupied space */
    for (j = 0; j < 1500; ++j)
    {
        struct loc new_grid;

        /* Increasing distance */
        d = (j + 149) / 150;

        /* Pick a location (skip LOS test) */
        if (!scatter(c, &new_grid, grid, d, false)) continue;

        /* Must have a "floor" grid (forbid players only) */
        if (!square_ispassable(c, &new_grid) || (square(c, &new_grid)->mon < 0))
            continue;

        /* Not allowed to go onto a icky location (house) */
        if ((p->wpos.depth == 0) && square_isvault(c, &new_grid)) continue;

        /* Remove any monster at that location */
        delete_monster(c, &new_grid);

        /* Place the player */
        loc_copy(&p->old_grid, &new_grid);
        loc_copy(&p->grid, &new_grid);

        return;
    }

    /* Paranoia: place the player in bounds */
    loc_copy(&p->old_grid, grid);
    loc_copy(&p->grid, grid);
}


static void generate_new_level(struct player *p)
{
    int id;
    bool new_level = false;
    struct chunk *c;
    struct loc grid;
/// T: moved this up for day/night change in the OPEN_SKY dungeons
    struct worldpos dpos;
    struct location *dungeon;
///

    id = get_player_index(get_connection(p->conn));
    c = chunk_get(&p->wpos);

    /* Paranoia */
    if (!chunk_has_players(&p->wpos)) return;

////* Play ambient sound on change of level. */

    // north areas always got snow
    if (p->wpos.depth == 0 && p->wpos.grid.x == 0 && p->wpos.grid.y == 4) // Helcaraxe
    {
        // check if it's raining then stop playback
        if (p->weather_type == 1) sound(p, MSG_SILENT0);

        Send_weather(p, 2, randint1(4), randint1(3));
        sound(p, MSG_WILD_GLACIER);
    }

    // always play it only while in a dungeon; if outside - sometimes
    if (p->wpos.depth == 0 && one_in_(2))
        ;
    else
        play_ambient_sound(p);
/////

    /* Check "maximum depth" to make sure it's still correct */
    if (p->wpos.depth > p->max_depth) p->max_depth = p->wpos.depth;

    /* Make sure the server doesn't think the player is in a store */
    p->store_num = -1;

    /* Somebody has entered an ungenerated level */
    if (!c)
    {
        new_level = true;

        /* Generate a dungeon level there */
        c = prepare_next_level(p);

        wild_deserted_message(p);

        // T: added check for OPEN_SKY
        // 1) labyrinths become lit in gen-cave.c
        // 2) cavern, gaunt and h-centre doesn't have any illumination in the gen-cave code
        if (c->profile == dun_labyrinth || c->profile == dun_cavern || c->profile == dun_gauntlet ||
            c->profile == dun_hard_centre)
        {
            wpos_init(&dpos, &c->wpos.grid, 0);
            dungeon = get_dungeon(&dpos);
            if (dungeon)
            {   // Mirkwood (must be always dark)
                if (p->wpos.grid.x == 0 && p->wpos.grid.y == -3 && p->wpos.depth > 0)
                    wiz_unlit(p, c);
                // last levels of Erebor (Lonely Mountain) - dark
                else if (p->wpos.grid.x == -1 && p->wpos.grid.y == -3 && p->wpos.depth > 70)
                    wiz_unlit(p, c);
                else if (df_has(dungeon->flags, DF_OPEN_SKY))
                    cave_illuminate(p, c, is_daytime());
            }
        }
    }
    /* Apply illumination */
    else
    {
        /* Clear the flags for each cave grid (cave dimensions may have changed) */
        player_cave_new(p, c->height, c->width);
        player_cave_clear(p, true);
        player_place_feeling(p, c);

        /* Illuminate */
        cave_illuminate(p, c, is_daytime());

        /* Ensure fixed encounters on special levels (wilderness) */
        if (special_level(&c->wpos) && (cfg_diving_mode < 2))
        {
            int i;

            for (i = 1; i < z_info->r_max; i++)
            {
                struct monster_race *race = &r_info[i];
                struct monster_group_info info = {0, 0};
                bool found = false;
                int tries = 50;

                /* The monster must be an unseen fixed encounter of this depth. */
                if (race->lore.spawned) continue;
                if (!rf_has(race->flags, RF_PWMANG_FIXED)) continue;
                if (race->level != c->wpos.depth) continue;
                if (!allow_location(race, &c->wpos)) continue;

                /* Pick a location and place the monster */
                while (tries-- && !found)
                {
                    if (rf_has(race->flags, RF_AQUATIC)) found = find_emptywater(c, &grid);
                    else if (rf_has(race->flags, RF_NO_DEATH)) found = find_training(c, &grid);
                    else found = (find_empty(c, &grid) && square_is_monster_walkable(c, &grid));
                }
                if (found)
                    place_new_monster(p, c, &grid, race, MON_ASLEEP | MON_GROUP, &info, ORIGIN_DROP);
                else
                    plog_fmt("Unable to place monster of race %s", race->name);
            }
        }
    }

    /* Give a level feeling to this player */
    p->obj_feeling = -1;
    p->mon_feeling = -1;
    if (random_level(&p->wpos)) display_feeling(p, false);
    p->upkeep->redraw |= (PR_STATE);

    /* Player gets to go first */
    if (p->upkeep->new_level_method != LEVEL_GHOST) set_energy(p, &p->wpos);

    /* Hack -- enforce illegal panel */
    loc_init(&p->offset_grid, z_info->dungeon_wid, z_info->dungeon_hgt);

    /* Determine starting location */
    switch (p->upkeep->new_level_method)
    {
        /* Climbed down */
        case LEVEL_DOWN:
        {
            loc_copy(&grid, &c->join->down);

            /* Never get pushed from stairs when entering a new level */
            if (new_level) delete_monster(c, &grid);
            break;
        }

        /* Climbed up */
        case LEVEL_UP:
        {
            loc_copy(&grid, &c->join->up);

            /* Never get pushed from stairs when entering a new level */
            if (new_level) delete_monster(c, &grid);
            break;
        }

        /* Teleported level */
        case LEVEL_RAND:
            loc_copy(&grid, &c->join->rand);
            break;

        /* Used ghostly travel, stay in bounds */
        case LEVEL_GHOST:
            loc_init(&grid, MIN(MAX(p->grid.x, 1), c->width - 2),
                MIN(MAX(p->grid.y, 1), c->height - 2));
            break;

        /* Over the river and through the woods */
        case LEVEL_OUTSIDE:
            loc_copy(&grid, &p->grid);
            break;

        /*
         * This is used instead of extending the level_rand_y/x
         * into the negative direction to prevent us from
         * allocating so many starting locations. Although this does
         * not make players teleport to similar locations, this
         * could be achieved by seeding the RNG with the depth.
         */
        case LEVEL_OUTSIDE_RAND:
        {
            /* Make sure we aren't in an "icky" or damaging location */
            do
            {
                loc_init(&grid, rand_range(1, c->width - 2), rand_range(1, c->height - 2));
            }
            while (square_isvault(c, &grid) || !square_ispassable(c, &grid) ||
                square_isdamaging(c, &grid));
            break;
        }
    }

    /* Place the player */
    place_player(p, c, &grid);

    /* Hack -- final check to ensure player is in bounds */
    if (!square_in_bounds_fully(c, &p->grid))
    {
        plog_fmt("Unable to place player %s at position (%d,%d)", p->name, p->grid.x, p->grid.y);
        plog_fmt("Cave type %d (w=%d,h=%d)", c->profile, c->width, c->height);
        my_assert(0);
    }

    /* Hack -- player position is valid now */
    p->placed = true;

    /* PWMAngband: give a warning when entering a gauntlet level */
    if (square_limited_teleport(c, &p->grid))
        msgt(p, MSG_ENTER_PIT, "The air feels very still!");

    if (p->wpos.grid.x == 1 && p->wpos.grid.y == 0 && p->wpos.depth == 5)
        sound(p, MSG_AMBIENT_VOICE); // hi from Yaga
    else if (p->wpos.grid.x == 0 && p->wpos.grid.y == 6 && p->wpos.depth == 1)
    {
        // Ironman entrance
        msgt(p, MSG_IRONMAN_START, "Where am I?.. Oh no.. It seems I was abducted to Thangorodrim!");
    }
    else if (p->wpos.grid.x == 0 && p->wpos.grid.y == 0 && p->wpos.depth == 1)
    {
        player_inc_timed(p, TMD_BLIND, 5, false, false); // kinda 'splash' screen
        player_inc_timed(p, TMD_INVIS, 5, false, false);
        player_inc_timed(p, TMD_REVIVE, 5, false, false);
        player_inc_timed(p, TMD_OCCUPIED, 5, false, false);
        sound(p, MSG_ENTER_RUINS); // enter Old Ruins
    }
    else if (p->wpos.grid.x == 1 && p->wpos.grid.y == 1 && p->wpos.depth == 4)
        sound(p, MSG_GONG); // enter Orc Caves
    else if (p->wpos.grid.x == 1 && p->wpos.grid.y == 1 && p->wpos.depth == 12)
        sound(p, MSG_ORC_CAVES); // hi from Solovei
    else if (p->wpos.grid.x == -1 && p->wpos.grid.y == 0 && p->wpos.depth == 20)
        sound(p, MSG_KIKIMORA); // hi from Kikimora
    else if (p->wpos.grid.x == -1 && p->wpos.grid.y == 2 && p->wpos.depth == 27)
        sound(p, MSG_MANOR); // hi from Koschei
    else if (p->wpos.grid.x == 0 && p->wpos.grid.y == 2 && p->wpos.depth == 35)
        sound(p, MSG_ENTER_BARROW); // hi from Witch-King
    else if (p->wpos.grid.x == -2 && p->wpos.grid.y == 2 && p->wpos.depth == 30)
    {
        msgt(p, MSG_BROADCAST_LEVEL, "Ecch.. You feel poisonous smell there!");
        msgt(p, MSG_BROADCAST_LEVEL, "You may want to ensure that you've got poison resistance...");
    }

    /* Add the player */
    square_set_mon(c, &p->grid, 0 - id);

    /* Redraw */
    square_light_spot(c, &p->grid);

    /* Prevent hound insta-death */
    if (new_level) remove_hounds(p, c);

    /* Choose panel */
    verify_panel(p);

    /* Redraw */
    p->upkeep->redraw |= (PR_MAP | PR_DEPTH | PR_FLOOR | PR_MONSTER | PR_OBJECT | PR_MONLIST |
        PR_ITEMLIST);

    /* Fully update the visuals (and monster distances) */
    update_view(p, c);
    update_monsters(c, true);
    update_players();

    /* Clear the flag */
    p->upkeep->new_level_method = 0;

    /* Cancel the target */
    target_set_monster(p, NULL);

    /* Cancel tracking */
    cursor_track(p, NULL);

    /* Cancel the health bar */
    health_track(p->upkeep, NULL);

    /* Calculate torch radius */
    p->upkeep->update |= (PU_BONUS);

    /* Detect secret doors and traps */
    search(p, c);

    /* Play music */
    redraw_stuff(p);
    Send_sound(p, -1);

    /* Play an ambient sound dependent on location */
    play_ambient_sound_location(p);
}


/*
 * Give the player some energy.
 */
static void energize_player(struct player *p)
{
    int energy;
    struct chunk *c = chunk_get(&p->wpos);
    bool allow_running = (in_town(&c->wpos) || !monsters_in_los(p, c));

    /* Player is idle */
    bool is_idle = has_energy(p, false);

    if (p->timed[TMD_PARALYZED] || p->timed[TMD_OCCUPIED] || player_timed_grade_eq(p, TMD_STUN, "Knocked Out"))
    {
        // Golems have 1/2 chance to act even during paralyze
        if (streq(p->race->name, "Golem") && p->timed[TMD_PARALYZED] && one_in_(2))
            ;
        else
            is_idle = true;
    }

    /* Update idle turn */
    if (is_idle && ht_zero(&p->idle_turn)) ht_copy(&p->idle_turn, &turn);
    else if (!is_idle && !ht_zero(&p->idle_turn)) ht_reset(&p->idle_turn);

    /* How much energy should we get? */
    energy = frame_energy(p->state.speed);

    /* Scale depending upon our time bubble */
    energy = energy * time_factor(p, c) / 100;

    /* Running speeds up time */
    if (p->upkeep->running && allow_running) energy = energy * RUNNING_FACTOR / 100;

    /* Hack -- record that amount for player turn calculation */
    p->charge += energy;

    /* Make sure they don't have too much */
    if (p->energy < move_energy(p->wpos.depth))
    {
        /* Give the player some energy */
        p->energy += energy;
    }

    /* Hack -- save the surplus in case we need more due to negative moves */
    else if (p->energy + p->extra_energy < energy_per_move(p))
        p->extra_energy += energy;

    /* Paralyzed or Knocked Out player gets no turn */
    if (p->timed[TMD_PARALYZED] || p->timed[TMD_OCCUPIED] || player_timed_grade_eq(p, TMD_STUN, "Knocked Out"))
    {
        // Golems have 1/2 chance to get turn even during paralyze
        if (streq(p->race->name, "Golem") && p->timed[TMD_PARALYZED] && one_in_(2))
            ;
        else
            do_cmd_sleep(p);
    }

    /* Hack -- if player has energy and we are in a slow time bubble, blink faster */
    if ((p->bubble_speed < NORMAL_TIME) && (p->blink_speed <= (uint32_t)cfg_fps))
    {
        p->blink_speed = (uint32_t)cfg_fps;
        if (has_energy(p, false)) p->blink_speed = (uint32_t)cfg_fps / 4;
    }
}


/*
 * Give monsters some energy.
 */
static void energize_monsters(struct chunk *c)
{
    int i;
    int mspeed, energy;

    /* Process the monsters (backwards) */
    for (i = cave_monster_max(c) - 1; i >= 1; i--)
    {
        struct monster *mon;

        /* Get a 'live' monster */
        mon = cave_monster(c, i);
        if (!mon->race) continue;

        /* Skip "unconscious" monsters */
        if (mon->hp == 0) continue;

        /* Calculate the net speed */
        mspeed = mon->mspeed;
        if (mon->m_timed[MON_TMD_FAST])
            mspeed += 10;
        if (mon->m_timed[MON_TMD_SLOW])
        {
            int slow_level = monster_effect_level(mon, MON_TMD_SLOW);

            mspeed -= (2 * slow_level);
        }

        /* Obtain the energy boost */
        energy = frame_energy(mspeed);

        /* If we are within a player's time bubble, scale our energy */
        if (mon->closest_player)
        {
            bool allow_running = (!in_town(&c->wpos) && !monsters_in_los(mon->closest_player, c));

            energy = energy * time_factor(mon->closest_player, c) / 100;

            /* Speed up time if the player is running, except in town */
            if (mon->closest_player->upkeep->running && allow_running)
                energy = energy * RUNNING_FACTOR / 100;
        }

        /* Make sure we don't store up too much energy */
        if (mon->energy < move_energy(mon->wpos.depth))
        {
            /* Give this monster some energy */
            mon->energy += energy;
        }
    }
}


/*
 * Pre-turn game loop.
 */
static void pre_turn_game_loop(void)
{
    int i;
    struct loc grid;

    on_new_level();

    /* Handle any network stuff */
    Net_input();

    /* Process monsters with even more energy first */
    for (grid.y = radius_wild; grid.y >= 0 - radius_wild; grid.y--)
    {
        for (grid.x = 0 - radius_wild; grid.x <= radius_wild; grid.x++)
        {
            struct wild_type *w_ptr = get_wt_info_at(&grid);

            for (i = 0; i <= w_ptr->max_depth - w_ptr->min_depth; i++)
            {
                struct chunk *c = w_ptr->chunk_list[i];

                if (c) process_monsters(c, true);
            }
        }
    }

    /* Check for death */
    process_death();
}


// Only rings for now.
// We use there real seconds, no need to convert cfg_fps
static void process_worn(struct player *p, struct object *ring)
{
    if (!ring) return;

    /* Increment worn turn counter */
    ring->worn_turn++;

    if (ring->worn_turn % 1920 != 0) return; // Checks every 30 minutes

    if (ring->kind->sval == lookup_sval(TV_RING, "Black Ring of Power"))
    {
        uint16_t turns = ring->worn_turn;

        char o_name[NORMAL_WID];
        object_desc(p, o_name, sizeof(o_name), ring, ODESC_BASE);

        // 1) remove HUNGER 2
        if (turns == 1920 && of_has(ring->flags, OF_HUNGER_2)) {
            of_off(ring->flags, OF_HUNGER_2);
            msg(p, "Your %s changes your metabolism.", o_name);
        }
        // 2) remove HUNGER
        else if (turns == 3840 && of_has(ring->flags, OF_HUNGER)) {
            of_off(ring->flags, OF_HUNGER);
            msg(p, "Your %s changes your metabolism.", o_name);
        }
    }
}


/*
 * Post-turn game loop.
 */
static void post_turn_game_loop(void)
{
    int i;
    struct loc grid;

    /* Check for death */
    process_death();

    /* Process the rest of the monsters */
    for (grid.y = radius_wild; grid.y >= 0 - radius_wild; grid.y--)
    {
        for (grid.x = 0 - radius_wild; grid.x <= radius_wild; grid.x++)
        {
            struct wild_type *w_ptr = get_wt_info_at(&grid);

            for (i = 0; i <= w_ptr->max_depth - w_ptr->min_depth; i++)
            {
                struct chunk *c = w_ptr->chunk_list[i];

                if (c)
                {
                    process_monsters(c, false);

                    /* Mark all monsters as ready to act when they have the energy */
                    reset_monsters(c);
                }
            }
        }
    }

    /* Check for death */
    process_death();

    /* Process the objects */
    for (grid.y = radius_wild; grid.y >= 0 - radius_wild; grid.y--)
    {
        for (grid.x = 0 - radius_wild; grid.x <= radius_wild; grid.x++)
        {
            struct wild_type *w_ptr = get_wt_info_at(&grid);

            for (i = 0; i <= w_ptr->max_depth - w_ptr->min_depth; i++)
            {
                struct chunk *c = w_ptr->chunk_list[i];

                if (c) process_objects(c);
            }
        }
    }

    /* Process the world */
    for (grid.y = radius_wild; grid.y >= 0 - radius_wild; grid.y--)
    {
        for (grid.x = 0 - radius_wild; grid.x <= radius_wild; grid.x++)
        {
            struct wild_type *w_ptr = get_wt_info_at(&grid);

            for (i = 0; i <= w_ptr->max_depth - w_ptr->min_depth; i++)
            {
                struct chunk *c = w_ptr->chunk_list[i];

                /* Process the world every ten turns */
                if (c && !(turn.turn % 10)) process_world(NULL, c);
            }
        }
    }

    /* Process the world */
    for (i = 1; i <= NumPlayers; i++)
    {
        struct player *p = player_get(i);

        /* Process the world of that player */
        if (!p->upkeep->new_level_method && !p->upkeep->funeral)
            process_world(p, chunk_get(&p->wpos));
    }

    /* Process everything else */
    process_various();

    /* Give energy to all players */
    for (i = 1; i <= NumPlayers; i++)
    {
        struct player *p = player_get(i);

        /* Give the player some energy */
        if (!p->upkeep->new_level_method && !p->upkeep->funeral) energize_player(p);
    }

    /* Give energy to all monsters */
    for (grid.y = radius_wild; grid.y >= 0 - radius_wild; grid.y--)
    {
        for (grid.x = 0 - radius_wild; grid.x <= radius_wild; grid.x++)
        {
            struct wild_type *w_ptr = get_wt_info_at(&grid);

            for (i = 0; i <= w_ptr->max_depth - w_ptr->min_depth; i++)
            {
                struct chunk *c = w_ptr->chunk_list[i];

                if (c) energize_monsters(c);
            }
        }
    }

    /* Count game turns */
    ht_add(&turn, 1);
    for (i = 1; i <= NumPlayers; i++)
    {
        struct player *p = player_get(i);

        if (p->upkeep->funeral) continue;

        /* Increment the game turn counter */
        ht_add(&p->game_turn, 1);

        /* Increment the player turn counter */
        if (p->charge >= move_energy(p->wpos.depth))
        {
            p->charge -= move_energy(p->wpos.depth);
            ht_add(&p->player_turn, 1);
        }

        /* Increment the active player turn counter */
        if (p->has_energy && ht_zero(&p->idle_turn)) ht_add(&p->active_turn, 1);

        /* Player has energy */
        p->has_energy = has_energy(p, false);

        /* Inform the client every second */
        if (!(turn.turn % cfg_fps))
        {
            Send_turn(p, ht_div(&p->game_turn, cfg_fps), ht_div(&p->player_turn, 1),
                ht_div(&p->active_turn, 1));

            // moved up to save CPU and avoid big numbers
            /* Increment worn turn counter if inside a dungeon */
            if (p->wpos.depth > 0)
            {
                struct object *right = slot_object(p, slot_by_name(p, "right hand"));
                struct object *left = slot_object(p, slot_by_name(p, "left hand"));

                process_worn(p, right);
                process_worn(p, left);
            }
        }
    }

    /* Refresh everybody's displays */
    for (i = 1; i <= NumPlayers; i++)
    {
        struct player *p = player_get(i);
        
        // pre-PWMA fix ///////////////
        if (p->upkeep->funeral) continue;
        //////////////////////////////

        /* Full refresh (includes monster/object lists) */
        p->full_refresh = true;

        /* Refresh */
        refresh_stuff(p);

        /* Normal refresh (without monster/object lists) */
        p->full_refresh = false;
    }

    /* Process extra stuff */
    for (i = 1; i <= NumPlayers; i++)
    {
        struct player *p = player_get(i);
        bool cannot_cast, cannot_cast_mimic, send = false;

        if (p->upkeep->funeral) continue;

        cannot_cast = player_cannot_cast(p, false);
        if (cannot_cast != p->cannot_cast)
        {
            p->cannot_cast = cannot_cast;
            send = true;
        }
        cannot_cast_mimic = player_cannot_cast_mimic(p, false);
        if (cannot_cast_mimic != p->cannot_cast_mimic)
        {
            p->cannot_cast_mimic = cannot_cast_mimic;
            send = true;
        }
        if (send) Send_extra(p);
    }

    /* Send any information over the network */
    Net_output();

    /* Get rid of dead players */
    for (i = NumPlayers; i > 0; i--)
    {
        struct player *p = player_get(i);
        char buf[MSG_LEN];
        connection_t *connp = get_connection(p->conn);

        if (!p->upkeep->funeral) continue;
        if (connp->state == CONN_QUIT) continue;

        /* Format string */
        if (!p->alive)
        {
            if (streq(p->died_from, "divine wrath"))
                my_strcpy(buf, "Killed by divine wrath", sizeof(buf));
            else
                my_strcpy(buf, "Retired", sizeof(buf));
        }
        else if (p->ghost)
            strnfmt(buf, sizeof(buf), "Destroyed by %s", p->died_from);
        else
            strnfmt(buf, sizeof(buf), "Killed by %s", p->died_from);

        /* Get rid of him */
        /*Destroy_connection(p->conn, buf);*/
        connp->quit_msg = string_make(buf);
        Conn_set_state(connp, CONN_QUIT, 1);
    }

    /* Kick out fainting players */
    for (i = NumPlayers; i > 0; i--)
    {
        struct player *p = player_get(i);

        if (!p->fainting) continue;

        /* Kick him */
        Destroy_connection(p->conn, "Fainting!");
    }

    on_leave_level();

    /* Make a new level if requested */
    for (i = 1; i <= NumPlayers; i++)
    {
        struct player *p = player_get(i);

        if (p->upkeep->new_level_method) generate_new_level(p);
    }
}


/*
 * Shimmer multi-hued things every ten game turns.
 *
 * Called in turn-based mode when the player is idle.
 */
static void process_player_shimmer(struct player *p)
{
    struct chunk *c = chunk_get(&p->wpos);
    static uint8_t loop = 0;
    int i;

    /* Every 10 game turns */
    loop++;
    if (loop < 10) return;
    loop = 0;

    /* Flicker self if multi-hued */
    if (p->poly_race && monster_shimmer(p->poly_race))
        square_light_spot_aux(p, c, &p->grid);

    /* Shimmer multi-hued objects */
    if (allow_shimmer(p)) shimmer_objects(p, c);

    /* Efficiency */
    if (!c->scan_monsters) return;

    /* Shimmer multi-hued monsters */
    for (i = 1; i < cave_monster_max(c); i++)
    {
        struct monster *mon = cave_monster(c, i);

        /* Light that spot */
        if (mon->race && monster_shimmer(mon->race))
            square_light_spot_aux(p, c, &mon->grid);
    }
}


/*
 * HIGHLY EXPERIMENTAL: turn-based mode (for single player games)
 *
 * Process player commands from the command queue, finishing when there is a
 * command using energy (any regular game command), or we run out of commands
 * and need another from the user, or the character changes level or dies, or
 * the game is stopped.
 */
static void process_player_turn_based(struct player *p)
{
    int time = move_energy(p->wpos.depth) / (10 * time_factor(p, chunk_get(&p->wpos)));

    /* Timeout various things */
    if (!(turn.turn % time)) decrease_timeouts(p, chunk_get(&p->wpos));

    /* Try to execute any commands on the command queue. */
    /* NB: process_pending_commands may have deleted the connection! */
    if (process_pending_commands(p->conn)) return;

    /* Shimmer multi-hued things if idle */
    if (!p->upkeep->new_level_method && monster_allow_shimmer(p) && has_energy(p, false))
        process_player_shimmer(p);

    /* Process the player until they use some energy */
    if (has_energy(p, false)) return;

    if (!p->upkeep->new_level_method && !p->upkeep->funeral)
        process_player_cleanup(p);
}


/*
 * The main game loop.
 *
 * This function will not exit until the level is completed,
 * the user dies, or the game is terminated.
 *
 * This is called every frame (1/FPS seconds).
 * A character will get frame_energy(speed) energy every frame and will be able to act once
 * move_energy(depth) energy has been accumulated. With a FPS of 75, a normal unhasted unburdened
 * character gets 1 player turn at 0' every (75 * 5 * 100) / (10 * 100 * 75) = 0.5 second.
 * Energy required will be doubled at 3000' and tripled at 4950', which means that a character
 * will need to be "Fast (+10)" and "Fast (+20)" at these depths to act every 0.5 second.
 *
 * Note that we process every player and the monsters, then quit. The "scheduling" code
 * (see sched.c) is the REAL main loop, which handles various inputs and timings.
 */
void run_game_loop(void)
{
    int i;

    /* HIGHLY EXPERIMENTAL: turn-based mode (for single player games) */
    if (TURN_BASED && process_turn_based())
    {
        struct player *p = player_get(1);

        /* Execute pre-turn processing if the player is not idle */
        if (ht_zero(&p->idle_turn)) pre_turn_game_loop();

        /* Process the player */
        if (!p->upkeep->new_level_method && !p->upkeep->funeral)
            process_player_turn_based(p);

        /* Execute post-turn processing if the player used some energy */
        if (!has_energy(p, false)) post_turn_game_loop();

        /* Player is idle: refresh and send info to client (for commands that don't use energy) */
        else
        {
            if (ht_zero(&p->idle_turn)) ht_copy(&p->idle_turn, &turn);

            /* Full refresh (includes monster/object lists) */
            p->full_refresh = true;

            /* Refresh */
            refresh_stuff(p);

            /* Normal refresh (without monster/object lists) */
            p->full_refresh = false;

            /* Send any information over the network */
            Net_output_p(p);
        }

        return;
    }

    /* Execute pre-turn processing */
    pre_turn_game_loop();

    /* Process the players */
    for (i = 1; i <= NumPlayers; i++)
    {
        struct player *p = player_get(i);

        /* Process that player */
        if (!p->upkeep->new_level_method && !p->upkeep->funeral) process_player(p);
    }

    /* Execute post-turn processing */
    post_turn_game_loop();
}


/*
 * Change a player into a King!
 */
void kingly(struct player *p)
{
    /* Fake death */
    my_strcpy(p->death_info.died_from, "winner", sizeof(p->death_info.died_from));

    /* Restore the experience */
    p->exp = p->max_exp;

    /* Restore the level */
    p->lev = p->max_lev;

    /* Hack -- player gets an XP bonus for beating the game */
    p->exp = p->max_exp += 10000000L;

    /* Hack -- ensure we are retired */
    p->retire_timer = 0;
}


bool level_keep_allocated(struct chunk *c)
{
    /* Don't deallocate levels which contain players */
    // note: it doesn't count DM! When testing - use regular char
    if (chunk_has_players(&c->wpos)) return true;

    /* Don't deallocate special levels */
    if (special_level(&c->wpos)) return true;

    /* Hack -- don't deallocate levels which contain owned houses */
    return level_has_owned_houses(&c->wpos);

    /* Hack -- don't deallocate levels which contain owned houses */
    // as we don't want to remove walls of unowned custom houses
    // return level_has_any_houses(&c->wpos);
}


#ifndef WINDOWS
static void signals_ignore_tstp(void);
static void signals_handle_tstp(void);
#endif


/*
 * Save the game
 *
 * PWMAngband: this should never be called, normal save is handled by shutdown_server() and
 * abnormal save is handled by exit_game_panic().
 */
static void save_game(struct player *p)
{
    /* Disturb the player */
    disturb(p, 0);

    /* Clear messages */
    message_flush(p);

    /* Handle stuff */
    handle_stuff(p);

    /* Message */
    msg(p, "Saving game...");

    /* The player is not dead */
    my_strcpy(p->died_from, "(saved)", sizeof(p->died_from));

#ifndef WINDOWS
    /* Forbid suspend */
    signals_ignore_tstp();
#endif

    /* Save the player */
    if (save_player(p, false))
        msg(p, "Saving game... done.");

    /* Save failed (oops) */
    else
        msg(p, "Saving game... failed!");

#ifndef WINDOWS
    /* Allow suspend again */
    signals_handle_tstp();
#endif

    /* Note that the player is not dead */
    my_strcpy(p->died_from, "(alive and well)", sizeof(p->died_from));
}


/*
 * Preserve artifacts on the ground.
 */
static void preserve_artifacts(void)
{
    int i;
    struct loc grid;

    for (grid.y = radius_wild; grid.y >= 0 - radius_wild; grid.y--)
    {
        for (grid.x = 0 - radius_wild; grid.x <= radius_wild; grid.x++)
        {
            struct wild_type *w_ptr = get_wt_info_at(&grid);

            for (i = 0; i <= w_ptr->max_depth - w_ptr->min_depth; i++)
            {
                struct chunk *c = w_ptr->chunk_list[i];
                struct object *obj;
                struct loc begin, end;
                struct loc_iterator iter;

                /* Don't deallocate special levels */
                if (!c || level_keep_allocated(c)) continue;

                loc_init(&begin, 0, 0);
                loc_init(&end, c->width, c->height);
                loc_iterator_first(&iter, &begin, &end);

                do
                {
                    for (obj = square_object(c, &iter.cur); obj; obj = obj->next)
                    {
                        /* Hack -- preserve artifacts */
                        if (obj->artifact)
                        {
                            /* Only works when owner is ingame */
                            struct player *p = player_get(get_owner_id(obj));

                            /* Mark artifact as abandoned */
                            set_artifact_info(p, obj, ARTS_ABANDONED);

                            /* Preserve any artifact */
                            preserve_artifact_aux(obj);
                        }
                    }
                }
                while (loc_iterator_next_strict(&iter));
            }
        }
    }
}


/*
 * Close up the current game (player may or may not be dead)
 *
 * PWMAngband: this should never be called, normal save is handled by shutdown_server() and
 * abnormal save is handled by exit_game_panic().
 */
static void close_game(void)
{
    int i;

#ifndef WINDOWS
    /* No suspending now */
    signals_ignore_tstp();
#endif

    for (i = 1; i <= NumPlayers; i++)
    {
        struct player *p = player_get(i);

        /* Handle stuff */
        handle_stuff(p);

        /* Flush the messages */
        message_flush(p);

        /* Handle death */
        if (p->is_dead)
        {
            /* Handle retirement */
            if (p->total_winner) kingly(p);

            /* Save memories */
            if (!save_player(p, false)) msg(p, "death save failed!");

            /* Handle score, show Top scores */
            enter_score(p, &p->death_info.time);
        }

        /* Still alive */
        else
        {
            /* Save the game */
            save_game(p);
        }
    }

    /* Preserve artifacts on the ground */
    preserve_artifacts();

    /* Try to save the server information + player names */
    save_server_info(false);
    save_account_info(false);

#ifndef WINDOWS
    /* Allow suspending now */
    signals_handle_tstp();
#endif
}


/*
 * Actually play a game.
 */
void play_game(void)
{
    /* Start server initialization */
    server_generated = false;

    /* Flash a message */
    plog("Please wait...");

    /* Attempt to load the server state information + player names */
    if (!load_server_info())
        quit("Broken server savefile");
    if (!load_account_info())
        quit("Broken player names savefile");

    /* Initialize server state information */
    if (!server_state_loaded) server_birth();

    plog("Object flavors initialized...");

    /* Initialize visual prefs */
    textui_prefs_init();

    /* Initialize graphics info and basic user pref data */
    if (cfg_load_pref_file)
    {
        plog_fmt("Loading pref file: %s", cfg_load_pref_file);
        process_pref_file(cfg_load_pref_file, false);
    }

    /* Server initialization is now "complete" */
    server_generated = true;

    /* Set up the contact socket, so we can allow players to connect */
    setup_contact_socket();

    /* Set up the network server */
    if (Setup_net_server() == -1)
        quit("Couldn't set up net server");

    /* Set up the main loop */
    install_timer_tick(run_game_loop, cfg_fps);

    /* Loop forever */
    sched();

    /* This should never, ever happen */
    plog("FATAL ERROR sched() returned!");

    /* Close stuff */
    close_game();

    /* Quit */
    quit(NULL);
}


/*
 * Shut down the server
 *
 * This function is called when the server is closed, or from the PWMAngband console.
 *
 * In here we try to save everybody's game, as well as save the server state.
 */
void shutdown_server(void)
{
    plog("Shutting down.");

    /* Stop the main loop */
    remove_timer_tick();

    /* Kick every player out and save his game */
    while (NumPlayers > 0)
    {
        /* Note that we always save the first player */
        struct player *p = player_get(1);

        /* Indicate cause */
        my_strcpy(p->died_from, "server shutdown", sizeof(p->died_from));

        /* Try to save */
        if (!save_player(p, false)) Destroy_connection(p->conn, "Server shutdown (save failed)");

        /* Successful save */
        Destroy_connection(p->conn, "Server shutdown (save succeeded)");
    }

    /* Preserve artifacts on the ground */
    preserve_artifacts();

    /* Try to save the server information + player names */
    if (!save_server_info(false)) plog("Server state save failed!");

    /* Successful save of server info */
    else plog("Server state save succeeded!");

    if (!save_account_info(false)) plog("Account info save failed!");
    else plog("Account info save succeeded!");

    /* Don't re-enter */
    server_generated = false;

    /* Tell the metaserver that we're gone */
    Report_to_meta(META_DIE);

    /* Quit normally */
    quit(NULL);
}


/*
 * Handle a fatal crash.
 *
 * Here we try to save every player's state, and the state of the server
 * in general. Note that we must be extremely careful not to have a crash
 * in this function, or some things may not get saved. Also, this function
 * may get called because some data structures are not in a "correct" state.
 * For this reason many paranoia checks are done to prevent bad pointer
 * dereferences.
 *
 * Note that this function would not be needed at all if there were no bugs.
 */
void exit_game_panic(void)
{
    int i = 1;

    /* If nothing important has happened, just return */
    if (!server_generated) return;

    plog("Shutting down (panic save).");

    /* Kick every player out and save his game */
    while (NumPlayers > (i - 1))
    {
        struct player *p = player_get(i);

        /* Don't dereference bad pointers */
        if (!p)
        {
            /* Skip to next player */
            i++;

            continue;
        }

        /* Hack -- turn off some things */
        disturb(p, 0);

        /* Hack -- delay death */
        if (p->chp < 0) p->is_dead = false;

#ifndef WINDOWS
        /* Forbid suspend */
        signals_ignore_tstp();
#endif

        /* Indicate panic save */
        my_strcpy(p->died_from, "(panic save)", sizeof(p->died_from));

        /* Hack -- unstatic if the DM left while manually designing a dungeon level */
        if (chunk_inhibit_players(&p->wpos)) chunk_set_player_count(&p->wpos, 0);

        /*
         * Try to save the player, don't worry if this fails because there
         * is nothing we can do now anyway
         */
        save_player(p, true);
        i++;
    }

    /* Preserve artifacts on the ground */
    preserve_artifacts();

    /* Try to save the server information + player names */
    if (!save_server_info(true)) plog("Server panic info save failed!");

    /* Successful panic save of server info */
    else plog("Server panic info save succeeded!");

    if (!save_account_info(true)) plog("Account panic info save failed!");
    else plog("Account panic info save succeeded!");

    /* Don't re-enter */
    server_generated = false;

    /* Free resources */
    cleanup_angband();
}


#ifdef WINDOWS
/*
 * Windows specific replacement for signal handling
 */
static LPTOP_LEVEL_EXCEPTION_FILTER old_handler;


/*
 * Callback to be called by Windows when our term closes, the user
 * logs off, the system is shutdown, etc.
 */
static BOOL ctrl_handler(DWORD fdwCtrlType)
{
    /* Save everything and quit the game */
    shutdown_server();
    return TRUE;
}


/*
 * Global unhandled exception handler
 *
 * If the server crashes under Windows, this is where we end up
 */
static LONG WINAPI MyUnhandledExceptionFilter(struct _EXCEPTION_POINTERS* ExceptionInfo)
{
    /*
     * We don't report to the meta server in this case, the meta
     * server will detect that we've gone anyway
     */

    /*
     * Call the previous exception handler, which we are assuming
     * is the MinGW exception handler which should have been implicitly
     * setup when we loaded the exchndl.dll library.
     */
    if (old_handler != NULL) old_handler(ExceptionInfo);

    /* Record the crash */
    if (server_generated)
    {
        plog_fmt("Exception 0x%lX at 0x%lX", ExceptionInfo->ExceptionRecord->ExceptionCode,
            (unsigned long)ExceptionInfo->ExceptionRecord->ExceptionAddress);
    }

    /* Save everything and quit the game */
    exit_game_panic();

    /* We don't expect to ever get here... but for what it's worth... */
    return (EXCEPTION_EXECUTE_HANDLER);
}


void setup_exit_handler(void)
{
    /* Trap CTRL+C, Logoff, Shutdown, etc */
    if (SetConsoleCtrlHandler((PHANDLER_ROUTINE)ctrl_handler, true))
        plog("Initialised exit save handler.");
    else
        plog("ERROR: Could not set panic save handler!");

    /* Trap unhandled exceptions, i.e. server crashes */
    old_handler = SetUnhandledExceptionFilter(MyUnhandledExceptionFilter);
}
#else
#include <signal.h>
static volatile sig_atomic_t signalbusy = 0;

static int16_t signal_count = 0;   /* Hack -- count interrupts */

#ifdef SIGTSTP
/*
 * Handle signals -- suspend
 *
 * Actually suspend the game, and then resume cleanly
 *
 * This will probably inflict much anger upon the suspender, but it is still
 * allowed (for now)
 */
static void handle_signal_suspend(int sig)
{
    /* Disable handler */
    signal(sig, SIG_IGN);

#ifdef SIGSTOP
    /* Suspend ourself */
    kill(0, SIGSTOP);
#endif
    /* Restore handler */
    signal(sig, handle_signal_suspend);
}
#endif

/*
 * Handle signals -- simple (interrupt and quit)
 *
 * This function was causing a *huge* number of problems, so it has
 * been simplified greatly. We keep a global variable which counts
 * the number of times the user attempts to kill the process, and
 * we commit suicide if the user does this a certain number of times.
 *
 * We attempt to give "feedback" to the user as he approaches the
 * suicide thresh-hold, but without penalizing accidental keypresses.
 *
 * To prevent messy accidents, we should reset this global variable
 * whenever the user enters a keypress, or something like that.
 *
 * This simply calls "exit_game_panic()", which should try to save
 * everyone's character and the server info, which is probably nicer
 * than killing everybody.
 */
static void handle_signal_simple(int sig)
{
    char msg[48];

    /* Disable handler */
    signal(sig, SIG_IGN);

    /* Construct the exit message in case it is needed */
    strnfmt(msg, sizeof(msg), "Exiting on signal %d!", sig);

    /* Nothing to save, just quit */
    if (!server_generated) quit(msg);

    /* Hack -- on SIGTERM, quit right away */
    if (sig == SIGTERM) signal_count = 5;

    /* Count the signals */
    signal_count++;

    /* Allow suicide (after 5) */
    if (signal_count >= 5)
    {
        /* Perform a "clean" shutdown */
        shutdown_server();
    }

    /* Give warning (after 4) */
    else if (signal_count >= 4)
        plog("Warning: Next signal kills server!");

    /* Restore handler */
    signal(sig, handle_signal_simple);
}

/*
 * Handle signal -- abort, kill, etc
 *
 * This one also calls exit_game_panic()
 */
static void handle_signal_abort(int sig)
{
    char msg[48];

    /* We are *not* reentrant */
    if (signalbusy) raise(sig);
    signalbusy = 1;

    plog("Unexpected signal, panic saving.");

    /* Construct the exit message */
    strnfmt(msg, sizeof(msg), "Exiting on signal %d!", sig);

    /* Nothing to save, just quit */
    if (!server_generated) quit(msg);

    /* Save everybody */
    exit_game_panic();

    /* Enable default handler */
    signal(sig, SIG_DFL);

    /* Reraise */
    raise(sig);
}

/*
 * Ignore SIGTSTP signals (keyboard suspend)
 */
static void signals_ignore_tstp(void)
{
#ifdef SIGTSTP
    signal(SIGTSTP, SIG_IGN);
#endif
}

/*
 * Handle SIGTSTP signals (keyboard suspend)
 */
static void signals_handle_tstp(void)
{
#ifdef SIGTSTP
    signal(SIGTSTP, handle_signal_suspend);
#endif
}

/*
 * Prepare to handle the relevant signals
 */
void signals_init(void)
{
#ifdef SIGHUP
    signal(SIGHUP, SIG_IGN);
#endif

#ifdef SIGTSTP
    signal(SIGTSTP, handle_signal_suspend);
#endif

#ifdef SIGINT
    signal(SIGINT, handle_signal_simple);
#endif

#ifdef SIGQUIT
    signal(SIGQUIT, handle_signal_simple);
#endif

#ifdef SIGFPE
    signal(SIGFPE, handle_signal_abort);
#endif

#ifdef SIGILL
    signal(SIGILL, handle_signal_abort);
#endif

#ifdef SIGTRAP
    signal(SIGTRAP, handle_signal_abort);
#endif

#ifdef SIGIOT
    signal(SIGIOT, handle_signal_abort);
#endif

#ifdef SIGKILL
    signal(SIGKILL, handle_signal_abort);
#endif

#ifdef SIGBUS
    signal(SIGBUS, handle_signal_abort);
#endif

#ifdef SIGSEGV
    signal(SIGSEGV, handle_signal_abort);
#endif

#ifdef SIGTERM
    signal(SIGTERM, handle_signal_simple);
#endif

#ifdef SIGPIPE
    signal(SIGPIPE, SIG_IGN);
#endif

#ifdef SIGEMT
    signal(SIGEMT, handle_signal_abort);
#endif

#ifdef SIGDANGER
    signal(SIGDANGER, handle_signal_abort);
#endif

#ifdef SIGSYS
    signal(SIGSYS, handle_signal_abort);
#endif

#ifdef SIGXCPU
    signal(SIGXCPU, handle_signal_abort);
#endif

#ifdef SIGPWR
    signal(SIGPWR, handle_signal_abort);
#endif
}
#endif
