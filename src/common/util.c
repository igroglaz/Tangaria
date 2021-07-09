/*
 * File: util.c
 * Purpose: Utility functions
 *
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
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


#include "angband.h"


struct object_kind *k_info;
struct ego_item *e_info;
struct player_race *races;
struct dragon_breed *breeds;
struct player_class *classes;
struct start_item *dm_start_items;
struct player_ability *player_abilities;
struct magic_realm *realms;
struct player_body *bodies;
struct monster_race *r_info;
struct monster_base *rb_info;
struct curse *curses;
struct trap_kind *trap_info;


void cleanup_p_race(void)
{
    struct player_race *p = races;
    struct player_race *next;
    struct player_shape *shape, *shape_next;
    struct barehanded_attack *attack, *attack_next;
    struct gift *item, *item_next;

    while (p)
    {
        next = p->next;
        string_free(p->name);
        mem_free(p->brands);
        mem_free(p->slays);
        shape = p->shapes;
        while (shape)
        {
            shape_next = shape->next;
            string_free(shape->name);
            mem_free(shape);
            shape = shape_next;
        }
        attack = p->attacks;
        while (attack)
        {
            attack_next = attack->next;
            string_free(attack->verb);
            string_free(attack->hit_extra);
            mem_free(attack);
            attack = attack_next;
        }
        item = p->gifts;
        while (item)
        {
            item_next = item->next;
            mem_free(item);
            item = item_next;
        }
        mem_free(p);
        p = next;
    }
}


void cleanup_realm(void)
{
    struct magic_realm *p = realms;
    struct magic_realm *next;

    while (p)
    {
        next = p->next;
        string_free(p->name);
        string_free(p->verb);
        string_free(p->spell_noun);
        string_free(p->book_noun);
        mem_free(p);
        p = next;
    }
}


/*
 * Free all the effects in a structure
 *
 * source the effects being freed
 */
void free_effect(struct effect *source)
{
    struct effect *e = source, *e_next;

    while (e)
    {
        e_next = e->next;
        dice_free(e->dice);
        string_free(e->self_msg);
        string_free(e->other_msg);
        mem_free(e);
        e = e_next;
    }
}


void cleanup_class(void)
{
    struct player_class *c = classes;
    struct player_class *next;
    struct start_item *item, *item_next;
    struct class_spell *spell;
    struct class_book *book;
    struct player_shape *shape, *shape_next;
    struct barehanded_attack *attack, *attack_next;
    int i, j;

    while (c)
    {
        next = c->next;
        string_free(c->name);
        mem_free(c->brands);
        mem_free(c->slays);
        for (i = 0; i < PY_MAX_LEVEL / 5; i++) string_free(c->title[i]);
        item = c->start_items;
        while (item)
        {
            item_next = item->next;
            mem_free(item->eopts);
            mem_free(item);
            item = item_next;
        }
        for (i = 0; c->magic.books && (i < c->magic.num_books); i++)
        {
            book = &c->magic.books[i];
            for (j = 0; j < book->num_spells; j++)
            {
                spell = &book->spells[j];
                string_free(spell->name);
                string_free(spell->text);
                free_effect(spell->effect);
            }
            mem_free(book->spells);
        }
        mem_free(c->magic.books);
        shape = c->shapes;
        while (shape)
        {
            shape_next = shape->next;
            string_free(shape->name);
            mem_free(shape);
            shape = shape_next;
        }
        attack = c->attacks;
        while (attack)
        {
            attack_next = attack->next;
            string_free(attack->verb);
            string_free(attack->hit_extra);
            mem_free(attack);
            attack = attack_next;
        }
        mem_free(c);
        c = next;
    }
}


void cleanup_dm_start_items(void)
{
    struct start_item *item = dm_start_items, *item_next;

    while (item)
    {
        item_next = item->next;
        mem_free(item);
        item = item_next;
    }
}


void cleanup_body(void)
{
    struct player_body *b = bodies;
    struct player_body *next;
    int i;

    while (b)
    {
        next = b->next;
        string_free(b->name);
        for (i = 0; i < b->count; i++)
            string_free(b->slots[i].name);
        mem_free(b->slots);
        mem_free(b);
        b = next;
    }
}


/*
 * Player Sexes
 *
 *      Title,
 *      Winner,
 *      Conqueror of the Nether Realm,
 *      Melkor killer
 */
player_sex sex_info[MAX_SEXES] =
{
    {
        "Female",
        "Queen",
        "Empress",
        "Goddess"
    },
    {
        "Male",
        "King",
        "Emperor",
        "God"
    },
    {
        "Neuter",
        "Regent",
        "Ruler",
        "Deity"
    }
};


/*
 * Abbreviations of healthy stats
 */
const char *stat_names[STAT_MAX] =
{
    "STR: ", "INT: ", "WIS: ", "DEX: ", "CON: ", "CHR: "
};


/*
 * Abbreviations of damaged stats
 */
const char *stat_names_reduced[STAT_MAX] =
{
    "Str: ", "Int: ", "Wis: ", "Dex: ", "Con: ", "Chr: "
};


/*
 * Global arrays for converting "keypad direction" into offsets
 */
s16b ddx[10] =
{ 0, -1, 0, 1, -1, 0, 1, -1, 0, 1 };


s16b ddy[10] =
{ 0, 1, 1, 1, 0, 0, 0, -1, -1, -1 };


/*
 * Formats 'fmt' into 'buf', with the following formatting characters:
 *
 * '~' at the end of a word (e.g. "fridge~") will pluralise
 *
 * '|x|y|' will be output as 'x' if singular or 'y' if plural (e.g. "kni|fe|ves|")
 *
 * '#' will be replaced with 'modstr' (which may contain the pluralising
 * formats given above).
 */
size_t obj_desc_name_format(char *buf, size_t max, size_t end, const char *fmt, const char *modstr,
    bool pluralise)
{
    /* Copy the string */
    while (*fmt)
    {
        /* Skip */
        if (*fmt == '&')
        {
            while (*fmt == ' ' || *fmt == '&')
                fmt++;
            continue;
        }

        /* Pluralizer (regular English plurals) */
        else if (*fmt == '~')
        {
            char prev = *(fmt - 1);

            if (!pluralise)
            {
                fmt++;
                continue;
            }

            /* e.g. cutlass-e-s, torch-e-s, box-e-s */
            if (prev == 's' || prev == 'h' || prev == 'x')
                strnfcat(buf, max, &end, "es");
            else
                strnfcat(buf, max, &end, "s");
        }

        /* Special plurals */
        else if (*fmt == '|')
        {
            /* e.g. kni|fe|ves| */
            const char *singular = fmt + 1;
            const char *plural   = strchr(singular, '|');
            const char *endmark  = NULL;

            if (plural)
            {
                plural++;
                endmark = strchr(plural, '|');
            }

            if (!singular || !plural || !endmark) return end;

            if (!pluralise)
                strnfcat(buf, max, &end, "%.*s", plural - singular - 1, singular);
            else
                strnfcat(buf, max, &end, "%.*s", endmark - plural, plural);

            fmt = endmark;
        }

        /* Add modstr, with pluralisation if relevant */
        else if (*fmt == '#')
            end = obj_desc_name_format(buf, max, end, modstr, NULL, pluralise);

        else
            buf[end++] = *fmt;

        fmt++;
    }

    buf[end] = 0;

    return end;
}


/*
 * Puts a very stripped-down version of an object's name into buf.
 * If aware is true, then the IDed names are used, otherwise
 * flavours, scroll names, etc will be used.
 *
 * Just truncates if the buffer isn't big enough.
 */
void object_kind_name(char *buf, size_t max, const struct object_kind *kind, bool aware)
{
    /* If not aware, the plain flavour (e.g. Copper) will do. */
    if (!aware && kind->flavor)
        my_strcpy(buf, kind->flavor->text, max);

    /* Use proper name (Healing, or whatever) */
    else
        obj_desc_name_format(buf, max, 0, kind->name, NULL, false);
}


/*
 * Return the numeric sval of the object kind with the given `tval` and name `name`.
 */
static int lookup_sval_aux(int tval, const char *name, bool silent)
{
    int k;
    unsigned int r;

    if (sscanf(name, "%u", &r) == 1) return r;

    /* Look for it */
    for (k = 0; k < z_info->k_max; k++)
    {
        struct object_kind *kind = &k_info[k];
        char cmp_name[MSG_LEN];

        if (!kind || !kind->name) continue;

        obj_desc_name_format(cmp_name, sizeof(cmp_name), NULL, kind->name, 0, false);

        /* Found a match */
        if ((kind->tval == tval) && !my_stricmp(cmp_name, name)) return kind->sval;
    }

    if (!silent) plog_fmt("No object (\"%s\",\"%s\")", tval_find_name(tval), name);
    return -1;
}


int lookup_sval(int tval, const char *name)
{
    return lookup_sval_aux(tval, name, false);
}


int lookup_sval_silent(int tval, const char *name)
{
    return lookup_sval_aux(tval, name, true);
}


void object_short_name(char *buf, size_t max, const char *name)
{
    size_t j, k;
    size_t len = strlen(name);

    /* Copy across the name, stripping modifiers & and ~ */
    for (j = 0, k = 0; ((j < len) && (k < max - 1)); j++)
    {
        if ((j == 0) && (name[0] == '&') && (name[1] == ' ')) j += 2;
        if (name[j] == '~') continue;
        buf[k++] = name[j];
    }
    buf[k] = 0;
}


/*
 * Return the object kind with the given `tval` and `sval`, or NULL.
 */
static struct object_kind *lookup_kind_aux(int tval, int sval, bool silent)
{
    int k;

    /* Look for it */
    for (k = 0; k < z_info->k_max; k++)
    {
        struct object_kind *kind = &k_info[k];

        if ((kind->tval == tval) && (kind->sval == sval)) return kind;
    }

    /* Failure */
    if (!silent) plog_fmt("No object: %d:%d (%s)", tval, sval, tval_find_name(tval));
    return NULL;
}


struct object_kind *lookup_kind(int tval, int sval)
{
    return lookup_kind_aux(tval, sval, false);
}


struct object_kind *lookup_kind_silent(int tval, int sval)
{
    return lookup_kind_aux(tval, sval, true);
}


struct object_kind *lookup_kind_by_name(int tval, const char *name)
{
    return lookup_kind(tval, lookup_sval(tval, name));
}


/*
 * Converts stat num into a six-char (right justified) string
 */
void cnv_stat(int val, char *out_val, size_t out_len)
{
    /* Stats above 18 need special treatment */
    if (val > 18)
    {
        int bonus = (val - 18);

        if (bonus >= 220)
            strnfmt(out_val, out_len, "18/***");
        else if (bonus >= 100)
            strnfmt(out_val, out_len, "18/%03d", bonus);
        else
            strnfmt(out_val, out_len, " 18/%02d", bonus);
    }
    else
        strnfmt(out_val, out_len, "    %2d", val);
}


/*
 * Find the start of a possible Roman numerals suffix by going back from the
 * end of the string to a space, then checking that all the remaining chars
 * are valid Roman numerals.
 */
static char *find_roman_suffix_start(const char *buf)
{
    char *start = strrchr(buf, ' ');
    char *p;

    if (start)
    {
        start++;
        p = start;
        while (*p)
        {
            if ((*p != 'I') && (*p != 'V') && (*p != 'X') && (*p != 'L') &&
                (*p != 'C') && (*p != 'D') && (*p != 'M'))
            {
                start = NULL;
                break;
            }
            ++p;
        }
    }
    return start;
}


/*
 * Converts an arabic numeral (int) to a roman numeral (char *).
 *
 * An arabic numeral is accepted in parameter `n`, and the corresponding
 * upper-case roman numeral is placed in the parameter `roman`.  The
 * length of the buffer must be passed in the `bufsize` parameter.  When
 * there is insufficient room in the buffer, or a roman numeral does not
 * exist (e.g. non-positive integers) a value of false is returned and the
 * `roman` buffer will be the empty string.  On success, a value of true is
 * returned and the zero-terminated roman numeral is placed in the
 * parameter `roman`.
 *
 */
static bool int_to_roman(int n, char *roman, size_t bufsize)
{
    /* Roman symbols */
    char roman_symbol_labels[13][3] =
        {"M", "CM", "D", "CD", "C", "XC", "L", "XL", "X", "IX", "V", "IV", "I"};
    int roman_symbol_values[13] =
        {1000, 900, 500, 400, 100, 90, 50, 40, 10, 9, 5, 4, 1};

    /* Clear the roman numeral buffer */
    roman[0] = '\0';

    /* Roman numerals have no zero or negative numbers */
    if (n < 1) return false;

    /* Build the roman numeral in the buffer */
    while (n > 0)
    {
        int i = 0;

        /* Find the largest possible roman symbol */
        while (n < roman_symbol_values[i]) i++;

        /* No room in buffer, so abort */
        if (strlen(roman) + strlen(roman_symbol_labels[i]) + 1 > bufsize) break;

        /* Add the roman symbol to the buffer */
        my_strcat(roman, roman_symbol_labels[i], bufsize);

        /* Decrease the value of the arabic numeral */
        n -= roman_symbol_values[i];
    }

    /* Ran out of space and aborted */
    if (n > 0)
    {
        /* Clean up and return */
        roman[0] = '\0';
        return false;
    }

    return true;
}


/*
 * Converts a roman numeral (char *) to an arabic numeral (int).
 *
 * The null-terminated roman numeral is accepted in the `roman`
 * parameter and the corresponding integer arabic numeral is returned.
 * Only upper-case values are considered. When the `roman` parameter
 * is empty or does not resemble a roman numeral, a value of -1 is
 * returned.
 */
static int roman_to_int(const char *roman)
{
    size_t i;
    int n = 0;
    char *p;
    char roman_token_chr1[] = "MDCLXVI";
    const char *roman_token_chr2[] = {0, 0, "DM", 0, "LC", 0, "VX"};
    int roman_token_vals[7][3] =
        {{1000}, {500}, {100, 400, 900}, {50}, {10, 40, 90}, {5}, {1, 4, 9}};

    if (strlen(roman) == 0) return -1;

    /*
     * Check each character for a roman token, and look ahead to the
     * character after this one to check for subtraction
     */
    for (i = 0; i < strlen(roman); i++)
    {
        char c1, c2;
        int c1i, c2i;

        /* Get the first and second chars of the next roman token */
        c1 = roman[i];
        c2 = roman[i + 1];

        /* Find the index for the first character */
        p = strchr(roman_token_chr1, c1);
        if (p) c1i = p - roman_token_chr1;
        else return -1;

        /* Find the index for the second character */
        c2i = 0;
        if (roman_token_chr2[c1i] && c2)
        {
            p = strchr(roman_token_chr2[c1i], c2);
            if (p)
            {
                c2i = (p - roman_token_chr2[c1i]) + 1;

                /* Two-digit token, so skip a char on the next pass */
                i++;
            }
        }

        /* Increase the arabic numeral */
        n += roman_token_vals[c1i][c2i];
    }

    return n;
}


/*
 * Get next incarnation.
 */
void get_next_incarnation(char *name, size_t len)
{
    char *buf;

    /* Handle incrementing name suffix */
    buf = find_roman_suffix_start(name);

    /* Try to increment the roman suffix */
    if (buf)
        int_to_roman(roman_to_int(buf) + 1, buf, len - (buf - (char *)&name));

    /* Allow incarnation for any character (Foo -> Foo II) */
    else
        my_strcat(name, " II", len);
}


/*
 * Get previous incarnation.
 */
bool get_previous_incarnation(char *name, size_t len)
{
    char *buf;

    /* Find the start of a possible Roman numerals suffix */
    buf = find_roman_suffix_start(name);
    if (!buf) return false;

    /* Try to decrement the roman suffix */
    return int_to_roman(roman_to_int(buf) - 1, buf, len - (buf - (char *)&name));
}


const char *strip_suffix(const char *name)
{
    static char buf[40];
    int i;
    int limit = 0;

    if (name[0])
    {
        char *suffix = find_roman_suffix_start(name);

        if (suffix)
            limit = suffix - name - 1;
        else
            limit = strlen(name);
    }

    for (i = 0; i < limit; i++)
    {
        char c = name[i];

        /* Convert all non-alphanumeric symbols */
        if (!isalpha((unsigned char)c) && !isdigit((unsigned char)c)) c = '_';

        /* Build "base_name" */
        buf[i] = c;
    }

    /* Terminate */
    buf[i] = '\0';

    /* Require a "base" name */
    if (!buf[0]) my_strcpy(buf, "PLAYER", sizeof(buf));

    return buf;
}


/*
 * Returns a "rating" of x depending on y, and sets "attr" to the
 * corresponding "attribute".
 */
const char *likert(int x, int y, byte *attr)
{
    /* Paranoia */
    if (y <= 0) y = 1;

    /* Negative values */
    if (x < 0)
    {
        *attr = COLOUR_RED;
        return ("Very Bad");
    }

    /* Analyze the value */
    switch ((x / y))
    {
        case 0:
        case 1:
        {
            *attr = COLOUR_RED;
            return ("Bad");
        }
        case 2:
        {
            *attr = COLOUR_RED;
            return ("Poor");
        }
        case 3:
        case 4:
        {
            *attr = COLOUR_YELLOW;
            return ("Fair");
        }
        case 5:
        {
            *attr = COLOUR_YELLOW;
            return ("Good");
        }
        case 6:
        {
            *attr = COLOUR_YELLOW;
            return ("Very Good");
        }
        case 7:
        case 8:
        {
            *attr = COLOUR_L_GREEN;
            return ("Excellent");
        }
        case 9:
        case 10:
        case 11:
        case 12:
        case 13:
        {
            *attr = COLOUR_L_GREEN;
            return ("Superb");
        }
        case 14:
        case 15:
        case 16:
        case 17:
        {
            *attr = COLOUR_L_GREEN;
            return ("Heroic");
        }
        default:
        {
            *attr = COLOUR_L_GREEN;
            return ("Legendary");
        }
    }
}


/*
 * Base experience levels, may be adjusted up for race and/or class
 */
static s32b player_exp[PY_MAX_LEVEL] =
{
    10,
    25,
    45,
    70,
    100,
    140,
    200,
    280,
    380,
    500,
    650,
    850,
    1100,
    1400,
    1800,
    2300,
    2900,
    3600,
    4400,
    5400,
    6800,
    8400,
    10200,
    12500,
    17500,
    25000,
    35000L,
    50000L,
    75000L,
    100000L,
    150000L,
    200000L,
    275000L,
    350000L,
    450000L,
    550000L,
    700000L,
    850000L,
    1000000L,
    1250000L,
    1500000L,
    1800000L,
    2100000L,
    2400000L,
    2700000L,
    3000000L,
    3500000L,
    4000000L,
    4500000L,
    5000000L
};


s32b adv_exp(s16b lev, s16b expfact)
{
    /* Max reached */
    if (lev >= PY_MAX_LEVEL) return 0L;

    /* High exp: first divide by 100 to avoid overflow */
    if (lev >= 20) return (s32b)((player_exp[lev - 1] / 100L) * expfact);

    /* Low exp */
    return (s32b)(player_exp[lev - 1] * expfact / 100L);
}


/*
 * Apply a tester function, skipping all non-objects and gold
 */
bool object_test(struct player *p, item_tester tester, const struct object *obj)
{
    /* Require object */
    if (!obj) return false;

    /* Ignore gold */
    if (tval_is_money(obj)) return false;

    /* Pass without a tester, or tail-call the tester if it exists */
    return (!tester || tester(p, obj));
}


struct player_class *player_id2class(guid id)
{
    struct player_class *c;

    for (c = classes; c; c = c->next)
    {
        if (guid_eq(c->cidx, id)) break;
    }

    return c;
}


struct player_class *lookup_player_class(const char *name)
{
    struct player_class *c;

    for (c = classes; c; c = c->next)
    {
        if (streq(c->name, name)) break;
    }

    return c;
}


int player_cmax(void)
{
    int n = 0;
    struct player_class *c;

    for (c = classes; c; c = c->next) n++;

    return n;
}


int player_amax(void)
{
    int n = 0;
    struct player_ability *a;

    for (a = player_abilities; a; a = a->next) n++;

    return n;
}


struct player_race *player_id2race(guid id)
{
    struct player_race *r;

    for (r = races; r; r = r->next)
    {
        if (guid_eq(r->ridx, id)) break;
    }

    return r;
}


int player_rmax(void)
{
    int n = 0;
    struct player_race *r;

    for (r = races; r; r = r->next) n++;

    return n;
}


int player_bmax(void)
{
    int n = 0;
    struct player_body *b;

    for (b = bodies; b; b = b->next) n++;

    return n;
}


typedef struct
{
    ignore_type_t ignore_type;
    int tval;
    const char *identifier;
} quality_ignore_struct;


/*
 * Any entry here with an identifier should appear above the entry with the
 * same tval and no identifier
 */
static quality_ignore_struct quality_mapping[] =
{
    {ITYPE_GREAT, TV_SWORD, "Chaos"},
    {ITYPE_GREAT, TV_POLEARM, "Slicing"},
    {ITYPE_GREAT, TV_HAFTED, "Disruption"},
    {ITYPE_SHARP, TV_SWORD, ""},
    {ITYPE_SHARP, TV_POLEARM, ""},
    {ITYPE_BLUNT, TV_HAFTED, ""},
    {ITYPE_BLUNT, TV_MSTAFF, ""},
    {ITYPE_SLING, TV_BOW, "Sling"},
    {ITYPE_BOW, TV_BOW, "Bow"},
    {ITYPE_CROSSBOW, TV_BOW, "Crossbow"},
    {ITYPE_SHOT, TV_SHOT, ""},
    {ITYPE_ARROW, TV_ARROW, ""},
    {ITYPE_BOLT, TV_BOLT, ""},
    {ITYPE_THROW, TV_ROCK, ""},
    {ITYPE_ROBE, TV_SOFT_ARMOR, "Robe"},
    {ITYPE_BASIC_DRAGON_ARMOR, TV_DRAG_ARMOR, "Black"},
    {ITYPE_BASIC_DRAGON_ARMOR, TV_DRAG_ARMOR, "Blue"},
    {ITYPE_BASIC_DRAGON_ARMOR, TV_DRAG_ARMOR, "White"},
    {ITYPE_BASIC_DRAGON_ARMOR, TV_DRAG_ARMOR, "Red"},
    {ITYPE_BASIC_DRAGON_ARMOR, TV_DRAG_ARMOR, "Green"},
    {ITYPE_MULTI_DRAGON_ARMOR, TV_DRAG_ARMOR, "Multi"},
    {ITYPE_HIGH_DRAGON_ARMOR, TV_DRAG_ARMOR, "Shadow"},
    {ITYPE_HIGH_DRAGON_ARMOR, TV_DRAG_ARMOR, "Law"},
    {ITYPE_HIGH_DRAGON_ARMOR, TV_DRAG_ARMOR, "Gold"},
    {ITYPE_HIGH_DRAGON_ARMOR, TV_DRAG_ARMOR, "Chaos"},
    {ITYPE_EXTRA_DRAGON_ARMOR, TV_DRAG_ARMOR, "Crystal"},
    {ITYPE_EXTRA_DRAGON_ARMOR, TV_DRAG_ARMOR, "Silver"},
    {ITYPE_EXTRA_DRAGON_ARMOR, TV_DRAG_ARMOR, "Ethereal"},
    {ITYPE_EXTRA_DRAGON_ARMOR, TV_DRAG_ARMOR, "Dracolisk"},
    {ITYPE_EXTRA_DRAGON_ARMOR, TV_DRAG_ARMOR, "Water"},
    {ITYPE_BALANCE_DRAGON_ARMOR, TV_DRAG_ARMOR, "Balance"},
    {ITYPE_POWER_DRAGON_ARMOR, TV_DRAG_ARMOR, "Power"},
    {ITYPE_BODY_ARMOR, TV_HARD_ARMOR, ""},
    {ITYPE_BODY_ARMOR, TV_SOFT_ARMOR, ""},
    {ITYPE_ELVEN_CLOAK, TV_CLOAK, "Elven"},
    {ITYPE_CLOAK, TV_CLOAK, ""},
    {ITYPE_SHIELD, TV_SHIELD, ""},
    {ITYPE_HEADGEAR, TV_HELM, ""},
    {ITYPE_HEADGEAR, TV_CROWN, ""},
    {ITYPE_HANDGEAR, TV_GLOVES, ""},
    {ITYPE_FEET, TV_BOOTS, ""},
    {ITYPE_DIGGER, TV_DIGGING, ""},
    {ITYPE_TOOL, TV_HORN, ""},
    {ITYPE_RING, TV_RING, ""},
    {ITYPE_AMULET, TV_AMULET, ""},
    {ITYPE_LIGHT, TV_LIGHT, ""}
};


/*
 * Find the ignore type of the object, or ITYPE_MAX if none
 */
ignore_type_t ignore_type_of(const struct object *obj)
{
    size_t i;

    /* Find the appropriate ignore group */
    for (i = 0; i < N_ELEMENTS(quality_mapping); i++)
    {
        if (quality_mapping[i].tval == obj->tval)
        {
            /* If there's an identifier, it must match */
            if (quality_mapping[i].identifier[0] &&
                !strstr(obj->kind->name, quality_mapping[i].identifier))
            {
                continue;
            }

            /* Otherwise we're fine */
            return quality_mapping[i].ignore_type;
        }
    }

    return ITYPE_MAX;
}


/*
 * Find whether an ignore type is valid for a given ego item
 */
bool ego_has_ignore_type(struct ego_item *ego, ignore_type_t itype)
{
    struct poss_item *poss;

    /* Go through all the possible items */
    for (poss = ego->poss_items; poss; poss = poss->next)
    {
        size_t i;
        struct object_kind *kind = &k_info[poss->kidx];

        /* Check the appropriate ignore group */
        for (i = 0; i < N_ELEMENTS(quality_mapping); i++)
        {
            if ((quality_mapping[i].tval == kind->tval) &&
                (quality_mapping[i].ignore_type == itype) &&
                strstr(kind->name, quality_mapping[i].identifier))
            {
                return true;
            }
        }
    }

    return false;
}


/*
 * Return the monster base matching the given name.
 */
struct monster_base *lookup_monster_base(const char *name)
{
    struct monster_base *base;

    /* Look for it */
    for (base = rb_info; base; base = base->next)
    {
        /* Found a match */
        if (base->name && streq(base->name, name)) return base;
    }

    return NULL;
}


/*
 * Returns the monster with the given name. If no monster has the exact name
 * given, returns the first monster with the given name as a (case-insensitive)
 * substring.
 */
struct monster_race *lookup_monster(const char *name)
{
    int i;
    struct monster_race *closest = NULL;

    /* Look for it */
    for (i = 0; i < z_info->r_max; i++)
    {
        struct monster_race *race = &r_info[i];

        if (!race->name) continue;

        /* Test for equality */
        if (my_stricmp(name, race->name) == 0) return race;

        /* Test for close matches */
        if (!closest && my_stristr(race->name, name)) closest = race;
    }

    /* Return our best match */
    return closest;
}


/*
 * Modify a stat value by a "modifier", return new value
 *
 * Stats go up: 3,4,...,17,18,18/10,18/20,...,18/220
 * Or even: 18/13, 18/23, 18/33, ..., 18/220
 *
 * Stats go down: 18/220, 18/210,..., 18/10, 18, 17, ..., 3
 * Or even: 18/13, 18/03, 18, 17, ..., 3
 */
s16b modify_stat_value(int value, int amount)
{
    int i;

    /* Reward or penalty */
    if (amount > 0)
    {
        /* Apply each point */
        for (i = 0; i < amount; i++)
        {
            /* One point at a time */
            if (value < 18) value++;

            /* Ten "points" at a time */
            else value += 10;
        }
    }
    else if (amount < 0)
    {
        /* Apply each point */
        for (i = 0; i < (0 - amount); i++)
        {
            /* Ten points at a time */
            if (value >= 18+10) value -= 10;

            /* Hack -- prevent weirdness */
            else if (value > 18) value = 18;

            /* One point at a time */
            else if (value > 3) value--;
        }
    }

    /* Return new value */
    return (value);
}


/*
 * Return the MSG_ flag that matches the given string. This does not handle SOUND_MAX.
 *
 * name is a string that contains the name of a flag or a number.
 */
int message_lookup_by_name(const char *name)
{
    static const char *message_names[] =
    {
        #define MSG(x, s) #x,
        #include "../common/list-message.h"
        #undef MSG
    };
    size_t i;
    unsigned int number;

    if (sscanf(name, "%u", &number) == 1)
        return ((number < MSG_MAX_PARSE)? (int)number: -1);

    for (i = 0; i < N_ELEMENTS(message_names); i++)
    {
        if (my_stricmp(name, message_names[i]) == 0)
            return (int)i;
    }

    return -1;
}


/*
 * Creates the player's body
 */
void player_embody(struct player *p)
{
    int i;

    memcpy(&p->body, &bodies[p->race->body], sizeof(p->body));
    p->body.slots = mem_zalloc(p->body.count * sizeof(struct equip_slot));
    memcpy(p->body.slots, bodies[p->race->body].slots, p->body.count * sizeof(struct equip_slot));

    for (i = 0; i < N_HISTORY_FLAGS; i++)
        p->hist_flags[i] = mem_zalloc((p->body.count + 1) * sizeof(cave_view_type));
}


const struct magic_realm *lookup_realm(const char *name)
{
    struct magic_realm *realm = realms;

    while (realm)
    {
        if (!my_stricmp(name, realm->name)) return realm;
        realm = realm->next;
    }

    /* Fail horribly */
    quit_fmt("Failed to find %s magic realm", name);
    return NULL;
}


/*
 * Find a trap kind based on its short description
 */
struct trap_kind *lookup_trap(const char *desc)
{
    int i;
    struct trap_kind *closest = NULL;

    /* Look for it */
    for (i = 1; i < z_info->trap_max; i++)
    {
        struct trap_kind *kind = &trap_info[i];

        /* Test for equality */
        if (streq(desc, kind->desc)) return kind;

        /* Test for close matches */
        if (!closest && my_stristr(kind->desc, desc)) closest = kind;
    }

    /* Return our best match */
    return closest;
}


/*
 * Returns N which is the 1 in N chance for recharging to fail.
 */
int recharge_failure_chance(const struct object *obj, int strength)
{
    /* Ease of recharge ranges from 9 down to 4 (wands) or 3 (staffs) */
    int ease_of_recharge = (100 - obj->kind->level) / 10;
    int raw_chance = strength + ease_of_recharge - 2 * (obj->pval / obj->number);

    return ((raw_chance > 1)? raw_chance: 1);
}


int race_modifier(const struct player_race *race, int mod, int lvl, bool poly)
{
    if (lvl >= race->modifiers[mod].lvl)
    {
        int adj = race->modifiers[mod].value.base;
        int xadj = race->modifiers[mod].value.sides;

        if (xadj)
        {
            int cap = lvl;

            if (race->modifiers[mod].value.m_bonus && (lvl > race->modifiers[mod].value.m_bonus))
                cap = race->modifiers[mod].value.m_bonus;
            adj += race->modifiers[mod].value.dice * ((cap - race->modifiers[mod].lvl) / xadj);
        }

        /* Polymorphed players only get half adjustment from race */
        if (poly)
        {
            if (adj > 0) return (adj + 1) / 2;
            if (adj < 0) return (adj - 1) / 2;
        }

        return adj;
    }

    return 0;
}


int class_modifier(const struct player_class *clazz, int mod, int lvl)
{
    if (lvl >= clazz->modifiers[mod].lvl)
    {
        int adj = clazz->modifiers[mod].value.base;
        int xadj = clazz->modifiers[mod].value.sides;

        if (xadj)
        {
            int cap = lvl;

            if (clazz->modifiers[mod].value.m_bonus && (lvl > clazz->modifiers[mod].value.m_bonus))
                cap = clazz->modifiers[mod].value.m_bonus;
            adj += clazz->modifiers[mod].value.dice * ((cap - clazz->modifiers[mod].lvl) / xadj);
        }

        return adj;
    }

    return 0;
}


/*
 * Check if the player has the given PF_ flag.
 */
bool player_has(struct player *p, int flag)
{
    if (pf_has(p->race->pflags, flag) && (p->lev >= p->race->pflvl[flag])) return true;
    if (pf_has(p->clazz->pflags, flag) && (p->lev >= p->clazz->pflvl[flag])) return true;
    return false;
}


/*
 * Stat Table (STR) -- help index into the "blow" table
 */
const int adj_str_blow[STAT_RANGE] =
{
    3   /* 3 */,
    4   /* 4 */,
    5   /* 5 */,
    6   /* 6 */,
    7   /* 7 */,
    8   /* 8 */,
    9   /* 9 */,
    10  /* 10 */,
    11  /* 11 */,
    12  /* 12 */,
    13  /* 13 */,
    14  /* 14 */,
    15  /* 15 */,
    16  /* 16 */,
    17  /* 17 */,
    20  /* 18/00-18/09 */,
    30  /* 18/10-18/19 */,
    40  /* 18/20-18/29 */,
    50  /* 18/30-18/39 */,
    60  /* 18/40-18/49 */,
    70  /* 18/50-18/59 */,
    80  /* 18/60-18/69 */,
    90  /* 18/70-18/79 */,
    100 /* 18/80-18/89 */,
    110 /* 18/90-18/99 */,
    120 /* 18/100-18/109 */,
    130 /* 18/110-18/119 */,
    140 /* 18/120-18/129 */,
    150 /* 18/130-18/139 */,
    160 /* 18/140-18/149 */,
    170 /* 18/150-18/159 */,
    180 /* 18/160-18/169 */,
    190 /* 18/170-18/179 */,
    200 /* 18/180-18/189 */,
    210 /* 18/190-18/199 */,
    220 /* 18/200-18/209 */,
    230 /* 18/210-18/219 */,
    240 /* 18/220+ */
};


/*
 * Stat Table (DEX) -- index into the "blow" table
 */
static const int adj_dex_blow[STAT_RANGE] =
{
    0   /* 3 */,
    0   /* 4 */,
    0   /* 5 */,
    0   /* 6 */,
    0   /* 7 */,
    0   /* 8 */,
    0   /* 9 */,
    1   /* 10 */,
    1   /* 11 */,
    1   /* 12 */,
    1   /* 13 */,
    1   /* 14 */,
    1   /* 15 */,
    1   /* 16 */,
    2   /* 17 */,
    2   /* 18/00-18/09 */,
    2   /* 18/10-18/19 */,
    3   /* 18/20-18/29 */,
    3   /* 18/30-18/39 */,
    4   /* 18/40-18/49 */,
    4   /* 18/50-18/59 */,
    5   /* 18/60-18/69 */,
    5   /* 18/70-18/79 */,
    6   /* 18/80-18/89 */,
    6   /* 18/90-18/99 */,
    7   /* 18/100-18/109 */,
    7   /* 18/110-18/119 */,
    8   /* 18/120-18/129 */,
    8   /* 18/130-18/139 */,
    8   /* 18/140-18/149 */,
    9   /* 18/150-18/159 */,
    9   /* 18/160-18/169 */,
    9   /* 18/170-18/179 */,
    10  /* 18/180-18/189 */,
    10  /* 18/190-18/199 */,
    11  /* 18/200-18/209 */,
    11  /* 18/210-18/219 */,
    11  /* 18/220+ */
};


/*
 * This table is used to help calculate the number of blows the player can
 * make in a single round of attacks (one player turn) with a normal weapon.
 *
 * This number ranges from a single blow/round for weak players to up to six
 * blows/round for powerful warriors.
 *
 * Note that certain artifacts and ego-items give "bonus" blows/round.
 *
 * First, from the player class, we extract some values:
 *
 *   Warrior     --> num = 6; mul = 5; div = MAX(30, weapon_weight);
 *   Mage        --> num = 4; mul = 2; div = MAX(40, weapon_weight);
 *   Druid       --> num = 4; mul = 3; div = MAX(35, weapon_weight);
 *   Priest      --> num = 4; mul = 3; div = MAX(35, weapon_weight);
 *   Necromancer --> num = 4; mul = 3; div = MAX(35, weapon_weight);
 *   Paladin     --> num = 5; mul = 5; div = MAX(30, weapon_weight);
 *   Rogue       --> num = 5; mul = 4; div = MAX(20, weapon_weight);
 *   Ranger      --> num = 5; mul = 4; div = MAX(35, weapon_weight);
 *   Blackguard  --> num = 5; mul = 5; div = MAX(100, weapon_weight);
 *   Sorceror    --> num = 1; mul = 2; div = MAX(40, weapon_weight);
 *   Unbeliever  --> num = 6; mul = 5; div = MAX(30, weapon_weight);
 *   Archer      --> num = 2; mul = 3; div = MAX(35, weapon_weight);
 *   Monk        --> irrelevant (barehanded damage)
 *   Telepath    --> num = 4; mul = 3; div = MAX(35, weapon_weight);
 *   Elemntalist --> num = 3; mul = 2; div = MAX(40, weapon_weight);
 *   Summoner    --> num = 1; mul = 2; div = MAX(40, weapon_weight);
 *   Shapechangr --> num = 5; mul = 4; div = MAX(35, weapon_weight);
 *
 * To get "P", we look up the relevant "adj_str_blow[]" (see above),
 * multiply it by "mul", and then divide it by "div", rounding down.
 *
 * To get "D", we look up the relevant "adj_dex_blow[]" (see above).
 *
 * Then we look up the energy cost of each blow using "blows_table[P][D]".
 * The player gets blows/round equal to 100/this number, up to a maximum of
 * "num" blows/round, plus any "bonus" blows/round.
 */
static const int blows_table[12][12] =
{
    /* P/D */
    /* 0,   1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11+ */

    /* 0  */
    {  100, 100, 95,  85,  75,  60,  50,  42,  35,  30,  25,  23 },

    /* 1  */
    {  100, 95,  85,  75,  60,  50,  42,  35,  30,  25,  23,  21 },

    /* 2  */
    {  95,  85,  75,  60,  50,  42,  35,  30,  26,  23,  21,  20 },

    /* 3  */
    {  85,  75,  60,  50,  42,  36,  32,  28,  25,  22,  20,  19 },

    /* 4  */
    {  75,  60,  50,  42,  36,  33,  28,  25,  23,  21,  19,  18 },

    /* 5  */
    {  60,  50,  42,  36,  33,  30,  27,  24,  22,  21,  19,  17 },

    /* 6  */
    {  50,  42,  36,  33,  30,  27,  25,  23,  21,  20,  18,  17 },

    /* 7  */
    {  42,  36,  33,  30,  28,  26,  24,  22,  20,  19,  18,  17 },

    /* 8  */
    {  36,  33,  30,  28,  26,  24,  22,  21,  20,  19,  17,  16 },

    /* 9  */
    {  35,  32,  29,  26,  24,  22,  21,  20,  19,  18,  17,  16 },

    /* 10 */
    {  34,  30,  27,  25,  23,  22,  21,  20,  19,  18,  17,  16 },

    /* 11+ */
    {  33,  29,  26,  24,  22,  21,  20,  19,  18,  17,  16,  15 }
};


int calc_blows_aux(struct player *p, int weight, int stat_str, int stat_dex)
{
    int min_weight = p->clazz->min_weight;
    int blow_energy;

    /* Enforce a minimum "weight" (tenth pounds) */
    int div = ((weight < min_weight)? min_weight: weight);

    /* Get the strength vs weight */
    int str_index = (adj_str_blow[stat_str] * p->clazz->att_multiply / div);

    /* Index by dexterity */
    int dex_index = MIN(adj_dex_blow[stat_dex], 11);

    /* Maximal value */
    if (str_index > 11) str_index = 11;

    /* Use the blows table to get energy per blow */
    blow_energy = blows_table[str_index][dex_index];

    return MIN(10000 / blow_energy, 100 * p->clazz->max_attacks);
}


int calc_stat_ind(int use)
{
    int ind;

    /* Values: n/a */
    if (use <= 3) ind = 0;

    /* Values: 3, 4, ..., 17 */
    else if (use <= 18) ind = (use - 3);

    /* Ranges: 18/00-18/09, ..., 18/210-18/219 */
    else if (use <= 18+219) ind = (15 + (use - 18) / 10);

    /* Range: 18/220+ */
    else ind = (37);

    my_assert((0 <= ind) && (ind < STAT_RANGE));

    return ind;
}


/*
 * Stat Table (INT/WIS) -- failure rate adjustment
 */
const int adj_mag_stat[STAT_RANGE] =
{
    -5  /* 3 */,
    -4  /* 4 */,
    -3  /* 5 */,
    -3  /* 6 */,
    -2  /* 7 */,
    -1  /* 8 */,
     0  /* 9 */,
     0  /* 10 */,
     0  /* 11 */,
     0  /* 12 */,
     0  /* 13 */,
     1  /* 14 */,
     2  /* 15 */,
     3  /* 16 */,
     4  /* 17 */,
     5  /* 18/00-18/09 */,
     6  /* 18/10-18/19 */,
     7  /* 18/20-18/29 */,
     8  /* 18/30-18/39 */,
     9  /* 18/40-18/49 */,
    10  /* 18/50-18/59 */,
    11  /* 18/60-18/69 */,
    12  /* 18/70-18/79 */,
    15  /* 18/80-18/89 */,
    18  /* 18/90-18/99 */,
    21  /* 18/100-18/109 */,
    24  /* 18/110-18/119 */,
    27  /* 18/120-18/129 */,
    30  /* 18/130-18/139 */,
    33  /* 18/140-18/149 */,
    36  /* 18/150-18/159 */,
    39  /* 18/160-18/169 */,
    42  /* 18/170-18/179 */,
    45  /* 18/180-18/189 */,
    48  /* 18/190-18/199 */,
    51  /* 18/200-18/209 */,
    54  /* 18/210-18/219 */,
    57  /* 18/220+ */
};


/*
 * Stat Table (INT/WIS) -- minimum failure rate (percentage)
 */
const int adj_mag_fail[STAT_RANGE] =
{
    99  /* 3 */,
    99  /* 4 */,
    99  /* 5 */,
    99  /* 6 */,
    99  /* 7 */,
    50  /* 8 */,
    30  /* 9 */,
    20  /* 10 */,
    15  /* 11 */,
    12  /* 12 */,
    11  /* 13 */,
    10  /* 14 */,
    9   /* 15 */,
    8   /* 16 */,
    7   /* 17 */,
    6   /* 18/00-18/09 */,
    6   /* 18/10-18/19 */,
    5   /* 18/20-18/29 */,
    5   /* 18/30-18/39 */,
    5   /* 18/40-18/49 */,
    4   /* 18/50-18/59 */,
    4   /* 18/60-18/69 */,
    4   /* 18/70-18/79 */,
    4   /* 18/80-18/89 */,
    3   /* 18/90-18/99 */,
    3   /* 18/100-18/109 */,
    2   /* 18/110-18/119 */,
    2   /* 18/120-18/129 */,
    2   /* 18/130-18/139 */,
    2   /* 18/140-18/149 */,
    1   /* 18/150-18/159 */,
    1   /* 18/160-18/169 */,
    1   /* 18/170-18/179 */,
    1   /* 18/180-18/189 */,
    1   /* 18/190-18/199 */,
    0   /* 18/200-18/209 */,
    0   /* 18/210-18/219 */,
    0   /* 18/220+ */
};
