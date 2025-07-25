/*
 * File: knowledge-ui.c
 * Purpose: Various kinds of browsing functions
 *
 * Copyright (c) 1997-2007 Robert A. Koeneke, James E. Wilson, Ben Harrison,
 * Eytan Zweig, Andrew Doull, Pete Mack.
 * Copyright (c) 2004 DarkGod (HTML dump code)
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
 * The first part of this file contains the knowledge menus. Generic display
 * routines are followed by sections which implement "subclasses" of the
 * abstract classes represented by member_funcs and group_funcs.
 *
 * After the knowledge menus are various knowledge functions - message review;
 * inventory, equipment, monster and object lists; symbol lookup; and the
 * "locate" command which scrolls the screen around the current dungeon level.
 */


/*
 * Helper class for generating joins
 */
typedef struct join
{
    int oid;
    int gid;
} join_t;


/*
 * A default group-by
 */
static join_t *default_join;


/*
 * Knowledge menu utilities
 */


/*
 * Return a specific ordering for the features
 */
static int feat_order(int feat)
{
    int special = feat_order_special(feat);

    if (special != -1) return special;

    if (tf_has(f_info[feat].flags, TF_SHOP)) return 6;
    if (tf_has(f_info[feat].flags, TF_STAIR)) return 2;
    if (tf_has(f_info[feat].flags, TF_DOOR_ANY)) return 1;

    /* These also have WALL set so check them first before checking WALL. */
    if (tf_has(f_info[feat].flags, TF_MAGMA) || tf_has(f_info[feat].flags, TF_QUARTZ)) return 4;

    /* These also have ROCK set so check them first before checking ROCK. */
    if (tf_has(f_info[feat].flags, TF_WALL)) return 3;
    if (tf_has(f_info[feat].flags, TF_ROCK)) return 5;

    /* Many above have PASSABLE so do this last. */
    if (tf_has(f_info[feat].flags, TF_PASSABLE)) return 0;
    return 7;
}


/*
 * Return a specific ordering for the traps
 */
static int trap_order(int trap)
{
    const struct trap_kind *t = &trap_info[trap];

    if (trf_has(t->flags, TRF_GLYPH)) return 0;
    if (trf_has(t->flags, TRF_LOCK)) return 1;
    if (trf_has(t->flags, TRF_TRAP)) return 2;
    return 3;
}


/*
 * MONSTERS
 */


struct ui_monster_category
{
    struct ui_monster_category *next;
    const char *name;
    const struct monster_base **inc_bases;
    bitflag inc_flags[RF_SIZE];
    int n_inc_bases, max_inc_bases;
};


struct ui_knowledge_parse_state
{
    struct ui_monster_category *categories;
};


/*
 * Is a flat array describing each monster group. Configured by
 * ui_knowledge.txt. The last element receives special treatment and is
 * used to catch any type of monster not caught by the other categories.
 * That's intended as a debugging tool while modding the game.
 */
static struct ui_monster_category *monster_group = NULL;


/*
 * Is the number of entries, including the last one receiving special
 * treatment, in monster_group.
 */
static int n_monster_group = 0;


static int m_cmp_race(const void *a, const void *b)
{
    const struct monster_race *r_a = &r_info[default_join[*(const int *)a].oid];
    const struct monster_race *r_b = &r_info[default_join[*(const int *)b].oid];
    int gid = default_join[*(const int *)a].gid;

    /* Group by */
    int c = gid - default_join[*(const int *)b].gid;

    if (c) return c;

    /*
     * If the group specifies monster bases, order those that are included
     * by the base by those bases. Those that aren't in any of the bases
     * appear last.
     */
    my_assert(gid >= 0 && gid < n_monster_group);
    if (monster_group[gid].n_inc_bases)
    {
        int base_a = monster_group[gid].n_inc_bases;
        int base_b = monster_group[gid].n_inc_bases;
        int i;

        for (i = 0; i < monster_group[gid].n_inc_bases; ++i)
        {
            if (r_a->base == monster_group[gid].inc_bases[i]) base_a = i;
            if (r_b->base == monster_group[gid].inc_bases[i]) base_b = i;
        }
        c = base_a - base_b;
        if (c) return c;
    }

    /*
     * Within the same base or outside of a specified base, order by level
     * and then by name.
     */
    c = r_a->level - r_b->level;
    if (c) return c;

    return strcmp(r_a->name, r_b->name);
}


static int count_known_monsters(struct player *p)
{
    int m_count = 0, i;

    for (i = 1; i < z_info->r_max; ++i)
    {
        struct monster_race *race = &r_info[i];
        struct monster_lore *lore = get_lore(p, race);
        bool classified = false;
        int j;

        if (!lore->pseen) continue;
        if (!race->name) continue;

        for (j = 0; j < n_monster_group - 1; ++j)
        {
            bool has_base = false;

            if (monster_group[j].n_inc_bases)
            {
                int k;

                for (k = 0; k < monster_group[j].n_inc_bases; ++k)
                {
                    if (race->base == monster_group[j].inc_bases[k])
                    {
                        ++m_count;
                        has_base = true;
                        classified = true;
                        break;
                    }
                }
            }
            if (!has_base && rf_is_inter(race->flags, monster_group[j].inc_flags))
            {
                ++m_count;
                classified = true;
            }
        }

        if (!classified) ++m_count;
    }

    return m_count;
}


/*
 * Display known monsters
 */
static void do_cmd_knowledge_monsters(struct player *p, int line)
{
    int *monsters;
    int m_count = count_known_monsters(p), i, ind;
    char file_name[MSG_LEN];
    ang_file *fff;
    int m_group = -1;

    /* Temporary file */
    fff = file_temp(file_name, sizeof(file_name));
    if (!fff) return;

    default_join = mem_zalloc(m_count * sizeof(join_t));
    monsters = mem_zalloc(m_count * sizeof(int));

    /* Collect matching monsters */
    ind = 0;
    for (i = 1; i < z_info->r_max; ++i)
    {
        struct monster_race *race = &r_info[i];
        struct monster_lore *lore = get_lore(p, race);
        bool classified = false;
        int j;

        if (!lore->pseen) continue;
        if (!race->name) continue;

        for (j = 0; j < n_monster_group - 1; ++j)
        {
            bool has_base = false;

            if (monster_group[j].n_inc_bases)
            {
                int k;

                for (k = 0; k < monster_group[j].n_inc_bases; ++k)
                {
                    if (race->base == monster_group[j].inc_bases[k])
                    {
                        my_assert(ind < m_count);
                        monsters[ind] = ind;
                        default_join[ind].oid = i;
                        default_join[ind].gid = j;
                        ++ind;
                        has_base = true;
                        classified = true;
                        break;
                    }
                }
            }
            if (!has_base && rf_is_inter(race->flags, monster_group[j].inc_flags))
            {
                my_assert(ind < m_count);
                monsters[ind] = ind;
                default_join[ind].oid = i;
                default_join[ind].gid = j;
                ++ind;
                classified = true;
            }
        }

        if (!classified)
        {
            my_assert(ind < m_count);
            monsters[ind] = ind;
            default_join[ind].oid = i;
            default_join[ind].gid = n_monster_group - 1;
            ++ind;
        }
    }

    /* Sort by kills (and level) */
    sort(monsters, m_count, sizeof(*monsters), m_cmp_race);

    /* Print the monsters */
    for (i = 0; i < m_count; i++)
    {
        int oid = default_join[monsters[i]].oid;
        int gid = default_join[monsters[i]].gid;
        char kills[6];

        /* Access the race */
        struct monster_race *race = &r_info[oid];
        struct monster_lore *lore = get_lore(p, race);

        /* Find graphics bits */
        uint8_t a = p->r_attr[oid];
        char c = p->r_char[oid];

        /* Paranoia */
        if (gid == -1) continue;

        /* Print group */
        if (gid != m_group)
        {
            m_group = gid;
            file_putf(fff, "w%s\n", monster_group[m_group].name);
        }

        /* Use ASCII symbol for distorted tiles */
        if (p->tile_distorted)
        {
            a = race->d_attr;
            c = race->d_char;
        }

        /* If uniques are purple, make it so */
        if (OPT(p, purple_uniques) && race_is_unique(race) && !(a & 0x80))
            a = COLOUR_VIOLET;

        /* Display kills */
        if (!race->rarity)
            my_strcpy(kills, "shape", sizeof(kills));
        else if (race_is_unique(race))
            my_strcpy(kills, (lore->pkills? " dead": "alive"), sizeof(kills));
        else
            strnfmt(kills, sizeof(kills), "%5d", lore->pkills);

        /* Print a message */
        file_putf(fff, "d%c%cw%-40s  %s\n", ((a & 0x80)? a: color_attr_to_char(a)), c, race->name,
            kills);
    }

    mem_free(default_join);
    default_join = NULL;
    mem_free(monsters);

    /* Close the file */
    file_close(fff);

    /* Display the file contents */
    show_file(p, file_name, "Monsters", line, 1);

    /* Remove the file */
    file_delete(file_name);
}


/*
 * ARTIFACTS
 */


/*
 * These are used for all the object sections
 */
static const grouper object_text_order[] =
{
    {TV_RING, "Ring"},
    {TV_AMULET, "Amulet"},
    {TV_POTION, "Potion"},
    {TV_SCROLL, "Scroll"},
    {TV_WAND, "Wand"},
    {TV_STAFF, "Staff"},
    {TV_ROD, "Rod"},
    {TV_FOOD, "Food"},
    {TV_MUSHROOM, "Mushroom"},
    {TV_CROP, "Crop"},
    {TV_COOKIE, "Fortune Cookies"},
    {TV_MAGIC_BOOK, "Magic Book"},
    {TV_PRAYER_BOOK, "Prayer Book"},
    {TV_NATURE_BOOK, "Nature Book"},
    {TV_SHADOW_BOOK, "Shadow Book"},
    {TV_PSI_BOOK, "Psi Book"},
    {TV_ELEM_BOOK, "Elemental Book"},
    {TV_TRAVEL_BOOK, "Travel Guide"},
    {TV_SKILLBOOK, "Skillbook"},
    {TV_LIGHT, "Light"},
    {TV_SWORD, "Sword"},
    {TV_POLEARM, "Polearm"},
    {TV_HAFTED, "Hafted"},
    {TV_MSTAFF, "Mage Staff"},
    {TV_BOW, "Shooter"},
    {TV_ARROW, "Ammunition (firing)"},
    {TV_BOLT, NULL},
    {TV_SHOT, NULL},
    {TV_ROCK, "Ammunition (throwing)"},
    {TV_SHIELD, "Shield"},
    {TV_CROWN, "Crown"},
    {TV_HELM, "Helm"},
    {TV_GLOVES, "Gloves"},
    {TV_BOOTS, "Boots"},
    {TV_CLOAK, "Cloak"},
    {TV_DRAG_ARMOR, "Dragon Armour"},
    {TV_HARD_ARMOR, "Hard Armour"},
    {TV_SOFT_ARMOR, "Soft Armour"},
    {TV_HORN, "Horn"},
    {TV_DIGGING, "Digger"},
    {TV_BELT, "Belt"},
    {TV_CHEST, "Chest"},
    {TV_SKELETON, NULL},
    {TV_BOTTLE, NULL},
    {TV_STONE, NULL},
    {TV_CORPSE, NULL},
    {TV_DEED, NULL},
    {TV_FLASK, NULL},
    {TV_COBBLE, NULL},
    {TV_JUNK, NULL},
    {TV_REAGENT, NULL},
    {0, NULL}
};


static int *obj_group_order = NULL;


struct cmp_art
{
    const struct artifact *artifact;
    char highlight;
    char owner[NORMAL_WID];
};


static int a_cmp_tval(const void *a, const void *b)
{
    const struct cmp_art *pa = a;
    const struct cmp_art *pb = b;
    const struct artifact *a_a = pa->artifact;
    const struct artifact *a_b = pb->artifact;

    /* Group by */
    int ta = obj_group_order[a_a->tval];
    int tb = obj_group_order[a_b->tval];
    int c = ta - tb;

    if (c) return c;

    /* Order by */
    c = a_a->sval - a_b->sval;
    if (c) return c;
    return strcmp(a_a->name, a_b->name);
}


static char highlight_unknown(struct player *p, int idx)
{
    /* Artifact is unknown */
    if (p->art_info[idx] < ARTS_FOUND) return 'd';

    /* Artifact is discarded and unfindable again */
    if (p->art_info[idx] > cfg_preserve_artifacts) return 'r';

    /* Artifact is discarded and findable again */
    return 'g';
}


/*
 * Collects the list of known artifacts
 *
 * Highlighting:
 *    'w': artifact is owned by the player
 *    'r': artifact is discarded and unfindable again
 *    'g': artifact is discarded and findable again
 *    'D': artifact is not owned or owned by someone else
 */
static int collect_known_artifacts(struct player *p, struct cmp_art *artifacts)
{
    int a_count = 0;
    int i;
    struct loc grid;
    struct object *obj;
    char *highlights = mem_zalloc(z_info->a_max * sizeof(char));
    char *owners = mem_zalloc(z_info->a_max * NORMAL_WID * sizeof(char));

    /* Check inventory and equipment of players */
    for (i = 1; i <= NumPlayers; i++)
    {
        struct player *player = player_get(i);

        /* Check this guy */
        for (obj = player->gear; obj; obj = obj->next)
        {
            /* Ignore non-artifacts */
            if (!true_artifact_p(obj)) continue;

            /* Note owner */
            strnfmt(&owners[obj->artifact->aidx * NORMAL_WID], NORMAL_WID, " (%s)", player->name);

            /* Artifact is owned by the player */
            if (player == p) highlights[obj->artifact->aidx] = 'w';

            /* Artifact is owned by someone else */
            else highlights[obj->artifact->aidx] = 'D';
        }
    }

    /* Check the world */
    for (grid.y = radius_wild; grid.y >= 0 - radius_wild; grid.y--)
    {
        for (grid.x = 0 - radius_wild; grid.x <= radius_wild; grid.x++)
        {
            struct wild_type *w_ptr = get_wt_info_at(&grid);

            for (i = 0; i <= w_ptr->max_depth - w_ptr->min_depth; i++)
            {
                struct chunk *c = w_ptr->chunk_list[i];
                struct loc begin, end;
                struct loc_iterator iter;

                if (!c) continue;

                loc_init(&begin, 0, 0);
                loc_init(&end, c->width, c->height);
                loc_iterator_first(&iter, &begin, &end);

                do
                {
                    for (obj = square_object(c, &iter.cur); obj; obj = obj->next)
                    {
                        /* Ignore non-artifacts */
                        if (!true_artifact_p(obj)) continue;

                        /* Note location */
                        strnfmt(&owners[obj->artifact->aidx * NORMAL_WID], NORMAL_WID,
                            " (%d ft)", c->wpos.depth * 50);

                        /* Artifact is owned by the player */
                        if (obj->owner == p->id)
                            highlights[obj->artifact->aidx] = 'w';

                        /* Artifact is not owned or owned by someone else */
                        else if (object_is_known(p, obj))
                            highlights[obj->artifact->aidx] = 'D';

                        /* Artifact is unknown */
                        else
                        {
                            highlights[obj->artifact->aidx] = highlight_unknown(p,
                                obj->artifact->aidx);
                        }
                    }
                }
                while (loc_iterator_next_strict(&iter));
            }
        }
    }

    /* Collect matching artifacts */
    for (i = 0; i < z_info->a_max; i++)
    {
        const struct artifact *art = &a_info[i];
        char h = highlights[i];

        /* Skip "empty" artifacts */
        if (!art->name) continue;

        /* Skip "uncreated + unfound" artifacts */
        if ((p->art_info[i] < ARTS_FOUND) && !is_artifact_created(art)) continue;

        /* Artifact is "uncreated" */
        if (!is_artifact_created(art)) h = highlight_unknown(p, i);

        /* Special case: artifact is owned by a non-connected player */
        if (!h)
        {
            int32_t owner = get_artifact_owner(art);

            /* Artifact is owned */
            if (owner)
            {
                h = 'D';

                /* Dungeon Masters see extra info */
                if (is_dm_p(p))
                    strnfmt(&owners[i * NORMAL_WID], NORMAL_WID, " (%s)", lookup_player_name(owner));
            }

            /* Artifact is unknown */
            else h = highlight_unknown(p, i);
        }

        /* Artifact is known */
        if (h != 'd')
        {
            int c = obj_group_order[art->tval];

            if (c >= 0)
            {
                artifacts[a_count].artifact = art;
                artifacts[a_count].highlight = h;
                my_strcpy(artifacts[a_count].owner, &owners[i * NORMAL_WID], NORMAL_WID);
                a_count++;
            }
        }
    }

    /* Free memory */
    mem_free(highlights);
    mem_free(owners);

    return a_count;
}


/*
 * Display known artifacts
 */
static void do_cmd_knowledge_artifacts(struct player *p, int line)
{
    struct cmp_art *artifacts;
    int a_count = 0;
    int i;
    char file_name[MSG_LEN];
    ang_file *fff;
    int a_group = -1;

    /* Temporary file */
    fff = file_temp(file_name, sizeof(file_name));
    if (!fff) return;

    artifacts = mem_zalloc(z_info->a_max * sizeof(struct cmp_art));

    /* Initialize other static variables */
    if (!obj_group_order)
    {
        int gid = -1;

        obj_group_order = mem_zalloc(TV_MAX * sizeof(int));

        /* Allow for missing values */
        for (i = 0; i < TV_MAX; i++) obj_group_order[i] = -1;

        for (i = 0; 0 != object_text_order[i].tval; i++)
        {
            if (object_text_order[i].name) gid = i;
            obj_group_order[object_text_order[i].tval] = gid;
        }
    }

    /* Collect valid artifacts */
    a_count = collect_known_artifacts(p, artifacts);

    /* Sort */
    sort(artifacts, a_count, sizeof(*artifacts), a_cmp_tval);

    /* Print the artifacts */
    for (i = 0; i < a_count; i++)
    {
        const struct artifact *art = artifacts[i].artifact;
        char o_name[NORMAL_WID];
        int gid = obj_group_order[art->tval];
        struct object *fake;

        /* Paranoia */
        if (gid == -1) continue;
        if (!make_fake_artifact(&fake, art)) continue;

        /* Print group */
        if (gid != a_group)
        {
            a_group = gid;
            file_putf(fff, "w%s\n", object_text_order[a_group].name);
        }

        /* Describe the artifact */
        object_desc(p, o_name, sizeof(o_name), fake, ODESC_BASE | ODESC_ARTIFACT);

        /* Print a message */
        /* Dungeon Masters see extra info */
        if (is_dm_p(p))
            file_putf(fff, "%c     The %s%s\n", artifacts[i].highlight, o_name, artifacts[i].owner);
        else
            file_putf(fff, "%c     The %s\n", artifacts[i].highlight, o_name);

        object_delete(&fake);
    }

    mem_free(artifacts);
    mem_free(obj_group_order);
    obj_group_order = NULL;

    /* Close the file */
    file_close(fff);

    /* Display the file contents */
    show_file(p, file_name, "Artifacts", line, 1);

    /* Remove the file */
    file_delete(file_name);
}


/*
 * EGO ITEMS
 */


static int e_cmp_tval(const void *a, const void *b)
{
    const struct ego_item *ea = &e_info[default_join[*(const int *)a].oid];
    const struct ego_item *eb = &e_info[default_join[*(const int *)b].oid];

    /* Group by */
    int c = default_join[*(const int *)a].gid - default_join[*(const int *)b].gid;

    if (c) return c;

    /* Order by */
    return strcmp(ea->name, eb->name);
}


/*
 * Display known ego items
 */
static void do_cmd_knowledge_ego_items(struct player *p, int line)
{
    int *egoitems;
    int e_count = 0;
    int i;
    int max_pairs = z_info->e_max * N_ELEMENTS(object_text_order);
    char file_name[MSG_LEN];
    ang_file *fff;
    int e_group = -1;

    /* Temporary file */
    fff = file_temp(file_name, sizeof(file_name));
    if (!fff) return;

    egoitems = mem_zalloc(max_pairs * sizeof(int));
    default_join = mem_zalloc(max_pairs * sizeof(join_t));

    /* Initialize other static variables */
    if (!obj_group_order)
    {
        int gid = -1;

        obj_group_order = mem_zalloc(TV_MAX * sizeof(int));

        /* Allow for missing values */
        for (i = 0; i < TV_MAX; i++) obj_group_order[i] = -1;

        for (i = 0; 0 != object_text_order[i].tval; i++)
        {
            if (object_text_order[i].name) gid = i;
            obj_group_order[object_text_order[i].tval] = gid;
        }
    }

    /* Look at all the ego items */
    for (i = 0; i < z_info->e_max; i++)
    {
        struct ego_item *ego = &e_info[i];
        size_t j;
        int *tval;
        struct poss_item *poss;

        /* Ignore non-egos && unknown egos */
        if (!(ego->name && p->ego_everseen[i])) continue;

        tval = mem_zalloc(N_ELEMENTS(object_text_order) * sizeof(int));

        /* Note the tvals which are possible for this ego */
        for (poss = ego->poss_items; poss; poss = poss->next)
        {
            struct object_kind *kind = &k_info[poss->kidx];

            my_assert(obj_group_order[kind->tval] >= 0);
            tval[obj_group_order[kind->tval]]++;
        }

        /* Count and put into the list */
        for (j = 0; j < N_ELEMENTS(object_text_order); j++)
        {
            int gid = obj_group_order[j];

            /* Skip if nothing in this group */
            if (gid < 0) continue;

            /* Ignore duplicates */
            if ((e_count > 0) && (gid == default_join[e_count - 1].gid) &&
                (i == default_join[e_count - 1].oid))
            {
                continue;
            }

            if (tval[gid])
            {
                egoitems[e_count] = e_count;
                default_join[e_count].oid = i;
                default_join[e_count++].gid = gid;
            }
        }

        mem_free(tval);
    }

    /* Sort */
    sort(egoitems, e_count, sizeof(*egoitems), e_cmp_tval);

    /* Print the ego items */
    for (i = 0; i < e_count; i++)
    {
        struct ego_item *ego = &e_info[default_join[egoitems[i]].oid];
        int gid = default_join[egoitems[i]].gid;

        /* Paranoia */
        if (gid == -1) continue;

        /* Print group */
        if (gid != e_group)
        {
            e_group = gid;
            file_putf(fff, "%s\n", object_text_order[e_group].name);
        }

        /* Print a message */
        file_putf(fff, "     %s\n", ego->name);
    }

    mem_free(default_join);
    default_join = NULL;
    mem_free(egoitems);
    mem_free(obj_group_order);
    obj_group_order = NULL;

    /* Close the file */
    file_close(fff);

    /* Display the file contents */
    show_file(p, file_name, "Ego Items", line, 0);

    /* Remove the file */
    file_delete(file_name);
}


/*
 * ORDINARY OBJECTS
 */


struct cmp_obj
{
    struct player *player;
    const struct object_kind *kind;
};


static int o_cmp_tval(const void *a, const void *b)
{
    const struct cmp_obj *pa = a;
    const struct cmp_obj *pb = b;
    const struct object_kind *k_a = pa->kind;
    const struct object_kind *k_b = pb->kind;

    /* Group by */
    int ta = obj_group_order[k_a->tval];
    int tb = obj_group_order[k_b->tval];
    int c = ta - tb;

    if (c) return c;

    /* Order by */
    c = pa->player->kind_aware[k_a->kidx] - pb->player->kind_aware[k_b->kidx];
    if (c) return -c;

    switch (k_a->tval)
    {
        case TV_LIGHT:
        case TV_MAGIC_BOOK:
        case TV_PRAYER_BOOK:
        case TV_NATURE_BOOK:
        case TV_SHADOW_BOOK:
        case TV_PSI_BOOK:
        case TV_ELEM_BOOK:
        case TV_TRAVEL_BOOK:
        case TV_SKILLBOOK:
        {
            /* Leave sorted by sval */
            break;
        }

        default:
        {
            if (pa->player->kind_aware[k_a->kidx]) return strcmp(k_a->name, k_b->name);

            /* Then in tried order */
            c = pa->player->kind_tried[k_a->kidx] - pb->player->kind_tried[k_b->kidx];
            if (c) return -c;

            return strcmp(k_a->flavor->text, k_b->flavor->text);
        }
    }

    return k_a->sval - k_b->sval;
}


/*
 * Display known objects
 */
static void do_cmd_knowledge_objects(struct player *p, int line)
{
    struct cmp_obj *objects;
    int o_count = 0;
    int i;
    char file_name[MSG_LEN];
    ang_file *fff;
    int o_group = -1;

    /* Temporary file */
    fff = file_temp(file_name, sizeof(file_name));
    if (!fff) return;

    objects = mem_zalloc(z_info->k_max * sizeof(struct cmp_obj));

    /* Initialize other static variables */
    if (!obj_group_order)
    {
        int gid = -1;

        obj_group_order = mem_zalloc(TV_MAX * sizeof(int));

        /* Allow for missing values */
        for (i = 0; i < TV_MAX; i++) obj_group_order[i] = -1;

        for (i = 0; 0 != object_text_order[i].tval; i++)
        {
            if (object_text_order[i].name) gid = i;
            obj_group_order[object_text_order[i].tval] = gid;
        }
    }

    /* Collect matching objects */
    for (i = 0; i < z_info->k_max; i++)
    {
        struct object_kind *kind = &k_info[i];
        bool aware_art = (kf_has(kind->kind_flags, KF_INSTA_ART) && p->kind_aware[i]);

        /*
         * It's in the list if we've ever seen it, or it has a flavour,
         * and either it's not one of the special artifacts, or if it is,
         * we're not aware of it yet. This way the flavour appears in the list
         * until it is found.
         */
        if ((p->kind_everseen[i] || kind->flavor) && !aware_art)
        {
            int c = obj_group_order[kind->tval];

            if (c >= 0)
            {
                objects[o_count].player = p;
                objects[o_count++].kind = kind;
            }
        }
    }

    /* Sort */
    sort(objects, o_count, sizeof(*objects), o_cmp_tval);

    /* Print the objects */
    for (i = 0; i < o_count; i++)
    {
        const struct object_kind *kind = objects[i].kind;
        char o_name[NORMAL_WID];
        int gid = obj_group_order[kind->tval];

        /* Choose a color */
        bool aware = p->kind_aware[kind->kidx];
        uint8_t attr = (aware? COLOUR_WHITE: COLOUR_SLATE);

        /* Find graphics bits -- versions of the object_char and object_attr defines */
        uint8_t a = object_kind_attr(p, kind);
        char c = object_kind_char(p, kind);

        /* Paranoia */
        if (gid == -1) continue;

        /* Print group */
        if (gid != o_group)
        {
            o_group = gid;
            file_putf(fff, "w%s\n", object_text_order[o_group].name);
        }

        /* Use ASCII symbol for distorted tiles */
        if (p->tile_distorted)
        {
            a = kind->d_attr;
            c = kind->d_char;

            if (kind->flavor && !(aware && a && c))
            {
                a = kind->flavor->d_attr;
                c = kind->flavor->d_char;
            }
        }

        object_kind_name(o_name, sizeof(o_name), kind, aware);

        /* If the type is "tried", display that */
        if (p->kind_tried[kind->kidx] && !aware) my_strcat(o_name, " {tried}", sizeof(o_name));

        /* Append flavour if desired */
        else if (OPT(p, show_flavors) && kind->flavor && aware)
        {
            my_strcat(o_name, " (", sizeof(o_name));
            my_strcat(o_name, kind->flavor->text, sizeof(o_name));
            my_strcat(o_name, ")", sizeof(o_name));
        }

        /* Print a message */
        file_putf(fff, "d%c%c%c%s\n", ((a & 0x80)? a: color_attr_to_char(a)), c,
            color_attr_to_char(attr), o_name);
    }

    mem_free(objects);
    mem_free(obj_group_order);
    obj_group_order = NULL;

    /* Close the file */
    file_close(fff);

    /* Display the file contents */
    show_file(p, file_name, "Known Objects", line, 1);

    /* Remove the file */
    file_delete(file_name);
}


/*
 * OBJECT RUNES
 */


/*
 * Description of each rune group
 */
static const char *rune_group_text[] =
{
    "Combat",
    "Modifiers",
    "Resists",
    "Brands",
    "Slays",
    "Curses",
    "Other",
    NULL
};


/*
 * Display known runes
 */
static void do_cmd_knowledge_runes(struct player *p, int line)
{
    int *runes;
    int rune_max = max_runes();
    int count = 0;
    int i;
    char file_name[MSG_LEN];
    ang_file *fff;
    int r_group = -1;

    /* Temporary file */
    fff = file_temp(file_name, sizeof(file_name));
    if (!fff) return;

    runes = mem_zalloc(rune_max * sizeof(int));

    /* Collect runes */
    for (i = 0; i < rune_max; i++)
    {
        /* Ignore unknown runes */
        if (!player_knows_rune(p, i)) continue;

        runes[count++] = i;
    }

    /* Print the runes */
    for (i = 0; i < count; i++)
    {
        int gid = rune_variety(runes[i]);

        /* Print group */
        if (gid != r_group)
        {
            r_group = gid;
            file_putf(fff, "w%s\n", rune_group_text[r_group]);
        }

        /* Print a message */
        file_putf(fff, "B     %s:\n", rune_name(runes[i]));
        file_putf(fff, "w          %s\n", rune_desc(runes[i]));
    }

    file_putf(fff, "w\n");
    file_putf(fff, "w%d unknown", rune_max - count);

    mem_free(runes);

    /* Close the file */
    file_close(fff);

    /* Display the file contents */
    show_file(p, file_name, "Object Runes", line, 1);

    /* Remove the file */
    file_delete(file_name);
}


/*
 * TERRAIN FEATURES
 */


/*
 * Description of each feature group
 */
static const char *feature_group_text[] =
{
    "Floors",
    "Doors",
    "Stairs",
    "Walls",
    "Streamers",
    "Obstructions",
    "Stores",
    "Other",
    NULL
};


static int f_cmp_fkind(const void *a, const void *b)
{
    const struct feature *fa = &f_info[*(const int *)a];
    const struct feature *fb = &f_info[*(const int *)b];

    /* Group by */
    int c = feat_order(*(const int *)a) - feat_order(*(const int *)b);

    if (c) return c;

    /* Order by feature name */
    return strcmp(fa->name, fb->name);
}


/*
 * Interact with feature visuals
 */
static void do_cmd_knowledge_features(struct player *p, int line)
{
    int *features;
    int f_count = 0;
    int i;
    char file_name[MSG_LEN];
    ang_file *fff;
    int f_group = -1;

    /* Temporary file */
    fff = file_temp(file_name, sizeof(file_name));
    if (!fff) return;

    features = mem_zalloc(FEAT_MAX * sizeof(int));

    /* Collect features */
    for (i = 0; i < FEAT_MAX; i++)
    {
        /* Ignore non-features and mimics */
        if (!f_info[i].name || f_info[i].mimic) continue;

        features[f_count++] = i;
    }

    /* Sort */
    sort(features, f_count, sizeof(*features), f_cmp_fkind);

    /* Print the features */
    for (i = 0; i < f_count; i++)
    {
        const struct feature *feat = &f_info[features[i]];
        int gid = feat_order(features[i]);

        /* Find graphics bits */
        uint8_t a = p->f_attr[features[i]][LIGHTING_LIT];
        char c = p->f_char[features[i]][LIGHTING_LIT];

        /* Paranoia */
        if (gid == -1) continue;

        /* Print group */
        if (gid != f_group)
        {
            f_group = gid;
            file_putf(fff, "w%s\n", feature_group_text[f_group]);
        }

        /* Use ASCII symbol for distorted tiles */
        if (p->tile_distorted)
        {
            a = feat->d_attr;
            c = feat->d_char;
        }

        /* Print a message */
        file_putf(fff, "d%c%cw%s\n", ((a & 0x80)? a: color_attr_to_char(a)), c, feat->name);
    }

    mem_free(features);

    /* Close the file */
    file_close(fff);

    /* Display the file contents */
    show_file(p, file_name, "Features", line, 1);

    /* Remove the file */
    file_delete(file_name);
}


/*
 * TRAPS
 */


/*
 * Description of each trap group
 */
static const char *trap_group_text[] =
{
    "Runes",
    "Locks",
    "Traps",
    "Other",
    NULL
};


static int t_cmp_tkind(const void *a, const void *b)
{
    const int a_val = *(const int *)a;
    const int b_val = *(const int *)b;
    const struct trap_kind *ta = &trap_info[a_val];
    const struct trap_kind *tb = &trap_info[b_val];

    /* Group by */
    int c = trap_order(a_val) - trap_order(b_val);

    if (c) return c;

    /* Order by name */
    if (ta->name)
    {
        if (tb->name) return strcmp(ta->name, tb->name);
        return 1;
    }
    if (tb->name) return -1;

    return 0;
}


/*
 * Interact with trap visuals
 */
static void do_cmd_knowledge_traps(struct player *p, int line)
{
    int *traps;
    int t_count = 0;
    int i;
    char file_name[MSG_LEN];
    ang_file *fff;
    int t_group = -1;

    /* Temporary file */
    fff = file_temp(file_name, sizeof(file_name));
    if (!fff) return;

    traps = mem_zalloc(z_info->trap_max * sizeof(int));

    /* Collect traps */
    for (i = 0; i < z_info->trap_max; i++)
    {
        /* Ignore non-traps */
        if (!trap_info[i].name) continue;

        traps[t_count++] = i;
    }

    /* Sort */
    sort(traps, t_count, sizeof(*traps), t_cmp_tkind);

    /* Print the traps */
    for (i = 0; i < t_count; i++)
    {
        const struct trap_kind *trap = &trap_info[traps[i]];
        int gid = trap_order(traps[i]);

        /* Find graphics bits */
        uint8_t a = p->t_attr[traps[i]][LIGHTING_LIT];
        char c = p->t_char[traps[i]][LIGHTING_LIT];

        /* Paranoia */
        if (gid == -1) continue;

        /* Print group */
        if (gid != t_group)
        {
            t_group = gid;
            file_putf(fff, "w%s\n", trap_group_text[t_group]);
        }

        /* Use ASCII symbol for distorted tiles */
        if (p->tile_distorted)
        {
            a = trap->d_attr;
            c = trap->d_char;
        }

        /* Print a message */
        file_putf(fff, "d%c%cw%s\n", ((a & 0x80)? a: color_attr_to_char(a)), c, trap->desc);
    }

    mem_free(traps);

    /* Close the file */
    file_close(fff);

    /* Display the file contents */
    show_file(p, file_name, "Traps", line, 1);

    /* Remove the file */
    file_delete(file_name);
}


/*
 * ui_knowledge.txt parsing
 */


static enum parser_error parse_monster_category(struct parser *p)
{
    struct ui_knowledge_parse_state *s = (struct ui_knowledge_parse_state*)parser_priv(p);
    struct ui_monster_category *c;

    my_assert(s);
    c = mem_zalloc(sizeof(*c));
    c->next = s->categories;
    c->name = string_make(parser_getstr(p, "name"));
    s->categories = c;
    return PARSE_ERROR_NONE;
}


static enum parser_error parse_mcat_include_base(struct parser *p)
{
    struct ui_knowledge_parse_state *s = (struct ui_knowledge_parse_state*)parser_priv(p);
    struct monster_base *b;

    my_assert(s);
    if (!s->categories) return PARSE_ERROR_MISSING_RECORD_HEADER;
    b = lookup_monster_base(parser_getstr(p, "name"));
    if (!b) return PARSE_ERROR_INVALID_MONSTER_BASE;
    my_assert(s->categories->n_inc_bases >= 0 &&
        s->categories->n_inc_bases <= s->categories->max_inc_bases);
    if (s->categories->n_inc_bases == s->categories->max_inc_bases)
    {
        if (s->categories->max_inc_bases > INT_MAX / (2 * (int)sizeof(struct monster_base*)))
            return PARSE_ERROR_TOO_MANY_ENTRIES;
        s->categories->max_inc_bases = (s->categories->max_inc_bases)?
            2 * s->categories->max_inc_bases: 2;
        s->categories->inc_bases = mem_realloc(s->categories->inc_bases,
            s->categories->max_inc_bases * sizeof(struct monster_base*));
    }
    s->categories->inc_bases[s->categories->n_inc_bases] = b;
    ++s->categories->n_inc_bases;

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_mcat_include_flag(struct parser *p)
{
    struct ui_knowledge_parse_state *s = (struct ui_knowledge_parse_state*)parser_priv(p);
    char *flags, *next_flag;

    my_assert(s);
    if (!s->categories) return PARSE_ERROR_MISSING_RECORD_HEADER;

    if (!parser_hasval(p, "flags")) return PARSE_ERROR_NONE;
    flags = string_make(parser_getstr(p, "flags"));
    next_flag = strtok(flags, " |");
    while (next_flag)
    {
        if (grab_flag(s->categories->inc_flags, RF_SIZE, r_info_flags, next_flag))
        {
            string_free(flags);
            return PARSE_ERROR_INVALID_FLAG;
        }
        next_flag = strtok(NULL, " |");
    }
    string_free(flags);

    return PARSE_ERROR_NONE;
}


static struct parser *init_ui_knowledge_parser(void)
{
    struct ui_knowledge_parse_state *s = mem_zalloc(sizeof(*s));
    struct parser *p = parser_new();

    parser_setpriv(p, s);
    parser_reg(p, "monster-category str name", parse_monster_category);
    parser_reg(p, "mcat-include-base str name", parse_mcat_include_base);
    parser_reg(p, "mcat-include-flag ?str flags", parse_mcat_include_flag);

    return p;
}


static errr run_ui_knowledge_parser(struct parser *p)
{
    return parse_file_quit_not_found(p, "ui_knowledge");
}


static void cleanup_ui_knowledge_parsed_data(void);


static errr finish_ui_knowledge_parser(struct parser *p)
{
    struct ui_knowledge_parse_state *s = (struct ui_knowledge_parse_state*)parser_priv(p);
    struct ui_monster_category *cursor;
    size_t count;

    my_assert(s);

    /* Count the number of categories and allocate a flat array for them. */
    count = 0;
    for (cursor = s->categories; cursor; cursor = cursor->next) ++count;
    if (count > INT_MAX - 1)
    {
        /*
         * The sorting and display logic for monster groups assumes
         * the number of categories fits in an int.
         */
        cursor = s->categories;
        while (cursor)
        {
            struct ui_monster_category *tgt = cursor;

            cursor = cursor->next;
            string_free((char*)tgt->name);
            mem_free(tgt->inc_bases);
            mem_free(tgt);
        }
        mem_free(s);
        return PARSE_ERROR_TOO_MANY_ENTRIES;
    }
    if (monster_group) cleanup_ui_knowledge_parsed_data();
    monster_group = mem_alloc((count + 1) * sizeof(*monster_group));
    n_monster_group = (int)(count + 1);

    /* Set the element at the end which receives special treatment. */
    monster_group[count].next = NULL;
    monster_group[count].name = string_make("***Unclassified***");
    monster_group[count].inc_bases = NULL;
    rf_wipe(monster_group[count].inc_flags);
    monster_group[count].n_inc_bases = 0;
    monster_group[count].max_inc_bases = 0;

    /*
     * Set the others, restoring the order they had in the data file.
     * Release the memory for the linked list (but not pointed to data
     * as ownership for that is transferred to the flat array).
     */
    cursor = s->categories;
    while (cursor)
    {
        struct ui_monster_category *src = cursor;

        cursor = cursor->next;
        --count;
        monster_group[count].next = monster_group + count + 1;
        monster_group[count].name = src->name;
        monster_group[count].inc_bases = src->inc_bases;
        rf_copy(monster_group[count].inc_flags, src->inc_flags);
        monster_group[count].n_inc_bases = src->n_inc_bases;
        monster_group[count].max_inc_bases = src->max_inc_bases;
        mem_free(src);
    }

    mem_free(s);
    parser_destroy(p);
    return 0;
}


static void cleanup_ui_knowledge_parsed_data(void)
{
    int i;

    for (i = 0; i < n_monster_group; ++i)
    {
        string_free((char*)monster_group[i].name);
        mem_free(monster_group[i].inc_bases);
    }
    mem_free(monster_group);
    monster_group = NULL;
    n_monster_group = 0;
}


struct file_parser ui_knowledge_parser =
{
    "ui_knowledge",
    init_ui_knowledge_parser,
    run_ui_knowledge_parser,
    finish_ui_knowledge_parser,
    cleanup_ui_knowledge_parsed_data
};


/*
 * Main knowledge menus
 */


static void do_cmd_knowledge_history(struct player *p, int line)
{
    ang_file *fff;
    char file_name[MSG_LEN];

    /* Temporary file */
    fff = file_temp(file_name, sizeof(file_name));
    if (!fff) return;

    /* Dump character history */
    dump_history(p, fff);

    /* Close the file */
    file_close(fff);

    /* Display the file contents */
    show_file(p, file_name, "Character History", line, 0);

    /* Remove the file */
    file_delete(file_name);
}


/*
 * Display known uniques
 */
static void do_cmd_knowledge_uniques(struct player *p, int line)
{
    int k, l, i, space, namelen, total = 0, width = NORMAL_WID - 2;
    ang_file *fff;
    char file_name[MSG_LEN], buf[MSG_LEN];
    int* idx;
    struct monster_race *race, *curr_race;
    struct monster_lore *lore;

    /* Temporary file */
    fff = file_temp(file_name, sizeof(file_name));
    if (!fff) return;

    idx = mem_zalloc(z_info->r_max * sizeof(int));

    /* Scan the monster races */
    for (k = 1; k < z_info->r_max; k++)
    {
        race = &r_info[k];

        /* Only print Uniques that can be killed */
        if (race_is_unique(race) && !rf_has(race->flags, RF_NO_DEATH))
        {
            /* Only display "known" uniques */
            if (race->lore.seen)
            {
                l = 0;
                while (l < total)
                {
                    curr_race = &r_info[idx[l]];
                    if (race->level > curr_race->level) break;
                    l++;
                }
                for (i = total; i > l; i--) idx[i] = idx[i - 1];
                idx[l] = k;
                total++;
            }
        }
    }

    if (total)
    {
        /* For each unique */
        for (l = total - 1; l >= 0; l--)
        {
            uint8_t ok = false;
            char highlight = 'D';

            race = &r_info[idx[l]];
            strnfmt(buf, sizeof(buf), "%s has been killed by: ", race->name);
            space = width - strlen(buf);

            /* Do we need to highlight this unique? */
            lore = get_lore(p, race);
            if (lore->pkills) highlight = 'w';

            /* Append all players who killed this unique */
            k = 0;
            for (i = 1; i <= NumPlayers; i++)
            {
                struct player *q = player_get(i);

                /* Skip the DM... except for the DM */
                if ((q->dm_flags & DM_SECRET_PRESENCE) && !is_dm_p(p)) continue;

                lore = get_lore(q, race);
                if (lore->pkills)
                {
                    ok = true;
                    namelen = strlen(q->name) + 2;
                    if (space - namelen < 0)
                    {
                        /* Out of space, flush the line */
                        file_putf(fff, "%c%s\n", highlight, buf);
                        my_strcpy(buf, "  ", sizeof(buf));
                        k = 0;
                        space = width;
                    }
                    if (k++) my_strcat(buf, ", ", sizeof(buf));
                    my_strcat(buf, q->name, sizeof(buf));
                    space -= namelen;
                }
            }

            if (ok)
                file_putf(fff, "%c%s\n", highlight, buf);
            else if (race->lore.tkills)
                file_putf(fff, "D%s has been killed by somebody.\n", race->name);
            else
                file_putf(fff, "D%s has never been killed!\n", race->name);
        }
    }
    else file_put(fff, "wNo uniques are witnessed so far.\n");

    /* Close the file */
    file_close(fff);

    /* Display the file contents */
    show_file(p, file_name, "Known Uniques", line, 1);

    /* Remove the file */
    file_delete(file_name);

    mem_free(idx);
}


/*
 * Display party gear
 */
static void do_cmd_knowledge_gear(struct player *p, int line)
{
    int k, i;
    ang_file *fff;
    char file_name[MSG_LEN];
    struct object *obj;
    char o_name[NORMAL_WID];
    uint8_t color;

    /* Temporary file */
    fff = file_temp(file_name, sizeof(file_name));
    if (!fff) return;

    /* Scan the players */
    for (k = 1; k <= NumPlayers; k++)
    {
        struct player *q = player_get(k);

        /* Only print connected players XXX */
        if (q->conn == -1) continue;

        /* Don't display self */
        if (p == q) continue;

        /* DM sees everybody - others see party members */
        if ((!p->party || (p->party != q->party)) && !(p->dm_flags & DM_SEE_PLAYERS))
            continue;

        /* Print a message */
        if (q->total_winner)
        {
            file_putf(fff, "G     %s the %s %s (%s, Level %d) at %d ft (%d, %d)\n", q->name,
                q->race->name, q->clazz->name, get_title(q), q->lev, q->wpos.depth * 50,
                q->wpos.grid.x, q->wpos.grid.y);
        }
        else
        {
            file_putf(fff, "G     %s the %s %s (Level %d) at %d ft (%d, %d)\n", q->name,
                q->race->name, q->clazz->name, q->lev, q->wpos.depth * 50,
                q->wpos.grid.x, q->wpos.grid.y);
        }

        /* Display the equipment */
        for (i = 0; i < q->body.count; i++)
        {
            obj = slot_object(q, i);

            /* We need an item */
            if (!obj) continue;

            /* Obtain an item description */
            object_desc(p, o_name, sizeof(o_name), obj, ODESC_PREFIX | ODESC_FULL);

            /* Get the color */
            color = obj->kind->base->attr;

            /* Display item description */
            file_putf(fff, "%c         %s\n", color_attr_to_char(color), o_name);
        }

        /* Next */
        file_put(fff, "w\n");
    }

    /* Close the file */
    file_close(fff);

    /* Display the file contents */
    if (p->dm_flags & DM_SEE_PLAYERS)
        show_file(p, file_name, "Player List", line, 1);
    else
        show_file(p, file_name, "Party Members", line, 1);

    /* Remove the file */
    file_delete(file_name);
}


/*
 * Display owned houses
 */
static void do_cmd_knowledge_houses(struct player *p, int line)
{
    char file_name[MSG_LEN];
    ang_file *fff;

    /* Temporary file */
    fff = file_temp(file_name, sizeof(file_name));
    if (!fff) return;

    /* List owned houses */
    house_list(p, fff);

    /* Close the file */
    file_close(fff);

    /* Display the file contents */
    show_file(p, file_name, "Owned Houses", line, 0);

    /* Remove the file */
    file_delete(file_name);
}


/*
 * Display visited dungeons and towns
 */
static void do_cmd_knowledge_dungeons(struct player *p, int line)
{
    char file_name[MSG_LEN];
    ang_file *fff;

    /* Temporary file */
    fff = file_temp(file_name, sizeof(file_name));
    if (!fff) return;

    /* List visited dungeons and towns */
    dungeon_list(p, fff);

    /* Close the file */
    file_close(fff);

    /* Display the file contents */
    show_file(p, file_name, "Visited Dungeons and Towns", line, 0);

    /* Remove the file */
    file_delete(file_name);
}


/*
 * Display the "player knowledge" menu.
 */
void do_cmd_knowledge(struct player *p, char type, int line)
{
    switch (type)
    {
        /* Display object knowledge */
        case SPECIAL_FILE_OBJECT:
            Send_term_info(p, NTERM_ACTIVATE, NTERM_WIN_SPECIAL);
            do_cmd_knowledge_objects(p, line);
            Send_term_info(p, NTERM_ACTIVATE, NTERM_WIN_OVERHEAD);
            break;

        /* Display rune knowledge */
        case SPECIAL_FILE_RUNE:
            Send_term_info(p, NTERM_ACTIVATE, NTERM_WIN_SPECIAL);
            do_cmd_knowledge_runes(p, line);
            Send_term_info(p, NTERM_ACTIVATE, NTERM_WIN_OVERHEAD);
            break;

        /* Display artifact knowledge */
        case SPECIAL_FILE_ARTIFACT:
            Send_term_info(p, NTERM_ACTIVATE, NTERM_WIN_SPECIAL);
            do_cmd_knowledge_artifacts(p, line);
            Send_term_info(p, NTERM_ACTIVATE, NTERM_WIN_OVERHEAD);
            break;

        /* Display ego item knowledge */
        case SPECIAL_FILE_EGO:
            Send_term_info(p, NTERM_ACTIVATE, NTERM_WIN_SPECIAL);
            do_cmd_knowledge_ego_items(p, line);
            Send_term_info(p, NTERM_ACTIVATE, NTERM_WIN_OVERHEAD);
            break;

        /* Display monster knowledge */
        case SPECIAL_FILE_KILL:
            Send_term_info(p, NTERM_ACTIVATE, NTERM_WIN_SPECIAL);
            do_cmd_knowledge_monsters(p, line);
            Send_term_info(p, NTERM_ACTIVATE, NTERM_WIN_OVERHEAD);
            break;

        /* Display feature knowledge */
        case SPECIAL_FILE_FEATURE:
            Send_term_info(p, NTERM_ACTIVATE, NTERM_WIN_SPECIAL);
            do_cmd_knowledge_features(p, line);
            Send_term_info(p, NTERM_ACTIVATE, NTERM_WIN_OVERHEAD);
            break;

        /* Display trap knowledge */
        case SPECIAL_FILE_TRAP:
            Send_term_info(p, NTERM_ACTIVATE, NTERM_WIN_SPECIAL);
            do_cmd_knowledge_traps(p, line);
            Send_term_info(p, NTERM_ACTIVATE, NTERM_WIN_OVERHEAD);
            break;

        /* Display hall of fame */
        case SPECIAL_FILE_SCORES:
            Send_term_info(p, NTERM_ACTIVATE, NTERM_WIN_SPECIAL);
            do_cmd_knowledge_scores(p, line);
            Send_term_info(p, NTERM_ACTIVATE, NTERM_WIN_OVERHEAD);
            break;

        /* Display character history */
        case SPECIAL_FILE_HISTORY:
            Send_term_info(p, NTERM_ACTIVATE, NTERM_WIN_SPECIAL);
            do_cmd_knowledge_history(p, line);
            Send_term_info(p, NTERM_ACTIVATE, NTERM_WIN_OVERHEAD);
            break;

        /* Display known uniques */
        case SPECIAL_FILE_UNIQUE:
            Send_term_info(p, NTERM_ACTIVATE, NTERM_WIN_SPECIAL);
            do_cmd_knowledge_uniques(p, line);
            Send_term_info(p, NTERM_ACTIVATE, NTERM_WIN_OVERHEAD);
            break;

        /* Display party gear */
        case SPECIAL_FILE_GEAR:
            Send_term_info(p, NTERM_ACTIVATE, NTERM_WIN_SPECIAL);
            do_cmd_knowledge_gear(p, line);
            Send_term_info(p, NTERM_ACTIVATE, NTERM_WIN_OVERHEAD);
            break;

        /* Display owned houses */
        case SPECIAL_FILE_HOUSES:
            Send_term_info(p, NTERM_ACTIVATE, NTERM_WIN_SPECIAL);
            do_cmd_knowledge_houses(p, line);
            Send_term_info(p, NTERM_ACTIVATE, NTERM_WIN_OVERHEAD);
            break;

        /* Display visited dungeons and towns */
        case SPECIAL_FILE_DUNGEONS:
            Send_term_info(p, NTERM_ACTIVATE, NTERM_WIN_SPECIAL);
            do_cmd_knowledge_dungeons(p, line);
            Send_term_info(p, NTERM_ACTIVATE, NTERM_WIN_OVERHEAD);
            break;
    }
}


/*
 * Other knowledge functions
 */


/*
 * Hack -- redraw the screen
 *
 * This command performs various low level updates, clears all the "extra"
 * windows, does a total redraw of the main window, and requests all of the
 * interesting updates and redraws that I can think of.
 */
void do_cmd_redraw(struct player *p)
{
    /* Not while shopping */
    if (in_store(p)) return;

    verify_panel(p);

    /* Combine the pack (later) */
    p->upkeep->notice |= (PN_COMBINE);

    /* Update stuff */
    p->upkeep->update |= (PU_BONUS | PU_SPELLS | PU_INVEN);

    /* Fully update the visuals */
    p->upkeep->update |= (PU_UPDATE_VIEW | PU_MONSTERS);

    /* Redraw */
    p->upkeep->redraw |= (PR_BASIC | PR_EXTRA | PR_MAP | PR_FLOOR | PR_SPELL | PR_MONSTER |
        PR_OBJECT | PR_MONLIST | PR_ITEMLIST);
    set_redraw_equip(p, NULL);
    set_redraw_inven(p, NULL);
}


/*
 * Drop some gold
 */
void do_cmd_drop_gold(struct player *p, int32_t amt)
{
    struct object *obj;

    // no drop on floor in town (don't try to hide your gold from thieves or death..)
    if (p->wpos.depth == 0 && streq(p->terrain, "\tFloor\0")) {
        msg(p, "You need to be outside the building to drop gold.");
        return;
    }

    /* Restrict ghosts */
    if (p->ghost && !(p->dm_flags & DM_GHOST_BODY))
    {
        msg(p, "You need a tangible body to drop gold!");
        return;
    }

    /* Handle the newbies_cannot_drop option */
    if (newbies_cannot_drop(p))
    {
        msg(p, "You are not experienced enough to drop gold.");
        return;
    }

    /* Error checks */
    if (amt <= 0) return;
    if (amt > p->au)
    {
        msg(p, "You do not have that much gold.");
        return;
    }

    /* Take a turn */
    use_energy(p);

    /* Subtract from the player's gold */
    p->au -= amt;

    /* Message */
    msg(p, "You drop %ld gold piece%s.", amt, PLURAL(amt));

    /* Redraw gold */
    p->upkeep->redraw |= (PR_GOLD);

    /* Setup the object */
    obj = object_new();
    object_prep(p, chunk_get(&p->wpos), obj, money_kind("gold", amt), 0, MINIMISE);

    /* Setup the "worth" */
    obj->pval = amt;

    /* Set original owner ONCE */
    if (obj->origin_player == 0) obj->origin_player = quark_add(p->name);

    /* Pile of gold is now owned */
    obj->owner = p->id;

    /* Drop it */
    drop_near(p, chunk_get(&p->wpos), &obj, 0, &p->grid, false, DROP_FADE, true);
}


/*
 * Get a random object from a player's inventory
 */
static struct object *get_random_player_object(struct player *p)
{
    int tries;

    /* Find an item */
    for (tries = 0; tries < 100; tries++)
    {
        /* Pick an item */
        int index = randint0(z_info->pack_size);

        /* Obtain the item */
        struct object *obj = p->upkeep->inven[index];

        /* Skip non-objects */
        if (obj == NULL) continue;

        /* Skip artifacts */
        if (obj->artifact) continue;

        /* Skip deeds of property */
        if (tval_is_deed(obj)) continue;

        return obj;
    }

    return NULL;
}


/*
 * Get a random object from a monster's inventory
 */
static struct object *get_random_monster_object(struct monster *mon)
{
    struct object *obj, *pick = NULL;
    int i = 1;

    /* Pick a random object */
    for (obj = mon->held_obj; obj; obj = obj->next)
    {
        /* Check it isn't a quest artifact */
        if (obj->artifact && (kf_has(obj->kind->kind_flags, KF_QUEST_ART) ||
            kf_has(obj->kind->kind_flags, KF_EPIC)))
            continue;

        if (one_in_(i)) pick = obj;
        i++;
    }

    return pick;
}


/*
 * Attempt to steal from another player or monster
 */
void do_cmd_steal(struct player *p, int dir)
{
    struct chunk *c = chunk_get(&p->wpos);
    struct source who_body;
    struct source *who = &who_body;
    char m_name[NORMAL_WID];
    struct object *obj;
    bool success = false;
    int notice;
    struct loc grid;

    /* Attack or steal from players */
    bool player_steal;

    /* Attack or steal from monsters */
    bool monster_steal;

    /* Paranoia */
    if ((dir == DIR_TARGET) || !dir) return;

    /* Ensure "dir" is in ddx/ddy array bounds */
    if (!VALID_DIR(dir)) return;

    next_grid(&grid, &p->grid, dir);
    square_actor(c, &grid, who);

    /* Restrict ghosts */
    if ((p->ghost && !(p->dm_flags & DM_GHOST_HANDS)) || p->timed[TMD_WRAITHFORM])
    {
        msg(p, "You need a tangible body to steal items!");
        return;
    }

    /* Not when under some status conditions */
    if (p->timed[TMD_BLIND] || p->timed[TMD_BLIND_REAL] ||
        p->timed[TMD_CONFUSED] || p->timed[TMD_CONFUSED_REAL] ||
        p->timed[TMD_IMAGE] || p->timed[TMD_IMAGE_REAL])
    {
        msg(p, "Your current condition prevents you from stealing anything.");
        return;
    }

    /* Restricted by choice */
    if ((cfg_limited_stores == 3) || OPT(p, birth_no_stores))
    {
        msg(p, "You cannot steal.");
        return;
    }

    /* Check preventive inscription '^S' */
    if (check_prevent_inscription(p, INSCRIPTION_STEAL))
    {
        msg(p, "The item's inscription prevents it.");
        return;
    }

    /* May only steal from visible players */
    player_steal = (who->player && player_is_visible(p, who->idx));
    if (player_steal)
    {
        /* May not steal if hostile */
        if (pvp_check(p, who->player, PVP_CHECK_ONE, true, 0x00))
        {
            /* Message */
            msg(p, "%s is on guard against you.", who->player->name);
            return;
        }

        /* May not steal if the target cannot retaliate */
        if ((cfg_pvp_hostility >= PVP_SAFE) || in_party(who->player, p->party) ||
            square_issafefloor(c, &grid))
        {
            /* Message */
            msg(p, "You cannot steal from that player.");
            return;
        }
    }

    /* May only steal from visible monsters */
    monster_steal = (player_has(p, PF_STEAL) && who->monster && monster_is_visible(p, who->idx));
    if (monster_steal)
    {
        /* May not steal if awake */
        if (!who->monster->m_timed[MON_TMD_SLEEP])
        {
            /* Message */
            monster_desc(p, m_name, sizeof(m_name), who->monster, MDESC_STANDARD);
            msg(p, "%s is on guard against you.", m_name);
            return;
        }

        /* May not steal if friendly */
        if (!pvm_check(p, who->monster))
        {
            /* Message */
            msg(p, "You cannot steal from that monster.");
            return;
        }
    }

    /* No valid target */
    if (!player_steal && !monster_steal)
    {
        msg(p, "You see nothing there to steal from.");
        return;
    }

    /* Find an item */
    if (player_steal)
    {
        /* Steal gold 25% of the time */
        if (magik(25))
        {
            int amt = who->player->au / 10;

            obj = NULL;

            /* No gold... or too much gold */
            if (!amt || ((p->au + amt) > PY_MAX_GOLD))
            {
                msg(p, "You can find nothing to steal from %s.", who->player->name);
                return;
            }
        }
        else
        {
            obj = get_random_player_object(who->player);

            /* No object with level requirement */
            if (!obj || !has_level_req(p, obj))
            {
                msg(p, "You can find nothing to steal from %s.", who->player->name);
                return;
            }
        }
    }
    else
    {
        obj = get_random_monster_object(who->monster);

        /* No object */
        if (!obj)
        {
            monster_desc(p, m_name, sizeof(m_name), who->monster, MDESC_TARG);
            msg(p, "You can find nothing to steal from %s.", m_name);
            return;
        }

        /* Too much gold */
        if (tval_is_money(obj) && ((p->au + obj->pval) > PY_MAX_GOLD))
        {
            monster_desc(p, m_name, sizeof(m_name), who->monster, MDESC_TARG);
            msg(p, "You can find nothing to steal from %s.", m_name);
            return;
        }
    }

    /* Can only steal items if they can be carried */
    if (obj && !tval_is_money(obj))
    {
        struct object *test;
        bool ok = true;
        int amt;

        /* Get a copy with the right "amt" */
        if (player_steal) amt = 1;
        else amt = obj->number;
        test = object_new();
        object_copy_amt(test, obj, amt);

        /* Note that the pack is too full */
        if (!inven_carry_okay(p, test)) ok = false;

        /* Note that the pack is too heavy */
        else if (!weight_okay(p, test)) ok = false;

        object_delete(&test);
        if (!ok)
        {
            msg(p, "You are too encumbered to steal anything");
            return;
        }
    }

    /* Compute chance of success */
    if (player_steal)
    {
        int chance = 3 * (adj_dex_safe[p->state.stat_ind[STAT_DEX]] -
            adj_dex_safe[who->player->state.stat_ind[STAT_DEX]]);

        /* Compute base chance of being noticed */
        notice = 5 * (adj_mag_stat[who->player->state.stat_ind[STAT_INT]] -
            p->state.skills[SKILL_STEALTH]);

        /* Hack -- rogues get bonuses to chances */
        if (player_has(p, PF_STEALING_IMPROV))
        {
            /* Increase chance by level */
            chance += 3 * p->lev;
            notice -= 3 * p->lev;
        }

        /* Hack -- always small chance to succeed */
        if (chance < 2) chance = 2;

        /* Check for success */
        if (magik(chance)) success = true;
    }
    else
    {
        /* Base monster protection and player stealing skill */
        bool unique = monster_is_unique(who->monster);
        int guard = (who->monster->race->level * (unique? 4: 3)) / 4 + who->monster->mspeed -
            p->state.speed;
        int steal_skill = p->state.skills[SKILL_STEALTH] + adj_dex_th[p->state.stat_ind[STAT_DEX]];
        int monster_reaction;

        /* Penalize some status conditions (PWMAngband: always) */
        guard /= 2;

        /* Monster base reaction, plus allowance for item weight */
        monster_reaction = guard / 2 + randint1(MAX(guard, 1));
        if (obj && !tval_is_money(obj)) monster_reaction += (obj->number * obj->weight) / 20;

        /* Check for success */
        if (monster_reaction < steal_skill) success = true;
    }

    /* Success! */
    if (success)
    {
        if (player_steal)
        {
            /* Steal gold 25% of the time */
            if (!obj)
            {
                int amt = who->player->au / 10;

                /* Move from target to thief */
                who->player->au -= amt;
                p->au += amt;

                /* Redraw */
                p->upkeep->redraw |= (PR_GOLD);
                who->player->upkeep->redraw |= (PR_GOLD);

                /* Tell thief */
                msg(p, "You steal %d gold from %s.", amt, who->player->name);

                /* Check for target noticing */
                if (magik(notice))
                {
                    /* Make target hostile */
                    pvp_check(who->player, p, PVP_ADD, true, 0x00);

                    /* Message */
                    msg(who->player, "You notice %s stealing %d gold!", p->name, amt);
                }
            }

            /* Steal an item */
            else
            {
                struct object *stolen;
                char o_name[NORMAL_WID], t_name[NORMAL_WID];
                bool none_left = false;

                /* Steal and carry, easier to notice heavier objects */
                stolen = gear_object_for_use(who->player, obj, 1, false, &none_left);
                object_desc(p, o_name, sizeof(o_name), stolen, ODESC_PREFIX | ODESC_FULL);
                object_desc(who->player, t_name, sizeof(t_name), stolen, ODESC_PREFIX | ODESC_FULL);
                notice += stolen->weight;
                inven_carry(p, stolen, true, false);

                /* Tell thief what he got */
                msg(p, "You steal %s from %s.", o_name, who->player->name);

                /* Check for target noticing */
                if (magik(notice))
                {
                    /* Make target hostile */
                    pvp_check(who->player, p, PVP_ADD, true, 0x00);

                    /* Message */
                    msg(who->player, "You notice %s stealing %s!", p->name, t_name);
                }
            }
        }
        else
        {
            /* Object no longer held */
            obj->held_m_idx = 0;
            pile_excise(&who->monster->held_obj, obj);

            if (tval_is_money(obj))
            {
                monster_desc(p, m_name, sizeof(m_name), who->monster, MDESC_TARG);
                msg(p, "You steal %d gold from %s.", obj->pval, m_name);
                p->au += obj->pval;
                p->upkeep->redraw |= (PR_GOLD);
                object_delete(&obj);
            }
            else
            {
                char o_name[NORMAL_WID];
                struct monster_lore *lore = get_lore(p, who->monster->race);

                object_desc(p, o_name, sizeof(o_name), obj, ODESC_PREFIX | ODESC_FULL);
                msg(p, "You steal %s from %s.", o_name, m_name);
                inven_carry(p, obj, true, false);

                /* Track thefts */
                if (lore->thefts < SHRT_MAX) lore->thefts++;
            }

            /* Monster wakes, may notice */
            monster_wake(p, who->monster, true, 50);
        }

        /* Player hit and run */
        if (p->timed[TMD_ATT_RUN])
        {
            struct source thief_body;
            struct source *thief = &thief_body;

            msg(p, "You vanish into the shadows!");
            msg_misc(p, "There is a puff of smoke!");
            source_player(thief, get_player_index(get_connection(p->conn)), p);
            effect_simple(EF_TELEPORT, thief, "20", 0, 0, 0, 0, 0, NULL);
            player_clear_timed(p, TMD_ATT_RUN, false);
        }
    }
    
    /* Failure */
    else
    {
        if (player_steal)
        {
            /* Message */
            msg(p, "You fail to steal anything from %s.", who->player->name);

            /* Easier to notice a failed attempt */
            if (magik(notice + 50))
            {
                /* Make target hostile */
                pvp_check(who->player, p, PVP_ADD, true, 0x00);

                /* Message */
                msg(who->player, "You notice %s trying to steal from you!", p->name);
            }
        }
        else
        {
            monster_desc(p, m_name, sizeof(m_name), who->monster, MDESC_TARG);
            msg(p, "You fail to steal anything from %s.", m_name);

            /* Monster wakes, may notice */
            monster_wake(p, who->monster, true, 50);
        }
    }

    /* Take a turn */
    use_energy(p);
}


/*
 * Display known info about a monster or group of monsters specified by name or symbol
 */
void do_cmd_query_symbol(struct player *p, const char *buf)
{
    int i;
    bool found = false;
    char *str;

    /* Let the player scroll through this info */
    p->special_file_type = SPECIAL_FILE_OTHER;

    /* Prepare player structure for text */
    text_out_init(p);

    /* Lowercase our search string */
    if (strlen(buf) > 1)
    {
        for (str = (char*)buf; *str; str++) *str = tolower((unsigned char)*str);
    }

    /* Collect matching monsters */
    for (i = 1; i < z_info->r_max; i++)
    {
        struct monster_race *race = &r_info[i];
        bool ok;

        /* Skip non-entries */
        if (!race->name) continue;

        /* Check if the input is a symbol */
        if (strlen(buf) == 1)
            ok = (race->d_char == buf[0]);

        /* The input is a partial monster name */
        else
        {
            char monster[NORMAL_WID];

            /* Clean up monster name */
            clean_name(monster, race->name);

            /* Check if cleaned name matches our search string */
            ok = (strstr(monster, buf) != NULL);
        }

        /* Dump info */
        if (ok)
        {
            /* Describe it */
            if (found) text_out(p, "\n\n\n");
            lore_description(p, race);

            found = true;
        }
    }
    if (!found) text_out(p, "You fail to remember any monsters of this kind.");

    /* Restore height and width of current dungeon level */
    text_out_done(p);

    /* Notify player */
    notify_player(p, format("Monster Recall ('%s')", buf), NTERM_WIN_MONSTER, false);
}


/*
 * Display known info about a monster/player
 */
void do_cmd_describe(struct player *p)
{
    struct source *cursor_who = &p->cursor_who;

    /* We need something */
    if (source_null(cursor_who)) return;

    /* Describe it */
    if (cursor_who->player) describe_player(p, cursor_who->player);
    else describe_monster(p, cursor_who->monster->race);

    /* Notify player */
    notify_player(p, "Monster Recall", NTERM_WIN_MONSTER, false);
}


/*
 * Allow the player to examine other sectors on the map
 */
void do_cmd_locate(struct player *p, int dir)
{
    struct loc begin, end;
    char tmp_val[NORMAL_WID];
    char out_val[160];
    int screen_hgt, screen_wid;
    int panel_hgt, panel_wid;

    /* Hack -- set locating */
    p->locating = (dir? true: false);

    /* Use dimensions that match those in display-ui.c. */
    screen_hgt = p->screen_rows / p->tile_hgt;
    screen_wid = p->screen_cols / p->tile_wid;

    panel_hgt = screen_hgt / 2;
    panel_wid = screen_wid / 2;

    /* Bound below to avoid division by zero */
    panel_hgt = MAX(panel_hgt, 1);
    panel_wid = MAX(panel_wid, 1);

    /* No direction, recenter */
    if (!dir)
    {
        /* Forget current panel */
        loc_init(&p->old_offset_grid, -1, -1);

        /* Recenter map around the player */
        verify_panel(p);

        return;
    }

    /* Ensure "dir" is in ddx/ddy array bounds */
    if (!VALID_DIR(dir)) return;

    /* Initialize */
    if (dir == DIR_TARGET)
    {
        /* Remember current panel */
        loc_copy(&p->old_offset_grid, &p->offset_grid);
    }

    /* Start at current panel */
    loc_copy(&begin, &p->old_offset_grid);

    /* Apply the motion */
    change_panel(p, dir);

    /* Handle stuff */
    handle_stuff(p);

    /* Get the current panel */
    loc_copy(&end, &p->offset_grid);

    /* Describe the location */
    if (loc_eq(&end, &begin))
        tmp_val[0] = '\0';
    else
    {
        strnfmt(tmp_val, sizeof(tmp_val), "%s%s of", ((end.y < begin.y)? " north":
            (end.y > begin.y)? " south": ""), ((end.x < begin.x)? " west":
            (end.x > begin.x)? " east": ""));
    }

    /* Prepare to ask which way to look */
    strnfmt(out_val, sizeof(out_val), "Map sector [%d,%d], which is%s your sector.  Direction?",
        (end.y / panel_hgt), (end.x / panel_wid), tmp_val);

    /* More detail */
    if (OPT(p, center_player))
    {
        strnfmt(out_val, sizeof(out_val),
            "Map sector [%d(%02d),%d(%02d)], which is%s your sector.  Direction?",
            (end.y / panel_hgt), (end.y % panel_hgt), (end.x / panel_wid),
            (end.x % panel_wid), tmp_val);
    }

    msg(p, out_val);
}


// for Villager class in do_cmd_fountain() when _ on the fields
const char* get_crop_message(int rng)
{
    switch(rng)
    {
    case 0: return "You munch on a tiny potato you found, tasting mostly dirt.";
    case 1: return "You crunch through a gritty potato, hoping no one's watching.";
    case 2: return "You bite into a stale potato, regretting it instantly.";
    case 3: return "You nibble a forgotten potato; it's tough but fills you slightly.";
    case 4: return "You tear into a wilted cabbage, its bitterness strong.";
    case 5: return "You chew on a leftover cabbage head, surprisingly crunchy.";
    case 6: return "You swallow bits of an old cabbage, your stomach protesting.";
    case 7: return "You gnaw on a rubbery cabbage, barely edible.";
    case 8: return "You managed to find an old carrot.. and almost break your teeth on it!";
    case 9: return "You crunch on a hard carrot, dirt included.";
    case 10: return "You chew a stubby carrot, wondering how long it's been there.";
    case 11: return "You force down a tough carrot, your jaw aching.";
    case 12: return "You bite into a bitter beet, immediately regretting your choices.";
    case 13: return "You munch on a leathery beet, determined to finish it.";
    case 14: return "You chew a forgotten beet; it's unpleasant but edible.";
    case 15: return "You gnaw at a hard beet, fighting the earthy taste.";
    case 16: return "You choke down a dry squash, missing better days.";
    case 17: return "You eat a hardened squash, barely soft enough to swallow.";
    case 18: return "You bravely chew an overlooked squash, flavor mostly gone.";
    case 19: return "You struggle with a tough squash, but manage to finish.";
    case 20: return "You bite dried corn off its stalk, kernels like tiny stones.";
    case 21: return "You chew through a small ear of old corn, gritty but edible.";
    case 22: return "You eat forgotten corn, thankful for even stale kernels.";
    case 23: return "You manage to gnaw through tough corn, barely swallowing.";
    case 24: return "You crunch old turnips, ignoring their pungent flavor.";
    case 25: return "You bravely chew a withered radish, fighting bitterness.";
    case 26: return "You force down a small onion, tears filling your eyes.";
    case 27: return "You snack on wild garlic, breath turning pungent.";
    case 28: return "You quickly eat wild berries, their sourness sharp.";
    case 29: return "You gulp down some wild mushrooms, hoping they're safe.";
    default: return "You nibble uncertain crops, hunger overcoming caution.";
    }
}


static void drink_fountain(struct player *p, struct object *obj)
{
    struct effect *effect;
    bool ident = false, used = false;

    /* The player is aware of the object's flavour */
    p->was_aware = object_flavor_is_aware(p, obj);

    /* Figure out effect to use */
    effect = object_effect(obj);

    /* Do effect */
    if (effect)
    {
        struct source who_body;
        struct source *who = &who_body;

        /* Do effect */
        if (effect->other_msg) msg_misc(p, effect->other_msg);
        source_player(who, get_player_index(get_connection(p->conn)), p);
        used = effect_do(effect, who, &ident, p->was_aware, 0, NULL, 0, 0, NULL);

        /* Quit if the item wasn't used and no knowledge was gained */
        if (!used && (p->was_aware || !ident)) return;
    }

    if (ident) object_notice_effect(p, obj);
}


/// drink from fountain or water can increase satiation
void drink_water_satiation(struct player *p, int satiation) {

    if (streq(p->race->name, "Vampire") || streq(p->race->name, "Undead") ||
        streq(p->race->name, "Golem") || streq(p->race->name, "Wraith") ||
        streq(p->race->name, "Djinn"))
        return;

    if (p->timed[TMD_FOOD] < 100) { // starving
        satiation += 300;
    } else if (p->timed[TMD_FOOD] < 400) { // faint
        satiation += 200;
    } else if (p->timed[TMD_FOOD] < 800) { // weak
        satiation += 100;
    } else if ((OPT(p, birth_zeitnot) || // zeitnot
        (OPT(p, birth_no_recall) && OPT(p, birth_force_descend))) &&
         p->timed[TMD_FOOD] < 3000)
            satiation += 750;

    /* Apply the satiation */
    player_inc_timed(p, TMD_FOOD, satiation, false, false);
}


/*
///// Drink from fountain/fill an empty bottle OR drink from water tile
 */
void do_cmd_fountain(struct player *p, int item)
{
    struct object *obj;
    struct object_kind *kind;
    struct chunk *c = chunk_get(&p->wpos);
    bool fountain = false;

    /* Restrict ghosts */
    if (p->ghost && !(p->dm_flags & DM_GHOST_BODY))
    {
        msg(p, "You need a tangible body to do that!");
        return;
    }

    /* Check preventive inscription '^_' */
    if (check_prevent_inscription(p, INSCRIPTION_FOUNTAIN))
    {
        msg(p, "The item's inscription prevents it.");
        return;
    }

    ///////////////////////////////////////// <<< separate case
    // Crafter can detonate sentry
    if (streq(p->clazz->name, "Crafter"))
    {       
        if (detonate_sentry(p))
            use_energy(p);
    }
    ///////////////////////////////////////// <<< separate case (so it will be possible to drink from fountains)

    /* If we stand on a fountain, ensure it is not dried out */
    if (square_isfountain(c, &p->grid))
    {
        if (square_isdryfountain(c, &p->grid))
        {
            msg(p, "The fountain is dried out.");
            return;
        }

        fountain = true;
    }
    // golem race drinks oil ! if not standing on fountain !
    else if (streq(p->race->name, "Golem"))
    {       
        if (use_oil(p))
        {
            use_energy(p);
            player_inc_timed(p, TMD_OCCUPIED, 1 + randint0(2), true, false);
        }

        return;
    }
    // Djinn race consume wand/staves ! if not standing on fountain !
    else if (streq(p->race->name, "Djinn"))
    {       
        if (consume_magic_items(p))
        {
            use_energy(p);
            player_inc_timed(p, TMD_OCCUPIED, 1 + randint0(2), true, false);
        }

        return;
    }
    // Undead race consume corpses ! if not standing on fountain !
    else if (streq(p->race->name, "Undead"))
    {       
        if (consume_corpse(p))
        {
            use_energy(p);
            player_inc_timed(p, TMD_OCCUPIED, 1 + randint0(2), true, false);
        }

        return;
    }
    else if (streq(p->clazz->name, "Villager") && // can dig out old crops from T fields
            (tf_has(f_info[square(c, &p->grid)->feat].flags, TF_T_FARM_FIELD)))
    {
        if (p->timed[TMD_FOOD] < 1500) // till upper threshold of "Hungry" status
        {
            int rng;

            // get plenty of extra satiation from stealing veggies
            // (idea is to make it a bit better than satiation from races abilities..
            // you kinda 'eat in advance' once, but can't repeat due <1500 limit)
            player_inc_timed(p, TMD_FOOD, 750, false, false);
            use_energy(p);
            player_inc_timed(p, TMD_OCCUPIED, 5 + randint0(3), true, false);
            
            // Get a random message
            rng = randint0(30);
            msg(p, get_crop_message(rng));
        }
        else
        {
            msg(p, "You are not hungry enough to steal from other farmers' fields...");
        }

        return;
    }
    /* Allow only on water tiles */
    else if (!square_iswater(c, &p->grid) &&
            !tf_has(f_info[square(c, &p->grid)->feat].flags, TF_SHALLOW_WATER) &&
            !tf_has(f_info[square(c, &p->grid)->feat].flags, TF_BAD_WATER) &&
            !tf_has(f_info[square(c, &p->grid)->feat].flags, TF_FOUL_WATER))
    {
        msg(p, "You need a source of water.");
        return;
    }

/*  No need as we allow everyone to drink from any water tile now

    else if ((item == -1) && !(streq(p->race->name, "Ent") || streq(p->race->name, "Merfolk")))
    {
        msg(p, "You need an empty bottle.");
        return;
    }
*/

    /* Take a turn */
    use_energy(p);
    
    /* Fruit bat! */
    if ((item == -1) && fountain)
    {
        struct monster_race *race_fruit_bat = get_race("fruit bat");
        bool poly = false;

        /* Try (very rarely) to turn the player into a fruit bat */
        if ((p->poly_race != race_fruit_bat) && one_in_(200)) poly = true;

        /* Try (very hard) to restore characters back to normal */
        if ((p->poly_race == race_fruit_bat) && one_in_(3)) poly = true;

        if (poly)
        {
            msg(p, "You drink from the fountain.");
            poly_bat(p, 100, NULL);

            drink_water_satiation(p, 1); // GULP
            /* Done */
            return;
        }
    }

    /* Summon water creature */
    if (one_in_(20))
    {
        static const struct summon_chance_t
        {
            const char *race;
            uint8_t minlev;
            uint8_t chance;
        } summon_chance[] =
        {
            {"giant green frog", 0, 100},
            {"giant red frog", 7, 90},
            {"water spirit", 17, 80},
            {"water vortex", 21, 70},
            {"water elemental", 33, 60},
            {"water hound", 37, 50},
            {"seahorse", 42, 40},                
            {"Djinn", 50, 5}  
        };
        int i;
        int attempts = 0;

        msg(p, "Something pops out of the water!");
        do {
            i = randint0(N_ELEMENTS(summon_chance));
            attempts++;
            if (attempts > 100) break; // Just pick whatever we got
        }
        while ((p->wpos.depth < summon_chance[i].minlev) || !magik(summon_chance[i].chance));
        summon_specific_race(p, c, &p->grid, get_race(summon_chance[i].race), 1);

        drink_water_satiation(p, 1); // GULP
        /* Done */
        return;
    }

    /* Fall in */
    if (fountain && one_in_(20))
    {
        msg(p, "You slip and fall in the water.");
        if (!player_passwall(p) && !can_swim(p))
        {
            int dam = damroll(4, 5);

            dam = player_apply_damage_reduction(p, dam, false, "drowning");
            if (dam && OPT(p, show_damage))
                msg(p, "You take $r%d^r damage.", dam);
            take_hit(p, dam, "drowning", "slipped and fell in a fountain");
        }

        drink_water_satiation(p, 1); // GULP
        /* Done */
        return;
    }

    /* Message */
    if ((item == -1) && fountain)
        msg(p, "You drink from the fountain.");
    else
        msg(p, "You take a handful of water and make a gulp.");
    
    // Unbeliever unmagics fountain
    if (streq(p->clazz->name, "Unbeliever") && fountain && !one_in_(4))
    {    
        msg(p, "This tepid water is tasteless.");
        kind = lookup_kind_by_name(TV_POTION, "Water");
    }

    // Ent turns fresh (not bottled) fountain water into nourishing draught
    else if (streq(p->race->name, "Ent") && fountain)
    {
        msg(p, "After you touch the water becomes sparky and clean.");
        kind = lookup_kind_by_name(TV_POTION, "Water");
    }
    
    // Open water source (lake, river...)
    else if (!fountain)
        kind = lookup_kind_by_name(TV_POTION, "Water");

    /* Ale */
    else if (fountain && one_in_(10))
    {
        msg(p, "Wow! Pure dwarven ale!");
        kind = lookup_kind_by_name(TV_FOOD, "Pint of Fine Ale");
    }

    /* Plain water */
    else if (fountain? one_in_(2): magik(90))
    {
        msg(p, "The water is clear and fresh.");
        kind = lookup_kind_by_name(TV_POTION, "Water");
    }

    /* Random potion effect for fountains */
    else
    {
        int sval;

        /* Generate a potion */
        while (true)
        {
            /* Pick a random potion */
            sval = randint1(kb_info[TV_POTION].num_svals);

            /* Access the kind index */
            kind = lookup_kind_silent(TV_POTION, sval);
            if (!kind) continue;

            /* No out of depth effect */
            if (p->wpos.depth < kind->alloc_min) continue;

            /* Less chance to get the effect deeper */
            if ((p->wpos.depth > kind->alloc_max) && magik(50)) continue;

            /* Apply rarity */
            if (!magik(kind->alloc_prob)) continue;

            /* Success */
            break;
        }
        
        if (streq(kind->name, "Salt Water"))
            kind = lookup_kind_by_name(TV_POTION, "Water");

        /* Message */
        if (!kind->cost)
            msg(p, "The water is dark and muddy.");
        else
            msg(p, "The water sparkles gently.");
    }

    /* Prepare the object */
    obj = object_new();
    object_prep(p, c, obj, kind, p->wpos.depth, RANDOMISE);

    /* Set origin */
    set_origin(obj, ORIGIN_FOUNTAIN, p->wpos.depth, NULL);

    /* Get an empty bottle */
    if (item >= 0)
    {
        struct object *bottle = object_from_index(p, item, true, true), *used;
        bool none_left = false;

        /* Paranoia: requires a bottle */
        if (!bottle || !tval_is_bottle(bottle)) return;

        /* Check preventive inscription '!_' */
        if (object_prevent_inscription(p, bottle, INSCRIPTION_FOUNTAIN, false))
        {
            msg(p, "The item's inscription prevents it.");
            return;
        }

        /* Eliminate the item */
        used = gear_object_for_use(p, bottle, 1, false, &none_left);
        object_delete(&used);

        /* Create the object */
        apply_magic(p, c, obj, p->wpos.depth, false, false, false, false);

        /* Drop it in the dungeon */
        drop_near(p, c, &obj, 0, &p->grid, true, DROP_FADE, false);
    }

    /* Drink from a fountain OR water tile */
    else
    {
        drink_fountain(p, obj);

        // Magic fountains nourishment (only ent and merfolk)
        if (streq(p->race->name, "Ent") && fountain)
        {
            if (p->timed[TMD_FOOD] < 8000)
                drink_water_satiation(p, 300); // GULP
            hp_player(p, p->wpos.depth / 2);
        }
        else if (streq(p->race->name, "Merfolk") && streq(kind->name, "Water") && fountain)
        {
            drink_water_satiation(p, 75); // GULP
            hp_player(p, p->wpos.depth);
        }
        // Now water tile. They provide nourishment until certain fed status
        else if (streq(p->race->name, "Ent"))
        {
            if (p->timed[TMD_FOOD] < 1500)
                drink_water_satiation(p, 200); // GULP
        }
        else if (streq(p->race->name, "Merfolk"))
        {
            if (p->timed[TMD_FOOD] < 1000)
                drink_water_satiation(p, 150); // GULP
        }
        else // everyone else get satiation only when "weak" or less
        {
            drink_water_satiation(p, 1); // GULP
        }

        // bad water (eg in Sewers dungeon)
        if (tf_has(f_info[square(c, &p->grid)->feat].flags, TF_BAD_WATER) && !streq(p->race->name, "Ent"))
        {
            msg(p, "This water is no good!");
            player_inc_timed(p, TMD_POISONED, 10, true, false);
        }
        else if (tf_has(f_info[square(c, &p->grid)->feat].flags, TF_FOUL_WATER))
        {
            msg(p, "What a sickening liquid!");
            player_inc_timed(p, TMD_CONFUSED, 5 + randint0(10), true, false);
            player_inc_timed(p, TMD_IMAGE, randint1(10), true, false);
            player_inc_timed(p, TMD_POISONED, 10, true, false);
        }
        object_delete(&obj);
    }

    /* Fountain dries out */
    if (fountain && one_in_(3))
    {
        msg(p, "The fountain suddenly dries up.");
        square_dry_fountain(c, &p->grid);
    }
}


/*
 * Centers the map on the player
 */
void do_cmd_center_map(struct player *p)
{
    /* Not while shopping */
    if (in_store(p)) return;

    center_panel(p);
}


/*
 * Display the main-screen monster list.
 */
void do_cmd_monlist(struct player *p)
{
    /* Display visible monsters */
    monster_list_show_interactive(p, p->max_hgt, NORMAL_WID - 5);

    /* Notify player */
    notify_player(p, "Monster List", NTERM_WIN_MONLIST, true);
}


/*
 * Display the main-screen item list.
 */
void do_cmd_itemlist(struct player *p)
{
    /* Display visible objects */
    object_list_show_interactive(p, p->max_hgt, NORMAL_WID - 5);

    /* Notify player */
    notify_player(p, "Object List", NTERM_WIN_OBJLIST, true);
}


/*
 * Check the status of "players"
 *
 * The player's name, race, class, and experience level are shown.
 */
void do_cmd_check_players(struct player *p, int line)
{
    int k;
    ang_file *fff;
    char file_name[MSG_LEN];

    /* Temporary file */
    fff = file_temp(file_name, sizeof(file_name));
    if (!fff) return;

    /* Scan the player races */
    for (k = 1; k <= NumPlayers; k++)
    {
        struct player *q = player_get(k);
        uint8_t attr = 'w';
        char modes[50];
        char winner[20];

        /* Only print connected players XXX */
        if (q->conn == -1) continue;

        /*
         * Don't display the dungeon master if the secret_dungeon_master
         * option is set (unless you're a DM yourself)
         */
        if ((q->dm_flags & DM_SECRET_PRESENCE) && !(p->dm_flags & DM_SEE_PLAYERS)) continue;

        /*** Determine color ***/

        /* Print self in green */
        if (p == q) attr = 'G';

        /* Print party members in blue */
        else if (p->party && p->party == q->party) attr = 'B';

        /* Print hostile players in red */
        else if (pvp_check(p, q, PVP_CHECK_BOTH, true, 0x00)) attr = 'r';

        /* Print potential hostile players in yellow */
        else if (pvp_check(p, q, PVP_CHECK_ONE, true, 0x00)) attr = 'y';

        /* Output color uint8_t */
        file_putf(fff, "%c", attr);

        /* Build mode string */
        get_player_modes(q, modes, sizeof(modes));

        winner[0] = '\0';
        if (q->total_winner) strnfmt(winner, sizeof(winner), "%s, ", get_title(q));

        /* Print a message */
        file_putf(fff, "     %s the %s %s %s (%sLevel %d, %s)", q->name, modes, q->race->name,
            q->clazz->name, winner, q->lev, parties[q->party].name);

        /* Print extra info if these people are not 'red' aka hostile */
        /* Hack -- always show extra info to dungeon master */
        if ((attr != 'r') || (p->dm_flags & DM_SEE_PLAYERS))
        {
            /* Newline */
            file_put(fff, "\n");
            file_putf(fff, "%c", attr);
            file_putf(fff, "     %s at %d ft (%d, %d)",
                STRZERO(q->locname)? "Wilderness": q->locname,
                q->wpos.depth * 50, q->wpos.grid.x, q->wpos.grid.y);
        }

        /* Newline */
        file_put(fff, "\n");
        file_putf(fff, "U         %s@%s\n", q->full_name, q->hostname);
    }

    /* Close the file */
    file_close(fff);

    /* Display the file contents */
    show_file(p, file_name, "Player List", line, 1);

    /* Remove the file */
    file_delete(file_name);
}


/*
 * Scroll through information.
 */
void do_cmd_check_other(struct player *p, int line)
{
    char buf[NORMAL_WID + 1];
    int i, j;
    int16_t last_info_line = get_last_info_line(p);

    /* Make sure the player is allowed to */
    if (!p->special_file_type) return;

    /* Dump the next 20 lines of the file */
    for (i = 0; i < 20; i++)
    {
        uint8_t attr = COLOUR_WHITE;

        /* We're done */
        if (line + i > MAX_TXT_INFO) break;
        if (line + i > last_info_line) break;

        /* Extract string */
        for (j = 0; j < NORMAL_WID; j++)
            buf[j] = get_info(p, line + i, j)->c;
        attr = get_info(p, line + i, 0)->a;
        buf[j] = '\0';

        /* Dump the line */
        Send_special_line(p, last_info_line, last_info_line - line, i, attr, &buf[0]);
    }
}


/*
 * Returns the "affinity" of the player to the monster race.
 */
static int affinity(struct player *p, struct monster_race *race)
{
    size_t i, kills = 0, count = 0;

    /* Calculate monster race affinity */
    for (i = 1; i < (size_t)z_info->r_max; i++)
    {
        struct monster_race *r = &r_info[i];
        struct monster_lore *il_ptr = get_lore(p, r);

        /* Skip non-entries */
        if (!r->name) continue;

        /* Skip uniques */
        if (race_is_unique(r)) continue;

        /* Skip different symbol */
        if (r->d_char != race->d_char) continue;

        /* Skip current */
        if (r == race) continue;

        /* Skip stronger */
        if (r->level > race->level) continue;

        /* Affinity with that monster race */
        kills += MIN(il_ptr->pkills, MAX(1, r->level));
        count += MAX(1, r->level);
    }

    return (count? ((100 * kills) / count): 0);
}


static bool mimic_shape(struct player_shape *shapes, struct monster_race *race, int lev)
{
    struct player_shape *shape = shapes;

    while (shape)
    {
        if ((lev >= shape->lvl) && streq(race->name, shape->name)) return true;
        shape = shape->next;
    }

    return false;
}


void do_cmd_poly(struct player *p, struct monster_race *race, bool check_kills, bool domsg)
{
    const char *prefix = "";

    /* Restrict ghosts */
    if (p->ghost)
    {
        if (domsg)
            msg(p, "You need a tangible body to polymorph!");
        else
            plog("You need a tangible body to polymorph!");
        return;
    }

    /* Nothing to do */
    if (race == p->poly_race)
    {
        if (domsg)
            msg(p, "You are already using that form.");
        else
            plog("You are already using that form.");
        return;
    }

    /* Polymorph into normal form */
    if (!race)
    {
        if (domsg)
            msg(p, "You polymorph back into your normal form.");
        else
            plog("You polymorph back into your normal form.");

        /* Wraithform */
        player_clear_timed(p, TMD_WRAITHFORM, true);

        /* Invisibility */
        player_clear_timed(p, TMD_INVIS, true);

        /* Normal form */
        p->poly_race = NULL;
        p->k_idx = 0;
        if (domsg) Send_poly(p, 0);

        /* Notice */
        p->upkeep->update |= (PU_BONUS | PU_MONSTERS);

        /* Redraw */
        p->upkeep->redraw |= (PR_MAP | PR_SPELL);
        set_redraw_equip(p, NULL);

        return;
    }

    /* Skip non-entries */
    if (!race->name)
    {
        if (domsg)
            msg(p, "This monster race doesn't exist.");
        else
            plog("This monster race doesn't exist.");
        return;
    }

    /* Don't learn level 0 forms */
    if (race->level == 0)
    {
        if (domsg)
            msg(p, "You cannot learn this monster race.");
        else
            plog("You cannot learn this monster race.");
        return;
    }

    /* Must not be unique (allow it to the DM for debug purposes) */
    if (race_is_unique(race) && !is_dm_p(p))
    {
        if (domsg)
            msg(p, "This monster race is unique.");
        else
            plog("This monster race is unique.");
        return;
    }

    /* Check required kill count */
    if (check_kills)
    {
        bool learnt;

        /* Race & class shapes */
        if (p->race->shapes || p->clazz->shapes)
        {
            learnt = mimic_shape(p->race->shapes, race, p->lev) ||
                mimic_shape(p->clazz->shapes, race, p->lev);
        }

        /* Regular Shapechanger */
        else
        {
            struct monster_lore *lore = get_lore(p, race);
            int rkills = 1;

            /* Perfect affinity lowers the requirement to 25% of the required kills */
            if (lore->pkills) rkills = ((race->level * (400 - 3 * affinity(p, race))) / 400);

            learnt = (lore->pkills >= rkills);
        }

        if (!learnt)
        {
            if (domsg)
                msg(p, "You have not learned that form yet.");
            else
                plog("You have not learned that form yet.");
            return;
        }
    }

    /* Polymorph into that monster */
    if (!race_is_unique(race))
        prefix = (is_a_vowel(tolower(race->name[0]))? "an ": "a ");
    if (domsg)
        msg(p, "You polymorph into %s%s.", prefix, race->name);
    else
        plog_fmt("You polymorph into %s%s.", prefix, race->name);

    /* Wraithform */
    if (rf_has(race->flags, RF_PASS_WALL))
    {
        p->timed[TMD_WRAITHFORM] = -1;
        p->upkeep->redraw |= PR_STATUS;
        handle_stuff(p);
    }
    else
        player_clear_timed(p, TMD_WRAITHFORM, true);

    /* Invisibility */
    if (race_is_invisible(race))
    {
        p->timed[TMD_INVIS] = -1;
        p->upkeep->update |= PU_MONSTERS;
        p->upkeep->redraw |= PR_STATUS;
        handle_stuff(p);
    }
    else
        player_clear_timed(p, TMD_INVIS, true);

    /* New form */
    p->poly_race = race;
    if (domsg) Send_poly(p, race->ridx);

    /* Unaware players */
    p->k_idx = 0;
    if (rf_has(race->flags, RF_UNAWARE)) p->k_idx = -1;

    // sound for bats ;)
    if (race->base == lookup_monster_base("bat"))
        sound(p, MSG_POLY_BAT);

    /* Hack -- random mimics */
    if (race->base == lookup_monster_base("random mimic"))
    {
        /* Random symbol from object set */
        while (1)
        {
            /* Select a random object */
            p->k_idx = randint0(z_info->k_max - 1) + 1;

            /* Skip non-entries */
            if (!k_info[p->k_idx].name) continue;

            /* Skip empty entries */
            if (!k_info[p->k_idx].d_attr || !k_info[p->k_idx].d_char)
                continue;

            /* Force race attr */
            if (k_info[p->k_idx].d_attr != race->d_attr) continue;

            /* Success */
            break;
        }
    }

    /* Hack -- object mimics */
    else if (race->mimic_kinds)
    {
        struct monster_mimic *mimic_kind;
        int i = 1;

        /* Pick a random object kind to mimic */
        for (mimic_kind = race->mimic_kinds; mimic_kind; mimic_kind = mimic_kind->next, i++)
        {
            if (one_in_(i)) p->k_idx = mimic_kind->kind->kidx;
        }
    }

    /* Notice */
    p->upkeep->update |= (PU_BONUS | PU_MONSTERS);

    /* Redraw */
    p->upkeep->redraw |= (PR_MAP | PR_SPELL);
    set_redraw_equip(p, NULL);
}


/*
 * Show every monster race with kill count/required kill count for polymorphing
 */
void do_cmd_check_poly(struct player *p, int line)
{
    char file_name[MSG_LEN];
    ang_file *fff;
    int k, total = 0;
    struct monster_race *race;
    struct monster_lore *lore;
    int aff, rkills;

    /* Temporary file */
    fff = file_temp(file_name, sizeof(file_name));
    if (!fff) return;

    /* Scan the monster races (backwards for easiness of use) */
    for (k = z_info->r_max - 1; k > 0; k--)
    {
        bool ok;

        race = &r_info[k];

        /* Skip non-entries */
        if (!race->name) continue;

        /* Skip level 0 forms */
        if (race->level == 0) continue;

        /* Only print non uniques */
        if (race_is_unique(race)) continue;

        /* Check if the input is a symbol */
        if (strlen(p->tempbuf) == 1)
        {
            if (p->tempbuf[0] == '*')
                ok = true;
            else
                ok = (race->d_char == p->tempbuf[0]);
        }

        /* The input is a partial monster name */
        else
        {
            char monster[NORMAL_WID];

            /* Clean up monster name */
            clean_name(monster, race->name);

            /* Check if cleaned name matches our search string */
            ok = (strstr(monster, p->tempbuf) != NULL);
        }

        if (!ok) continue;

        /* Race & class shapes */
        if (p->race->shapes || p->clazz->shapes)
        {
            if (mimic_shape(p->race->shapes, race, p->lev) ||
                mimic_shape(p->clazz->shapes, race, p->lev))
            {
                file_putf(fff, "G[%d] %s (learnt)\n", k, race->name);
                total++;
            }
        }

        /* Regular Shapechanger */
        else
        {
            lore = get_lore(p, race);

            /* Only display "known" races */
            if (!lore->pkills) continue;

            /* Perfect affinity lowers the requirement to 25% of the required kills */
            aff = affinity(p, race);
            rkills = ((race->level * (400 - 3 * aff)) / 400);

            /* Check required kill count */
            if (lore->pkills >= rkills)
                file_putf(fff, "G[%d] %s: %d (learnt)\n", k, race->name, lore->pkills);
            else
            {
                char color = 'w';

                if (rkills > 0)
                {
                    int perc = 100 * lore->pkills / rkills;

                    if (perc >= 75) color = 'y';
                    else if (perc >= 50) color = 'o';
                }

                file_putf(fff, "%c[%d] %s: %d (%d more to go, affinity = %d%%)\n", color, k,
                    race->name, lore->pkills, rkills - lore->pkills, aff);
            }

            total++;
        }
    }

    if (!total) file_put(fff, "wNothing so far.\n");

    /* Close the file */
    file_close(fff);

    /* Display the file contents */
    show_file(p, file_name, (player_has(p, PF_MONSTER_SPELLS)? "Killed List": "Forms"), line, 1);

    /* Remove the file */
    file_delete(file_name);
}   


/*
 * Show socials
 */
void do_cmd_check_socials(struct player *p, int line)
{
    char file_name[MSG_LEN];
    ang_file *fff;
    int k;
    struct social *s;

    /* Temporary file */
    fff = file_temp(file_name, sizeof(file_name));
    if (!fff) return;

    /* Scan the socials */
    for (k = 0; k < z_info->soc_max; k++)
    {
        s = &soc_info[k];

        /* Print the socials */
        file_putf(fff, "w%s\n", s->name);
    }

    /* Close the file */
    file_close(fff);

    /* Display the file contents */
    show_file(p, file_name, "Socials", line, 1);

    /* Remove the file */
    file_delete(file_name);
}


void do_cmd_interactive(struct player *p, int type, uint32_t query)
{
    /* Hack -- use special term */
    Send_term_info(p, NTERM_ACTIVATE, NTERM_WIN_SPECIAL);

    /* Let the player scroll through this info */
    p->special_file_type = type;

    /* Perform action */
    switch (type)
    {
        /* Help file */
        case SPECIAL_FILE_HELP:
            common_file_peruse(p, query);
            do_cmd_check_other(p, p->interactive_line - p->interactive_next);
            break;
    }

    /* Hack -- return to main term */
    Send_term_info(p, NTERM_ACTIVATE, NTERM_WIN_OVERHEAD);
}
