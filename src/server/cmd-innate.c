/*
 * File: cmd-innate.c
 * Purpose: Innate casting
 *
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
 * Use a ghostly ability.
 */
void do_cmd_ghost(struct player *p, int ability, int dir)
{
    struct player_class *c = lookup_player_class("Ghost");
    const struct class_book *book = &c->magic.books[0];
    int spell_index;
    struct class_spell *spell;
    struct source who_body;
    struct source *who = &who_body;

    /* Check for ghost-ness */
    if (!p->ghost || player_can_undead(p)) return;

    /* Not when confused */
    if (p->timed[TMD_CONFUSED] || p->timed[TMD_CONFUSED_REAL])
    {
        msg(p, "You are too confused!");
        return;
    }

    /* Set the ability number */
    if (ability < c->magic.total_spells)
    {
        spell_index = ability;

        /* Access the ability */
        spell = &book->spells[spell_index];
    }
    else
    {
        spell_index = ability - c->magic.total_spells;

        /* Access the ability */
        spell = &book->spells[spell_index];

        /* Projected spells */
        if (!spell->sproj)
        {
            msg(p, "You cannot project that ability.");
            return;
        }
    }

    /* Check for level */
    if (spell->slevel > p->lev)
    {
        /* Message */
        msg(p, "You aren't powerful enough to use that ability.");
        return;
    }

    /* Set current spell and inscription */
    p->current_spell = spell_index;
    p->current_item = 0;

    /* Only fire in direction 5 if we have a target */
    if ((dir == DIR_TARGET) && !target_okay(p)) return;

    source_player(who, get_player_index(get_connection(p->conn)), p);

    /* Projected */
    if (ability >= c->magic.total_spells)
    {
        project_aimed(who, PROJ_PROJECT, dir, spell_index, PROJECT_STOP | PROJECT_KILL | PROJECT_PLAY,
            "killed");
    }

    /* Cast the spell */
    else
    {
        bool ident = false, used;
        struct beam_info beam;

        fill_beam_info(p, spell_index, &beam);

        if (spell->effect && spell->effect->other_msg)
            msg_print_near(p, MSG_PY_SPELL, spell->effect->other_msg);
        target_fix(p);
        used = effect_do(spell->effect, who, &ident, true, dir, &beam, 0, 0, NULL);
        target_release(p);
        if (!used) return;
    }

    /* Take a turn */
    use_energy(p);

    /* Paranoia -- don't kill the DM */
    if (is_dm_p(p)) return;

    /* Too much can kill you */
    if (p->exp < spell->slevel * spell->smana)
    {
        const char *pself = player_self(p);
        char df[160];

        strnfmt(df, sizeof(df), "exhausted %s with ghostly spells", pself);

        /* This should pierce invulnerability */
        take_hit(p, 5000, "the strain of ghostly powers", df);
    }

    /* Take some experience */
    player_exp_lose(p, spell->slevel * spell->smana, true);
}


/*
 * Cast a breath attack
 *
 * Can be "cancelled" at the "Direction?" prompt for free.
 */
void do_cmd_breath(struct player *p, int dir)
{
    bitflag mon_breath[RSF_SIZE];
    int typ;
    struct effect *effect;
    bool ident = false;
    struct source who_body;
    struct source *who = &who_body;

    /* Restrict ghosts */
    if (p->ghost && !(p->dm_flags & DM_GHOST_BODY))
    {
        msg(p, "You need a tangible body to breathe!");
        return;
    }


    // Classes 'y' 1st - so races won't block them
    if (p->poly_race && streq(p->clazz->name, "Druid"))
    {
        // bird can heal
        if (streq(p->poly_race->name, "bird-form"))
        {
            // if can spend mana - heal
            if (p->csp > 1 + p->msp / 5)
            {
                use_energy(p);
                p->csp -= 1 + p->msp / 5;
                hp_player_safe(p, 1 + (p->lev));
            }
            else
                msgt(p, MSG_SPELL_FAIL, "You need more mana to heal!");

            return;
        }
        // boar - can charge (teleport) to closest monster; up to 3 distance
        else if (streq(p->poly_race->name, "boar-form"))
        {
            // dice for distance
            char dice_string[1];
            // convert int to char with '0' ..
            // it will work as our distance is only one digit..
            // but to be able to convert 2+ digits - use snprintf() (see above, beholder)
            dice_string[0] = (p->lev / 30 + 3) + '0';

            if (p->csp > 2 + p->lev / 3)
            {
                use_energy(p);
                p->csp -= 2 + p->lev / 3;

                effect = mem_zalloc(sizeof(struct effect));
                effect->index = EF_TELEPORT_TO;
                // init dice
                effect->dice = dice_new();
                // fill dice struct
                dice_parse_string(effect->dice, dice_string);

                source_player(who, get_player_index(get_connection(p->conn)), p);
                effect_do(effect, who, &ident, false, dir, NULL, 0, 0, NULL);

                free_effect(effect);
            }
            else
                msgt(p, MSG_SPELL_FAIL, "You need more mana to charge!");

            return;
        }
        // wolf - can summon wolf by loud howl
        else if (streq(p->poly_race->name, "wolf-form"))
        {
            struct chunk *c = chunk_get(&p->wpos);
            
            // check cooldown (only for WOLF in case of classes abilities!)
            if (p->y_cooldown) {
                msg(p, "%u turns till power recharges.", p->y_cooldown);
                sound(p, MSG_SPELL_FAIL);
                return;
            }

            // cost full mana
            if (p->csp == p->msp)
            {
                use_energy(p);
                p->csp = 0;

                // howl
                source_player(who, get_player_index(get_connection(p->conn)), p);
                effect_simple(EF_WAKE, who, 0, 0, 0, 0, 0, 0, NULL);
                // summon
                summon_specific_race_aux(p, c, &p->grid, get_race("tamed wolf"), 1 + p->lev / 30, true);
                msgt(p, MSG_HOWL, "You howl to summon your wolf-friends!");
                
                p->y_cooldown = 255; // cooldown
            }
            else
                msgt(p, MSG_SPELL_FAIL, "You need full mana to summon wolves!");
            return;
        }
    }


    // check cooldown
    if (p->y_cooldown) {
        msg(p, "%u turns till power recharges.", p->y_cooldown);
        sound(p, MSG_SPELL_FAIL);
        return;
    }


    // Now special races' effects
    
    // Spider weaves 8 webs around it
    if (streq(p->race->name, "Spider"))
    {
        /* Take a turn */
        use_energy(p);

        /* Make the breath attack an effect */
        effect = mem_zalloc(sizeof(struct effect));
        effect->index = EF_WEB; // satiation spending included
        
        if (p->msp)
            p->y_cooldown = 120; // magic users less CD
        else
            p->y_cooldown = 240; // fighters CD

        /* Cast the breath attack */
        source_player(who, get_player_index(get_connection(p->conn)), p);
        effect_do(effect, who, &ident, false, dir, NULL, 0, 0, NULL);

        free_effect(effect);
        
        return;        
    }
    else if (streq(p->race->name, "Ooze") && p->lev > 5)
    {
        // can be used only on full HPs
        if (p->chp == p->mhp)
        {
            struct chunk *c = chunk_get(&p->wpos);
            use_energy(p);

            // (summoning more than 2 glitch - one stuck in place. so we summon max 2)
            summon_specific_race_aux(p, c, &p->grid, get_race("oozling"), 1 + p->lev / 30, true);

            player_inc_timed(p, TMD_OCCUPIED, 1 + randint1(4), true, false);
            
            if (p->mhp < 20)
                p->chp -= 5;
            else // take a hit (ok, only on full HP)
                p->chp -= p->mhp / 3;

            if (p->msp)
                p->y_cooldown = 30; // magic users less CD
            else
                p->y_cooldown = 60; // fighters CD

            // Redraw the player's health and map
            p->upkeep->redraw |= (PR_HP | PR_MAP);
        }
        else
            msgt(p, MSG_SPELL_FAIL, "You should have full health to multiply.");

        return;
        
    }
    else if (streq(p->race->name, "Beholder"))
    {
        // can be used only on full HPs
        if (p->chp == p->mhp)
        {
            // element for ray
            int b_elem = randint1(p->lev / 10);
            // Dmg dice.. We need array with eg 5 elements because we will parse the number (eg 50)
            // to a separate array elements (string). So eg int 50 will become {'5','0'} - which means
            // array with two numbers (eg dice_string[2]).. We take [5] just in case for bigger numbers.
            // (also no need '\0' there in the end, it seems)
            char dice_string[5];
            // dice int number which we will convert to string later
            int dice_calc = p->lev;

            use_energy(p);

            effect = mem_zalloc(sizeof(struct effect));
            effect->index = EF_SHORT_BEAM;
            effect->radius = 3 + p->lev / 25; // up to 5

            // magic users got boni dmg
            if (p->msp)
            {
                dice_calc *= 2;
                effect->radius += p->lev / 10;
                p->y_cooldown = 30; // cooldown
            }
            else
                p->y_cooldown = 60; // cooldown fighters
            
            // init dice
            effect->dice = dice_new();
            // convert int to char
            snprintf(dice_string, sizeof(dice_string), "%d", dice_calc);
            // fill dice struct with dmg
            dice_parse_string(effect->dice, dice_string);

            // element
            switch (b_elem)
            {
                case 1: effect->subtype = PROJ_TIME; break;
                case 2: effect->subtype = PROJ_NEXUS; break;
                case 3: effect->subtype = PROJ_DISEN; break;
                case 4: effect->subtype = PROJ_DARK; break;
                case 5: effect->subtype = PROJ_FORCE; break;
                default: effect->subtype = PROJ_TIME; break; // paranoia
            }

            source_player(who, get_player_index(get_connection(p->conn)), p);
            effect_do(effect, who, &ident, false, dir, NULL, 0, 0, NULL);

            free_effect(effect);

            player_inc_timed(p, TMD_IMAGE, 1 + randint0(2), false, false);
            
            if (p->mhp < 20)
                p->chp -= 1;
            else // take a slight hit (ok, only on full HP)
                p->chp -= (p->mhp / 20);

            p->upkeep->redraw |= (PR_HP);
            p->upkeep->redraw |= (PR_MAP);
        }
        else
            msgt(p, MSG_SPELL_FAIL, "You should have full health to fire a ray.");

        return;
    }
    else if (streq(p->race->name, "Demonic") && p->lev > 5)
    {
        struct chunk *c = chunk_get(&p->wpos); // for summon

        // can be used only on full HPs
        if (p->chp == p->mhp)
        {
            int d_summ = randint0(1 + p->lev / 5); // demon type

            use_energy(p);

            switch (d_summ)
            {
                case 0: summon_specific_race_aux(p, c, &p->grid, get_race("manes"), 1, true); break; //
                case 1: summon_specific_race_aux(p, c, &p->grid, get_race("lemure"), 1, true); break; // 
                case 2: summon_specific_race_aux(p, c, &p->grid, get_race("tengu"), 1, true); break; // 
                case 3: summon_specific_race_aux(p, c, &p->grid, get_race("darkling"), 1, true); break; // 
                case 4: summon_specific_race_aux(p, c, &p->grid, get_race("gremlin"), 1, true); break; // 
                case 5: summon_specific_race_aux(p, c, &p->grid, get_race("quasit"), 1, true); break; // 
                case 6: summon_specific_race_aux(p, c, &p->grid, get_race("imp"), 1, true); break; // 
                case 7: summon_specific_race_aux(p, c, &p->grid, get_race("flamme"), 1, true); break; // 
                case 8: summon_specific_race_aux(p, c, &p->grid, get_race("gargoyle"), 1, true); break; // 
                case 9: summon_specific_race_aux(p, c, &p->grid, get_race("bodak"), 1, true); break; // 
                case 10: summon_specific_race_aux(p, c, &p->grid, get_race("mezzodaemon"), 1, true); break; // 
                case 11: summon_specific_race_aux(p, c, &p->grid, get_race("death quasit"), 1, true); break; // 
            }

            player_inc_timed(p, TMD_OCCUPIED, 3 + randint0(2), true, false);

            p->chp -= p->mhp / 3; // take a hit (ok, only on full HP)
            
            if (p->msp)
                p->y_cooldown = 60; // magic users less CD
            else
                p->y_cooldown = 120; // fighters CD

            p->upkeep->redraw |= (PR_HP);
            p->upkeep->redraw |= (PR_MAP);
        }
        else
            msgt(p, MSG_SPELL_FAIL, "You need to have full health to use this ability!");

        return;
    }
    else if (streq(p->race->name, "Djinn"))
    {
        char dice_string[5];
        // dice.. see 'beholder' for explanation
        int dice_calc = p->lev / 5 + randint1(100);

        // baaad number (have chance to clone mob at least sometimes)
        if (p->lev >= 35 && dice_calc == 13) {
            dice_calc = 7;
        }

        // convert int to char
        snprintf(dice_string, sizeof(dice_string), "%d", dice_calc);

        use_energy(p);

        source_player(who, get_player_index(get_connection(p->conn)), p);
        effect_simple(EF_WONDER, who, dice_string, 0, 0, 0, 0, 0, NULL);

        player_inc_timed(p, TMD_OCCUPIED, 1 + randint0(2), true, false);

        if (p->msp)
            p->y_cooldown = 10; // magic users less CD
        else
            p->y_cooldown = 20; // fighters CD

        return;
    }
    else if (streq(p->race->name, "Pixie"))
    {
        use_energy(p);

        player_inc_timed(p, TMD_INVIS, 20 + p->lev, true, false);
        // msg in violet color
        msgt(p, MSG_GENERIC, " $v%s^v", "You've become as intangible as a wisp of cloud in the sky.");

        p->y_cooldown = 255;

        // update visual after invis
        p->upkeep->update |= PU_MONSTERS;
        p->upkeep->redraw |= PR_STATUS;
        handle_stuff(p);

        return;
    }
    else if (streq(p->race->name, "Draconian"))
    {
        // can be used only on full HPs
        if (p->chp == p->mhp)
        {
            char dice_string[5];
            int dice_calc = randint0(2) + p->lev * (1 + p->lev / 25);

            use_energy(p);

            effect = mem_zalloc(sizeof(struct effect));

            if (one_in_(2))
                effect->index = EF_SHORT_BEAM;
            else
                effect->index = EF_BOLT_RADIUS;
            effect->radius = 1 + p->lev / 5 + randint0(2);

            // init dice
            effect->dice = dice_new();
            // convert int to char
            snprintf(dice_string, sizeof(dice_string), "%d", dice_calc);
            // fill dice struct with dmg
            dice_parse_string(effect->dice, dice_string);
            effect->subtype = PROJ_FIRE;

            source_player(who, get_player_index(get_connection(p->conn)), p);
            effect_do(effect, who, &ident, false, dir, NULL, 0, 0, NULL);

            free_effect(effect);

            p->chp -= p->mhp / 5; // take a hit (ok, only on full HP)

            if (p->msp)
                p->y_cooldown = 15; // magic users less CD
            else
                p->y_cooldown = 30; // fighters CD

            p->upkeep->redraw |= (PR_HP);
            p->upkeep->redraw |= (PR_MAP);
        }
        else
            msgt(p, MSG_SPELL_FAIL, "You should have full health to breath fire.");

        return;
    }
    else if (streq(p->race->name, "Undead") && p->chp - (p->mhp / 5) > 0)
    {
        use_energy(p);
        source_player(who, get_player_index(get_connection(p->conn)), p);
        
        // quite big radius. but at CD
        effect_simple(EF_DETECT_LIVING_MONSTERS, who, 0, 0, 0, 0, 10 + p->lev, 10 + p->lev, NULL);

        p->chp -= p->mhp / 5; // take a hit (ok, as we check hp above)
        
        p->y_cooldown = 255; // cooldown

        player_inc_timed(p, TMD_OCCUPIED, 1 + randint0(2), true, false);
        return;
    }
    else if (streq(p->race->name, "Imp"))
    {
        use_energy(p);
        source_player(who, get_player_index(get_connection(p->conn)), p);
        effect_simple(EF_TELEPORT, who, "13", 0, 0, 0, 0, 0, NULL);
        player_inc_timed(p, TMD_OCCUPIED, 1 + randint0(2), true, false);
        return;
    }
    else if (streq(p->race->name, "Homunculus"))
    {
        use_energy(p);
        source_player(who, get_player_index(get_connection(p->conn)), p);
        effect_simple(EF_TELEPORT, who, "10", 0, 0, 0, 0, 0, NULL);
        return;
    }
    else if (streq(p->race->name, "Wraith")) // TODO: test and rebalance
    {
        use_energy(p);
        source_player(who, get_player_index(get_connection(p->conn)), p);
        player_inc_timed(p, TMD_WRAITHFORM, 5, false, false);
        player_inc_timed(p, TMD_BLIND_REAL, 5, false, false); // no cure
        
        p->y_cooldown = 60; // cooldown
        
        return;
    }
    else if (streq(p->race->name, "Wisp"))
    {
        char dice_string[5];
        int dice_calc = randint0(p->lev);

        use_energy(p);

        effect = mem_zalloc(sizeof(struct effect));
        effect->index = EF_BLAST;
        effect->subtype = PROJ_LIGHT_WEAK;
        effect->radius = 1 + p->lev / 7;
        // init dice
        effect->dice = dice_new();
        // convert int to char
        snprintf(dice_string, sizeof(dice_string), "%d", dice_calc);
        // fill dice struct with dmg
        dice_parse_string(effect->dice, dice_string);

        source_player(who, get_player_index(get_connection(p->conn)), p);
        effect_do(effect, who, &ident, false, dir, NULL, 0, 0, NULL);

        free_effect(effect);

        effect_simple(EF_LIGHT_AREA, who, 0, 0, 0, 0, 0, 0, NULL);
        player_inc_timed(p, TMD_OCCUPIED, 1 + randint0(2), true, false);
        return;
    }
    else if (streq(p->race->name, "Lizardmen") && p->chp < p->mhp)
    {
        use_energy(p);
        player_inc_timed(p, TMD_OCCUPIED, 2, false, false);

        // restore 3% HP or if almost full - restore all
        if (p->mhp < 30) // too small HP to be able to calculate 3%
            hp_player(p, 1);
        else if (p->chp + p->mhp / 30 < p->mhp)
            hp_player(p, p->mhp / 30); // restore 3%
        else
            hp_player(p, p->mhp - p->chp); // restore to full HP

        return;
    }
    else if (streq(p->race->name, "Forest-Goblin"))
    {
        use_energy(p);
        player_inc_timed(p, TMD_COVERTRACKS, 20 + p->lev, false, false);
        
        p->y_cooldown = 255; // cooldown

        player_inc_timed(p, TMD_OCCUPIED, 2, false, false);
        return;
    }
    else if (streq(p->race->name, "Gnoll"))
    {
        use_energy(p);
        source_player(who, get_player_index(get_connection(p->conn)), p);
        // big radius, but CD
        effect_simple(EF_DETECT_FEARFUL_MONSTERS, who, 0, 0, 0, 0, 10 + p->lev, 10 + p->lev, NULL);
        
        p->y_cooldown = 255; // cooldown
        
        player_inc_timed(p, TMD_OCCUPIED, 2, true, false);
        return;
    }
    else if (streq(p->race->name, "Naga") && p->chp - (p->mhp / 10) > 0)
    {
        use_energy(p);
        player_inc_timed(p, TMD_OFFENSIVE_STANCE, 5 + p->lev / 2, false, false);

        p->y_cooldown = 255; // cooldown

        p->chp -= p->mhp / 10; // take a hit (ok, checked above)

        p->upkeep->redraw |= (PR_HP);
        p->upkeep->redraw |= (PR_MAP);
        return;
    }
    else if (streq(p->race->name, "Titan"))
    {
            char dice_string[5];
            int dice_calc = randint0(100) + p->lev * 3;
            use_energy(p);
            effect = mem_zalloc(sizeof(struct effect));
            effect->index = EF_SHORT_BEAM;
            effect->radius = 1;
            // init dice
            effect->dice = dice_new();
            // convert int to char
            snprintf(dice_string, sizeof(dice_string), "%d", dice_calc);
            // fill dice struct with dmg
            dice_parse_string(effect->dice, dice_string);
            effect->subtype = PROJ_KILL_WALL;
            source_player(who, get_player_index(get_connection(p->conn)), p);
            effect_do(effect, who, &ident, false, dir, NULL, 0, 0, NULL);
            free_effect(effect);
            p->y_cooldown = 2;
            return;
    }
    else if (streq(p->race->name, "Troglodyte"))
    {
            char dice_string[5];
            // if trogly current food lvl < this amount, it increases to (amount + 1)
            int dice_calc = 14; // 14%
            use_energy(p);
            effect = mem_zalloc(sizeof(struct effect));
            effect->index = EF_NOURISH;
            effect->subtype = 3; // means INC_TO case in effect_handler_NOURISH() (not %!)
            // init dice
            effect->dice = dice_new();
            // convert int to char
            snprintf(dice_string, sizeof(dice_string), "%d", dice_calc);
            // fill dice struct with dmg
            dice_parse_string(effect->dice, dice_string);
            source_player(who, get_player_index(get_connection(p->conn)), p);
            effect_do(effect, who, &ident, false, dir, NULL, 0, 0, NULL);
            free_effect(effect);
            
            p->y_cooldown = 1000; // cooldown
            
            player_inc_timed(p, TMD_OCCUPIED, 2, false, false);

            return;
    }
    else if (streq(p->race->name, "Minotaur"))
    {
        // can be used only on full HPs
        if (p->chp == p->mhp)
        {
            use_energy(p);
            player_inc_timed(p, TMD_SPEEDY, 5 + p->lev / 3, false, false);

            p->chp -= p->mhp / 2; // take a hit (ok, only on full hp)

            p->y_cooldown = 255; // cooldown

            p->upkeep->redraw |= (PR_HP);
            p->upkeep->redraw |= (PR_MAP);
        }
        else
            msgt(p, MSG_SPELL_FAIL, "You should have full health to speed up.");

        return;
    }
    else if (streq(p->race->name, "Harpy"))
    {
        use_energy(p);
        player_clear_timed(p, TMD_SLOW, false);
        p->upkeep->redraw |= (PR_STATE);
        return;
    }
    else if (streq(p->race->name, "Centaur"))
    {
        char dice_string[1];
        dice_string[0] = (p->lev / 25 + 3) + '0';

        if (p->chp == p->mhp)
        {
            use_energy(p);
            effect = mem_zalloc(sizeof(struct effect));
            effect->index = EF_TELEPORT_TO;
            // init dice
            effect->dice = dice_new();
            // fill dice struct
            dice_parse_string(effect->dice, dice_string);
            source_player(who, get_player_index(get_connection(p->conn)), p);
            effect_do(effect, who, &ident, false, dir, NULL, 0, 0, NULL);
            free_effect(effect);

            p->chp -= p->mhp / 4; // take a hit (ok, only of full hp)

            p->y_cooldown = 30;

            p->upkeep->redraw |= (PR_HP);
            p->upkeep->redraw |= (PR_MAP);
        }
        else
            msgt(p, MSG_SPELL_FAIL, "You should have full health to charge.");

        return;
    }
    else if (streq(p->race->name, "Frostmen"))
    {
        if (p->chp == p->mhp)
        {
            char dice_string[5];
            int dice_calc = 1;
            use_energy(p);
            effect = mem_zalloc(sizeof(struct effect));
            effect->index = EF_SHORT_BEAM;
            effect->subtype = PROJ_RAISE; // you know nothing, Jon Snow
            effect->radius = 1;
            // init dice
            effect->dice = dice_new();
            // convert int to char
            snprintf(dice_string, sizeof(dice_string), "%d", dice_calc);
            // fill dice struct with dmg
            dice_parse_string(effect->dice, dice_string);
            source_player(who, get_player_index(get_connection(p->conn)), p);
            effect_do(effect, who, &ident, false, dir, NULL, 0, 0, NULL);
            free_effect(effect);

            p->chp -= p->mhp / 3; // take a hit (ok, only on full hp)
            
            if (p->msp)
                p->y_cooldown = 255; // magic users less CD
            else
                p->y_cooldown = 180; // fighters CD

            player_inc_timed(p, TMD_OCCUPIED, 2, false, false);
            p->upkeep->redraw |= (PR_HP);
            p->upkeep->redraw |= (PR_MAP);
        }
        else
            msgt(p, MSG_SPELL_FAIL, "You should have full health for the ritual.");

        return;
    }
    else if (streq(p->race->name, "Elemental"))
    {
        // can be used only on full HPs
        if (p->chp == p->mhp)
        {
            // element for ray
            int b_elem;
            char dice_string[5];
            int dice_calc = p->lev;

            // magic users got boni dmg.. mature elems too
            if (p->lev >= 30 || p->msp > 0)
                dice_calc *= 2;
            // elementals become powerful fellas.. 200dmg
            if (p->lev == 50)
                dice_calc *= 2;

            if (p->lev < 30) b_elem = 0; // elec
            else if (p->lev < 40) b_elem = 1; // cold
            else if (p->lev < 50) b_elem = 2; // fire
            else b_elem = 3; // acid

            use_energy(p);

            effect = mem_zalloc(sizeof(struct effect));
            effect->index = EF_BOLT_RADIUS;
            effect->radius = 3 + p->lev / 15; // up to 6

            // magic users more distance
            if (p->msp > 0)
                effect->radius += p->lev / 10;

            // init dice
            effect->dice = dice_new();
            // convert int to char
            snprintf(dice_string, sizeof(dice_string), "%d", dice_calc);
            // fill dice struct with dmg
            dice_parse_string(effect->dice, dice_string);

            // element
            switch (b_elem)
            {
                case 0: effect->subtype = PROJ_ELEC; break;
                case 1: effect->subtype = PROJ_COLD; break;
                case 2: effect->subtype = PROJ_FIRE; break;
                case 3: effect->subtype = PROJ_ACID; break;
            }

            source_player(who, get_player_index(get_connection(p->conn)), p);
            effect_do(effect, who, &ident, false, dir, NULL, 0, 0, NULL);

            free_effect(effect);

            p->chp -= p->mhp / 10; // take a slight hit (ok, only on full hp)

            if (p->msp)
                p->y_cooldown = 15; // magic users less CD
            else
                p->y_cooldown = 30; // fighters CD

            player_inc_timed(p, TMD_OCCUPIED, 2, false, false);
            p->upkeep->redraw |= (PR_HP);
            p->upkeep->redraw |= (PR_MAP);
        }
        else
            msgt(p, MSG_SPELL_FAIL, "You should have full health to attack with elements.");

        return;
    }
    else if (streq(p->race->name, "Wood-Elf"))
    {
        use_energy(p);
        player_inc_timed(p, TMD_SINVIS, 2 + p->lev, false, false);
        player_inc_timed(p, TMD_OCCUPIED, 3, false, false);
        return;
    }
    else if (streq(p->race->name, "High-Elf"))
    {
        use_energy(p);
        player_inc_timed(p, TMD_SINFRA, 5 + p->lev, false, false);
        player_inc_timed(p, TMD_OCCUPIED, 3, false, false);
        return;
    }
    else if (streq(p->race->name, "Golem"))
    {
        use_energy(p);
        
        effect = mem_zalloc(sizeof(struct effect));
        effect->index = EF_SAND_WALL;

        source_player(who, get_player_index(get_connection(p->conn)), p);
        effect_do(effect, who, &ident, false, dir, NULL, 0, 0, NULL);

        p->y_cooldown = 10 - (p->lev / 5); // cooldown: 10 turns on 1 lvl, 0 turns on 50 lvl

        free_effect(effect);

        if (p->lev < 10) {
            player_dec_timed(p, TMD_FOOD, 100, false);
            player_inc_timed(p, TMD_OCCUPIED, 6, false, false);
        } else if (p->lev < 20) {
            player_dec_timed(p, TMD_FOOD, 80, false);
            player_inc_timed(p, TMD_OCCUPIED, 5, false, false);
        } else if (p->lev < 30) {
            player_dec_timed(p, TMD_FOOD, 60, false);
            player_inc_timed(p, TMD_OCCUPIED, 4, false, false);
        } else if (p->lev < 40) {
            player_dec_timed(p, TMD_FOOD, 40, false);
            player_inc_timed(p, TMD_OCCUPIED, 3, false, false);
        } else if (p->lev < 50) {
            player_dec_timed(p, TMD_FOOD, 30, false);
            player_inc_timed(p, TMD_OCCUPIED, 2, false, false);
        } else {
            player_dec_timed(p, TMD_FOOD, 20, false);
            player_inc_timed(p, TMD_OCCUPIED,  1 + randint0(2), false, false);
        }

        return;
    }
    else if (streq(p->race->name, "Gargoyle"))
    {
        use_energy(p);
        player_inc_timed(p, TMD_ANCHOR, 5, false, false);    
        return;
    }
    else if (streq(p->race->name, "Nephalem"))
    {
        if (p->chp == p->mhp)
        {
            use_energy(p);
            effect = mem_zalloc(sizeof(struct effect));
            effect->index = EF_GLYPH;
            effect->subtype = GLYPH_WARDING;
            effect->dice = dice_new();
            source_player(who, get_player_index(get_connection(p->conn)), p);
            effect_do(effect, who, &ident, false, dir, NULL, 0, 0, NULL);
            free_effect(effect);

            // sacrifice (leaves 1% hp)
            p->chp = p->mhp / 100;
            // low hp heroes: ensure at least 1 HP remains
            if (p->chp < 1)
                p->chp = 1;

            p->y_cooldown = 500; // cooldown

            player_inc_timed(p, TMD_OCCUPIED, 3, false, false);
            p->upkeep->redraw |= (PR_HP);
            p->upkeep->redraw |= (PR_MAP);
        }
        else
            msgt(p, MSG_SPELL_FAIL, "You should have full health for the ritual.");

        return;
    }
    else if (streq(p->race->name, "Celestial"))
    {
        // can be used only on full HPs
        if (p->chp == p->mhp)
        {
            // element for ray
            char dice_string[5];
            int dice_calc = p->lev;

            // magic users got boni dmg..
            if (p->lev >= 30 || p->msp > 0)
                dice_calc *= 2;

            use_energy(p);

            effect = mem_zalloc(sizeof(struct effect));
            effect->index = EF_BLAST;
            effect->subtype = PROJ_HOLY_ORB;
            effect->radius = 1 + (p->lev / 30); ////////// <<<< !

            // init dice
            effect->dice = dice_new();
            // convert int to char
            snprintf(dice_string, sizeof(dice_string), "%d", dice_calc);
            // fill dice struct with dmg
            dice_parse_string(effect->dice, dice_string);

            source_player(who, get_player_index(get_connection(p->conn)), p);
            effect_do(effect, who, &ident, false, dir, NULL, 0, 0, NULL);

            free_effect(effect);

            p->chp -= p->mhp / 10; // take a slight hit (ok, only on full hp)

            if (p->msp)
                p->y_cooldown = 15; // magic users less CD
            else
                p->y_cooldown = 30; // fighters CD

            p->upkeep->redraw |= (PR_HP);
            p->upkeep->redraw |= (PR_MAP);
        }
        else
            msgt(p, MSG_SPELL_FAIL, "You should have full health to inflict holy touch.");

        return;
    }
    else if (streq(p->race->name, "Balrog"))
    {
        char dice_string[5];
        int dice_calc = p->lev;

        // magic users got boni dmg.. especially mature
        if (p->lev >= 30 || p->msp > 0)
            dice_calc *= 2;
        if (p->lev == 50)
            dice_calc *= 2;

        use_energy(p);

        effect = mem_zalloc(sizeof(struct effect));
        if (one_in_(5 - p->lev / 15))
            effect->index = EF_SHORT_BEAM;
        else
            effect->index = EF_BOLT_RADIUS;
        effect->radius = 5 + p->lev / 15;
        effect->subtype = PROJ_FIRE;

        // magic users more distance
        if (p->msp > 0)
            effect->radius += p->lev / 25;

        // init dice
        effect->dice = dice_new();
        // convert int to char
        snprintf(dice_string, sizeof(dice_string), "%d", dice_calc);
        // fill dice struct with dmg
        dice_parse_string(effect->dice, dice_string);

        source_player(who, get_player_index(get_connection(p->conn)), p);
        effect_do(effect, who, &ident, false, dir, NULL, 0, 0, NULL);

        free_effect(effect);

        p->y_cooldown = 15;

        return;
    }
    else if (streq(p->race->name, "Maiar") && p->chp - (p->mhp / 15) > 0)
    {
        char dice_string[5];
        int dice_calc = p->lev;

        use_energy(p);

        effect = mem_zalloc(sizeof(struct effect));
        if (one_in_(3 - p->lev / 30))
            effect->index = EF_SHORT_BEAM;
        else
            effect->index = EF_BOLT_RADIUS;
        effect->subtype = PROJ_ELEC;
        effect->radius = 3 + p->lev / 10;

        // magic users got boni dmg.. especially mature
        if (p->lev >= 30 || p->msp > 0)
            dice_calc *= 2;
        if (p->lev == 50)
            dice_calc *= 2;
        // magic users more distance
        if (p->msp > 0)
            effect->radius += p->lev / 25;

        // init dice
        effect->dice = dice_new();
        // convert int to char
        snprintf(dice_string, sizeof(dice_string), "%d", dice_calc);
        // fill dice struct with dmg
        dice_parse_string(effect->dice, dice_string);

        source_player(who, get_player_index(get_connection(p->conn)), p);
        effect_do(effect, who, &ident, false, dir, NULL, 0, 0, NULL);

        free_effect(effect);

        if (p->msp)
            p->y_cooldown = 10; // magic users less CD
        else
            p->y_cooldown = 20; // fighters CD

        p->chp -= p->mhp / 10; // // take a slight hit (ok, checked above)

        p->upkeep->redraw |= (PR_HP);
        p->upkeep->redraw |= (PR_MAP);
        return;
    }
    else if (streq(p->race->name, "Vampire"))
    {
        use_energy(p);
        player_inc_timed(p, TMD_FLIGHT, 5 + p->lev / 2, false, false);
        
        if (p->msp)
            p->y_cooldown = 180; // magic users less CD
        else
            p->y_cooldown = 255; // fighters CD

        return;
    }
    else if (streq(p->race->name, "Werewolf"))
    {
            use_energy(p);
            do_cmd_poly(p, NULL, false, true);
            return;
    }
    else if (streq(p->race->name, "Dark-Elf"))
    {
        if (p->chp == p->mhp)
        {
            use_energy(p);
            player_inc_timed(p, TMD_ATT_POIS, 10 + p->lev / 2, false, false);
            player_inc_timed(p, TMD_OCCUPIED, 2, false, false);

            p->chp -= p->mhp / 10; // take a slight hit (ok, only on full hp)

            p->y_cooldown = 255; // cooldown

            p->upkeep->redraw |= (PR_HP);
            p->upkeep->redraw |= (PR_MAP);
        }
        else
            msgt(p, MSG_SPELL_FAIL, "You should have full health to work with poison.");

        return;
    }
    else if (streq(p->race->name, "Orc"))
    {
        // call warg
        if (p->chp == p->mhp)
        {
            struct chunk *c = chunk_get(&p->wpos);
            use_energy(p);

            // howl
            source_player(who, get_player_index(get_connection(p->conn)), p);
            effect_simple(EF_WAKE, who, 0, 0, 0, 0, 0, 0, NULL);
            // summon
            summon_specific_race_aux(p, c, &p->grid, get_race("tamed wolf"), 1, true);
            
            p->y_cooldown = 300; // cooldown

            p->chp -= p->mhp / 3; // take a hit (ok, only on full hp)

            p->upkeep->redraw |= (PR_HP);
            p->upkeep->redraw |= (PR_MAP);
        }
        else
            msgt(p, MSG_SPELL_FAIL, "You should have full health to summon your warg.");

        return;
    }
    else if (streq(p->race->name, "Troll"))
    {
        use_energy(p);
        player_inc_timed(p, TMD_SHIELD, 10 + p->lev / 2, false, false);
        player_inc_timed(p, TMD_OCCUPIED, 2, false, false);
        
        p->y_cooldown = 300; // cooldown

        return;
    }
    else if (streq(p->race->name, "Ogre"))
    {
        use_energy(p);
        source_player(who, get_player_index(get_connection(p->conn)), p);
        if (p->msp > 0 && p->csp < p->msp)
            p->csp++; // ok
        else if (p->chp < p->mhp)
            p->chp++; // ok
        player_inc_timed(p, TMD_OCCUPIED, 2, false, false);
        return;
    }
    else if (streq(p->race->name, "Half-Giant"))
    {
        if (p->chp == p->mhp)
        {
            use_energy(p);
            
            // magic resistance
            player_inc_timed(p, TMD_SAFE, 7 + p->lev / 5, false, false);

            p->chp -= p->mhp / 4; // take a hit (ok, only on full hp)

            p->y_cooldown = 300; // cooldown

            p->upkeep->redraw |= (PR_HP);
            p->upkeep->redraw |= (PR_MAP);
        }
        else
            msgt(p, MSG_SPELL_FAIL, "You should have full health to harden your resistance.");

        return;
    }
    else if (streq(p->race->name, "Goblin"))
    {
        if (p->chp == p->mhp)
        {
            use_energy(p);
            player_inc_timed(p, TMD_BLOODLUST, 2 + p->lev / 7, true, false);

            p->chp -= p->mhp / 2; // take a hit (ok, only on full hp)
            
            p->y_cooldown = 1000; // BIG cooldown

            p->upkeep->redraw |= (PR_HP);
            p->upkeep->redraw |= (PR_MAP);
        }
        else
            msgt(p, MSG_SPELL_FAIL, "You should have full health to become bloodthirsty.");

        return;
    }
    else if (streq(p->race->name, "Black-Dwarf"))
    {
        use_energy(p);
        source_player(who, get_player_index(get_connection(p->conn)), p);
        
        // big radius, but CD
        effect_simple(EF_DETECT_DOORS, who, 0, 0, 0, 0, 10 + p->lev, 10 + p->lev, NULL);

        p->y_cooldown = 255; // cooldown        

        player_inc_timed(p, TMD_OCCUPIED, 2, true, false);
        return;
    }
    else if (streq(p->race->name, "Barbarian"))
    {
            use_energy(p);
            source_player(who, get_player_index(get_connection(p->conn)), p);
            effect_simple(EF_WAKE, who, 0, 0, 0, 0, 0, 0, NULL);
            player_inc_timed(p, TMD_HERO, 10 + p->lev / 2, false, false);
            player_inc_timed(p, TMD_TAUNT, 10 + p->lev / 2, false, false);

            p->y_cooldown = 255; // cooldown
            
            return;
    }
    else if (streq(p->race->name, "Merfolk"))
    {
        use_energy(p);
        player_clear_timed(p, TMD_POISONED, false);
        player_inc_timed(p, TMD_OCCUPIED, 2, true, false);
        return;
    }
    else if (streq(p->race->name, "Damned") && p->chp - (p->mhp / 10) > 0)
    {
        int i, count = 0;

        // Count players on the level
        for (i = 1; i <= NumPlayers; i++)
        {
            struct player *player = player_get(i);

            /* Skip this player */
            if (player == p) continue;

            /* Count */
            if (wpos_eq(&player->wpos, &p->wpos)) count++;
        }

        // if there additional players at the level
        if (count > 0)
        {
            msgt(p, MSG_SPELL_FAIL, "You can interfere summon only if you are alone on level.");
            return;
        }

        use_energy(p);
        player_inc_timed(p, TMD_ANTISUMMON, 2 + p->lev / 7, false, false);
        player_inc_timed(p, TMD_OCCUPIED, 2, false, false);

        p->y_cooldown = 500; // cooldown

        p->chp -= p->mhp / 10; // take a hit (ok, as we check hp above)

        p->upkeep->redraw |= (PR_HP);
        p->upkeep->redraw |= (PR_MAP);
        return;
    }
    else if (streq(p->race->name, "Black-Numenor"))
    {
        use_energy(p);
        source_player(who, get_player_index(get_connection(p->conn)), p);
        
        // big radius, but CD
        effect_simple(EF_DETECT_EVIL, who, 0, 0, 0, 0, 10 + p->lev, 10 + p->lev, NULL);

        p->y_cooldown = 300; // cooldown

        player_inc_timed(p, TMD_OCCUPIED, 2, false, false);
        return;
    }
    else if (streq(p->race->name, "Thunderlord"))
    {
        if (p->chp == p->mhp)
        {
            use_energy(p);
            // shouldn't remove temp res from other source, so inc
            player_inc_timed(p, TMD_OPP_ELEC, 2 + p->lev / 2, false, false);
            // shouldn't cheeze to charge up several times
            player_set_timed(p, TMD_ATT_ELEC, 2 + p->lev / 2, false);

            p->chp -= p->mhp / 4; // take a hit (ok, only on full hp)

            p->y_cooldown = 300; // cooldown

            player_inc_timed(p, TMD_OCCUPIED, 2, true, false);
        }
        else
            msgt(p, MSG_SPELL_FAIL, "You should have full health to charge lightning.");

        return;
    }
    else if (streq(p->race->name, "Yeek"))
    {
        use_energy(p);
        player_clear_timed(p, TMD_AFRAID, false);
        player_inc_timed(p, TMD_BOLD, 1 + p->lev, false, false);
        return;
    }
    else if (streq(p->race->name, "Kobold") && p->chp - (p->mhp / 5) > 0)
    {
        use_energy(p);
        player_inc_timed(p, TMD_OPP_POIS, 10 + p->lev / 2, false, false);

        p->y_cooldown = 255; // cooldown

        p->chp -= p->mhp / 5; // take a hit (ok, as we check hp above)

        return;
    }
    else if (streq(p->race->name, "Dunadan"))
    {
        use_energy(p);
        player_clear_timed(p, TMD_BLACKBREATH, false);
        if (p->chp < p->mhp)
            p->chp++; // ok
        player_inc_timed(p, TMD_OCCUPIED, 2, true, false);
        return;
    }
    else if (streq(p->race->name, "Half-Orc"))
    {
        use_energy(p);
        source_player(who, get_player_index(get_connection(p->conn)), p);
        effect_simple(EF_RESTORE_STAT, who, "0", STAT_STR, 0, 0, 0, 0, NULL);
        player_inc_timed(p, TMD_OCCUPIED, 2, true, false);
        return;
    }
    else if (streq(p->race->name, "Dwarf"))
    {
        use_energy(p);
        source_player(who, get_player_index(get_connection(p->conn)), p);
              
        effect_simple(EF_DETECT_TREASURES, who, 0, 0, 0, 0, 10 + p->lev, 10 + p->lev, NULL);
        
        p->y_cooldown = 255; // cooldown

        player_inc_timed(p, TMD_OCCUPIED, 2, true, false);
        return;
    }
    else if (streq(p->race->name, "Gnome"))
    {
        use_energy(p);
        player_clear_timed(p, TMD_IMAGE, false);
        p->upkeep->redraw |= (PR_STATE);
        return;
    }
    else if (streq(p->race->name, "Halfling"))
    {
        if (p->chp == p->mhp)
        {
            use_energy(p);
            player_inc_timed(p, TMD_TRAPSAFE, 3, false, false);
            return;
        }
        else
            msgt(p, MSG_SPELL_FAIL, "You should be in healthy state to avoid traps.");

        return;
    }
    else if (streq(p->race->name, "Elf"))
    {
        use_energy(p);
        player_inc_timed(p, TMD_BLESSED, 10 + p->lev / 2, false, false);
        
        p->y_cooldown = 255; // cooldown
        
        return;
    }
    else if (streq(p->race->name, "Half-Elf"))
    {
        use_energy(p);
        player_inc_timed(p, TMD_HERO, 10 + p->lev / 2, false, false);
        
        p->y_cooldown = 255; // cooldown
        
        return;
    }
    else if (streq(p->race->name, "Human"))
    {
        if (p->lev > 29) {
            use_energy(p);
            source_player(who, get_player_index(get_connection(p->conn)), p);
            effect_simple(EF_RESTORE_STAT, who, "0", STAT_CON, 0, 0, 0, 0, NULL);
            player_inc_timed(p, TMD_OCCUPIED, 2, true, false);

            p->y_cooldown = 20; // cooldown
            
            return;
        } else
            msg(p, "You need to reach level 30 to restore Constitution.");
    }
    else if (streq(p->race->name, "Half-Troll"))
    {
        use_energy(p);
        sound(p, MSG_NPC_BELCH);
        p->y_cooldown = randint1(30); // cooldown
        return;
    }
    // (again... why !shapechanger? don't remember. maybe just cause it imba?)
    else if (streq(p->race->name, "Ent") && !streq(p->clazz->name, "Shapechanger") &&
             p->lev > 5)
    {
        use_energy(p);
            
        effect = mem_zalloc(sizeof(struct effect));
        effect->index = EF_CREATE_TREES;

        source_player(who, get_player_index(get_connection(p->conn)), p);
        effect_do(effect, who, &ident, false, dir, NULL, 0, 0, NULL);

        free_effect(effect);

        player_inc_timed(p, TMD_OCCUPIED, 1 + randint1(4), true, false);
        take_hit(p, p->mhp / 9, "exhaustion", "exhausted from gardening");

        p->y_cooldown = 300; // cooldown

        return;
    }


    /* Handle polymorphed players */
    rsf_wipe(mon_breath);
    if (p->poly_race)
    {
        /* Hack -- require correct "breath attack" */
        rsf_copy(mon_breath, p->poly_race->spell_flags);
        set_breath(mon_breath);
    }

    /* No breath attacks */
    if (rsf_is_empty(mon_breath))
    {
        msg(p, "You are not able to breathe anything but air...");
        return;
    }

    /* Take a turn */
    use_energy(p);

    /* Apply confusion */
    player_confuse_dir(p, &dir);

    /* Get breath effect */
    typ = breath_effect(p, mon_breath);

    /* Make the breath attack an effect */
    effect = mem_zalloc(sizeof(struct effect));
    effect->index = EF_BREATH;
    effect->subtype = typ;
    effect->radius = 20;

    /* Cast the breath attack */
    source_player(who, get_player_index(get_connection(p->conn)), p);
    effect_do(effect, who, &ident, true, dir, NULL, 0, 0, NULL);

    // Breathing also consumes food: see handler_breath()

    free_effect(effect);
}


/*
 * Cast a mimic spell
 */
void do_cmd_mimic(struct player *p, int page, int spell_index, int dir)
{
    int i, j = 0, k = 0, chance;
    struct class_spell *spell;
    bool projected = false;

    /* Check the player can cast mimic spells at all */
    if (player_cannot_cast_mimic(p, true)) return;

    /* Check each spell */
    for (i = 0; i < p->clazz->magic.books[0].num_spells; i++)
    {
        int flag;

        /* Access the spell */
        spell = &p->clazz->magic.books[0].spells[i];

        /* Access the spell flag */
        flag = spell->effect->flag;

        /* Check spell availability */
        if (!(p->poly_race && rsf_has(p->poly_race->spell_flags, flag))) continue;

        /* Did we find it? */
        if (page == -1)
        {
            /* Normal spell */
            if (flag == spell_index) break;

            /* Projected spell */
            if (flag == 0 - spell_index)
            {
                if (!spell->sproj)
                {
                    msg(p, "You cannot project that spell.");
                    return;
                }
                projected = true;
                break;
            }
        }
        if (page == k)
        {
            /* Normal spell */
            if (j == spell_index) break;

            /* Projected spell */
            if (j == spell_index - p->clazz->magic.total_spells)
            {
                if (!spell->sproj)
                {
                    msg(p, "You cannot project that spell.");
                    return;
                }
                projected = true;
                break;
            }
        }

        /* Next spell */
        j++;
        if (j == MAX_SPELLS_PER_PAGE)
        {
            j = 0;
            k++;
        }
    }

    /* Paranoia */
    if (i == p->clazz->magic.books[0].num_spells) return;

    /* Check mana */
    if ((spell->smana > p->csp) && !OPT(p, risky_casting))
    {
        msg(p, "You do not have enough mana to %s this %s.", spell->realm->verb,
            spell->realm->spell_noun);
        return;
    }

    /* Antimagic field (no effect on spells that are not "magical") */
    if ((spell->smana > 0) && check_antimagic(p, chunk_get(&p->wpos), NULL))
    {
        use_energy(p);
        return;
    }

    /* Spell cost */
    p->spell_cost = spell->smana;

    /* Spell failure chance */
    chance = spell->sfail;

    /* Failed spell */
    if (magik(chance))
        msg(p, "You failed to get the spell off!");

    /* Process spell */
    else
    {
        struct source who_body;
        struct source *who = &who_body;

        /* Set current spell and inscription */
        p->current_spell = spell->sidx;
        p->current_item = 0;

        /* Only fire in direction 5 if we have a target */
        if ((dir == DIR_TARGET) && !target_okay(p)) return;

        /* Unaware players casting spells reveal themselves */
        if (p->k_idx) aware_player(p, p);

        source_player(who, get_player_index(get_connection(p->conn)), p);

        /* Projected */
        if (projected)
        {
            project_aimed(who, PROJ_PROJECT, dir, spell->sidx,
                PROJECT_STOP | PROJECT_KILL | PROJECT_PLAY, "killed");
        }

        /* Cast the spell */
        else
        {
            bool ident = false, used;

            if (spell->effect && spell->effect->other_msg)
            {
                /* Hack -- formatted message */
                switch (spell->effect->flag)
                {
                    case RSF_HEAL:
                    case RSF_TELE_TO:
                    case RSF_FORGET:
                    case RSF_S_KIN:
                    {
                        msg_format_near(p, MSG_PY_SPELL, spell->effect->other_msg,
                            player_poss(p));
                        break;
                    }
                    default:
                    {
                        msg_print_near(p, MSG_PY_SPELL, spell->effect->other_msg);
                        break;
                    }
                }
            }
            target_fix(p);
            used = effect_do(spell->effect, who, &ident, true, dir, NULL, 0, 0, NULL);
            target_release(p);
            if (!used) return;
        }
    }

    /* Take a turn */
    use_energy(p);

    /* Use some mana */
    use_mana(p);
}
