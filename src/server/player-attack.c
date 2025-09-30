/*
 * File: player-attack.c
 * Purpose: Attacks (both throwing and melee) by the player
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
 * Hit and breakage calculations
 */


/*
 * Returns percent chance of an object breaking after throwing or shooting.
 *
 * Artifacts will never break.
 *
 * Beyond that, each item kind has a percent chance to break (0-100). When the
 * object hits its target this chance is used.
 *
 * When an object misses it also has a chance to break. This is determined by
 * squaring the normaly breakage probability. So an item that breaks 100% of
 * the time on hit will also break 100% of the time on a miss, whereas a 50%
 * hit-breakage chance gives a 25% miss-breakage chance, and a 10% hit breakage
 * chance gives a 1% miss-breakage chance.
 */
int breakage_chance(const struct object *obj, bool hit_target)
{
    int perc = obj->kind->base->break_perc;

    if (obj->artifact) return 0;
    if (of_has(obj->flags, OF_THROWING) && !of_has(obj->flags, OF_EXPLODE) && !tval_is_ammo(obj))
        perc = 1;
    if (!hit_target) return (perc * perc) / 100;

    return perc;
}


/*
 * Calculate the player's base melee to-hit value without regard to a specific
 * monster.
 * See also: chance_of_missile_hit_base
 *
 * p The player
 * weapon The player's weapon
 */
int chance_of_melee_hit_base(const struct player *p, const struct object *weapon)
{
    int bonus = p->state.to_h;

    if (weapon) bonus += object_to_hit(weapon);
    if (p->state.bless_wield) bonus += 2;

    return p->state.skills[SKILL_TO_HIT_MELEE] + bonus * BTH_PLUS_ADJ;
}


/*
 * Calculate the player's melee to-hit value against a specific target.
 * See also: chance_of_missile_hit
 *
 * p The player
 * weapon The player's weapon
 * visible If the target is visible
 */
static int chance_of_melee_hit(const struct player *p, const struct object *weapon, bool visible)
{
    int chance = chance_of_melee_hit_base(p, weapon);

    /* Non-visible targets have a to-hit penalty of 50% */
    return (visible? chance: chance / 2);
}


/*
 * Calculate the player's base missile to-hit value without regard to a specific
 * monster.
 * See also: chance_of_melee_hit_base
 *
 * p The player
 * missile The missile to launch
 * launcher The launcher to use (optional)
 */
static int chance_of_missile_hit_base(struct player *p, struct object *missile,
    struct object *launcher)
{
    int bonus = object_to_hit(missile);
    int chance;

    if (!launcher)
    {
        /*
         * Other thrown objects are easier to use, but only throwing weapons
         * take advantage of bonuses to Skill and Deadliness from other
         * equipped items.
         */
        if (of_has(missile->flags, OF_THROWING))
        {
            bonus += p->state.to_h;
            chance = p->state.skills[SKILL_TO_HIT_THROW] + bonus * BTH_PLUS_ADJ;
        }
        else
            chance = 3 * p->state.skills[SKILL_TO_HIT_THROW] / 2 + bonus * BTH_PLUS_ADJ;
        
        // magic classes are VERY bad in throwing until high lvl
        if (player_of_has(p, OF_CLUMSY))
        {
            chance -= 50 - p->lev;
            if (chance < 5) chance = 5;
        }
    }
    else
    {
        bonus += p->state.to_h + object_to_hit(launcher);
        chance = p->state.skills[SKILL_TO_HIT_BOW] + bonus * BTH_PLUS_ADJ;
        
        // magic classes are bad in shooting until high lvl
        // also BAD_SHOOTER are bad in using weapons
        if ((player_of_has(p, OF_CLUMSY) || player_of_has(p, OF_BAD_SHOOTER)) && one_in_(2))
        {
            chance -= 50 - p->lev;
            if (chance < 5) chance = 5;
        }
    }

    return chance;
}


/*
 * Calculate the player's missile to-hit value against a specific target.
 * See also: chance_of_melee_hit
 *
 * p The player
 * missile The missile to launch
 * launcher The launcher to use (optional)
 * grid The target grid
 * visible If the target is visible
 */
static int chance_of_missile_hit(struct player *p, struct object *missile,
    struct object *launcher, struct loc *grid, bool visible)
{
    int chance = chance_of_missile_hit_base(p, missile, launcher);

    /* Penalize for distance */
    chance -= distance(&p->grid, grid);

    /* Non-visible targets have a to-hit penalty of 50% */
    return (visible? chance: chance / 2);
}


/*
 * Determine if a hit roll is successful against the target AC.
 * See also: hit_chance
 *
 * to_hit To total to-hit value to use
 * ac The AC to roll against
 */
bool test_hit(int to_hit, int ac)
{
    random_chance c;

    hit_chance(&c, to_hit, ac);
    return random_chance_check(&c);
}


/*
 * Return a random_chance by reference, which represents the likelihood of a
 * hit roll succeeding for the given to_hit and ac values. The hit calculation
 * will:
 *
 * Always hit 12% of the time
 * Always miss 5% of the time
 * Put a floor of 9 on the to-hit value
 * Roll between 0 and the to-hit value
 * The outcome must be >= AC*2/3 to be considered a hit
 *
 * chance The random_chance to return-by-reference
 * to_hit The to-hit value to use
 * ac The AC to roll against
 */
void hit_chance(random_chance *chance, int to_hit, int ac)
{
    /* Percentages scaled to 10,000 to avoid rounding error */
    const int HUNDRED_PCT = 10000;
    const int ALWAYS_HIT = 1200;
    const int ALWAYS_MISS = 500;

    /* Put a floor on the to_hit */
    to_hit = MAX(9, to_hit);

    /* Calculate the hit percentage */
    chance->numerator = MAX(0, to_hit - ac * 2 / 3);
    chance->denominator = to_hit;

    /* Convert the ratio to a scaled percentage */
    chance->numerator = HUNDRED_PCT * chance->numerator / chance->denominator;
    chance->denominator = HUNDRED_PCT;

    /* The calculated rate only applies when the guaranteed hit/miss don't */
    chance->numerator = chance->numerator * (HUNDRED_PCT - ALWAYS_MISS - ALWAYS_HIT) / HUNDRED_PCT;

    /* Add in the guaranteed hit */
    chance->numerator += ALWAYS_HIT;
}


/*
 * Damage calculations
 */


/*
 * Check if a target is debuffed in such a way as to make a critical
 * hit more likely.
 */
static bool is_debuffed(struct source *target)
{
    if (target->monster)
    {
        return (target->monster->m_timed[MON_TMD_CONF] || target->monster->m_timed[MON_TMD_HOLD] ||
            target->monster->m_timed[MON_TMD_FEAR] ||
            target->monster->m_timed[MON_TMD_STUN] || target->monster->m_timed[MON_TMD_BLIND]);
    }
    if (target->player)
    {
        return (target->player->timed[TMD_CONFUSED] || target->player->timed[TMD_CONFUSED_REAL] ||
            target->player->timed[TMD_PARALYZED] || player_of_has(target->player, OF_AFRAID) ||
            target->player->timed[TMD_BLIND] || target->player->timed[TMD_BLIND_REAL]);
    }
    return false;
}


/*
 * Determine damage for critical hits from shooting.
 *
 * Factor in item weight, total plusses, and player level.
 */
static int critical_shot(struct player *p, struct source *target, int weight, int plus, int dam,
    bool launched, uint32_t *msg_type)
{
    int to_h = p->state.to_h + plus;
    int chance, new_dam;
    struct object *bow = equipped_item_by_slot_name(p, "shooting");

    if (is_debuffed(target)) to_h += z_info->r_crit_debuff_toh;
    chance = z_info->r_crit_chance_weight_scl * weight +
        z_info->r_crit_chance_toh_scl * to_h +
        z_info->r_crit_chance_level_scl * p->lev +
        z_info->r_crit_chance_offset;
    if (launched)
        chance += z_info->r_crit_chance_launched_toh_skill_scl * p->state.skills[SKILL_TO_HIT_BOW];
    else
        chance += z_info->r_crit_chance_thrown_toh_skill_scl * p->state.skills[SKILL_TO_HIT_THROW];

    if (randint1(z_info->r_crit_chance_range) > chance || !z_info->r_crit_level_head)
    {
        if (bow)
        {
            if (kf_has(bow->kind->kind_flags, KF_SHOOTS_ARROWS))
                *msg_type = MSG_SHOOT_BOW;
            else if (kf_has(bow->kind->kind_flags, KF_SHOOTS_BOLTS))
                *msg_type = MSG_SHOOT_CROSSBOW;
            else if (kf_has(bow->kind->kind_flags, KF_SHOOTS_SHOTS))
                *msg_type = MSG_SHOOT_SLING;
        }
        else
            *msg_type = MSG_SHOOT; // rocks (boomerangs too for now..)

        new_dam = dam;
    }
    else
    {
        int power = z_info->r_crit_power_weight_scl * weight + randint1(z_info->r_crit_power_random);
        const struct critical_level *this_l = z_info->r_crit_level_head;

        while (power >= this_l->cutoff && this_l->next) this_l = this_l->next;
        *msg_type = this_l->msgt;
        new_dam = this_l->add + this_l->mult * dam;
        /* it was before:
            else if (power < 500)
                *msg_type = MSG_SHOOT_GOOD;
            else if (power < 1000)
                *msg_type = MSG_SHOOT_GREAT;
            else
                *msg_type = MSG_SHOOT_SUPERB;
        */
    }

    return new_dam;
}


/*
 * Determine damage for critical hits from melee.
 *
 * Factor in weapon weight, total plusses, player level.
 */
static int critical_melee(struct player *p, struct source *target, int weight, int plus, int dam,
    uint32_t *msg_type)
{
    int to_h = p->state.to_h + plus;
    int chance, new_dam;
    struct object *obj = equipped_item_by_slot_name(p, "weapon");

    if (is_debuffed(target)) to_h += z_info->m_crit_debuff_toh;
    chance = z_info->m_crit_chance_weight_scl * weight +
        z_info->m_crit_chance_toh_scl * to_h +
        z_info->m_crit_chance_level_scl * p->lev +
        z_info->m_crit_chance_toh_skill_scl * p->state.skills[SKILL_TO_HIT_MELEE] +
        z_info->m_crit_chance_offset;

    /* Apply Touch of Death */
    if (p->timed[TMD_DEADLY] && magik(25))
    {
        *msg_type = MSG_HIT_HI_CRITICAL;
        new_dam = 4 * dam; // + 30;
    }
    else if (randint1(z_info->m_crit_chance_range) > chance || !z_info->m_crit_level_head)
    {
        if (obj) // with weapon
        {
            if (obj->tval == TV_SWORD)
                *msg_type = MSG_HIT_BLADE;
            else if ((obj->tval == TV_HAFTED) && (obj->sval == lookup_sval(obj->tval, "Whip")))
                *msg_type = MSG_HIT_WHIP;
            else if (obj->tval == TV_HAFTED)
                *msg_type = MSG_HIT_MACE;
            else
                *msg_type = MSG_HIT;
        }
        else
            *msg_type = MSG_PUNCH; // unarmed (barehand)

        new_dam = dam;
    }
    else
    {
        int power = z_info->m_crit_power_weight_scl * weight + randint1(z_info->m_crit_power_random);
        const struct critical_level *this_l = z_info->m_crit_level_head;

        while (power >= this_l->cutoff && this_l->next) this_l = this_l->next;
        *msg_type = this_l->msgt;
        new_dam = this_l->add + this_l->mult * dam;
    }

    return new_dam;
}


struct delayed_effects
{
    bool fear;
    bool poison;
    bool cut;
    bool stun;
    bool slow;
    bool conf;
    bool blind;
    bool para;
    bool stab_sleep;
    bool stab_flee;
};


/*
 * Determine standard melee damage.
 *
 * Factor in damage dice, to-dam and any brand or slay.
 */
static int melee_damage(struct player *p, struct object *obj, random_value dice, int best_mult,
    struct source *target, struct delayed_effects *effects, int *d_dam)
{
    int dmg = randcalc(dice, 0, RANDOMISE);

    /* Base damage for Shadow touch and cuts/stuns */
    *d_dam = dmg;

    dmg *= best_mult;
    
    // Werewolves got CUT bonus at night
    if (streq(p->race->name, "Werewolf") && p->lev > 49 && best_mult < 2 && !is_daytime())
        dmg *= 2;

    /* Stabbing attacks (require a weapon) */
    if (target->monster && obj)
    {
        if (effects->stab_sleep) dmg *= (3 + p->lev / 40);
        if (effects->stab_flee) dmg = dmg * 3 / 2;
    }

    dmg += object_to_dam(obj);

    return dmg;
}


/*
 * Determine standard ranged damage.
 *
 * Factor in damage dice, to-dam, multiplier and any brand or slay.
 */
static int ranged_damage(struct player *p, struct object *missile, struct object *launcher,
    int best_mult, int mult)
{
    int dam;

    /* If we have a slay or brand, modify the multiplier appropriately */
    if (best_mult > 1)
    {
        if (mult > 1) mult += best_mult;
        else mult = best_mult;
    }

    /* Apply damage: multiplier, slays, bonuses */
    dam = damroll(missile->dd, missile->ds);

    dam += object_to_dam(missile);

    if (launcher)
    {
        dam += object_to_dam(launcher);
        
        // magic classes are bad in shooting until high lvl
        // also BAD_SHOOTER are bad in using weapons
        if ((player_of_has(p, OF_CLUMSY) || player_of_has(p, OF_BAD_SHOOTER)) && p->lev < 25 && one_in_(2))
            dam /= 2;
    }
    else if (of_has(missile->flags, OF_THROWING))
    {
        /* Adjust damage for throwing weapons */
        int might = 2 + missile->weight / 12;

        /* Good at throwing */
        if (player_has(p, PF_FAST_THROW)) might = 2 + (missile->weight + p->lev) / 12;

        // magic classes are VERY bad in throwing until high lvl
        if (player_of_has(p, OF_CLUMSY) && one_in_(2))
        {
            int divisor = 3 - p->lev / 15;
            if (divisor < 1) divisor = 1; // avoid division by 0 or negative
            dam /= divisor;
        }

        dam *= might;
    }

    dam *= mult;
    if (tval_is_ammo(missile) && p->timed[TMD_BOWBRAND] && !p->brand.blast) dam += p->brand.dam;

    return dam;
}


/*
 * Apply the player damage bonuses
 */
static int player_damage_bonus(struct player_state *state)
{
    return state->to_d;
}


/*
 * Non-damage melee blow effects
 */


/*
 * Apply blow side effects
 */
static void blow_side_effects(struct player *p, struct source *target,
    struct delayed_effects *effects, struct side_effects *seffects, bool do_conf,
    struct object *obj, char name[NORMAL_WID], bool do_blind, bool do_para, bool do_fear,
    bool *do_quake, int dmg, random_value dice, int d_dam, bool do_slow, bool do_circle)
{
    /* Apply poison */
    if (seffects->do_poison)
    {
        if (target->monster)
        {
            if (mon_inc_timed(p, target->monster, MON_TMD_POIS, 5 + randint1(5),
                MON_TMD_FLG_NOMESSAGE))
            {
                effects->poison = true;
            }
        }
        else
            player_inc_timed(target->player, TMD_POISONED, randint1(p->lev) + 5, true, true);
    }

    /* Apply life leech */
    // doesn't work on powerful mobs (except for vampire)
    if (streq(p->race->name, "Vampire") && target->monster &&
        monster_is_living(target->monster))
    {
        int drain = ((d_dam > target->monster->hp)? target->monster->hp: d_dam);

        // circle attack gives less life leech
        if (do_circle)
            drain /= 2;

        if (monster_is_powerful(target->monster->race) && one_in_(2))
            ;
        else
            hp_player_safe(p, 1 + drain / 2);
    }
    else if ((p->timed[TMD_ATT_VAMP] || seffects->do_leech) && target->monster &&
        monster_is_living(target->monster) && !monster_is_powerful(target->monster->race))
    {
        int drain = ((d_dam > target->monster->hp)? target->monster->hp: d_dam);

        // circle attack gives less life leech
        if (do_circle)
            drain /= 2;

        hp_player_safe(p, 1 + drain / 3);
    }
    else if (streq(p->clazz->name, "Unbeliever") && target->monster &&
        target->monster->race->freq_spell && !target->monster->race->freq_innate &&
        !monster_is_powerful(target->monster->race))
    {
        int drain = ((d_dam > target->monster->hp)? target->monster->hp: d_dam);

        // circle attack gives less life leech
        if (do_circle)
            drain /= 2;

        hp_player_safe(p, 1 + drain / 4);
    }
    else if (streq(p->clazz->name, "Inquisitor") && target->monster &&
        monster_is_fearful(target->monster) && !monster_is_unique(target->monster) && !monster_is_powerful(target->monster->race))
    {
        int drain = ((d_dam > target->monster->hp)? target->monster->hp: d_dam);

        // circle attack gives less life leech
        if (do_circle)
            drain /= 2;

        hp_player_safe(p, 1 + drain / 4);
    }

    // Necromancer got small additional life leech (traumaturgy)
    if (streq(p->clazz->name, "Necromancer") && target->monster &&
        monster_is_living(target->monster))
    {
            int drain = p->lev / 10;

            // circle attack gives less life leech
            if (do_circle)
                drain /= 2;

            hp_player_safe(p, 1 + drain);
    }

    // Mage's "Frost Shield" spell gives cold brand
    if (p->timed[TMD_ICY_AURA] && (streq(p->clazz->name, "Mage") ||
        streq(p->clazz->name, "Battlemage") || streq(p->clazz->name, "Elementalist")) && p->lev > 20)
    {
        player_inc_timed(p, TMD_ATT_COLD, 5, true, true);
    }

    /* Confusion attack */
    if (p->timed[TMD_ATT_CONF])
    {
        player_clear_timed(p, TMD_ATT_CONF, true);
        msg(p, "Your hands stop glowing.");
        do_conf = true;
    }
    // ... and Cutthroat stance check
    else if (p->timed[TMD_CRUSHING_STANCE] && magik(1 + p->lev / 3)) // 1 -> 17%
    {
        do_conf = true;
    }

    /* Handle polymorphed players */
    if (p->poly_race && obj)
    {
        int m = randint0(z_info->mon_blows_max);

        /* Extract the attack infomation */
        struct blow_effect *effect = p->poly_race->blow[m].effect;
        struct blow_method *method = p->poly_race->blow[m].method;

        melee_effect_handler_context_t context;
        melee_effect_handler_f effect_handler;

        /* There must be an attack */
        if (method)
        {
            /* Describe the attack method */
            seffects->do_cut = method->cut;
            seffects->do_stun = method->stun;

            /* Initialize */
            context.p = p;
            context.target = target;
            context.ddesc = name;
            context.do_blind = do_blind;
            context.do_para = do_para;
            context.do_conf = do_conf;
            context.do_fear = do_fear;
            strnfmt(context.flav, sizeof(context.flav), "was killed by %s", name);
            context.do_quake = *do_quake;
            context.do_stun = seffects->do_stun;
            context.damage = dmg;
            context.style = TYPE_PVX;

            /* Perform the actual effect. */
            effect_handler = melee_handler_for_blow_effect(effect->name);

            if (effect_handler != NULL)
                effect_handler(&context);
            else
                plog_fmt("Effect handler not found for %s.", effect->name);

            /* Save any changes made in the handler for later use. */
            do_blind = context.do_blind;
            do_para = context.do_para;
            do_conf = context.do_conf;
            do_fear = context.do_fear;
            *do_quake = context.do_quake;
            seffects->do_stun = context.do_stun;
        }
    }

    /* Ghosts get fear attacks */
    if (p->ghost && !player_can_undead(p)) do_fear = true;

    // Werewolves got CUT at night
    if (streq(p->race->name, "Werewolf") && !is_daytime() && p->lev > 14)
        seffects->do_cut = true;
    // ... and Cutthroat stance check
    else if (p->timed[TMD_CUTTING_STANCE] && magik(p->lev)) // 1 -> 50%
         seffects->do_cut = true;

    /* Only one of cut or stun */
    if (seffects->do_cut && seffects->do_stun)
    {
        /* Cancel cut */
        if (magik(50))
            seffects->do_cut = 0;

        /* Cancel stun */
        else
            seffects->do_stun = 0;
    }

    /* Handle cut */
    if (seffects->do_cut)
    {
        /* PvP */
        if (target->player)
        {
            seffects->do_cut = get_cut(dice, d_dam);

            /* Apply the cut */
            if (seffects->do_cut)
                player_inc_timed(target->player, TMD_CUT, seffects->do_cut, true, true);
        }
        else if (mon_inc_timed(p, target->monster, MON_TMD_CUT, 5 + randint1(5),
            MON_TMD_FLG_NOMESSAGE))
        {
            effects->cut = true;
        }
    }

    /* Handle stun */
    if (seffects->do_stun)
    {
        /* PvP: stunning attack */
        if (target->player)
        {
            seffects->do_stun = get_stun(dice, d_dam);

            /* Apply the stun */
            if (seffects->do_stun)
                player_inc_timed(target->player, TMD_STUN, seffects->do_stun, true, true);
        }
        else if (mon_inc_timed(p, target->monster, MON_TMD_STUN, 5 + randint1(5),
            MON_TMD_FLG_NOMESSAGE))
        {
            effects->stun = true;
        }
    }

    /* Apply slowing */
    if (do_slow)
    {
        /* PvP: slowing attack */
        if (target->player)
            player_inc_timed(target->player, TMD_SLOW, randint0(4) + 4, true, true);
        else if (dmg && mon_inc_timed(p, target->monster, MON_TMD_SLOW, 20, MON_TMD_FLG_NOMESSAGE))
            effects->slow = true;
    }

    /* Apply fear */
    if (do_fear)
    {
        /* PvP: fear attack */
        if (target->player)
        {
            /* Player is terrified */
            if (magik(target->player->state.skills[SKILL_SAVE]))
                msg(p, "%s is unaffected.", name);
            else
                player_inc_timed(target->player, TMD_AFRAID, 3 + randint1(p->lev), true, true);
        }
        else if (mon_inc_timed(p, target->monster, MON_TMD_FEAR, 10 + randint1(10),
            MON_TMD_FLG_NOMESSAGE))
        {
            effects->fear = true;
        }
    }

    /* Apply confusion */
    if (do_conf)
    {
        /* PvP: confusing attack */
        if (target->player)
        {
            /* Player is confused */
            player_inc_timed(target->player, TMD_CONFUSED, 10 + randint0(p->lev) / 10, true, true);
        }
        else if (mon_inc_timed(p, target->monster, MON_TMD_CONF, 5 + randint1(5),
            MON_TMD_FLG_NOMESSAGE))
        {
            effects->conf = true;
        }
    }

    /* Apply blindness */
    if (do_blind)
    {
        /* PvP: blinding attack */
        if (target->player)
        {
            /* Player is blinded */
            player_inc_timed(target->player, TMD_BLIND, 10 + randint1(p->lev), true, true);
        }
        else if (mon_inc_timed(p, target->monster, MON_TMD_BLIND, 5 + randint1(5),
            MON_TMD_FLG_NOMESSAGE))
        {
            effects->blind = true;
        }
    }

    /* Handle paralysis */
    if (do_para)
    {
        /* PvP: paralyzing attack */
        if (target->player)
        {
            /* Player is paralyzed */
            player_inc_timed(target->player, TMD_PARALYZED, 3 + randint1(p->lev), true, true);
        }
        else if (mon_inc_timed(p, target->monster, MON_TMD_HOLD, 3 + randint1(5),
            MON_TMD_FLG_NOMESSAGE))
        {
            effects->para = true;
        }
    }
}


/*
 * Apply blow after effects
 */
static bool blow_after_effects(struct player *p, struct chunk *c, struct loc *grid, bool circle,
    int splash, bool quake)
{
    bool stop = false;

    /* Apply circular kick: do damage to anything around the attacker */
    if (circle)
    {
        fire_ball(p, PROJ_MISSILE, 0, splash, 1, false, true);
        show_monster_messages(p);

        /* Target may be dead */
        if (!square(c, grid)->mon) stop = true;
    }

    /* Apply earthquake brand */
    if (quake)
    {
        struct source who_body;
        struct source *who = &who_body;

        source_player(who, get_player_index(get_connection(p->conn)), p);
        effect_simple(EF_EARTHQUAKE, who, "0", 0, 10, 0, 0, 0, NULL);

        /* Target may be dead or moved */
        if (!square(c, grid)->mon) stop = true;
    }

    return stop;
}


/*
 * Melee attack
 */


/* Melee and throwing hit types */
static const struct hit_types melee_hit_types[] =
{
    {MSG_MISS, NULL},
    {MSG_HIT, NULL},
    {MSG_HIT_BLADE, NULL},
    {MSG_HIT_MACE, NULL},
    {MSG_HIT_WHIP, NULL},
    {MSG_PUNCH, NULL},
    {MSG_HIT_GOOD, "It was a good hit!"},
    {MSG_HIT_GREAT, "It was a great hit!"},
    {MSG_HIT_SUPERB, "It was a superb hit!"},
    {MSG_HIT_HI_GREAT, "It was a *GREAT* hit!"},
    {MSG_HIT_HI_SUPERB, "It was a *SUPERB* hit!"},
    {MSG_HIT_HI_CRITICAL, "It was a *CRITICAL* hit!"}
};


/* Special effects for barehanded attacks */
enum
{
    #define MA(a) MA_##a,
    #include "list-attack-effects.h"
    #undef MA
    MA_MAX
};


/*
 * Attack the monster at the given location with a single blow.
 */
static bool py_attack_real(struct player *p, struct chunk *c, struct loc *grid,
    struct delayed_effects *effects)
{
    size_t i;

    /* Information about the target of the attack */
    struct source target_body;
    struct source *target = &target_body;
    char target_name[NORMAL_WID];
    bool stop = false;
    bool visible;
    int ac;
    char name[NORMAL_WID];

    /* The weapon used */
    struct object *obj = equipped_item_by_slot_name(p, "weapon");

    /* Information about the attack */
    int splash = 0;
    bool do_quake = false;
    bool success = false;

    char verb[30], hit_extra[30];
    uint32_t msg_type = MSG_HIT;
    int dmg, d_dam, reduced;

    /* Information about the attacker */
    char killer_name[NORMAL_WID];
    random_value dice;
    int show_mhit, show_mdam;
    int show_shit, show_sdam;
    bool do_circle = false;
    bool do_slow = false, do_fear = false, do_conf = false, do_blind = false, do_para = false;
    struct side_effects seffects;

    memset(&seffects, 0, sizeof(seffects));

    /* Default to punching */
    my_strcpy(verb, "punch", sizeof(verb));
    if (obj) my_strcpy(verb, "hit", sizeof(verb));
    hit_extra[0] = '\0';

    /* Information about the target of the attack */
    square_actor(c, grid, target);
    if (target->monster)
    {
        visible = monster_is_visible(p, target->idx);
        ac = target->monster->ac;
    }
    else
    {
        visible = player_is_visible(p, target->idx);
        ac = target->player->state.ac + target->player->state.to_a;
    }

    /* Extract target name */
    if (target->monster)
        monster_desc(p, target_name, sizeof(target_name), target->monster, MDESC_TARG);
    else
    {
        player_desc(p, target_name, sizeof(target_name), target->player, false);
        player_desc(p, name, sizeof(name), target->player, true);
    }

    /* Auto-Recall if possible and visible */
    if (target->monster && visible) monster_race_track(p->upkeep, target);

    /* Track a new monster */
    if (visible) health_track(p->upkeep, target);

    /* Handle player fear */
    if (player_of_has(p, OF_AFRAID))
    {
        equip_learn_flag(p, OF_AFRAID);
        msgt(p, MSG_AFRAID, "You are too afraid to attack %s!", target_name);
        return false;
    }

    /* Disturb the target */
    if (target->monster)
    {
        monster_wake(p, target->monster, false, 100);
        mon_clear_timed(p, target->monster, MON_TMD_HOLD, MON_TMD_FLG_NOTIFY);
    }
    else
        disturb(target->player, 0);

    /* See if the player hit */
    success = test_hit(chance_of_melee_hit(p, obj, visible), ac);

    /* Extract killer name */
    if (target->player)
        player_desc(target->player, killer_name, sizeof(killer_name), p, true);

    /* If a miss, skip this hit */
    if (!success)
    {
        //  Cutthroat penetrates AC (additional success in 1% -> 17%)
        if (p->timed[TMD_PIERCING_STANCE] && magik(1 + (p->lev / 3)))
        {
            ; // lucky stab
        }
        else // YES, SKIP THIS HIT
        {
            effects->stab_sleep = false;
            msgt(p, MSG_MISS, "You miss %s.", target_name);
            if (target->player) msg(target->player, "%s misses you.", killer_name);

            /* Small chance of bloodlust side-effects */
            if (p->timed[TMD_BLOODLUST] && one_in_(50))
            {
                msg(p, "You feel strange...");
                player_over_exert(p, PY_EXERT_SCRAMBLE, 20, 20);
            }

            return false;
        }
    }

    /* Information about the attacker */
    memset(&dice, 0, sizeof(dice));
    get_plusses(p, &p->known_state, &dice.dice, &dice.sides, &show_mhit, &show_mdam, &show_shit,
        &show_sdam);

    /* Ghosts do barehanded damage relative to level */
    if (p->ghost && !player_can_undead(p))
        dmg = d_dam = randcalc(dice, 0, RANDOMISE);
    else
    {
        int best_mult = 1;

        /* Handle polymorphed players + temp branding */
        player_attack_modifier(p, target, &best_mult, &seffects, verb, sizeof(verb), false, false);

        /* Best attack from all slays or brands on all non-launcher equipment */
        for (i = 2; i < (size_t)p->body.count; i++)
        {
            struct object *equipped = slot_object(p, i);

            if (equipped)
            {
                improve_attack_modifier(p, equipped, target, &best_mult, &seffects, verb,
                    sizeof(verb), false);
            }
        }

        /* Handle barehanded damage from special attacks */
        if (p->race->attacks || p->clazz->attacks)
        {
            struct barehanded_attack *attacks, *attack;
            int count = 0, pick;
            bool ok;

            /* Take special attacks from class 1/3 of the time */
            if (p->race->attacks && p->clazz->attacks)
            {
                if (magik(66)) attacks = p->race->attacks;
                else attacks = p->clazz->attacks;
            }
            else if (p->race->attacks) attacks = p->race->attacks;
            else attacks = p->clazz->attacks;

            /* Count the available special attacks */
            attack = attacks;
            while (attack)
            {
                count++;
                attack = attack->next;
            }

            /* Pick one */
            do
            {
                pick = randint0(count);
                attack = attacks;
                while (pick)
                {
                    pick--;
                    attack = attack->next;
                }

                ok = true;

                /* Special monk attacks: only when not stunned, confused or encumbered */
                if (player_has(p, PF_MARTIAL_ARTS) && (attacks == p->clazz->attacks) &&
                    (attack->effect != MA_NONE))
                {
                    ok = !p->timed[TMD_STUN] && !p->timed[TMD_CONFUSED] &&
                         !p->timed[TMD_CONFUSED_REAL] && monk_armor_ok(p);
                }

                /* Apply minimum level */
                if (ok) ok = (attack->min_level <= p->lev);

                /* Chance of failure vs player level */
                if (ok) ok = !CHANCE(attack->chance, p->lev);
            }
            while (!ok);

            my_strcpy(verb, attack->verb, sizeof(verb));
            my_strcpy(hit_extra, attack->hit_extra, sizeof(hit_extra));

            /* Special effect: extra damage side */
            if (attack->effect == MA_SIDE) dice.sides++;

            /* Special effect: extra damage dice */
            if (attack->effect == MA_DICE) dice.dice++;

            /* Special effect: crushing attack */
            if (attack->effect == MA_CRUSH)
            {
                if (p->poly_race) // dragon nerf
                {
                    dice.dice++;
                    dice.sides++;
                }
                else
                {
                    dice.dice += 2;
                    dice.sides += 2;
                }
                seffects.do_stun = 1;
            }

            /* Compute the damage */
            dmg = d_dam = randcalc(dice, 0, RANDOMISE);
            dmg *= best_mult;
            dmg = critical_melee(p, target, p->lev * randint1(10), p->lev, dmg, &msg_type);

            /* Special effect: knee attack */
            if (attack->effect == MA_KNEE)
            {
                bool male = false;

                /* Male target */
                if (target->monster)
                    male = rf_has(target->monster->race->flags, RF_MALE);
                else
                    male = (target->player->psex == SEX_MALE);

                /* Stuns male targets */
                if (male)
                {
                    my_strcpy(hit_extra, " in the groin with your knee", sizeof(hit_extra));
                    seffects.do_stun = 1;
                }
            }

            /* Special effect: extra damage */
            if (attack->effect == MA_DAM)
            {
                if (p->poly_race) // dragon nerf
                    dmg = dmg * 6 / 5;
                else
                    dmg = dmg * 5 / 4;
            }

            /* Special effect: slowing attack */
            if (attack->effect == MA_SLOW)
            {
                /* Slows some targets */
                if (target->monster && !rf_has(target->monster->race->flags, RF_NEVER_MOVE) &&
                    !monster_is_shape_unique(target->monster) &&
                    (is_humanoid(target->monster->race) ||
                    rf_has(target->monster->race->flags, RF_HAS_LEGS)) &&
                    !CHANCE(target->monster->level - 10, (dmg < 11)? 1: (dmg - 10)))
                {
                    my_strcpy(verb, "kick", sizeof(verb));
                    my_strcpy(hit_extra, " in the ankle", sizeof(hit_extra));
                    do_slow = true;
                }
            }

            /* Special effect: stunning attack */
            if ((attack->effect == MA_STUN) || (attack->effect == MA_JUMP))
                seffects.do_stun = 1;

            /* Special effect: cutting attack */
            if ((attack->effect == MA_CUT) || (attack->effect == MA_JUMP))
                seffects.do_cut = 1;

            /* Special effect: circular attack */
            if (attack->effect == MA_CIRCLE)
            {
                splash = dmg;
                do_circle = true;
            }

            /* Special non-racial dragon/hydra attacks: tail attacks */
            if ((player_has(p, PF_DRAGON) || player_has(p, PF_HYDRA)) &&
                (attacks == p->clazz->attacks))
            {
                my_strcpy(verb, "hit", sizeof(verb));
                my_strcpy(hit_extra, " with your tail", sizeof(hit_extra));
            }
        }
        else
        {
            int weight = 0;

            /* Handle normal weapon */
            if (obj)
            {
                weight = obj->weight;
                improve_attack_modifier(p, obj, target, &best_mult, &seffects, verb, sizeof(verb),
                    false);
            }

            /* Get the damage */
            dmg = melee_damage(p, obj, dice, best_mult, target, effects, &d_dam);

            /* For now, exclude criticals on unarmed combat */
            if (obj)
            {
                dmg = critical_melee(p, target, weight, object_to_hit(obj), dmg, &msg_type);

                /* Learn by use for the weapon */
                object_notice_attack_plusses(p, obj);
            }


            /* Splash damage and earthquakes */
            splash = (weight * dmg) / 100;

            // Grond... quake chance halves with each additional BpR (1 BpR = 20%, 2 BpR = 10%, etc.)
            if (player_of_has(p, OF_IMPACT) && dmg > 50)
            {
                // Base chance is 20% (1 in 5), then halves for each BpR..
                // 2 BpR: 10% chance (1 in 10).. 3 BpR: 5% chance (1 in 20).. etc
                uint32_t chance_threshold = 5 << ((p->state.num_blows / 100) - 1);
                if ((RNG % chance_threshold) == 0)
                {
                    do_quake = true;
                    equip_learn_flag(p, OF_IMPACT);
                }
            }
        }
    }

    /* Learn by use for other equipped items */
    equip_learn_on_melee_attack(p);

    /* Apply the player damage bonuses */
    dmg += player_damage_bonus(&p->state);

    /* PWMAngband: freezing aura reduces damage  */
    if (target->player && (target->player->timed[TMD_ICY_AURA] > 0 ||
        target->player->timed[TMD_DEFENSIVE_STANCE] ||
        (target->player->poly_race && streq(target->player->poly_race->name, "bear-form"))))
        dmg = (dmg * 90) / 100;

    /* No negative damage; change verb if no damage done */
    if (dmg <= 0)
    {
        dmg = 0;
        msg_type = MSG_MISS;
        my_strcpy(verb, "fail to harm", sizeof(verb));
        hit_extra[0] = '\0';
    }

    /* Special messages */
    if (target->monster)
    {
        reduced = dmg;

        /* Stabbing attacks */
        if (reduced && effects->stab_sleep)
            my_strcpy(verb, "cruelly stab", sizeof(verb));
        if (reduced && effects->stab_flee)
            my_strcpy(verb, "backstab", sizeof(verb));
    }
    else
    {
        /*
         * Player damage reduction does not affect the damage used for
         * side effect calculations so leave dmg as is.
         */
        reduced = player_apply_damage_reduction(target->player, dmg, false, "strange melee blow");
        if (!reduced)
        {
            msg_type = MSG_MISS;
            my_strcpy(verb, "fail to harm", sizeof(verb));
            hit_extra[0] = '\0';
        }

        /* Tell the target what happened */
        if (!reduced)
            msg(target->player, "%s fails to harm you.", killer_name);
        else if (OPT(target->player, show_damage))
            msg(target->player, "%s hits you for $r%d^r damage.", killer_name, reduced);
        else
            msg(target->player, "%s hits you.", killer_name);
    }

    /* Tell the player what happened */
    for (i = 0; i < N_ELEMENTS(melee_hit_types); i++)
    {
        const char *dmg_text = "";

        if (msg_type != melee_hit_types[i].msg_type) continue;
        if (reduced && OPT(p, show_damage)) dmg_text = format(" for $g%d^g damage", reduced);
        if (melee_hit_types[i].text)
        {
            msgt(p, msg_type, "You %s %s%s%s. %s", verb, target_name, hit_extra, dmg_text,
                melee_hit_types[i].text);
        }
        else
        {
            // sound for unarmed (barehand) attack
            if (!obj)
                msg_type = MSG_PUNCH;
            
            msgt(p, msg_type, "You %s %s%s%s.", verb, target_name, hit_extra, dmg_text);
        }
    }

    effects->stab_sleep = false;

    // Knight offensive stance AoE
    if (p->timed[TMD_OFFENSIVE_STANCE] && one_in_(2))
    {
        do_circle = true;
        splash = dmg;
    }

    /* Pre-damage side effects */
    // added do_circle to check vampiric attacks
    blow_side_effects(p, target, effects, &seffects, do_conf, obj, name, do_blind, do_para, do_fear,
        &do_quake, dmg, dice, d_dam, do_slow, do_circle);

    /* Damage, check for fear and death */
    if (target->monster)
        stop = mon_take_hit(p, c, target->monster, dmg, &effects->fear, -2);
    else
    {
        char df[160];

        strnfmt(df, sizeof(df), "was brutally murdered by %s", p->name);
        stop = take_hit(target->player, reduced, p->name, df);

        /* Handle freezing aura */
        if (!stop && target->player->timed[TMD_ICY_AURA])
        {
            fire_ball(target->player, PROJ_ICE, 0,
                1 + target->player->lev / 5 + randint0(dmg) / 10, 1, false, true);

            /* Stop if player is dead */
            if (p->is_dead) stop = true;

            /* Stop if player is stunned */
            if (p->timed[TMD_STUN]) stop = true;
        }
    }

    /* Small chance of bloodlust side-effects */
    if (p->timed[TMD_BLOODLUST] && one_in_(50))
    {
        msg(p, "You feel something give way!");
        player_over_exert(p, PY_EXERT_CON, 20, 0);
    }

    if (stop) memset(effects, 0, sizeof(struct delayed_effects));

    /* Post-damage effects */
    if (blow_after_effects(p, c, grid, do_circle, splash, do_quake))
        stop = true;

    return stop;
}


/*
 * Attempt a shield bash; return true if the monster dies
 */
static bool attempt_shield_bash(struct player *p, struct chunk *c, struct monster *mon, bool *fear,
    int *blows, int num_blows)
{
    struct object *weapon = slot_object(p, slot_by_name(p, "weapon"));
    struct object *shield = slot_object(p, slot_by_name(p, "arm"));
    int bash_quality, bash_dam;

    /* Bashing chance depends on melee skill, DEX, and a level bonus. */
    int bash_chance = p->state.skills[SKILL_TO_HIT_MELEE] / 8 +
        adj_dex_th[p->state.stat_ind[STAT_DEX]] / 2;

    /* No shield, no bash */
    if (!shield) return false;

    /* Monster is too pathetic, don't bother */
    if (mon->race->level < p->lev / 2) return false;

    /* Players bash more often when they see a real need: */
    if (!weapon)
    {
        /* Unarmed... */
        if (!p->poly_race) // except polymorphed players
            bash_chance *= 4;
    }
    else if (weapon->dd * weapon->ds * num_blows < shield->dd * shield->ds * 3)
    {
        /* ... or armed with a puny weapon */
        bash_chance *= 2;
    }

    /* Try to get in a shield bash. */
    if (bash_chance <= randint0(200 + mon->race->level)) return false;

    /* Calculate attack quality, a mix of momentum and accuracy. */
    bash_quality = p->state.skills[SKILL_TO_HIT_MELEE] / 4 + p->wt / 8 +
        p->upkeep->total_weight / 80 + shield->weight / 2;

    /* Calculate damage. Big shields are deadly. */
    bash_dam = damroll(shield->dd, shield->ds);

    /* Multiply by quality and experience factors */
    bash_dam *= bash_quality / 40 + p->lev / 14;

    /* Strength bonus. */
    bash_dam += adj_str_td[p->state.stat_ind[STAT_STR]];

    /* Paranoia. */
    if (bash_dam <= 0) return false;
    bash_dam = MIN(bash_dam, 125);

    if (OPT(p, show_damage)) msgt(p, MSG_HIT, "You get in a shield bash for $g%d^g damage!", bash_dam);
    else msgt(p, MSG_HIT, "You get in a shield bash!");

    /* Encourage the player to keep wearing that heavy shield. */
    if (randint1(bash_dam) > 30 + randint1(bash_dam / 2))
        msgt(p, MSG_HIT_HI_SUPERB, "WHAMM!");

    /* Damage, check for fear and death. */
    if (mon_take_hit(p, c, mon, bash_dam, fear, -2)) return true;

    /* Stunning. */
    if (bash_quality + p->lev > randint1(200 + mon->race->level * 8))
        mon_inc_timed(p, mon, MON_TMD_STUN, randint0(p->lev / 5) + 4, 0);

    /* Confusion. */
    if (bash_quality + p->lev > randint1(300 + mon->race->level * 12))
        mon_inc_timed(p, mon, MON_TMD_CONF, randint0(p->lev / 5) + 4, 0);

    /* The player will sometimes stumble. */
    if (35 + adj_dex_th[p->state.stat_ind[STAT_DEX]] < randint1(60))
    {
        *blows += randint1(num_blows);
        msgt(p, MSG_GENERIC, "You stumble!");
    }

    return false;
}


/*
 * Attack the monster at the given location
 *
 * We get blows until energy drops below that required for another blow, or
 * until the target monster dies. Each blow is handled by py_attack_real().
 * We don't allow @ to spend more than 1 turn's worth of energy, to avoid slower
 * monsters getting double moves.
 */
void py_attack(struct player *p, struct chunk *c, struct loc *grid)
{
    int num_blows;
    bool stop = false;
    int blows = 0;
    struct delayed_effects effects;
    struct monster *mon = square_monster(c, grid);
    bool visible = (mon && monster_is_visible(p, mon->midx));

    memset(&effects, 0, sizeof(effects));

    /* Handle polymorphed players */
    if (p->poly_race && rf_has(p->poly_race->flags, RF_NEVER_BLOW)) return;

    /* Unaware players attacking something reveal themselves */
    if (p->k_idx) aware_player(p, p);

    /* Rogues get stabbing attacks against sleeping and fleeing (visible) monsters */
    if (visible && player_has(p, PF_BACK_STAB))
    {
        if (mon->m_timed[MON_TMD_SLEEP]) effects.stab_sleep = true;
        else if (mon->m_timed[MON_TMD_FEAR]) effects.stab_flee = true;
    }

    /* Disturb the player */
    disturb(p, 0);

    /* Calculate number of blows */
    num_blows = (p->state.num_blows + p->frac_blow) / 100;

    /* Calculate remainder */
    p->frac_blow = (p->state.num_blows + p->frac_blow) % 100;

    /* Reward blackguards with 5% of max SPs, min 1/2 point */
    // only in dungeon
    if (player_has(p, PF_COMBAT_REGEN) && p->wpos.depth > 0)
    {
        int32_t sp_gain = (((int32_t)MAX(p->msp, 10)) * 16384) / 5;

        player_adjust_mana_precise(p, sp_gain);
    }

    /* Player attempts a shield bash if they can, and if monster is visible and not too pathetic */
    if (visible && player_has(p, PF_SHIELD_BASH))
    {
        /* Monster may die */
        stop = attempt_shield_bash(p, c, mon, &effects.fear, &blows, num_blows);
    }

    /* Take blows until energy runs out or monster dies */
    while ((blows < num_blows) && !stop)
    {
        stop = py_attack_real(p, c, grid, &effects);
        blows++;
    }

    /* Player attempts a phantom blow if monster is visible */
    if (visible && p->timed[TMD_HOLD_WEAPON] && !stop)
        stop = mon_take_hit(p, c, mon, damroll(p->lev / 7 - 1, 6), &effects.fear, -2);

    /* Delay messages */
    if (visible && !stop)
    {
        if (effects.fear) add_monster_message(p, mon, MON_MSG_FLEE_IN_TERROR, true);
        if (effects.poison) add_monster_message(p, mon, MON_MSG_POISONED, true);
        if (effects.cut) add_monster_message(p, mon, MON_MSG_BLEED, true);
        if (effects.stun) add_monster_message(p, mon, MON_MSG_DAZED, true);
        if (effects.slow) add_monster_message(p, mon, MON_MSG_SLOWED, true);
        if (effects.conf) add_monster_message(p, mon, MON_MSG_CONFUSED, true);
        if (effects.blind) add_monster_message(p, mon, MON_MSG_BLIND, true);
        if (effects.para) add_monster_message(p, mon, MON_MSG_HELD, true);
    }
}


void un_power(struct player *p, struct source *who, bool* obvious)
{
    struct object *obj;
    int tries;
    int unpower = 0, newcharge;
    int rlev;

    /* Get level */
    if (who->monster)
        rlev = ((who->monster->level >= 1)? who->monster->level: 1);
    else
        rlev = who->player->lev;

    /* Find an item */
    for (tries = 0; tries < 10; tries++)
    {
        /* Pick an item */
        obj = p->upkeep->inven[randint0(z_info->pack_size)];

        /* Skip non-objects */
        if (obj == NULL) continue;

        /* Drain charged wands/staves */
        if (tval_can_have_charges(obj))
        {
            /* Charged? */
            if (obj->pval)
            {
                /* Get number of charge to drain */
                unpower = (rlev / (obj->kind->level + 2)) + 1;

                /* Get new charge value, don't allow negative */
                newcharge = MAX((obj->pval - unpower), 0);

                /* Remove the charges */
                obj->pval = newcharge;
            }
        }

        if (unpower)
        {
            int heal = rlev * unpower;

            /* Message */
            msg(p, "Energy drains from your pack!");

            /* Obvious */
            *obvious = true;

            /* Heal */
            if (who->monster)
            {
                /* Don't heal more than max hp */
                heal = MIN(heal, who->monster->maxhp - who->monster->hp);

                /* Heal */
                who->monster->hp += heal;

                /* Redraw (later) if needed */
                update_health(who);
            }
            else
                hp_player_safe(who->player, heal);

            /* Combine the pack */
            p->upkeep->notice |= (PN_COMBINE);

            /* Redraw stuff */
            set_redraw_inven(p, obj);

            /* Affect only a single inventory slot */
            break;
        }
    }
}


static void use_fud(struct player *p, struct object *obj)
{
    struct effect *effect;
    bool ident = false, used = false;

    /* The player is aware of the object's flavour */
    p->was_aware = object_flavor_is_aware(p, obj);

    /* Figure out effect to use */
    effect = object_effect(obj);

    /* Do effect */
    if (effect)
    {
        struct source who_body;
        struct source *who = &who_body;

        /* Make a noise! */
        sound(p, MSG_EAT);

        /* Do effect */
        if (effect->other_msg) msg_misc(p, effect->other_msg);
        source_player(who, get_player_index(get_connection(p->conn)), p);
        used = effect_do(effect, who, &ident, p->was_aware, 0, NULL, 0, 0, NULL);

        /* Quit if the item wasn't used and no knowledge was gained */
        if (!used && (p->was_aware || !ident)) return;
    }

    if (ident) object_notice_effect(p, obj);

    if (ident && !p->was_aware)
        object_learn_on_use(p, obj);
    else if (used)
        object_flavor_tried(p, obj);
}


void eat_fud(struct player *p, struct player *q, bool* obvious)
{
    int tries;

    /* Steal some food */
    for (tries = 0; tries < 10; tries++)
    {
        /* Pick an item from the pack */
        int index = randint0(z_info->pack_size);

        struct object *obj, *eaten;
        char o_name[NORMAL_WID];
        bool none_left = false;

        /* Get the item */
        obj = p->upkeep->inven[index];

        /* Skip non-objects */
        if (obj == NULL) continue;

        /* Only eat food */
        if (!tval_is_edible(obj)) continue;

        if (obj->number == 1)
        {
            object_desc(p, o_name, sizeof(o_name), obj, ODESC_BASE);
            msg(p, "Your %s (%c) was eaten!", o_name, gear_to_label(p, obj));
        }
        else
        {
            object_desc(p, o_name, sizeof(o_name), obj, ODESC_PREFIX | ODESC_BASE);
            msg(p, "One of your %s (%c) was eaten!", o_name, gear_to_label(p, obj));
        }

        /* PWMAngband: feed the offending player in PvP! */
        if (q) use_fud(q, obj);

        /* Steal and eat */
        eaten = gear_object_for_use(p, obj, 1, false, &none_left);
        object_delete(&eaten);

        /* Obvious */
        *obvious = true;

        /* Done */
        break;
    }
}


void drain_xp(struct player *p, int amt)
{
    int chance = 100 - (amt / 2) - (amt / 40) * 5;

    if (player_of_has(p, OF_HOLD_LIFE) && magik(chance))
        msg(p, "You keep hold of your life force!");
    else
    {
        int32_t d = damroll(amt, 6) + (p->exp / 100) * z_info->life_drain_percent;
        if (player_of_has(p, OF_HOLD_LIFE))
        {
            msg(p, "You feel your life slipping away!");
            player_exp_lose(p, d / 10, false);
        }
        else
        {
            msg(p, "You feel your life draining away!");
            player_exp_lose(p, d, false);
        }
    }
}


// weapon disarm
void drop_weapon(struct player *p, int damage)
{
    int tmp;
    struct object *obj;

    /* This effect is *very* nasty - it should be very rare */
    /* So give a chance to avoid it to everyone */
    if (magik(50)) return;

    /* A high DEX will help a lot */
    tmp = adj_dex_safe[p->state.stat_ind[STAT_DEX]];
    if (tmp > 95) tmp = 95;
    if (magik(tmp)) return;

    /* Access the weapon */
    obj = equipped_item_by_slot_name(p, "weapon");

    /* No weapon used - bleeding effect instead */
    if (!obj)
    {
        player_inc_timed(p, TMD_CUT, 100, true, true);
        return;
    }

    /* We are immune anyway */
    if (p->timed[TMD_HOLD_WEAPON]) return;

    /* Artifacts are safe */
    if (obj->artifact && magik(90)) return;

    /* Stuck weapons can't be removed */
    if (!obj_can_takeoff(obj)) return;

    /* Two-handed weapons are safe */
    if (kf_has(obj->kind->kind_flags, KF_TWO_HANDED) && magik(90)) return;

    /* Give an extra chance for comfortable weapons */
    if (magik(50) && !(p->state.heavy_wield || p->state.cumber_shield)) return;

    /* Finally give an extra chance for weak blows */
    if (!magik(damage)) return;

    /* Really unlucky or really lousy fighters get disarmed */
    msgt(p, MSG_DISARM_WEAPON, "Disarmed! You lose grip of your weapon!");
    if (!inven_drop(p, obj, 1, true))
    {
        /* Protect true artifacts at shallow depths */
        msg(p, "You manage to catch your weapon before it falls to the ground.");
    }
    p->upkeep->update |= (PU_BONUS);
}


/*
 * Check for hostility (player vs target).
 */
static bool pvx_check(struct player *p, struct source *who, uint16_t feat)
{
    /* Player here */
    if (who->player)
    {
        int mode = (target_equals(p, who)? PVP_DIRECT: PVP_INDIRECT);

        return pvp_check(p, who->player, mode, true, feat);
    }

    /* Monster here */
    if (who->monster) return pvm_check(p, who->monster);

    /* Nothing here */
    return false;
}


typedef struct delayed_ranged_effects
{
    struct monster *mon;
    int dmg;
    bool fear;
    bool poison;
    bool stun;
    bool cut;
    bool conf;
    struct delayed_ranged_effects *next;
} ranged_effects;


/*
 * This is a helper function to manage a linked list of delayed range effects.
 */
static ranged_effects *get_delayed_ranged_effects(ranged_effects **effects, struct monster *mon)
{
    ranged_effects *current = *effects;

    /* Walk through the list to get the corresponding monster */
    while (current)
    {
        /* Found a match */
        if (current->mon == mon) return current;

        current = current->next;
    }

    /* No match: create, assign and return */
    current = mem_zalloc(sizeof(ranged_effects));
    current->mon = mon;
    current->next = *effects;
    *effects = current;
    return current;
}


/*
 * Wipes a dead monster from the linked list of delayed range effects.
 */
static void wipe_delayed_ranged_effects(ranged_effects **effects, struct monster *mon)
{
    ranged_effects *current = *effects, *next;

    /* Empty list */
    if (!current) return;

    /* First element */
    if ((*effects)->mon == mon)
    {
        /* Wipe the dead monster */
        next = (*effects)->next;
        mem_free(*effects);
        *effects = next;
        return;
    }

    /* Walk through the list to get the corresponding monster */
    while (current)
    {
        next = current->next;

        /* End of list */
        if (!next) return;

        /* Found a match */
        if (next->mon == mon)
        {
            /* Wipe the dead monster */
            current->next = next->next;
            mem_free(next);
            return;
        }

        current = next;
    }
}


/*
 * Find the attr/char pair to use for a missile.
 *
 * It is moving (or has moved) from start to end.
 */
static void missile_pict(struct player *p, const struct object *obj, struct loc *start,
    struct loc *end, uint8_t *a, char *c)
{
    /* Get a nice missile picture for arrows and bolts */
    if (tval_is_arrow(obj))
        bolt_pict(p, start, end, PROJ_ARROW, a, c);
    else if (tval_is_bolt(obj))
        bolt_pict(p, start, end, PROJ_BOLT, a, c);
    else
    {
        /* Default to object picture */
        *a = object_attr(p, obj);
        *c = object_char(p, obj);
    }
}


/*
 * Ranged attacks
 */


/* Shooting hit types */
static const struct hit_types ranged_hit_types[] =
{
    {MSG_SHOOT_MISS, NULL},
    {MSG_SHOOT, NULL},
    {MSG_SHOOT_HIT, NULL},
    {MSG_SHOOT_BOW, NULL},
    {MSG_SHOOT_CROSSBOW, NULL},
    {MSG_SHOOT_SLING, NULL},
    {MSG_SHOOT_BOOMERANG, NULL},
    {MSG_SHOOT_GOOD, "It was a good hit!"},
    {MSG_SHOOT_GREAT, "It was a great hit!"},
    {MSG_SHOOT_SUPERB, "It was a superb hit!"}
};


/*
 * This is a helper function used by do_cmd_throw and do_cmd_fire.
 *
 * It abstracts out the projectile path, display code, identify and clean up
 * logic, while using the 'attack' parameter to do work particular to each
 * kind of attack.
 */
 // bool shooted - true if projectile was shooted; false if it was thrown
static bool ranged_helper(struct player *p, struct object *obj, int dir, int range, int num_shots,
    ranged_attack attack, const struct hit_types *hit_types, int num_types, bool magic, bool pierce,
    bool ranged_effect, bool shooted)
{
    int i, j;
    int path_n;
    struct loc path_g[256];
    struct loc grid, target;
    bool hit_target = false;
    struct object *missile;
    int shots = 0;
    bool more = true;
    ranged_effects *effects = NULL, *current;
    struct chunk *c = chunk_get(&p->wpos);

    /* Start at the player */
    loc_copy(&grid, &p->grid);

    /* Predict the "target" location */
    loc_init(&target, grid.x + 99 * ddx[dir], grid.y + 99 * ddy[dir]);

    /* Check for target validity */
    if ((dir == DIR_TARGET) && target_okay(p))
    {
        int taim;

        target_get(p, &target);

        /* Check distance */
        taim = distance(&grid, &target);

        // make cobbles have smaller distance (than throwing stones)
        if (obj->tval == TV_COBBLE)
            range /= 2;

        if (taim > range)
        {
            msg(p, "Target out of range by %d squares.", taim - range);
            return false;
        }
    }

    /* Shooting sound */
    if (shooted)
    {
        if (obj->tval == TV_ARROW)
            sound(p, MSG_SHOOT_BOW);
        else if (obj->tval == TV_BOLT)
            sound(p, MSG_SHOOT_CROSSBOW);
        else if (obj->tval == TV_SHOT)
            sound(p, MSG_SHOOT_SLING);
        else
            sound(p, MSG_THROWN);
    }
    else
    {
        if (obj->sval == lookup_sval(obj->tval, "Boomerang"))
            sound(p, MSG_SHOOT_BOOMERANG);
        else if (obj->tval == TV_COBBLE)
            sound(p, MSG_SHOOT); // stone sound
        else
            sound(p, MSG_THROWN);
    }

    /* Take a turn */
    use_energy(p);

    /* Attack once for each legal shot */
    while ((shots < num_shots) && more)
    {
        struct loc ball;
        struct source who_body;
        struct source *who = &who_body;
        bool none_left = false;

        more = false;

        loc_init(&ball, -1, -1);

        /* Start at the player */
        loc_copy(&grid, &p->grid);

        /* Calculate the path */
        path_n = project_path(NULL, c, path_g, range, &grid, &target, (pierce? PROJECT_THRU: 0));

        /* Handle stuff */
        handle_stuff(p);

        /* Project along the path */
        for (i = 0; i < path_n; ++i)
        {
            struct missile data;

            /* Disable throwing through open house door */
            if (square_home_isopendoor(c, &path_g[i])) break;
            
            /* Hack -- disable throwing through house window */
            if (square_is_window(c, &path_g[i])) break;            

            /* Stop before hitting walls */
            if (!square_ispassable(c, &path_g[i]) && !square_isprojectable(c, &path_g[i]) &&
                !(square_istree(c, &path_g[i]) && !one_in_(4)))
            {
                /* Special case: potion VS house door */
                if (tval_is_potion(obj) && square_home_iscloseddoor(c, &path_g[i]))
                {
                    /* Break it */
                    hit_target = true;

                    /* Find suitable color */
                    colorize_door(p, obj->kind, c, &path_g[i]);
                }

                /* Done */
                break;
            }

            /* Get missile picture */
            missile_pict(p, obj, &grid, &path_g[i], &data.mattr, &data.mchar);

            /* Advance */
            loc_copy(&grid, &path_g[i]);

            /* Tell the UI to display the missile */
            loc_copy(&data.grid, &path_g[i]);
            display_missile(c, &data);

            /* Don't allow if not hostile */
            square_actor(c, &grid, who);
            if (!pvx_check(p, who, square(c, &grid)->feat))
            {
                /* Allow curative potions to heal non hostile targets */
                if (tval_is_potion(obj))
                {
                    struct effect *e = object_effect(obj);

                    while (e)
                    {
                        if (e->index == EF_HEAL_HP_ONCE)
                        {
                            random_value v;
                            char dice[5];

                            dice_random_value(e->dice, NULL, &v);
                            strnfmt(dice, sizeof(dice), "%d", v.base);

                            if (who->monster)
                            {
                                who->player = p;
                                effect_simple(EF_MON_HEAL_HP, who, dice, 0, 0, 0, 0, 0, NULL);
                            }
                            else if (who->player)
                                effect_simple(EF_HEAL_HP_ONCE, who, dice, 0, 0, 0, 0, 0, NULL);
                            break;
                        }
                        e = e->next;
                    }
                }

                memset(who, 0, sizeof(struct source));
            }

            /* Try the attack on the target at (x, y) if any */
            if (!source_null(who))
            {
                bool visible;
                bool fear = false;
                char m_name[NORMAL_WID];
                int note_dies = MON_MSG_DIE;
                struct attack_result result = attack(p, obj, &grid);
                int dmg = result.dmg, reduced;
                uint32_t msg_type = result.msg_type;
                const char *verb = result.verb;

                /* Target info */
                if (who->monster)
                {
                    visible = monster_is_obvious(p, who->idx, who->monster);
                    monster_desc(p, m_name, sizeof(m_name), who->monster, MDESC_OBJE);

                    /////////////////////////////////////////////
                    // < REFLECT: some monster can reflect (eg Angels)
                    if (rf_has(who->monster->race->flags, RF_METAL))
                    {
                        bool reflects;
                        
                        if (streq(who->monster->race->name, "The Stormbringer"))
                            reflects = (RNG % 4) != 3;  // 75%
                        else // 50% chance for other metal monsters
                            reflects = (RNG % 2);
                        
                        if (reflects)
                        {
                            if (visible)
                                msgt(p, MSG_RESIST_A_LOT, "The %s reflects your attack!", m_name);
                            else
                                msgt(p, MSG_RESIST_A_LOT, "Your attack is reflected!");
                            more = true;
                            continue;
                        }
                    }
                    // REFLECT >
                    ////////////////////////////////////////////

                    if (monster_is_destroyed(who->monster->race))
                        note_dies = MON_MSG_DESTROYED;
                }
                else
                {
                    visible = (player_is_visible(p, who->idx) && !who->player->k_idx);
                    my_strcpy(m_name, who->player->name, sizeof(m_name));
                }

                if (result.success)
                {
                    char o_name[NORMAL_WID];
                    bool dead = false;

                    hit_target = true;

                    missile_learn_on_ranged_attack(p, obj);

                    /* Learn by use for other equipped items */
                    equip_learn_on_ranged_attack(p);

                    /* Describe the object */
                    object_desc(p, o_name, sizeof(o_name), obj, ODESC_FULL | ODESC_SINGULAR);

                    /* No negative damage; change verb if no damage done */
                    if (dmg <= 0)
                    {
                        dmg = 0;
                        msg_type = MSG_SHOOT_MISS;
                        verb = "fails to harm";
                    }

                    reduced = dmg;

                    /* Message */
                    if (who->player)
                    {
                        char killer_name[NORMAL_WID];

                        reduced = player_apply_damage_reduction(who->player, dmg, false, "strange ranged attack");
                        if (!reduced)
                        {
                            msg_type = MSG_MISS;
                            verb = "fails to harm";
                        }

                        /* Killer name */
                        player_desc(who->player, killer_name, sizeof(killer_name), p, true);

                        if (!reduced)
                            msg(who->player, "%s throws a %s at you.", killer_name, o_name);
                        else if (OPT(who->player, show_damage))
                        {
                            msg(who->player, "%s hits you with a %s for $r%d^r damage.",
                                killer_name, o_name, reduced);
                        }
                        else
                            msg(who->player, "%s hits you with a %s.", killer_name, o_name);
                    }

                    if (!visible)
                    {
                        /* Invisible monster/player */
                        msgt(p, MSG_SHOOT_HIT, "The %s finds a mark.", o_name);
                    }
                    else
                    {
                        int type;

                        /* Handle visible monster/player */
                        for (type = 0; type < num_types; type++)
                        {
                            const char *dmg_text = "";

                            if (msg_type != hit_types[type].msg_type) continue;
                            if (reduced && OPT(p, show_damage))
                                dmg_text = format(" for $g%d^g damage", reduced);
                            if (hit_types[type].text)
                            {
                                msgt(p, msg_type, "Your %s %s %s%s. %s", o_name, verb, m_name,
                                    dmg_text, hit_types[type].text);
                            }
                            else
                            {
                                msgt(p, msg_type, "Your %s %s %s%s.", o_name, verb, m_name,
                                    dmg_text);
                            }
                        }

                        /* Track this target */
                        if (who->monster) monster_race_track(p->upkeep, who);
                        health_track(p->upkeep, who);
                    }

                    /* Hit the target, check for death */
                    if (who->monster)
                    {
                        /* Hit the monster, check for death */
                        dead = mon_take_hit(p, c, who->monster, dmg, &fear, note_dies);
                    }
                    else
                    {
                        char df[160];

                        strnfmt(df, sizeof(df), "was shot to death with a %s by %s", o_name,
                            p->name);

                        /* Hit the player, check for death */
                        dead = take_hit(who->player, reduced, p->name, df);
                    }

                    /* Message */
                    if (!dead)
                    {
                        if (who->monster)
                        {
                            current = get_delayed_ranged_effects(&effects, who->monster);
                            current->dmg += dmg;
                        }
                        else
                            player_pain(p, who->player, dmg);
                    }
                    else if (who->monster)
                        wipe_delayed_ranged_effects(&effects, who->monster);

                    /* Apply poison */
                    if (result.effects.do_poison && !dead)
                    {
                        if (who->player)
                        {
                            player_inc_timed(who->player, TMD_POISONED, randint1(p->lev) + 5, true,
                                true);
                        }
                        else if (mon_inc_timed(p, who->monster, MON_TMD_POIS, 5 + randint1(5),
                            MON_TMD_FLG_NOMESSAGE))
                        {
                            current = get_delayed_ranged_effects(&effects, who->monster);
                            current->poison = true;
                        }
                    }

                    /* Apply stun */
                    if (result.effects.do_stun && !dead)
                    {
                        if (who->player)
                        {
                            player_inc_timed(who->player, TMD_STUN, randint1(p->lev) + 5, true,
                                true);
                        }
                        else if (mon_inc_timed(p, who->monster, MON_TMD_STUN, 5 + randint1(5),
                            MON_TMD_FLG_NOMESSAGE))
                        {
                            current = get_delayed_ranged_effects(&effects, who->monster);
                            current->stun = true;
                        }
                    }

                    /* Apply cut */
                    if (result.effects.do_cut && !dead)
                    {
                        if (who->player)
                        {
                            player_inc_timed(who->player, TMD_CUT, randint1(p->lev) + 5, true,
                                true);
                        }
                        else if (mon_inc_timed(p, who->monster, MON_TMD_CUT, 5 + randint1(5),
                            MON_TMD_FLG_NOMESSAGE))
                        {
                            current = get_delayed_ranged_effects(&effects, who->monster);
                            current->cut = true;
                        }
                    }

                    /* Apply archer confusion brand */
                    if (ranged_effect && has_bowbrand(p, PROJ_MON_CONF, false) && !dead)
                    {
                        if (who->player)
                        {
                            player_inc_timed(who->player, TMD_CONFUSED,
                                3 + randint1(10 + randint0(p->lev) / 10), true, true);
                        }
                        else if (mon_inc_timed(p, who->monster, MON_TMD_CONF, 5 + randint1(5),
                            MON_TMD_FLG_NOMESSAGE))
                        {
                            current = get_delayed_ranged_effects(&effects, who->monster);
                            current->conf = true;
                        }
                    }

                    /* Add a nice ball if needed */
                    if (ranged_effect && p->timed[TMD_BOWBRAND] && p->brand.blast)
                        loc_copy(&ball, &grid);

                    /* Take note */
                    if (!dead && fear)
                    {
                        current = get_delayed_ranged_effects(&effects, who->monster);
                        current->fear = true;
                    }

                    if (!dead) more = true;
                }
                else
                {
                    if (visible)
                    {
                        char o_name[NORMAL_WID];

                        /* Handle visible monster/player */
                        object_desc(p, o_name, sizeof(o_name), obj, ODESC_FULL | ODESC_SINGULAR);
                        msgt(p, MSG_SHOOT_MISS, "The %s misses %s.", o_name, m_name);

                        /* Track this target */
                        if (who->monster) monster_race_track(p->upkeep, who);
                        health_track(p->upkeep, who);
                    }

                    more = true;
                }

                /* Stop the missile */
                if (!pierce) break;
            }

            /* Stop if non-projectable but passable */
            if (!square_isprojectable(c, &path_g[i])) break;
        }

        /* Ball effect */
        if ((ball.y >= 0) && (ball.x >= 0))
        {
            int p_flag = PROJECT_JUMP | PROJECT_GRID | PROJECT_ITEM | PROJECT_KILL | PROJECT_PLAY;
            struct source act_body;
            struct source *p_act = &act_body;

            source_player(p_act, get_player_index(get_connection(p->conn)), p);

            p->current_sound = -2;
            project(p_act, 2, c, &ball, p->brand.dam, p->brand.type, p_flag, 0, 0, "killed");
            p->current_sound = -1;
        }

        /* Drop (or break) near that location */
        if (!magic)
        {
            /* Get the missile */
            if (object_is_carried(p, obj))
                missile = gear_object_for_use(p, obj, 1, true, &none_left);
            else
                missile = floor_object_for_use(p, c, obj, 1, true, &none_left);

            /* Chance of breakage (during attacks) */
            j = breakage_chance(missile, hit_target);

            // 5% to break projectile in zeitnot till lvl 15
            // (non-nced ammo only)
            if (j > 5 && p->lev < 15 &&
                obj->to_h == 0 && obj->to_d == 0 &&
                tval_is_ammo(obj) && OPT(p, birth_zeitnot))
                    j = 5;

            /* Handle the newbies_cannot_drop option */
            if (newbies_cannot_drop(p)) j = 100;

            /* Drop (or break) near that location */
            drop_near(p, c, &missile, j, &grid, true, DROP_FADE, false);
        }

        shots++;
    }

    /* Delay messages */
    while (effects)
    {
        /* Paranoia: only process living monsters */
        /* This is necessary to take into account monsters killed by ball effects */
        if (effects->mon->race && monster_is_obvious(p, effects->mon->midx, effects->mon))
        {
            if (effects->dmg) message_pain(p, effects->mon, effects->dmg);
            if (effects->poison) add_monster_message(p, effects->mon, MON_MSG_POISONED, true);
            if (effects->cut) add_monster_message(p, effects->mon, MON_MSG_BLEED, true);
            if (effects->stun) add_monster_message(p, effects->mon, MON_MSG_DAZED, true);
            if (effects->conf) add_monster_message(p, effects->mon, MON_MSG_CONFUSED, true);
            if (effects->fear) add_monster_message(p, effects->mon, MON_MSG_FLEE_IN_TERROR, true);
        }

        current = effects->next;
        mem_free(effects);
        effects = current;
    }

    return more;
}


/*
 * Helper function used with ranged_helper by do_cmd_fire.
 */
static struct attack_result make_ranged_shot(struct player *p, struct object *ammo, struct loc *grid)
{
    struct attack_result result;
    struct object *bow = equipped_item_by_slot_name(p, "shooting");
    int multiplier = (bow? p->state.ammo_mult: 1);
    int best_mult = 1;
    struct chunk *c = chunk_get(&p->wpos);
    struct source target_body;
    struct source *target = &target_body;
    bool visible;
    int ac;

    memset(&result, 0, sizeof(result));
    my_strcpy(result.verb, "hits", sizeof(result.verb));

    /* Target info */
    square_actor(c, grid, target);
    if (target->monster)
    {
        visible = monster_is_obvious(p, target->idx, target->monster);
        ac = target->monster->ac;
    }
    else
    {
        visible = (player_is_visible(p, target->idx) && !target->player->k_idx);
        ac = target->player->state.ac + target->player->state.to_a;
    }

    /* Did we hit it */
    if (!test_hit(chance_of_missile_hit(p, ammo, bow, grid, visible), ac)) return result;

    result.success = true;

    player_attack_modifier(p, target, &best_mult, &result.effects, result.verb,
        sizeof(result.verb), true, true);
    improve_attack_modifier(p, ammo, target, &best_mult, &result.effects, result.verb,
        sizeof(result.verb), true);
    if (bow)
    {
        improve_attack_modifier(p, bow, target, &best_mult, &result.effects, result.verb,
            sizeof(result.verb), true);
    }

    result.dmg = ranged_damage(p, ammo, bow, best_mult, multiplier);
    result.dmg = critical_shot(p, target, ammo->weight, object_to_hit(ammo), result.dmg, true,
        &result.msg_type);

    missile_learn_on_ranged_attack(p, bow);

    return result;
}


/*
 * Helper function used with ranged_helper by do_cmd_throw.
 */
static struct attack_result make_ranged_throw(struct player *p, struct object *obj, struct loc *grid)
{
    struct attack_result result;
    int multiplier = 1;
    int best_mult = 1;
    struct chunk *c = chunk_get(&p->wpos);
    struct source target_body;
    struct source *target = &target_body;
    bool visible;
    int ac;

    memset(&result, 0, sizeof(result));
    my_strcpy(result.verb, "hits", sizeof(result.verb));

    /* Target info */
    square_actor(c, grid, target);
    if (target->monster)
    {
        visible = monster_is_obvious(p, target->idx, target->monster);
        ac = target->monster->ac;
    }
    else
    {
        visible = (player_is_visible(p, target->idx) && !target->player->k_idx);
        ac = target->player->state.ac + target->player->state.to_a;
    }

    /* If we missed then we're done */
    if (!test_hit(chance_of_missile_hit(p, obj, NULL, grid, visible), ac)) return result;

    result.success = true;

    player_attack_modifier(p, target, &best_mult, &result.effects, result.verb,
        sizeof(result.verb), true, tval_is_ammo(obj));
    improve_attack_modifier(p, obj, target, &best_mult, &result.effects, result.verb,
        sizeof(result.verb), true);

    result.dmg = ranged_damage(p, obj, NULL, best_mult, multiplier);
    result.dmg = critical_shot(p, target, obj->weight, object_to_hit(obj), result.dmg, false,
        &result.msg_type);

    /* Direct adjustment for exploding things (flasks of oil) */
    if (of_has(obj->flags, OF_EXPLODE)) result.dmg *= 3;

    /* Direct adjustment for potions of poison */
    if (tval_is_potion(obj) && streq(obj->kind->name, "Poison"))
    {
        result.dmg *= 3;
        result.effects.do_poison = true;
    }

    return result;
}


/*
 * Fire an object from the quiver, pack or floor at a target.
 */
bool do_cmd_fire(struct player *p, int dir, int item)
{
    int range = MIN(6 + 2 * p->state.ammo_mult, z_info->max_range);
    int num_shots;
    ranged_attack attack = make_ranged_shot;
    struct object *obj = object_from_index(p, item, true, true);
    bool magic, pierce, more;

    /* Paranoia: requires an item */
    if (!obj) return false;

    /* Restrict ghosts */
    if ((p->ghost && !(p->dm_flags & DM_GHOST_HANDS)) || p->timed[TMD_WRAITHFORM])
    {
        msg(p, "You cannot fire missiles!");
        return false;
    }

    /* Check preventive inscription '^f' */
    if (check_prevent_inscription(p, INSCRIPTION_FIRE))
    {
        msg(p, "The item's inscription prevents it.");
        return false;
    }

    /* Make sure the player isn't firing wielded items */
    if (object_is_equipped(p->body, obj))
    {
        msg(p, "You cannot fire wielded items.");
        return false;
    }

    /* Restricted by choice */
    if (!object_is_carried(p, obj) && !is_owner(p, obj))
    {
        msg(p, "This item belongs to someone else!");
        return false;
    }

    /* Must meet level requirement */
    if (!object_is_carried(p, obj) && !has_level_req(p, obj))
    {
        msg(p, "You don't have the required level!");
        return false;
    }

    /* Paranoia: requires a proper missile */
    if (obj->tval != p->state.ammo_tval) return false;

    /* Check preventive inscription '!f' */
    if (object_prevent_inscription(p, obj, INSCRIPTION_FIRE, false))
    {
        msg(p, "The item's inscription prevents it.");
        return false;
    }

    /* Restrict artifacts */
    if (obj->artifact && newbies_cannot_drop(p))
    {
        msg(p, "You cannot fire that!");
        return false;
    }

    /* Never in wrong house */
    if (!check_store_drop(p))
    {
        msg(p, "You cannot fire this here.");
        return false;
    }

    /* Ensure "dir" is in ddx/ddy array bounds */
    if (!VALID_DIR(dir)) return false;

    /* Apply confusion */
    player_confuse_dir(p, &dir);

    /* Only fire in direction 5 if we have a target */
    if ((dir == DIR_TARGET) && !target_okay(p)) return false;

    magic = of_has(obj->flags, OF_AMMO_MAGIC);
    pierce = has_bowbrand(p, PROJ_ARROW, false);

    /* Temporary "Farsight" */
    if (p->timed[TMD_FARSIGHT]) range += (p->lev - 7) / 10;

    /* Calculate number of shots */
    num_shots = (p->state.num_shots + p->frac_shot) / 10;

    /* Check if we have enough missiles */
    if (!magic && (num_shots > obj->number)) num_shots = obj->number;

    /* Calculate remainder */
    p->frac_shot = (p->state.num_shots + p->frac_shot) % 10;

    /* Take shots until energy runs out or monster dies */
    more = ranged_helper(p, obj, dir, range, num_shots, attack, ranged_hit_types,
        (int)N_ELEMENTS(ranged_hit_types), magic, pierce, true, true);

    return more;
}


/*
 * Throw an object from the quiver, pack or floor, or, in limited circumstances,
 * the equipment.
 */
bool do_cmd_throw(struct player *p, int dir, int item)
{
    int num_shots = 1;
    int str = adj_str_blow[p->state.stat_ind[STAT_STR]];
    ranged_attack attack = make_ranged_throw;
    int weight;
    int range;
    struct object *obj = object_from_index(p, item, true, true);
    bool magic = false, more;

    /* Paranoia: requires an item */
    if (!obj) return false;

    /* Restrict ghosts */
    if ((p->ghost && !(p->dm_flags & DM_GHOST_HANDS)) || p->timed[TMD_WRAITHFORM])
    {
        msg(p, "You cannot throw items!");
        return false;
    }

    /* Check preventive inscription '^v' */
    if (check_prevent_inscription(p, INSCRIPTION_THROW))
    {
        msg(p, "The item's inscription prevents it.");
        return false;
    }

    /* Restricted by choice */
    if (!object_is_carried(p, obj) && !is_owner(p, obj))
    {
        msg(p, "This item belongs to someone else!");
        return false;
    }

    /* Must meet level requirement */
    if (!object_is_carried(p, obj) && !has_level_req(p, obj))
    {
        msg(p, "You don't have the required level!");
        return false;
    }

    /* Check preventive inscription '!v' */
    if (object_prevent_inscription(p, obj, INSCRIPTION_THROW, false))
    {
        msg(p, "The item's inscription prevents it.");
        return false;
    }

    if (tval_is_ammo(obj)) magic = of_has(obj->flags, OF_AMMO_MAGIC);

    /* Restrict artifacts */
    if (obj->artifact && (!magic || newbies_cannot_drop(p)))
    {
        msg(p, "You cannot throw that!");
        return false;
    }

    // Can't throw cursed, NO_DROP and soulbound items;
    // with exception of EXPLODE object flag for Alchemist class potions
    if ((obj->curses || obj->soulbound || of_has(obj->flags, OF_NO_DROP)) && !of_has(obj->flags, OF_EXPLODE))
    {
        msg(p, "You cannot throw this.");
        return false;
    }

    /* Never in wrong house */
    if (!check_store_drop(p))
    {
        msg(p, "You cannot throw this here.");
        return false;
    }

    if (object_is_equipped(p->body, obj))
    {
        /* Restrict which equipment can be thrown */
        if (obj_can_takeoff(obj) && tval_is_melee_weapon(obj)) inven_takeoff(p, obj);
        else return false;
    }

    weight = MAX(obj->weight, 10);
    range = MIN(((str + 20) * 10) / weight, 10);

    /* Apply confusion */
    player_confuse_dir(p, &dir);

    /* Take shots until energy runs out or monster dies */
    more = ranged_helper(p, obj, dir, range, num_shots, attack, ranged_hit_types,
        (int)N_ELEMENTS(ranged_hit_types), magic, false, false, false);

    return more;
}


/*
 * Fire at nearest target
 */
bool do_cmd_fire_at_nearest(struct player *p)
{
    /* The direction '5' means 'use the target' */
    int i, dir = DIR_TARGET;
    struct object *ammo = NULL;
    struct object *bow = equipped_item_by_slot_name(p, "shooting");
    bool result;

    /* Check energy */
    if (!has_energy(p, true)) return false;

    /* Find first eligible ammo in the quiver */
    for (i = 0; i < z_info->quiver_size; i++)
    {
        if (!p->upkeep->quiver[i]) continue;
        if (bow)
        {
            if (p->upkeep->quiver[i]->tval != p->state.ammo_tval) continue;
        }
        else
        {
            if (!tval_is_ammo(p->upkeep->quiver[i]) ||
                !of_has(p->upkeep->quiver[i]->flags, OF_THROWING))
            {
                continue;
            }
        }
        ammo = p->upkeep->quiver[i];
        break;
    }

    /* Require usable ammo */
    if (!ammo)
    {
        msg(p, "You have no ammunition in the quiver to fire.");

        /* Cancel repeat */
        disturb(p, 1);
        return true;
    }

    /* Require foe */
    if (!target_set_closest(p, TARGET_KILL | TARGET_QUIET))
    {
        /* Cancel repeat */
        disturb(p, 1);
        return true;
    }

    /* Fire! */
    if (bow) result = do_cmd_fire(p, dir, ammo->oidx);
    else result = do_cmd_throw(p, dir, ammo->oidx);
    if (!result)
    {
        /* Cancel repeat */
        disturb(p, 1);
        return true;
    }

    /* Repeat */
    if (p->firing_request) cmd_fire_at_nearest(p);
    return true;
}
