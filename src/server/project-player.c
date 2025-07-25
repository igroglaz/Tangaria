/*
 * File: project-player.c
 * Purpose: Projection effects on players
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

/*

Old note (?): some of code in this file is for PvP, eg in monster-spell.txt:

effect:TIMED_INC:AMNESIA
dice:8
effect:PROJECT:FORGET
dice:5+1d5

first line is monster -> player
second line is PvP.

*/


#include "s-angband.h"


/*
 * Is player susceptible to an attack?
 *
 * true means normal damage, false means no damage
 */
static bool is_susceptible(struct monster_race *race, int type)
{
    switch (type)
    {
        /* Check these attacks against vulnerability (polymorphed players) */
        case PROJ_LIGHT_WEAK: return (race && rf_has(race->flags, RF_HURT_LIGHT));
        case PROJ_KILL_WALL: return (race && rf_has(race->flags, RF_HURT_ROCK));
        case PROJ_DISP_EVIL: return (race && race_is_evil(race));
        case PROJ_DISP_UNDEAD: return (race && rf_has(race->flags, RF_UNDEAD));

        /* Check these attacks against immunity (polymorphed players) */
        case PROJ_PSI_DRAIN:
        case PROJ_MON_DRAIN:
        case PROJ_DRAIN: return !(race && monster_is_nonliving(race));

        /* Everything else will cause normal damage */
        default: return true;
    }
}


/*
 * Is player vulnerable to an attack?
 *
 * true means extra damage, false means normal damage
 */
static bool is_vulnerable(struct monster_race *race, int type)
{
    switch (type)
    {
        /* Check these attacks against vulnerability (polymorphed players) */
        case PROJ_FIRE: return (race && rf_has(race->flags, RF_HURT_FIRE));
        case PROJ_COLD:
        case PROJ_ICE: return (race && rf_has(race->flags, RF_HURT_COLD));
        case PROJ_LIGHT: return (race && rf_has(race->flags, RF_HURT_LIGHT));

        /* Everything else will cause normal damage */
        default: return false;
    }
}


/*
 * Adjust damage according to resistance or vulnerability.
 *
 * type is the attack type we are checking.
 * dam is the unadjusted damage.
 * dam_aspect is the calc we want (min, avg, max, random).
 * resist is the degree of resistance (-1 = vuln, 3 = immune).
 */
// Notes:
// 1) INCLUDES bolt, beam, ball, BREATH and other effects
// 2) BREATH damage_cap listed in projection.txt. Other spell damage not capped!
int adjust_dam(struct player *p, int type, int dam, aspect dam_aspect, int resist)
{
    int i, denom = 0;

    /* If an actual player exists, get their actual resist */
    if (p && (type < ELEM_MAX))
    {
        /* Hack -- ice damage checks against cold resistance */
        int res_type = ((type == PROJ_ICE)? PROJ_COLD: type);

        resist = ((res_type < ELEM_MAX)? p->state.el_info[res_type].res_level[0]: 0);

        /* Notice element stuff */
        equip_learn_element(p, res_type);
    }

    if (dam <= 0) return 0;

    // IMMUNE
    // Note: in T vulnerability prevents getting full immunity, some dmg must apply
    if (resist == 3)
    {
        if (p)
            ; // later reduced in ~9 times by the loop "for (i = resist; i > 0; i--)"
              // note: we must add (resist < 3) to other vuln if cases!
        else
            return 0; // common PWMA case
    }

    /* Hack -- ACID damage is halved by armour */
    if ((type == PROJ_ACID) && p && minus_ac(p)) dam = (dam + 1) / 2;

    /* Hack -- biofeedback halves "sharp" damage */
    if (p)
    {
        if (p->timed[TMD_BIOFEEDBACK])
        {
            switch (type)
            {
                case PROJ_MISSILE:
                case PROJ_SHOT:
                case PROJ_ARROW:
                case PROJ_BOLT:
                case PROJ_BOULDER:
                case PROJ_SHARD:
                case PROJ_SOUND:
                    dam = (dam + 1) / 2;
                    break;
            }
        }
        // reflection class skill -- only applies if biofeedback is not active
        else if (p->timed[TMD_REFLECTION])
        {
            switch (type)
            {
                case PROJ_MISSILE:
                case PROJ_SHOT:
                case PROJ_ARROW:
                case PROJ_BOLT:
                    dam = (dam + 1) / 2;
                    break;
            }
        }
    }

    /* (Perma)polymorphed #1 Hack -- no damage from certain attacks unless vulnerable */
    // (LIGHT_WEAK, KILL_WALL, DISP_..., DRAIN_...)
    if (p && !is_susceptible(p->poly_race, type))
        dam = 0;

    /* Vulnerable */
    // For p race vulnerability this case possible only till lvl 30 (cause after >30 
    // we do not lower resistance value) OR if you wear cursed item.
    // Note: (resist == -1) applied only to vulnerability - if it's not covered with
    // resistances; while most common case that IT IS covered by resistance, so
    // it stops being vulnerability anymore (as we take there stuff from p "state").
    // So at 30+lvl we hardcode a new vulnerability with "else if (p && p->lev >= 30)"
    if (resist == -1)
    {
        if (p)
        {
            // BA_LIGHT gives very small dmg, so increase it
            if (type == PROJ_LIGHT)
            {
                if (p->lev < 30)
                {
                    dam = dam * 3 / 2; // 50%
                }
                // having -1 at 30+ lvl must be not better than being resistant
                else
                {
                    if (dam * 2 > 425) // BR_LIGHT cap
                        dam = 425 + p->lev; // -1 penalty: up to 50 dmg
                    else
                        dam = (dam * 2) + p->lev; // +100% + ~50
                }
            }

            // so there only TIME
            else if (type == PROJ_TIME)
            {
                if (dam * 3 / 2 > 150) // BR_TIME cap
                    dam = 150 + p->lev; // -1 penalty
                else
                    dam = (dam * 3 / 2) + p->lev; // +50% + ~50
            }
            // all other elements. Note: for DARK_WEAK we have separate case below
            // so there will be regular BR_DARK (as BA_DARK is 60+ lvl)
            else
                dam = dam * 4 / 3; // 33%
        }
        else
            return (dam * 4 / 3); // common PWMA case
    }
    /* (Perma)polymorphed #2 Hack -- extra damage from certain attacks if vulnerable */
    // (FIRE, COLD, ICE, LIGHT)
    // (note: actually being poly is more dangerous as regular vuln xtra dmg
    //  not applied if it kills you; while poly vuln applied always and in bigger number)
    else if (p && is_vulnerable(p->poly_race, type))
    {
        // LIGHT special
        if (type == PROJ_LIGHT)
        {
            if (p->lev < 30)
            {
                dam = dam * 3 / 2; // 50%
            }
            // having -1 at 30+ lvl must be not better than being resistant
            else
            {
                if (dam * 2 > 425) // BR_LIGHT cap
                    dam = 425; //
                else
                    dam = (dam * 2); // +100%
            }
        }
        else // FIRE, COLD, ICE etc
            dam = dam * 4 / 3;
    }
    // p races vulnerabilities (covered) - must always give extra dmg after lvl 30
    else if (p && p->lev >= 30 && resist < 3) // except immunity
    {
        int vuln_xtra_dmg = 0; // we don't want apply xtra vuln damage if it will kill p

        // FIRE
        if (type == PROJ_FIRE && (streq(p->race->name, "Ent") ||
            streq(p->race->name, "Undead") || streq(p->race->name, "Frostmen") ||
            streq(p->race->name, "Wraith")))
        {
            vuln_xtra_dmg = dam / 8; // 12.5%
        }

        // COLD
        else if (type == PROJ_COLD && (streq(p->race->name, "Balrog") ||
            streq(p->race->name, "Imp")))
        {
            vuln_xtra_dmg = dam / 8; // 12.5%
        }

        // LIGHT
        // Note: BA_LIGHT (main source of light dmg) it's very minor effect...
        /*
            Spell       | Average Damage (for 60 SPELL_POWER)
            ------------|----------------
            BA_ACID     | 105.0
            BA_ELEC     | 53.0
            BA_FIRE     | 115.0
            BA_COLD     | 55.0
            BA_POIS     | 82.5
            BA_SHAR     | 55.0
            BA_NETH     | 295.0
            BA_WATE     | 125.0
            BA_MANA     | 355.0
            BA_HOLY     | 55.0
            BA_DARK     | 295.0
            BA_LIGHT    | 55.0
            MIND_BLAST  | 36.0
            WOUND       | 66.0
        */
        // so lets make it 2x as it's most popular vulnerability
        else if (type == PROJ_LIGHT && (streq(p->race->name, "Goblin") ||
                 streq(p->race->name, "Ogre") || streq(p->race->name, "Troll") ||
                 streq(p->race->name, "Orc") || streq(p->race->name, "Dark-Elf") ||
                 streq(p->race->name, "Undead") || streq(p->race->name, "Vampire") ||
                 streq(p->race->name, "Demonic") || streq(p->race->name, "Balrog") ||
                 streq(p->race->name, "Spider") || streq(p->race->name, "Troglodyte") ||
                 streq(p->race->name, "Wraith") || streq(p->race->name, "Beholder") ||
                 streq(p->race->name, "Ooze")))
        {
            // Double damage, but cap total at 425 (BR_LIGHT cap)
            if (dam * 2 <= 425)
                vuln_xtra_dmg = dam; // Normal doubling
        }

        // TIME
        else if (type == PROJ_TIME && streq(p->race->name, "Celestial"))
        {
            vuln_xtra_dmg = dam / 2; // 50%
        }
        // DARK (very powerful attacks sometimes)
        else if (type == PROJ_DARK && (streq(p->race->name, "Celestial") ||
                 streq(p->race->name, "Maiar") || streq(p->race->name, "Wisp")))
        {
            vuln_xtra_dmg = dam / 20; // 5%
        }
        
        // NOTE: wew shouldn't put there stuff like:
        /*
        "all other elements
        else
            vuln_xtra_dmg = dam / 10; // 10%
        */
        // because there we don't have vuln check! So general 'else' will just increase ALL dmg.
        // So each racial vuln must be hardcoded with the race name

        // Now... Apply vulnerability extra damage ONLY if it won't kill the player
        if (vuln_xtra_dmg > 0)
        {
            if (p->chp - (dam + vuln_xtra_dmg) >= 1)
                dam += vuln_xtra_dmg;
        }
    }


    // SPECIAL case for DARKNESS spell and dark-vulnerable p races
    // (there is no DARK_WEAK resistances.. so it's separate case)
    if (type == PROJ_DARK_WEAK && p && resist < 3 && (streq(p->race->name, "Maiar") ||
        streq(p->race->name, "Celestial") || streq(p->race->name, "Wisp")))
    {
        // DARKNESS spell makes 10 damage by default. We add 10% max HP extra
        int dark_weak_xtra_dmg = p->mhp / 10;
        // Only add extra damage if it won't kill the player
        // (but p still can die due regular 10 dmg)
        if (p->chp - (dam + dark_weak_xtra_dmg) >= 1) 
            dam += dark_weak_xtra_dmg;
        
        // note: it's the only case which ok to do so, as there are no
        // resistance to DARK_WEAK, so we won't mess with processing dam with resistance
        // loop below (if (denom) dam = ...)
    }

    // SPECIAL case for LIGHT_WEAK and Vampire race
    else if (type == PROJ_LIGHT_WEAK && p && resist < 3 &&
             (streq(p->race->name, "Vampire") || streq(p->race->name, "Undead")))
    {
        int light_weak_xtra_dmg = p->mhp / 10;
        if (p->chp - (dam + light_weak_xtra_dmg) >= 1)
            dam += light_weak_xtra_dmg;
    }

    /*
     * Variable resists vary the denominator, so we need to invert the logic
     * of dam_aspect. (m_bonus is unused)
     */
    switch (dam_aspect)
    {
        case MINIMISE:
            denom = randcalc(projections[type].denominator, 0, MAXIMISE);
            break;
        case MAXIMISE:
            denom = randcalc(projections[type].denominator, 0, MINIMISE);
            break;
        case AVERAGE:
        case RANDOMISE:
            denom = randcalc(projections[type].denominator, 0, dam_aspect);
            break;
    }

    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
    // now calculate final damage (for resist == 0, resist == 1, resist == 2)
    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////
    // Examples:

    // 1) Single Resistance (resist == 1):
    // Loop runs 1 time
    // dam = dam * numerator / denominator
    // For acid: dam = dam * 1 / 3 = 33% damage

    // 2)Double Resistance (resist == 2):
    // Loop runs 2 times
    // First iteration: dam = dam * 1 / 3
    // Second iteration: dam = (dam * 1/3) * 1 / 3 = dam * 1/9
    // Result: 11% damage

    if (resist > 0) // -- we need this check so we won't use -1 vuln in this loop
    {
        for (i = resist; i > 0; i--)
        {
            if (denom) dam = dam * projections[type].numerator / denom;
        }
    }
    //////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////


    // never kill fully healed player with 1 magic blow (if not on CD)
    if (p && p->chp == p->mhp && p->y_cooldown == 0)
    {
            if (p->chp - dam < 1) // TODO: special case for p->timed[TMD_MANASHIELD] ?
            {
                msg(p, "%s (level %d) survived a magical blast of %d damage - by a hair!",
                        p->name, p->lev, dam);
                dam = p->chp - 1; // make damage which will leave 1 hp

                // apply 'innate' CD so it won't repeat too often
                p->y_cooldown = 1000;
            }
    }

    return dam;
}


/*
 * Player handlers
 */


/*
 * Drain stats at random
 *
 * p is the affected player
 * num is the number of points to drain
 */
static void project_player_drain_stats(struct player *p, int num)
{
    int i, k = 0;
    const char *act = NULL;

    for (i = 0; i < num; i++)
    {
        switch (randint1(STAT_MAX))
        {
            case 1: k = STAT_STR; act = "strong"; break;
            case 2: k = STAT_INT; act = "bright"; break;
            case 3: k = STAT_WIS; act = "wise"; break;
            case 4: k = STAT_DEX; act = "agile"; break;
            case 5: k = STAT_CON; act = "hale"; break;
            case 6: k = STAT_CHR; act = "beautiful"; break;
        }

        msg(p, "You're not as %s as you used to be...", act);
        player_stat_dec(p, k, false);
    }
}


void project_player_time_effects(struct player *p, struct source *who)
{
    /* Life draining */
    if (one_in_(2))
    {
        int drain = (100 + (p->exp / 100) * z_info->life_drain_percent) -
        randint0(p->state.stat_ind[STAT_CHR]);

        msg(p, "You feel your life force draining away!");
        player_exp_lose(p, drain, false);
    }

    /* Drain some stats */
    else if (!one_in_(5))
    {
        int num = 1;

        /* PWMAngband: time resistance prevents nastiest effect */
        if (!player_resists(p, ELEM_TIME)) num = 2;

        project_player_drain_stats(p, num);
    }

    /* Drain all stats */
    else
    {
        int perma = p->state.el_info[ELEM_TIME].res_level[0];

        if (p->timed[TMD_ANCHOR]) perma--;

        if (who->monster)
            update_smart_learn(who->monster, p, 0, 0, ELEM_TIME);

        /* PWMAngband: permanent time resistance prevents the effect completely */
        if (perma)
            msg(p, "You resist the effect!");

        /* PWMAngband: space/time anchor prevents nastiest effect */
        else if (p->timed[TMD_ANCHOR])
        {
            /* Life draining */
            if (randint1(9) < 6)
            {
                msg(p, "You feel your life force draining away!");
                player_exp_lose(p, 100 + (p->exp / 100) * z_info->life_drain_percent, false);
            }

            /* Drain one stat */
            else
                project_player_drain_stats(p, 1);
        }

        /* Normal case */
        else
        {
            int i;

            msg(p, "You're not as powerful as you used to be...");
            for (i = 0; i < STAT_MAX; i++) player_stat_dec(p, i, false);
        }
    }
}


typedef struct project_player_handler_context_s
{
    /* Input values */
    struct source *origin;
    int r;
    struct chunk *cave;
    struct loc grid;
    int dam;
    int type;
    int power;

    /* Return values */
    bool obvious;
} project_player_handler_context_t;


typedef int (*project_player_handler_f)(project_player_handler_context_t *);


static int project_player_handler_ACID(project_player_handler_context_t *context)
{
    struct player *p = player_get(0 - square(context->cave, &context->grid)->mon);

    if (context->origin->monster)
        update_smart_learn(context->origin->monster, p, 0, 0, ELEM_ACID);
    if (player_is_immune(p, ELEM_ACID))
    {
        msg(p, "You resist the effect!");
        return 0;
    }

    inven_damage(p, PROJ_ACID, MIN(context->dam * 5, 300));

    return 0;
}


static int project_player_handler_ELEC(project_player_handler_context_t *context)
{
    struct player *p = player_get(0 - square(context->cave, &context->grid)->mon);

    if (context->origin->monster)
        update_smart_learn(context->origin->monster, p, 0, 0, ELEM_ELEC);
    if (player_is_immune(p, ELEM_ELEC))
    {
        msg(p, "You resist the effect!");
        return 0;
    }

    inven_damage(p, PROJ_ELEC, MIN(context->dam * 5, 300));

    return 0;
}


static int project_player_handler_FIRE(project_player_handler_context_t *context)
{
    struct player *p = player_get(0 - square(context->cave, &context->grid)->mon);
    bool check = (context->origin->monster? false: true);

    if (context->origin->monster)
        update_smart_learn(context->origin->monster, p, 0, 0, ELEM_FIRE);
    if (player_is_immune(p, ELEM_FIRE))
    {
        msg(p, "You resist the effect!");
        return 0;
    }

    inven_damage(p, PROJ_FIRE, MIN(context->dam * 5, 300));

    /* Occasional side-effects for powerful fire attacks */
    if (context->power >= 80)
    {
        if (randint0(context->dam) > 500)
        {
            struct source who_body;
            struct source *who = &who_body;

            msg(p, "The intense heat saps you.");
            source_player(who, get_player_index(get_connection(p->conn)), p);
            effect_simple(EF_DRAIN_STAT, who, "0", STAT_STR, 0, 0, 0, 0, &context->obvious);
        }
        if (randint0(context->dam) > 500)
        {
            if (player_inc_timed(p, TMD_BLIND, randint1(context->dam / 100), true, check))
                msg(p, "Your eyes fill with smoke!");
        }
        if (randint0(context->dam) > 500)
        {
            if (player_inc_timed(p, TMD_POISONED, randint1(context->dam / 10), true, check))
                msg(p, "You are assailed by poisonous fumes!");
        }
    }

    return 0;
}


static int project_player_handler_COLD(project_player_handler_context_t *context)
{
    struct player *p = player_get(0 - square(context->cave, &context->grid)->mon);

    if (context->origin->monster)
        update_smart_learn(context->origin->monster, p, 0, 0, ELEM_COLD);
    if (player_is_immune(p, ELEM_COLD))
    {
        msg(p, "You resist the effect!");
        return 0;
    }

    inven_damage(p, PROJ_COLD, MIN(context->dam * 5, 300));

    /* Occasional side-effects for powerful cold attacks */
    if (context->power >= 80)
    {
        if (randint0(context->dam) > 500)
        {
            struct source who_body;
            struct source *who = &who_body;

            msg(p, "The cold seeps into your bones.");
            source_player(who, get_player_index(get_connection(p->conn)), p);
            effect_simple(EF_DRAIN_STAT, who, "0", STAT_DEX, 0, 0, 0, 0, &context->obvious);
        }
        if (randint0(context->dam) > 500)
        {
            if (player_of_has(p, OF_HOLD_LIFE))
                equip_learn_flag(p, OF_HOLD_LIFE);
            else
            {
                int drain = context->dam;

                msg(p, "The cold withers your life force!");
                player_exp_lose(p, drain, false);
            }
        }
    }

    return 0;
}


static int project_player_handler_POIS(project_player_handler_context_t *context)
{
    struct player *p = player_get(0 - square(context->cave, &context->grid)->mon);
    bool check = (context->origin->monster? false: true);
    int xtra = 0;

    if (player_resists(p, ELEM_POIS))
    {
        msg(p, "You resist the effect!");
        return 0;
    }

    player_inc_timed(p, TMD_POISONED, 10 + randint1(context->dam), true, check);


    /* Occasional side-effects for powerful poison attacks */
    if (context->power >= 60)
    {
        if (randint0(context->dam) > 200)
        {
            if (!player_is_immune(p, ELEM_ACID))
            {
                int dam = context->dam / 5;

                msg(p, "The venom stings your skin!");
                inven_damage(p, PROJ_ACID, dam);
                xtra = adjust_dam(p, PROJ_ACID, dam, RANDOMISE,
                    p->state.el_info[PROJ_ACID].res_level[0]);
            }
        }
        if (randint0(context->dam) > 200)
        {
            struct source who_body;
            struct source *who = &who_body;

            msg(p, "The stench sickens you.");
            source_player(who, get_player_index(get_connection(p->conn)), p);
            effect_simple(EF_DRAIN_STAT, who, "0", STAT_CON, 0, 0, 0, 0, &context->obvious);
        }
    }

    return xtra;
}


static int project_player_handler_LIGHT(project_player_handler_context_t *context)
{
    struct player *p = player_get(0 - square(context->cave, &context->grid)->mon);
    bool check = (context->origin->monster? false: true);

    if (context->origin->monster)
        update_smart_learn(context->origin->monster, p, OF_PROT_BLIND, 0, -1);
    if (player_resists(p, ELEM_LIGHT) || player_of_has(p, OF_PROT_BLIND))
    {
        msg(p, "You resist the effect!");
        return 0;
    }

    player_inc_timed(p, TMD_BLIND, 2 + randint1(5), true, check);

    /* Confusion for strong unresisted light */
    if (context->dam > 300)
    {
        /* Check for resistance before issuing a message. */
        if (player_inc_check(p, NULL, TMD_CONFUSED, false)) msg(p, "You are dazzled!");
        player_inc_timed(p, TMD_CONFUSED, 2 + randint1(context->dam / 100), true, check);
    }

    return 0;
}


static int project_player_handler_DARK(project_player_handler_context_t *context)
{
    struct player *p = player_get(0 - square(context->cave, &context->grid)->mon);
    bool check = (context->origin->monster? false: true);

    if (context->origin->monster)
        update_smart_learn(context->origin->monster, p, OF_PROT_BLIND, 0, -1);
    if (player_resists(p, ELEM_DARK) || player_of_has(p, OF_PROT_BLIND))
    {
        msg(p, "You resist the effect!");
        return 0;
    }

    player_inc_timed(p, TMD_BLIND, 2 + randint1(5), true, check);

    /* Unresisted dark from powerful monsters is bad news */
    if (context->power >= 70)
    {
        /* Life draining */
        if (randint0(context->dam) > 100)
        {
            if (player_of_has(p, OF_HOLD_LIFE))
                equip_learn_flag(p, OF_HOLD_LIFE);
            else
            {
                int drain = context->dam;

                msg(p, "The darkness steals your life force!");
                player_exp_lose(p, drain, false);
            }
        }

        /* Slowing */
        if (randint0(context->dam) > 200)
        {
            msg(p, "You feel unsure of yourself in the darkness.");
            player_inc_timed(p, TMD_SLOW, context->dam / 100, true, false);
        }

        /* Amnesia */
        if (randint0(context->dam) > 300 && !player_of_has(p, OF_PROT_AMNESIA))
        {
            msg(p, "Darkness penetrates your mind!");
            player_inc_timed(p, TMD_AMNESIA, context->dam / 100, true, false);
        }
    }

    return 0;
}


static int project_player_handler_SOUND(project_player_handler_context_t *context)
{
    struct player *p = player_get(0 - square(context->cave, &context->grid)->mon);
    bool check = (context->origin->monster? false: true);

    if (context->origin->monster)
        update_smart_learn(context->origin->monster, p, OF_PROT_STUN, 0, -1);
    if (player_resists(p, ELEM_SOUND) || player_of_has(p, OF_PROT_STUN))
    {
        msg(p, "You resist the effect!");
        return 0;
    }

    /* Stun */
    player_inc_timed(p, TMD_STUN, MIN(5 + randint1(context->dam / 3), 35), true, check);

    /* Confusion for strong unresisted sound */
    if (context->dam > 300)
    {
        /* Check for resistance before issuing a message. */
        if (player_inc_check(p, NULL, TMD_CONFUSED, false)) msg(p, "The noise disorients you.");
        player_inc_timed(p, TMD_CONFUSED, 2 + randint1(context->dam / 100), true, check);
    }

    return 0;
}


static int project_player_handler_SHARD(project_player_handler_context_t *context)
{
    struct player *p = player_get(0 - square(context->cave, &context->grid)->mon);
    bool check = (context->origin->monster? false: true);

    if (player_resists(p, ELEM_SHARD))
    {
        msg(p, "You resist the effect!");
        return 0;
    }

    /* Cuts */
    player_inc_timed(p, TMD_CUT, randint1(context->dam), true, check);

    return 0;
}


static int project_player_handler_NEXUS(project_player_handler_context_t *context)
{
    struct player *p = player_get(0 - square(context->cave, &context->grid)->mon);
    struct source who_body;
    struct source *who = &who_body;

    source_player(who, get_player_index(get_connection(p->conn)), p);

    if (player_resists(p, ELEM_NEXUS))
    {
        msg(p, "You resist the effect!");
        return 0;
    }

    /* Stat swap */
    if (magik(p->state.skills[SKILL_SAVE]))
        msg(p, "You avoid the effect!");
    else
        player_inc_timed(p, TMD_SCRAMBLE, randint0(20) + 20, true, true);

    /* Teleport to */
    if (one_in_(3))
    {
        if (context->origin->monster)
        {
            who->monster = context->origin->monster;
            effect_simple(EF_TELEPORT_TO, who, "0", 0, 0, 0, 0, 0, NULL);
        }
        else
        {
            effect_simple(EF_TELEPORT_TO, who, "0", 0, 0, 0, context->origin->player->grid.y,
                context->origin->player->grid.x, NULL);
        }

        /* Hack -- get new location */
        loc_copy(&context->grid, &p->grid);
    }

    /* Teleport level */
    else if (one_in_(4))
    {
        if (magik(p->state.skills[SKILL_SAVE]))
        {
            msg(p, "You avoid the effect!");
            return 0;
        }
        effect_simple(EF_TELEPORT_LEVEL, who, "0", 0, 0, 0, 0, 0, NULL);
    }

    /* Teleport */
    else
    {
        effect_simple(EF_TELEPORT, who, "200", 0, 0, 0, 0, 0, NULL);

        /* Hack -- get new location */
        loc_copy(&context->grid, &p->grid);
    }

    return 0;
}


static int project_player_handler_NETHER(project_player_handler_context_t *context)
{
    struct player *p = player_get(0 - square(context->cave, &context->grid)->mon);
    int drain = 20 + (p->exp / 50) * z_info->life_drain_percent;

    if (context->origin->monster)
        update_smart_learn(context->origin->monster, p, OF_HOLD_LIFE, 0, -1);
    if (player_resists(p, ELEM_NETHER) || player_of_has(p, OF_HOLD_LIFE))
    {
        msg(p, "You resist the effect!");
        return 0;
    }

    /* Life draining */
    msg(p, "You feel your life force draining away!");
    player_exp_lose(p, drain, false);

    /* Powerful nether attacks have further side-effects */
    if (context->power >= 80)
    {
        /* Mana loss */
        if ((randint0(context->dam) > 100) && p->msp)
        {
            int old_num = get_player_num(p);

            msg(p, "Your mind is dulled.");
            p->csp -= MIN(p->csp, context->dam / 10);
            redraw_picture(p, old_num);
            p->upkeep->redraw |= PR_MANA;
        }

        /* Loss of energy */
        if (randint0(context->dam) > 200)
        {
            msg(p, "Your energy is sapped!");
            p->energy = 0;
            p->extra_energy = 0;
        }
    }

    return 0;
}


static int project_player_handler_CHAOS(project_player_handler_context_t *context)
{
    struct player *p = player_get(0 - square(context->cave, &context->grid)->mon);
    bool check = (context->origin->monster? false: true);

    if (context->origin->monster)
    {
        update_smart_learn(context->origin->monster, p, OF_PROT_CONF, 0, -1);
        update_smart_learn(context->origin->monster, p, OF_HOLD_LIFE, 0, -1);
    }
    if (player_resists(p, ELEM_CHAOS))
    {
        msg(p, "You resist the effect!");
        return 0;
    }

    /* Hallucination */
    player_inc_timed(p, TMD_IMAGE, randint1(10), true, check);

    /* Confusion */
    if (player_of_has(p, OF_PROT_CONF))
        msg(p, "You resist the effect!");
    else
        player_inc_timed(p, TMD_CONFUSED, 10 + randint0(20), true, check);

    /* Life draining */
    if (player_of_has(p, OF_HOLD_LIFE))
        msg(p, "You resist the effect!");
    else
    {
        int drain = 50 + (p->exp / 50) * z_info->life_drain_percent;

        msg(p, "You feel your life force draining away!");
        player_exp_lose(p, drain, false);
    }

    return 0;
}


static int project_player_handler_DISEN(project_player_handler_context_t *context)
{
    struct player *p = player_get(0 - square(context->cave, &context->grid)->mon);
    struct source who_body;
    struct source *who = &who_body;

    if (player_resists(p, ELEM_DISEN))
    {
        msg(p, "You resist the effect!");
        return 0;
    }

    /* Disenchant gear */
    source_player(who, get_player_index(get_connection(p->conn)), p);
    effect_simple(EF_DISENCHANT, who, "0", 0, 0, 0, 0, 0, NULL);

    return 0;
}


static int project_player_handler_WATER(project_player_handler_context_t *context)
{
    struct player *p = player_get(0 - square(context->cave, &context->grid)->mon);
    bool check = (context->origin->monster? false: true);

    if (context->origin->monster)
    {
        update_smart_learn(context->origin->monster, p, OF_PROT_CONF, 0, -1);
        update_smart_learn(context->origin->monster, p, OF_PROT_STUN, 0, -1);
    }

    /* Confusion */
    if (player_of_has(p, OF_PROT_CONF))
        msg(p, "You resist the effect!");
    else
        player_inc_timed(p, TMD_CONFUSED, 5 + randint1(5), true, check);

    /* Stun */
    if (player_of_has(p, OF_PROT_STUN))
        msg(p, "You resist the effect!");
    else
        player_inc_timed(p, TMD_STUN, randint1(40), true, check);

    return 0;
}


static int project_player_handler_ICE(project_player_handler_context_t *context)
{
    struct player *p = player_get(0 - square(context->cave, &context->grid)->mon);
    bool check = (context->origin->monster? false: true);

    if (context->origin->monster)
    {
        update_smart_learn(context->origin->monster, p, 0, 0, ELEM_SHARD);
        update_smart_learn(context->origin->monster, p, OF_PROT_STUN, 0, -1);
        update_smart_learn(context->origin->monster, p, 0, 0, ELEM_COLD);
    }

    if (player_is_immune(p, ELEM_COLD))
        msg(p, "You resist the effect!");
    else
        inven_damage(p, PROJ_COLD, MIN(context->dam * 5, 300));

    /* Cuts */
    if (player_resists(p, ELEM_SHARD))
        msg(p, "You resist the effect!");
    else
        player_inc_timed(p, TMD_CUT, damroll(5, 8), true, check);

    /* Stun */
    if (player_of_has(p, OF_PROT_STUN))
        msg(p, "You resist the effect!");
    else
        player_inc_timed(p, TMD_STUN, randint1(15), true, check);

    return 0;
}


static int project_player_handler_GRAVITY(project_player_handler_context_t *context)
{
    struct player *p = player_get(0 - square(context->cave, &context->grid)->mon);
    bool check = (context->origin->monster? false: true);

    msg(p, "Gravity warps around you.");

    if (context->origin->monster)
        update_smart_learn(context->origin->monster, p, OF_PROT_STUN, 0, -1);

    /* Blink */
    if (randint1(127) > p->lev)
    {
        struct source who_body;
        struct source *who = &who_body;

        source_player(who, get_player_index(get_connection(p->conn)), p);
        effect_simple(EF_TELEPORT, who, "5", 0, 0, 0, 0, 0, NULL);

        /* Hack -- get new location */
        loc_copy(&context->grid, &p->grid);
    }

    /* Slow */
    player_inc_timed(p, TMD_SLOW, 4 + randint0(4), true, check);

    /* Stun */
    if (player_of_has(p, OF_PROT_STUN))
        msg(p, "You resist the effect!");
    else
        player_inc_timed(p, TMD_STUN, MIN(5 + randint1(context->dam / 3), 35), true, check);

    return 0;
}


static int project_player_handler_INERTIA(project_player_handler_context_t *context)
{
    struct player *p = player_get(0 - square(context->cave, &context->grid)->mon);
    bool check = (context->origin->monster? false: true);

    /* Slow */
    player_inc_timed(p, TMD_SLOW, 4 + randint0(4), true, check);

    return 0;
}


static int project_player_handler_FORCE(project_player_handler_context_t *context)
{
    struct player *p = player_get(0 - square(context->cave, &context->grid)->mon);
    bool check = (context->origin->monster? false: true);
    struct loc centre;
    struct source who_body;
    struct source *who = &who_body;

    /* Get location of caster (assumes index of caster is not zero) */
    origin_get_loc(&centre, context->origin);

    if (context->origin->monster)
        update_smart_learn(context->origin->monster, p, OF_PROT_STUN, 0, -1);
    if (player_of_has(p, OF_PROT_STUN))
    {
        msg(p, "You resist the effect!");
        return 0;
    }

    /* Stun */
    player_inc_timed(p, TMD_STUN, randint1(20), true, check);

    /* Thrust player away. */
    source_player(who, get_player_index(get_connection(p->conn)), p);
    who->trap = context->origin->trap;
    thrust_away(context->cave, who, &centre, 3 + context->dam / 20);

    /* Hack -- get new location */
    loc_copy(&context->grid, &p->grid);

    return 0;
}


static int project_player_handler_TIME(project_player_handler_context_t *context)
{
    struct player *p = player_get(0 - square(context->cave, &context->grid)->mon);

    project_player_time_effects(p, context->origin);

    return 0;
}


static int project_player_handler_PLASMA(project_player_handler_context_t *context)
{
    struct player *p = player_get(0 - square(context->cave, &context->grid)->mon);
    bool check = (context->origin->monster? false: true);

    if (context->origin->monster)
        update_smart_learn(context->origin->monster, p, OF_PROT_STUN, 0, -1);
    if (player_of_has(p, OF_PROT_STUN))
    {
        msg(p, "You resist the effect!");
        return 0;
    }

    /* Stun */
    if (RNG % 2) // make it more rare to prevent plasma hounds insta-stun
        player_inc_timed(p, TMD_STUN, MIN(5 + randint1(context->dam * 3 / 4), 35), true, check);

    return 0;
}


static int project_player_handler_METEOR(project_player_handler_context_t *context) {return 0;}
static int project_player_handler_MISSILE(project_player_handler_context_t *context) {return 0;}
static int project_player_handler_MANA(project_player_handler_context_t *context) {return 0;}
static int project_player_handler_HOLY_ORB(project_player_handler_context_t *context) {return 0;}
static int project_player_handler_SHOT(project_player_handler_context_t *context) {return 0;}
static int project_player_handler_ARROW(project_player_handler_context_t *context) {return 0;}
static int project_player_handler_BOLT(project_player_handler_context_t *context) {return 0;}
static int project_player_handler_BOULDER(project_player_handler_context_t *context) {return 0;}
static int project_player_handler_LIGHT_WEAK(project_player_handler_context_t *context) {return 0;}
static int project_player_handler_DARK_WEAK(project_player_handler_context_t *context) {return 0;}
static int project_player_handler_KILL_WALL(project_player_handler_context_t *context) {return 0;}
static int project_player_handler_KILL_DOOR(project_player_handler_context_t *context) {return 0;}
static int project_player_handler_KILL_TRAP(project_player_handler_context_t *context) {return 0;}
static int project_player_handler_MAKE_DOOR(project_player_handler_context_t *context) {return 0;}
static int project_player_handler_MAKE_TRAP(project_player_handler_context_t *context) {return 0;}
static int project_player_handler_STONE_WALL(project_player_handler_context_t *context) {return 0;}
static int project_player_handler_RAISE(project_player_handler_context_t *context) {return 0;}


/* PvP handlers */


static int project_player_handler_AWAY_EVIL(project_player_handler_context_t *context)
{
    struct player *p = player_get(0 - square(context->cave, &context->grid)->mon);

    /* Only evil players */
    if (p->poly_race && race_is_evil(p->poly_race))
    {
        char dice[5];
        struct source who_body;
        struct source *who = &who_body;

        strnfmt(dice, sizeof(dice), "%d", context->dam);
        source_player(who, get_player_index(get_connection(p->conn)), p);
        effect_simple(EF_TELEPORT, who, dice, 0, 0, 0, 0, 0, NULL);

        /* Hack -- get new location */
        loc_copy(&context->grid, &p->grid);
    }
    else
        msg(p, "You resist the effect!");

    return 0;
}


static int project_player_handler_AWAY_SPIRIT(project_player_handler_context_t *context)
{
    struct player *p = player_get(0 - square(context->cave, &context->grid)->mon);

    /* Only spirit players */
    if (p->poly_race && rf_has(p->poly_race->flags, RF_SPIRIT))
    {
        char dice[5];
        struct source who_body;
        struct source *who = &who_body;

        strnfmt(dice, sizeof(dice), "%d", context->dam);
        source_player(who, get_player_index(get_connection(p->conn)), p);
        effect_simple(EF_TELEPORT, who, dice, 0, 0, 0, 0, 0, NULL);

        /* Hack -- get new location */
        loc_copy(&context->grid, &p->grid);
    }
    else
        msg(p, "You resist the effect!");

    return 0;
}


static int project_player_handler_AWAY_ALL(project_player_handler_context_t *context)
{
    struct player *p = player_get(0 - square(context->cave, &context->grid)->mon);
    char dice[5];
    struct source who_body;
    struct source *who = &who_body;

    strnfmt(dice, sizeof(dice), "%d", context->dam);
    source_player(who, get_player_index(get_connection(p->conn)), p);
    effect_simple(EF_TELEPORT, who, dice, 0, 0, 0, 0, 0, NULL);

    /* Hack -- get new location */
    loc_copy(&context->grid, &p->grid);

    return 0;
}


static void apply_fear(struct player *p)
{
    if (player_of_has(p, OF_PROT_FEAR))
        msg(p, "You resist the effect!");
    else
        player_inc_timed(p, TMD_AFRAID, 3 + randint1(4), true, true);
}


static int project_player_handler_TURN_UNDEAD(project_player_handler_context_t *context)
{
    struct player *p = player_get(0 - square(context->cave, &context->grid)->mon);

    /* Only undead players */
    if (p->poly_race && rf_has(p->poly_race->flags, RF_UNDEAD))
        apply_fear(p);
    else
        msg(p, "You resist the effect!");

    return 0;
}


static int project_player_handler_TURN_LIVING(project_player_handler_context_t *context)
{
    struct player *p = player_get(0 - square(context->cave, &context->grid)->mon);

    /* Only living players */
    if (!p->poly_race)
        apply_fear(p);
    else if (!rf_has(p->poly_race->flags, RF_UNDEAD) && !rf_has(p->poly_race->flags, RF_NONLIVING))
        apply_fear(p);
    else
        msg(p, "You resist the effect!");

    return 0;
}


static int project_player_handler_TURN_ALL(project_player_handler_context_t *context)
{
    struct player *p = player_get(0 - square(context->cave, &context->grid)->mon);

    apply_fear(p);

    return 0;
}


static int project_player_handler_DISP_UNDEAD(project_player_handler_context_t *context) {return 0;}
static int project_player_handler_DISP_EVIL(project_player_handler_context_t *context) {return 0;}
static int project_player_handler_DISP_ALL(project_player_handler_context_t *context) {return 0;}


static void apply_paralysis(struct player *p)
{
    if (player_of_has(p, OF_FREE_ACT))
        msg(p, "You resist the effect!");
    else
        player_inc_timed(p, TMD_PARALYZED, 3 + randint1(4), true, true);
}


static int project_player_handler_SLEEP_UNDEAD(project_player_handler_context_t *context)
{
    struct player *p = player_get(0 - square(context->cave, &context->grid)->mon);

    /* Only undead players */
    if (p->poly_race && rf_has(p->poly_race->flags, RF_UNDEAD))
        apply_paralysis(p);
    else
        msg(p, "You resist the effect!");

    return 0;
}


static int project_player_handler_SLEEP_EVIL(project_player_handler_context_t *context)
{
    struct player *p = player_get(0 - square(context->cave, &context->grid)->mon);

    /* Only evil players */
    if (p->poly_race && race_is_evil(p->poly_race))
        apply_paralysis(p);
    else
        msg(p, "You resist the effect!");

    return 0;
}


static int project_player_handler_SLEEP_ALL(project_player_handler_context_t *context)
{
    struct player *p = player_get(0 - square(context->cave, &context->grid)->mon);

    apply_paralysis(p);

    return 0;
}


static int project_player_handler_MON_CLONE(project_player_handler_context_t *context)
{
    struct player *p = player_get(0 - square(context->cave, &context->grid)->mon);

    /* Disable */
    msg(p, "You resist the effect!");

    return 0;
}


static int project_player_handler_MON_POLY(project_player_handler_context_t *context)
{
    struct player *p = player_get(0 - square(context->cave, &context->grid)->mon);

    if (player_resists(p, ELEM_NEXUS))
        msg(p, "You resist the effect!");

    /* Swap stats */
    else if (one_in_(2))
    {
        if (magik(p->state.skills[SKILL_SAVE]))
            msg(p, "You avoid the effect!");
        else
            player_inc_timed(p, TMD_SCRAMBLE, randint0(20) + 20, true, true);
    }

    /* Poly bat */
    else
    {
        char killer[NORMAL_WID];

        my_strcpy(killer, context->origin->player->name, sizeof(killer));
        poly_bat(p, 10 + context->dam * 4, killer);
    }

    return 0;
}


static int project_player_handler_MON_HEAL(project_player_handler_context_t *context)
{
    struct player *p = player_get(0 - square(context->cave, &context->grid)->mon);

    /* Heal */
    hp_player(p, context->dam);

    return 0;
}


static int project_player_handler_MON_SPEED(project_player_handler_context_t *context)
{
    project_player_handler_MON_CLONE(context);

    return 0;
}


static int project_player_handler_MON_SLOW(project_player_handler_context_t *context)
{
    struct player *p = player_get(0 - square(context->cave, &context->grid)->mon);

    /* Slow */
    if (player_of_has(p, OF_FREE_ACT))
        msg(p, "You resist the effect!");
    else
        player_inc_timed(p, TMD_SLOW, 3 + randint1(4), true, true);

    return 0;
}


static int project_player_handler_MON_CONF(project_player_handler_context_t *context)
{
    struct player *p = player_get(0 - square(context->cave, &context->grid)->mon);

    /* Confusion */
    if (player_of_has(p, OF_PROT_CONF))
        msg(p, "You resist the effect!");
    else
        player_inc_timed(p, TMD_CONFUSED, 3 + randint1(4), true, true);

    return 0;
}


static int project_player_handler_MON_HOLD(project_player_handler_context_t *context)
{
    struct player *p = player_get(0 - square(context->cave, &context->grid)->mon);

    /* Paralysis */
    if (player_of_has(p, OF_FREE_ACT))
        msg(p, "You resist the effect!");
    else
        player_inc_timed(p, TMD_PARALYZED, 3 + randint1(4), true, true);

    return 0;
}


static int project_player_handler_MON_STUN(project_player_handler_context_t *context)
{
    struct player *p = player_get(0 - square(context->cave, &context->grid)->mon);

    /* Stun */
    if (player_of_has(p, OF_PROT_STUN))
        msg(p, "You resist the effect!");
    else
        player_inc_timed(p, TMD_STUN, 3 + randint1(4), true, true);

    return 0;
}


static int project_player_handler_MON_DRAIN(project_player_handler_context_t *context) {return 0;}
static int project_player_handler_MON_CRUSH(project_player_handler_context_t *context) {return 0;}


static int project_player_handler_PSI(project_player_handler_context_t *context)
{
    struct player *p = player_get(0 - square(context->cave, &context->grid)->mon);

    switch (randint1(4))
    {
        /* Confusion */
        case 1:
        {
            if (player_of_has(p, OF_PROT_CONF))
                msg(p, "You resist the effect!");
            else
                player_inc_timed(p, TMD_CONFUSED, 3 + randint1(4), true, true);
            break;
        }

        /* Stun */
        case 2:
        {
            if (player_of_has(p, OF_PROT_STUN))
                msg(p, "You resist the effect!");
            else
                player_inc_timed(p, TMD_STUN, 3 + randint1(4), true, true);
            break;
        }

        /* Fear */
        case 3:
        {
            if (player_of_has(p, OF_PROT_FEAR))
                msg(p, "You resist the effect!");
            else
                player_inc_timed(p, TMD_AFRAID, 3 + randint1(4), true, true);
            break;
        }

        /* Paralysis */
        default:
        {
            if (player_of_has(p, OF_FREE_ACT))
                msg(p, "You resist the effect!");
            else
                player_inc_timed(p, TMD_PARALYZED, 3 + randint1(4), true, true);
            break;
        }
    }

    return 0;
}


static int project_player_handler_PSI_DRAIN(project_player_handler_context_t *context)
{
    struct player *p = player_get(0 - square(context->cave, &context->grid)->mon);
    int drain = context->dam;
    char dice[5];

    if (!drain) return 0;

    if (drain > p->chp) drain = p->chp;
    strnfmt(dice, sizeof(dice), "%d", 1 + 3 * drain / 4);
    effect_simple(EF_RESTORE_MANA, context->origin, dice, 0, 0, 0, 0, 0, NULL);

    return 0;
}


static int project_player_handler_CURSE(project_player_handler_context_t *context)
{
    struct player *p = player_get(0 - square(context->cave, &context->grid)->mon);
    int power;

    if (context->origin->monster) power = context->origin->monster->race->spell_power;
    else power = context->origin->player->lev * 2;
    if (power < 55) return 0;

    /* Cuts */
    player_inc_timed(p, TMD_CUT, damroll(power / 5 - 10, 10), true, true);

    return 0;
}


static int project_player_handler_DRAIN(project_player_handler_context_t *context) {return 0;}
static int project_player_handler_COMMAND(project_player_handler_context_t *context) {return 0;}


static int project_player_handler_TELE_TO(project_player_handler_context_t *context)
{
    struct player *p = player_get(0 - square(context->cave, &context->grid)->mon);
    struct source who_body;
    struct source *who = &who_body;

    source_player(who, get_player_index(get_connection(p->conn)), p);
    effect_simple(EF_TELEPORT_TO, who, "0", 0, 0, 0, context->origin->player->grid.y,
        context->origin->player->grid.x, NULL);

    /* Hack -- get new location */
    loc_copy(&context->grid, &p->grid);

    return 0;
}


static int project_player_handler_TELE_LEVEL(project_player_handler_context_t *context)
{
    struct player *p = player_get(0 - square(context->cave, &context->grid)->mon);

    if (player_resists(p, ELEM_NEXUS))
        msg(p, "You resist the effect!");
    else
    {
        struct source who_body;
        struct source *who = &who_body;

        source_player(who, get_player_index(get_connection(p->conn)), p);
        effect_simple(EF_TELEPORT_LEVEL, who, "0", 0, 0, 0, 0, 0, NULL);
    }

    return 0;
}


static int project_player_handler_MON_BLIND(project_player_handler_context_t *context)
{
    struct player *p = player_get(0 - square(context->cave, &context->grid)->mon);

    /* Blindness */
    if (player_of_has(p, OF_PROT_BLIND))
        msg(p, "You resist the effect!");
    else
        player_inc_timed(p, TMD_BLIND, 11 + randint1(4), true, true);

    return 0;
}


static int project_player_handler_DRAIN_MANA(project_player_handler_context_t *context)
{
    struct player *p = player_get(0 - square(context->cave, &context->grid)->mon);
    bool seen = (!p->timed[TMD_BLIND] && player_is_visible(p, context->origin->idx));

    drain_mana(p, context->origin, (randint1(context->dam) / 2) + 1, seen);

    return 0;
}


static int project_player_handler_FORGET(project_player_handler_context_t *context)
{
    struct player *p = player_get(0 - square(context->cave, &context->grid)->mon);

    /* Amnesia */
    if (!player_of_has(p, OF_PROT_AMNESIA))
        player_inc_timed(p, TMD_AMNESIA, 8, true, true);

    return 0;
}


static int project_player_handler_BLAST(project_player_handler_context_t *context)
{
    struct player *p = player_get(0 - square(context->cave, &context->grid)->mon);

    /* Confusion */
    if (player_of_has(p, OF_PROT_CONF))
        msg(p, "You resist the effect!");
    else
        player_inc_timed(p, TMD_CONFUSED, 3 + randint1(4), true, true);

    /* Amnesia */
    if (!player_of_has(p, OF_PROT_AMNESIA))
        player_inc_timed(p, TMD_AMNESIA, 4, true, true);

    return 0;
}


static int project_player_handler_SMASH(project_player_handler_context_t *context)
{
    struct player *p = player_get(0 - square(context->cave, &context->grid)->mon);

    /* Slow */
    player_inc_timed(p, TMD_SLOW, 3 + randint1(4), true, true);

    /* Blindness */
    if (player_of_has(p, OF_PROT_BLIND))
        msg(p, "You resist the effect!");
    else
        player_inc_timed(p, TMD_BLIND, 7 + randint1(8), true, true);

    /* Confusion */
    if (player_of_has(p, OF_PROT_CONF))
        msg(p, "You resist the effect!");
    else
        player_inc_timed(p, TMD_CONFUSED, 3 + randint1(4), true, true);

    /* Paralysis */
    if (player_of_has(p, OF_FREE_ACT))
        msg(p, "You resist the effect!");
    else
        player_inc_timed(p, TMD_PARALYZED, 3 + randint1(4), true, true);

    /* Amnesia */
    if (!player_of_has(p, OF_PROT_AMNESIA))
        player_inc_timed(p, TMD_AMNESIA, 4, true, true);

    return 0;
}


static int project_player_handler_CONTROL(project_player_handler_context_t *context) {return 0;}


static int project_player_handler_PROJECT(project_player_handler_context_t *context)
{
    struct player *p = player_get(0 - square(context->cave, &context->grid)->mon);

    int cidx = context->origin->player->clazz->cidx;

    if (context->origin->player->ghost && !player_can_undead(context->origin->player))
        cidx = lookup_player_class("Ghost")->cidx;

    /* "dam" is used as spell index */
    cast_spell_proj(p, cidx, context->dam, false);

    return 0;
}


static int project_player_handler_TREES(project_player_handler_context_t *context) {return 0;}


static int project_player_handler_AWAY_ANIMAL(project_player_handler_context_t *context)
{
    struct player *p = player_get(0 - square(context->cave, &context->grid)->mon);

    /* Only players polymorphed into an animal form */
    if (p->poly_race && race_is_animal(p->poly_race))
    {
        char dice[5];
        struct source who_body;
        struct source *who = &who_body;

        strnfmt(dice, sizeof(dice), "%d", context->dam);
        source_player(who, get_player_index(get_connection(p->conn)), p);
        effect_simple(EF_TELEPORT, who, dice, 0, 0, 0, 0, 0, NULL);

        /* Hack -- get new location */
        loc_copy(&context->grid, &p->grid);
    }
    else
        msg(p, "You resist the effect!");

    return 0;
}


static const project_player_handler_f player_handlers[] =
{
    #define ELEM(a, b, c, d) project_player_handler_##a,
    #include "../common/list-elements.h"
    #undef ELEM
    #define PROJ(a) project_player_handler_##a,
    #include "../common/list-projections.h"
    #undef PROJ
    NULL
};


static bool project_m_is_threat(int type)
{
    bool threat = false;

    /* Is this type of attack a threat? */
    switch (type)
    {
        case PROJ_MON_SPEED: break;
        default: threat = true; break;
    }

    return threat;
}


static bool project_p_is_threat(int type)
{
    bool threat = false;

    /* Is this type of attack a threat? */
    switch (type)
    {
        case PROJ_MON_SPEED:
        case PROJ_AWAY_ALL:
        case PROJ_PROJECT:
        case PROJ_MON_HEAL:
        case PROJ_TELE_TO:
        case PROJ_TELE_LEVEL: break;
        default: threat = true; break;
    }

    return threat;
}


/*
 * Called from project() to affect the player
 *
 * Called for projections with the PROJECT_PLAY flag set, which includes
 * bolt, beam, ball and breath effects.
 *
 * origin is the origin of the effect
 * r is the distance from the centre of the effect
 * c is the current cave
 * (y, x) the coordinates of the grid being handled
 * dam is the "damage" from the effect at distance r from the centre
 * typ is the projection (PROJ_) type
 * what is the message to other players if the target dies
 * did_hit is true if a player was hit
 * was_obvious is true if the effects were obvious
 *
 * If "r" is non-zero, then the blast was centered elsewhere; the damage
 * is reduced in project() before being passed in here. This can happen if a
 * monster breathes at the player and hits a wall instead.
 *
 * We assume the player is aware of some effect, and always return "true".
 */
void project_p(struct source *origin, int r, struct chunk *c, struct loc *grid, int dam, int typ,
    int power, const char *what, bool *did_hit, bool *was_obvious, struct loc *newgrid)
{
    bool blind, seen;
    bool obvious = true;
    bool dead = false;

    /* Monster name (for attacks) */
    char m_name[NORMAL_WID];

    /* Monster or trap name (for damage) */
    char killer[NORMAL_WID];

    project_player_handler_f player_handler;
    project_player_handler_context_t context;

    /* Projected spell */
    int index = dam;

    struct player *p = player_get(0 - square(c, grid)->mon);

    /* The effects were positive, do not disturb */
    bool positive_effect = false;

    *did_hit = false;
    *was_obvious = false;
    loc_copy(newgrid, grid);

    /* Decoy has been hit */
    if (square_isdecoyed(c, grid) && dam) square_destroy_decoy(p, c, grid);

    /* No player here */
    if (!p) return;

    /* Never affect projector (except when trying to polymorph or heal self) */
    if ((origin->player == p) && (typ != PROJ_MON_POLY) && (typ != PROJ_MON_HEAL)) return;

    /* Obtain player info */
    blind = (p->timed[TMD_BLIND] || p->timed[TMD_BLIND_REAL]) ? true : false;
    seen = !blind;

    my_strcpy(killer, "something strange", sizeof(killer));

    /* Hack -- polymorph or heal self */
    if ((origin->player == p) && ((typ == PROJ_MON_POLY) || (typ == PROJ_MON_HEAL))) {}

    /* Hit by a trap */
    else if (origin->trap)
    {
        /* Get the trap name */
        strnfmt(killer, sizeof(killer), "%s%s",
            (is_a_vowel(origin->trap->kind->desc[0])? "an ": "a "), origin->trap->kind->desc);
    }

    /* Hit by a trap (from a chest) */
    else if (origin->chest_trap)
    {
        /* Get the trap name */
        strnfmt(killer, sizeof(killer), "%s", origin->chest_trap->msg_death);
    }

    /* The caster is a monster */
    else if (origin->monster)
    {
        /* Check it is visible */
        if (!monster_is_visible(p, origin->idx)) seen = false;

        /* Get the monster name */
        monster_desc(p, m_name, sizeof(m_name), origin->monster, MDESC_CAPITAL);

        /* Get the monster's real name */
        monster_desc(p, killer, sizeof(killer), origin->monster, MDESC_DIED_FROM);

        /* Check hostility for threatening spells */
        if (project_m_is_threat(typ))
        {
            if (!pvm_check(p, origin->monster)) return;

            /* Monster sees what is going on */
            update_smart_learn(origin->monster, p, 0, 0, typ);
        }
        else
            dam = 0;
    }

    /* The caster is a player */
    else if (origin->player)
    {
        /* Check it is visible */
        if (!player_is_visible(p, origin->idx)) seen = false;

        /* Get the player name */
        player_desc(p, m_name, sizeof(m_name), origin->player, true);

        /* Get the player's real name */
        my_strcpy(killer, origin->player->name, sizeof(killer));

        /* Check hostility for threatening spells */
        if (project_p_is_threat(typ))
        {
            struct source target_body;
            struct source *target = &target_body;
            int mode;

            source_player(target, 0, p);
            mode = (target_equals(origin->player, target)? PVP_DIRECT: PVP_INDIRECT);

            if (!pvp_check(origin->player, p, mode, true, square(c, grid)->feat))
                return;
        }
        else
            positive_effect = true;
    }

    /* Let player know what is going on */
    if (!seen && projections[typ].blind_desc) msg(p, "You %s!", projections[typ].blind_desc);

    /* Hack -- polymorph self */
    if ((origin->player == p) && (typ == PROJ_MON_POLY))
    {
        /* XXX */
        dam = adjust_dam(p, PROJ_MON_POLY, dam, RANDOMISE, 0);
    }

    /* Hit by a trap */
    else if (origin->trap)
    {
        /* Adjust damage for resistance, immunity or vulnerability, and apply it */
        dam = adjust_dam(p, typ, dam, RANDOMISE, 0);
        if (dam)
        {
            char df[160];
            int reduced;

            /*
             * Account for the player's damage reduction. That does not
             * affect the side effects (i.e. player_handler), so leave
             * context.dam unmodified.
             */
            reduced = player_apply_damage_reduction(p, dam, false, killer);
            if (reduced && OPT(p, show_damage))
                msg(p, "You take $r%d^r damage.", reduced);

            trap_msg_death(p, origin->trap, df, sizeof(df));
            dead = take_hit(p, reduced, killer, df);
        }
    }

    /* The caster is a monster */
    else if (origin->monster)
    {
        /* Adjust damage for resistance, immunity or vulnerability, and apply it */
        dam = adjust_dam(p, typ, dam, RANDOMISE, 0);
        if (dam)
        {
            char df[160];
            int reduced;

            /*
             * Account for the player's damage reduction. That does not
             * affect the side effects (i.e. player_handler), so leave
             * context.dam unmodified.
             */
            reduced = player_apply_damage_reduction(p, dam, true, killer);
            if (reduced && OPT(p, show_damage))
                msg(p, "You take $r%d^r damage.", reduced);

            strnfmt(df, sizeof(df), "was %s by %s", what, killer);
            dead = take_hit(p, reduced, killer, df);
        }
    }

    /* The caster is a player */
    else if (origin->player)
    {
        /* Try a saving throw if available */
        if ((projections[typ].flags & ATT_SAVE) && magik(p->state.skills[SKILL_SAVE]))
        {
            msg(p, "You resist the effects!");

            /* Hack */
            dead = true;
        }
        else
        {
            bool non_physical = ((projections[typ].flags & ATT_NON_PHYS)? true: false);

            /* Adjust damage for resistance, immunity or vulnerability, and apply it */
            dam = adjust_dam(p, typ, dam, RANDOMISE, 0);
            if (dam && (projections[typ].flags & ATT_DAMAGE))
            {
                char df[160];
                int reduced;

                /*
                 * Account for the player's damage reduction. That does not
                 * affect the side effects (i.e. player_handler), so leave
                 * context.dam unmodified.
                 */
                reduced = player_apply_damage_reduction(p, dam, non_physical, killer);
                if (reduced && OPT(p, show_damage))
                    msg(p, "You take $r%d^r damage.", reduced);

                strnfmt(df, sizeof(df), "was %s by %s", what, killer);
                dead = take_hit(p, reduced, killer, df);

                /* Give a message */
                if (reduced && !dead) player_pain(origin->player, p, reduced);
            }

            /* Projected spell */
            if (projections[typ].flags & ATT_RAW) dam = index;
        }
    }

    context.origin = origin;
    context.r = r;
    context.cave = c;
    loc_copy(&context.grid, grid);
    context.dam = dam;
    context.type = typ;
    context.power = power;
    context.obvious = obvious;

    player_handler = player_handlers[typ];

    /* Handle side effects, possibly including extra damage */
    if ((player_handler != NULL) && !dead)
    {
        int xtra = player_handler(&context);
        char df[160];

        strnfmt(df, sizeof(df), "was %s by %s", what, killer);
        xtra = player_apply_damage_reduction(p, xtra, true, killer);
		if (xtra > 0 && OPT(p, show_damage))
            msg(p, "You take an extra $r%d^r damage.", xtra);
		take_hit(p, xtra, killer, df);
    }

    obvious = context.obvious;

    /* Disturb */
    if (!positive_effect) disturb(p, 0);

    /* Track this player */
    *did_hit = true;
    loc_copy(newgrid, &context.grid);

    /* Return "Anything seen?" */
    *was_obvious = obvious;
}