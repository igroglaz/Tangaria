/*
 * File: player-properties.c
 * Purpose: Class and race abilities
 *
 * Copyright (c) 1997-2020 Ben Harrison, James E. Wilson, Robert A. Koeneke,
 * Leon Marrick, Bahman Rabii, Nick McConnell
 * Copyright (c) 2024 MAngband and PWMAngband Developers
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
 * Ability utilities
 */


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
        int idx, lvl = -1, res_level;

        for (idx = 0; idx < MAX_EL_INFO; idx++)
        {
            int curlvl = c->el_info[ability->index].lvl[idx];

            if (player->lev < curlvl) continue;
            if (curlvl > lvl)
            {
                lvl = curlvl;
                res_level = c->el_info[ability->index].res_level[idx];
            }
        }
        if (lvl == -1) return false;
        if (res_level != ability->value) return false;
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
        int idx, lvl = -1, res_level;

        for (idx = 0; idx < MAX_EL_INFO; idx++)
        {
            int curlvl = r->el_info[ability->index].lvl[idx];

            if (player->lev < curlvl) continue;
            if (curlvl > lvl)
            {
                lvl = curlvl;
                res_level = r->el_info[ability->index].res_level[idx];
            }
        }
        if (lvl == -1) return false;
        if (res_level != ability->value) return false;
    }

	return true;
}


/*
 * Browse known abilities
 */
static void view_abilities(void)
{
	struct player_ability *ability;
    int num_abilities = 0;
    struct player_ability ability_list[32];

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
	view_ability_menu(ability_list, num_abilities);
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
    uint8_t flvl[OF_MAX], bitflag pflags[PF_SIZE], uint8_t pflvl[PF__MAX],
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
            for (i = 0; i < MAX_EL_INFO; i++)
            {
                if (el_info[ability->index].res_level[i] != ability->value) continue;
                n_abilities++;
            }
        }
    }

    /* Sort abilities */
    abilities = mem_zalloc(n_abilities * sizeof(struct player_ability *));
    for (ability = player_abilities; ability; ability = ability->next)
    {
        int lvl, idx = -1;

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
            for (i = 0; i < MAX_EL_INFO; i++)
            {
                if (el_info[ability->index].res_level[i] != ability->value) continue;
                lvl = el_info[ability->index].lvl[i];
                idx = i;
            }
            if (idx == -1) continue;
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
                curlvl = el_info[abilities[i - 1]->index].lvl[abilities[i - 1]->idx];
            if (lvl >= curlvl) break;
            abilities[i] = abilities[i - 1];
        }
        abilities[i] = ability;
        abilities[i]->idx = idx;
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
                el_info[abilities[i]->index].lvl[abilities[i]->idx]);
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