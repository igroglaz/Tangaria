/*
 * File: mon-attack.c
 * Purpose: Monster attacks
 *
 * Copyright (c) 1997 Ben Harrison
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
 * This file deals with monster attacks (including spells) as follows:
 *
 * Give monsters more intelligent attack/spell selection based on
 * observations of previous attacks on the player, and/or by allowing
 * the monster to "cheat" and know the player status.
 *
 * Maintain an idea of the player status, and use that information
 * to occasionally eliminate "ineffective" spell attacks.  We could
 * also eliminate ineffective normal attacks, but there is no reason
 * for the monster to do this, since he gains no benefit.
 * Note that MINDLESS monsters are not allowed to use this code.
 * And non-INTELLIGENT monsters only use it partially effectively.
 *
 * Actually learn what the player resists, and use that information
 * to remove attacks or spells before using them.
 */


//// Send "slash effect" to the client ////
void slash_fx(struct monster *mon, struct source *who)
{
    int mon_x, mon_y;
    int who_x, who_y;
    int dx, dy;
    int dir;

    const int dirrrs[3][3] = {
        { 9, 8, 7 },
        { 6, 5, 4 },
        { 3, 2, 1 },
    };

    bool seen = ((who->player->timed[TMD_BLIND] == 0) &&
        monster_is_visible(who->player, mon->midx));

    // Check the monster is visible to the player
    if (!seen) return;

    mon_x = mon->grid.x;
    mon_y = mon->grid.y;

    if (who->monster)
    {
        who_x = who->monster->grid.x;
        who_y = who->monster->grid.y;
    }
    else
    {
        who_x = who->player->grid.x;
        who_y = who->player->grid.y;
    }

    // Determine direction
    dx = mon_x - who_x;
    dy = mon_y - who_y;
    if (dx < -1) dx = -1;
    if (dx > 1) dx = 1;
    if (dy < -1) dy = -1;
    if (dy > 1) dy = 1;

    // dir
    // |1|2|3|
    // |4|5|6|
    // |7|8|9|
    dir = dirrrs[dy + 1][dx + 1];

    // Send it !
    Send_slash_fx(who->player, mon_y, mon_x, dir);
}


/*
 * Given the monster, *mon, and cave *c, set *dist to the distance to the
 * monster's target and *grid to the target's location. Accounts for a player
 * decoy, if present. Either dist or grid may be NULL if that value is not
 * needed.
 */
static void monster_get_target_dist_grid(struct chunk *c, struct monster *mon,
    int target_m_dis, int *dist, struct loc *target, struct loc *grid)
{
    if (monster_is_decoyed(c, mon))
    {
        struct loc *decoy = cave_find_decoy(c);

        if (dist) *dist = distance(&mon->grid, decoy);
        if (grid) loc_copy(grid, decoy);
    }
    else
    {
        if (dist) *dist = target_m_dis;
        if (grid) loc_copy(grid, target);
    }
}


/*
 * Check if a monster has a chance of casting a spell this turn
 */
static bool monster_can_cast(struct player *p, struct chunk *c, struct monster *mon,
    int target_m_dis, struct loc *grid)
{
    int chance = mon->race->freq_spell;
    int tdist;
    struct loc tgrid;

    monster_get_target_dist_grid(c, mon, target_m_dis, &tdist, grid, &tgrid);

    /* Cannot cast spells when blind */
    if (mon->m_timed[MON_TMD_BLIND]) return false;

    /* Not allowed to cast spells */
    if (!chance) return false;

    /* Taunted monsters are likely just to attack */
    if (p->timed[TMD_TAUNT]) chance /= 2;

    /* Monsters at their preferred range are more likely to cast */
    if (tdist == mon->best_range) chance *= 2;

    /* Only do spells occasionally */
    if (!magik(chance)) return false;

    /* Check range */
    if (tdist > z_info->max_range) return false;

    /* Check path (destination could be standing on a wall) */
    if (!projectable(p, c, &mon->grid, &tgrid, PROJECT_SHORT, false)) return false;

    /* If the target isn't the player, only cast if the player can witness */
    if (!loc_eq(&p->grid, &tgrid) && !square_isview(p, &mon->grid) && !!square_isview(p, &tgrid))
    {
        struct loc *path = mem_alloc(z_info->max_range * sizeof(*path));
        int npath, ipath;

        npath = project_path(p, c, path, z_info->max_range, &mon->grid, &tgrid, PROJECT_SHORT);
        ipath = 0;
        while (1)
        {
            /* No point on path visible. Don't cast. */
            if (ipath >= npath)
            {
                mem_free(path);
                return false;
            }

            if (square_isview(p, &path[ipath])) break;
            ++ipath;
        }
        mem_free(path);
    }

    return true;
}


/*
 * Remove the "bad" spells from a spell list
 */
static void remove_bad_spells(struct player *p, struct chunk *c, struct monster *mon,
    bitflag f[RSF_SIZE])
{
    bitflag f2[RSF_SIZE];
    int tdist;

    /* MvM */
    if (!p) return;

    monster_get_target_dist_grid(c, mon, mon->cdis, &tdist, NULL, NULL);

    /* Take working copy of spell flags */
    rsf_copy(f2, f);

    /* Don't heal if full */
    if (mon->hp >= mon->maxhp) rsf_off(f2, RSF_HEAL);

    /* Don't heal others if no injuries */
    if (rsf_has(f2, RSF_HEAL_KIN) && !find_any_nearby_injured_kin(chunk_get(&p->wpos), mon))
        rsf_off(f2, RSF_HEAL_KIN);

    /* Don't haste if hasted with time remaining */
    if (mon->m_timed[MON_TMD_FAST] > 10) rsf_off(f2, RSF_HASTE);

    /* Don't teleport to if the player is already next to us */
    if (tdist == 1)
    {
        rsf_off(f2, RSF_TELE_TO);
        rsf_off(f2, RSF_TELE_SELF_TO);
    }

    /* Don't use the lash effect if the player is too far away */
    if (tdist > 2) rsf_off(f2, RSF_WHIP);
    if (tdist > 3) rsf_off(f2, RSF_SPIT);

    /* Update acquired knowledge */
    if (cfg_ai_learn)
    {
        size_t i;
        bitflag ai_flags[OF_SIZE], ai_pflags[PF_SIZE];
        struct element_info el[ELEM_MAX];
        bool know_something = false;

        /* Occasionally forget player status */
        if (one_in_(20))
        {
            of_wipe(mon->known_pstate.flags);
            pf_wipe(mon->known_pstate.pflags);
            for (i = 0; i < ELEM_MAX; i++)
                mon->known_pstate.el_info[i].res_level[0] = 0;
        }

        /* Use the memorized info */
        of_wipe(ai_flags);
        pf_wipe(ai_pflags);
        of_copy(ai_flags, mon->known_pstate.flags);
        pf_copy(ai_pflags, mon->known_pstate.pflags);
        if (!of_is_empty(ai_flags) || !pf_is_empty(ai_pflags)) know_something = true;

        memset(el, 0, ELEM_MAX * sizeof(struct element_info));
        for (i = 0; i < ELEM_MAX; i++)
        {
            el[i].res_level[0] = mon->known_pstate.el_info[i].res_level[0];
            if (el[i].res_level[0] != 0) know_something = true;
        }

        /* Cancel out certain flags based on knowledge */
        if (know_something) unset_spells(p, f2, ai_flags, ai_pflags, el, mon);
    }

    /* Use working copy of spell flags */
    rsf_copy(f, f2);
}


/*
 * Determine if there is a space near the selected spot in which
 * a summoned creature can appear
 */
static bool summon_possible(struct chunk *c, struct loc *grid)
{
    struct loc begin, end;
    struct loc_iterator iter;

    loc_init(&begin, grid->x - 2, grid->y - 2);
    loc_init(&end, grid->x + 2, grid->y + 2);
    loc_iterator_first(&iter, &begin, &end);

    /* Start at the location, and check 2 grids in each dir */
    do
    {
        /* Ignore illegal locations */
        if (!square_in_bounds(c, &iter.cur)) continue;

        /* Only check a circular area */
        if (distance(grid, &iter.cur) > 2) continue;

        /* No summon on glyph of warding */
        if (square_iswarded(c, &iter.cur)) continue;

        /* If it's empty floor grid in line of sight, we're good */
        if (square_isemptyfloor(c, &iter.cur) && los(c, grid, &iter.cur))
            return true;
    }
    while (loc_iterator_next(&iter));

    return false;
}


/*
 * Have a monster choose a spell to cast.
 *
 * Note that the monster's spell list has already had "useless" spells
 * (bolts that won't hit the player, summons without room, etc.) removed.
 * Perhaps that should be done by this function.
 *
 * Stupid monsters will just pick a spell randomly.  Smart monsters
 * will choose more "intelligently".
 *
 * This function could be an efficiency bottleneck.
 */
static int choose_attack_spell(bitflag *f, bool innate)
{
    int num = 0;
    uint8_t spells[RSF_MAX];
    int i;

    /* Paranoid initialization */
    for (i = 0; i < RSF_MAX; i++) spells[i] = 0;

    /* Extract spells, filtering as necessary */
    for (i = FLAG_START; i < RSF_MAX; i++)
    {
        if (!innate && mon_spell_is_innate(i)) continue;
        if (innate && !mon_spell_is_innate(i)) continue;
        if (rsf_has(f, i)) spells[num++] = i;
    }

    /* Pick at random */
    return (spells[randint0(num)]);
}


/*
 * Failure rate of a monster's spell, based on spell power and current status
 */
static int monster_spell_failrate(struct monster *mon, int thrown_spell)
{
    int power = MIN(mon->race->spell_power, 1);
    int failrate = 0;

    /* Stupid monsters will never fail (for jellies and such) */
    if (!monster_is_stupid(mon->race))
    {
        /* Base failrate */
        failrate = 25 - (power + 3) / 4;

        /* Fear adds 20% */
        if (mon->m_timed[MON_TMD_FEAR]) failrate += 20;

        /* Confusion and disenchantment add 50% */
        if (mon->m_timed[MON_TMD_CONF] || mon->m_timed[MON_TMD_DISEN]) failrate += 50;
    }

    if (failrate < 0) failrate = 0;

    /* Pets/slaves will be unlikely to summon */
    if (mon->master && is_spell_summon(thrown_spell)) failrate = 95;

    return failrate;
}


/*
 * Calculate the base to-hit value for a monster attack based on race only.
 * See also: chance_of_spell_hit_base
 *
 * race The monster race
 * effect The attack
 */
int chance_of_monster_hit_base(const struct monster_race *race, const struct blow_effect *effect)
{
    return MAX(race->level, 1) * 3 + effect->power;
}


/*
 * Calculate the to-hit value of a monster attack for a specific monster
 *
 * mon The monster
 * effect The attack
 */
static int chance_of_monster_hit(const struct monster *mon, const struct blow_effect *effect)
{
    int to_hit = chance_of_monster_hit_base(mon->race, effect);

    /* Apply stun hit reduction if applicable */
    if (mon->m_timed[MON_TMD_STUN])
        to_hit = to_hit * (100 - STUN_HIT_REDUCTION) / 100;

    return to_hit;
}


/*
 * Have a monster choose a spell to cast (remove all "useless" spells).
 */
static int get_thrown_spell(struct player *p, struct player *who, struct chunk *c,
    struct monster *mon, int target_m_dis, struct loc *grid)
{
    int thrown_spell, failrate;
    bitflag f[RSF_SIZE];
    bool innate;

    /* Check prerequisites */
    if (!monster_can_cast(p, c, mon, target_m_dis, grid)) return -1;

    /* Extract the racial spell flags */
    rsf_copy(f, mon->race->spell_flags);

    /* Smart monsters can use "desperate" spells */
    if (monster_is_smart(mon) && (mon->hp < mon->maxhp / 10) && magik(50))
        ignore_spells(f, RST_DAMAGE | RST_INNATE | RST_MISSILE);

    /* Non-stupid monsters do some filtering */
    if (!monster_is_stupid(mon->race))
    {
        struct loc tgrid;

        /* Remove the "ineffective" spells */
        remove_bad_spells(who, c, mon, f);

        /* Check for a clean bolt shot */
        monster_get_target_dist_grid(c, mon, 0, NULL, grid, &tgrid);
        if (test_spells(f, RST_BOLT) && !projectable(p, c, &mon->grid, &tgrid, PROJECT_STOP, false))
            ignore_spells(f, RST_BOLT);

        /* Check for a possible summon */
        if (!summon_possible(c, &mon->grid))
            ignore_spells(f, RST_SUMMON);
    }

    /* No spells left */
    if (rsf_is_empty(f)) return -1;

    /* Choose a spell to cast */
    innate = magik(mon->race->freq_innate);
    thrown_spell = choose_attack_spell(f, innate);

    /* Abort if no spell was chosen */
    if (!thrown_spell) return -1;

    /* Check for spell failure (innate attacks never fail) */
    failrate = monster_spell_failrate(mon, thrown_spell);
    if (!mon_spell_is_innate(thrown_spell) && magik(failrate))
    {
        char m_name[NORMAL_WID];

        /* Get the monster name (or "it") */
        monster_desc(p, m_name, sizeof(m_name), mon, MDESC_STANDARD);

        /* Message */
        msg(p, "%s tries to cast a spell, but fails.", m_name);

        return -2;
    }

    return thrown_spell;
}


/*
 * Creatures can cast spells, shoot missiles, and breathe.
 *
 * Returns "true" if a spell (or whatever) was (successfully) cast.
 *
 * Perhaps monsters should breathe at locations *near* the player,
 * since this would allow them to inflict "partial" damage.
 *
 * It will not be possible to "correctly" handle the case in which a
 * monster attempts to attack a location which is thought to contain
 * the player, but which in fact is nowhere near the player, since this
 * might induce all sorts of messages about the attack itself, and about
 * the effects of the attack, which the player might or might not be in
 * a position to observe.  Thus, for simplicity, it is probably best to
 * only allow "faulty" attacks by a monster if one of the important grids
 * (probably the initial or final grid) is in fact in view of the player.
 * It may be necessary to actually prevent spell attacks except when the
 * monster actually has line of sight to the player.  Note that a monster
 * could be left in a bizarre situation after the player ducked behind a
 * pillar and then teleported away, for example.
 *
 * Note that this function attempts to optimize the use of spells for the
 * cases in which the monster has no spells, or has spells but cannot use
 * them, or has spells but they will have no "useful" effect.  Note that
 * this function has been an efficiency bottleneck in the past.
 */
bool make_ranged_attack(struct source *who, struct chunk *c, struct monster *mon, int target_m_dis)
{
    struct monster_lore *lore = get_lore(who->player, mon->race);
    int thrown_spell;
    bool seen = ((who->player->timed[TMD_BLIND] == 0) &&
                 (who->player->timed[TMD_BLIND_REAL] == 0) &&
                  monster_is_visible(who->player, mon->midx));

    /* Stop if player is dead or gone */
    if (!who->player->alive || who->player->is_dead || who->player->upkeep->new_level_method)
        return false;

    /* Choose a spell to cast */
    thrown_spell = get_thrown_spell(who->player, (who->monster? NULL: who->player), c, mon,
        target_m_dis, (who->monster? &who->monster->grid: &who->player->grid));

    /* Abort if no spell was chosen */
    if (thrown_spell < 0) return ((thrown_spell == -1)? false: true);

    /* If we see a hidden monster try to cast a spell, become aware of it */
    if (monster_is_camouflaged(mon)) become_aware(who->player, c, mon);

    /* Cast the spell. */
    disturb(who->player, 0);
    do_mon_spell(who->player, c, who->monster, thrown_spell, mon, seen);

    /* Remember what the monster did */
    if (seen)
    {
        rsf_on(lore->spell_flags, thrown_spell);

        /* Innate spell */
        if (mon_spell_is_innate(thrown_spell))
        {
            if (lore->cast_innate < UCHAR_MAX) lore->cast_innate++;
        }

        /* Bolt or Ball, or Special spell */
        else
        {
            if (lore->cast_spell < UCHAR_MAX) lore->cast_spell++;
        }
    }

    /* Always take note of monsters that kill you */
    if (who->player->is_dead && (lore->pdeaths < SHRT_MAX))
        lore->pdeaths++;
    if (who->player->is_dead && (mon->race->lore.tdeaths < SHRT_MAX))
        mon->race->lore.tdeaths++;

    /* Record any new info */
    lore_update(mon->race, lore);

    // Slash effect
    slash_fx(mon, who);

    /* A spell was cast */
    return true;
}


/*
 * Critical blow.  All hits that do 95% of total possible damage,
 * and which also do at least 20 damage, or, sometimes, N damage.
 * This is used only to determine "cuts" and "stuns".
 */
static int monster_critical(random_value dice, int dam)
{
    int max = 0;
    int total = randcalc(dice, 0, MAXIMISE);

    /* Must do at least 95% of perfect */
    if (dam < total * 19 / 20) return (0);

    /* Weak blows rarely work */
    if ((dam < 20) && !magik(dam)) return (0);

    /* Perfect damage */
    if (dam == total) max++;

    /* Super-charge */
    if (dam >= 20)
    {
        while (magik(2)) max++;
    }

    /* Critical damage */
    if (dam > 45) return (6 + max);
    if (dam > 33) return (5 + max);
    if (dam > 25) return (4 + max);
    if (dam > 18) return (3 + max);
    if (dam > 11) return (2 + max);
    return (1 + max);
}


/*
 * Determine if an attack against the player succeeds.
 */
bool check_hit(struct player *p, int to_hit)
{
    /* If anything checks vs ac, the player learns ac bonuses */
    equip_learn_on_defend(p);

    /* Check if the target was hit */
    return test_hit(to_hit, p->state.ac + p->state.to_a);
}


/*
 * Calculate how much damage remains after armor is taken into account
 * (does for a physical attack what adjust_dam does for an elemental attack).
 */
int adjust_dam_armor(int damage, int ac)
{
    return damage - (damage * ((ac < 240)? ac: 240) / 400);
}


static bool check_hit_aux(struct monster *mon, struct source *who, struct blow_effect *effect)
{
    if (who->monster)
        return test_hit(chance_of_monster_hit(mon, effect), who->monster->ac);
    return check_hit(who->player, chance_of_monster_hit(mon, effect));
}


/*
 * Attack a target via physical attacks.
 */
bool make_attack_normal(struct monster *mon, struct source *who)
{
    struct monster_lore *lore = get_lore(who->player, mon->race);
    struct monster_lore *target_l_ptr = (who->monster?
        get_lore(who->player, who->monster->race): NULL);
    int rlev = ((mon->level >= 1)? mon->level: 1);
    int ap_cnt;
    char m_name[NORMAL_WID];
    char target_m_name[NORMAL_WID];
    char ddesc[NORMAL_WID];
    int blinked = 0;

    /* Assume a default death */
    uint8_t note_dies = MON_MSG_DIE;

    /* Some monsters get "destroyed" */
    if (monster_is_destroyed(mon->race))
    {
        /* Special note at death */
        note_dies = MON_MSG_DESTROYED;
    }

    /* Don't attack shoppers */
    if (in_store(who->player)) return false;

    /* Not allowed to attack */
    if (rf_has(mon->race->flags, RF_NEVER_BLOW)) return false;

    // druid in bird-form can dodge melee blows
    if (who->player->poly_race && streq(who->player->poly_race->name, "bird-form") && magik(who->player->lev))
    {
        msg(who->player, "You dodge the attack!");
        return false;
    }
    // some races can dodge
    else if ((streq(who->player->race->name, "Halfling") || streq(who->player->race->name, "Forest-Goblin") ||
             streq(who->player->race->name, "Pixie")) && magik(5))
    {
        msg(who->player, "You dodge the attack!");
        return false;
    }
    // Wraiths can DODGE
    else if (streq(who->player->race->name, "Wraith") && magik(10))
    {
        msg(who->player, "The attack pass through you without causing any damage!");
        return false;
    }

    /* Get the monster name (or "it") */
    monster_desc(who->player, m_name, sizeof(m_name), mon, MDESC_STANDARD);
    if (who->monster)
        monster_desc(who->player, target_m_name, sizeof(target_m_name), who->monster, MDESC_DEFAULT);
    else
        my_strcpy(target_m_name, "you", sizeof(target_m_name));

    /* Get the "died from" information (i.e. "a kobold") */
    monster_desc(who->player, ddesc, sizeof(ddesc), mon, MDESC_DIED_FROM);

    /* Scan through all blows */
    for (ap_cnt = 0; ap_cnt < z_info->mon_blows_max; ap_cnt++)
    {
        struct loc grid;
        bool visible = (monster_is_visible(who->player, mon->midx) || (mon->race->light > 0));
        bool obvious = false;
        int damage = 0;
        int do_cut = 0;
        int do_stun = 0;
        bool do_conf = false, do_fear = false, do_blind = false, do_para = false;
        bool dead = false;

        /* Extract the attack infomation */
        struct blow_effect *effect = mon->blow[ap_cnt].effect;
        struct blow_method *method = mon->blow[ap_cnt].method;
        random_value dice = mon->blow[ap_cnt].dice;

        loc_copy(&grid, &who->player->grid);

        /* No more attacks */
        if (!method) break;

        /* Stop if player is dead or gone */
        if (!who->player->alive || who->player->is_dead || who->player->upkeep->new_level_method)
            break;

        /* Monster hits target */
        my_assert(effect);
        if (streq(effect->name, "NONE") || check_hit_aux(mon, who, effect))
        {
            melee_effect_handler_f effect_handler;
            const char* flav = NULL;

            /* Always disturbing */
            disturb(who->player, 0);

            /* Apply "protection from evil" */
            if ((who->player->timed[TMD_PROTEVIL] > 0) && !who->monster)
            {
                /* Learn about the evil flag */
                if (visible) rf_on(lore->flags, RF_EVIL);

                if (monster_is_evil(mon) && (who->player->lev >= rlev) &&
                    !magik(PY_MAX_LEVEL - who->player->lev))
                {
                    /* Message */
                    msg(who->player, "%s is repelled.", m_name);

                    /* Next attack */
                    continue;
                }
            }

            do_cut = method->cut;
            do_stun = method->stun;
            if (!who->monster) flav = method->flavor;
            if (!flav) flav = "killed";

            /* Assume all attacks are obvious */
            obvious = true;

            /* Roll dice */
            damage = randcalc(dice, 0, RANDOMISE);

            /* Reduce damage when stunned */
            if (mon->m_timed[MON_TMD_STUN]) damage = (damage * (100 - STUN_DAM_REDUCTION)) / 100;

            /* PWMAngband: freezing aura reduces damage  */
            if ((who->player->timed[TMD_ICY_AURA] > 0) && !who->monster)
                damage = (damage * 90) / 100;

            /* Perform the actual effect. */
            effect_handler = melee_handler_for_blow_effect(effect->name);

            if (effect_handler != NULL)
            {
                melee_effect_handler_context_t context;

                /* Initialize */
                context.p = who->player;
                context.mon = mon;
                context.target = who;
                context.target_l_ptr = target_l_ptr;
                context.rlev = rlev;
                context.method = method;
                context.ac = (who->monster? who->monster->ac:
                    (who->player->state.ac + who->player->state.to_a));
                context.ddesc = ddesc;
                context.obvious = obvious;
                context.visible = visible;
                context.dead = dead;
                context.do_blind = do_blind;
                context.do_para = do_para;
                context.do_conf = do_conf;
                context.do_fear = do_fear;
                strnfmt(context.flav, sizeof(context.flav), "was %s by %s", flav, ddesc);
                context.blinked = blinked;
                context.damage = damage;
                context.note_dies = note_dies;
                context.style = (who->monster? TYPE_MVM: TYPE_MVP);

                effect_handler(&context);

                /* Save any changes made in the handler for later use. */
                obvious = context.obvious;
                if (who->monster)
                    dead = context.dead;
                else
                    dead = who->player->is_dead;
                do_blind = context.do_blind;
                do_para = context.do_para;
                do_conf = context.do_conf;
                do_fear = context.do_fear;
                blinked = context.blinked;
                damage = context.damage;
            }
            else
                plog_fmt("Effect handler not found for %s.", effect->name);

            /* Handle effects (only if not dead) */
            if (!dead)
            {
                /* Only one of cut or stun */
                if (do_cut && do_stun)
                {
                    /* Cancel cut */
                    if (magik(50))
                        do_cut = 0;

                    /* Cancel stun */
                    else
                        do_stun = 0;
                }

                /* Handle cut */
                if (do_cut)
                {
                    if (who->monster)
                    {
                        mon_inc_timed(who->player, who->monster, MON_TMD_CUT, 5 + randint1(5),
                            MON_TMD_FLG_NOTIFY);
                    }
                    else
                    {
                        do_cut = get_cut(dice, damage);
                        if (do_cut) player_inc_timed(who->player, TMD_CUT, do_cut, true, true);
                    }
                }

                /* Handle stun */
                if (do_stun)
                {
                    if (who->monster)
                    {
                        mon_inc_timed(who->player, who->monster, MON_TMD_STUN, 5 + randint1(5),
                            MON_TMD_FLG_NOTIFY);
                    }
                    else
                    {
                        /* Apply the stun */
                        do_stun = get_stun(dice, damage);

                        // prevent insta-KO during the current turn
                        if (do_stun > 0)
                        {
                            int stun_now;

                            // reset stun counter if it's a new turn
                            if (who->player->stun_turn != turn.turn)
                            {
                                who->player->stun_amt_this_turn = 0;
                                who->player->stun_turn = turn.turn;
                            }

                            stun_now = who->player->timed[TMD_STUN] + who->player->stun_amt_this_turn;

                            // prevent instant KO: cap stun increase per turn to 90
                            if (stun_now + do_stun > 90)
                                do_stun = MAX(0, 90 - who->player->stun_amt_this_turn);

                            if (do_stun > 0)
                            {
                                player_inc_timed(who->player, TMD_STUN, do_stun, true, true);
                                who->player->stun_amt_this_turn += do_stun;
                            }
                        }
                    }
                }

                /* Apply fear */
                if (do_fear)
                {
                    mon_inc_timed(who->player, who->monster, MON_TMD_FEAR, 10 + randint1(10),
                        MON_TMD_FLG_NOTIFY);
                }

                /* Apply confusion */
                if (do_conf)
                {
                    mon_inc_timed(who->player, who->monster, MON_TMD_CONF, 5 + randint1(5),
                        MON_TMD_FLG_NOTIFY);
                }

                /* Apply blindness */
                if (do_blind)
                {
                    mon_inc_timed(who->player, who->monster, MON_TMD_BLIND, 5 + randint1(5),
                        MON_TMD_FLG_NOTIFY);
                }

                /* Handle paralysis */
                if (do_para)
                {
                    mon_inc_timed(who->player, who->monster, MON_TMD_HOLD, 3 + randint1(5),
                        MON_TMD_FLG_NOTIFY);
                }
            }
        }

        /* Visible monster missed target, so notify if appropriate. */
        else if (visible && method->miss)
        {
            /* Disturbing */
            disturb(who->player, 0);

            /* Message */
            msgt(who->player, MSG_MISS, "%s misses %s.", m_name, target_m_name);
        }

        /* Analyze "visible" monsters only */
        if (visible)
        {
            /* Count "obvious" attacks (and ones that cause damage) */
            if (obvious || damage || (lore->blows[ap_cnt] > 10))
            {
                /* Count attacks of this type */
                if (lore->blows[ap_cnt] < UCHAR_MAX)
                    lore->blows[ap_cnt]++;
            }
        }

        /* Handle freezing aura */
        if (who->player->timed[TMD_ICY_AURA] && !who->monster)
        {
            who->player->icy_aura = true;
            fire_ball(who->player, PROJ_ICE, 0, 1 + who->player->lev / 5 + randint0(damage) / 10, 1,
                false, true);
            who->player->icy_aura = false;

            /* Stop if monster is unconscious */
            if (mon->hp == 0) break;
        }

        /* Skip the other blows if the player has moved */
        if (dead || !loc_eq(&grid, &who->player->grid)) break;
    }

    /* Blink away */
    if (blinked == 2)
    {
        char dice[5];
        struct loc grid;
        struct source origin_body;
        struct source *origin = &origin_body;

        loc_copy(&grid, &mon->grid);

        source_player(origin, get_player_index(get_connection(who->player->conn)), who->player);
        origin->monster = mon;

        strnfmt(dice, sizeof(dice), "%d", z_info->max_sight * 2 + 5);
        effect_simple(EF_TELEPORT, origin, dice, 0, 0, 0, 0, 0, NULL);
        if (!loc_eq(&grid, &mon->grid))
        {
            if (!who->player->is_dead && square_isseen(who->player, &mon->grid))
                add_monster_message(who->player, mon, MON_MSG_HIT_AND_RUN, true);
        }
    }
    else if (blinked == 1)
    {
        struct loc grid;
        struct source origin_body;
        struct source *origin = &origin_body;

        loc_copy(&grid, &mon->grid);

        source_player(origin, get_player_index(get_connection(who->player->conn)), who->player);
        origin->monster = mon;

        effect_simple(EF_TELEPORT, origin, "10", 0, 0, 0, 0, 0, NULL);
        if (!loc_eq(&grid, &mon->grid))
            msg(who->player, "%s blinks away.", m_name);
    }

    /* Always notice cause of death */
    if (who->player->is_dead && (lore->pdeaths < SHRT_MAX))
        lore->pdeaths++;
    if (who->player->is_dead && (mon->race->lore.tdeaths < SHRT_MAX))
        mon->race->lore.tdeaths++;

    /* Learn lore */
    lore_update(mon->race, lore);

    // Slash effect
    slash_fx(mon, who);

    /* Assume we attacked */
    return true;
}


int get_cut(random_value dice, int d_dam)
{
    /* Critical hit (zero if non-critical) */
    int amt, tmp = monster_critical(dice, d_dam);

    /* Roll for damage */
    switch (tmp)
    {
        case 0: amt = 0; break;
        case 1: amt = randint1(5); break;
        case 2: amt = randint1(5) + 5; break;
        case 3: amt = randint1(20) + 20; break;
        case 4: amt = randint1(50) + 50; break;
        case 5: amt = randint1(100) + 100; break;
        case 6: amt = 300; break;
        default: amt = 500; break;
    }

    return amt;
}


int get_stun(random_value dice, int d_dam)
{
    /* Critical hit (zero if non-critical) */
    int amt, tmp = monster_critical(dice, d_dam);

    /* Roll for damage */
    switch (tmp)
    {
        case 0: amt = 0; break;
        case 1: amt = randint1(5); break;
        case 2: amt = randint1(10) + 10; break;
        case 3: amt = randint1(20) + 20; break;
        case 4: amt = randint1(30) + 30; break;
        case 5: amt = randint1(40) + 40; break;
        case 6: amt = 100; break;
        default: amt = 200; break;
    }

    return amt;
}
