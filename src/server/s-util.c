/*
 * File: s-util.c
 * Purpose: Utility functions
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


char color_attr_to_char(int a)
{
    if (a == COLOUR_MULTI) return 'v';
    if ((a < COLOUR_DARK) || (a > COLOUR_DEEP_L_BLUE)) return 'w';
    return color_table[a].index_char;
}


#define end_of_segment(A) ((A) == ' ' || (A) == '!' || (A) == '@' || (A) == '^')

/*
 * Parse item's inscriptions, extract "^abc" and "^a ^b ^c"
 * cases and cache them.
 */
void fill_prevent_inscription(bool *arr, quark_t quark)
{
    const char *ax;

    /* Init quark */
    ax = quark_str(quark);
    if (ax == NULL) return;

    /* Find start of segment */
    while ((ax = strchr(ax, '^')) != NULL)
    {
        /* Parse segment */
        while (ax++ != NULL)
        {
            /* Reached end of quark, stop */
            if (*ax == 0) break;

            /* Reached end of segment, stop */
            if (end_of_segment(*ax)) break;

            /* Found a "Preventing Inscription" */
            arr[MIN(127, (uint8_t)(*ax))] = true;
        }
    }
}


/*
 * Refresh combined list of player's preventive inscriptions
 * after an update to his equipment was made.
 */
void update_prevent_inscriptions(struct player *p)
{
    struct object *obj;
    int i;

    /* Clear flags */
    for (i = 0; i < 128; i++) p->prevents[i] = false;

    /* Scan equipment */
    for (i = 0; i < p->body.count; i++)
    {
        obj = slot_object(p, i);

        /* Item exists and has inscription */
        if (obj && obj->note)
        {
            /* Fill */
            fill_prevent_inscription(p->prevents, obj->note);
        }
    }
}


void alloc_info_icky(struct player *p)
{
    int i, j;

    if (p->info_icky) return;

    p->last_info_line_icky = p->last_info_line;
    p->info_icky = mem_zalloc((p->last_info_line_icky + 1) * sizeof(cave_view_type*));
    for (i = 0; i <= p->last_info_line_icky; i++)
    {
        p->info_icky[i] = mem_zalloc(NORMAL_WID * sizeof(cave_view_type));
        for (j = 0; j < NORMAL_WID; j++)
        {
            p->info_icky[i][j].a = p->info[i][j].a;
            p->info_icky[i][j].c = p->info[i][j].c;
        }
    }
}


int16_t get_last_info_line(struct player *p)
{
    if (p->info_icky) return p->last_info_line_icky;
    return p->last_info_line;
}


cave_view_type* get_info(struct player *p, int y, int x)
{
    if (p->info_icky) return &p->info_icky[y][x];
    return &p->info[y][x];
}


void free_info_icky(struct player *p)
{
    mem_nfree((void**)p->info_icky, p->last_info_line_icky + 1);
    p->info_icky = NULL;
}


void alloc_header_icky(struct player *p, const char *header)
{
    if (p->header_icky) return;

    p->header_icky = mem_zalloc(NORMAL_WID * sizeof(char));
    my_strcpy(p->header_icky, header, NORMAL_WID);
}


const char *get_header(struct player *p, const char *header)
{
    if (p->header_icky) return p->header_icky;
    return header;
}


void free_header_icky(struct player *p)
{
    if (!p->header_icky) return;

    mem_free(p->header_icky);
    p->header_icky = NULL;
}


void set_ghost_flag(struct player *p, int16_t flag, bool report)
{
    p->ghost = flag;
    if (flag)
    {
        p->timed[TMD_INVIS] = -1;
        p->upkeep->update |= PU_MONSTERS;
        p->upkeep->redraw |= PR_STATUS;
        if (report) handle_stuff(p);
    }
    else
        player_clear_timed(p, TMD_INVIS, true);
}


void notify_player_popup(struct player *p, char *header, uint16_t term, uint16_t pop)
{
    int i;

    /* Use a colored, non browsable display (popup mode) */
    Send_term_info(p, NTERM_ACTIVATE, term);
    Send_term_info(p, NTERM_CLEAR, 0);

    for (i = 0; i < p->last_info_line; i++)
        Send_remote_line(p, i);

    Send_special_other(p, header, 0, false);

    Send_term_info(p, NTERM_FRESH, pop);
    Send_term_info(p, NTERM_ACTIVATE, NTERM_WIN_OVERHEAD);
}


void notify_player(struct player *p, char *header, uint16_t term, bool symbol)
{
    /* Notify player */
    if (p->last_info_line >= p->max_hgt - 4)
    {
        int i, j;

        /* Hack -- use special term */
        Send_term_info(p, NTERM_ACTIVATE, NTERM_WIN_SPECIAL);

        for (i = 0; i < p->last_info_line; i++)
        {
            /* Hack -- we have a symbol as first character */
            if (symbol)
            {
                /* Shift data by 2 characters to the right */
                for (j = NORMAL_WID - 1; j > 2; j--) p->info[i][j].c = p->info[i][j - 2].c;

                /* Main color */
                p->info[i][2].c = (char)p->info[i][1].a;

                /* Add symbol */
                p->info[i][1].c = p->info[i][0].c;
                p->info[i][0].c = (char)p->info[i][0].a;

                /* Hack -- special coloring */
                p->info[i][0].a = COLOUR_SYMBOL;
            }

            /* Remove color info */
            else p->info[i][0].a = COLOUR_WHITE;
        }

        /* Force fullon mode (non colored, browsable display) */
        Send_special_other(p, header, 1, true);
    }
    else
        notify_player_popup(p, header, term, NTERM_POP);
}


const char *player_poss(struct player *p)
{
    switch (p->psex)
    {
        case SEX_FEMALE: return "her";
        case SEX_MALE: return "his";
    }
    return "its";
}


const char *player_self(struct player *p)
{
    switch (p->psex)
    {
        case SEX_FEMALE: return "herself";
        case SEX_MALE: return "himself";
    }
    return "itself";
}


/*** Player access functions ***/


const char *get_title(struct player *p)
{
    /* Winner */
    switch (p->total_winner)
    {
        case 1: return p->sex->winner;
        case 2: return p->sex->conqueror;
        case 3: return p->sex->killer;
    }

    /* Normal */
    return p->clazz->title[(p->lev - 1) / 5];
}


int16_t get_speed(struct player *p)
{
    int16_t speed = p->state.speed;

    /* Hack -- visually "undo" the "Stealth Mode" slowdown */
    if (p->stealthy) speed += 10;

    return speed - 110;
}


// ... state, dice.dice, dice.sides, show_mhit, show_mdam, show_shit, show_sdam
void get_plusses(struct player *p, struct player_state *state, int* dd, int* ds, int* mhit,
    int* mdam, int* shit, int* sdam)
{
    struct object *obj = equipped_item_by_slot_name(p, "weapon");

    /* Default to punching for one damage */
    *dd = *ds = 1;

    /* Initialize with the known bonuses */
    *mhit = *shit = state->to_h;
    *mdam = state->to_d;
    *sdam = 0;

    /* Ghosts do barehanded damage relative to level */
    if (p->ghost && !player_can_undead(p)) *dd = 1 + (p->lev - 1) / 2;

    /* Monks and permanently polymorphed characters do barehanded damage */
    else if (player_has(p, PF_MARTIAL_ARTS) || player_has(p, PF_PERM_SHAPE))
    {
        *dd = 1 + p->lev / 10;
        *ds = 3 + p->lev / 10;

        // boni dmg for those monks who are not dragons or other OP forms
        if (!p->poly_race && p->lev >= 15)
            *dd += 1; // do not use *dd++ - as it means *(dd++)
    }

    /* Get the wielded weapon */
    else if (obj)
    {
        /* Use displayed dice if real dice not known */
        if (obj->known->dd && obj->known->ds)
        {
            *dd = obj->dd;
            *ds = obj->ds;
        }
        else
        {
            *dd = obj->kind->dd;
            *ds = obj->kind->ds;
        }

        /* If known, add the wielded weapon bonuses */
        if (obj->known->to_h && obj->known->to_d)
        {
            *mhit += object_to_hit(obj);
            *mdam += object_to_dam(obj);
        }
    }

    /* Get the wielded bow */
    obj = equipped_item_by_slot_name(p, "shooting");
    if (obj)
    {
        /* If known, add the wielded bow bonuses */
        if (obj->known->to_h && obj->known->to_d)
        {
            *shit += object_to_hit(obj);
            *sdam += object_to_dam(obj);
        }
    }
}


int16_t get_melee_skill(struct player *p)
{
    return (p->state.skills[SKILL_TO_HIT_MELEE] * 10) / BTH_PLUS_ADJ;
}


int16_t get_ranged_skill(struct player *p)
{
    return (p->state.skills[SKILL_TO_HIT_BOW] * 10) / BTH_PLUS_ADJ;
}


uint8_t get_dtrap(struct player *p)
{
    /* Only on random levels */
    if (!random_level(&p->wpos)) return 0;

    /* Edge of detected area */
    if (square_dtrap_edge(p, chunk_get(&p->wpos), &p->grid)) return 2;

    /* Detected area (safe) */
    if (square_isdtrap(p, &p->grid)) return 1;

    /* Non-detected area (watch out) */
    return 0;
}


int get_diff(struct player *p)
{
    return weight_remaining(p);
}


struct timed_grade *get_grade(int i)
{
    return timed_effects[i].grade;
}


bool strrep(char *dest, size_t len, const char *src, const char *search, const char *replace)
{
    char *ptr = strstr(src, search);

    if (ptr)
    {
        my_strcpy(dest, src, 1 + ptr - src);
        my_strcat(dest, replace, len);
        my_strcat(dest, ptr + strlen(search), len);
        return true;
    }

    my_strcpy(dest, src, len);
    return false;
}


void strrepall(char *dest, size_t len, const char *src, const char *search, const char *replace)
{
    char *buf = mem_zalloc(len);

    my_strcpy(buf, src, len);
    while (strrep(dest, len, buf, search, replace))
        my_strcpy(buf, dest, len);

    mem_free(buf);
}


char* stristr(const char* str1, const char* str2)
{
    const char* p1 = str1;
    const char* p2 = str2;
    const char* r = ((*p2 == 0)? str1: 0);

    while (*p1 != 0 && *p2 != 0)
    {
        if (tolower((unsigned char)*p1) == tolower((unsigned char)*p2))
        {
            if (r == 0) r = p1;
            p2++;
        }
        else
        {
            p2 = str2;
            if (r != 0) p1 = r + 1;
            if (tolower((unsigned char)*p1) == tolower((unsigned char)*p2))
            {
                r = p1;
                p2++;
            }
            else
                r = 0;
        }
        p1++;
    }

    return ((*p2 == 0)? (char*)r: 0);
}


void clean_name(char *buf, char *name)
{
    char *str;
    char *dst;
    bool amp = false;

    dst = buf;
    for (str = name; *str; str++)
    {
        /* Hack -- notice '&' */
        if (*str == '&')
            amp = true;
        else
        {
            /* Lowercase string */
            if (isalpha(*str))
                *dst++ = tolower((unsigned char)*str);

            /* Other allowed symbols */
            else if (isdigit(*str) || (*str == '-') || (*str == '*') || (*str == '\''))
                *dst++ = *str;

            /* Hack -- allow space if not after '&' */
            else if ((*str == ' ') && !amp)
                *dst++ = *str;

            amp = false;
        }
    }
    *dst++ = '\0';
}
