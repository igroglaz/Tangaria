/*
 * File: effects.c
 * Purpose: Public effect and auxiliary functions for every effect in the game
 *
 * Copyright (c) 2007 Andi Sidwell
 * Copyright (c) 2016 Ben Semmler, Nick McConnell
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
 * Properties of effects
 */


/*
 * Useful things about effects.
 */
static const struct effect_kind effects[] =
{
    {EF_NONE, false, NULL, NULL, NULL},
    #define EFFECT(x, a, b, c, d, e) {EF_##x, a, b, effect_handler_##x, e},
    #include "list-effects.h"
    #undef EFFECT
    {EF_MAX, false, NULL, NULL, NULL}
};


static const char *effect_names[] =
{
    NULL,
    #define EFFECT(x, a, b, c, d, e) #x,
    #include "list-effects.h"
    #undef EFFECT
    NULL
};


/*
 * Utility functions
 */


static bool effect_valid(const struct effect *effect)
{
    if (!effect) return false;
    return ((effect->index > EF_NONE) && (effect->index < EF_MAX));
}


bool effect_aim(const struct effect *effect)
{
    const struct effect *e = effect;

    if (!effect_valid(effect)) return false;

    while (e)
    {
        if (effects[e->index].aim) return true;
        e = e->next;
    }

    return false;
}


const char *effect_info(const struct effect *effect, const char *name)
{
    if (!effect_valid(effect)) return NULL;

    /* Hack -- teleport other (show nothing) */
    if ((effect->index == EF_BOLT) && (effect->subtype == PROJ_AWAY_ALL)) return NULL;

    /* Hack -- non-explosive branded shots (show nothing) */
    if ((effect->index == EF_BOW_BRAND) && (effect->radius == 0)) return NULL;

    /* Hack -- non-damaging LOS effects (show nothing) */
    if ((effect->index == EF_PROJECT_LOS_AWARE) && (effect->other == 0)) return NULL;

    /* Hack -- illumination ("damage" value is used for radius, so change the tip accordingly) */
    if ((effect->index == EF_LIGHT_AREA) && streq(name, "elemental"))
        return "range";

    /* Hack -- mana drain ("damage" value is used for healing, so change the tip accordingly) */
    if ((effect->index == EF_BOLT_AWARE) && (effect->subtype == PROJ_DRAIN_MANA))
        return "heal";

    return effects[effect->index].info;
}


const char *effect_desc(const struct effect *effect)
{
    if (!effect_valid(effect)) return NULL;

    return effects[effect->index].desc;
}


effect_index effect_lookup(const char *name)
{
    size_t i;

    for (i = 0; i < N_ELEMENTS(effect_names); i++)
    {
        const char *effect_name = effect_names[i];

        /* Test for equality */
        if ((effect_name != NULL) && streq(name, effect_name)) return i;
    }

    return EF_MAX;
}


/*
 * Translate a string to an effect parameter subtype index
 */
int effect_subtype(int index, const char *type)
{
    int val = -1;

    /* Assign according to effect index */
    switch (index)
    {
        /* Projection name */
        case EF_ALTER:
        case EF_ARC:
        case EF_BALL:
        case EF_BALL_OBVIOUS:
        case EF_BEAM:
        case EF_BEAM_OBVIOUS:
        case EF_BLAST:
        case EF_BLAST_OBVIOUS:
        case EF_BOLT:
        case EF_BOLT_AWARE:
        case EF_BOLT_OR_BEAM:
        case EF_BOLT_STATUS:
        case EF_BOLT_STATUS_DAM:
        case EF_BOW_BRAND:
        case EF_BOW_BRAND_SHOT:
        case EF_BREATH:
        case EF_DAMAGE:
        case EF_DESTRUCTION:
        case EF_LASH:
        case EF_LINE:
        case EF_MELEE_BLOWS:
        case EF_PROJECT:
        case EF_PROJECT_LOS:
        case EF_PROJECT_LOS_AWARE:
        case EF_SHORT_BEAM:
        case EF_SPOT:
        case EF_STAR:
        case EF_STAR_BALL:
        case EF_STRIKE:
        case EF_SWARM:
        case EF_TOUCH:
        case EF_TOUCH_AWARE:
        {
            val = proj_name_to_idx(type);
            break;
        }

        /* Inscribe a glyph */
        case EF_GLYPH:
        {
            if (streq(type, "WARDING")) val = GLYPH_WARDING;
            else if (streq(type, "DECOY")) val = GLYPH_DECOY;
            break;
        }

        case EF_TELEPORT:
        case EF_TELEPORT_LEVEL:
        {
            if (streq(type, "NONE")) val = 0;
            else val = proj_name_to_idx(type);
            break;
        }

        /* Timed effect name */
        case EF_CURE:
        case EF_TIMED_DEC:
        case EF_TIMED_INC:
        case EF_TIMED_INC_NO_RES:
        case EF_TIMED_SET:
        {
            val = timed_name_to_idx(type);
            break;
        }

        /* Nourishment types */
        case EF_NOURISH:
        {
            if (streq(type, "INC_BY")) val = 0;
            else if (streq(type, "DEC_BY")) val = 1;
            else if (streq(type, "SET_TO")) val = 2;
            else if (streq(type, "INC_TO")) val = 3;
            break;
        }

        /* Monster timed effect name */
        case EF_MON_TIMED_INC:
        {
            val = mon_timed_name_to_idx(type);
            break;
        }

        /* Summon name */
        case EF_SUMMON:
        {
            val = summon_name_to_idx(type);
            break;
        }

        /* Stat name */
        case EF_DRAIN_STAT:
        case EF_GAIN_STAT:
        case EF_LOSE_STAT:        
        case EF_LOSE_RANDOM_STAT:
        case EF_RESTORE_STAT:
        {
            val = stat_name_to_idx(type);
            break;
        }

        /* Enchant type name - not worth a separate function */
        case EF_ENCHANT:
        {
            if (streq(type, "TOBOTH")) val = ENCH_TOBOTH;
            else if (streq(type, "TOHIT")) val = ENCH_TOHIT;
            else if (streq(type, "TODAM")) val = ENCH_TODAM;
            else if (streq(type, "TOAC")) val = ENCH_TOAC;
            break;
        }

        /* Targeted earthquake */
        case EF_EARTHQUAKE:
        {
            if (streq(type, "TARGETED")) val = 1;
            else if (streq(type, "NONE")) val = 0;
            break;
        }

        /* Allow monster teleport toward */
        case EF_TELEPORT_TO:
        {
            if (streq(type, "SELF")) val = 1;
            else if (streq(type, "NONE")) val = 0;
            break;
        }

        /* Some effects only want a radius, so this is a dummy */
        default:
        {
            if (streq(type, "NONE")) val = 0;
            break;
        }
    }

    return val;
}


/*
 * Execution of effects
 */


/*
 * Execute an effect chain.
 *
 * effect is the effect chain
 * origin is the origin of the effect (player, monster etc.)
 * ident  will be updated if the effect is identifiable
 *        (NB: no effect ever sets *ident to false)
 * aware  indicates whether the player is aware of the effect already
 * dir    is the direction the effect will go in
 * beam   is the base chance out of 100 that a BOLT_OR_BEAM effect will beam
 * boost  is the extent to which skill surpasses difficulty, used as % boost. It
 *        ranges from 0 to 138.
 */
bool effect_do(struct effect *effect, struct source *origin, bool *ident, bool aware, int dir,
    struct beam_info *beam, int boost, quark_t note, struct monster *target_mon)
{
    bool completed = false;
    effect_handler_f handler;
    struct worldpos wpos;

    /* Paranoia */
    if (origin->player) memcpy(&wpos, &origin->player->wpos, sizeof(wpos));
    else if (origin->monster) memcpy(&wpos, &origin->monster->wpos, sizeof(wpos));
    else if (target_mon) memcpy(&wpos, &target_mon->wpos, sizeof(wpos));
    else quit("No valid source in effect_do(). Please report this bug.");

    do
    {
        int random_choices = 0, leftover = 0;
        random_value value;

        if (!effect_valid(effect))
            quit("Bad effect passed to effect_do(). Please report this bug.");

        memset(&value, 0, sizeof(value));
        if (effect->dice != NULL)
            random_choices = dice_roll(effect->dice, (void *)origin, &value);

        /* Deal with special random effect */
        if (effect->index == EF_RANDOM)
        {
            int choice;

            /*
             * If it has no subeffects, act as if it completed
             * successfully and go to the next effect.
             */
            if (random_choices <= 0)
            {
                completed = true;
                effect = effect->next;
                continue;
            }

            choice = randint0(random_choices);
            leftover = random_choices - choice;

            /* Skip to the chosen effect */
            effect = effect->next;
            while (choice-- && effect) effect = effect->next;
            if (!effect)
            {
                /* There's fewer subeffects than expected. Act as if it ran successfully. */
                completed = true;
                break;
            }

            /* Roll the damage, if needed */
            memset(&value, 0, sizeof(value));
            if (effect->dice != NULL)
                dice_roll(effect->dice, (void *)origin, &value);
        }

        /* Handle the effect */
        handler = effects[effect->index].handler;
        if (handler != NULL)
        {
            effect_handler_context_t context;

            context.effect = effect->index;
            context.origin = origin;
            context.cave = chunk_get(&wpos);
            context.aware = aware;
            context.dir = dir;
            context.boost = boost;
            context.value = value;
            context.subtype = effect->subtype;
            context.radius = effect->radius;
            context.other = effect->other;
            context.y = effect->y;
            context.x = effect->x;
            context.self_msg = effect->self_msg;
            context.ident = *ident;
            context.note = note;
            context.flag = effect->flag;
            context.target_mon = target_mon;

            memset(&context.beam, 0, sizeof(context.beam));
            if (beam)
            {
                context.beam.beam = beam->beam;
                context.beam.spell_power = beam->spell_power;
                context.beam.elem_power = beam->elem_power;
                my_strcpy(context.beam.inscription, beam->inscription,
                    sizeof(context.beam.inscription));
            }

            completed = handler(&context);
            *ident = context.ident;

            /* PWMAngband: stop at the first non-handled effect */
            if (!completed) return false;

            /* PWMAngband: message if not already displayed */
            if (context.self_msg) msg(context.origin->player, context.self_msg);
        }
        else
            quit("Effect not handled. Please report this bug.");

        /* Get the next effect, if there is one */
        if (leftover)
        {
            /* Skip the remaining non-chosen effects */
            while (leftover-- && effect) effect = effect->next;
        }
        else
            effect = effect->next;
    }
    while (effect);

    return completed;
}


/*
 * Perform a single effect with a simple dice string and parameters
 * Calling with ident a valid pointer will (depending on effect) give success
 * information; ident = NULL will ignore this
 */
bool effect_simple(int index, struct source *origin, const char *dice_string, int subtype,
    int radius, int other, int y, int x, bool *ident)
{
    struct effect effect;
    int dir = 0;
    bool dummy_ident = false, result;

    /* Set all the values */
    memset(&effect, 0, sizeof(effect));
    effect.index = index;
    effect.dice = dice_new();
    dice_parse_string(effect.dice, dice_string);
    effect.subtype = subtype;
    effect.radius = radius;
    effect.other = other;
    effect.y = y;
    effect.x = x;

    /* Direction if needed (PWMAngband: simply use actual target) */
    if (effect_aim(&effect)) dir = DIR_TARGET;

    /* Do the effect */
    if (ident)
        result = effect_do(&effect, origin, ident, true, dir, NULL, 0, 0, NULL);
    else
        result = effect_do(&effect, origin, &dummy_ident, true, dir, NULL, 0, 0, NULL);

    dice_free(effect.dice);
    return result;
}
