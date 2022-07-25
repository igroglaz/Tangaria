/*
 * File: cmd-innate.c
 * Purpose: Innate casting
 *
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
    if (p->timed[TMD_CONFUSED])
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

    /* Too much can kill you */
    if (p->exp < spell->slevel * spell->smana)
    {
        const char *pself = player_self(p);
        char df[160];

        strnfmt(df, sizeof(df), "exhausted %s with ghostly spells", pself);
        take_hit(p, 5000, "the strain of ghostly powers", false, df);
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
    
    // Special races' effects (Spider weaves web, Ent grow trees etc)
    if (streq(p->race->name, "Spider") && !streq(p->clazz->name, "Shapechanger"))
    {
        // one tile web
        if (streq(p->clazz->name, "Warrior") || streq(p->clazz->name, "Monk") ||
        streq(p->clazz->name, "Unbeliever"))
        {
            /* Take a turn */
            use_energy(p);

            /* Make the breath attack an effect */
            effect = mem_zalloc(sizeof(struct effect));
            effect->index = EF_WEB_SPIDER;

            /* Cast the breath attack */
            source_player(who, get_player_index(get_connection(p->conn)), p);
            effect_do(effect, who, &ident, false, dir, NULL, 0, 0, NULL);

            free_effect(effect);
            
            return;
        }
        // surrounding multiple web
        else // all classes except warr, monk, unbeliever
        {
            /* Take a turn */
            use_energy(p);

            /* Make the breath attack an effect */
            effect = mem_zalloc(sizeof(struct effect));
            effect->index = EF_WEB;

            /* Cast the breath attack */
            source_player(who, get_player_index(get_connection(p->conn)), p);
            effect_do(effect, who, &ident, false, dir, NULL, 0, 0, NULL);

            free_effect(effect);
            
            return;
        }            
    }
    else if (streq(p->race->name, "Ooze") && p->lev > 5)
    {
        struct chunk *c = chunk_get(&p->wpos);
        use_energy(p);

        summon_specific_race_aux(p, c, &p->grid, get_race("oozling"), 1 + p->lev / 25, true);

        player_dec_timed(p, TMD_FOOD, (100 - p->lev), false);
        player_inc_timed(p, TMD_OCCUPIED, 1 + randint1(4), true, false);
        take_hit(p, p->mhp / 4, "overbreeding", false, "bred without sparing itself");

        return;
    }
    else if (streq(p->race->name, "Beholder"))
    {
        // can be used only on full HPs
        if (p->chp == p->mhp)
        {
            // element for ray
            int b_elem = randint0(1 + p->lev / 5);
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
            if (p->msp > 0)
            {
                dice_calc *= 2;
                effect->radius += p->lev / 10;
            }
            
            // init dice
            effect->dice = dice_new();
            // convert int to char
            snprintf(dice_string, sizeof(dice_string), "%d", dice_calc);
            // fill dice struct with dmg
            dice_parse_string(effect->dice, dice_string);

            // element
            switch (b_elem)
            {
                case 0: effect->subtype = PROJ_TURN_ALL; break; // scare
                case 1: effect->subtype = PROJ_MON_CONF; break; // conf
                case 2: effect->subtype = PROJ_MON_BLIND; break; // blind
                case 3: effect->subtype = PROJ_MON_STUN; break;// stun
                case 4: effect->subtype = PROJ_INERTIA; break;// slow
                case 5: effect->subtype = PROJ_MON_HOLD; break;// mob can't move
                case 6: effect->subtype = PROJ_SLEEP_ALL; break;// sleep
                case 7: effect->subtype = PROJ_DARK; break;
                case 8: effect->subtype = PROJ_PSI; break;
                case 9: effect->subtype = PROJ_PSI_DRAIN; break;
                case 10: effect->subtype = PROJ_TIME; break;
                case 11: effect->subtype = PROJ_DISEN; break;// spellcasters less successful
            }

            source_player(who, get_player_index(get_connection(p->conn)), p);
            effect_do(effect, who, &ident, false, dir, NULL, 0, 0, NULL);

            free_effect(effect);

            player_dec_timed(p, TMD_FOOD, 25, false);
            p->chp -= p->chp / 20; // take a slight hit
            player_inc_timed(p, TMD_IMAGE, 1 + randint0(1), false, false);
                
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
                case 4: summon_specific_race_aux(p, c, &p->grid, get_race("homunculus"), 1, true); break; // 
                case 5: summon_specific_race_aux(p, c, &p->grid, get_race("quasit"), 1, true); break; // 
                case 6: summon_specific_race_aux(p, c, &p->grid, get_race("imp"), 1, true); break; // 
                case 7: summon_specific_race_aux(p, c, &p->grid, get_race("flamme"), 1, true); break; // 
                case 8: summon_specific_race_aux(p, c, &p->grid, get_race("gargoyle"), 1, true); break; // 
                case 9: summon_specific_race_aux(p, c, &p->grid, get_race("bodak"), 1, true); break; // 
                case 10: summon_specific_race_aux(p, c, &p->grid, get_race("mezzodaemon"), 1, true); break; // 
                case 11: summon_specific_race_aux(p, c, &p->grid, get_race("death quasit"), 1, true); break; // 
            }

            player_inc_timed(p, TMD_OCCUPIED, 3 + randint0(2), true, false);
            p->chp -= p->chp / 3; // take a hit
            player_dec_timed(p, TMD_FOOD, (150 - p->lev), false);
        }
        else
            msgt(p, MSG_SPELL_FAIL, "You need to have full health to use this ability!");

        return;
    }
    else if (streq(p->race->name, "Djinn"))
    {
        // dice.. see 'beholder' for explanation
        int dice_calc = p->lev / 5 + randint1(100);
        char dice_string[5];

        // convert int to char
        snprintf(dice_string, sizeof(dice_string), "%d", dice_calc);

        use_energy(p);

        source_player(who, get_player_index(get_connection(p->conn)), p);
        effect_simple(EF_WONDER, who, dice_string, 0, 0, 0, 0, 0, NULL);

        player_dec_timed(p, TMD_FOOD, (50 - p->lev / 2), false);
        player_inc_timed(p, TMD_OCCUPIED, 1 + randint0(1), true, false);

        return;
    }
    else if (streq(p->race->name, "Pixie"))
    {
        // can be used only on full HPs
        if (p->chp == p->mhp)
        {
            use_energy(p);

            player_inc_timed(p, TMD_INVIS, 20 + p->lev, true, false);

            p->chp -= p->chp / 3; // take a hit
            player_dec_timed(p, TMD_FOOD, (50 - p->lev / 2), false);
            player_inc_timed(p, TMD_OCCUPIED, 1 + randint0(1), true, false);
        }
        else
            msgt(p, MSG_SPELL_FAIL, "You should have full health to become invisible.");

        return;
    }
    else if (streq(p->race->name, "Draconian"))
    {
        // can be used only on full HPs
        if (p->chp == p->mhp)
        {
            char dice_string[5];
            int dice_calc = randint0(1) + p->lev * (1 + p->lev / 25);

            use_energy(p);

            effect = mem_zalloc(sizeof(struct effect));
            effect->index = EF_SHORT_BEAM;
            effect->radius = 1 + randint0(p->lev / 5);
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

            player_dec_timed(p, TMD_FOOD, 25, false);
            p->chp -= p->chp / 5; // take a hit
        }
        else
            msgt(p, MSG_SPELL_FAIL, "You should have full health to breath fire.");

        return;
    }
    else if (streq(p->race->name, "Undead"))
    {
        use_energy(p);
        source_player(who, get_player_index(get_connection(p->conn)), p);
        effect_simple(EF_DETECT_LIVING_MONSTERS, who, 0, 0, 0, 0, 10 + p->lev / 2, 10 + p->lev / 2, NULL);
        p->chp -= p->chp / 15; // take a hit
        player_dec_timed(p, TMD_FOOD, 1 + p->lev / 5, false);
        player_inc_timed(p, TMD_OCCUPIED, 1 + randint0(1), true, false);
        return;
    }
    else if (streq(p->race->name, "Imp"))
    {
        use_energy(p);
        source_player(who, get_player_index(get_connection(p->conn)), p);
        effect_simple(EF_TELEPORT, who, "13", 0, 0, 0, 0, 0, NULL);
        player_inc_timed(p, TMD_OCCUPIED, 1 + randint0(1), true, false);
        return;
    }
    else if (streq(p->race->name, "Wraith"))
    {
        use_energy(p);
        source_player(who, get_player_index(get_connection(p->conn)), p);
        player_inc_timed(p, TMD_WRAITHFORM, 5, false, false);
        player_inc_timed(p, TMD_BLIND, 5, false, false);
        player_dec_timed(p, TMD_FOOD, 5, false);
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
        player_dec_timed(p, TMD_FOOD, 3, false);
        player_inc_timed(p, TMD_OCCUPIED, 1 + randint0(1), true, false);
        return;
    }
    else if (streq(p->race->name, "Lizardmen") && p->chp < p->mhp)
    {
        use_energy(p);
        // restore 3% HP or if almost full - restore all
        if (p->mhp < 30) // too small HP to be able to calculate 3%
            p->chp++;
        else if (p->chp + p->mhp / 30 < p->mhp)
            p->chp += p->mhp / 30; // restore 3%
        else
            p->chp = p->mhp;
        player_inc_timed(p, TMD_OCCUPIED, 2, false, false);
        p->upkeep->redraw |= (PR_HP);
        return;
    }
    else if (streq(p->race->name, "Ent") && !streq(p->clazz->name, "Shapechanger") &&
             p->lev > 5)
    {
        use_energy(p);
            
        effect = mem_zalloc(sizeof(struct effect));
        effect->index = EF_CREATE_TREES;

        source_player(who, get_player_index(get_connection(p->conn)), p);
        effect_do(effect, who, &ident, false, dir, NULL, 0, 0, NULL);

        free_effect(effect);

        player_dec_timed(p, TMD_FOOD, (350 - (p->lev * 5)), false);
        player_inc_timed(p, TMD_OCCUPIED, 1 + randint1(4), true, false);
        take_hit(p, p->mhp / 9, "exhaustion", false, "exhausted from gardening");

        return;
    }
    else if (streq(p->clazz->name, "Druid") && p->poly_race)
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
            }
            else
                msgt(p, MSG_SPELL_FAIL, "You need full mana to summon wolves!");
            return;
        }
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
