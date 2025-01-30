/*
 * File: player-timed.c
 * Purpose: Timed effects handling
 *
 * Copyright (c) 1997 Ben Harrison
 * Copyright (c) 2007 Andi Sidwell
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


int PY_FOOD_MAX;
int PY_FOOD_FULL;
int PY_FOOD_HUNGRY;
int PY_FOOD_WEAK;
int PY_FOOD_FAINT;
int PY_FOOD_STARVE;


/*
 * Parsing functions for player_timed.txt
 */


static const char *list_player_flag_names[] =
{
    #define PF(a) #a,
    #include "../common/list-player-flags.h"
    #undef PF
    NULL
};


struct timed_effect_data timed_effects[] =
{
    #define TMD(a, b, c) {#a, b, c, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, NULL, NULL, NULL, 0, 0, OF_NONE, false, -1, -1, -1},
    #include "../common/list-player-timed.h"
    #undef TMD
    {"MAX", 0, 0, NULL, NULL, NULL, NULL, NULL, NULL, 0, NULL, NULL, NULL, NULL, 0, 0, OF_NONE, false, -1, -1, -1}
};


int timed_name_to_idx(const char *name)
{
    size_t i;

    for (i = 0; i < N_ELEMENTS(timed_effects); i++)
    {
        if (!my_stricmp(name, timed_effects[i].name)) return i;
    }

    return -1;
}


/*
 * List of timed effect names
 */
static const char *list_timed_effect_names[] =
{
    #define TMD(a, b, c) #a,
    #include "../common/list-player-timed.h"
    #undef TMD
    "MAX"
};


static enum parser_error parse_player_timed_name(struct parser *p)
{
    struct timed_effect_parse_state *ps = (struct timed_effect_parse_state*)parser_priv(p);
    const char *name = parser_getstr(p, "name");
    int index;

    if (grab_name("timed effect", name, list_timed_effect_names,
        N_ELEMENTS(list_timed_effect_names), &index))
    {
        return PARSE_ERROR_INVALID_SPELL_NAME;
    }

    my_assert(ps);
    ps->t = &timed_effects[index];
    ps->e = NULL;

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_player_timed_desc(struct parser *p)
{
    struct timed_effect_parse_state *ps = (struct timed_effect_parse_state*)parser_priv(p);

    my_assert(ps);
    if (!ps->t) return PARSE_ERROR_MISSING_RECORD_HEADER;
    ps->t->desc = string_append(ps->t->desc, parser_getstr(p, "desc"));
    return PARSE_ERROR_NONE;
}


static enum parser_error parse_player_timed_end_message(struct parser *p)
{
    struct timed_effect_parse_state *ps = (struct timed_effect_parse_state*)parser_priv(p);

    my_assert(ps);
    if (!ps->t) return PARSE_ERROR_MISSING_RECORD_HEADER;
    ps->t->on_end = string_append(ps->t->on_end, parser_getstr(p, "text"));
    return PARSE_ERROR_NONE;
}


static enum parser_error parse_player_timed_increase_message(struct parser *p)
{
    struct timed_effect_parse_state *ps = (struct timed_effect_parse_state*)parser_priv(p);

    my_assert(ps);
    if (!ps->t) return PARSE_ERROR_MISSING_RECORD_HEADER;
    ps->t->on_increase = string_append(ps->t->on_increase, parser_getstr(p, "text"));
    return PARSE_ERROR_NONE;
}


static enum parser_error parse_player_timed_decrease_message(struct parser *p)
{
    struct timed_effect_parse_state *ps = (struct timed_effect_parse_state*)parser_priv(p);

    my_assert(ps);
    if (!ps->t) return PARSE_ERROR_MISSING_RECORD_HEADER;
    ps->t->on_decrease = string_append(ps->t->on_decrease, parser_getstr(p, "text"));
    return PARSE_ERROR_NONE;
}


static enum parser_error parse_player_timed_nbegin_message(struct parser *p)
{
    struct timed_effect_parse_state *ps = (struct timed_effect_parse_state*)parser_priv(p);

    my_assert(ps);
    if (!ps->t) return PARSE_ERROR_MISSING_RECORD_HEADER;
    ps->t->near_begin = string_append(ps->t->near_begin, parser_getstr(p, "text"));
    return PARSE_ERROR_NONE;
}


static enum parser_error parse_player_timed_nend_message(struct parser *p)
{
    struct timed_effect_parse_state *ps = (struct timed_effect_parse_state*)parser_priv(p);

    my_assert(ps);
    if (!ps->t) return PARSE_ERROR_MISSING_RECORD_HEADER;
    ps->t->near_end = string_append(ps->t->near_end, parser_getstr(p, "text"));
    return PARSE_ERROR_NONE;
}


static enum parser_error parse_player_timed_message_type(struct parser *p)
{
    struct timed_effect_parse_state *ps = (struct timed_effect_parse_state*)parser_priv(p);

    my_assert(ps);
    if (!ps->t) return PARSE_ERROR_MISSING_RECORD_HEADER;
    ps->t->msgt = message_lookup_by_name(parser_getsym(p, "type"));
    return ((ps->t->msgt < 0)? PARSE_ERROR_INVALID_MESSAGE: PARSE_ERROR_NONE);
}


static enum parser_error parse_player_timed_fail(struct parser *p)
{
    struct timed_effect_parse_state *ps = (struct timed_effect_parse_state*)parser_priv(p);
    const char *name = parser_getstr(p, "flag");
    struct timed_failure *f;
    int code, idx;

    my_assert(ps);
    if (!ps->t) return PARSE_ERROR_MISSING_RECORD_HEADER;

    code = parser_getuint(p, "code");
    switch (code)
    {
        case TMD_FAIL_FLAG_OBJECT:
        {
            idx = lookup_flag(list_obj_flag_names, name);
            if (idx == FLAG_END) return PARSE_ERROR_INVALID_FLAG;
            break;
        }

        case TMD_FAIL_FLAG_PLAYER:
        {
            idx = lookup_flag(list_player_flag_names, name);
            if (idx == FLAG_END) return PARSE_ERROR_INVALID_FLAG;
            break;
        }

        case TMD_FAIL_FLAG_RESIST:
        case TMD_FAIL_FLAG_VULN:
        {
            idx = 0;
            while (list_element_names[idx] && !streq(list_element_names[idx], name)) idx++;
            if (idx == ELEM_MAX) return PARSE_ERROR_INVALID_FLAG;
            break;
        }

        case TMD_FAIL_FLAG_TIMED_EFFECT:
        {
            if (grab_name("timed effect", name, list_timed_effect_names,
                N_ELEMENTS(list_timed_effect_names), &idx))
            {
                return PARSE_ERROR_INVALID_FLAG;
            }
            break;
        }

        default: return PARSE_ERROR_INVALID_FLAG;
    }

    f = mem_alloc(sizeof(*f));
    f->next = ps->t->fail;
    f->code = code;
    f->idx = idx;
    ps->t->fail = f;

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_player_timed_grade(struct parser *p)
{
    struct timed_effect_parse_state *ps = (struct timed_effect_parse_state*)parser_priv(p);
    const char *color = parser_getsym(p, "color");
    int grade_max = parser_getint(p, "max");
    struct timed_grade *current, *l;
    int attr, food_scl;

    my_assert(ps);
    if (!ps->t) return PARSE_ERROR_MISSING_RECORD_HEADER;

    /*
     * The maximum has to be greater than zero so it doesn't interfere
     * with the implicit "off" grade which has a maximum of 0. Because
     * a timed effect duration is stored as an int16_t in struct player,
     * also guarantee that the maximum is compatible with that.
     */
    food_scl = (streq(ps->t->name, "FOOD"))? (int)z_info->food_value: 1;
    if (grade_max <= 0 || grade_max > 32767 / food_scl)
        return PARSE_ERROR_INVALID_VALUE;

    if (strlen(color) > 1) attr = color_text_to_attr(color);
    else attr = color_char_to_attr(color[0]);
    if (attr < 0) return PARSE_ERROR_INVALID_COLOR;

    /* Make a zero grade structure if there isn't one */
    current = ps->t->grade;
    if (!current)
    {
        ps->t->grade = mem_zalloc(sizeof(struct timed_grade));
        current = ps->t->grade;
    }

    /* Move to the highest grade so far */
    while (current->next)
    {
        current = current->next;

        /* Enforce that the grades appear in ascending order. */
        if (grade_max * food_scl <= current->max)
            return PARSE_ERROR_INVALID_VALUE;
    }

    /* Add the new one */
    l = mem_zalloc(sizeof(*l));
    current->next = l;
    l->grade = current->grade + 1;
    l->color = attr;
    l->max = grade_max;
    l->name = string_make(parser_getsym(p, "name"));
    l->up_msg = string_make(parser_getsym(p, "up_msg"));
    if (parser_hasval(p, "down_msg"))
        l->down_msg  = string_make(parser_getsym(p, "down_msg"));

    /* Set food constants and deal with percentages */
    if (food_scl != 1)
    {
        l->max *= food_scl;
        if (l->name)
        {
            if (streq(l->name, "Starving")) PY_FOOD_STARVE = l->max;
            else if (streq(l->name, "Faint")) PY_FOOD_FAINT = l->max;
            else if (streq(l->name, "Weak")) PY_FOOD_WEAK = l->max;
            else if (streq(l->name, "Hungry")) PY_FOOD_HUNGRY = l->max;
            else if (streq(l->name, "Fed")) PY_FOOD_FULL = l->max;
            else if (streq(l->name, "Full")) PY_FOOD_MAX = l->max;
        }
    }

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_player_timed_resist(struct parser *p)
{
    struct timed_effect_parse_state *ps = (struct timed_effect_parse_state*)parser_priv(p);
    const char *name = parser_getsym(p, "elem");
    int idx = (name? proj_name_to_idx(name): -1);

    my_assert(ps);
    if (!ps->t) return PARSE_ERROR_MISSING_RECORD_HEADER;
    if (idx < 0 || idx >= ELEM_MAX) return PARSE_ERROR_INVALID_VALUE;
    ps->t->temp_resist = idx;
    return PARSE_ERROR_NONE;
}


static enum parser_error parse_player_timed_brand(struct parser *p)
{
    struct timed_effect_parse_state *ps = (struct timed_effect_parse_state*)parser_priv(p);
    const char *name = parser_getsym(p, "name");
    int idx = z_info->brand_max;

    my_assert(ps);
    if (!ps->t) return PARSE_ERROR_MISSING_RECORD_HEADER;
    if (name)
    {
        for (idx = 0; idx < z_info->brand_max; ++idx)
        {
            if (streq(name, brands[idx].code)) break;
        }
    }
    if (idx == z_info->brand_max) return PARSE_ERROR_UNRECOGNISED_BRAND;
    ps->t->temp_brand = idx;
    return PARSE_ERROR_NONE;
}


static enum parser_error parse_player_timed_slay(struct parser *p)
{
    struct timed_effect_parse_state *ps = (struct timed_effect_parse_state*)parser_priv(p);
    const char *name = parser_getsym(p, "name");
    int idx = z_info->slay_max;

    my_assert(ps);
    if (!ps->t) return PARSE_ERROR_MISSING_RECORD_HEADER;
    if (name)
    {
        for (idx = 0; idx < z_info->slay_max; ++idx)
        {
            if (streq(name, slays[idx].code)) break;
        }
    }
    if (idx == z_info->slay_max) return PARSE_ERROR_UNRECOGNISED_BRAND;
    ps->t->temp_slay = idx;
    return PARSE_ERROR_NONE;
}


static enum parser_error parse_player_timed_flag_synonym(struct parser *p)
{
    struct timed_effect_parse_state *ps = (struct timed_effect_parse_state*)parser_priv(p);
    const char *code = parser_getsym(p, "code");
    bool is_exact = parser_getint(p, "exact") != 0;
    int idx = (code? code_index_in_array(list_obj_flag_names, code): OF_NONE);

    my_assert(ps);
    if (!ps->t) return PARSE_ERROR_MISSING_RECORD_HEADER;
    if (idx <= OF_NONE) return PARSE_ERROR_INVALID_OBJ_PROP_CODE;
    ps->t->oflag_dup = idx;
    ps->t->oflag_syn = is_exact;
    return PARSE_ERROR_NONE;
}


static enum parser_error parse_player_timed_on_begin_effect(struct parser *p)
{
    struct timed_effect_parse_state *ps = (struct timed_effect_parse_state*)parser_priv(p);
    struct effect *effect;

    my_assert(ps);
    if (!ps->t) return PARSE_ERROR_MISSING_RECORD_HEADER;

    /* Go to the next vacant effect and set it to the new one. */
    ps->e = mem_zalloc(sizeof(*effect));
    if (ps->t->on_begin_effect)
    {
        effect = ps->t->on_begin_effect;
        while (effect->next) effect = effect->next;
        effect->next = ps->e;
    }
    else
        ps->t->on_begin_effect = ps->e;

    /* Fill in the details. */
    return grab_effect_data(p, ps->e);
}


static enum parser_error parse_player_timed_on_end_effect(struct parser *p)
{
    struct timed_effect_parse_state *ps = (struct timed_effect_parse_state*)parser_priv(p);
    struct effect *effect;

    my_assert(ps);
    if (!ps->t) return PARSE_ERROR_MISSING_RECORD_HEADER;

    /* Go to the next vacant effect and set it to the new one */
    ps->e = mem_zalloc(sizeof(*effect));
    if (ps->t->on_end_effect)
    {
        effect = ps->t->on_end_effect;
        while (effect->next) effect = effect->next;
        effect->next = ps->e;
    }
    else
        ps->t->on_end_effect = ps->e;

    /* Fill in the detail */
    return grab_effect_data(p, ps->e);
}


static enum parser_error parse_player_timed_effect_yx(struct parser *p)
{
    struct timed_effect_parse_state *ps = (struct timed_effect_parse_state*)parser_priv(p);

    my_assert(ps);
    if (!ps->e) return PARSE_ERROR_MISSING_RECORD_HEADER;
    ps->e->y = parser_getint(p, "y");
    ps->e->x = parser_getint(p, "x");

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_player_timed_effect_dice(struct parser *p)
{
    struct timed_effect_parse_state *ps = (struct timed_effect_parse_state*)parser_priv(p);
    dice_t *dice;
    const char *string;

    my_assert(ps);
    if (!ps->e) return PARSE_ERROR_MISSING_RECORD_HEADER;
    dice = dice_new();
    if (dice == NULL)
        return PARSE_ERROR_INVALID_DICE;
    string = parser_getstr(p, "dice");
    if (dice_parse_string(dice, string))
    {
        dice_free(ps->e->dice);
        ps->e->dice = dice;
    }
    else
    {
        dice_free(dice);
        return PARSE_ERROR_INVALID_DICE;
    }

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_player_timed_effect_expr(struct parser *p)
{
    struct timed_effect_parse_state *ps = (struct timed_effect_parse_state*)parser_priv(p);
    expression_t *expression;
    expression_base_value_f function;
    const char *name, *base, *expr;
    enum parser_error result;

    my_assert(ps);
    if (!ps->e || !ps->e->dice)
        return PARSE_ERROR_MISSING_RECORD_HEADER;
    name = parser_getsym(p, "name");
    base = parser_getsym(p, "base");
    expr = parser_getstr(p, "expr");
    expression = expression_new();
    if (expression == NULL)
        return PARSE_ERROR_INVALID_EXPRESSION;
    function = effect_value_base_by_name(base);
    expression_set_base_value(expression, function);
    if (expression_add_operations_string(expression, expr) < 0)
        result = PARSE_ERROR_BAD_EXPRESSION_STRING;
    else if (dice_bind_expression(ps->e->dice, name, expression) < 0)
        result = PARSE_ERROR_UNBOUND_EXPRESSION;
    else
        result = PARSE_ERROR_NONE;

    /*
     * The dice object makes a deep copy of the expression, so we can
     * free it.
     */
    expression_free(expression);

    return result;
}


static enum parser_error parse_player_timed_effect_msg(struct parser *p)
{
    struct timed_effect_parse_state *ps = (struct timed_effect_parse_state*)parser_priv(p);

    my_assert(ps);
    if (!ps->e) return PARSE_ERROR_MISSING_RECORD_HEADER;
    ps->e->self_msg = string_append(ps->e->self_msg, parser_getstr(p, "text"));

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_player_timed_effect_flags(struct parser *p)
{
    struct timed_effect_parse_state *ps = (struct timed_effect_parse_state*)parser_priv(p);
    char *flags, *s;

    my_assert(ps);
    if (!ps->t) return PARSE_ERROR_MISSING_RECORD_HEADER;
    if (!parser_hasval(p, "flags"))
        return PARSE_ERROR_NONE;
    flags = string_make(parser_getstr(p, "flags"));
    s = strtok(flags, " |");
    while (s)
    {
        if (streq(s, "NONSTACKING"))
            ps->t->flags |= TMD_FLAG_NONSTACKING;
        else
        {
            string_free(flags);
            return PARSE_ERROR_INVALID_FLAG;
        }
        s = strtok(NULL, " |");
    }

    string_free(flags);
    return PARSE_ERROR_NONE;
}


static enum parser_error parse_player_timed_effect_lower_bound(struct parser *p)
{
    struct timed_effect_parse_state *ps = (struct timed_effect_parse_state*)parser_priv(p);
    int bound = parser_getint(p, "bound");

    my_assert(ps);
    if (!ps->t) return PARSE_ERROR_MISSING_RECORD_HEADER;

    /*
     * Don't allow negative lower bounds (breaks the logic for testing
     * whether a timed effect is active). Also, since int16_t is used to
     * store a timed effect's duration in struct player, don't allow
     * lower bounds that aren't compatible with that.
     */
    if (bound < 0 || bound > 32767)
        return PARSE_ERROR_INVALID_VALUE;
    ps->t->lower_bound = bound;

    return PARSE_ERROR_NONE;
}


static struct parser *init_parse_player_timed(void)
{
    struct parser *p = parser_new();
    struct timed_effect_parse_state *ps = mem_zalloc(sizeof(*ps));

    parser_setpriv(p, ps);
    parser_reg(p, "name str name", parse_player_timed_name);
    parser_reg(p, "desc str desc", parse_player_timed_desc);
    parser_reg(p, "on-end str text", parse_player_timed_end_message);
    parser_reg(p, "on-increase str text", parse_player_timed_increase_message);
    parser_reg(p, "on-decrease str text", parse_player_timed_decrease_message);
    parser_reg(p, "near-begin str text", parse_player_timed_nbegin_message);
    parser_reg(p, "near-end str text", parse_player_timed_nend_message);
    parser_reg(p, "msgt sym type", parse_player_timed_message_type);
    parser_reg(p, "fail uint code str flag", parse_player_timed_fail);
    parser_reg(p, "grade sym color int max sym name sym up_msg ?sym down_msg",
        parse_player_timed_grade);
    parser_reg(p, "resist sym elem", parse_player_timed_resist);
    parser_reg(p, "brand sym name", parse_player_timed_brand);
    parser_reg(p, "slay sym name", parse_player_timed_slay);
    parser_reg(p, "flag-synonym sym code int exact", parse_player_timed_flag_synonym);
    parser_reg(p, "on-begin-effect sym eff ?sym type ?int radius ?int other",
        parse_player_timed_on_begin_effect);
    parser_reg(p, "on-end-effect sym eff ?sym type ?int radius ?int other",
        parse_player_timed_on_end_effect);
    parser_reg(p, "effect-yx int y int x", parse_player_timed_effect_yx);
    parser_reg(p, "effect-dice str dice", parse_player_timed_effect_dice);
    parser_reg(p, "effect-expr sym name sym base str expr", parse_player_timed_effect_expr);
    parser_reg(p, "effect-msg str text", parse_player_timed_effect_msg);
    parser_reg(p, "flags ?str flags", parse_player_timed_effect_flags);
    parser_reg(p, "lower-bound int bound", parse_player_timed_effect_lower_bound);
    return p;
}


static errr run_parse_player_timed(struct parser *p)
{
    return parse_file_quit_not_found(p, "player_timed");
}


static errr finish_parse_player_timed(struct parser *p)
{
    struct timed_effect_parse_state *ps = (struct timed_effect_parse_state*)parser_priv(p);

    mem_free(ps);
    parser_destroy(p);
    return 0;
}


static void cleanup_player_timed(void)
{
    size_t i;

    /*
     * Besides cleaning up any dynamically allocated resources, revert
     * any fields set during parsing to their default values so the
     * timed_effects array is back to where it started after its static
     * initialization.
     */
    for (i = 0; i < TMD_MAX; i++)
    {
        struct timed_effect_data *effect = &timed_effects[i];
        struct timed_failure *f = effect->fail;
        struct timed_grade *grade = effect->grade;

        while (f)
        {
            struct timed_failure *next = f->next;

            mem_free(f);
            f = next;
        }
        effect->fail = NULL;

        while (grade)
        {
            struct timed_grade *next = grade->next;

            string_free(grade->name);
            string_free(grade->up_msg);
            string_free(grade->down_msg);
            mem_free(grade);
            grade = next;
        }
        effect->grade = NULL;

        string_free(effect->desc);
        effect->desc = NULL;
        string_free(effect->on_end);
        effect->on_end = NULL;
        string_free(effect->on_increase);
        effect->on_increase = NULL;
        string_free(effect->on_decrease);
        effect->on_decrease = NULL;
        string_free(effect->near_begin);
        effect->near_begin = NULL;
        string_free(effect->near_end);
        effect->near_end = NULL;
        effect->msgt = 0;

        free_effect(effect->on_begin_effect);
        effect->on_begin_effect = NULL;
        free_effect(effect->on_end_effect);
        effect->on_end_effect = NULL;

        effect->flags = 0;
        effect->lower_bound = 0;
        effect->oflag_dup = OF_NONE;
        effect->oflag_syn = false;
        effect->temp_resist = -1;
        effect->temp_brand = -1;
        effect->temp_slay = -1;
    }
}


struct file_parser player_timed_parser =
{
    "player timed effects",
    init_parse_player_timed,
    run_parse_player_timed,
    finish_parse_player_timed,
    cleanup_player_timed
};


/*
 * Set "p->timed[TMD_BOWBRAND]", notice observable changes
 */
static bool set_bow_brand(struct player *p, int v)
{
    bool notice = false;

    /* Open */
    if (v)
    {
        if (!p->timed[TMD_BOWBRAND])
        {
            switch (p->brand.type)
            {
                case PROJ_ELEC:
                {
                    if (p->brand.blast)
                    {
                        msg_misc(p, "'s missiles glow deep blue.");
                        msg(p, "Your missiles glow deep blue!");
                    }
                    else
                    {
                        msg_misc(p, "'s missiles are covered with lightning.");
                        msg(p, "Your missiles are covered with lightning!");
                    }
                    break;
                }
                case PROJ_COLD:
                {
                    if (p->brand.blast)
                    {
                        msg_misc(p, "'s missiles glow bright white.");
                        msg(p, "Your missiles glow bright white!");
                    }
                    else
                    {
                        msg_misc(p, "'s missiles are covered with frost.");
                        msg(p, "Your missiles are covered with frost!");
                    }
                    break;
                }
                case PROJ_FIRE:
                {
                    if (p->brand.blast)
                    {
                        msg_misc(p, "'s missiles glow deep red.");
                        msg(p, "Your missiles glow deep red!");
                    }
                    else
                    {
                        msg_misc(p, "'s missiles are covered with fire.");
                        msg(p, "Your missiles are covered with fire!");
                    }
                    break;
                }
                case PROJ_ACID:
                {
                    if (p->brand.blast)
                    {
                        msg_misc(p, "'s missiles glow pitch black.");
                        msg(p, "Your missiles glow pitch black!");
                    }
                    else
                    {
                        msg_misc(p, "'s missiles are covered with acid.");
                        msg(p, "Your missiles are covered with acid!");
                    }
                    break;
                }
                case PROJ_MON_CONF:
                    msg_misc(p, "'s missiles glow many colors.");
                    msg(p, "Your missiles glow many colors!");
                    break;
                case PROJ_POIS:
                    msg_misc(p, "'s missiles are covered with venom.");
                    msg(p, "Your missiles are covered with venom!");
                    break;
                case PROJ_ARROW:
                    msg_misc(p, "'s missiles sharpen.");
                    msg(p, "Your missiles sharpen!");
                    break;
                case PROJ_SHARD:
                    msg_misc(p, "'s missiles become explosive.");
                    msg(p, "Your missiles become explosive!");
                    break;
                case PROJ_MISSILE:
                    msg_misc(p, "'s missiles glow with power.");
                    msg(p, "Your missiles glow with power!");
                    break;
                case PROJ_SOUND:
                    msg_misc(p, "'s missiles vibrate in a strange way.");
                    msg(p, "Your missiles vibrate in a strange way!");
                    break;
            }
            notice = true;
        }
    }

    /* Shut */
    else
    {
        if (p->timed[TMD_BOWBRAND])
        {
            msg_misc(p, "'s missiles seem normal again.");
            msg(p, "Your missiles seem normal again.");
            notice = true;
        }
    }

    /* Use the value */
    p->timed[TMD_BOWBRAND] = v;

    /* Nothing to notice */
    if (!notice) return false;

    /* Disturb */
    disturb(p, 0);

    /* Redraw the "brand" */
    p->upkeep->redraw |= (PR_STATUS);

    /* Handle stuff */
    handle_stuff(p);

    /* Result */
    return true;
}


/*
 * Set a timed event permanently.
 */
static bool player_set_timed_perma(struct player *p, int idx)
{
    struct timed_effect_data *effect;
    bool notify = true;
    struct timed_grade *current_grade;

    /* No change */
    if (p->timed[idx] == -1) return false;

    /* Don't mention some effects */
    if ((idx == TMD_OPP_ACID) && player_is_immune(p, ELEM_ACID)) notify = false;
    if ((idx == TMD_OPP_ELEC) && player_is_immune(p, ELEM_ELEC)) notify = false;
    if ((idx == TMD_OPP_FIRE) && player_is_immune(p, ELEM_FIRE)) notify = false;
    if ((idx == TMD_OPP_COLD) && player_is_immune(p, ELEM_COLD)) notify = false;

    /* Find the effect */
    effect = &timed_effects[idx];
    current_grade = effect->grade;
    while (current_grade->next) current_grade = current_grade->next;

    /* Turning on, always mention */
    if (p->timed[idx] == 0)
    {
        msg_misc(p, effect->near_begin);
        msgt(p, effect->msgt, current_grade->up_msg);
        notify = true;
    }

    /* Use the value */
    p->timed[idx] = -1;

    /* Nothing to notice */
    if (!notify) return false;

    /* Disturb */
    disturb(p, 0);

    /* Reveal hidden players */
    if (p->k_idx) aware_player(p, p);

    /* Update the visuals, as appropriate. */
    p->upkeep->update |= effect->flag_update;
    p->upkeep->redraw |= effect->flag_redraw;

    /* Handle stuff */
    handle_stuff(p);

    /* Result */
    return true;
}


/*
 * Set "p->timed[TMD_ADRENALINE]", notice observable changes
 * Note the interaction with biofeedback
 */
static bool set_adrenaline(struct player *p, int v)
{
    int old_aux = 0, new_aux = 0;
    bool notice = false;

    /* Limit duration (100 turns / 20 turns at 5th stage) */
    if (v > 100)
    {
        int dam;

        v = 100;
        dam = damroll(2, v);
        dam = player_apply_damage_reduction(p, dam, false, "adrenaline poisoning");

        /* Too much adrenaline causes damage */
        msg(p, "Your body can't handle that much adrenaline!");
        if (dam && OPT(p, show_damage))
            msg(p, "You take $r%d^r damage.", dam);
        take_hit(p, dam, "adrenaline poisoning", "had a heart attack due to too much adrenaline");
        notice = true;
    }

    /* Different stages */
    if (p->timed[TMD_ADRENALINE] > 0) old_aux = 1 + (p->timed[TMD_ADRENALINE] - 1) / 20;
    if (v > 0) new_aux = 1 + (v - 1) / 20;

    /* Increase stage */
    if (new_aux > old_aux)
    {
        /* Describe the state */
        switch (new_aux)
        {
            /* Berserk strength effect */
            case 1:
            {
                msg_misc(p, "'s veins are flooded with adrenaline.");
                msg(p, "Adrenaline surges through your veins!");
                hp_player(p, 30);
                player_clear_timed(p, TMD_AFRAID, true);
                player_set_timed_perma(p, TMD_BOLD);
                player_set_timed_perma(p, TMD_SHERO);

                /* Adrenaline doesn't work well when biofeedback is activated */
                if (p->timed[TMD_BIOFEEDBACK])
                {
                    int dam = damroll(2, v);

                    dam = player_apply_damage_reduction(p, dam, false, "adrenaline poisoning");
                    if (dam && OPT(p, show_damage))
                        msg(p, "You take $r%d^r damage.", dam);
                    player_clear_timed(p, TMD_BIOFEEDBACK, true);
                    take_hit(p, dam, "adrenaline poisoning",
                        "had a heart attack due to too much adrenaline");
                }
                break;
            }

            /* Increase Str/Dex/Con */
            case 2:
            {
                msg_misc(p, "feels powerful.");
                msg(p, "You feel powerful!");
                break;
            }

            /* Increase Str/Dex/Con + increase to-dam */
            case 3:
            {
                msg_misc(p, "feels more powerful.");
                msg(p, "You feel more powerful!");
                msg_misc(p, "'s hands glow red.");
                msg(p, "Your hands glow red!");
                break;
            }

            /* Increase Str/Dex/Con + increase attack speed */
            case 4:
            {
                msg_misc(p, "feels more powerful.");
                msg(p, "You feel more powerful!");
                msg_misc(p, "'s hands tingle.");
                msg(p, "Your hands tingle!");
                break;
            }

            /* Increase Str/Dex/Con + haste effect */
            case 5:
            {
                msg_misc(p, "feels more powerful.");
                msg(p, "You feel more powerful!");
                player_set_timed_perma(p, TMD_FAST);
                break;
            }
        }

        /* Notice */
        notice = true;
    }

    /* Decrease stage */
    else if (new_aux < old_aux)
    {
        /* Describe the state */
        switch (new_aux)
        {
            /* None */
            case 0:
            {
                msg_misc(p, "'s veins are drained of adrenaline.");
                msg(p, "The adrenaline drains out of your veins.");
                player_clear_timed(p, TMD_BOLD, true);
                player_clear_timed(p, TMD_SHERO, true);
                break;
            }

            /* Decrease Str/Dex/Con */
            case 1:
            {
                msg_misc(p, "feels less powerful.");
                msg(p, "You feel less powerful.");
                break;
            }

            /* Decrease Str/Dex/Con + decrease to-dam */
            case 2:
            {
                msg_misc(p, "feels less powerful.");
                msg(p, "You feel less powerful.");
                msg_misc(p, "'s hands stop glowing.");
                msg(p, "Your hands stop glowing.");
                break;
            }

            /* Decrease Str/Dex/Con + decrease attack speed */
            case 3:
            {
                msg_misc(p, "feels more powerful.");
                msg(p, "You feel more powerful!");
                msg_misc(p, "'s hands ache.");
                msg(p, "Your hands ache.");
                break;
            }

            /* Decrease Str/Dex/Con + lose haste effect */
            case 4:
            {
                msg_misc(p, "feels less powerful.");
                msg(p, "You feel less powerful.");
                player_clear_timed(p, TMD_FAST, true);
                break;
            }
        }

        /* Notice */
        notice = true;
    }

    /* Use the value */
    p->timed[TMD_ADRENALINE] = v;

    /* Nothing to notice */
    if (!notice) return false;

    /* Notice */
    p->upkeep->update |= (PU_BONUS);

    /* Disturb */
    disturb(p, 0);

    /* Redraw the "adrenaline" */
    p->upkeep->redraw |= (PR_STATUS);

    /* Handle stuff */
    handle_stuff(p);

    /* Result */
    return true;
}


/*
 * Set "p->timed[TMD_BIOFEEDBACK]", notice observable changes
 * Note the interaction with adrenaline
 */
static bool set_biofeedback(struct player *p, int v)
{
    bool notice = false;

    /* Open */
    if (v)
    {
        if (!p->timed[TMD_BIOFEEDBACK])
        {
            msg_misc(p, "'s pulse slows down.");
            msg(p, "Your pulse slows down!");

            /* Biofeedback doesn't work well when adrenaline is activated */
            if (p->timed[TMD_ADRENALINE])
            {
                player_clear_timed(p, TMD_ADRENALINE, true);
                if (one_in_(8))
                {
                    msg(p, "You feel weak and tired!");
                    player_inc_timed(p, TMD_SLOW, randint0(4) + 4, true, false);
                    if (one_in_(5))
                        player_inc_timed(p, TMD_PARALYZED, randint0(4) + 4, true, false);
                    if (one_in_(3)) player_inc_timed(p, TMD_STUN, randint1(30), true, false);
                }
            }
            notice = true;
        }

        /* Biofeedback can't reach high values */
        if (v > 35 + p->lev)
        {
            msg(p, "You speed up your pulse to avoid fainting!");
            v = 35 + p->lev;
            notice = true;
        }
    }

    /* Shut */
    else
    {
        if (p->timed[TMD_BIOFEEDBACK])
        {
            msg_misc(p, "'s pulse speeds up.");
            msg(p, "You lose control of your blood flow.");
            notice = true;
        }

    }

    /* Use the value */
    p->timed[TMD_BIOFEEDBACK] = v;

    /* Nothing to notice */
    if (!notice) return false;

    /* Notice */
    p->upkeep->update |= (PU_BONUS);

    /* Disturb */
    disturb(p, 0);

    /* Redraw the "biofeedback" */
    p->upkeep->redraw |= (PR_STATUS);

    /* Handle stuff */
    handle_stuff(p);

    /* Result */
    return true;
}


/*
 * Set "p->timed[TMD_HARMONY]", notice observable changes
 */
static bool set_harmony(struct player *p, int v)
{
    int old_aux = 0, new_aux = 0;
    bool notice = false;

    /* Limit duration (100 turns / 20 turns at 5th stage) */
    if (v > 100) v = 100;

    /* Different stages */
    if (p->timed[TMD_HARMONY] > 0) old_aux = 1 + (p->timed[TMD_HARMONY] - 1) / 20;
    if (v > 0) new_aux = 1 + (v - 1) / 20;

    /* Increase stage */
    if (new_aux > old_aux)
    {
        /* Describe the state */
        switch (new_aux)
        {
            /* Bless effect */
            case 1:
            {
                msg_misc(p, "feels attuned to nature.");
                msg(p, "You feel attuned to nature!");
                player_set_timed_perma(p, TMD_BLESSED);
                break;
            }

            /* Increase Str/Dex/Con */
            case 2:
            {
                msg_misc(p, "feels powerful.");
                msg(p, "You feel powerful!");
                break;
            }

            /* Increase Str/Dex/Con + shield effect */
            case 3:
            {
                msg_misc(p, "feels more powerful.");
                msg(p, "You feel more powerful!");
                player_set_timed_perma(p, TMD_SHIELD);
                break;
            }

            /* Increase Str/Dex/Con + resistance effect */
            case 4:
            {
                msg_misc(p, "feels more powerful.");
                msg(p, "You feel more powerful!");
                player_set_timed_perma(p, TMD_OPP_ACID);
                player_set_timed_perma(p, TMD_OPP_ELEC);
                player_set_timed_perma(p, TMD_OPP_FIRE);
                player_set_timed_perma(p, TMD_OPP_COLD);
                player_set_timed_perma(p, TMD_OPP_POIS);
                break;
            }

            /* Increase Str/Dex/Con + haste effect */
            case 5:
            {
                msg_misc(p, "feels more powerful.");
                msg(p, "You feel more powerful!");
                player_set_timed_perma(p, TMD_FAST);
                break;
            }
        }

        /* Notice */
        notice = true;
    }

    /* Decrease stage */
    else if (new_aux < old_aux)
    {
        /* Describe the state */
        switch (new_aux)
        {
            /* None */
            case 0:
            {
                msg_misc(p, "feels less attuned to nature.");
                msg(p, "You feel less attuned to nature.");
                player_clear_timed(p, TMD_BLESSED, true);
                break;
            }

            /* Decrease Str/Dex/Con */
            case 1:
            {
                msg_misc(p, "feels less powerful.");
                msg(p, "You feel less powerful.");
                break;
            }

            /* Decrease Str/Dex/Con + lose shield effect */
            case 2:
            {
                msg_misc(p, "feels less powerful.");
                msg(p, "You feel less powerful.");
                player_clear_timed(p, TMD_SHIELD, true);
                break;
            }

            /* Decrease Str/Dex/Con + lose resistance effect */
            case 3:
            {
                msg_misc(p, "feels less powerful.");
                msg(p, "You feel less powerful.");
                player_clear_timed(p, TMD_OPP_ACID, true);
                player_clear_timed(p, TMD_OPP_ELEC, true);
                player_clear_timed(p, TMD_OPP_FIRE, true);
                player_clear_timed(p, TMD_OPP_COLD, true);
                player_clear_timed(p, TMD_OPP_POIS, true);
                break;
            }

            /* Decrease Str/Dex/Con + lose haste effect */
            case 4:
            {
                msg_misc(p, "feels less powerful.");
                msg(p, "You feel less powerful.");
                player_clear_timed(p, TMD_FAST, true);
                break;
            }
        }

        /* Notice */
        notice = true;
    }

    /* Use the value */
    p->timed[TMD_HARMONY] = v;

    /* Nothing to notice */
    if (!notice) return false;

    /* Notice */
    p->upkeep->update |= (PU_BONUS);

    /* Disturb */
    disturb(p, 0);

    /* Redraw the status */
    p->upkeep->redraw |= (PR_STATUS);

    /* Handle stuff */
    handle_stuff(p);

    /* Result */
    return true;
}


/*
 * Return true if the player timed effect matches the given string
 */
bool player_timed_grade_eq(struct player *p, int idx, const char *match)
{
    if (p->timed[idx])
    {
        struct timed_grade *grade = timed_effects[idx].grade;

        while (p->timed[idx] > grade->max) grade = grade->next;
        if (grade->name && streq(grade->name, match)) return true;
    }

    return false;
}


/*
 * Setting, increasing, decreasing and clearing timed effects
 */


static bool player_of_has_not_timed(struct player *p, int flag)
{
    bitflag collect_f[OF_SIZE], f[OF_SIZE];
    int i;

    player_flags(p, collect_f);

    for (i = 0; i < p->body.count; i++)
    {
        struct object *obj = slot_object(p, i);

        if (!obj) continue;
        object_flags(obj, f);
        of_union(collect_f, f);
    }

    return of_has(collect_f, flag);
}


/*
 * Set a timed event.
 *
 * p is the player to affect.
 * idx is the index, greater than equal to zero and less than TMD_MAX, for the effect.
 * v is the new value for the effect. Internally, v is coerced to lie
 * within the range of acceptable values for the effect.
 * notify, if true, allows for messages, updates to the user interface,
 * and player disturbance if setting the effect doesn't duplicate an effect
 * already present. If false, prevents messages, updates to the user interface,
 * and player disturbance unless setting the effect increases the effect's
 * gradation or decreases the effect's gradation when the effect has messages
 * for the gradations that lapse.
 *
 * Returns whether setting the effect caused the player to be notified.
 */
bool player_set_timed(struct player *p, int idx, int v, bool notify)
{
    struct timed_effect_data *effect;
    struct timed_grade *new_grade, *current_grade;
    struct object *weapon;
    bool result, no_disturb = false;
    int food_meter = 0;

    my_assert(idx >= 0);
    my_assert(idx < TMD_MAX);

    effect = &timed_effects[idx];
    new_grade = effect->grade;
    current_grade = effect->grade;
    weapon = equipped_item_by_slot_name(p, "weapon");

    /* Lower bound */
    v = MAX(v, effect->lower_bound);

    /* No change */
    if (p->timed[idx] == v) return false;

    /* Find the grade we will be going to, and the current one */
    while (v > new_grade->max)
    {
        new_grade = new_grade->next;
        if (!new_grade->next) break;
    }
    while (p->timed[idx] > current_grade->max)
    {
        current_grade = current_grade->next;
        if (!current_grade->next) break;
    }

    /* Upper bound */
    if (v > new_grade->max)
    {
        /* No change: tried to exceed the maximum possible and already there */
        if (p->timed[idx] == new_grade->max) return false;

        v = new_grade->max;
    }

    // Knight class stances
    if (idx == TMD_BALANCED_STANCE)
    {
        player_clear_timed(p, TMD_DEFENSIVE_STANCE, false);
        player_clear_timed(p, TMD_OFFENSIVE_STANCE, false);
    }
    else if (idx == TMD_DEFENSIVE_STANCE)
    {
        player_clear_timed(p, TMD_BALANCED_STANCE, false);
        player_clear_timed(p, TMD_OFFENSIVE_STANCE, false);
    }
    else if (idx == TMD_OFFENSIVE_STANCE)
    {
        player_clear_timed(p, TMD_BALANCED_STANCE, false);
        player_clear_timed(p, TMD_DEFENSIVE_STANCE, false);
    }

    /* Hack -- call other functions, reveal hidden players if noticed */
    if ((idx == TMD_STUN) && (p->dm_flags & DM_INVULNERABLE))
    {
        /* Hack -- the DM can not be stunned */
        if (p->k_idx) aware_player(p, p);
        return true;
    }
    if ((idx == TMD_CUT) && p->ghost && (v > 0))
    {
        /* Ghosts cannot bleed */
        if (p->k_idx) aware_player(p, p);
        return true;
    }
    if (idx == TMD_BOWBRAND)
    {
        result = set_bow_brand(p, v);
        if (result && p->k_idx) aware_player(p, p);
        return result;
    }
    if (idx == TMD_ADRENALINE)
    {
        result = set_adrenaline(p, v);
        if (result && p->k_idx) aware_player(p, p);
        return result;
    }
    if (idx == TMD_BIOFEEDBACK)
    {
        result = set_biofeedback(p, v);
        if (result && p->k_idx) aware_player(p, p);
        return result;
    }
    if (idx == TMD_HARMONY)
    {
        result = set_harmony(p, v);
        if (result && p->k_idx) aware_player(p, p);
        return result;
    }

    /* Don't mention effects which already match the known player state. */
    if (timed_effects[idx].temp_resist != -1 &&
        p->obj_k->el_info[timed_effects[idx].temp_resist].res_level[0] &&
        player_is_immune(p, timed_effects[idx].temp_resist))
    {
        notify = false;
    }
    if (timed_effects[idx].oflag_syn && timed_effects[idx].oflag_dup != OF_NONE &&
        of_has(p->obj_k->flags, timed_effects[idx].oflag_dup) &&
        player_of_has_not_timed(p, timed_effects[idx].oflag_dup))
    {
        notify = false;
    }

    /* Always mention going up a grade, otherwise on request */
    if (new_grade->grade > current_grade->grade)
    {
        msg_misc(p, effect->near_begin);
        // exception: entering Old Ruins. We don't wanna play sound 'blind' there
        // as we have there sound with a sliding door: sound(p, MSG_ENTER_RUINS)
        if (p->wpos.depth != 1 && p->exp != 0)
            print_custom_message(p, weapon, new_grade->up_msg, effect->msgt);
        notify = true;
    }
    else if ((new_grade->grade < current_grade->grade) && new_grade->down_msg)
    {
        msg_misc(p, effect->near_begin);
        print_custom_message(p, weapon, new_grade->down_msg, effect->msgt);
        notify = true;
    }
    else if (notify)
    {
        /* Finishing */
        if (v == 0)
        {
            msg_misc(p, effect->near_end);
            // exception: entering Old Ruins. We don't wanna play recovery 'ding' sound
            // as we have there sound with a sliding door: sound(p, MSG_ENTER_RUINS)
            if (p->wpos.depth != 1 && p->exp != 0)
                print_custom_message(p, weapon, effect->on_end, MSG_RECOVER);
            if (!OPT(p, disturb_effect_end)) no_disturb = true;
        }

        /* Decrementing */
        else if ((p->timed[idx] > v) && effect->on_decrease)
            print_custom_message(p, weapon, effect->on_decrease, effect->msgt);

        /* Incrementing */
        else if ((v > p->timed[idx]) && effect->on_increase)
            print_custom_message(p, weapon, effect->on_increase, effect->msgt);
    }

    /* Dispatch effects for transitions. */
    if (v > 0 && !p->timed[idx])
    {
        /* The effect starts. */
        if (effect->on_begin_effect)
        {
            bool ident = false;
            struct source who_body;
            struct source *who = &who_body;

            source_player(who, get_player_index(get_connection(p->conn)), p);
            effect_do(effect->on_begin_effect, who, &ident, true, 0, NULL, 0, 0, NULL);
        }
    }
    else if (v == 0)
    {
        /* The effect lapses. */
        if (effect->on_end_effect)
        {
            bool ident = false;
            struct source who_body;
            struct source *who = &who_body;

            source_player(who, get_player_index(get_connection(p->conn)), p);
            effect_do(effect->on_end_effect, who, &ident, true, 0, NULL, 0, 0, NULL);
        }
    }

    /* Hack -- food meter */
    if (idx == TMD_FOOD) food_meter = p->timed[idx] / 100;

    /* Use the value */
    p->timed[idx] = v;

    /* Hack -- food meter */
    if ((idx == TMD_FOOD) && (food_meter != p->timed[idx] / 100))
    {
        if (!notify) no_disturb = true;
        notify = true;
    }

    if (notify)
    {
        /* Disturb */
        if (!no_disturb) disturb(p, 0);

        /* Reveal hidden players */
        if (p->k_idx) aware_player(p, p);

        /* Update the visuals, as appropriate. */
        p->upkeep->update |= effect->flag_update;
        p->upkeep->redraw |= effect->flag_redraw;

        /* Handle stuff */
        handle_stuff(p);
    }

    return notify;
}


/*
 * Check whether a timed effect will affect the player
 *
 * p is the player to check.
 * idx is the index, greater than equal to zero and less than TMD_MAX, for the effect.
 * lore, if true, modifies the check so it is appropriate for filling
 * in details of monster recall.
 *
 * Returns whether the player can be affected by the effect.
 */
bool player_inc_check(struct player *p, struct monster *mon, int idx, bool lore)
{
    struct timed_effect_data *effect = &timed_effects[idx];
    struct timed_failure *f = effect->fail;

    while (f)
    {
        switch (f->code)
        {
            case TMD_FAIL_FLAG_OBJECT:
            {
                /* Effect is inhibited by an object flag */
                if (!lore) equip_learn_flag(p, f->idx);

                /* If the effect is from a monster action, extra stuff happens */
                if (mon && !lore) update_smart_learn(mon, p, f->idx, 0, -1);
                if (player_of_has(p, f->idx))
                {
                    if (mon && !lore) msg(p, "You resist the effect!");
                    return false;
                }

                break;
            }

            case TMD_FAIL_FLAG_RESIST:
            {
                /* Effect is inhibited by a resist */
                my_assert(f->idx >= 0 && f->idx < ELEM_MAX);
                if (!lore) equip_learn_element(p, f->idx);
                if (p->state.el_info[f->idx].res_level[0] > 0) return false;

                break;
            }

            case TMD_FAIL_FLAG_VULN:
            {
                /* Effect is inhibited by a vulnerability */
                my_assert(f->idx >= 0 && f->idx < ELEM_MAX);
                if (p->state.el_info[f->idx].res_level[0] < 0)
                {
                    if (!lore) equip_learn_element(p, f->idx);
                    return false;
                }

                break;
            }

            case TMD_FAIL_FLAG_PLAYER:
            {
                /* Effect is inhibited by a player flag */
                if (player_has(p, f->idx)) return false;

                break;
            }

            case TMD_FAIL_FLAG_TIMED_EFFECT:
            {
                /*
                 * Effect is inhibited by a timed effect. If timed
                 * effect is active, it is known to the player, so
                 * there's no difference between whether this is
                 * solely a lore check or not.
                 */
                my_assert(f->idx >= 0 && f->idx < TMD_MAX);
                if (p->timed[f->idx]) return false;

                break;
            }

            default:
            {
                /* Should never happen. */
                my_assert(0);

                break;
            }
        }
        f = f->next;
    }

    /* Nothing prevents this effect from incrementing. */
    return true;
}


/*
 * Increase the timed effect `idx` by `v`.
 *
 * If the effect is from a monster action, extra stuff happens.
 */
bool player_inc_timed_aux(struct player *p, struct monster *mon, int idx, int v, bool notify,
    bool check)
{
    my_assert(idx >= 0);
    my_assert(idx < TMD_MAX);

    if (!check || player_inc_check(p, mon, idx, false))
    {
        /* Block the increase if the effect is nonstacking and already active. */
        if ((timed_effects[idx].flags & TMD_FLAG_NONSTACKING) && p->timed[idx] > 0)
            return false;

        // some additional thresholds so conditions won't overstuck
        if ((idx == TMD_SLOW || idx == TMD_CONFUSED || idx == TMD_AFRAID)
        && (p->timed[idx] > 10)) return false;

        if ((idx == TMD_POISONED) && (p->timed[idx] > 15)) return false;

        if ((idx == TMD_STUN) && (p->timed[idx] > 35)) return false;

        /* Hack -- permanent effect */
        if (p->timed[idx] == -1) return false;

        /* Handle polymorphed players */
        if (p->poly_race && (idx == TMD_AMNESIA))
        {
            if (rf_has(p->poly_race->flags, RF_EMPTY_MIND)) v /= 2;
            if (rf_has(p->poly_race->flags, RF_WEIRD_MIND)) v = v * 3 / 4;
        }

        return player_set_timed(p, idx, p->timed[idx] + v, notify);
    }

    return false;
}


/*
 * Increase the timed effect `idx` by `v`.
 *
 * p is the player to affect.
 * idx is the index, greater than equal to zero and less than TMD_MAX, for the effect.
 * notify, if true, allows for messages, updates to the user interface,
 * and player disturbance if increasing the duration doesn't duplicate an effect
 * already present. If false, prevents messages, updates to the user interface,
 * and player disturbance unless increasing the duration increases the effect's
 * gradation or decreases the effect's gradation when the effect has messages
 * for the gradations that lapse.
 * check, if true, allows for the player to resist the effect if
 * player_inc_check(p, idx, false) is true.
 *
 * Returns whether increasing the duration caused the player to be notified.
 */
bool player_inc_timed(struct player *p, int idx, int v, bool notify, bool check)
{
    return player_inc_timed_aux(p, NULL, idx, v, notify, check);
}


/*
 * Decrease the timed effect `idx` by `v`.
 *
 * p is the player to affect.
 * idx is the index, greater than equal to zero and less than TMD_MAX, for the effect.
 * v is the amount to subtract from the effect's duration.
 * notify, if true or v is greater than or equal to the effect's current
 * duration, allows for messages, updates to the user interface, and player
 * disturbance. If false and v is less than the effect's current duration,
 * prevents messages, updates to the user interface, and player disturbance
 * unless the change to the duration increases the effect's gradation or
 * decreases the effect's gradation when the effect has messages for the
 * gradations that lapse.
 *
 * Returns whether changing the duration caused the player to be notified.
 */
bool player_dec_timed(struct player *p, int idx, int v, bool notify)
{
    int new_value;

    my_assert(idx >= 0);
    my_assert(idx < TMD_MAX);
    new_value = p->timed[idx] - v;

    if (p->no_disturb_icky && (new_value > 0)) p->no_disturb_icky = false;

    /* Obey `notify` if not finishing; if finishing, always notify */
    if (new_value > 0) return player_set_timed(p, idx, new_value, notify);

    return player_set_timed(p, idx, new_value, true);
}


/*
 * Clear the timed effect `idx`.
 *
 * p is the player to affect.
 * idx is the index, greater than equal to zero and less than TMD_MAX, for the effect.
 * notify, if true, allows for messages, updates to the user interface,
 * and player disturbance if clearing the effect doesn't duplicate an effect
 * already present. If false, prevents messages, updates to the user interface,
 * and player disturbance unless clearing the effect decreases the effect's
 * gradation and the effect has messages for the gradations that lapse.
 *
 * Returns whether clearing the effect caused the player to be notified.
 */
bool player_clear_timed(struct player *p, int idx, bool notify)
{
    my_assert(idx >= 0);
    my_assert(idx < TMD_MAX);

    return player_set_timed(p, idx, 0, notify);
}
