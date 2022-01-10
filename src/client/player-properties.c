/*
 * File: player-properties.c
 * Purpose: Class and race abilities
 *
 * Copyright (c) 1997-2020 Ben Harrison, James E. Wilson, Robert A. Koeneke,
 * Leon Marrick, Bahman Rabii, Nick McConnell
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


#include "c-angband.h"


/*
 * Ability data structures and utilities
 */


enum {
    PLAYER_FLAG_NONE,
    PLAYER_FLAG_SPECIAL,
    PLAYER_FLAG_RACE,
    PLAYER_FLAG_CLASS
};


static bool class_has_ability(const struct player_class *c, struct player_ability *ability)
{
    if (!ability->name) return false;

    if (streq(ability->type, "object"))
    {
        if (!of_has(c->flags, ability->index)) return false;
        if (c->flvl[ability->index] > player->lev) return false;
    }
    else if (streq(ability->type, "player"))
    {
        if (!pf_has(c->pflags, ability->index)) return false;
        if (c->pflvl[ability->index] > player->lev) return false;
    }
    else if (streq(ability->type, "element"))
    {
        if (c->el_info[ability->index].res_level != ability->value) return false;
        if (c->el_info[ability->index].lvl > player->lev) return false;
    }

	return true;
}


static bool race_has_ability(const struct player_race *r, struct player_ability *ability)
{
	if (!ability->name) return false;

    if (streq(ability->type, "object"))
    {
        if (!of_has(r->flags, ability->index)) return false;
        if (r->flvl[ability->index] > player->lev) return false;
    }
    else if (streq(ability->type, "player"))
    {
        if (!pf_has(r->pflags, ability->index)) return false;
        if (r->pflvl[ability->index] > player->lev) return false;
    }
    else if (streq(ability->type, "element"))
    {
        if (r->el_info[ability->index].res_level != ability->value) return false;
        if (r->el_info[ability->index].lvl > player->lev) return false;
    }

	return true;
}


/*
 * Code for viewing race and class abilities
 */


int num_abilities;
struct player_ability ability_list[32];


static char view_ability_tag(struct menu *menu, int oid)
{
	return I2A(oid);
}


/*
 * Display an entry on the gain ability menu
 */
static void view_ability_display(struct menu *menu, int oid, bool cursor, int row, int col,
    int width)
{
	char buf[NORMAL_WID];
	byte color;
	struct player_ability *choices = menu->menu_data;

	switch (choices[oid].group)
    {
	    case PLAYER_FLAG_SPECIAL:
		{
			strnfmt(buf, sizeof(buf), "Specialty Ability: %s", choices[oid].name);
			color = COLOUR_GREEN;
			break;
		}
	    case PLAYER_FLAG_CLASS:
		{
			strnfmt(buf, sizeof(buf), "Class: %s", choices[oid].name);
			color = COLOUR_UMBER;
			break;
		}
	    case PLAYER_FLAG_RACE:
		{
			strnfmt(buf, sizeof(buf), "Racial: %s", choices[oid].name);
			color = COLOUR_ORANGE;
			break;
		}
	    default:
		{
			my_strcpy(buf, "Mysterious", sizeof(buf));
			color = COLOUR_PURPLE;
		}
	}

	/* Print it */
	c_put_str(cursor? COLOUR_WHITE: color, buf, row, col);
}


/*
 * Show ability long description when browsing
 */
static void view_ability_menu_browser(int oid, void *data, const region *loc)
{
	struct player_ability *choices = data;
    char desc[MSG_LEN];

	clear_from(loc->row + loc->page_rows);
	Term_gotoxy(loc->col, loc->row + loc->page_rows);
    strnfmt(desc, sizeof(desc), "\n%s\n", (char *) choices[oid].desc);
	text_out_to_screen(COLOUR_L_BLUE, desc);
}


/*
 * Display list available specialties.
 */
static void view_ability_menu(void)
{
	struct menu menu;
	menu_iter menu_f = {view_ability_tag, 0, view_ability_display, 0, 0};
	region loc = {0, 0, 70, -99};
	char buf[NORMAL_WID];

	/* Save the screen and clear it */
	screen_save();

	/* Prompt choices */
	strnfmt(buf, sizeof(buf), "Race and class abilities (%c-%c, ESC=exit): ", I2A(0),
        I2A(num_abilities - 1));

	/* Set up the menu */
	menu_init(&menu, MN_SKIN_SCROLL, &menu_f);
	menu.header = buf;
	menu_setpriv(&menu, num_abilities, ability_list);
	loc.page_rows = num_abilities + 1;
	menu.flags = MN_DBL_TAP;
	menu.browse_hook = view_ability_menu_browser;
	region_erase_bordered(&loc);
	menu_layout(&menu, &loc);

	menu_select(&menu, 0, false);

	/* Load screen */
	screen_load(true);
}


/*
 * Browse known abilities
 */
static void view_abilities(void)
{
	struct player_ability *ability;

	/* Count the number of class powers we have */
	for (ability = player_abilities; ability; ability = ability->next)
    {
		if (class_has_ability(player->clazz, ability))
        {
			memcpy(&ability_list[num_abilities], ability, sizeof(struct player_ability));
			ability_list[num_abilities++].group = PLAYER_FLAG_CLASS;
		}
	}

	/* Count the number of race powers we have */
	for (ability = player_abilities; ability; ability = ability->next)
    {
		if (race_has_ability(player->race, ability))
        {
			memcpy(&ability_list[num_abilities], ability, sizeof(struct player_ability));
			ability_list[num_abilities++].group = PLAYER_FLAG_RACE;
		}
	}

	/* View choices until user exits */
	view_ability_menu();

	/* Exit */
	num_abilities = 0;
}


/*
 * Interact with abilities
 */
void do_cmd_abilities(void)
{
	/* View existing abilities */
	view_abilities();
}


static const char *obj_mods[] =
{
    #define STAT(a, b, c) #a,
    #include "../common/list-stats.h"
    #undef STAT
    #define OBJ_MOD(a, b, c) #a,
    #include "../common/list-object-modifiers.h"
    #undef OBJ_MOD
    NULL
};


/*
 * Interact with stats
 */
static void do_cmd_stats(char *name, struct modifier modifiers[OBJ_MOD_MAX], bitflag flags[OF_SIZE],
    byte flvl[OF_MAX], bitflag pflags[PF_SIZE], byte pflvl[PF__MAX],
    struct element_info el_info[ELEM_MAX])
{
    int mod, row = 2, n_abilities, ability_row, i;
    char buf[70];
    struct player_ability *ability;
    struct player_ability **abilities;

    screen_save();
    clear_from(0);

    /* Header */
    c_prt(COLOUR_YELLOW, name, 0, 0);

    /* Stats */
    for (mod = 0; mod < STAT_MAX; mod++)
    {
        if (modifiers[mod].value.sides)
        {
            strnfmt(buf, sizeof(buf), "%s%+3d from level %d %+3d every %d levels to level %d",
                stat_names_reduced[mod], modifiers[mod].value.base, modifiers[mod].lvl,
                modifiers[mod].value.dice, modifiers[mod].value.sides,
                modifiers[mod].value.m_bonus? modifiers[mod].value.m_bonus: 50);
        }
        else if (modifiers[mod].value.base)
        {
            strnfmt(buf, sizeof(buf), "%s%+3d from level %d", stat_names_reduced[mod],
                modifiers[mod].value.base, modifiers[mod].lvl);
        }
        else
            strnfmt(buf, sizeof(buf), "%s%+3d", stat_names_reduced[mod], 0);
        prt(buf, row++, 2);
    }

    /* Skip row */
    row++;

    /* Modifiers */
    for (mod = STAT_MAX; mod < OBJ_MOD_MAX; mod++)
    {
        if (!modifiers[mod].value.base && !modifiers[mod].value.sides) continue;

        if (modifiers[mod].value.sides)
        {
            strnfmt(buf, sizeof(buf), "%s%+4d from level %d %+4d every %d levels to level %d",
                obj_mods[mod], modifiers[mod].value.base, modifiers[mod].lvl,
                modifiers[mod].value.dice, modifiers[mod].value.sides,
                modifiers[mod].value.m_bonus? modifiers[mod].value.m_bonus: 50);
        }
        else
        {
            strnfmt(buf, sizeof(buf), "%s%+4d from level %d", obj_mods[mod],
                modifiers[mod].value.base, modifiers[mod].lvl);
        }
        prt(buf, row++, 2);
    }

    /* Skip row */
    row++;

    /* Count abilities */
    n_abilities = 0;
    for (ability = player_abilities; ability; ability = ability->next)
    {
        if (!ability->name) continue;
        if (streq(ability->type, "object"))
        {
            if (!of_has(flags, ability->index)) continue;
            n_abilities++;
        }
        else if (streq(ability->type, "player"))
        {
            if (!pf_has(pflags, ability->index)) continue;
            n_abilities++;
        }
        else if (streq(ability->type, "element"))
        {
            if (el_info[ability->index].res_level != ability->value) continue;
            n_abilities++;
        }
    }

    /* Sort abilities */
    abilities = mem_zalloc(n_abilities * sizeof(struct player_ability *));
    for (ability = player_abilities; ability; ability = ability->next)
    {
        int lvl;

        if (!ability->name) continue;
        if (streq(ability->type, "object"))
        {
            if (!of_has(flags, ability->index)) continue;
            lvl = flvl[ability->index];
        }
        else if (streq(ability->type, "player"))
        {
            if (!pf_has(pflags, ability->index)) continue;
            lvl = pflvl[ability->index];
        }
        else if (streq(ability->type, "element"))
        {
            if (el_info[ability->index].res_level != ability->value) continue;
            lvl = el_info[ability->index].lvl;
        }
        for (i = n_abilities - 1; i > 0; i--)
        {
            int curlvl;

            if (abilities[i - 1] == NULL) continue;
            if (streq(abilities[i - 1]->type, "object"))
                curlvl = flvl[abilities[i - 1]->index];
            else if (streq(abilities[i - 1]->type, "player"))
                curlvl = pflvl[abilities[i - 1]->index];
            else if (streq(abilities[i - 1]->type, "element"))
                curlvl = el_info[abilities[i - 1]->index].lvl;
            if (lvl >= curlvl) break;
            abilities[i] = abilities[i - 1];
        }
        abilities[i] = ability;
    }

    /* Abilities */
    ability_row = row;
    for (i = 0; i < n_abilities; i++)
    {
        if (streq(abilities[i]->type, "object"))
        {
            strnfmt(buf, sizeof(buf), "%s from level %d", abilities[i]->name,
                flvl[abilities[i]->index]);
        }
        else if (streq(abilities[i]->type, "player"))
        {
            if (pflvl[abilities[i]->index] > 0)
            {
                strnfmt(buf, sizeof(buf), "%s from level %d", abilities[i]->name,
                    pflvl[abilities[i]->index]);
            }
            else
                strnfmt(buf, sizeof(buf), "%s", abilities[i]->name);
        }
        else if (streq(abilities[i]->type, "element"))
        {
            strnfmt(buf, sizeof(buf), "%s from level %d", abilities[i]->name,
                el_info[abilities[i]->index].lvl);
        }

        if (row == 23)
        {
            prt("-- more --", 23, 2);
            inkey();
            clear_from(ability_row);
            row = ability_row;
        }
        prt(buf, row++, 2);
    }

    mem_free(abilities);

    inkey();
    screen_load(false);
}


/*
 * Interact with race stats
 */
void do_cmd_race_stats(struct player_race *r)
{
	do_cmd_stats(r->name, r->modifiers, r->flags, r->flvl, r->pflags, r->pflvl, r->el_info);
}


/*
 * Interact with class stats
 */
void do_cmd_class_stats(struct player_class *c)
{
	do_cmd_stats(c->name, c->modifiers, c->flags, c->flvl, c->pflags, c->pflvl, c->el_info);
}