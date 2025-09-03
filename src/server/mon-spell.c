/*
 * File: mon-spell.c
 * Purpose: Monster spell casting and selection
 *
 * Copyright (c) 2010-14 Chris Carr and Nick McConnell
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
 * Spell casting
 */


typedef enum
{
    SPELL_TAG_NONE,
    SPELL_TAG_NAME,
    SPELL_TAG_PRONOUN,
    SPELL_TAG_TARGET,
	SPELL_TAG_TYPE,
	SPELL_TAG_OF_TYPE,
    SPELL_TAG_KIN
} spell_tag_t;


static spell_tag_t spell_tag_lookup(const char *tag)
{
    if (strncmp(tag, "name", 4) == 0) return SPELL_TAG_NAME;
    if (strncmp(tag, "pronoun", 7) == 0) return SPELL_TAG_PRONOUN;
    if (strncmp(tag, "target", 6) == 0) return SPELL_TAG_TARGET;
    if (strncmp(tag, "type", 4) == 0) return SPELL_TAG_TYPE;
    if (strncmp(tag, "oftype", 6) == 0) return SPELL_TAG_OF_TYPE;
    if (strncmp(tag, "kin", 3) == 0) return SPELL_TAG_KIN;
    return SPELL_TAG_NONE;
}


/*
 * Lookup a race-specific message for a spell.
 *
 * r is the race.
 * s_idx is the spell index.
 * msg_type is the type of message.
 *
 * Return the text of the message if there's a race-specific one or NULL if there is not.
 */
static const char *find_alternate_spell_message(const struct monster_race *r, int s_idx,
    enum monster_altmsg_type msg_type)
{
    const struct monster_altmsg *am = r->spell_msgs;

    while (1)
    {
        if (!am) return NULL;
        if (am->index == s_idx && am->msg_type == msg_type) return am->message;
        am = am->next;
    }
}


/*
 * Print a monster spell message.
 *
 * We fill in the monster name and/or pronoun where necessary in
 * the message to replace instances of {name} or {pronoun}.
 */
static void spell_message(struct player *p, struct monster *mon, const struct monster_spell *spell,
    bool seen, bool hits, struct monster *target_mon)
{
    const char punct[] = ".!?;:,'";
    char buf[MSG_LEN] = "\0";
    const char *next;
    const char *s;
    const char *tag;
    const char *in_cursor;
    size_t end = 0;
    struct monster_spell_level *level = spell->level;
    char tmp[MSG_LEN];
    bool is_leading;

    /* Get the right level of message */
    while (level->next && mon->race->spell_power >= level->next->power) level = level->next;

    /* Get the message */
    if (!seen)
    {
        if (target_mon) return;
        in_cursor = find_alternate_spell_message(mon->race, spell->index, MON_ALTMSG_UNSEEN);
        if (in_cursor == NULL)
        {
            in_cursor = level->blind_message;
            if (in_cursor == NULL)
            {
                plog_fmt("No message-invis for monster spell %d cast by %s. Please report this bug.",
                    (int)spell->index, mon->race->name);
                return;
            }
        }
        else if (in_cursor[0] == '\0') return;
    }
    else if (!hits)
    {
        in_cursor = find_alternate_spell_message(mon->race, spell->index, MON_ALTMSG_MISS);
        if (in_cursor == NULL)
        {
            in_cursor = level->miss_message;
            if (in_cursor == NULL)
            {
                plog_fmt("No message-miss for monster spell %d cast by %s. Please report this bug.",
                    (int)spell->index, mon->race->name);
                return;
            }
        }
        else if (in_cursor[0] == '\0') return;
    }
    else
    {
        in_cursor = find_alternate_spell_message(mon->race, spell->index, MON_ALTMSG_SEEN);
        if (in_cursor == NULL)
        {
            in_cursor = level->message;
            if (in_cursor == NULL)
            {
                plog_fmt("No message-vis for monster spell %d cast by %s. Please report this bug.",
                    (int)spell->index, mon->race->name);
                return;
            }
        }
        else if (in_cursor[0] == '\0') return;
    }

    next = strchr(in_cursor, '{');
    is_leading = (next == in_cursor);
    while (next)
    {
        /* Copy the text leading up to this { */
        strnfcat(buf, sizeof(buf), &end, "%.*s", (int)(next - in_cursor), in_cursor);

        s = next + 1;
        while (*s && isalpha((unsigned char) *s)) s++;

        /* Valid tag */
        if (*s == '}')
        {
            /* Start the tag after the { */
            tag = next + 1;
            in_cursor = s + 1;

            switch (spell_tag_lookup(tag))
            {
                case SPELL_TAG_NAME:
                {
                    char m_name[NORMAL_WID];
                    int mdesc_mode = (MDESC_IND_HID | MDESC_PRO_HID);

                    if (is_leading) mdesc_mode |= MDESC_CAPITAL;
                    if (!strchr(punct, *in_cursor)) mdesc_mode |= MDESC_COMMA;

                    /* Get the monster name (or "it") */
                    monster_desc(p, m_name, sizeof(m_name), mon, mdesc_mode);

                    strnfcat(buf, sizeof(buf), &end, "%s", m_name);
                    break;
                }
                case SPELL_TAG_PRONOUN:
                {
                    char m_poss[NORMAL_WID];

                    /* Get the monster possessive ("his"/"her"/"its") */
                    monster_desc(p, m_poss, sizeof(m_poss), mon, MDESC_PRO_VIS | MDESC_POSS);

                    strnfcat(buf, sizeof(buf), &end, "%s", m_poss);
                    break;
                }
                case SPELL_TAG_TARGET:
                {
                    char m_name[NORMAL_WID];
                    int mdesc_mode = MDESC_TARG;

                    if (!strchr(punct, *in_cursor)) mdesc_mode |= MDESC_COMMA;

                    if (target_mon)
                    {
                        monster_desc(p, m_name, sizeof(m_name), target_mon, mdesc_mode);
                        strnfcat(buf, sizeof(buf), &end, "%s", m_name);
                    }
                    else
                        strnfcat(buf, sizeof(buf), &end, "%s", "you");

                    break;
                }
                case SPELL_TAG_TYPE:
                {
                    /* Get the attack type (assuming lash) */
                    int type = mon->race->blow[0].effect->lash_type;
                    char *type_name = projections[type].lash_desc;

                    strnfcat(buf, sizeof(buf), &end, "%s", type_name);
                    break;
                }
                case SPELL_TAG_OF_TYPE:
                {
                    /* Get the attack type (assuming lash) */
                    int type = mon->race->blow[0].effect->lash_type;
                    char *type_name = projections[type].lash_desc;

                    if (type_name)
                    {
                        strnfcat(buf, sizeof(buf), &end, "%s", " of ");
                        strnfcat(buf, sizeof(buf), &end, "%s", type_name);
                    }
                    break;
                }
                case SPELL_TAG_KIN:
                {
                    strnfcat(buf, sizeof(buf), &end, "%s",
                        (monster_is_unique(mon)? "minions": "kin"));
                    break;
                }
                default: break;
            }
        }

        /* An invalid tag, skip it */
        else
            in_cursor = next + 1;

        next = strchr(in_cursor, '{');
        is_leading = false;
    }
    strnfcat(buf, sizeof(buf), &end, "%s", in_cursor);

    /* Hack -- replace "your" by "some" */
    if (target_mon) strrep(tmp, sizeof(tmp), buf, "your", "some");

    if (spell->msgt) msgt(p, spell->msgt, buf);
    else msg(p, buf);

    /* Hack -- print message to nearby players */
    if (level->near_message && !target_mon)
    {
        int i;

        /* Check each player */
        for (i = 1; i <= NumPlayers; i++)
        {
            /* Check this player */
            struct player *player = player_get(i);

            /* Don't send the message to the player who caused it */
            if (p == player) continue;

            /* Make sure this player is on this level */
            if (!wpos_eq(&player->wpos, &p->wpos)) continue;

            /* Can he see this monster? */
            if (square_isview(player, &mon->grid))
            {
                char m_name[NORMAL_WID];

                /* Get the monster name (or "it") */
                monster_desc(player, m_name, sizeof(m_name), mon, MDESC_STANDARD);

                /* Send the message */
                msgt(player, MSG_MON_OTHER, level->near_message, m_name, p->name);
            }
        }
    }
}


const struct monster_spell *monster_spell_by_index(int index)
{
    const struct monster_spell *spell = monster_spells;

    while (spell)
    {
        if (spell->index == index) break;
        spell = spell->next;
    }
    return spell;
}


/*
 * Types of monster spells used for spell selection.
 */
static const struct mon_spell_info
{
    uint16_t index; /* Numerical index (RSF_FOO) */
    int type;   /* Type bitflag */
    uint8_t save;  /* Type of saving throw */
} mon_spell_types[] =
{
    #define RSF(a, b, c) {RSF_##a, b, c},
    #include "../common/list-mon-spells.h"
    #undef RSF
    {RSF_MAX, 0, 0}
};


/*
 * Check if a spell effect which has been saved against would also have
 * been prevented by an object property, and learn the appropriate rune
 */
static void spell_check_for_fail_rune(struct player *p, const struct monster_spell *spell)
{
    struct effect *effect = spell->effect;

    while (effect)
    {
        /* Special case - teleport level */
        if (effect->index == EF_TELEPORT_LEVEL) equip_learn_element(p, ELEM_NEXUS);

        /* Timed effects */
        else if (effect->index == EF_TIMED_INC) player_inc_check(p, NULL, effect->subtype, false);

        effect = effect->next;
    }
}


/*
 * Calculate the base to-hit value for a monster spell based on race only
 * See also: chance_of_monster_hit_base
 */
static int chance_of_spell_hit_base(const struct monster_race *race,
    const struct monster_spell *spell)
{
    return MAX(race->level, 1) * 3 + spell->hit;
}


/*
 * Calculate the to-hit value of a monster spell for a specific monster
 */
static int chance_of_spell_hit(const struct monster *mon, const struct monster_spell *spell)
{
    int to_hit = chance_of_spell_hit_base(mon->race, spell);
    int i;

    /* Apply confusion hit reduction for each level of confusion */
    for (i = 0; i < monster_effect_level(mon, MON_TMD_CONF); i++)
        to_hit = to_hit * (100 - CONF_HIT_REDUCTION) / 100;

    return to_hit;
}


/*
 * Process a monster spell
 *
 * p is the affected player
 * c is the current cave level
 * target_mon is the target monster (or NULL if targeting the player)
 * index is the monster spell flag (RSF_FOO)
 * mon is the attacking monster
 * seen is whether the player can see the monster at this moment
 */
void do_mon_spell(struct player *p, struct chunk *c, struct monster *target_mon, int index,
    struct monster *mon, bool seen)
{
    const struct monster_spell *spell = monster_spell_by_index(index);
    bool ident = false;
    bool hits;
    const struct mon_spell_info *info = &mon_spell_types[index];

    /* Antimagic field prevents magical spells from working */
    if (!(info->type & (RST_BREATH | RST_DIRECT | RST_MISSILE)) && check_antimagic(p, c, mon))
        return;

    /* Antisummon field prevents summoning spells from working */
    if ((info->type & RST_SUMMON) && check_antisummon(p, mon)) return;

    /* See if it hits */
    if (spell->hit == 100)
        hits = true;
    else if (spell->hit == 0)
        hits = false;
    else
    {
        if (target_mon)
            hits = test_hit(chance_of_spell_hit(mon, spell), target_mon->race->ac);
        else
            hits = check_hit(p, chance_of_spell_hit(mon, spell));
    }

    /* Tell the player what's going on */
    disturb(p, 0);
    spell_message(p, mon, spell, seen, hits, target_mon);

    if (hits)
    {
        bool save = false;

        if (!target_mon)
        {
            if ((info->save & RSV_SKILL) && magik(p->state.skills[SKILL_SAVE])) save = true;
            if ((info->save & RSV_UNDEAD) && resist_undead_attacks(p, mon->race)) save = true;
        }

        /* Try a saving throw if available */
        if (save)
        {
            struct monster_spell_level *level = spell->level;

            /* Get the right level of save message */
            while (level->next && mon->race->spell_power >= level->next->power) level = level->next;

            msg(p, level->save_message);
            spell_check_for_fail_rune(p, spell);
        }
        else
        {
            struct source who_body;
            struct source *who = &who_body;

            /* Learn about projectable attacks */
            if (!target_mon && (info->type & (RST_BOLT | RST_BALL | RST_BREATH)))
                update_smart_learn(mon, p, 0, 0, spell->effect->subtype);

            source_player(who, get_player_index(get_connection(p->conn)), p);
            who->monster = mon;
            effect_do(spell->effect, who, &ident, true, 0, NULL, 0, 0, target_mon);
        }
    }
}


/*
 * Spell selection
 */


static bool mon_spell_is_valid(int index)
{
    return ((index > RSF_NONE) && (index < RSF_MAX));
}


static bool monster_spell_is_breath(int index)
{
    return ((mon_spell_types[index].type & RST_BREATH)? true: false);
}


static bool mon_spell_has_damage(int index)
{
    return ((mon_spell_types[index].type & RST_DAMAGE)? true: false);
}


bool mon_spell_is_innate(int index)
{
    return ((mon_spell_types[index].type & RST_INNATE)? true: false);
}


/*
 * Test a spell bitflag for a type of spell.
 * Returns true if any desired type is among the flagset
 *
 * f is the set of spell flags we're testing
 * types is the spell type(s) we're looking for
 */
bool test_spells(bitflag *f, int types)
{
    const struct mon_spell_info *info;

    for (info = mon_spell_types; info->index < RSF_MAX; info++)
    {
        if (rsf_has(f, info->index) && (info->type & types))
            return true;
    }

    return false;
}


/*
 * Set a spell bitflag to allow only breaths.
 */
void set_breath(bitflag *f)
{
    const struct mon_spell_info *info;

    for (info = mon_spell_types; info->index < RSF_MAX; info++)
    {
        if (rsf_has(f, info->index) && !(info->type & RST_BREATH))
            rsf_off(f, info->index);
    }
}


/*
 * Set a spell bitflag to ignore a specific set of spell types.
 *
 * f is the set of spell flags we're pruning
 * types is the spell type(s) we're ignoring
 */
void ignore_spells(bitflag *f, int types)
{
    const struct mon_spell_info *info;

    for (info = mon_spell_types; info->index < RSF_MAX; info++)
    {
        if (rsf_has(f, info->index) && (info->type & types))
            rsf_off(f, info->index);
    }
}


/*
 * Turn off spells with a side effect or a proj_type that is resisted by
 * something in flags, subject to intelligence and chance.
 *
 * p is the affected player
 * spells is the set of spells we're pruning
 * flags is the set of flags we're testing
 * pflags is the set of player flags we're testing
 * el is the attack element
 * mon is the monster whose spells we are considering
 */
void unset_spells(struct player *p, bitflag *spells, bitflag *flags, bitflag *pflags,
    struct element_info *el, const struct monster *mon)
{
    const struct mon_spell_info *info;
    bool smart = monster_is_smart(mon);

    for (info = mon_spell_types; info->index < RSF_MAX; info++)
    {
        const struct monster_spell *spell = monster_spell_by_index(info->index);
        const struct effect *effect;

        /* Ignore missing spells */
        if (!spell) continue;
        if (!rsf_has(spells, info->index)) continue;

        /* Get the effect */
        effect = spell->effect;

        /* First we test the elemental spells */
        if (info->type & (RST_BOLT | RST_BALL | RST_BREATH))
        {
            int element = effect->subtype;
            int learn_chance = el[element].res_level[0] * (smart? 50: 25);

            if (magik(learn_chance)) rsf_off(spells, info->index);
        }

        /* Now others with resisted effects */
        else
        {
            while (effect)
            {
                /* Timed effects */
                if ((smart || !one_in_(3)) && (effect->index == EF_TIMED_INC))
                {
                    const struct timed_failure *f;
                    bool resisted = false;

                    my_assert(effect->subtype >= 0 && effect->subtype < TMD_MAX);
                    for (f = timed_effects[effect->subtype].fail; f && !resisted; f = f->next)
                    {
                        switch (f->code)
                        {
                            case TMD_FAIL_FLAG_OBJECT:
                            {
                                if (of_has(flags, f->idx)) resisted = true;
                                break;
                            }

                            case TMD_FAIL_FLAG_RESIST:
                            {
                                if (el[f->idx].res_level[0] > 0) resisted = true;
                                break;
                            }

                            case TMD_FAIL_FLAG_VULN:
                            {
                                if (el[f->idx].res_level[0] < 0) resisted = true;
                                break;
                            }

                            case TMD_FAIL_FLAG_PLAYER:
                            {
                                if (pf_has(pflags, f->idx)) resisted = true;
                                break;
                            }

                            /*
                             * The monster doesn't track the timed effects present on the player
                             * so do nothing with resistances due to those.
                             */
                            case TMD_FAIL_FLAG_TIMED_EFFECT: break;
                        }
                    }

                    if (resisted) break;
                }

                /* Mana drain */
                if ((smart || one_in_(2)) && (effect->index == EF_DRAIN_MANA) &&
                    pf_has(pflags, PF_NO_MANA))
                {
                    break;
                }

                effect = effect->next;
            }
            if (effect)
                rsf_off(spells, info->index);
        }
    }
}


/*
 * Determine the damage of a spell attack which ignores monster hp
 * (i.e. bolts and balls, including arrows/boulders/storms/etc.)
 *
 * spell is the attack type
 * race is the monster race of the attacker
 * dam_aspect is the damage calc required (min, avg, max, random)
 */
static int nonhp_dam(const struct monster_spell *spell, const struct monster_race *race,
    aspect dam_aspect)
{
    int dam = 0;
    struct effect *effect = spell->effect;

    /* Set the reference race for calculations */
    ref_race = race;

    /* Now add the damage for each effect (PWMAngband: discard PROJECT -- used for MvM) */
    while (effect)
    {
        random_value rand;

        memset(&rand, 0, sizeof(rand));

        /* Lash needs special treatment bacause it depends on monster blows */
        if (effect->index == EF_LASH)
        {
            int i;

            /* Scan through all blows for damage */
            for (i = 0; i < z_info->mon_blows_max; i++)
            {
                /* Extract the attack infomation */
                random_value dice = race->blow[i].dice;

                /* Full damage of first blow, plus half damage of others */
                dam += randcalc(dice, race->level, RANDOMISE) / (i? 2: 1);
            }
        }

        /* Timed effects increases don't count as damage in lore */
        else if (effect->dice && (effect->index != EF_TIMED_INC) && (effect->index != EF_PROJECT))
        {
            dice_roll(effect->dice, NULL, &rand);
            dam += randcalc(rand, 0, dam_aspect);
        }
        effect = effect->next;
    }

    ref_race = NULL;

    return dam;
}


/*
 * Determine the damage of a monster breath attack
 *
 * type is the attack element type
 * hp is the monster's hp
 */
int breath_dam(int type, int hp)
{
    struct projection *element = &projections[type];
    int dam;

    /* Damage is based on monster's current hp */
    dam = hp / element->divisor;

    /* Check for maximum damage */
    if (dam > element->damage_cap) dam = element->damage_cap;

    return dam;
}


/*
 * Calculate the damage of a monster spell.
 *
 * index is the index of the spell in question.
 * hp is the hp of the casting monster.
 * race is the race of the casting monster.
 * dam_aspect is the damage calc we want (min, max, avg, random).
 */
static int mon_spell_dam(int index, int hp, const struct monster_race *race, aspect dam_aspect)
{
    const struct monster_spell *spell = monster_spell_by_index(index);

    if (monster_spell_is_breath(index)) return breath_dam(spell->effect->subtype, hp);
    return nonhp_dam(spell, race, dam_aspect);
}


/*
 * Create a mask of monster spell flags of a specific type.
 *
 * f is the flag array we're filling
 * ... is the list of flags we're looking for
 *
 * N.B. RST_NONE must be the last item in the ... list
 */
void create_mon_spell_mask(bitflag *f, ...)
{
    const struct mon_spell_info *rs;
    int i;
    va_list args;

    rsf_wipe(f);

    va_start(args, f);

    /* Process each type in the va_args */
    for (i = va_arg(args, int); i != RST_NONE; i = va_arg(args, int))
    {
        for (rs = mon_spell_types; rs->index < RSF_MAX; rs++)
        {
            if (rs->type & i)
                rsf_on(f, rs->index);
        }
    }

    va_end(args);
}


const char *mon_spell_lore_description(int index, const struct monster_race *race)
{
    if (mon_spell_is_valid(index))
    {
        const struct monster_spell *spell = monster_spell_by_index(index);
        struct monster_spell_level *level = spell->level;

        /* Get the right level of description */
        while (level->next && race->spell_power >= level->next->power) level = level->next;

        return level->lore_desc;
    }

    return "";
}


int mon_spell_lore_damage(int index, const struct monster_race *race, bool know_hp)
{
    if (mon_spell_is_valid(index) && mon_spell_has_damage(index))
    {
        int hp = (know_hp? race->avg_hp: 0);

        return mon_spell_dam(index, hp, race, MAXIMISE);
    }

    return 0;
}


/*
 * PWMAngband
 */


/*
 * Set all spell bitflags in a set of spell flags.
 */
void init_spells(bitflag *f)
{
    const struct mon_spell_info *info;

    for (info = mon_spell_types; info->index < RSF_MAX; info++)
    {
        if (info->index) rsf_on(f, info->index);
    }
}


bool is_spell_summon(int index)
{
    const struct mon_spell_info *info = &mon_spell_types[index];

    if (info->type & RST_SUMMON) return true;
    return false;
}


int spell_effect(int index)
{
    const struct monster_spell *spell = monster_spell_by_index(index);

    return spell->effect->subtype;
}


int breath_effect(struct player *p, bitflag mon_breath[RSF_SIZE])
{
    int flag, thrown_breath;
    int breath[20], num = 0;
    const struct monster_spell *spell;
    char buf[NORMAL_WID];

    /* Extract the breath attacks */
    for (flag = rsf_next(mon_breath, FLAG_START); flag != FLAG_END;
        flag = rsf_next(mon_breath, flag + 1))
    {
        breath[num++] = flag;
    }

    /* Choose a breath attack */
    thrown_breath = breath[randint0(num)];
    spell = monster_spell_by_index(thrown_breath);

    /* Message */
    msgt(p, spell->msgt, "You breathe %s.", spell->level->lore_desc);
    strnfmt(buf, sizeof(buf), " breathes %s.", spell->level->lore_desc);
    msg_misc(p, buf);

    return spell_effect(thrown_breath);
}
