/*
 * File: target-ui.c
 * Purpose: UI for targeting code
 *
 * Copyright (c) 1997-2014 Angband contributors
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


#include "s-angband.h"


enum target_aux_result
{
    TAR_TRUE,       /* Handler returns true */
    TAR_FALSE,      /* Handler returns false */
    TAR_BREAK,      /* Handler breaks loop */
    TAR_CONTINUE,   /* Handler restarts loop */
    TAR_NEXT        /* Call next handler */
};


/*
 * Holds state passed between target_set_interactive_aux() and the handlers
 * that help it handle different types of grids or situations. In general,
 * the handlers should only modify press (passed back from
 * target_set_interactive_aux() to target_set_interactive()) and boring
 * (modulates how later handlers act).
 */
struct target_aux_state
{
    char coord_desc[20];
    const char *phrase1;
    const char *phrase2;
    struct loc *grid;
    u32b press;
    int mode;
    bool boring;
    struct source *who;
    const char *help;
};


typedef enum target_aux_result (*target_aux_handler)(struct chunk *c, struct player *p,
    struct target_aux_state *auxst);


/*
 * Check if a UI event matches a certain keycode ('a', 'b', etc)
 */
static bool event_is_key(u32b e, u32b key)
{
    return (e == key);
}


/*
 * Display targeting help at the bottom of the screen
 */
static void target_display_help(char *help, size_t len, bool monster, bool object, bool free)
{
    /* Display help */
    my_strcpy(help, "[Press <dir>, 'p', 'q', 'r'", len);
    if (free) my_strcat(help, ", 'm'", len);
    else my_strcat(help, ", '+', '-', 'o'", len);
    if (monster || free) my_strcat(help, ", 't'", len);
    if (object) my_strcat(help, ", 'i'", len);
    my_strcat(help, ", Return, or Space]", len);
}


/*
 * Perform the minimum "whole panel" adjustment to ensure that the given
 * location is contained inside the current panel, and return true if any
 * such adjustment was performed.
 */
static bool adjust_panel_help(struct player *p, int y, int x)
{
    struct loc grid;
    int screen_hgt, screen_wid;
    int panel_wid, panel_hgt;

    screen_hgt = p->screen_rows / p->tile_hgt;
    screen_wid = p->screen_cols / p->tile_wid;

    panel_wid = screen_wid / 2;
    panel_hgt = screen_hgt / 2;

    /* Paranoia */
    if (panel_wid < 1) panel_wid = 1;
    if (panel_hgt < 1) panel_hgt = 1;

    loc_copy(&grid, &p->offset_grid);

    /* Adjust as needed */
    while (y >= grid.y + screen_hgt) grid.y += panel_hgt;
    while (y < grid.y) grid.y -= panel_hgt;

    /* Adjust as needed */
    while (x >= grid.x + screen_wid) grid.x += panel_wid;
    while (x < grid.x) grid.x -= panel_wid;

    /* Use "modify_panel" */
    return (modify_panel(p, &grid));
}


/*
 * Do we need to inform client about target info?
 */
static bool need_target_info(struct player *p, u32b query, byte step)
{
    bool need_info = false;

    /* Acknowledge */
    if (query == '\0') need_info = true;

    /* Next step */
    if (p->tt_step < step) need_info = true;

    /* Print help */
    if ((query == KC_ENTER) && (p->tt_step == step) && p->tt_help)
        need_info = true;

    /* Advance step */
    if (need_info) p->tt_step = step;

    /* Clear help */
    else p->tt_help = false;

    return need_info;
}


/*
 * Inform client about target info
 */
static bool target_info(struct player *p, struct loc *grid, const char *info, const char *help,
    u32b query)
{
    int col = grid->x - p->offset_grid.x;
    int row = grid->y - p->offset_grid.y + 1;
    bool dble = true;
    struct loc above;

    next_grid(&above, grid, DIR_N);

    /* Do nothing on quit */
    if ((query == 'q') || (query == ESCAPE)) return false;

    /* Hack -- is there something targetable above our position? */
    if (square_in_bounds_fully(chunk_get(&p->wpos), &above) && target_accept(p, &above))
        dble = false;

    /* Display help info */
    if (p->tt_help)
        Send_target_info(p, col, row, dble, help);

    /* Display target info */
    else
        Send_target_info(p, col, row, dble, info);

    /* Toggle help */
    p->tt_help = !p->tt_help;

    return true;
}


/*
 * Help target_set_interactive_aux(): reset the state for another pass
 * through the handlers.
 */
static enum target_aux_result aux_reinit(struct chunk *c, struct player *p,
    struct target_aux_state *auxst)
{
    /* Bail if looking at a forbidden grid. Don't run any more handlers. */
    if (!square_in_bounds(c, auxst->grid)) return TAR_BREAK;

    /* Assume boring */
    auxst->boring = true;

    /* Looking at the player's grid */
    if (auxst->who->player && (auxst->who->player == p))
    {
        auxst->phrase1 = "You are ";
        auxst->phrase2 = "on ";
    }

    /* Default */
    else
    {
        auxst->phrase1 = "You see ";
        auxst->phrase2 = "";
    }

    return TAR_NEXT;
}


/*
 * Help target_set_interactive_aux(): handle hallucination.
 */
static enum target_aux_result aux_hallucinate(struct chunk *c, struct player *p,
    struct target_aux_state *auxst)
{
    const char *name_strange = "something strange";
    char out_val[256];

    /* Hallucination messes things up */
    if (!p->timed[TMD_IMAGE]) return TAR_NEXT;

    /* Display a message */
    strnfmt(out_val, sizeof(out_val), "%s%s%s, %s.", auxst->phrase1, auxst->phrase2, name_strange,
        auxst->coord_desc);

    /* Inform client */
    if (need_target_info(p, auxst->press, TARGET_NONE))
    {
        return (target_info(p, auxst->grid, out_val, auxst->help, auxst->press)? TAR_TRUE:
            TAR_FALSE);
    }

    /* Stop on everything but "return" */
    if (auxst->press == KC_ENTER)
    {
        auxst->press = '\0';
        return TAR_CONTINUE;
    }

    return TAR_FALSE;
}


/*
 * Help target_set_interactive_aux(): handle players.
 *
 * Note that if a player is in the grid, we update both the player
 * recall info and the health bar info to track that player.
 */
static enum target_aux_result aux_player(struct chunk *c, struct player *p,
    struct target_aux_state *auxst)
{
    char player_name[NORMAL_WID];
    char out_val[256];
    bool recall;

    /* Actual visible players */
    if (!auxst->who->player) return TAR_NEXT;
    if (auxst->who->player == p) return TAR_NEXT;
    if (!player_is_visible(p, auxst->who->idx)) return TAR_NEXT;

    /* Not boring */
    auxst->boring = false;

    /* Unaware players get a pseudo description */
    if (auxst->who->player->k_idx)
    {
        const char *s3 = "";

        /* Acting as an object: get a pseudo object description */
        if (auxst->who->player->k_idx > 0)
        {
            struct object_kind *kind = &k_info[auxst->who->player->k_idx];
            struct object *fake = object_new();

            object_prep(p, c, fake, kind, 0, MINIMISE);
            if (tval_is_money_k(kind)) fake->pval = 1;
            object_desc(p, player_name, sizeof(player_name), fake, ODESC_PREFIX | ODESC_BASE);
            object_delete(&fake);
        }

        /* Acting as a feature: get a pseudo feature description */
        else
        {
            int feat = feat_pseudo(auxst->who->player->poly_race->d_char);

            my_strcpy(player_name, f_info[feat].name, sizeof(player_name));
            s3 = (is_a_vowel(player_name[0])? "an ": "a ");
        }

        /* Describe the player */
        strnfmt(out_val, sizeof(out_val), "%s%s%s%s, %s.", auxst->phrase1, auxst->phrase2, s3,
            player_name, auxst->coord_desc);

        /* Inform client */
        if (need_target_info(p, auxst->press, TARGET_MON))
        {
            return (target_info(p, auxst->grid, out_val, auxst->help, auxst->press)? TAR_TRUE:
                TAR_FALSE);
        }

        /* Stop on everything but "return" */
        if (auxst->press != KC_ENTER) return TAR_BREAK;

        /* Paranoia */
        return TAR_TRUE;
    }

    /* Get the player name */
    strnfmt(player_name, sizeof(player_name), "%s the %s %s", auxst->who->player->name,
        auxst->who->player->race->name, auxst->who->player->clazz->name);

    /* Track this player */
    monster_race_track(p->upkeep, auxst->who);
    health_track(p->upkeep, auxst->who);
    cursor_track(p, auxst->who);
    handle_stuff(p);

    /* Interact */
    recall = false;
    if ((auxst->press == 'r') && (p->tt_step == TARGET_MON))
        recall = true;

    /* Recall or target */
    if (recall)
    {
        do_cmd_describe(p);
        return TAR_FALSE;
    }
    else
    {
        char buf[NORMAL_WID];

        /* Describe the player */
        look_player_desc(auxst->who->player, buf, sizeof(buf));

        /* Describe, and prompt for recall */
        strnfmt(out_val, sizeof(out_val), "%s%s%s (%s), %s.", auxst->phrase1, auxst->phrase2,
            player_name, buf, auxst->coord_desc);

        /* Inform client */
        if (need_target_info(p, auxst->press, TARGET_MON))
        {
            return (target_info(p, auxst->grid, out_val, auxst->help, auxst->press)? TAR_TRUE:
                TAR_FALSE);
        }
    }

    /* Stop on everything but "return"/"space" */
    if ((auxst->press != KC_ENTER) && (auxst->press != ' ')) return TAR_BREAK;

    /* Sometimes stop at "space" key */
    if ((auxst->press == ' ') && !(auxst->mode & (TARGET_LOOK))) return TAR_BREAK;

    /* Take account of gender */
    if (auxst->who->player->psex == SEX_FEMALE) auxst->phrase1 = "She is ";
    else if (auxst->who->player->psex == SEX_MALE) auxst->phrase1 = "He is ";
    else auxst->phrase1 = "It is ";

    /* Use a preposition */
    auxst->phrase2 = "on ";

    return TAR_NEXT;
}


/*
 * Help target_set_interactive_aux(): handle monsters.
 *
 * Note that if a monster is in the grid, we update both the monster
 * recall info and the health bar info to track that monster.
 */
static enum target_aux_result aux_monster(struct chunk *c, struct player *p,
    struct target_aux_state *auxst)
{
    char m_name[NORMAL_WID];
    char out_val[256];
    bool recall;

    /* Actual visible monsters */
    if (!auxst->who->monster) return TAR_NEXT;
    if (!monster_is_obvious(p, auxst->who->idx, auxst->who->monster)) return TAR_NEXT;

    /* Not boring */
    auxst->boring = false;

    /* Get the monster name ("a kobold") */
    monster_desc(p, m_name, sizeof(m_name), auxst->who->monster, MDESC_IND_VIS);

    /* Track this monster */
    monster_race_track(p->upkeep, auxst->who);
    health_track(p->upkeep, auxst->who);
    cursor_track(p, auxst->who);
    handle_stuff(p);

    /* Interact */
    recall = false;
    if ((auxst->press == 'r') && (p->tt_step == TARGET_MON))
        recall = true;

    /* Recall or target */
    if (recall)
    {
        do_cmd_describe(p);
        return TAR_FALSE;
    }
    else
    {
        char buf[NORMAL_WID];

        /* Describe the monster */
        look_mon_desc(auxst->who->monster, buf, sizeof(buf));

        /* Describe, and prompt for recall */
        strnfmt(out_val, sizeof(out_val), "%s%s%s (%s), %s.", auxst->phrase1, auxst->phrase2,
            m_name, buf, auxst->coord_desc);

        /* Inform client */
        if (need_target_info(p, auxst->press, TARGET_MON))
        {
            return (target_info(p, auxst->grid, out_val, auxst->help, auxst->press)? TAR_TRUE:
                TAR_FALSE);
        }
    }

    /* Stop on everything but "return"/"space" */
    if ((auxst->press != KC_ENTER) && (auxst->press != ' ')) return TAR_BREAK;

    /* Sometimes stop at "space" key */
    if ((auxst->press == ' ') && !(auxst->mode & (TARGET_LOOK))) return TAR_BREAK;

    /* Take account of gender */
    if (rf_has(auxst->who->monster->race->flags, RF_FEMALE)) auxst->phrase1 = "She is ";
    else if (rf_has(auxst->who->monster->race->flags, RF_MALE)) auxst->phrase1 = "He is ";
    else auxst->phrase1 = "It is ";

    /* Describe carried objects (DMs only) */
    if (is_dm_p(p))
    {
        /* Use a verb */
        auxst->phrase2 = "carrying ";

        /* Change the intro */
        if (p->tt_o) auxst->phrase2 = "also carrying ";

        /* Scan all objects being carried */
        if (!p->tt_o) p->tt_o = auxst->who->monster->held_obj;
        else p->tt_o = p->tt_o->next;
        if (p->tt_o)
        {
            char o_name[NORMAL_WID];

            /* Obtain an object description */
            object_desc(p, o_name, sizeof(o_name), p->tt_o, ODESC_PREFIX | ODESC_FULL);

            /* Describe the object */
            strnfmt(out_val, sizeof(out_val), "%s%s%s, %s.", auxst->phrase1, auxst->phrase2, o_name,
                auxst->coord_desc);

            /* Inform client */
            return (target_info(p, auxst->grid, out_val, auxst->help, auxst->press)? TAR_TRUE:
                TAR_FALSE);
        }
    }

    /* Use a preposition */
    auxst->phrase2 = "on ";

    return TAR_NEXT;
}


/*
 * Help target_set_interactive_aux(): handle visible traps.
 */
static enum target_aux_result aux_trap(struct chunk *c, struct player *p,
    struct target_aux_state *auxst)
{
    struct trap *trap;
    char out_val[256];
    const char *lphrase3;

    /* A trap */
    trap = square_known_trap(p, c, auxst->grid);
    if (trap)
    {
        bool recall = false;

        /* Not boring */
        auxst->boring = false;

        /* Pick proper indefinite article */
        lphrase3 = (is_a_vowel(trap->kind->desc[0])? "an ": "a ");

        /* Interact */
        if ((auxst->press == 'r') && (p->tt_step == TARGET_TRAP))
            recall = true;

        /* Recall */
        if (recall)
        {
            /* Recall on screen */
            describe_trap(p, trap);
            return TAR_FALSE;
        }

        /* Normal */
        else
        {
            /* Describe, and prompt for recall */
            strnfmt(out_val, sizeof(out_val), "%s%s%s%s, %s.", auxst->phrase1, auxst->phrase2,
                lphrase3, trap->kind->desc, auxst->coord_desc);

            /* Inform client */
            if (need_target_info(p, auxst->press, TARGET_TRAP))
            {
                return (target_info(p, auxst->grid, out_val, auxst->help, auxst->press)? TAR_TRUE:
                    TAR_FALSE);
            }
        }

        /* Stop on everything but "return"/"space" */
        if ((auxst->press != KC_ENTER) && (auxst->press != ' ')) return TAR_BREAK;

        /* Sometimes stop at "space" key */
        if ((auxst->press == ' ') && !(auxst->mode & (TARGET_LOOK))) return TAR_BREAK;
    }

    /* Double break */
    if (square_known_trap(p, c, auxst->grid)) return TAR_BREAK;

    return TAR_NEXT;
}


/*
 * Help target_set_interactive_aux(): handle objects.
 */
static enum target_aux_result aux_object(struct chunk *c, struct player *p,
    struct target_aux_state *auxst)
{
    int floor_max = z_info->floor_size;
    struct object **floor_list = mem_zalloc(floor_max * sizeof(*floor_list));
    char out_val[256];
    int floor_num;

    /* Scan all sensed objects in the grid */
    floor_num = scan_distant_floor(p, c, floor_list, floor_max, auxst->grid);
    if (floor_num == 0)
    {
        mem_free(floor_list);
        return TAR_NEXT;
    }

    /* Not boring */
    auxst->boring = false;

    track_object(p->upkeep, floor_list[0]);
    handle_stuff(p);

    /* If there is more than one item... */
    if (floor_num > 1)
    {
        /* Describe the pile */
        strnfmt(out_val, sizeof(out_val), "%s%sa pile of %d objects, %s.", auxst->phrase1,
            auxst->phrase2, floor_num, auxst->coord_desc);

        /* Inform client */
        if (need_target_info(p, auxst->press, TARGET_OBJ))
        {
            mem_free(floor_list);
            return (target_info(p, auxst->grid, out_val, auxst->help, auxst->press)? TAR_TRUE:
                    TAR_FALSE);
        }

        /* Display objects */
        if (auxst->press == 'r')
        {
            msg(p, "You see:");
            display_floor(p, c, floor_list, floor_num, false);
            show_floor(p, OLIST_WEIGHT | OLIST_GOLD);
            mem_free(floor_list);
            return TAR_FALSE;
        }

        /* Done */
        mem_free(floor_list);
        return TAR_BREAK;
    }
    else
    {
        bool recall = false;
        char o_name[NORMAL_WID];

        /* Only one object to display */
        struct object *obj = floor_list[0];

        /* Not boring */
        auxst->boring = false;

        /* Obtain an object description */
        object_desc(p, o_name, sizeof(o_name), obj, ODESC_PREFIX | ODESC_FULL);

        /* Interact */
        if ((auxst->press == 'r') && (p->tt_step == TARGET_OBJ))
            recall = true;

        /* Recall */
        if (recall)
        {
            /* Recall on screen */
            display_object_recall_interactive(p, obj, o_name);
            mem_free(floor_list);
            return TAR_FALSE;
        }

        /* Normal */
        else
        {
            /* Describe, and prompt for recall */
            strnfmt(out_val, sizeof(out_val), "%s%s%s, %s.", auxst->phrase1, auxst->phrase2, o_name,
                auxst->coord_desc);

            /* Inform client */
            if (need_target_info(p, auxst->press, TARGET_OBJ))
            {
                mem_free(floor_list);
                return (target_info(p, auxst->grid, out_val, auxst->help, auxst->press)? TAR_TRUE:
                        TAR_FALSE);
            }
        }

        /* Stop on everything but "return"/"space" */
        if ((auxst->press != KC_ENTER) && (auxst->press != ' '))
        {
            mem_free(floor_list);
            return TAR_BREAK;
        }

        /* Sometimes stop at "space" key */
        if ((auxst->press == ' ') && !(auxst->mode & (TARGET_LOOK)))
        {
            mem_free(floor_list);
            return TAR_BREAK;
        }

        /* Plurals */
        auxst->phrase1 = VERB_AGREEMENT(obj->number, "It is ", "They are ");

        /* Preposition */
        auxst->phrase2 = "on ";
    }

    mem_free(floor_list);
    return TAR_NEXT;
}


/*
 * Help target_set_interactive_aux(): handle terrain.
 */
static enum target_aux_result aux_terrain(struct chunk *c, struct player *p,
    struct target_aux_state *auxst)
{
    const char *name, *lphrase3;
    char out_val[256];
    int feat = square_apparent_feat(p, c, auxst->grid);
    bool recall = false;
    struct location *dungeon = get_dungeon(&p->wpos);

    if (!auxst->boring && !feat_isterrain(feat)) return TAR_NEXT;

    /* Terrain feature if needed */
    name = square_apparent_name(p, c, auxst->grid);

    /* Pick a preposition if needed */
    if (*auxst->phrase2) auxst->phrase2 = square_apparent_look_in_preposition(p, c, auxst->grid);

    /* Pick prefix for the name */
    lphrase3 = square_apparent_look_prefix(p, c, auxst->grid);

    /* Hack -- dungeon entrance */
    if (dungeon && square_isdownstairs(c, auxst->grid))
    {
        lphrase3 = "the entrance to ";
        name = dungeon->name;
    }

    /* Interact */
    if ((auxst->press == 'r') && (p->tt_step == TARGET_FEAT))
        recall = true;

    /* Recall */
    if (recall)
    {
        /* Recall on screen */
        describe_feat(p, &f_info[feat]);
        return TAR_FALSE;
    }

    /* Normal */
    else
    {
        /* Message */
        strnfmt(out_val, sizeof(out_val), "%s%s%s%s, %s.", auxst->phrase1, auxst->phrase2, lphrase3,
            name, auxst->coord_desc);

        /* Inform client */
        if (need_target_info(p, auxst->press, TARGET_FEAT))
        {
            return (target_info(p, auxst->grid, out_val, auxst->help, auxst->press)? TAR_TRUE:
                TAR_FALSE);
        }
    }

    /* Stop on everything but "return"/"space" */
    if ((auxst->press != KC_ENTER) && (auxst->press != ' ')) return TAR_BREAK;

    return TAR_NEXT;
}


/*
 * Help target_set_interactive_aux(): check what's in press to decide whether
 * to do another pass through the handlers.
 */
static enum target_aux_result aux_wrapup(struct chunk *c, struct player *p,
    struct target_aux_state *auxst)
{
    /* Stop on everything but "return" */
    if (auxst->press != KC_ENTER) return TAR_BREAK;

    /* Paranoia */
    return TAR_TRUE;
}


/*
 * Examine a grid, return a keypress.
 *
 * The "mode" argument contains the "TARGET_LOOK" bit flag, which
 * indicates that the "space" key should scan through the contents
 * of the grid, instead of simply returning immediately.  This lets
 * the "look" command get complete information, without making the
 * "target" command annoying.
 *
 * This function correctly handles multiple objects per grid, and objects
 * and terrain features in the same grid, though the latter never happens.
 *
 * This function must handle blindness/hallucination.
 */
static bool target_set_interactive_aux(struct player *p, struct loc *grid, int mode,
    const char *help, u32b query)
{
    /*
     * If there's other types to be handled, insert a function to do so
     * between aux_hallucinate and aux_wrapup. Because each handler
     * can signal that the sequence be halted, these are ordered in
     * decreasing order of precedence.
     */
    target_aux_handler handlers[] =
    {
        aux_reinit,
        aux_hallucinate,
        aux_player,
        aux_monster,
        aux_trap,
        aux_object,
        aux_terrain,
        aux_wrapup
    };
    struct target_aux_state auxst;
    int ihandler;
    int tries = 200;
    struct chunk *c = chunk_get(&p->wpos);
    struct source who_body;

    auxst.mode = mode;
    auxst.press = query;
    auxst.who = &who_body;
    square_actor(c, grid, auxst.who);
    auxst.help = help;

    /* Describe the square location */
    auxst.grid = grid;
    grid_desc(p, auxst.coord_desc, sizeof(auxst.coord_desc), grid);

    /* Apply the handlers in order until done */
    ihandler = 0;
    while (tries--)
    {
        enum target_aux_result result = (*handlers[ihandler])(c, p, &auxst);

        if (result == TAR_TRUE) return true;
        if (result == TAR_FALSE) return false;
        if (result == TAR_BREAK) break;
        if (result == TAR_CONTINUE) continue;
        ++ihandler;
        if (ihandler >= (int) N_ELEMENTS(handlers)) ihandler = 0;
    }

    /* Paranoia */
    if (!tries) plog_fmt("Infinite loop in target_set_interactive_aux: %c", query);

    /* Keep going */
    return false;
}


/*
 * Draw a visible path over the squares between (x1,y1) and (x2,y2).
 *
 * The path consists of "*", which are white except where there is a
 * monster, object or feature in the grid.
 *
 * This routine has (at least) three weaknesses:
 * - remembered objects/walls which are no longer present are not shown,
 * - squares which (e.g.) the player has walked through in the dark are
 *   treated as unknown space.
 * - walls which appear strange due to hallucination aren't treated correctly.
 *
 * The first two result from information being lost from the dungeon arrays,
 * which requires changes elsewhere.
 */
int draw_path(struct player *p, u16b path_n, struct loc *path_g, struct loc *grid)
{
    int i;
    bool on_screen;
    bool pastknown = false;
    struct chunk *c = chunk_get(&p->wpos);

    /* No path, so do nothing. */
    if (path_n < 1) return 0;

    /*
     * The starting square is never drawn, but notice if it is being
     * displayed. In theory, it could be the last such square.
     */
    on_screen = panel_contains(p, grid);

    /* Draw the path. */
    for (i = 0; i < path_n; i++)
    {
        byte colour;
        struct object *obj = square_known_pile(p, c, &path_g[i]);
        struct source who_body;
        struct source *who = &who_body;

        /*
         * As path[] is a straight line and the screen is oblong,
         * there is only section of path[] on-screen.
         * If the square being drawn is visible, this is part of it.
         * If none of it has been drawn, continue until some of it
         * is found or the last square is reached.
         * If some of it has been drawn, finish now as there are no
         * more visible squares to draw.
         */
        if (panel_contains(p, &path_g[i])) on_screen = true;
        else if (on_screen) break;
        else continue;

        square_actor(c, &path_g[i], who);

        /* Once we pass an unknown square, we no longer know if we will reach later squares */
        if (pastknown)
            colour = COLOUR_L_DARK;

        /* Choose a colour (monsters). */
        else if (who->monster && monster_is_visible(p, who->idx))
        {
            /* Mimics act as objects */
            if (monster_is_mimicking(who->monster))
                colour = COLOUR_YELLOW;

            /* Visible monsters are red. */
            else
                colour = COLOUR_L_RED;
        }

        /* Choose a colour (players). */
        else if (who->player && player_is_visible(p, who->idx))
        {
            /* Player mimics act as objects (or features) */
            if (who->player->k_idx > 0)
                colour = COLOUR_YELLOW;
            else if (who->player->k_idx < 0)
                colour = COLOUR_WHITE;

            /* Visible players are red. */
            else
                colour = COLOUR_L_RED;
        }

        /* Known objects are yellow. */
        else if (obj)
            colour = COLOUR_YELLOW;

        /* Known walls are blue. */
        else if (!square_isprojectable(c, &path_g[i]) &&
            (square_isknown(p, &path_g[i]) || square_isseen(p, &path_g[i])))
        {
            colour = COLOUR_BLUE;
        }

        /* Unknown squares are grey. */
        else if (!square_isknown(p, &path_g[i]) && !square_isseen(p, &path_g[i]))
        {
            pastknown = true;
            colour = COLOUR_L_DARK;
        }

        /* Unoccupied squares are white. */
        else
            colour = COLOUR_WHITE;

        /* Draw the path segment */
        draw_path_grid(p, &path_g[i], colour, '*');
    }

    /* Flush and wait (delay for consistency) */
    if (i) Send_flush(p, true, p->do_visuals? 4: 1);
    else Send_flush(p, true, 0);

    return i;
}


/*
 * Load the attr/char at each point along "path" which is on screen.
 */
void load_path(struct player *p, u16b path_n, struct loc *path_g)
{
    int i;

    for (i = 0; i < path_n; i++)
    {
        if (!panel_contains(p, &path_g[i])) continue;
        square_light_spot_aux(p, chunk_get(&p->wpos), &path_g[i]);
    }

    Send_flush(p, true, 0);
    p->path_drawn = false;
}


/*
 * Return true if the object pile contains the player's tracked object
 *
 * PWMAngband: simplified by using the "known" pile and checking vs visible and known objects
 */
static bool pile_is_tracked(struct player *p, struct chunk *c, struct loc *grid)
{
    struct object *obj;

    for (obj = square_known_pile(p, c, grid); obj; obj = obj->next)
    {
        /* Must be known and visible */
        if (is_unknown(obj) || ignore_item_ok(p, obj)) continue;

        if (p->upkeep->object == obj) return true;
    }

    return false;
}


/*
 * Extract a direction (or zero) from a character
 */
static int target_dir(u32b ch)
{
    int d = 0;

    /* Already a direction? */
    if (ch <= 9)
        d = ch;
    else if (isdigit((unsigned char)ch))
        d = D2I(ch);
    else if (isarrow(ch))
    {
        switch (ch)
        {
            case ARROW_DOWN:  d = 2; break;
            case ARROW_LEFT:  d = 4; break;
            case ARROW_RIGHT: d = 6; break;
            case ARROW_UP:    d = 8; break;
        }
    }

    /* Paranoia */
    if (d == 5) d = 0;

    /* Return direction */
    return (d);
}


static void set_target_index(struct player *p, s16b index)
{
    p->target_index = index;
    p->tt_o = NULL;
}


/*
 * Handle "target" and "look". May be called from commands or "get_aim_dir()".
 *
 * Currently, when "interesting" grids are being used, and a directional key is
 * pressed, we only scroll by a single panel, in the direction requested, and
 * check for any interesting grids on that panel. The "correct" solution would
 * actually involve scanning a larger set of grids, including ones in panels
 * which are adjacent to the one currently scanned, but this is overkill for
 * this function.
 *
 * Targetting/observing an "outer border grid" may induce problems, so this is
 * not currently allowed.
 *
 * The player can use the direction keys to move among "interesting"
 * grids in a heuristic manner, or the "space", "+", and "-" keys to
 * move through the "interesting" grids in a sequential manner, or
 * can enter "location" mode, and use the direction keys to move one
 * grid at a time in any direction.  The "t" (set target) command will
 * only target a monster (as opposed to a location) if the monster is
 * target_able and the "interesting" mode is being used.
 *
 * The current grid is described using the "look" method above, and
 * a new command may be entered at any time, but note that if the
 * "TARGET_LOOK" bit flag is set (or if we are in "location" mode,
 * where "space" has no obvious meaning) then "space" will scan
 * through the description of the current grid until done, instead
 * of immediately jumping to the next "interesting" grid.  This
 * allows the "target" command to retain its old semantics.
 *
 * The "*", "+", and "-" keys may always be used to jump immediately
 * to the next (or previous) interesting grid, in the proper mode.
 *
 * The "return" key may always be used to scan through a complete
 * grid description (forever).
 *
 * This command will cancel any old target, even if used from
 * inside the "look" command.
 *
 * 'mode' is one of TARGET_LOOK or TARGET_KILL.
 * 'x' and 'y' are the initial position of the target to be highlighted,
 * or -1 if no location is specified.
 * Returns true if a target has been successfully set, false otherwise.
 */
bool target_set_interactive(struct player *p, int mode, u32b press)
{
    bool done = false;
    struct point_set *targets;
    char help[NORMAL_WID];
    struct target old_target_body;
    struct target *old_target = &old_target_body;
    bool auto_target = false;
    int tries = 200;
    struct chunk *c = chunk_get(&p->wpos);

    /* Paranoia */
    if (!c) return false;

    /* Remove old targeting path */
    if (p->path_drawn) load_path(p, p->path_n, p->path_g);

    /* Hack -- auto-target if requested */
    if ((mode & (TARGET_AIM)) && OPT(p, use_old_target) && target_okay(p))
    {
        memcpy(old_target, &p->target, sizeof(struct target));
        auto_target = true;
    }

    if (press == '\0')
    {
        p->show_interesting = true;
        p->tt_step = TARGET_NONE;
        p->tt_help = false;
    }

    /* Start on the player */
    if (press == '\0')
    {
        loc_copy(&p->tt_grid, &p->grid);

        /* Hack -- auto-target if requested */
        if (auto_target) loc_copy(&p->tt_grid, &old_target->grid);
    }

    /* Cancel target */
	target_set_monster(p, NULL);

    /* Cancel tracking */
    cursor_track(p, NULL);

    /* Prepare the target set */
    targets = target_get_monsters(p, mode, true);

    /* Start near the player */
    if (press == '\0')
    {
        set_target_index(p, 0);

        /* Hack -- auto-target if requested */
        if (auto_target)
        {
            int i;

            /* Find the old target */
            for (i = 0; i < point_set_size(targets); i++)
            {
                struct source temp_who_body;
                struct source *temp_who = &temp_who_body;

                square_actor(c, &targets->pts[i].grid, temp_who);

                if (source_equal(temp_who, &old_target->target_who))
                {
                    set_target_index(p, i);
                    break;
                }
            }
        }
    }

    /* Interact */
    while (tries-- && !done)
    {
        bool use_interesting_mode, use_free_mode, has_target, has_object;
        struct source who_body;
        struct source *who = &who_body;

        /* Paranoia: grids could have changed! */
        if (p->target_index >= point_set_size(targets))
            set_target_index(p, point_set_size(targets) - 1);
        if (p->target_index < 0) set_target_index(p, 0);

#ifdef NOTARGET_PROMPT
        /* No targets */
        if (p->show_interesting && !point_set_size(targets))
        {
            /* Analyze */
            switch (press)
            {
                case ESCAPE:
                case 'q': break;

                case 'p':
                {
                    p->show_interesting = false;
                    break;
                }

                default:
                {
                    int col = p->grid.x - p->offset_grid.x;
                    int row = p->grid.y - p->offset_grid.y + 1;
                    bool dble = true;
                    struct loc grid;

                    loc_init(&grid, p->grid.x, p->grid.y - 1);

                    /* Hack -- is there something targetable above our position? */
                    if (square_in_bounds_fully(c, &grid) && target_accept(p, &grid))
                        dble = false;

                    Send_target_info(p, col, row, dble, "Nothing to target. [p,q]");
                    point_set_dispose(targets);
                    return false;
                }
            }
        }
#endif

        use_interesting_mode = p->show_interesting && point_set_size(targets);
        use_free_mode = !use_interesting_mode;

        /* Use an interesting grid if requested and there are any */
        if (use_interesting_mode)
        {
            loc_copy(&p->tt_grid, &targets->pts[p->target_index].grid);

            /* Adjust panel if needed */
            if (adjust_panel_help(p, p->tt_grid.y, p->tt_grid.x)) handle_stuff(p);
        }

        /* Update help */
        square_actor(c, &p->tt_grid, who);
        has_target = target_able(p, who);
        has_object = !(mode & TARGET_KILL) && pile_is_tracked(p, c, &p->tt_grid);
        target_display_help(help, sizeof(help), has_target, has_object, use_free_mode);

        /* Find the path. */
        p->path_n = project_path(p, c, p->path_g, z_info->max_range, &p->grid, &p->tt_grid,
            PROJECT_THRU | PROJECT_INFO);

        /* Draw the path in "target" mode. If there is one */
        if (mode & TARGET_KILL)
            p->path_drawn = draw_path(p, p->path_n, p->path_g, &p->grid);

        /* Describe and Prompt */
        if (target_set_interactive_aux(p, &p->tt_grid, mode | (use_free_mode? TARGET_LOOK: 0),
            help, press))
        {
            point_set_dispose(targets);
            return false;
        }

        /* Remove the path */
        if (p->path_drawn) load_path(p, p->path_n, p->path_g);

        /* Handle an input event */
        if (event_is_key(press, ESCAPE) || event_is_key(press, 'q') || event_is_key(press, 'r'))
        {
            /* Cancel */
            done = true;
        }
        else if (event_is_key(press, ' ') || event_is_key(press, '(') || event_is_key(press, '*') ||
            event_is_key(press, '+'))
        {
            /* Cycle interesting target forward */
            if (use_interesting_mode)
            {
                set_target_index(p, p->target_index + 1);
                if (p->target_index == point_set_size(targets)) set_target_index(p, 0);
            }

            /* Hack -- acknowledge */
            press = '\0';
        }
        else if (event_is_key(press, '-'))
        {
            /* Cycle interesting target backwards */
            if (use_interesting_mode)
            {
                set_target_index(p, p->target_index - 1);
                if (p->target_index == -1) set_target_index(p, point_set_size(targets) - 1);
            }

            /* Hack -- acknowledge */
            press = '\0';
        }
        else if (event_is_key(press, 'p'))
        {
            /* Focus the player and switch to free mode */
            loc_copy(&p->tt_grid, &p->grid);
            p->show_interesting = false;

            /* Recenter around player */
            verify_panel(p);
            handle_stuff(p);

            /* Hack -- acknowledge */
            press = '\0';
        }
        else if (event_is_key(press, 'o'))
        {
            /* Switch to free mode */
            p->show_interesting = false;

            /* Hack -- acknowledge */
            press = '\0';
        }
        else if (event_is_key(press, 'm'))
        {
            /* Switch to interesting mode */
            if (use_free_mode && point_set_size(targets) > 0)
            {
                int min_dist = 999, i;

                p->show_interesting = true;
                set_target_index(p, 0);

                /* Pick a nearby monster */
                for (i = 0; i < point_set_size(targets); i++)
                {
                    int dist = distance(&p->tt_grid, &targets->pts[i].grid);

                    /* Pick closest */
                    if (dist < min_dist)
                    {
                        set_target_index(p, i);
                        min_dist = dist;
                    }
                }

                /* Nothing interesting */
                if (min_dist == 999) p->show_interesting = false;
            }

            /* Hack -- acknowledge */
            press = '\0';
        }
        else if (event_is_key(press, 't') || event_is_key(press, '5') || event_is_key(press, '0') ||
            event_is_key(press, '.'))
        {
            /* Set a target and done */
            if (use_interesting_mode)
            {
                square_actor(c, &p->tt_grid, who);

                if (target_able(p, who))
                {
                    health_track(p->upkeep, who);
                    target_set_monster(p, who);
                }
            }
            else
                target_set_location(p, &p->tt_grid);

            done = true;
        }
        else if (event_is_key(press, 'i'))
        {
            /* Ignore the tracked object, set by target_set_interactive_aux() */
            if (has_object)
            {
                struct object *base_obj;

                /* PWMAngband: get this item's base object (because we track the known copy of it) */
                for (base_obj = square_object(c, &p->tt_grid); base_obj; base_obj = base_obj->next)
                {
                    if (base_obj->oidx == p->upkeep->object->oidx)
                    {
                        /*textui_cmd_ignore_menu(p->upkeep->object);*/

                        /* PWMAngband: just toggle ignore for now */
                        do_cmd_destroy_aux(p, base_obj, false);
                        square_know_pile(p, c, &p->tt_grid);
                        p->upkeep->update |= (PU_UPDATE_VIEW);
                        p->upkeep->redraw |= (PR_MAP | PR_OBJECT | PR_ITEMLIST);

                        /* Recalculate interesting grids */
                        point_set_dispose(targets);
                        targets = target_get_monsters(p, mode, true);

                        break;
                    }
                }
            }

            /* Hack -- acknowledge */
            press = '\0';
        }
        else
        {
            /* Try to extract a direction from the key press */
            int dir = target_dir(press);

            if (!dir)
            {
                /* Hack -- acknowledge */
                if (press != KC_ENTER) press = '\0';
            }
            else if (use_interesting_mode)
            {
                /* Interesting mode direction: Pick new interesting grid */
                int old_y = targets->pts[p->target_index].grid.y;
                int old_x = targets->pts[p->target_index].grid.x;
                int new_index;

                /* Look for a new interesting grid */
                new_index = target_pick(old_y, old_x, ddy[dir], ddx[dir], targets);

                /* If none found, try in the next panel */
                if (new_index < 0)
                {
                    struct loc offset_grid;

                    loc_copy(&offset_grid, &p->offset_grid);
                    if (change_panel(p, dir))
                    {
                        /* Recalculate interesting grids */
                        point_set_dispose(targets);
                        targets = target_get_monsters(p, mode, true);

                        /* Look for a new interesting grid again */
                        new_index = target_pick(old_y, old_x, ddy[dir], ddx[dir], targets);

                        /* If none found again, reset the panel and do nothing */
                        if ((new_index < 0) && modify_panel(p, &offset_grid))
                        {
                            /* Recalculate interesting grids */
                            point_set_dispose(targets);
                            targets = target_get_monsters(p, mode, true);
                        }

                        handle_stuff(p);
                    }
                }

                /* Use interesting grid if found */
                if (new_index >= 0) set_target_index(p, new_index);

                /* Hack -- acknowledge */
                press = '\0';
            }
            else
            {
                /* Free mode direction: Move cursor */
                p->tt_grid.x += ddx[dir];
                p->tt_grid.y += ddy[dir];

                /* Keep 1 away from the edge */
                p->tt_grid.x = MAX(1, MIN(p->tt_grid.x, c->width - 2));
                p->tt_grid.y = MAX(1, MIN(p->tt_grid.y, c->height - 2));

                /* Adjust panel if needed */
                if (adjust_panel_help(p, p->tt_grid.y, p->tt_grid.x))
                {
                    handle_stuff(p);

                    /* Recalculate interesting grids */
                    point_set_dispose(targets);
                    targets = target_get_monsters(p, mode, true);
                }

                /* Hack -- acknowledge */
                press = '\0';
            }
        }
    }

    /* Paranoia */
    if (!tries) plog_fmt("Infinite loop in target_set_interactive: %lu", press);

    /* Forget */
    point_set_dispose(targets);

    /* Recenter around player */
    verify_panel(p);
    handle_stuff(p);

    return p->target.target_set;
}