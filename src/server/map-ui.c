/*
 * File: map-ui.c
 * Purpose: Writing level map info to the screen
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
 * Hack -- hallucinatory monster
 */
static void hallucinatory_monster(struct player *p, bool server, uint16_t *a, char *c)
{
    while (1)
    {
        /* Select a random monster */
        int i = randint0(z_info->r_max);
        struct monster_race *race = &r_info[i];

        /* Skip non-entries */
        if (!race->name) continue;

        /* Retrieve attr/char */
        if (server)
        {
            *a = monster_x_attr[i];
            *c = monster_x_char[i];
        }
        else
        {
            *a = p->r_attr[i];
            *c = p->r_char[i];
        }

        return;
    }
}


/*
 * Hack -- hallucinatory object
 */
static void hallucinatory_object(struct player *p, bool server, uint16_t *a, char *c)
{
    while (1)
    {
        /* Select a random object */
        int i = randint0(z_info->k_max - 1) + 1;
        struct object_kind *kind = &k_info[i];

        /* Skip non-entries */
        if (!kind->name) continue;

        /* Retrieve attr/char (without flavors) */
        if (server)
        {
            *a = kind_x_attr[i];
            *c = kind_x_char[i];
        }
        else
        {
            *a = p->k_attr[i];
            *c = p->k_char[i];
        }

        /* Hack -- skip empty entries */
        if ((*a == 0) || (*c == 0)) continue;

        return;
    }
}


/*
 * Return the correct "color" of another player
 */
static uint8_t player_color(struct player *p)
{
    /* Ghosts */
    if (p->ghost) return COLOUR_L_WHITE;

    /* Cloaked rogues */
    if (p->timed[TMD_MIMIC]) return player_id2class(p->tim_mimic_what)->attr;

    /* Color is based off of class */
    return p->clazz->attr;
}


typedef struct
{
    int flag;
    uint8_t first_color;
    uint8_t second_color;
} breath_attr_struct;


/*
 * Table of breath colors.  Must match listings in a single set of
 * monster spell flags.
 *
 * The value "255" is special.  Monsters with that kind of breath
 * may be any color.
 */
static breath_attr_struct breath_to_attr[] =
{
    {RSF_BR_ACID, COLOUR_SLATE, COLOUR_L_DARK},
    {RSF_BR_ELEC, COLOUR_BLUE, COLOUR_L_BLUE},
    {RSF_BR_FIRE, COLOUR_RED, COLOUR_L_RED},
    {RSF_BR_COLD, COLOUR_WHITE, COLOUR_L_WHITE},
    {RSF_BR_POIS, COLOUR_GREEN, COLOUR_L_GREEN},
    {RSF_BR_NETH, COLOUR_L_GREEN, COLOUR_GREEN},
    {RSF_BR_LIGHT, COLOUR_ORANGE, COLOUR_YELLOW},
    {RSF_BR_DARK, COLOUR_L_DARK, COLOUR_SLATE},
    {RSF_BR_SOUN, COLOUR_YELLOW, COLOUR_L_UMBER},
    {RSF_BR_CHAO, 255, 255},
    {RSF_BR_DISE, COLOUR_VIOLET, COLOUR_L_BLUE},
    {RSF_BR_NEXU, COLOUR_VIOLET, COLOUR_L_RED},
    {RSF_BR_TIME, COLOUR_L_BLUE, COLOUR_BLUE},
    {RSF_BR_INER, COLOUR_L_WHITE, COLOUR_SLATE},
    {RSF_BR_GRAV, COLOUR_L_WHITE, COLOUR_SLATE},
    {RSF_BR_SHAR, COLOUR_UMBER, COLOUR_L_UMBER},
    {RSF_BR_PLAS, COLOUR_ORANGE, COLOUR_RED},
    {RSF_BR_WALL, COLOUR_UMBER, COLOUR_L_UMBER},
    {RSF_BR_MANA, COLOUR_L_DARK, COLOUR_SLATE},
    {RSF_BR_WATE, COLOUR_BLUE, COLOUR_SLATE}
};


/*
 * Multi-hued monsters shimmer according to their breaths.
 *
 * If a monster has only one kind of breath, it uses both colors 
 * associated with that breath.  Otherwise, it just uses the first 
 * color for any of its breaths.
 *
 * If a monster does not breath anything, it can be any color.
 */
static uint8_t multi_hued_attr_breath(struct monster_race *race)
{
    bitflag mon_breath[RSF_SIZE];
    size_t i;
    int j, breaths = 0, stored_colors = 0;
    uint8_t allowed_attrs[15];
    uint8_t second_color = 0;

    /* Monsters with no ranged attacks can be any color */
    if (!race->freq_spell) return randint1(BASIC_COLORS - 1);

    /* Hack -- require correct "breath attack" */
    rsf_copy(mon_breath, race->spell_flags);
    set_breath(mon_breath);

    /* Check breaths */
    for (i = 0; i < N_ELEMENTS(breath_to_attr); i++)
    {
        bool stored = false;
        uint8_t first_color;

        /* Don't have that breath */
        if (!rsf_has(mon_breath, breath_to_attr[i].flag)) continue;

        /* Get the first color of this breath */
        first_color = breath_to_attr[i].first_color;

        /* Monster can be of any color */
        if (first_color == 255) return randint1(BASIC_COLORS - 1);

        /* Increment the number of breaths */
        breaths++;

        /* Monsters with lots of breaths may be any color. */
        if (breaths == 6) return randint1(BASIC_COLORS - 1);

        /* Check if already stored */
        for (j = 0; j < stored_colors; j++)
        {
            /* Already stored */
            if (allowed_attrs[j] == first_color) stored = true;
        }

        /* If not, store the first color */
        if (!stored)
        {
            allowed_attrs[stored_colors] = first_color;
            stored_colors++;
        }

        /* 
         * Remember (but do not immediately store) the second color 
         * of the first breath.
         */
        if (breaths == 1) second_color = breath_to_attr[i].second_color;
    }

    /* Monsters with no breaths may be of any color. */
    if (breaths == 0) return randint1(BASIC_COLORS - 1);

    /* If monster has one breath, store the second color too. */
    if (breaths == 1)
    {
        allowed_attrs[stored_colors] = second_color;
        stored_colors++;
    }

    /* Pick a color at random */
    return (allowed_attrs[randint0(stored_colors)]);
}


static uint8_t get_flicker_attr(struct player *p, const struct monster_race *race, const uint8_t base_attr)
{
    uint8_t attr;

    /* Get the color cycled attribute, if available. */
    attr = visuals_cycler_get_attr_for_race(race, p->flicker);

    /* Fall back to the flicker attribute. */
    if (attr == BASIC_COLORS) attr = visuals_flicker_get_attr_for_frame(base_attr, p->flicker);

    /* Fall back to the static attribute if cycling fails. */
    if (attr == BASIC_COLORS) attr = base_attr;

    p->flicker++;

    return attr;
}


/*
 * Return the correct attr/char pair for any player
 */
static void player_pict(struct player *p, struct chunk *cv, struct player *q, bool server, uint16_t *a,
    char *c)
{
    int life, timefactor;
    bool show_as_number = true;
    int mode = p->use_graphics - 1;

    /* Get the "player" attr */
    if (q == p)
    {
        /* Handle himself */
        if (server) *a = monster_x_attr[0];
        else *a = p->r_attr[0];
    }
    else
    {
        /* Handle other */
        *a = player_color(q);
        if (p->use_graphics && !server)
            *a = presets[mode].player_presets[q->psex][q->clazz->cidx][q->race->ridx].a;

        /* Hack -- elementalists */
        if (!p->use_graphics && (*a == COLOUR_MULTI))
        {
            /* Set default attr */
            *a = COLOUR_VIOLET;

            /* Shimmer the player */
            if (allow_shimmer(p))
            {
                switch (randint0(5))
                {
                    case 0: *a = COLOUR_WHITE; break;
                    case 1: *a = COLOUR_RED; break;
                    case 2: *a = COLOUR_GREEN; break;
                    case 3: *a = COLOUR_BLUE; break;
                    case 4: *a = COLOUR_SLATE; break;
                }
            }
        }
    }

    /* Get the "player" char */
    if (q == p)
    {
        /* Handle himself */
        if (server) *c = monster_x_char[0];
        else *c = p->r_char[0];
    }
    else
    {
        /* Handle other */
        if (server) *c = monster_x_char[0];
        else *c = p->r_char[0];
        if (p->use_graphics && !server)
            *c = presets[mode].player_presets[q->psex][q->clazz->cidx][q->race->ridx].c;
    }

    // hc appearence of some races
    if (p->use_graphics)
    {
        struct monster_race *race;
        if (q == p)
        {
            if (streq(p->race->name, "Werewolf") && is_daytime())
            {
                race = get_race("daylight_werewolf");
                
                if (server)
                {
                    *a = monster_x_attr[race->ridx];
                    *c = monster_x_char[race->ridx];
                }
                else
                {
                    *a = p->r_attr[race->ridx];
                    *c = p->r_char[race->ridx];
                }
            }
            else if (streq(p->race->name, "Spider"))
            {
                if (p->lev < 5)
                    race = get_race("s'spider");
                else if (p->lev < 10)
                    race = get_race("s'weaver");
                else if (p->lev < 15)
                    race = get_race("s'warrior");
                else if (p->lev < 20)
                    race = get_race("s'guard");
                else if (p->lev < 25)
                    race = get_race("s'apprentice");
                else if (p->lev < 30)
                    race = get_race("s'adept");
                else if (p->lev < 35)
                    race = get_race("s'master");
                else if (p->lev < 40)
                    race = get_race("s'aranea");
                else if (p->lev < 45)
                    race = get_race("s'elder");
                else if (p->lev < 50)
                    race = get_race("s'ancient");
                else
                    race = get_race("s'lord");
                
                if (server)
                {
                    *a = monster_x_attr[race->ridx];
                    *c = monster_x_char[race->ridx];
                }
                else
                {
                    *a = p->r_attr[race->ridx];
                    *c = p->r_char[race->ridx];
                }
            }
        }
        else if (q != p)
        {
            if (streq(q->race->name, "Werewolf") && is_daytime())
            {
                race = get_race("daylight_werewolf");

                if (server)
                {
                    *a = monster_x_attr[race->ridx];
                    *c = monster_x_char[race->ridx];
                }
                else
                {
                    *a = p->r_attr[race->ridx];
                    *c = p->r_char[race->ridx];
                }
            }
            else if (streq(q->race->name, "Spider"))
            {
                if (q->lev < 5)
                    race = get_race("s'spider");
                else if (q->lev < 10)
                    race = get_race("s'weaver");
                else if (q->lev < 15)
                    race = get_race("s'warrior");
                else if (q->lev < 20)
                    race = get_race("s'guard");
                else if (q->lev < 25)
                    race = get_race("s'apprentice");
                else if (q->lev < 30)
                    race = get_race("s'adept");
                else if (q->lev < 35)
                    race = get_race("s'master");
                else if (q->lev < 40)
                    race = get_race("s'aranea");
                else if (q->lev < 45)
                    race = get_race("s'elder");
                else if (q->lev < 50)
                    race = get_race("s'ancient");
                else
                    race = get_race("s'lord");

                if (server)
                {
                    *a = monster_x_attr[race->ridx];
                    *c = monster_x_char[race->ridx];
                }
                else
                {
                    *a = p->r_attr[race->ridx];
                    *c = p->r_char[race->ridx];
                }
            }
        }
    }

    /* Handle ghosts in graphical mode */
    if (p->use_graphics && q->ghost)
    {
        struct monster_race *race = get_race("ghost");

        if (server)
        {
            *a = monster_x_attr[race->ridx];
            *c = monster_x_char[race->ridx];
        }
        else
        {
            *a = p->r_attr[race->ridx];
            *c = p->r_char[race->ridx];
        }
    }

    /* Handle polymorphed players: use monster attr/char */
    if (q->poly_race)
    {
        /* Desired attr */
        if (server) *a = monster_x_attr[q->poly_race->ridx];
        else *a = p->r_attr[q->poly_race->ridx];

        /* Desired char */
        if (server) *c = monster_x_char[q->poly_race->ridx];
        else *c = p->r_char[q->poly_race->ridx];

        /* Multi-hued monster */
        if (monster_shimmer(q->poly_race) && monster_allow_shimmer(p))
        {
            if (rf_has(q->poly_race->flags, RF_ATTR_MULTI))
                *a = multi_hued_attr_breath(q->poly_race);
            else if (rf_has(q->poly_race->flags, RF_ATTR_FLICKER))
                *a = get_flicker_attr(p, q->poly_race, (uint8_t)*a);
        }
    }

    /* Handle mimic form: use object attr/char (don't shimmer) */
    if (q->k_idx > 0)
    {
        struct object_kind *kind = &k_info[q->k_idx];

        /* Normal attr and char */
        if (server)
        {
            *a = kind_x_attr[kind->kidx];
            *c = kind_x_char[kind->kidx];
        }
        else
        {
            *a = object_kind_attr(p, kind);
            *c = object_kind_char(p, kind);
        }

        /* Set default attr */
        if (!p->use_graphics && (*a == COLOUR_MULTI)) *a = COLOUR_VIOLET;
    }

    /* Hack -- highlight party leader! */
    if (!p->use_graphics && (q != p) && is_party_owner(p, q) && OPT(p, highlight_leader) &&
        magik(50))
    {
        if (*a == COLOUR_YELLOW) *a = COLOUR_L_DARK;
        else *a = COLOUR_YELLOW;
    }

    /* Give interesting visual effects in non-graphical mode for the player */
    if (!p->use_graphics && (q == p))
    {
        /* Give a visual effect to some spells */
        if (p->timed[TMD_MANASHIELD] || p->timed[TMD_INVULN] || p->timed[TMD_DEADLY])
            *a = COLOUR_VIOLET;

        /* Handle hp_changes_color option */
        else if (OPT(p, hp_changes_color))
        {
            *a = COLOUR_WHITE;
            life = ((p->chp * 95) / (p->mhp * 10));
            if (life < 9) *a = COLOUR_YELLOW;
            if (life < 7) *a = COLOUR_ORANGE;
            if (life < 5) *a = COLOUR_L_RED;
            if (life < 3) *a = COLOUR_RED;
            show_as_number = false;
        }
    }

    /* If we are in a slow time bubble, give a visual warning */
    if (q == p)
    {
        timefactor = time_factor(p, cv);
        if (timefactor < NORMAL_TIME)
        {
            /* Initialize bubble info */
            if (p->bubble_speed >= NORMAL_TIME)
            {
                /* Reset bubble turn */
                ht_copy(&p->bubble_change, &turn);

                /* Normal -> bubble color */
                p->bubble_colour = true;

                /* Delay next blink */
                p->blink_speed = (uint32_t)cfg_fps * 2;
            }

            /* Switch between normal and bubble color */
            if (ht_diff(&turn, &p->bubble_change) > p->blink_speed)
            {
                /* Reset bubble turn */
                ht_copy(&p->bubble_change, &turn);

                /* Switch bubble color */
                p->bubble_colour = !p->bubble_colour;

                /* Remove first time delay */
                if (p->blink_speed > (uint32_t)cfg_fps) p->blink_speed = (uint32_t)cfg_fps;
            }
        }
        else
        {
            /* Reset bubble color */
            p->bubble_colour = false;

            /* Reset blink speed */
            p->blink_speed = (uint32_t)cfg_fps;
        }

        p->bubble_speed = timefactor;

        /* Use bubble color */
        if (p->bubble_colour && !p->use_graphics)
        {
            if (*a == COLOUR_WHITE) *a = COLOUR_VIOLET;
            else *a = COLOUR_WHITE;
        }
    }

    /* Display the player as a number if hp/mana is low (70% or less) */
    if (show_as_number)
    {
        /* Sorcerors protected by disruption shield get % of mana */
        if (q->timed[TMD_MANASHIELD])
            life = (q->csp * 95) / (q->msp * 10);

        /* Other players get % of hps */
        else
            life = (q->chp * 95) / (q->mhp * 10);

        /* Paranoia */
        if (life < 0) life = 0;

        /* Display a number if hp/mana is 70% or less */
        if (life < 8)
        {
            /* Desired char */
            *c = I2D(life);

            /* Use presets in gfx mode */
            if (p->use_graphics && !server)
            {
                *a = presets[mode].player_numbers[life].a;
                *c = presets[mode].player_numbers[life].c;

                /* Use bubble presets */
                if (p->bubble_colour)
                {
                    *a = presets[mode].player_bubbles[life].a;
                    *c = presets[mode].player_bubbles[life].c;
                }
            }
        }
    }
}


/*
 * Apply text lighting effects
 */
static void grid_get_attr(struct player *p, struct grid_data *g, uint16_t *a)
{
    /* Save the high-bit, since it's used for attr inversion in GCU */
    uint16_t a0 = (*a & 0x80);

    /* Remove the high bit so we can add it back again at the end */
    *a = (*a & 0x7F);

    /* Play with fg colours for terrain affected by torchlight */
    if (feat_is_torch(g->f_idx))
    {
        /* Brighten if torchlit, darken if out of LoS, super dark for UNLIGHT */
        switch (g->lighting)
        {
            case LIGHTING_TORCH:
            {
                *a = get_color(*a, ATTR_LITE, 1);
                if ((*a == COLOUR_YELLOW) && OPT(p, view_orange_light)) *a = COLOUR_ORANGE;
                break;
            }
            case LIGHTING_LIT: *a = get_color(*a, ATTR_DARK, 1); break;
            case LIGHTING_DARK: *a = get_color(*a, ATTR_DARK, 2); break;
            default: break;
        }
    }

    /* Add the attr inversion back for GCU */
    if (a0) *a = a0 | *a;

    /* Hybrid or block walls */
    if (feat_is_wall(g->f_idx))
    {
        if (OPT(p, hybrid_walls))
            *a = *a + (MULT_BG * BG_DARK);
        else if (OPT(p, solid_walls))
            *a = *a + (MULT_BG * BG_SAME);
    }
}


/*
 * Get the graphics of a listed trap.
 *
 * We should probably have better handling of stacked traps, but that can
 * wait until we do, in fact, have stacked traps under normal conditions.
 */
static bool get_trap_graphics(struct player *p, struct chunk *c, bool server,
    struct grid_data *g, uint16_t *a, char *ch)
{
    struct trap *trap = g->trap;

    /* Trap is visible */
    if (trf_has(trap->flags, TRF_VISIBLE) || trf_has(trap->flags, TRF_GLYPH))
    {
		/* Get the graphics */
        if (server)
        {
            *a = trap_x_attr[trap->kind->tidx][g->lighting];
            *ch = trap_x_char[trap->kind->tidx][g->lighting];
        }
        else
        {
            *a = p->t_attr[trap->kind->tidx][g->lighting];
            *ch = p->t_char[trap->kind->tidx][g->lighting];
        }
	
		/* We found a trap */
		return true;
    }

    /* No traps found with the requirement */
    return false;
}


/*
 * This function takes a pointer to a grid info struct describing the
 * contents of a grid location (as obtained through the function map_info)
 * and fills in the character and attr pairs for display.
 *
 * ap and cp are filled with the attr/char pair for the monster, object or
 * floor tile that is at the "top" of the grid (monsters covering objects, 
 * which cover floor, assuming all are present).
 *
 * tap and tcp are filled with the attr/char pair for the floor, regardless
 * of what is on it.  This can be used by graphical displays with
 * transparency to place an object onto a floor tile, is desired.
 *
 * Any lighting effects are also applied to these pairs, clear monsters allow
 * the underlying colour or feature to show through (ATTR_CLEAR and
 * CHAR_CLEAR), multi-hued colour-changing (ATTR_MULTI) is applied, and so on.
 *
 * NOTES:
 * This is called pretty frequently, whenever a grid on the map display
 * needs updating, so don't overcomplicate it.
 *
 * The "zero" entry in the feature/object/monster arrays are
 * used to provide "special" attr/char codes, with "monster zero" being
 * used for the player attr/char, "object zero" being used for the "pile"
 * attr/char, and "feature zero" being used for the "darkness" attr/char.
 *
 * TODO:
 * The transformations for tile colors, or brightness for the 16x16
 * tiles should be handled differently.  One possibility would be to
 * extend struct feature with attr/char definitions for the different states.
 * This will probably be done outside of the current text->graphics mappings
 * though.
 */
void grid_data_as_text(struct player *p, struct chunk *cv, bool server, struct grid_data *g,
    uint16_t *ap, char *cp, uint16_t *tap, char *tcp)
{
    uint16_t a;
    char c;
    bool use_graphics;

    /* Normal attr and char */
    if (server)
    {
        a = feat_x_attr[g->f_idx][g->lighting];
        c = feat_x_char[g->f_idx][g->lighting];
    }
    else
    {
        a = p->f_attr[g->f_idx][g->lighting];
        c = p->f_char[g->f_idx][g->lighting];
    }

    /* Hack -- use basic lighting for unmapped tiles */
    use_graphics = (p->use_graphics && (a & 0x80));

    /* Apply text lighting effects */
    if (!use_graphics) grid_get_attr(p, g, &a);

    /* Save the terrain info for the transparency effects */
    (*tap) = a;
    (*tcp) = c;

    /* There is a trap in this grid, and we are not hallucinating */
    if (g->trap && !g->hallucinate)
    {
        /* Change graphics to indicate a trap (if visible) */
        get_trap_graphics(p, cv, server, g, &a, &c);
    }

    /* If there's an object, deal with that. */
    if (g->unseen_money)
    {
        /* $$$ gets an orange star */
        if (server)
        {
            a = kind_x_attr[unknown_gold_kind->kidx];
            c = kind_x_char[unknown_gold_kind->kidx];
        }
        else
        {
            a = object_kind_attr(p, unknown_gold_kind);
            c = object_kind_char(p, unknown_gold_kind);
        }
    }
    if (g->unseen_object)
    {
        /* Everything else gets a red star */
        if (server)
        {
            a = kind_x_attr[unknown_item_kind->kidx];
            c = kind_x_char[unknown_item_kind->kidx];
        }
        else
        {
            a = object_kind_attr(p, unknown_item_kind);
            c = object_kind_char(p, unknown_item_kind);
        }
    }
    else if (g->first_obj)
    {
        if (g->hallucinate)
        {
            /* Just pick a random object to display. */
            hallucinatory_object(p, server, &a, &c);
        }
        else if (g->multiple_objects)
        {
            /* Get the "pile" feature instead */
            if (server)
            {
                a = kind_x_attr[pile_kind->kidx];
                c = kind_x_char[pile_kind->kidx];
            }
            else
            {
                a = object_kind_attr(p, pile_kind);
                c = object_kind_char(p, pile_kind);
            }
        }
        else
        {
            /* Normal attr and char */
            if (server)
            {
                a = kind_x_attr[g->first_obj->kind->kidx];
                c = kind_x_char[g->first_obj->kind->kidx];
            }
            else
            {
                a = object_attr(p, g->first_obj);
                c = object_char(p, g->first_obj);
            }

            /* Multi-hued object */
            if (object_shimmer(g->first_obj))
            {
                /* Set default attr */
                if (a == COLOUR_MULTI) a = COLOUR_VIOLET;

                /* Shimmer the object */
                if (allow_shimmer(p))
                {
                    a = randint1(BASIC_COLORS - 1);

                    /* Redraw object list if needed */
                    if (g->first_obj->attr != a) p->upkeep->redraw |= PR_ITEMLIST;
                }
            }

            /* Store the drawing attr so we can use it elsewhere */
            g->first_obj->attr = (a % MAX_COLORS);
        }
    }

    /* Handle monsters, players and trap borders */
    if (g->m_idx > 0)
    {
        struct monster *mon = (g->hallucinate? NULL: cave_monster(cv, g->m_idx));

        if (g->hallucinate)
        {
            /* Just pick a random monster to display. */
            hallucinatory_monster(p, server, &a, &c);
        }
        else if (!monster_is_camouflaged(mon))
        {
            uint8_t da;
            char dc;

            /* Desired attr & char */
            /* Hack -- use ASCII symbols instead of tiles if wanted */
            if (server || OPT(p, ascii_mon))
            {
                da = monster_x_attr[mon->race->ridx];
                dc = monster_x_char[mon->race->ridx];
            }
            else
            {
                da = p->r_attr[mon->race->ridx];
                dc = p->r_char[mon->race->ridx];
            }

            /* Special handling of attrs and/or chars */
            if (da & 0x80)
            {
                /* Special attr/char codes */
                a = da;
                c = dc;
            }
            else if (OPT(p, purple_uniques) && monster_is_shape_unique(mon))
            {
                /* Turn uniques purple if desired (violet, actually) */
                a = COLOUR_VIOLET;
                c = dc;
            }
            else if (monster_shimmer(mon->race))
            {
                /* Multi-hued monster */
                a = da;
                c = dc;

                /* Shimmer the monster */
                if (monster_allow_shimmer(p))
                {
                    /* Multi-hued attr */
                    if (rf_has(mon->race->flags, RF_ATTR_MULTI))
                        a = multi_hued_attr_breath(mon->race);
                    else if (rf_has(mon->race->flags, RF_ATTR_FLICKER))
                        a = get_flicker_attr(p, mon->race, da);

                    /* Redraw monster list if needed */
                    if (mon->attr != a) p->upkeep->redraw |= PR_MONLIST;
                }
            }
            else if (!flags_test(mon->race->flags, RF_SIZE, RF_ATTR_CLEAR, RF_CHAR_CLEAR, FLAG_END))
            {
                /* Normal monster (not "clear" in any way) */
                a = da;
                c = dc;
            }
            else if (a & 0x80)
            {
                /* Hack -- bizarre grid under monster */
                a = da;
                c = dc;
            }
            else if (!rf_has(mon->race->flags, RF_CHAR_CLEAR))
            {
                /* Normal char, Clear attr, monster */
                c = dc;
            }
            else if (!rf_has(mon->race->flags, RF_ATTR_CLEAR))
            {
                /* Normal attr, Clear char, monster */
                a = da;
            }

            /* Hack -- random mimics */
            if (mon->mimicked_k_idx)
            {
                if (server)
                {
                    if (p->use_graphics) a = kind_x_attr[mon->mimicked_k_idx];
                    c = kind_x_char[mon->mimicked_k_idx];
                }
                else
                {
                    if (p->use_graphics) a = p->k_attr[mon->mimicked_k_idx];
                    c = p->k_char[mon->mimicked_k_idx];
                }
            }

            /* Store the drawing attr so we can use it elsewhere */
            mon->attr = (a % MAX_COLORS);
        }
    }
    else if (g->is_player)
    {
        player_pict(p, cv, p, server, &a, &c);
        Send_player_pos(p);
    }
    else if (g->m_idx < 0)
    {
        if (g->hallucinate)
        {
            int16_t k_idx = player_get(0 - g->m_idx)->k_idx;

            /* Player mimics an object -- just pick a random object to display. */
            if (k_idx > 0)
                hallucinatory_object(p, server, &a, &c);

            /* Player mimics a feature -- display him normally. */
            else if (k_idx < 0)
                player_pict(p, cv, player_get(0 - g->m_idx), server, &a, &c);

            /* Just pick a random monster to display. */
            else
                hallucinatory_monster(p, server, &a, &c);
        }
        else
            player_pict(p, cv, player_get(0 - g->m_idx), server, &a, &c);
    }

    /* Result */
    (*ap) = a;
    (*cp) = c;
}


/*
 * Redraw (on the screen) the current map panel.
 *
 * The main screen will always be at least 24x80 in size.
 */
void prt_map(struct player *p)
{
    uint16_t a, ta;
    char c, tc;
    struct grid_data g;
    struct loc grid;
    int vy, vx;
    int ty, tx;
    int screen_hgt, screen_wid;
    struct chunk *cv = chunk_get(&p->wpos);

    screen_hgt = p->screen_rows / p->tile_hgt;
    screen_wid = p->screen_cols / p->tile_wid;

    /* Assume screen */
    ty = p->offset_grid.y + screen_hgt;
    tx = p->offset_grid.x + screen_wid;

    /* Dump the map */
    for (grid.y = p->offset_grid.y, vy = 1; grid.y < ty; vy++, grid.y++)
    {
        /* First clear the old stuff */
        for (grid.x = 0; grid.x < z_info->dungeon_wid; grid.x++)
        {
            p->scr_info[vy][grid.x].c = 0;
            p->scr_info[vy][grid.x].a = 0;
            p->trn_info[vy][grid.x].c = 0;
            p->trn_info[vy][grid.x].a = 0;
        }

        /* Scan the columns of row "y" */
        for (grid.x = p->offset_grid.x, vx = 0; grid.x < tx; vx++, grid.x++)
        {
            /* Check bounds */
            if (!square_in_bounds(cv, &grid)) continue;

            /* Determine what is there */
            map_info(p, cv, &grid, &g);
            grid_data_as_text(p, cv, false, &g, &a, &c, &ta, &tc);

            p->scr_info[vy][vx].c = c;
            p->scr_info[vy][vx].a = a;
            p->trn_info[vy][vx].c = tc;
            p->trn_info[vy][vx].a = ta;
        }

        /* Send that line of info */
        Send_line_info(p, vy);
    }

    /* Reset the line counter */
    Send_line_info(p, -1);
}


/*
 * Display a "small-scale" map of the dungeon in the active Term.
 *
 * Note the use of a specialized "priority" function to allow this function
 * to work with any graphic attr/char mappings, and the attempts to optimize
 * this function where possible.
 */
void display_map(struct player *p, bool subwindow)
{
    int map_hgt, map_wid;
    int row, col;
    int x, y;
    struct grid_data g;
    uint16_t a, ta;
    char c, tc;
    uint8_t tp;
    struct chunk *cv = chunk_get(&p->wpos);
    struct loc begin, end;
    struct loc_iterator iter;

    /* Priority array */
    uint8_t **mp;

    uint16_t **ma;
    char **mc;

    uint8_t *mpx;
    uint8_t *mpy;
    uint16_t *mpa;

    /* Desired map size */
    map_hgt = p->max_hgt - ROW_MAP - 1;
    map_wid = p->screen_cols;

    /* Hack -- classic mini-map */
    if (subwindow)
    {
        map_hgt = NORMAL_HGT;
        map_wid = NORMAL_WID;
    }

    /* Prevent accidents */
    if (map_hgt > cv->height) map_hgt = cv->height;
    if (map_wid > cv->width) map_wid = cv->width;

    /* Prevent accidents */
    if ((map_wid < 1) || (map_hgt < 1)) return;

    mp = mem_zalloc(cv->height * sizeof(uint8_t*));
    ma = mem_zalloc(cv->height * sizeof(uint16_t*));
    mc = mem_zalloc(cv->height * sizeof(char*));
    for (y = 0; y < cv->height; y++)
    {
        mp[y] = mem_zalloc(cv->width * sizeof(uint8_t));
        ma[y] = mem_zalloc(cv->width * sizeof(uint16_t));
        mc[y] = mem_zalloc(cv->width * sizeof(char));
    }

    if (OPT(p, highlight_players))
    {
        mpx = mem_zalloc(NumPlayers * sizeof(uint8_t*));
        mpy = mem_zalloc(NumPlayers * sizeof(uint8_t*));
        mpa = mem_zalloc(NumPlayers * sizeof(uint16_t*));
    }

    /* Initialize chars & attributes */
    for (y = 0; y < map_hgt; ++y)
    {
        for (x = 0; x < map_wid; ++x)
        {
            /* Nothing here */
            ma[y][x] = COLOUR_WHITE;
            mc[y][x] = ' ';
        }
    }

    loc_init(&begin, 0, 0);
    loc_init(&end, cv->width, cv->height);
    loc_iterator_first(&iter, &begin, &end);

    /* Analyze the actual map */
    do
    {
        row = (iter.cur.y * map_hgt / cv->height);
        col = (iter.cur.x * map_wid / cv->width);

        /* Get the attr/char at that map location */
        map_info(p, cv, &iter.cur, &g);
        grid_data_as_text(p, cv, false, &g, &a, &c, &ta, &tc);

        /* Get the priority of that attr/char */
        tp = f_info[g.f_idx].priority;

        /* Stuff on top of terrain gets higher priority */
        if ((a != ta) || (c != tc)) tp = 22;

        /* Save "best" */
        if (mp[row][col] < tp)
        {
            /* Hack -- make every grid on the map lit */
            g.lighting = LIGHTING_LIT;
            grid_data_as_text(p, cv, false, &g, &a, &c, &ta, &tc);

            /* Display stuff on top of terrain if it exists */
            if ((a != ta) || (c != tc))
            {
                ta = a;
                tc = c;
            }

            /* Save the char */
            mc[row][col] = tc;

            /* Save the attr */
            ma[row][col] = ta;

            /* Save priority */
            mp[row][col] = tp;
        }
    }
    while (loc_iterator_next_strict(&iter));

    /* Make sure all players are visible in main window */
    if (!subwindow)
    {
        int i, idx = 0, party_n = 0;

        /* Count party members */
        for (i = 1; p->party && (i <= NumPlayers); i++)
        {
            struct player *q = player_get(i);

            /* If he's not here, skip him */
            if (!wpos_eq(&q->wpos, &cv->wpos)) continue;
            if (q == p) continue;

            if (q->party == p->party) party_n++;
        }

        /* Player location */
        row = (p->grid.y * map_hgt / cv->height);
        col = (p->grid.x * map_wid / cv->width);

        player_pict(p, cv, p, false, &ta, &tc);

        /* Set the "player" attr */
        ma[row][col] = ta;

        /* Set the "player" char */
        mc[row][col] = tc;

        /* Highlight player on the map */
        Send_minipos(p, row, col, true, party_n);

        /* Highlight party members on the map */
        for (i = 1; i <= NumPlayers; i++)
        {
            struct player *q = player_get(i);

            /* If he's not here, skip him */
            if (!wpos_eq(&q->wpos, &cv->wpos)) continue;
            if (q == p) continue;

            /* Player location */
            row = (q->grid.y * map_hgt / cv->height);
            col = (q->grid.x * map_wid / cv->width);

            player_pict(p, cv, q, false, &ta, &tc);

            /* Set the "player" attr */
            ma[row][col] = ta;

            /* Set the "player" char */
            mc[row][col] = tc;

            /* Highlight party members on the map */
            if (p->party && q->party == p->party) Send_minipos(p, row, col, false, idx++);
        }
    }

    /* Activate mini-map window */
    if (subwindow) Send_term_info(p, NTERM_ACTIVATE, NTERM_WIN_MAP);

    if (subwindow && OPT(p, highlight_players))
    {
        int i;

        /* Check players */
        for (i = 1; i <= NumPlayers; i++)
        {
            struct player *q = player_get(i);

            /* If he's not here, skip him */
            if (!wpos_eq(&q->wpos, &cv->wpos))
            {
                /* Reset array */
                mpy[i] = 255;
                mpx[i] = 255;
                mpa[i] = 0;
                continue;
            }

            /* Skip hostile players */
            if (pvp_check(p, q, PVP_CHECK_ONE, true, 0x00))
            {
                /* Reset array */
                mpy[i] = 255;
                mpx[i] = 255;
                mpa[i] = 0;
                continue;
            }

            /* Player location */
            mpy[i] = (q->grid.y * map_hgt / cv->height);
            mpx[i] = (q->grid.x * map_wid / cv->width);

            if (q == p) mpa[i] = COLOUR_YELLOW;
            else if (p->party && q->party == p->party) mpa[i] = COLOUR_L_BLUE;
            else mpa[i] = COLOUR_L_UMBER;
        }
    }

    /* Display each map line in order */
    for (y = 0; y < map_hgt; ++y)
    {
        /* Display the line */
        for (x = 0; x < map_wid; ++x)
        {
            ta = ma[y][x];
            tc = mc[y][x];

            /* Display players on mini map */
            if (subwindow && OPT(p, highlight_players))
            {
                int i;

                for (i = 1; i <= NumPlayers; i++)
                {
                    if ((x == mpx[i]) && (y == mpy[i]))
                    {
                        ta = mpa[i];
                        tc = '@';
                    }
                }
            }

            /* Add the character */
            p->scr_info[y][x].c = tc;
            p->scr_info[y][x].a = ta;
        }

        /* Send that line of info */
        Send_mini_map(p, y, map_wid);

        /* Throw some nonsense into the "screen_info" so it gets cleared */
        for (x = 0; x < map_wid; x++)
        {
            p->scr_info[y][x].c = 0;
            p->scr_info[y][x].a = 255;
            p->trn_info[y][x].c = 0;
            p->trn_info[y][x].a = 0;
        }
    }

    /* Reset the line counter */
    Send_mini_map(p, -1, 0);

    /* Restore main window */
    if (subwindow) Send_term_info(p, NTERM_ACTIVATE, NTERM_WIN_OVERHEAD);

    for (y = 0; y < cv->height; y++)
    {
        mem_free(mp[y]);
        mem_free(ma[y]);
        mem_free(mc[y]);
    }
    mem_free(mp);
    mem_free(ma);
    mem_free(mc);

    if (OPT(p, highlight_players))
    {
        mem_free(mpx);
        mem_free(mpy);
        mem_free(mpa);
    }
}


static int get_wilderness_type(struct player *p, struct loc *grid)
{
    struct wild_type *w_ptr = get_wt_info_at(grid);

    /* If off the map, set to unknown type */
    if (!w_ptr) return -1;

    /* If the player hasnt been here, dont show him the terrain */
    if (!wild_is_explored(p, &w_ptr->wpos)) return -1;

    /* Determine wilderness type */
    return w_ptr->type;
}


static void wild_display_map(struct player *p)
{
    int map_hgt, map_wid;
    int col;
    int x, y;
    struct grid_data g;
    uint16_t a, ta;
    char c, tc;
    struct chunk *cv = chunk_get(&p->wpos);
    uint16_t **ma;
    char **mc;
    char buf[NORMAL_WID];

    /* Desired map size */
    map_hgt = p->max_hgt - ROW_MAP - 1;
    map_wid = p->screen_cols;

    /* Prevent accidents */
    if (map_hgt > cv->height) map_hgt = cv->height;
    if (map_wid > cv->width) map_wid = cv->width;

    /* Prevent accidents */
    if ((map_wid < 1) || (map_hgt < 1)) return;

    ma = mem_zalloc(cv->height * sizeof(uint16_t*));
    mc = mem_zalloc(cv->height * sizeof(char*));
    for (y = 0; y < cv->height; y++)
    {
        ma[y] = mem_zalloc(cv->width * sizeof(uint16_t));
        mc[y] = mem_zalloc(cv->width * sizeof(char));
    }

    /* Clear the chars and attributes */
    for (y = 0; y < map_hgt; ++y)
    {
        for (x = 0; x < map_wid; ++x)
        {
            /* Nothing here */
            ma[y][x] = COLOUR_WHITE;
            mc[y][x] = ' ';
        }
    }

    /* Analyze the actual map */
    for (y = 0; y < map_hgt; y++)
    {
        for (x = 0; x < map_wid; x++)
        {
            int type;
            struct loc grid;

            /* Location */
            loc_init(&grid, p->wpos.grid.x - map_wid / 2 + x, p->wpos.grid.y + map_hgt / 2 - y);

            /* Get wilderness type */
            type = get_wilderness_type(p, &grid);

            /* Initialize our grid_data structure */
            memset(&g, 0, sizeof(g));
            g.lighting = LIGHTING_LIT;
            g.in_view = true;

            /* Set meta terrain feature */
            if (type >= 0)
            {
                struct worldpos wpos;
                struct location *town;

                g.f_idx = wf_info[type].feat_idx;

                /* Show a down staircase if the location contains a dungeon (outside of towns) */
                wpos_init(&wpos, &grid, 0);
                if ((get_dungeon(&wpos) != NULL) && !in_town(&wpos))
                    g.f_idx = FEAT_MORE;

                /* Show town symbol if it exists */
                town = get_town(&wpos);
                if (town && town->feat) g.f_idx = town->feat;
            }

            /* Extract the current attr/char at that map location */
            grid_data_as_text(p, cv, false, &g, &a, &c, &ta, &tc);

            /* Display stuff on top of terrain if it exists */
            if ((a != ta) || (c != tc))
            {
                ta = a;
                tc = c;
            }

            /* Put the player in the center */
            if ((y == map_hgt / 2) && (x == map_wid / 2))
            {
                player_pict(p, cv, p, false, &ta, &tc);

                /* Highlight player on the wild map */
                Send_minipos(p, y, x, true, 0);
            }

            /* Save the char */
            mc[y][x] = tc;

            /* Save the attr */
            ma[y][x] = ta;
        }
    }

    /* Prepare bottom string */
    buf[0] = '\0';
    my_strcat(buf, " ", sizeof(buf));
    if (p->wpos.depth > 0)
    {
        struct worldpos wpos;

        wpos_init(&wpos, &p->wpos.grid, 0);
        my_strcat(buf, get_dungeon(&wpos)->name, sizeof(buf));
    }
    else
        wild_cat_depth(&p->wpos, buf, sizeof(buf));
    my_strcat(buf, " ", sizeof(buf));

    /* Print string at the bottom */
    col = map_wid - strlen(buf);
    for (x = col; x < map_wid; x++)
    {
        mc[map_hgt - 1][x] = buf[x - col];
        ma[map_hgt - 1][x] = COLOUR_WHITE;
    }

    /* Display each map line in order */
    for (y = 0; y < map_hgt; ++y)
    {
        /* Display the line */
        for (x = 0; x < map_wid; ++x)
        {
            /* Add the character */
            p->scr_info[y][x].c = mc[y][x];
            p->scr_info[y][x].a = ma[y][x];
        }

        /* Send that line of info */
        Send_mini_map(p, y, map_wid);

        /* Throw some nonsense into the "screen_info" so it gets cleared */
        for (x = 0; x < map_wid; x++)
        {
            p->scr_info[y][x].c = 0;
            p->scr_info[y][x].a = 255;
            p->trn_info[y][x].c = 0;
            p->trn_info[y][x].a = 0;
        }
    }

    /* Reset the line counter */
    Send_mini_map(p, -1, 0);

    for (y = 0; y < cv->height; y++)
    {
        mem_free(ma[y]);
        mem_free(mc[y]);
    }
    mem_free(ma);
    mem_free(mc);
}


/*
 * Display a "small-scale" map of the dungeon.
 *
 * Note that the "player" is always displayed on the map.
 */
void do_cmd_view_map(struct player *p)
{
    display_map(p, false);
}


/*
 * Display a "small-scale" map of the wilderness.
 *
 * Note that the "player" is always displayed on the map.
 */
void do_cmd_wild_map(struct player *p)
{
    wild_display_map(p);
}
