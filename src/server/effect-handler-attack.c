/*
 * File: effect-handler-attack.c
 * Purpose: Handler functions for attack effects
 *
 * Copyright (c) 2007 Andi Sidwell
 * Copyright (c) 2016 Ben Semmler, Nick McConnell
 * Copyright (c) 2021 MAngband and PWMAngband Developers
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


static void get_target(struct chunk *c, struct source *origin, int dir, struct loc *grid)
{
    /* MvX */
    if (origin->monster)
    {
        int accuracy = monster_effect_accuracy(origin->monster, MON_TMD_CONF, CONF_RANDOM_CHANCE);

        if (randint1(100) > accuracy)
        {
            dir = ddd[randint0(8)];
            next_grid(grid, &origin->monster->grid, dir);
        }
        else
        {
            if (monster_is_decoyed(c, origin->monster))
                loc_copy(grid, cave_find_decoy(c));
            else
                loc_copy(grid, &origin->player->grid);
        }
    }

    /* Ask for a target if no direction given */
    else if ((dir == DIR_TARGET) && target_okay(origin->player))
        target_get(origin->player, grid);

    /* Use the adjacent grid in the given direction as target */
    else
        next_grid(grid, &origin->player->grid, dir);
}


/*
 * Apply the project() function in a direction, or at a target
 */
bool project_aimed(struct source *origin, int typ, int dir, int dam, int flg, const char *what)
{
    struct chunk *c = chunk_get(&origin->player->wpos);
    struct source who_body;
    struct source *who = &who_body;
    struct loc grid;

    /* Pass through the target if needed */
    flg |= (PROJECT_THRU);

    /* Ensure "dir" is in ddx/ddy array bounds */
    if (!VALID_DIR(dir)) return false;

    get_target(c, origin, dir, &grid);

    /* Hack -- only one source */
    if (origin->monster)
        source_monster(who, origin->monster);
    else
        source_player(who, get_player_index(get_connection(origin->player->conn)), origin->player);

    /* Aim at the target, do NOT explode */
    return (project(who, 0, c, &grid, dam, typ, flg, 0, 0, what));
}


/*
 * Apply the project() function to grids around the player
 */
static bool project_touch(struct player *p, int dam, int rad, int typ, bool aware,
    struct monster *mon)
{
    struct loc pgrid;
    int flg = PROJECT_GRID | PROJECT_ITEM | PROJECT_KILL | PROJECT_PLAY | PROJECT_HIDE | PROJECT_THRU;
    struct source who_body;
    struct source *who = &who_body;
    struct chunk *c = chunk_get(&p->wpos);

    if (mon && monster_is_decoyed(c, mon))
    {
        loc_copy(&pgrid, cave_find_decoy(c));
        flg |= PROJECT_JUMP;
    }
    else
        loc_copy(&pgrid, &p->grid);
    source_player(who, get_player_index(get_connection(p->conn)), p);

    if (aware) flg |= PROJECT_AWARE;
    return (project(who, rad, c, &pgrid, dam, typ, flg, 0, 0, "killed"));
}


/*
 * Apply a "project()" directly to all viewable monsters
 */
bool project_los(effect_handler_context_t *context, int typ, int dam, bool obvious)
{
    int i;
    struct loc origin;
    int flg = PROJECT_JUMP | PROJECT_KILL | PROJECT_PLAY | PROJECT_HIDE;
    struct source who_body;
    struct source *who = &who_body;
    struct chunk *c = context->cave;
    struct player *p = context->origin->player;

    origin_get_loc(&origin, context->origin);

    if (context->origin->monster)
        source_monster(who, context->origin->monster);
    else
        source_player(who, get_player_index(get_connection(p->conn)), p);

    if (obvious) flg |= PROJECT_AWARE;

    p->current_sound = -2;

    /* Affect all (nearby) monsters */
    for (i = 1; i < cave_monster_max(c); i++)
    {
        struct monster *mon = cave_monster(c, i);

        /* Paranoia -- skip dead monsters */
        if (!mon->race) continue;

        /* Don't affect the caster */
        if (context->origin->monster && (context->origin->monster == mon)) continue;

        /* Require line of sight */
        if (!los(c, &origin, &mon->grid)) continue;

        /* Jump directly to the monster */
        if (project(who, 0, c, &mon->grid, dam, typ, flg, 0, 0, "killed"))
            obvious = true;
    }

    /* Affect all (nearby) players */
    for (i = 1; i <= NumPlayers; i++)
    {
        struct player *q = player_get(i);

        /* Skip the dungeon master if hidden */
        if (q->dm_flags & DM_SECRET_PRESENCE) continue;

        /* Skip players not on this level */
        if (!wpos_eq(&q->wpos, &p->wpos)) continue;

        /* Skip ourself */
        if (q == p) continue;

        /* Require line of sight */
        if (!los(c, &origin, &q->grid)) continue;

        /* Jump directly to the player */
        if (project(who, 0, c, &q->grid, dam, typ, flg, 0, 0, "killed"))
            obvious = true;
    }

    p->current_sound = -1;

    /* Result */
    return (obvious);
}


/*
 * Cast a beam spell
 * Pass through monsters, as a "beam"
 * Affect monsters (not grids or objects)
 */
static bool fire_beam(struct source *origin, int typ, int dir, int dam, bool obvious)
{
    bool result;
    int flg = PROJECT_BEAM | PROJECT_KILL | PROJECT_PLAY;

    if (obvious) flg |= PROJECT_AWARE;
    origin->player->current_sound = -2;
    result = project_aimed(origin, typ, dir, dam, flg, "annihilated");
    origin->player->current_sound = -1;
    return result;
}


static bool light_line_aux(struct source *origin, int dir, int typ, int dam)
{
    bool result;
    int flg = PROJECT_BEAM | PROJECT_GRID | PROJECT_KILL | PROJECT_PLAY;

    origin->player->current_sound = -2;
    result = project_aimed(origin, typ, dir, dam, flg, "killed");
    origin->player->current_sound = -1;
    return result;
}


/*
 * Cast a bolt spell
 * Stop if we hit a monster, as a "bolt"
 * Affect monsters (not grids or objects)
 */
bool fire_bolt(struct source *origin, int typ, int dir, int dam, bool obvious)
{
    int flg = PROJECT_STOP | PROJECT_KILL | PROJECT_PLAY;

    if (obvious) flg |= PROJECT_AWARE;
    return (project_aimed(origin, typ, dir, dam, flg, "annihilated"));
}


/*
 * Cast a ball spell
 * Stop if we hit a monster, act as a "ball"
 * Allow "target" mode to pass over monsters
 * Affect grids, objects, and monsters
 */
bool fire_ball(struct player *p, int typ, int dir, int dam, int rad, bool obvious, bool constant)
{
    struct loc target;
    bool result;
    int flg = PROJECT_THRU | PROJECT_STOP | PROJECT_GRID | PROJECT_ITEM | PROJECT_KILL | PROJECT_PLAY;
    struct source who_body;
    struct source *who = &who_body;

    source_player(who, get_player_index(get_connection(p->conn)), p);

    if (obvious) flg |= PROJECT_AWARE;

    /* Hack -- heal self && blasts */
    if ((typ == PROJ_MON_HEAL) || constant) flg |= PROJECT_CONST;

    /* Ensure "dir" is in ddx/ddy array bounds */
    if (!VALID_DIR(dir)) return false;

    /* Use the given direction */
    next_grid(&target, &p->grid, dir);

    /* Hack -- use an actual "target" */
    if ((dir == DIR_TARGET) && target_okay(p))
    {
        flg &= ~(PROJECT_STOP | PROJECT_THRU);
        target_get(p, &target);
    }

    /* Analyze the "dir" and the "target".  Hurt items on floor. */
    p->current_sound = -2;
    result = project(who, rad, chunk_get(&p->wpos), &target, dam, typ, flg, 0, 0, "annihilated");
    p->current_sound = -1;
    return result;
}


static bool handler_breath(effect_handler_context_t *context, bool use_boost)
{
    int dam = effect_calculate_value(context, use_boost);
    int type = context->subtype;
    struct loc target;
    struct source who_body;
    struct source *who = &who_body;

    /*
     * Diameter of source starts at 4, so full strength up to 3 grids from
     * the breather.
     */
    int diameter_of_source = 4;

    /* Minimum breath width is 20 degrees */
    int degrees_of_arc = MAX(context->other, 20);

    int flg = PROJECT_ARC | PROJECT_GRID | PROJECT_ITEM | PROJECT_KILL | PROJECT_PLAY;

    /* Distance breathed has no fixed limit. */
    int rad = z_info->max_range;

    /* Hack -- already used up */
    bool used = (context->radius == 1);

    /* Ensure "dir" is in ddx/ddy array bounds */
    if (!VALID_DIR(context->dir)) return false;

    /* Player or monster? */
    if (context->origin->monster)
    {
        int accuracy = monster_effect_accuracy(context->origin->monster, MON_TMD_CONF,
            CONF_RANDOM_CHANCE);

        source_monster(who, context->origin->monster);

        /* Breath parameters for monsters are monster-dependent */
        dam = breath_dam(type, context->origin->monster->hp);

        /* Powerful monster */
        if (monster_is_powerful(context->origin->monster->race))
        {
            /* Breath is now full strength at 5 grids */
            diameter_of_source *= 3;
            diameter_of_source /= 2;
        }

        /* Target player or monster? */
        if (randint1(100) > accuracy)
        {
            /* Confused direction. */
            int dir = ddd[randint0(8)];

            next_grid(&target, &context->origin->monster->grid, dir);
        }
        else if (context->target_mon)
        {
            /* Target monster. */
            loc_copy(&target, &context->target_mon->grid);
        }
        else
        {
            /* Target player. */
            if (monster_is_decoyed(context->cave, context->origin->monster))
                loc_copy(&target, cave_find_decoy(context->cave));
            else
                loc_copy(&target, &context->origin->player->grid);
            who->target = context->origin->player;
        }
    }
    else
    {
        /* PWMAngband: let Power Dragon Scale Mails breathe a random element */
        if (type == PROJ_MISSILE)
        {
            bitflag mon_breath[RSF_SIZE];

            /* Allow all elements */
            rsf_wipe(mon_breath);
            init_spells(mon_breath);
            set_breath(mon_breath);

            /* Get breath effect */
            type = breath_effect(context->origin->player, mon_breath);
        }

        /* Handle polymorphed players */
        else if (context->origin->player->poly_race && (dam == 0))
        {
            const char *pself = player_self(context->origin->player);
            char df[160];

            /* Damage */
            dam = breath_dam(type, context->origin->player->chp);

            /* Boost damage to take into account player hp vs monster hp */
            dam = (dam * 6) / 5;

            /* Breathing damages health instead of costing mana */
            strnfmt(df, sizeof(df), "exhausted %s with breathing", pself);
            take_hit(context->origin->player, context->origin->player->mhp / 20,
                "the strain of breathing", false, df);
            if (context->origin->player->is_dead) return !used;

            /* Breathing also consumes food */
            if (!context->origin->player->ghost)
                player_dec_timed(context->origin->player, TMD_FOOD, 50, false);

            /* Powerful breath */
            if (monster_is_powerful(context->origin->player->poly_race))
            {
                diameter_of_source *= 3;
                diameter_of_source /= 2;
            }
        }

        source_player(who, get_player_index(get_connection(context->origin->player->conn)),
            context->origin->player);

        /* Ask for a target if no direction given */
        if ((context->dir == DIR_TARGET) && target_okay(context->origin->player))
            target_get(context->origin->player, &target);
        else
        {
            /* Hack -- no target available, default to random direction */
            if (context->dir == DIR_TARGET) context->dir = 0;

            /* Hack -- no direction given, default to random direction */
            if (!context->dir) context->dir = ddd[randint0(8)];

            /* Use the given direction */
            next_grid(&target, &context->origin->player->grid, context->dir);
        }
    }

    /* Adjust the diameter of the energy source */
    if (degrees_of_arc < 60)
    {
        /*
         * Narrower cone means energy drops off less quickly. We now have:
         * - 30 degree regular breath  | full strength at 7 grids
         * - 30 degree powerful breath | full strength at 11 grids
         * - 20 degree regular breath  | full strength at 11 grids
         * - 20 degree powerful breath | full strength at 17 grids
         * where grids are measured from the breather.
         */
        diameter_of_source = diameter_of_source * 60 / degrees_of_arc;

        /* Max */
        if (diameter_of_source > 25) diameter_of_source = 25;
    }

    /* Breathe at the target */
    context->origin->player->current_sound = -2;
    if (project(who, rad, context->cave, &target, dam, type, flg, degrees_of_arc, diameter_of_source,
        "vaporized"))
    {
        context->ident = true;
    }
    context->origin->player->current_sound = -1;

    return !used;
}


/* Helper for destruction and wipe effects */
static int wreck_havoc(effect_handler_context_t *context, int r, int *hurt, bool wipe)
{
    struct loc begin, end;
    struct loc_iterator iter;
    int k, count = 0;

    loc_init(&begin, context->origin->player->grid.x - r, context->origin->player->grid.y - r);
    loc_init(&end, context->origin->player->grid.x + r, context->origin->player->grid.y + r);
    loc_iterator_first(&iter, &begin, &end);

    /* Big area of affect */
    do
    {
        /* Skip illegal grids */
        if (!square_in_bounds_fully(context->cave, &iter.cur)) continue;

        /* Extract the distance */
        k = distance(&context->origin->player->grid, &iter.cur);

        /* Stay in the circle of death */
        if (k > r) continue;

        /* Lose room and vault */
        sqinfo_off(square(context->cave, &iter.cur)->info, SQUARE_ROOM);
        sqinfo_off(square(context->cave, &iter.cur)->info, SQUARE_VAULT);
        sqinfo_off(square(context->cave, &iter.cur)->info, SQUARE_NO_TELEPORT);
        sqinfo_off(square(context->cave, &iter.cur)->info, SQUARE_LIMITED_TELE);
        if (square_ispitfloor(context->cave, &iter.cur))
            square_clear_feat(context->cave, &iter.cur);

        /* Forget completely */
        square_unglow(context->cave, &iter.cur);
        square_forget_all(context->cave, &iter.cur);
        square_light_spot(context->cave, &iter.cur);

        /* Hack -- notice player affect */
        if (square(context->cave, &iter.cur)->mon < 0)
        {
            /* Hurt the player later */
            hurt[count] = 0 - square(context->cave, &iter.cur)->mon;
            count++;

            /* Do not hurt this grid */
            continue;
        }

        /* Hack -- skip the epicenter */
        if (loc_eq(&iter.cur, &context->origin->player->grid)) continue;

        /* Delete the monster (if any) */
        delete_monster(context->cave, &iter.cur);
        if (square_ispitfloor(context->cave, &iter.cur))
            square_clear_feat(context->cave, &iter.cur);

        /* Don't remove stairs */
        if (square_isstairs(context->cave, &iter.cur)) continue;

        /* Destroy any grid that isn't a permanent wall */
        if (!square_isunpassable(context->cave, &iter.cur))
        {
            /* Delete objects */
            square_forget_pile_all(context->cave, &iter.cur);
            square_excise_pile(context->cave, &iter.cur);
            if (wipe) square_clear_feat(context->cave, &iter.cur);
            else square_destroy(context->cave, &iter.cur);
        }
    }
    while (loc_iterator_next(&iter));

    return count;
}


static bool py_attack_grid(struct player *p, struct chunk *c, struct loc *grid)
{
    struct source who_body;
    struct source *who = &who_body;
    int oldhp, newhp;

    square_actor(c, grid, who);

    /* Monster */
    if (who->monster)
    {
        /* Reveal mimics */
        if (monster_is_camouflaged(who->monster))
        {
            become_aware(p, c, who->monster);

            /* Mimic wakes up and becomes aware */
            if (pvm_check(p, who->monster))
                monster_wake(p, who->monster, false, 100);
        }

        oldhp = who->monster->hp;

        /* Attack */
        if (pvm_check(p, who->monster)) py_attack(p, c, grid);

        newhp = who->monster->hp;
    }

    /* Player */
    else if (who->player)
    {
        /* Reveal mimics */
        if (who->player->k_idx)
            aware_player(p, who->player);

        oldhp = who->player->chp;

        /* Attack */
        if (pvp_check(p, who->player, PVP_DIRECT, true, square(c, grid)->feat))
            py_attack(p, c, grid);

        newhp = who->player->chp;
    }

    /* Nobody */
    else
        return false;

    /* Lame test for hitting the target */
    return ((newhp > 0) && (newhp != oldhp));
}


static void heal_monster(struct player *p, struct monster *mon, struct source *origin, int amount)
{
    char m_name[NORMAL_WID], m_poss[NORMAL_WID];
    bool seen;

    /* Get the monster name (or "it") */
    monster_desc(p, m_name, sizeof(m_name), mon, MDESC_STANDARD);

    /* Get the monster possessive ("his"/"her"/"its") */
    monster_desc(p, m_poss, sizeof(m_poss), mon, MDESC_PRO_VIS | MDESC_POSS);

    seen = (!p->timed[TMD_BLIND] && monster_is_visible(p, mon->midx));

    /* Heal some */
    mon->hp += amount;

    /* Fully healed */
    if (mon->hp >= mon->maxhp)
    {
        mon->hp = mon->maxhp;

        if (seen)
            msg(p, "%s looks REALLY healthy!", m_name);
        else
            msg(p, "%s sounds REALLY healthy!", m_name);
    }

    /* Partially healed */
    else if (seen)
        msg(p, "%s looks healthier.", m_name);
    else
        msg(p, "%s sounds healthier.", m_name);

    /* Redraw (later) if needed */
    update_health(origin);

    /* Cancel fear */
    if (mon->m_timed[MON_TMD_FEAR])
    {
        mon_clear_timed(p, mon, MON_TMD_FEAR, MON_TMD_FLG_NOMESSAGE);
        msg(p, "%s recovers %s courage.", m_name, m_poss);
    }

    /* Cancel poison */
    if (mon->m_timed[MON_TMD_POIS])
    {
        mon_clear_timed(p, mon, MON_TMD_POIS, MON_TMD_FLG_NOMESSAGE);
        msg(p, "%s is no longer poisoned.", m_name);
    }  

    /* Cancel bleeding */
    if (mon->m_timed[MON_TMD_CUT])
    {
        mon_clear_timed(p, mon, MON_TMD_CUT, MON_TMD_FLG_NOMESSAGE);
        msg(p, "%s is no longer bleeding.", m_name);
    }
}


/*
 * Effect handlers
 */


/*
 * Cast an alter spell
 * Affect objects and grids (not monsters)
 */
bool effect_handler_ALTER(effect_handler_context_t *context)
{
    int flg = PROJECT_BEAM | PROJECT_GRID | PROJECT_ITEM;

    if (project_aimed(context->origin, context->subtype, context->dir, 0, flg, "killed"))
        context->ident = true;
    return true;
}


/*
 * Breathe an element, in a cone from the breath
 * Affect grids, objects, and monsters
 * context->subtype is element, context->other degrees of arc
 *
 * PWMAngband: if context->radius is set, object is already used up; use device boost
 */
bool effect_handler_ARC(effect_handler_context_t *context)
{
    return handler_breath(context, true);
}


/*
 * Cast a ball spell
 * Stop if we hit a monster or the player, act as a ball
 * Allow target mode to pass over monsters
 * Affect grids, objects, and monsters
 */
bool effect_handler_BALL(effect_handler_context_t *context)
{
    int dam = effect_calculate_value(context, true);
    int rad = (context->radius? context->radius: 2);
    struct loc target;
    struct source who_body;
    struct source *who = &who_body;
    int flg = PROJECT_THRU | PROJECT_STOP | PROJECT_GRID | PROJECT_ITEM | PROJECT_KILL | PROJECT_PLAY;
    const char *what = "annihilated";

    /* Ensure "dir" is in ddx/ddy array bounds */
    if (!VALID_DIR(context->dir)) return false;

    /* Player or monster? */
    if (context->origin->monster)
    {
        int accuracy = monster_effect_accuracy(context->origin->monster, MON_TMD_CONF,
            CONF_RANDOM_CHANCE);

        source_monster(who, context->origin->monster);

        /* Powerful monster */
        if (monster_is_powerful(context->origin->monster->race)) rad++;

        flg &= ~(PROJECT_STOP | PROJECT_THRU);

        /* Handle confusion */
        if (randint1(100) > accuracy)
        {
            int dir = ddd[randint0(8)];

            next_grid(&target, &context->origin->monster->grid, dir);
        }

        /* Target monster */
        else if (context->target_mon)
            loc_copy(&target, &context->target_mon->grid);

        /* Target player */
        else
        {
            if (monster_is_decoyed(context->cave, context->origin->monster))
                loc_copy(&target, cave_find_decoy(context->cave));
            else
                loc_copy(&target, &context->origin->player->grid);
            who->target = context->origin->player;
        }
    }
    else
    {
        struct trap *trap = context->origin->trap;

        if (trap)
        {
            loc_copy(&target, &trap->grid);
            source_trap(who, trap);
        }
        else
        {
            if (context->other) rad += context->origin->player->lev / context->other;

            /* Hack -- mimics */
            if (context->origin->player->poly_race &&
                monster_is_powerful(context->origin->player->poly_race))
            {
                rad++;
            }

            /* Hack -- elementalists */
            rad = rad + context->beam.spell_power / 2;
            rad = rad * (20 + context->beam.elem_power) / 20;

            source_player(who, get_player_index(get_connection(context->origin->player->conn)),
                context->origin->player);

            // Fireball spell (2 mana)
            if (streq(context->origin->player->clazz->name, "Wizard") && context->origin->player->spell_cost == 2)
            {
                // dmg
                if (dam > 10)
                    dam *= context->origin->player->lev / 10;
            }

            /* Ask for a target if no direction given */
            if ((context->dir == DIR_TARGET) && target_okay(context->origin->player))
            {
                flg &= ~(PROJECT_STOP | PROJECT_THRU);
                target_get(context->origin->player, &target);
            }

            /* Use the given direction */
            else
                next_grid(&target, &context->origin->player->grid, context->dir);
        }
    }

    /* Aim at the target, explode */
    context->origin->player->current_sound = -2;
    if (project(who, rad, context->cave, &target, dam, context->subtype, flg, 0, 0, what))
        context->ident = true;
    context->origin->player->current_sound = -1;

    return true;
}


/*
 * Cast a ball spell which effect is obvious.
 * If context->other is negative, allow only on random levels.
 */
bool effect_handler_BALL_OBVIOUS(effect_handler_context_t *context)
{
    int dam = effect_calculate_value(context, true);
    int rad = context->radius + ((context->other > 0)? (context->origin->player->lev / context->other): 0);

    /* Only on random levels */
    if ((context->other < 0) && !random_level(&context->origin->player->wpos))
    {
        msg(context->origin->player, "You cannot create traps here...");
        context->ident = true;
        return false;
    }

    if (fire_ball(context->origin->player, context->subtype, context->dir, dam, rad, true, false))
        context->ident = true;
    return true;
}


/*
 * Cast a beam spell
 * Pass through monsters, as a beam
 * Affect monsters (not grids or objects)
 */
bool effect_handler_BEAM(effect_handler_context_t *context)
{
    int dam = effect_calculate_value(context, true);

    fire_beam(context->origin, context->subtype, context->dir, dam, false);
    if (!context->origin->player->timed[TMD_BLIND]) context->ident = true;
    return true;
}


/*
 * Cast a beam spell which effect is obvious
 */
bool effect_handler_BEAM_OBVIOUS(effect_handler_context_t *context)
{
    int dam = effect_calculate_value(context, true);

    if (fire_beam(context->origin, context->subtype, context->dir, dam, true))
        context->ident = true;
    return true;
}


/*
 * Cast a ball spell centered on the character
 */
bool effect_handler_BLAST(effect_handler_context_t *context)
{
    int dam = effect_calculate_value(context, false);
    int rad = context->radius + (context->other? (context->origin->player->lev / context->other): 0);

    /* Hack -- elementalists */
    rad = rad + context->beam.spell_power / 2;
    rad = rad * (20 + context->beam.elem_power) / 20;

    if (fire_ball(context->origin->player, context->subtype, 0, dam, rad, false, true))
        context->ident = true;
    return true;
}


/*
 * Cast a ball spell centered on the character (with obvious effects)
 */
bool effect_handler_BLAST_OBVIOUS(effect_handler_context_t *context)
{
    int dam = effect_calculate_value(context, false);
    int rad = context->radius + (context->other? (context->origin->player->lev / context->other): 0);

    /* Monster */
    if (context->origin->monster)
    {
        int rlev = ((context->origin->monster->race->level >= 1)?
            context->origin->monster->race->level: 1);
        struct source who_body;
        struct source *who = &who_body;

        rad = context->radius + (context->other? (rlev / context->other): 0);

        source_monster(who, context->origin->monster);
        project(who, rad, context->cave, &context->origin->monster->grid, 0, context->subtype,
            PROJECT_ITEM | PROJECT_HIDE, 0, 0, "killed");
        update_smart_learn(context->origin->monster, context->origin->player, 0, 0, context->subtype);
    }

    /* Player */
    else if (fire_ball(context->origin->player, context->subtype, 0, dam, rad, true, false))
        context->ident = true;

    return true;
}


/*
 * Cast a bolt spell
 * Stop if we hit a monster, as a bolt
 * Affect monsters (not grids or objects)
 *
 * PWMAngband: setting context->radius is a hack for teleport other
 */
bool effect_handler_BOLT(effect_handler_context_t *context)
{
    int dam = effect_calculate_value(context, true);

    /* Hack -- teleport other */
    if (context->radius)
    {
        context->origin->player->current_sound = -2;
        sound(context->origin->player, MSG_TPOTHER);
        if (fire_bolt(context->origin, context->subtype, context->dir, dam, false))
            context->ident = true;
        context->origin->player->current_sound = -1;
    }

    /* MvM */
    else if (context->target_mon)
    {
        int flag = PROJECT_STOP | PROJECT_KILL | PROJECT_AWARE;
        struct source who_body;
        struct source *who = &who_body;
        struct loc target;
        int accuracy = monster_effect_accuracy(context->origin->monster, MON_TMD_CONF,
            CONF_RANDOM_CHANCE);

        if (randint1(100) > accuracy)
        {
            int dir = ddd[randint0(8)];

            next_grid(&target, &context->origin->monster->grid, dir);
        }
        else
            loc_copy(&target, &context->target_mon->grid);

        source_monster(who, context->origin->monster);
        project(who, 0, context->cave, &target, dam, context->subtype, flag, 0, 0, "annihilated");
    }

    /* Normal case */
    else if (fire_bolt(context->origin, context->subtype, context->dir, dam, false))
        context->ident = true;

    return true;
}


/*
 * Cast a bolt spell
 * Stop if we hit a monster, as a bolt
 * Affect monsters (not grids or objects)
 * Notice stuff based on awareness of the effect
 *
 * PWMAngband: if context->radius is set, forbid on static levels
 */
bool effect_handler_BOLT_AWARE(effect_handler_context_t *context)
{
    int dam = effect_calculate_value(context, true);

    /* Forbid in the towns and on special levels */
    if (context->radius && forbid_special(&context->origin->player->wpos))
    {
        msg(context->origin->player, "You cannot polymorph monsters here...");
        context->ident = true;
        return false;
    }

    if (fire_bolt(context->origin, context->subtype, context->dir, dam, context->aware))
        context->ident = true;
    return true;
}


/*
 * Cast a bolt spell, or rarely, a beam spell
 * context->other is used as any adjustment to the regular beam chance
 */
bool effect_handler_BOLT_OR_BEAM(effect_handler_context_t *context)
{
    int dam = effect_calculate_value(context, true);
    int beam = context->beam.beam + context->other;

    /* Hack -- space/time anchor */
    if (context->origin->player->timed[TMD_ANCHOR] && (context->subtype == PROJ_TIME))
    {
        if (one_in_(3))
        {
            msg(context->origin->player, "The space/time anchor stops your time bolt!");
            context->ident = true;
            return true;
        }
        if (one_in_(3))
            player_clear_timed(context->origin->player, TMD_ANCHOR, true);
    }

    if (magik(beam))
        fire_beam(context->origin, context->subtype, context->dir, dam, false);
    else
        fire_bolt(context->origin, context->subtype, context->dir, dam, false);
    if (!context->origin->player->timed[TMD_BLIND]) context->ident = true;
    return true;
}


/*
 * Cast a bolt spell
 * Stop if we hit a monster, as a bolt
 * Affect monsters (not grids or objects)
 *
 * Like BOLT, but only identifies on noticing an effect
 */
bool effect_handler_BOLT_STATUS(effect_handler_context_t *context)
{
    return effect_handler_BOLT(context);
}


/*
 * Cast a bolt spell
 * Stop if we hit a monster, as a bolt
 * Affect monsters (not grids or objects)
 *
 * The same as BOLT_STATUS, but done as a separate function to aid descriptions
 */
bool effect_handler_BOLT_STATUS_DAM(effect_handler_context_t *context)
{
    return effect_handler_BOLT(context);
}


/*
 * Breathe an element, in a cone from the breath
 * Affect grids, objects, and monsters
 * context->subtype is element, context->other degrees of arc
 *
 * PWMAngband: if context->radius is set, object is already used up; don't use device boost
 */
bool effect_handler_BREATH(effect_handler_context_t *context)
{
    return handler_breath(context, false);
}


/*
 * Curse a monster for direct damage
 */
bool effect_handler_CURSE(effect_handler_context_t *context)
{
    int dam = effect_calculate_value(context, false);
    bool fear = false;
    bool dead = false;
    struct source *target_who = &context->origin->player->target.target_who;

    context->ident = true;

    /* Need to choose a monster, not just point */
    if (!target_able(context->origin->player, target_who))
    {
        msg(context->origin->player, "No target selected!");
        return false;
    }

    if (target_who->monster)
    {
        dead = mon_take_hit(context->origin->player, context->cave, target_who->monster, dam, &fear,
            MON_MSG_DIE);
        if (!dead && monster_is_visible(context->origin->player, target_who->monster->midx))
        {
            if (dam > 0) message_pain(context->origin->player, target_who->monster, dam);
            if (fear)
            {
                add_monster_message(context->origin->player, target_who->monster,
                    MON_MSG_FLEE_IN_TERROR, true);
            }
        }
    }
    else
    {
        char killer[NORMAL_WID];
        char df[160];

        my_strcpy(killer, context->origin->player->name, sizeof(killer));
        strnfmt(df, sizeof(df), "was killed by %s", killer);
        dead = take_hit(target_who->player, dam, killer, false, df);
        if ((dam > 0) && !dead)
            player_pain(context->origin->player, target_who->player, dam);
    }

    return true;
}


/*
 * Deal damage from the current monster or trap to the player
 */
bool effect_handler_DAMAGE(effect_handler_context_t *context)
{
    int dam = effect_calculate_value(context, false);
    char killer[NORMAL_WID];
    struct monster *mon = context->origin->monster;
    struct trap *trap = context->origin->trap;
    struct object *obj = context->origin->obj;
    struct chest_trap *chest_trap = context->origin->chest_trap;
    bool non_physical;
    char df[160];

    /* Always ID */
    context->ident = true;

    /* A monster */
    if (mon)
    {
        const char *what = "annihilated";
        struct loc *decoy = cave_find_decoy(context->cave);

        /* Damage another monster */
        if (context->target_mon)
        {
            int flag = PROJECT_STOP | PROJECT_KILL | PROJECT_AWARE;
            struct source who_body;
            struct source *who = &who_body;

            source_monster(who, mon);
            project(who, 0, context->cave, &context->target_mon->grid, dam, context->subtype, flag,
                0, 0, what);

            return true;
        }

        /* Destroy a decoy */
        if (!loc_is_zero(decoy))
        {
            square_destroy_decoy(context->origin->player, context->cave, decoy);
            return true;
        }

        /* Get the "died from" name in case this attack kills @ */
        monster_desc(context->origin->player, killer, sizeof(killer), mon, MDESC_DIED_FROM);

        if ((context->subtype == PROJ_BLAST) || (context->subtype == PROJ_SMASH))
            what = "turned into an unthinking vegetable";
        non_physical = true;
        strnfmt(df, sizeof(df), "was %s by %s", what, killer);
    }

    /* A trap */
    else if (trap)
    {
        const char *article = (is_a_vowel(trap->kind->desc[0])? "an ": "a ");

        strnfmt(killer, sizeof(killer), "%s%s", article, trap->kind->desc);
        non_physical = false;
        trap_msg_death(context->origin->player, trap, df, sizeof(df));
    }

    /* A cursed weapon */
    else if (obj)
    {
        object_desc(context->origin->player, killer, sizeof(killer), obj, ODESC_PREFIX | ODESC_BASE);
        non_physical = false;
        strnfmt(df, sizeof(df), "was killed by %s", killer);
    }

    /* A chest */
    else if (chest_trap)
    {
        non_physical = false;
        strnfmt(df, sizeof(df), "was killed by %s", chest_trap->msg_death);
    }

    /* The player */
    else
    {
        if (context->self_msg) my_strcpy(killer, context->self_msg, sizeof(killer));
        else my_strcpy(killer, "self-inflicted wounds", sizeof(killer));
        non_physical = true;
        strnfmt(df, sizeof(df), "was killed by %s", killer);
    }

    /* Hit the player */
    take_hit(context->origin->player, dam, killer, non_physical, df);

    context->self_msg = NULL;
    return true;
}


/*
 * The destruction effect
 *
 * This effect "deletes" monsters (instead of killing them).
 *
 * This is always an effect centered on the player; it is similar to the
 * earthquake effect.
 *
 * PWMAngband: the radius can be set in context->value.base (Major Havoc); if
 * context->other is set, destroy the area silently.
 */
bool effect_handler_DESTRUCTION(effect_handler_context_t *context)
{
    int k, r = effect_calculate_value(context, false);
    int elem = context->subtype;
    int hurt[MAX_PLAYERS];
    int count;

    if (context->radius) r = context->radius;
    context->ident = true;

    /* Only on random levels */
    if (!random_level(&context->origin->player->wpos))
    {
        if (!context->other) msg(context->origin->player, "The ground shakes for a moment.");
        return true;
    }

    if (!context->other) msg_misc(context->origin->player, " unleashes great power!");

    /* Big area of affect */
    count = wreck_havoc(context, r, hurt, false);

    /* Hack -- affect players */
    for (k = 0; k < count; k++)
    {
        struct player *p = player_get(hurt[k]);

        /* Message */
        if (elem == ELEM_LIGHT) msg(p, "There is a searing blast of light!");
        else msg(p, "Darkness seems to crush you!");

        /* Blind the player */
        equip_learn_element(p, elem);
        if (!player_resists(p, elem))
            player_inc_timed(p, TMD_BLIND, 10 + randint1(10), true, true);

        /* Fully update the visuals */
        p->upkeep->update |= (PU_UPDATE_VIEW | PU_MONSTERS);

        /* Redraw */
        p->upkeep->redraw |= (PR_MONLIST | PR_ITEMLIST);
    }

    return true;
}


bool effect_handler_DETONATE(effect_handler_context_t *context)
{
    int i;
    struct monster *mon;
    int p_flag = PROJECT_JUMP | PROJECT_GRID | PROJECT_ITEM | PROJECT_KILL | PROJECT_PLAY;

    /* Make all controlled jellies and vortices explode */
    for (i = cave_monster_max(context->cave) - 1; i >= 1; i--)
    {
        mon = cave_monster(context->cave, i);

        /* Skip dead monsters */
        if (!mon->race) continue;

        /* Skip non slaves */
        if (context->origin->player->id != mon->master) continue;

        /* Jellies explode with a slowing effect */
        if (match_monster_bases(mon->race->base, "jelly", "mold", NULL))
        {
            struct source who_body;
            struct source *who = &who_body;

            source_monster(who, mon);
            project(who, 2, context->cave, &mon->grid, 20, PROJ_MON_SLOW, p_flag, 0, 0, "killed");

            /* Delete the monster */
            delete_monster_idx(context->cave, i);
        }

        /* Vortices explode with a ball effect */
        else if (match_monster_bases(mon->race->base, "vortex", NULL))
        {
            bitflag f[RSF_SIZE];
            int num = 0, j;
            byte spells[RSF_MAX];
            struct source who_body;
            struct source *who = &who_body;

            /* Extract the racial spell flags */
            rsf_copy(f, mon->race->spell_flags);

            /* Require breath attacks */
            set_breath(f);

            /* Extract spells */
            for (j = FLAG_START; j < RSF_MAX; j++)
            {
                if (rsf_has(f, j)) spells[num++] = j;
            }

            /* Pick at random */
            source_monster(who, mon);
            project(who, 2, context->cave, &mon->grid, mon->level,
                spell_effect(spells[randint0(num)]), p_flag, 0, 0, "killed");

            /* Delete the monster */
            delete_monster_idx(context->cave, i);
        }

        // Necromancer's skeletons explode with a ball effect
        else if (match_monster_bases(mon->race->base, "skeleton", NULL))
        {
            struct source who_body;
            struct source *who = &who_body;

            source_monster(who, mon);
            project(who, 2, context->cave, &mon->grid, 300, PROJ_MISSILE, p_flag, 0, 0, "killed");

            /* Delete the monster */
            delete_monster_idx(context->cave, i);
        }

    }
    return true;
}


/*
 * Induce an earthquake of the given radius at the given location.
 *
 * This will turn some walls into floors and some floors into walls.
 *
 * The player will take damage and jump into a safe grid if possible,
 * otherwise, he will tunnel through the rubble instantaneously.
 *
 * Monsters will take damage, and jump into a safe grid if possible,
 * otherwise they will be buried in the rubble, disappearing from
 * the level in the same way that they do when banished.
 *
 * Note that players and monsters (except eaters of walls and passers
 * through walls) will never occupy the same grid as a wall (or door).
 *
 * PWMAngband: the radius can be set in context->value.base (Minor Havoc); if
 * context->origin->monster is set, quake the area silently around the monster
 */
bool effect_handler_EARTHQUAKE(effect_handler_context_t *context)
{
    int r = effect_calculate_value(context, false);
    bool targeted = (context->subtype? true: false);
    int i, y, x, j;
    struct loc offset, centre, safe_grid;
    int safe_grids = 0;
    int damage = 0;
    int hurt[MAX_PLAYERS];
    bool map[32][32];
    int count = 0;

    loc_init(&safe_grid, 0, 0);

    if (context->radius) r = context->radius;
    context->ident = true;

    /* Only on random levels */
    if (!random_level(&context->origin->player->wpos))
    {
        if (!context->origin->monster)
            msg(context->origin->player, "The ground shakes for a moment.");
        return true;
    }

    /* Determine the epicentre */
    origin_get_loc(&centre, context->origin);

    if (!context->origin->monster)
    {
        msg(context->origin->player, "The ground shakes! The ceiling caves in!");
        msg_misc(context->origin->player, " causes the ground to shake!");
    }

    /* Sometimes ask for a target */
    if (targeted)
    {
        /* Ensure "dir" is in ddx/ddy array bounds */
        if (!VALID_DIR(context->dir)) return false;

        /* Ask for a target if no direction given */
        if ((context->dir == DIR_TARGET) && target_okay(context->origin->player))
            target_get(context->origin->player, &centre);
    }

    /* Paranoia -- enforce maximum range */
    if (r > 15) r = 15;

    /* Initialize a map of the maximal blast area */
    for (y = 0; y < 32; y++)
    {
        for (x = 0; x < 32; x++) map[y][x] = false;
    }

    /* Check around the epicenter */
    for (offset.y = -r; offset.y <= r; offset.y++)
    {
        for (offset.x = -r; offset.x <= r; offset.x++)
        {
            struct loc grid;

            /* Extract the location */
            loc_sum(&grid, &centre, &offset);

            /* Skip illegal grids */
            if (!square_in_bounds_fully(context->cave, &grid)) continue;

            /* Skip distant grids */
            if (distance(&centre, &grid) > r) continue;

            /* Take note of any player */
            if (square(context->cave, &grid)->mon < 0)
            {
                hurt[count] = square(context->cave, &grid)->mon;
                count++;
            }

            /* Lose room and vault */
            sqinfo_off(square(context->cave, &grid)->info, SQUARE_ROOM);
            sqinfo_off(square(context->cave, &grid)->info, SQUARE_VAULT);
            sqinfo_off(square(context->cave, &grid)->info, SQUARE_NO_TELEPORT);
            sqinfo_off(square(context->cave, &grid)->info, SQUARE_LIMITED_TELE);
            if (square_ispitfloor(context->cave, &grid)) square_clear_feat(context->cave, &grid);

            /* Forget completely */
            square_unglow(context->cave, &grid);
            square_forget_all(context->cave, &grid);
            square_light_spot(context->cave, &grid);

            /* Skip the epicenter */
            if (loc_is_zero(&offset)) continue;

            /* Skip most grids */
            if (magik(85)) continue;

            /* Damage this grid */
            map[16 + offset.y][16 + offset.x] = true;

            /* Take note of player damage */
            if (square(context->cave, &grid)->mon < 0) hurt[count - 1] = 0 - hurt[count - 1];
        }
    }

    /* First, affect the players (if necessary) */
    for (j = 0; j < count; j++)
    {
        struct player *player;

        /* Skip undamaged players */
        if (hurt[j] < 0) continue;

        player = player_get(hurt[j]);

        safe_grids = 0; damage = 0;
        loc_init(&safe_grid, 0, 0);

        /* Check around the player */
        for (i = 0; i < 8; i++)
        {
            struct loc grid;

            /* Get the location */
            loc_sum(&grid, &player->grid, &ddgrid_ddd[i]);

            /* Skip illegal grids */
            if (!square_in_bounds_fully(context->cave, &grid)) continue;

            /* Skip non-empty grids - allow pushing into traps and webs */
            if (!square_isopen(context->cave, &grid)) continue;

            /* Important -- skip grids marked for damage */
            if (map[16 + grid.y - centre.y][16 + grid.x - centre.x]) continue;

            /* Count "safe" grids, apply the randomizer */
            if ((++safe_grids > 1) && randint0(safe_grids)) continue;

            /* Save the safe location */
            loc_copy(&safe_grid, &grid);
        }

        /* Random message */
        switch (randint1(3))
        {
            case 1:
            {
                msg(player, "The cave ceiling collapses on you!");
                break;
            }
            case 2:
            {
                msg(player, "The cave floor twists in an unnatural way!");
                break;
            }
            default:
            {
                msg(player, "The cave quakes!");
                msg(player, "You are pummeled with debris!");
                break;
            }
        }

        /* Hurt the player a lot */
        if (!safe_grids)
        {
            /* Message and damage */
            msg(player, "You are severely crushed!");
            damage = 300;
        }

        /* Destroy the grid, and push the player to safety */
        else
        {
            /* Calculate results */
            switch (randint1(3))
            {
                case 1:
                {
                    msg(player, "You nimbly dodge the blast!");
                    damage = 0;
                    break;
                }
                case 2:
                {
                    msg(player, "You are bashed by rubble!");
                    damage = damroll(10, 4);
                    player_inc_timed(player, TMD_STUN, randint1(50), true, true);
                    break;
                }
                case 3:
                {
                    msg(player, "You are crushed between the floor and ceiling!");
                    damage = damroll(10, 4);
                    player_inc_timed(player, TMD_STUN, randint1(50), true, true);
                    break;
                }
            }

            /* Move player */
            monster_swap(context->cave, &player->grid, &safe_grid);
        }

        /* Take some damage */
        if (damage)
            take_hit(player, damage, "an earthquake", false, "was crushed by tons of falling rocks");
    }

    /* Examine the quaked region */
    for (offset.y = -r; offset.y <= r; offset.y++)
    {
        for (offset.x = -r; offset.x <= r; offset.x++)
        {
            struct loc grid;

            /* Extract the location */
            loc_sum(&grid, &centre, &offset);

            /* Skip illegal grids */
            if (!square_in_bounds_fully(context->cave, &grid)) continue;

            /* Skip unaffected grids */
            if (!map[16 + offset.y][16 + offset.x]) continue;

            /* Process monsters */
            if (square(context->cave, &grid)->mon > 0)
            {
                struct monster *mon = square_monster(context->cave, &grid);

                /* Most monsters cannot co-exist with rock */
                if (!monster_passes_walls(mon->race))
                {
                    /* Assume not safe */
                    safe_grids = 0;

                    /* Monster can move to escape the wall */
                    if (!rf_has(mon->race->flags, RF_NEVER_MOVE))
                    {
                        /* Look for safety */
                        for (i = 0; i < 8; i++)
                        {
                            struct loc safe;

                            /* Get the grid */
                            loc_sum(&safe, &grid, &ddgrid_ddd[i]);

                            /* Skip illegal grids */
                            if (!square_in_bounds_fully(context->cave, &safe)) continue;

                            /* Skip non-empty grids */
                            if (!square_isempty(context->cave, &safe)) continue;

                            /* No safety on glyph of warding */
                            if (square_iswarded(context->cave, &safe)) continue;

                            /* Important -- skip "quake" grids */
                            if (map[16 + safe.y - centre.y][16 + safe.x - centre.x]) continue;

                            /* Count "safe" grids, apply the randomizer */
                            if ((++safe_grids > 1) && randint0(safe_grids)) continue;

                            /* Save the safe grid */
                            loc_copy(&safe_grid, &safe);
                        }
                    }

                    /* Give players a message */
                    for (j = 0; j < count; j++)
                    {
                        /* Get player */
                        struct player *player = player_get(abs(hurt[j]));

                        /* Scream in pain */
                        add_monster_message(player, mon, MON_MSG_WAIL, true);
                    }

                    /* Take damage from the quake */
                    damage = (safe_grids? damroll(4, 8): (mon->hp + 1));

                    /* Monster is certainly awake, not thinking about player */
                    monster_wake(context->origin->player, mon, false, 0);
                    mon_clear_timed(context->origin->player, mon, MON_TMD_HOLD, MON_TMD_FLG_NOTIFY);

                    /* If the quake finished the monster off, show message */
                    if ((mon->hp < damage) && (mon->hp >= 0))
                    {
                        /* Give players a message */
                        for (j = 0; j < count; j++)
                        {
                            /* Get player */
                            struct player *player = player_get(abs(hurt[j]));

                            /* Message */
                            add_monster_message(player, mon, MON_MSG_EMBEDDED, true);
                        }
                    }

                    /* Apply damage directly */
                    mon->hp -= damage;

                    /* Delete (not kill) "dead" monsters */
                    if (mon->hp < 0)
                    {
                        /* Delete the monster */
                        delete_monster(context->cave, &grid);
                        if (square_ispitfloor(context->cave, &grid))
                            square_clear_feat(context->cave, &grid);

                        /* No longer safe */
                        safe_grids = 0;
                    }

                    /* Escape from the rock */
                    if (safe_grids)
                    {
                        /* Move the monster */
                        monster_swap(context->cave, &grid, &safe_grid);
                    }
                }
            }
        }
    }

    /* Important -- no wall on players */
    for (j = 0; j < count; j++)
    {
        /* Get player */
        struct player *player = player_get(abs(hurt[j]));

        map[16 + player->grid.y - centre.y][16 + player->grid.x - centre.x] = false;
    }

    /* Examine the quaked region and damage marked grids if possible */
    for (offset.y = -r; offset.y <= r; offset.y++)
    {
        for (offset.x = -r; offset.x <= r; offset.x++)
        {
            struct loc grid;

            /* Extract the location */
            loc_sum(&grid, &centre, &offset);

            /* Skip illegal grids */
            if (!square_in_bounds_fully(context->cave, &grid)) continue;

            /* Note unaffected grids for light changes, etc. */
            if (!map[16 + offset.y][16 + offset.x]) square_light_spot(context->cave, &grid);

            /* Destroy location and all objects (if valid) */
            else if (square_changeable(context->cave, &grid))
            {
                square_forget_pile_all(context->cave, &grid);
                square_excise_pile(context->cave, &grid);
                square_earthquake(context->cave, &grid);
            }
        }
    }

    for (j = 0; j < count; j++)
    {
        /* Get player */
        struct player *player = player_get(abs(hurt[j]));

        /* Fully update the visuals */
        player->upkeep->update |= (PU_UPDATE_VIEW | PU_MONSTERS);

        /* Redraw */
        player->upkeep->redraw |= (PR_HEALTH | PR_MONLIST | PR_ITEMLIST);
    }

    return true;
}


/*
 * Heal the player by a given percentage of their wounds, or a minimum
 * amount, whichever is larger.
 *
 * context->value.base should be the minimum, and
 * context->value.m_bonus the percentage
 */
bool effect_handler_HEAL_HP(effect_handler_context_t *context)
{
    int num, amount;

    /* Paranoia */
    if ((context->value.m_bonus <= 0) && (context->value.base <= 0)) return true;

    /* Always ID */
    context->ident = true;

    /* No healing needed */
    if (context->origin->player->chp >= context->origin->player->mhp) return true;

    /* Figure healing level */
    num = ((context->origin->player->mhp - context->origin->player->chp) *
        context->value.m_bonus) / 100;

    /* PWMAngband: Cell Adjustment heals a variable amount of hps */
    amount = context->value.base + damroll(context->value.dice, context->value.sides);

    /* Enforce minimums */
    if (num < amount) num = amount;

    if (context->self_msg) msg(context->origin->player, context->self_msg);
    hp_player(context->origin->player, num);

    context->self_msg = NULL;
    return true;
}


/*
 * Crack a whip, or spit at the player; actually just a finite length beam
 * Affect grids, objects, and monsters
 * context->radius is length of beam
 */
bool effect_handler_LASH(effect_handler_context_t *context)
{
    int dam = effect_calculate_value(context, false);
    int rad = context->radius;
    int flg = PROJECT_ARC | PROJECT_GRID | PROJECT_ITEM | PROJECT_KILL | PROJECT_PLAY;
    int i, type;
    struct loc target;
    struct source who_body;
    struct source *who = &who_body;
    int diameter_of_source;
    const struct monster_race *race = NULL;

    /* Paranoia */
    if (rad > z_info->max_range) rad = z_info->max_range;

    /*
     * Diameter of source is the same as the radius, so the effect is
     * essentially full strength for its entire length.
     */
    diameter_of_source = rad;

    /* Ensure "dir" is in ddx/ddy array bounds */
    if (!VALID_DIR(context->dir)) return false;

    /* Player or monster? */
    if (context->origin->monster)
    {
        source_monster(who, context->origin->monster);

        /* Target player or monster? */
        if (context->target_mon)
            loc_copy(&target, &context->target_mon->grid);
        else
        {
            if (monster_is_decoyed(context->cave, context->origin->monster))
                loc_copy(&target, cave_find_decoy(context->cave));
            else
                loc_copy(&target, &context->origin->player->grid);
            who->target = context->origin->player;
        }

        race = context->origin->monster->race;
    }
    else if (context->origin->player->poly_race)
    {
        /* Handle polymorphed players */
        source_player(who, get_player_index(get_connection(context->origin->player->conn)),
            context->origin->player);

        /* Ask for a target if no direction given */
        if ((context->dir == DIR_TARGET) && target_okay(context->origin->player))
            target_get(context->origin->player, &target);
        else
        {
            /* Hack -- no target available, default to random direction */
            if (context->dir == DIR_TARGET) context->dir = 0;

            /* Hack -- no direction given, default to random direction */
            if (!context->dir) context->dir = ddd[randint0(8)];

            /* Use the given direction */
            next_grid(&target, &context->origin->player->grid, context->dir);
        }

        race = context->origin->player->poly_race;
    }

    if (!race) return false;

    /* Get the type (default is PROJ_MISSILE) */
    type = race->blow[0].effect->lash_type;

    /* Scan through all blows for damage */
    for (i = 0; i < z_info->mon_blows_max; i++)
    {
        /* Extract the attack infomation */
        random_value dice = race->blow[i].dice;

        /* Full damage of first blow, plus half damage of others */
        dam += randcalc(dice, race->level, RANDOMISE) / (i? 2: 1);
    }

    /* No damaging blows */
    if (!dam) return false;

    /* Check bounds */
    if (diameter_of_source > 25) diameter_of_source = 25;

    /* Lash the target */
    context->origin->player->current_sound = -2;
    if (project(who, rad, context->cave, &target, dam, type, flg, 0, diameter_of_source,
        "lashed"))
    {
        context->ident = true;
    }
    context->origin->player->current_sound = -1;

    return true;
}


/*
 * Cast a line spell
 * Pass through monsters, as a beam
 * Affect monsters and grids (not objects)
 *
 * PWMAngband: setting context->value.m_bonus is a hack for elementalists to
 * get multiple lines
 */
bool effect_handler_LINE(effect_handler_context_t *context)
{
    int dam = effect_calculate_value(context, true);
    int y, num = (context->value.m_bonus? context->value.m_bonus: 1);

    if (context->self_msg && !context->origin->player->timed[TMD_BLIND])
        msg(context->origin->player, context->self_msg);
    for (y = 0; y < num; y++)
    {
        if (light_line_aux(context->origin, context->dir, context->subtype, dam))
            context->ident = true;
    }

    context->self_msg = NULL;
    return true;
}


bool effect_handler_MELEE_BLOWS(effect_handler_context_t *context)
{
    int dam = effect_calculate_value(context, false);
    struct loc target;

    /* Ensure "dir" is in ddx/ddy array bounds */
    if (!VALID_DIR(context->dir)) return false;

    /* Use the given direction */
    next_grid(&target, &context->origin->player->grid, context->dir);

    /* Hack -- use an actual "target" */
    if ((context->dir == DIR_TARGET) && target_okay(context->origin->player))
    {
        target_get(context->origin->player, &target);

        /* Check distance */
        if (distance(&context->origin->player->grid, &target) > 1)
        {
            msg(context->origin->player, "Target out of range.");
            context->ident = true;
            return true;
        }
    }

    if (py_attack_grid(context->origin->player, context->cave, &target))
    {
        struct source who_body;
        struct source *who = &who_body;

        source_player(who, get_player_index(get_connection(context->origin->player->conn)),
            context->origin->player);

        /* Analyze the "dir" and the "target", do NOT explode */
        if (project(who, 0, context->cave, &target, dam, context->subtype,
            PROJECT_GRID | PROJECT_KILL | PROJECT_PLAY, 0, 0, "annihilated"))
        {
            context->ident = true;
        }
    }

    return true;
}


/*
 * Monster self-healing.
 */
bool effect_handler_MON_HEAL_HP(effect_handler_context_t *context)
{
    struct monster *mon = context->origin->monster;
    int amount = effect_calculate_value(context, false);

    if (!mon) return true;

    /* No stupid message when at full health */
    if (mon->hp == mon->maxhp) return true;

    heal_monster(context->origin->player, mon, context->origin, amount);

    /* ID */
    context->ident = true;

    return true;
}


/*
 * Monster healing of kin.
 */
bool effect_handler_MON_HEAL_KIN(effect_handler_context_t *context)
{
    struct monster *mon = context->origin->monster;
    int amount = effect_calculate_value(context, false);

    if (!mon) return true;

    /* Find a nearby monster */
    mon = choose_nearby_injured_kin(context->cave, mon);
    if (!mon) return true;

    /* No stupid message when at full health */
    if (mon->hp == mon->maxhp) return true;

    heal_monster(context->origin->player, mon, context->origin, amount);

    /* ID */
    context->ident = true;

    return true;
}


/*
 * Dummy effect, to tell the effect code to apply a "project()" on a monster (for MvM mode).
 */
bool effect_handler_PROJECT(effect_handler_context_t *context)
{
    int dam = effect_calculate_value(context, false);
    int flag = PROJECT_STOP | PROJECT_KILL | PROJECT_AWARE;
    struct source who_body;
    struct source *who = &who_body;

    if (!context->origin->monster) return true;

    /* MvM only */
    if (!context->target_mon) return true;

    source_monster(who, context->origin->monster);
    project(who, 0, context->cave, &context->target_mon->grid, dam, context->subtype, flag, 0, 0,
        "annihilated");

    return true;
}


/*
 * Apply a "project()" directly to all viewable monsters. If context->other is
 * set, the effect damage boost is applied.
 *
 * Note that affected monsters are NOT auto-tracked by this usage.
 */
bool effect_handler_PROJECT_LOS(effect_handler_context_t *context)
{
    int dam = effect_calculate_value(context, context->other? true: false);
    int typ = context->subtype;

    project_los(context, typ, dam, false);
    context->ident = true;
    return true;
}


/*
 * Apply a "project()" directly to all viewable monsters. If context->other is
 * set, the effect damage boost is applied.
 *
 * Note that affected monsters are NOT auto-tracked by this usage.
 */
bool effect_handler_PROJECT_LOS_AWARE(effect_handler_context_t *context)
{
    int dam = effect_calculate_value(context, context->other? true: false);
    int typ = context->subtype;

    if (project_los(context, typ, dam, context->aware) && context->self_msg)
        msg(context->origin->player, context->self_msg);

    context->ident = true;
    context->self_msg = NULL;
    return true;
}


/*
 * Cast a defined length beam spell.
 * Affect the player, grids, objects, and monsters
 * context->subtype is element, context->radius radius
 */
bool effect_handler_SHORT_BEAM(effect_handler_context_t *context)
{
    int dam = effect_calculate_value(context, false);
    int type = context->subtype;
    int rad = context->radius;
    int flg = PROJECT_ARC | PROJECT_GRID | PROJECT_ITEM | PROJECT_KILL | PROJECT_PLAY;
    int diameter_of_source;
    struct loc target;
    struct source who_body;
    struct source *who = &who_body;

    /* Paranoia */
    if (rad > z_info->max_range) rad = z_info->max_range;

    /*
     * Diameter of source is the same as the radius, so the effect is
     * essentially full strength for its entire length.
     */
    diameter_of_source = rad;

    /* Ensure "dir" is in ddx/ddy array bounds */
    if (!VALID_DIR(context->dir)) return false;

    /* Player or monster? */
    if (context->origin->monster)
    {
        source_monster(who, context->origin->monster);

        /* Target player or monster? */
        if (context->target_mon)
            loc_copy(&target, &context->target_mon->grid);
        else
        {
            if (monster_is_decoyed(context->cave, context->origin->monster))
                loc_copy(&target, cave_find_decoy(context->cave));
            else
                loc_copy(&target, &context->origin->player->grid);
            who->target = context->origin->player;
        }
    }
    else
    {
        source_player(who, get_player_index(get_connection(context->origin->player->conn)),
            context->origin->player);

        // Magic Blade spell (mana 1)
        if (streq(context->origin->player->clazz->name, "Wizard") && context->origin->player->spell_cost == 1)
        {
            // distance
            rad += context->origin->player->lev / 10;
            if (rad > 5) rad = 5;
            // dmg
            if (dam > 10)
                dam *= context->origin->player->lev / 10;
        }

        /* Ask for a target if no direction given */
        if ((context->dir == DIR_TARGET) && target_okay(context->origin->player))
            target_get(context->origin->player, &target);
        else
        {
            /* Hack -- no target available, default to random direction */
            if (context->dir == DIR_TARGET) context->dir = 0;

            /* Hack -- no direction given, default to random direction */
            if (!context->dir) context->dir = ddd[randint0(8)];

            /* Use the given direction */
            next_grid(&target, &context->origin->player->grid, context->dir);
        }
    }

    /* Check bounds */
    if (diameter_of_source > 25) diameter_of_source = 25;

    /* Aim at the target */
    context->origin->player->current_sound = -2;
    if (project(who, rad, context->cave, &target, dam, type, flg, 0, diameter_of_source,
        "annihilated"))
    {
        context->ident = true;
    }
    context->origin->player->current_sound = -1;

    return true;
}


/*
 * Project from the player's grid at the player, act as a ball
 * Affect the player, grids, objects, and monsters
 */
bool effect_handler_SPOT(effect_handler_context_t *context)
{
    int dam = effect_calculate_value(context, false);
    int rad = context->radius;
    int flg = PROJECT_STOP | PROJECT_GRID | PROJECT_ITEM | PROJECT_KILL | PROJECT_PLAY;
    bool result;
    struct source who_body;
    struct source *who = &who_body;

    source_trap(who, context->origin->trap);

    /* Aim at the target, explode */
    context->origin->player->current_sound = -2;
    result = project(who, rad, chunk_get(&context->origin->player->wpos),
        &context->origin->player->grid, dam, context->subtype, flg, 0, 0, "annihilated");
    context->origin->player->current_sound = -1;
    if (result) context->ident = true;

    return true;
}


/*
 * Cast a line spell in every direction
 * Stop if we hit a monster, act as a ball
 * Affect grids, objects, and monsters
 *
 * PWMAngband: if context->radius is set, divide the damage by that amount
 */
bool effect_handler_STAR(effect_handler_context_t *context)
{
    int dam = effect_calculate_value(context, true);
    int i;

    if (context->radius) dam /= context->radius;

    if (context->self_msg && !context->origin->player->timed[TMD_BLIND])
        msg(context->origin->player, context->self_msg);
    context->origin->player->do_visuals = true;
    for (i = 0; i < 8; i++)
        light_line_aux(context->origin, ddd[i], context->subtype, dam);
    context->origin->player->do_visuals = false;
    if (!context->origin->player->timed[TMD_BLIND]) context->ident = true;

    context->self_msg = NULL;
    return true;
}


/*
 * Cast a ball spell in every direction
 * Stop if we hit a monster, act as a ball
 * Affect grids, objects, and monsters
 */
bool effect_handler_STAR_BALL(effect_handler_context_t *context)
{
    int dam = effect_calculate_value(context, true);
    int i;

    if (context->self_msg && !context->origin->player->timed[TMD_BLIND])
        msg(context->origin->player, context->self_msg);
    for (i = 0; i < 8; i++)
    {
        fire_ball(context->origin->player, context->subtype, ddd[i], dam, context->radius, false,
            false);
    }
    if (!context->origin->player->timed[TMD_BLIND]) context->ident = true;

    context->self_msg = NULL;
    return true;
}


/*
 * Strike the target with a ball from above
 */
bool effect_handler_STRIKE(effect_handler_context_t *context)
{
    int dam = effect_calculate_value(context, true);
    struct loc target;
    struct source who_body;
    struct source *who = &who_body;
    int flg = PROJECT_JUMP | PROJECT_GRID | PROJECT_ITEM | PROJECT_KILL | PROJECT_PLAY;
    const char *what = "annihilated";

    loc_copy(&target, &context->origin->player->grid);
    source_player(who, get_player_index(get_connection(context->origin->player->conn)),
        context->origin->player);

    /* Ask for a target; if no direction given, the player is struck  */
    if ((context->dir == DIR_TARGET) && target_okay(context->origin->player))
        target_get(context->origin->player, &target);
    else
    {
        msg(context->origin->player, "You must have a target.");
        return false;
    }

    /* Enforce line of sight */
    if (!projectable(context->origin->player, context->cave, &context->origin->player->grid,
        &target, PROJECT_NONE, true) || !square_isknown(context->origin->player, &target))
    {
        return false;
    }

    /* Aim at the target. Hurt items on floor. */
    context->origin->player->current_sound = -2;
    if (project(who, context->radius, context->cave, &target, dam, context->subtype, flg, 0, 0, what))
        context->ident = true;
    context->origin->player->current_sound = -1;

    return true;
}


/*
 * Cast multiple non-jumping ball spells at the same target.
 *
 * Targets absolute coordinates instead of a specific monster, so that
 * the death of the monster doesn't change the target's location.
 */
bool effect_handler_SWARM(effect_handler_context_t *context)
{
    int dam = effect_calculate_value(context, true);
    int num = context->value.m_bonus;
    struct loc target;
    int flg = PROJECT_THRU | PROJECT_STOP | PROJECT_GRID | PROJECT_ITEM | PROJECT_KILL | PROJECT_PLAY;
    struct source who_body;
    struct source *who = &who_body;

    source_player(who, get_player_index(get_connection(context->origin->player->conn)),
        context->origin->player);

    /* Ensure "dir" is in ddx/ddy array bounds */
    if (!VALID_DIR(context->dir)) return false;

    /* Use the given direction */
    next_grid(&target, &context->origin->player->grid, context->dir);

    /* Hack -- use an actual "target" */
    if ((context->dir == DIR_TARGET) && target_okay(context->origin->player))
        target_get(context->origin->player, &target);

    context->origin->player->current_sound = -2;
    while (num--)
    {
        /* Aim at the target. Hurt items on floor. */
        if (project(who, context->radius, context->cave, &target, dam, context->subtype, flg, 0, 0,
            "annihilated"))
        {
            context->ident = true;
        }
    }
    context->origin->player->current_sound = -1;

    return true;
}


bool effect_handler_SWEEP(effect_handler_context_t *context)
{
    int d;

    for (d = 0; d < 8; d++)
    {
        struct loc adjacent;

        loc_sum(&adjacent, &context->origin->player->grid, &ddgrid_ddd[d]);
        py_attack_grid(context->origin->player, context->cave, &adjacent);
    }

    return true;
}


/*
 * Draw energy from a nearby undead
 */
bool effect_handler_TAP_UNLIFE(effect_handler_context_t *context)
{
    int amount = effect_calculate_value(context, false);
    int drain = 0;
    bool fear = false;
    bool dead = false;
    struct source *target_who = &context->origin->player->target.target_who;
    char dice[5];

    context->ident = true;

    /* Need to choose a monster, not just point */
    if (!target_able(context->origin->player, target_who))
    {
        msg(context->origin->player, "No target selected!");
        return false;
    }

    if (target_who->monster)
    {
        char m_name[NORMAL_WID];

        /* Must be undead */
        if (!monster_is_undead(target_who->monster->race))
        {
            msg(context->origin->player, "Nothing happens.");
            return false;
        }

        /* Hurt the monster */
        monster_desc(context->origin->player, m_name, sizeof(m_name), target_who->monster,
            MDESC_DEFAULT);
        msg(context->origin->player, "You draw power from the %s.", m_name);
        drain = MIN(target_who->monster->hp, amount) / 4;
        dead = mon_take_hit(context->origin->player, context->cave, target_who->monster, amount,
            &fear, MON_MSG_DESTROYED);

        /* Cancel the targeting of the dead creature. */
        if (dead)
            memset(&context->origin->player->target, 0, sizeof(context->origin->player->target));

        /* Handle fear for surviving monsters */
        else if (monster_is_visible(context->origin->player, target_who->monster->midx))
        {
            if (amount > 0) message_pain(context->origin->player, target_who->monster, amount);
            if (fear)
            {
                add_monster_message(context->origin->player, target_who->monster,
                    MON_MSG_FLEE_IN_TERROR, true);
            }
        }
    }
    else
    {
        char killer[NORMAL_WID];
        char df[160];

        /* Must be undead */
        if (!target_who->player->poly_race ||
            !rf_has(target_who->player->poly_race->flags, RF_UNDEAD))
        {
            msg(context->origin->player, "Nothing happens.");
            return false;
        }

        /* Hurt the player */
        msg(context->origin->player, "You draw power from %s.", target_who->player->name);
        drain = MIN(target_who->player->chp, amount) / 4;
        my_strcpy(killer, context->origin->player->name, sizeof(killer));
        strnfmt(df, sizeof(df), "was killed by %s", killer);
        dead = take_hit(target_who->player, amount, killer, false, df);
        if (dead)
            memset(&context->origin->player->target, 0, sizeof(context->origin->player->target));
        else if (amount > 0)
            player_pain(context->origin->player, target_who->player, amount);
    }

    /* Gain mana */
    strnfmt(dice, sizeof(dice), "%d", drain);
    effect_simple(EF_RESTORE_MANA, context->origin, dice, 0, 0, 0, 0, 0, NULL);

    return true;
}


/*
 * Affect adjacent grids
 *
 * PWMAngband: set context->other to 1 to prevent the effect on static levels
 */
bool effect_handler_TOUCH(effect_handler_context_t *context)
{
    int dam = effect_calculate_value(context, false);
    int rad = (context->radius? context->radius: 1);

    /* Only on random levels */
    if ((context->other == 1) && !random_level(&context->origin->player->wpos))
    {
        msg(context->origin->player, "Nothing happens.");
        return true;
    }

    /* Monster cast at monster */
    if (context->target_mon)
    {
        int flg = PROJECT_GRID | PROJECT_ITEM | PROJECT_HIDE;
        struct source who_body;
        struct source *who = &who_body;

        source_monster(who, context->origin->monster);
        project(who, rad, context->cave, &context->target_mon->grid, 0, context->subtype, flg, 0, 0,
            "killed");
        return true;
    }

    if (project_touch(context->origin->player, dam, rad, context->subtype, false,
        context->origin->monster))
    {
        context->ident = true;
        if (context->self_msg) msg(context->origin->player, context->self_msg);
    }

    context->self_msg = NULL;
    return true;
}


/*
 * Affect adjacent grids (radius 1 ball attack)
 * Notice stuff based on awareness of the effect
 */
bool effect_handler_TOUCH_AWARE(effect_handler_context_t *context)
{
    int dam = effect_calculate_value(context, false);

    if (project_touch(context->origin->player, dam, 1, context->subtype, context->aware, NULL))
        context->ident = true;
    return true;
}


/* Wipe everything */
bool effect_handler_WIPE_AREA(effect_handler_context_t *context)
{
    int k, r = context->radius;
    int hurt[MAX_PLAYERS];
    int count;

    /* Paranoia -- enforce maximum range */
    if (r > 12) r = 12;

    /* Only on random levels */
    if (!random_level(&context->origin->player->wpos))
    {
        msg(context->origin->player, "The ground shakes for a moment.");
        return true;
    }

    /* Check around the epicenter */
    count = wreck_havoc(context, r, hurt, true);

    /* Hack -- affect players */
    for (k = 0; k < count; k++)
    {
        struct player *p = player_get(hurt[k]);

        /* Message */
        msg(p, "There is a searing blast of light!");

        /* Fully update the visuals */
        p->upkeep->update |= (PU_UPDATE_VIEW | PU_MONSTERS);

        /* Redraw */
        p->upkeep->redraw |= (PR_MONLIST | PR_ITEMLIST);
    }

    return true;
}


/*
 * The "wonder" effect.
 */
bool effect_handler_WONDER(effect_handler_context_t *context)
{
    /*
     * This spell should become more useful (more controlled) as the player
     * gains experience levels. Thus, add 1/5 of the player's level to the die roll.
     * This eliminates the worst effects later on, while keeping the results quite
     * random. It also allows some potent effects only at high level.
     */

    int plev = context->origin->player->lev;
    int die = effect_calculate_value(context, false);
    effect_handler_f handler = NULL;
    effect_handler_context_t new_context;

    memset(&new_context, 0, sizeof(new_context));
    new_context.origin = context->origin;
    new_context.cave = context->cave;
    new_context.aware = context->aware;
    new_context.dir = context->dir;
    new_context.beam = context->beam;
    new_context.boost = context->boost;
    new_context.ident = context->ident;

    if (die > 100) msg(context->origin->player, "You feel a surge of power!");

    if (die < 8)
    {
        msg_misc(context->origin->player, " mumbles.");
        new_context.subtype = PROJ_MON_CLONE;
        handler = effect_handler_BOLT;
    }
    else if (die < 14)
    {
        msg_misc(context->origin->player, " mumbles.");
        new_context.value.base = 100;
        new_context.subtype = PROJ_MON_SPEED;
        handler = effect_handler_BOLT;
    }
    else if (die < 26)
    {
        msg_misc(context->origin->player, " mumbles.");
        new_context.value.dice = 4;
        new_context.value.sides = 6;
        new_context.subtype = PROJ_MON_HEAL;
        handler = effect_handler_BOLT;
    }
    else if (die < 31)
    {
        msg_misc(context->origin->player, " discharges an everchanging blast of energy.");
        new_context.aware = false;
        new_context.value.base = plev;
        new_context.subtype = PROJ_MON_POLY;
        new_context.radius = 1;
        handler = effect_handler_BOLT_AWARE;
    }
    else if (die < 36)
    {
        msg_misc(context->origin->player, " fires a magic missile.");
        new_context.value.dice = 3 + (plev - 1) / 5;
        new_context.value.sides = 4;
        new_context.subtype = PROJ_MISSILE;
        new_context.other = -10;
        handler = effect_handler_BOLT_OR_BEAM;
    }
    else if (die < 41)
    {
        msg_misc(context->origin->player, " makes a complicated gesture.");
        new_context.aware = false;
        new_context.value.base = 5;
        new_context.value.dice = 1;
        new_context.value.sides = 5;
        new_context.subtype = PROJ_MON_CONF;
        handler = effect_handler_BOLT_AWARE;
    }
    else if (die < 46)
    {
        msg_misc(context->origin->player, " fires a stinking cloud.");
        new_context.value.base = 20 + plev / 2;
        new_context.subtype = PROJ_POIS;
        new_context.radius = 3;
        handler = effect_handler_BALL;
    }
    else if (die < 51)
    {
        msg_misc(context->origin->player, "'s hands project a line of shimmering blue light.");
        new_context.value.dice = 6;
        new_context.value.sides = 8;
        new_context.subtype = PROJ_LIGHT_WEAK;
        new_context.self_msg = "A line of shimmering blue light appears.";
        handler = effect_handler_LINE;
    }
    else if (die < 56)
    {
        msg_misc(context->origin->player, " fires a lightning bolt.");
        new_context.value.dice = 3 + (plev - 5) / 6;
        new_context.value.sides = 6;
        new_context.subtype = PROJ_ELEC;
        handler = effect_handler_BEAM;
    }
    else if (die < 61)
    {
        msg_misc(context->origin->player, " fires a frost bolt.");
        new_context.value.dice = 5 + (plev - 5) / 4;
        new_context.value.sides = 8;
        new_context.subtype = PROJ_COLD;
        new_context.other = -10;
        handler = effect_handler_BOLT_OR_BEAM;
    }
    else if (die < 66)
    {
        msg_misc(context->origin->player, " fires an acid bolt.");
        new_context.value.dice = 6 + (plev - 5) / 4;
        new_context.value.sides = 8;
        new_context.subtype = PROJ_ACID;
        handler = effect_handler_BOLT_OR_BEAM;
    }
    else if (die < 71)
    {
        msg_misc(context->origin->player, " fires a fire bolt.");
        new_context.value.dice = 8 + (plev - 5) / 4;
        new_context.value.sides = 8;
        new_context.subtype = PROJ_FIRE;
        handler = effect_handler_BOLT_OR_BEAM;
    }
    else if (die < 76)
    {
        msg_misc(context->origin->player, " fires a bolt filled with pure energy!");
        new_context.value.base = 75;
        new_context.subtype = PROJ_MON_DRAIN;
        handler = effect_handler_BOLT;
    }
    else if (die < 81)
    {
        msg_misc(context->origin->player, " fires a lightning ball.");
        new_context.value.base = 30 + plev / 2;
        new_context.subtype = PROJ_ELEC;
        new_context.radius = 2;
        handler = effect_handler_BALL;
    }
    else if (die < 86)
    {
        msg_misc(context->origin->player, " fires an acid ball.");
        new_context.value.base = 40 + plev;
        new_context.subtype = PROJ_ACID;
        new_context.radius = 2;
        handler = effect_handler_BALL;
    }
    else if (die < 91)
    {
        msg_misc(context->origin->player, " fires an ice ball.");
        new_context.value.base = 70 + plev;
        new_context.subtype = PROJ_ICE;
        new_context.radius = 3;
        handler = effect_handler_BALL;
    }
    else if (die < 96)
    {
        msg_misc(context->origin->player, " fires a fire ball.");
        new_context.value.base = 80 + plev;
        new_context.subtype = PROJ_FIRE;
        new_context.radius = 3;
        handler = effect_handler_BALL;
    }
    else if (die < 101)
    {
        msg_misc(context->origin->player, " fires a massive bolt filled with pure energy!");
        new_context.value.base = 100 + plev;
        new_context.subtype = PROJ_MON_DRAIN;
        handler = effect_handler_BOLT;
    }
    else if (die < 104)
    {
        msg_misc(context->origin->player, " mumbles.");
        new_context.radius = 12;
        handler = effect_handler_EARTHQUAKE;
    }
    else if (die < 106)
    {
        msg_misc(context->origin->player, " mumbles.");
        new_context.radius = 15;
        handler = effect_handler_DESTRUCTION;
    }
    else if (die < 108)
    {
        msg_misc(context->origin->player, " mumbles.");
        handler = effect_handler_BANISH;
    }
    else if (die < 110)
    {
        msg_misc(context->origin->player, " mumbles.");
        new_context.value.base = 120;
        new_context.subtype = PROJ_DISP_ALL;
        new_context.other = 1;
        handler = effect_handler_PROJECT_LOS;
    }

    if (handler != NULL)
    {
        bool handled = handler(&new_context);

        context->ident = new_context.ident;
        return handled;
    }

    /* RARE */
    msg_misc(context->origin->player, " mumbles.");
    effect_simple(EF_PROJECT_LOS, context->origin, "150", PROJ_DISP_ALL, 0, 1, 0, 0, &context->ident);
    effect_simple(EF_PROJECT_LOS, context->origin, "20", PROJ_MON_SLOW, 0, 0, 0, 0, &context->ident);
    effect_simple(EF_PROJECT_LOS, context->origin, "0", PROJ_SLEEP_ALL, 0, 0, 0, 0, &context->ident);
    effect_simple(EF_HEAL_HP, context->origin, "300", 0, 0, 0, 0, 0, &context->ident);

    return true;
}