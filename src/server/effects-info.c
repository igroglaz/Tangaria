/*
 * File: effects-info.c
 * Purpose: Implement interfaces for displaying information about effects
 *
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 * Copyright (c) 2004 Robert Ruehlmann
 * Copyright (c) 2010 Andi Sidwell
 * Copyright (c) 2020 Eric Branlund
 * Copyright (c) 2023 MAngband and PWMAngband Developers
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


static struct
{
    int index;
    int args;
    int efinfo_flag;
    const char *desc;
} base_descs[] =
{
    {EF_NONE, 0, EFINFO_NONE, ""},
    #define EFFECT(x, a, b, c, d, e) {EF_##x, c, d, e},
    #include "list-effects.h"
    #undef EFFECT
    {EF_MAX, 0, EFINFO_NONE, ""}
};


/*
 * Get the possible dice strings.
 */
static void format_dice_string(const random_value *v, int multiplier, size_t len, char* dice_string)
{
    if (v->dice && v->base)
    {
        if (multiplier == 1)
            strnfmt(dice_string, len, "{%d+%dd%d}", v->base, v->dice, v->sides);
        else
        {
            strnfmt(dice_string, len, "{%d+%d*(%dd%d)}", multiplier * v->base, multiplier, v->dice,
                v->sides);
        }
    }
    else if (v->dice)
    {
        if (multiplier == 1)
            strnfmt(dice_string, len, "{%dd%d}", v->dice, v->sides);
        else
            strnfmt(dice_string, len, "{%d*(%dd%d)}", multiplier, v->dice, v->sides);
    }
    else
        strnfmt(dice_string, len, "{%d}", multiplier * v->base);
}


/*
 * Appends a message describing the magical device skill bonus and the average
 * damage. Average damage is only displayed if there is variance or a magical
 * device bonus.
 */
static void append_damage(char *buffer, size_t buffer_size, random_value value,
    int dev_skill_boost)
{
    if (dev_skill_boost != 0)
    {
        my_strcat(buffer, format(", which your device skill increases by {%d%%%%}", dev_skill_boost),
            buffer_size);
    }
    if (randcalc_varies(value) || (dev_skill_boost > 0))
    {
        /* Ten times the average damage, for 1 digit of precision */
        int dam = (100 + dev_skill_boost) * randcalc(value, 0, AVERAGE) / 10;

        my_strcat(buffer, format(" for an average of {%d.%d} damage", dam / 10, dam % 10),
            buffer_size);
    }
}


void print_effect(struct player *p, const char *d)
{
    char desc[MSG_LEN];
    char *t;
    bool colored = false;

    /* Print a colourised description */
    my_strcpy(desc, d, sizeof(desc));
    t = strtok(desc, "{}");
    while (t)
    {
        if (colored) text_out_c(p, COLOUR_L_GREEN, t);
        else text_out(p, t);
        colored = !colored;
        t = strtok(NULL, "{}");
    }
}


bool effect_describe(struct player *p, const struct object *obj, const struct effect *e)
{
    char desc[MSG_LEN];
    int random_choices = 0;
    struct source actor_body;
    struct source *data = &actor_body;
    bool same_effect = false, same_value = false;
    random_value value;
    bool value_set = false;

    source_player(data, 0, p);

    while (e)
    {
        int roll = 0;
        char dice_string[20];
        int level, boost;

        /* Skip blank descriptions */
        if (!effect_desc(e))
        {
            e = e->next;
            continue;
        }

        level = (obj->artifact? get_artifact_level(p, obj): obj->kind->level);
        boost = MAX((p->state.skills[SKILL_DEVICE] - level) / 2, 0);

        memset(&value, 0, sizeof(value));

        /* Deal with special clear value effect. */
        if (e->index == EF_CLEAR_VALUE)
        {
            my_assert(value_set);
            value_set = false;
            e = e->next;
            continue;
        }

        /* Deal with special set value effect. */
        if (e->index == EF_SET_VALUE)
        {
            my_assert(e->dice != NULL);
            roll = dice_roll(e->dice, (void *)data, &value);
            value_set = true;
            e = e->next;
            continue;
        }

        if ((e->dice != NULL) && !value_set)
            roll = dice_roll(e->dice, (void *)data, &value);

        /* Deal with special random effect */
        if (e->index == EF_RANDOM) random_choices = roll + 1;

        /* Get the possible dice strings */
        format_dice_string(&value, 1, sizeof(dice_string), dice_string);

        /* Check all the possible types of description format */
        switch (base_descs[e->index].efinfo_flag)
        {
            /* Straight copy */
            case EFINFO_NONE:
            {
                my_strcpy(desc, effect_desc(e), sizeof(desc));
                break;
            }

            case EFINFO_DICE:
            {
                strnfmt(desc, sizeof(desc), effect_desc(e), dice_string);
                break;
            }

            /* Healing sometimes has a minimum percentage */
            case EFINFO_HEAL:
            {
                char min_string[50];

                if (value.m_bonus)
                {
                    strnfmt(min_string, sizeof(min_string),
                        " (or {%d%%} of max HP, whichever is greater)", value.m_bonus);
                }
                else
                    my_strcpy(min_string, "", sizeof(min_string));
                strnfmt(desc, sizeof(desc), effect_desc(e), dice_string, min_string);
                break;
            }

            /* Use dice string */
            case EFINFO_CONST:
            {
                strnfmt(desc, sizeof(desc), effect_desc(e), dice_string);
                break;
            }

            /* PWMAngband: use dice string and apply digestion rate */
            case EFINFO_FOOD:
            {
                /* Basic digestion rate based on speed */
                int rate = player_digest(p);

                /* Adjust for player speed */
                int multiplier = turn_energy(p->state.speed);

                char *fed = "feeds you";
                char turn_dice_string[20];

                if (e->subtype)
                {
                    if (e->subtype > 1) fed = "leaves you nourished";
                    else fed = "uses enough food value";
                }

                format_dice_string(&value, z_info->food_value * multiplier / rate,
                    sizeof(turn_dice_string), turn_dice_string);

                /* Hack -- check previous effect */
                if (same_effect)
                {
                    same_effect = false;

                    /* Identical: skip */
                    if (same_value)
                    {
                        same_value = false;
                        desc[0] = '\0';
                    }

                    /* Different values: display the value */
                    else
                    {
                        strnfmt(desc, sizeof(desc), "for %s turns (%s percent)", turn_dice_string,
                            dice_string);
                    }
                }
                else
                    strnfmt(desc, sizeof(desc), effect_desc(e), fed, turn_dice_string, dice_string);

                /* Hack -- check next effect */
                if (e->next && (e->next->index == e->index) && (e->next->subtype == e->subtype))
                {
                    random_value nextvalue;

                    memset(&nextvalue, 0, sizeof(nextvalue));
                    if (e->next->dice != NULL)
                        dice_roll(e->next->dice, (void *)data, &nextvalue);

                    same_effect = true;
                    if ((nextvalue.base == value.base) && (nextvalue.dice == value.dice) &&
                        (nextvalue.sides == value.sides))
                    {
                        same_value = true;
                    }
                }

                break;
            }

            /* Timed effect description */
            case EFINFO_CURE:
            {
                strnfmt(desc, sizeof(desc), effect_desc(e), timed_effects[e->subtype].desc);
                break;
            }

            /* Timed effect description + duration */
            case EFINFO_TIMED:
            {
                strnfmt(desc, sizeof(desc), effect_desc(e), timed_effects[e->subtype].desc,
                    dice_string);
                break;
            }

            /* Stat name */
            case EFINFO_STAT:
            {
                strnfmt(desc, sizeof(desc), effect_desc(e),
                    lookup_obj_property(OBJ_PROPERTY_STAT, e->subtype)->name);
                break;
            }

            /* PWMAngband: restore original description (spell effect description + dice string) */
            case EFINFO_SEEN:
            {
                const char *proj_desc = projections[e->subtype].desc;

                /* Hack -- some effects have a duration */
                if (strstr(proj_desc, "%s"))
                {
                    char tmp[100];

                    strnfmt(tmp, sizeof(tmp), proj_desc, dice_string);
                    strnfmt(desc, sizeof(desc), effect_desc(e), tmp);
                }
                else
                    strnfmt(desc, sizeof(desc), effect_desc(e), proj_desc);
                break;
            }

            /* Summon effect description */
            case EFINFO_SUMM:
            {
                /* Hack -- check previous effect */
                if (same_effect)
                {
                    same_effect = false;

                    /* Identical: skip */
                    desc[0] = '\0';
                }
                else
                    strnfmt(desc, sizeof(desc), effect_desc(e), dice_string, summon_desc(e->subtype));

                /* Hack -- check next effect */
                if (e->next && (e->next->index == e->index) && (e->next->subtype == e->subtype))
                {
                    random_value nextvalue;

                    memset(&nextvalue, 0, sizeof(nextvalue));
                    if (e->next->dice != NULL)
                        dice_roll(e->next->dice, (void *)data, &nextvalue);

                    if ((nextvalue.base == value.base) && (nextvalue.dice == value.dice) &&
                        (nextvalue.sides == value.sides))
                    {
                        same_effect = true;
                    }
                }

                /* Hack -- only display suffix if last */
                if (!e->next)
                    my_strcat(desc, " generated at the current dungeon level", sizeof(desc));

                break;
            }

            /* PWMAngband: just use dice string since it's only used for objects */
            case EFINFO_TELE:
            {
                strnfmt(desc, sizeof(desc), effect_desc(e), dice_string);
                break;
            }

            /* PWMAngband: using dice string or radius because it's not always a constant */
            case EFINFO_QUAKE:
            {
                if (e->radius)
                    strnfmt(dice_string, sizeof(dice_string), "{%d}", e->radius);
                strnfmt(desc, sizeof(desc), effect_desc(e), dice_string);
                break;
            }

            /* Object generated balls are elemental */
            /* PWMAngband: restore original description (reverse radius and description) */
            case EFINFO_BALL:
            {
                strnfmt(desc, sizeof(desc), effect_desc(e), e->radius,
                    projections[e->subtype].desc, dice_string);
                append_damage(desc, sizeof(desc), value, boost);
                break;
            }

            /* Object generated breaths are elemental */
            /* PWMAngband: restore original description (effect + damage) */
            case EFINFO_BREATH:
            {
                /* Hack -- check next effect */
                if (e->next && (e->next->index == e->index) && (e->index != EF_BREATH))
                {
                    random_value nextvalue;

                    memset(&nextvalue, 0, sizeof(nextvalue));
                    if (e->next->dice != NULL)
                        dice_roll(e->next->dice, (void *)data, &nextvalue);
                    if ((nextvalue.base == value.base) && (nextvalue.dice == value.dice) &&
                        (nextvalue.sides == value.sides))
                    {
                        same_effect = true;
                    }
                }

                /* Hack -- check previous effect */
                if (same_value)
                {
                    if (same_effect) same_effect = false;
                    else same_value = false;

                    /* Same values: display the effect, only display damage if last */
                    if (!e->next)
                    {
                        strnfmt(desc, sizeof(desc), "%s for %s points of damage",
                            projections[e->subtype].desc, dice_string);
                    }
                    else
                        my_strcpy(desc, projections[e->subtype].desc, sizeof(desc));
                }

                /* Hack -- check next effect */
                else if (same_effect)
                {
                    same_effect = false;
                    same_value = true;

                    strnfmt(desc, sizeof(desc), "produces a cone of %s",
                            projections[e->subtype].desc);
                }

                /* Normal case */
                else
                {
                    strnfmt(desc, sizeof(desc), effect_desc(e), projections[e->subtype].desc,
                        dice_string);
                }

                /* Hack -- only display boost if last */
                if (!e->next)
                    append_damage(desc, sizeof(desc), value, ((e->index == EF_BREATH)? 0: boost));

                break;
            }

            /* Currently no object generated short beams */
            case EFINFO_SHORT:
            {
                strnfmt(desc, sizeof(desc), effect_desc(e), projections[e->subtype].desc, e->radius,
                    dice_string);
                break;
            }

            /* Currently no object generated lashes */
            case EFINFO_LASH:
            {
                strnfmt(desc, sizeof(desc), effect_desc(e),
                    projections[e->subtype].lash_desc, e->subtype);
                break;
            }

            /* Bolts that inflict status */
            case EFINFO_BOLT:
            {
                const char *proj_desc = projections[e->subtype].desc;

                /* Hack -- some effects have a duration */
                if (strstr(proj_desc, "%s"))
                {
                    char tmp[100];

                    strnfmt(tmp, sizeof(tmp), proj_desc, dice_string);
                    strnfmt(desc, sizeof(desc), effect_desc(e), tmp);
                }
                else
                    strnfmt(desc, sizeof(desc), effect_desc(e), proj_desc);
                break;
            }

            /* Bolts and beams that damage */
            case EFINFO_BOLTD:
            {
                strnfmt(desc, sizeof(desc), effect_desc(e),
                    projections[e->subtype].desc, dice_string);
                append_damage(desc, sizeof(desc), value, boost);
                break;
            }

            /* PWMAngband: restore original description (spell effect description + dice string) */
            case EFINFO_TOUCH:
            {
                const char *proj_desc = projections[e->subtype].desc;

                /* Hack -- some effects have a duration */
                if (strstr(proj_desc, "%s"))
                {
                    char tmp[100];

                    strnfmt(tmp, sizeof(tmp), proj_desc, dice_string);
                    strnfmt(desc, sizeof(desc), effect_desc(e), tmp);
                }
                else
                    strnfmt(desc, sizeof(desc), effect_desc(e), proj_desc);
                break;
            }

            /* PWMAngband: restore mana can restore a fixed amount of mana points, or all of them */
            case EFINFO_MANA:
            {
                if (!value.base) my_strcpy(dice_string, "all your", sizeof(dice_string));
                strnfmt(desc, sizeof(desc), effect_desc(e), dice_string);
                break;
            }

            /* PWMAngband: restore original description */
            case EFINFO_ENCHANT:
            {
                const char *what;

                switch (e->subtype)
                {
                    case 1: what = "a weapon's to-hit bonus"; break;
                    case 2: what = "a weapon's to-dam bonus"; break;
                    case 3: what = "a weapon's to-hit and to-dam bonuses"; break;
                    case 4: what = "a piece of armor"; break;
                    default: what = "something"; /* XXX */
                }
                strnfmt(desc, sizeof(desc), effect_desc(e), what, dice_string);
                break;
            }

            // Currently no object generated bolt radius
            case EFINFO_BOLT_RADIUS:
            {
                strnfmt(desc, sizeof(desc), effect_desc(e), projections[e->subtype].desc, e->radius, dice_string);
                break;
            }

            default:
            {
                msg(p, "Bad effect description passed to describe_effect(). Please report this bug.");
                return false;
            }
        }

        if (STRZERO(desc))
        {
            if (random_choices >= 1) random_choices--;
            e = e->next;
            continue;
        }
        else
            print_effect(p, desc);

        /*
         * Random choices need special treatment - note that this code
         * assumes that RANDOM and the random choices will be the last
         * effect in the object/activation description
         */
        if (random_choices >= 1)
        {
            if (e->index == EF_RANDOM) {}
            else if (random_choices > 2) text_out(p, ", ");
            else if (random_choices == 2) text_out(p, " or ");
            random_choices--;
        }
        else if (e->next)
        {
            struct effect *next = e->next;

            if (next->next && (next->index != EF_RANDOM) && effect_desc(next))
                text_out(p, ", ");
            else
                text_out(p, " and ");
        }
        e = e->next;
    }

    return true;
}


/*
 * Returns a pointer to the next effect in the effect stack, skipping over
 * all the sub-effects from random effects
 */
struct effect *effect_next(struct effect *effect, void *data)
{
    if (effect->index == EF_RANDOM)
    {
        struct effect *e = effect;
        int num_subeffects = MAX(0, dice_evaluate(effect->dice, 0, AVERAGE, data, NULL));
        int i;

        /* Skip all the sub-effects, plus one to advance beyond current */
        for (i = 0; e != NULL && i < num_subeffects + 1; i++) e = e->next;

        return e;
    }

    return effect->next;
}


/*
 * Checks if the effect deals damage, by checking the effect's info string.
 * Random effects are considered to deal damage if any sub-effect deals
 * damage.
 */
bool effect_damages(const struct effect *effect, void *data, const char *name)
{
    const char *type;

    if (effect->index == EF_RANDOM)
    {
        /* Random effect */
        struct effect *e = effect->next;
        int num_subeffects = dice_evaluate(effect->dice, 0, AVERAGE, data, NULL);
        int i;

        /* Check if any of the subeffects do damage */
        for (i = 0; e != NULL && i < num_subeffects; i++)
        {
            if (effect_damages(e, data, name)) return true;
            e = e->next;
        }

        return false;
    }

    type = effect_info(effect, name);

    /* Non-random effect, check the info string for damage */
    return ((type != NULL) && streq(type, "dam"));
}


/*
 * Calculates the average damage of the effect. Random effects return an
 * average of all sub-effect averages.
 */
int effect_avg_damage(const struct effect *effect, void *data, const char *name,
    const random_value *shared_rv)
{
    if (effect->index == EF_RANDOM)
    {
        /* Random effect, check the sub-effects to accumulate damage */
        int total = 0;
        struct effect *e = effect->next;
        int n_stated = dice_evaluate(effect->dice, 0, AVERAGE, data, NULL);
        int n_actual = 0;
        int i;

        for (i = 0; e != NULL && i < n_stated; i++)
        {
            total += effect_avg_damage(e, data, name, shared_rv);
            ++n_actual;
            e = e->next;
        }

        /* Return an average of the sub-effects' average damages */
        return (n_actual > 0)? total / n_actual: 0;
    }

    if (shared_rv) return randcalc(*shared_rv, 0, AVERAGE);

    /* Non-random effect, calculate the average damage (be sure dice is defined) */
    if (effect_damages(effect, data, name) && (effect->dice != NULL))
        return dice_evaluate(effect->dice, 0, AVERAGE, data, NULL);

    return 0;
}


/*
 * Returns the projection of the effect, or an empty string if it has none.
 * Random effects only return a projection if all sub-effects have the same
 * projection.
 */
const char *effect_projection(const struct effect *effect, void *data)
{
    if (effect->index == EF_RANDOM)
    {
        /* Random effect */
        int num_subeffects = dice_evaluate(effect->dice, 0, AVERAGE, data, NULL);
        struct effect *e;
        const char *subeffect_proj;
        int i;

        /* Check if all subeffects have the same projection, and if not just give up on it */
        if (num_subeffects <= 0 || !effect->next) return "";

        e = effect->next;
        subeffect_proj = effect_projection(e, data);
        for (i = 0; e != NULL && i < num_subeffects; i++)
        {
            if (!streq(subeffect_proj, effect_projection(e, data))) return "";
            e = e->next;
        }

        return subeffect_proj;
    }

    if (projections[effect->subtype].desc != NULL)
    {
        /* Non-random effect, extract the projection if there is one */
        switch (base_descs[effect->index].efinfo_flag)
        {
            case EFINFO_BALL:
            case EFINFO_BOLTD:
            case EFINFO_BREATH:
                return projections[effect->subtype].desc;
        }
    }

    return "";
}


/*
 * Help effect_summarize_properties() and summarize_cure(): add one element
 * to the linked list of object properties.
 */
static void add_to_summaries(struct effect_object_property **summaries, int idx, int reslevel_min,
    int reslevel_max, enum effect_object_property_kind kind)
{
    struct effect_object_property *prop = mem_alloc(sizeof(*prop));

    prop->next = *summaries;
    prop->idx = idx;
    prop->reslevel_min = reslevel_min;
    prop->reslevel_max = reslevel_max;
    prop->kind = kind;
    *summaries = prop;
}


/*
 * Help effect_summarize_properties(): update the summaries for an effect that
 * acts like a cure.
 *
 * tmd Is the TMD_* index for the timed effect being cured.
 * summaries Is the pointer to the linked list of summaries.
 * unsummarized_count Is the count of unsummarized effects.
 */
static void summarize_cure(int tmd, struct effect_object_property **summaries,
    int *unsummarized_count)
{
    const struct timed_failure *f = timed_effects[tmd].fail;

    while (f)
    {
        if (f->code == TMD_FAIL_FLAG_OBJECT)
            add_to_summaries(summaries, f->idx, 0, 0, EFPROP_CURE_FLAG);
        else if (f->code == TMD_FAIL_FLAG_RESIST)
            add_to_summaries(summaries, f->idx, -1, 0, EFPROP_CURE_RESIST);
        else
            ++*unsummarized_count;
        f = f->next;
    }
}


/*
 * Return a summary of the object properties that match up with the effects in
 * an effect chain.
 *
 * ef Is the pointer to the first effect in the chain.
 * unsummarized_count If not NULL, *unsummarized_count will be set to
 * the count of effects in the chain that do something which can't be summarized
 * by an object property.
 *
 * Return a pointer to a linked list of the object properties implied
 * by the effect chain. When no longer needed, each element of that linked
 * list should be released with mem_free().
 */
struct effect_object_property *effect_summarize_properties(struct player *p, const struct effect *ef,
    int *unsummarized_count)
{
    int unsummarized = 0;
    struct effect_object_property *summaries = NULL;
    dice_t *remembered_dice = NULL;
    struct source actor_body;
    struct source *data = &actor_body;

    source_player(data, 0, p);

    for (; ef; ef = ef->next)
    {
        int value_this;

        switch (ef->index)
        {
            case EF_RANDOM:
            {
                /*
                 * For random or select effects, summarize all of the
                 * subeffects since any of them is possible. That's
                 * equivalent to simply skipping over the random or
                 * select effect and stepping one by one through what
                 * follows.
                 */
                break;
            }

            case EF_SET_VALUE:
            {
                /*
                 * Remember the value. Does nothing that should be
                 * remembered in the summaries or unsummarized count.
                 */
                remembered_dice = ef->dice;
                break;
            }

            case EF_CLEAR_VALUE:
            {
                /*
                 * Forget the value. Does nothing that should be
                 * remembered in the summaries or unsummarized count.
                 */
                remembered_dice = NULL;
                break;
            }

            case EF_CURE:
            {
                if (ef->subtype >= 0 && ef->subtype < TMD_MAX)
                    summarize_cure(ef->subtype, &summaries, &unsummarized);
                break;
            }

            case EF_TIMED_SET:
            {
                int value_max = (ef->dice? dice_evaluate(ef->dice, 0, MAXIMISE, data, NULL): 0);

                value_this = (remembered_dice?
                    dice_evaluate(remembered_dice, 0, MAXIMISE, data, NULL): value_max);
                if (value_this <= 0 && ef->subtype >= 0 && ef->subtype < TMD_MAX)
                {
                    /* It's equivalent to a cure. */
                    summarize_cure(ef->subtype, &summaries, &unsummarized);
                    break;
                }

                /* Fall through. */
            }

            case EF_TIMED_INC:
            case EF_TIMED_INC_NO_RES:
            {
                int value_max = (ef->dice? dice_evaluate(ef->dice, 0, MAXIMISE, data, NULL): 0);

                value_this = (remembered_dice?
                    dice_evaluate(remembered_dice, 0, MAXIMISE, data, NULL): value_max);
                if (value_this > 0 && ef->subtype >= 0 && ef->subtype < TMD_MAX)
                {
                    bool summarized = false;
                    const struct timed_failure *f;

                    if (timed_effects[ef->subtype].oflag_dup != OF_NONE)
                    {
                        add_to_summaries(&summaries, timed_effects[ef->subtype].oflag_dup, 0, 0,
                            timed_effects[ef->subtype].oflag_syn? EFPROP_OBJECT_FLAG_EXACT:
                                EFPROP_OBJECT_FLAG);
                        summarized = true;
                    }
                    if (timed_effects[ef->subtype].temp_resist >= 0)
                    {
                        int rmin = -1, rmax = 1;

                        f = timed_effects[ef->subtype].fail;
                        while (f)
                        {
                            if (f->idx == timed_effects[ef->subtype].temp_resist)
                            {
                                if (f->code == TMD_FAIL_FLAG_RESIST)
                                    rmax = MIN(rmax, 0);
                                else if (f->code == TMD_FAIL_FLAG_VULN)
                                    rmin = MAX(rmin, 0);
                            }
                            f = f->next;
                        }
                        add_to_summaries(&summaries, timed_effects[ef->subtype].temp_resist,
                            rmin, rmax, EFPROP_RESIST);
                        summarized = true;
                    }
                    f = timed_effects[ef->subtype].fail;
                    while (f)
                    {
                        switch (f->code)
                        {
                            case TMD_FAIL_FLAG_OBJECT:
                            {
                                add_to_summaries(&summaries, f->idx, 0, 0, EFPROP_CONFLICT_FLAG);
                                summarized = true;
                                break;
                            }

                            case TMD_FAIL_FLAG_RESIST:
                            {
                                if (f->idx != timed_effects[ef->subtype].temp_resist)
                                {
                                    add_to_summaries(&summaries, f->idx, -1, 0,
                                        EFPROP_CONFLICT_RESIST);
                                    summarized = true;
                                }
                                break;
                            }

                            case TMD_FAIL_FLAG_VULN:
                            {
                                if (f->idx != timed_effects[ef->subtype].temp_resist)
                                {
                                    add_to_summaries(&summaries, f->idx, 0, 3,
                                        EFPROP_CONFLICT_VULN);
                                    summarized = true;
                                }
                                break;
                            }
                        }
                        f = f->next;
                    }
                    if (timed_effects[ef->subtype].temp_brand >= 0)
                    {
                        add_to_summaries(&summaries, timed_effects[ef->subtype].temp_brand, 0, 0,
                            EFPROP_BRAND);
                        summarized = true;
                    }
                    if (timed_effects[ef->subtype].temp_slay >= 0)
                    {
                        add_to_summaries(&summaries, timed_effects[ef->subtype].temp_slay, 0, 0,
                            EFPROP_SLAY);
                        summarized = true;
                    }
                    if (!summarized) ++unsummarized;
                }
                break;
            }

            case EF_TIMED_DEC:
            {
                int value_max = (ef->dice? dice_evaluate(ef->dice, 0, MAXIMISE, data, NULL): 0);

                value_this = (remembered_dice?
                    dice_evaluate(remembered_dice, 0, MAXIMISE, data, NULL): value_max);

                /* If it decreases the duration, it's a partial cure. */
                if (value_this > 0)
                    summarize_cure(ef->subtype, &summaries, &unsummarized);

                break;
            }

            case EF_TELEPORT:
            case EF_TELEPORT_TO:
            case EF_TELEPORT_LEVEL:
            {
                add_to_summaries(&summaries, OF_NO_TELEPORT, 0, 0, EFPROP_CONFLICT_FLAG);
                break;
            }

            /*
             * There's other effects that have limited utility when the
             * object already has some flags:
             * DISABLE_TRAPS with OF_TRAP_IMMUNE is only good for unlocking
             * DETECT_INVISIBLE with OF_SEE_INVISIBLE or OF_TELEPATHY
             * RESTORE_x with OF_SUST_x
             * RESTORE_EXP with OF_HOLD_LIFE
             * For now, don't try to flag those.
             */
            default:
            {
                /* Everything else isn't related to an object property. */
                ++unsummarized;
                break;
            }
        }
    }

    if (unsummarized_count) *unsummarized_count = unsummarized;
    return summaries;
}
