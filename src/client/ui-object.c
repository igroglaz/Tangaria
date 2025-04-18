/*
 * File: ui-object.c
 * Purpose: Object lists and selection, and other object-related UI functions
 *
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 * Copyright (c) 2007-9 Andi Sidwell, Chris Carr, Ed Graham, Erik Osheim
 * Copyright (c) 2015 Nick McConnell
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


#include "c-angband.h"


/* Floor items */
struct object **floor_items;
uint8_t floor_num;


/*
 * Variables for object display and selection
 */
#define MAX_ITEMS 50


/*
 * Info about a particular object
 */
struct object_menu_data
{
    char label[NORMAL_WID];
    char equip_label[NORMAL_WID];
    struct object *object;
    char o_name[NORMAL_WID];
    char key;
};


static struct object_menu_data items[MAX_ITEMS];
static int num_obj;
static int num_head;
static size_t max_len;
static int ex_width;
static int ex_offset;


/*
 * Display of individual objects in lists or for selection
 */


/*
 * Get the spellbook structure from an object which is a book the player can
 * cast from
 */
static const struct class_book *player_object_to_book(const struct object *obj)
{
    int i;

    for (i = 0; i < player->clazz->magic.num_books; i++)
    {
        if ((obj->tval == player->clazz->magic.books[i].tval) &&
            (obj->sval == player->clazz->magic.books[i].sval))
        {
            return &player->clazz->magic.books[i];
        }
    }

    return NULL;
}


/*
 * Display an object. Each object may be prefixed with a label.
 * Used by show_inven(), show_equip(), show_quiver() and show_floor().
 * Mode flags are documented in object.h
 */
static void show_obj(int obj_num, int row, int col, bool cursor, int mode)
{
    int attr;
    int label_attr = (cursor? COLOUR_L_BLUE: COLOUR_WHITE);
    int ex_offset_ctr;
    char buf[NORMAL_WID];
    struct object *obj = items[obj_num].object;
    bool show_label = ((mode & OLIST_WINDOW)? true: false);
    int label_size = (show_label? strlen(items[obj_num].label): 0);
    int equip_label_size = strlen(items[obj_num].equip_label);

    /* Clear the line */
    prt("", row + obj_num, MAX(col - 1, 0));

    /* If we have no label then we won't display anything */
    if (!strlen(items[obj_num].label)) return;

    /* Print the label */
    if (show_label)
        c_put_str(label_attr, items[obj_num].label, row + obj_num, col);

    /* Print the equipment label */
    c_put_str(label_attr, items[obj_num].equip_label, row + obj_num, col + label_size);

    /* Limit object name */
    if (label_size + equip_label_size + strlen(items[obj_num].o_name) > (size_t)ex_offset)
    {
        int truncate = ex_offset - label_size - equip_label_size;

        if (truncate < 0) truncate = 0;
        if ((size_t)truncate > sizeof(items[obj_num].o_name) - 1)
            truncate = sizeof(items[obj_num].o_name) - 1;

        items[obj_num].o_name[truncate] = '\0';
    }

    /* Item kind determines the color of the output */
    if (obj)
    {
        attr = obj->info_xtra.attr;

        /* Unreadable books are a special case */
        if (tval_is_book(obj) && (player_object_to_book(obj) == NULL))
            attr = COLOUR_SLATE;
    }
    else
        attr = COLOUR_SLATE;

    /* Object name */
    c_put_str(attr, items[obj_num].o_name, row + obj_num, col + label_size + equip_label_size);

    /* If we don't have an object, we can skip the rest of the output */
    if (!obj) return;

    /* Extra fields */
    ex_offset_ctr = ex_offset;

    /* Price */
    if ((mode & OLIST_PRICE) && obj->askprice)
    {
        int32_t price = obj->askprice;

        strnfmt(buf, sizeof(buf), "%6d au", price);
        put_str(buf, row + obj_num, col + ex_offset_ctr);
        ex_offset_ctr += 9;
    }

    /* Failure chance for magic devices and activations */
    if (mode & OLIST_FAIL)
    {
        uint8_t fail = obj->info_xtra.fail;

        if (fail != 255)
        {
            if (fail <= 100)
                strnfmt(buf, sizeof(buf), "%4d%% fail", fail);
            else
                my_strcpy(buf, "    ? fail", sizeof(buf));
            put_str(buf, row + obj_num, col + ex_offset_ctr);
            ex_offset_ctr += 10;
        }
    }

    /* Failure chances for recharging an item */
    if (mode & OLIST_RECHARGE)
    {
        int fail = 1000 / recharge_failure_chance(obj, player->upkeep->recharge_pow);

        if (obj->info_xtra.known_effect)
            strnfmt(buf, sizeof(buf), "%2d.%1d%% fail", fail / 10, fail % 10);
        else
            my_strcpy(buf, "    ? fail", sizeof(buf));
        put_str(buf, row + obj_num, col + ex_offset_ctr);
        ex_offset_ctr += 10;
    }

    /* Weight */
    if ((mode & OLIST_WEIGHT) && obj->weight)
    {
        int weight = obj->weight;

        strnfmt(buf, sizeof(buf), "%4d.%1d lb", weight / 10, weight % 10);
        put_str(buf, row + obj_num, col + ex_offset_ctr);
    }
}


/*
 * Display of lists of objects
 */


/*
 * Clear the object list.
 */
static void wipe_obj_list(void)
{
    /* Zero the constants */
    num_obj = 0;
    num_head = 0;
    max_len = 0;
    ex_width = 0;
    ex_offset = 0;

    /* Clear the existing contents */
    memset(items, 0, MAX_ITEMS * sizeof(struct object_menu_data));
}


/*
 * Build the object list.
 */
static void build_obj_list(int last, struct object **list, item_tester tester, int mode)
{
    bool gold_ok = ((mode & OLIST_GOLD)? true: false);
    bool in_term = ((mode & OLIST_WINDOW)? true: false);
    bool show_empty = ((mode & OLIST_SEMPTY)? true: false);
    bool equip = (list? false: true);
    bool quiver = ((list == player->upkeep->quiver)? true: false);
    bool book_tags = ((mode & OLIST_BOOK_TAGS)? true: false);
    int i;

    /* Build the object list */
    for (i = 0; i <= last; i++)
    {
        struct object *obj = (equip? slot_object(player, i): list[i]);
        struct object_menu_data *entry = &items[num_obj];

        /* Acceptable items get a label */
        if (object_test(player, tester, obj) || (obj && tval_is_money(obj) && gold_ok))
        {
            if (quiver) entry->key = I2D(i);
            else if (book_tags) entry->key = I2D(obj->sval);
            else entry->key = all_letters_nohjkl[i];
        }

        /* Unacceptable items are still sometimes shown */
        else if ((!obj && show_empty) || in_term)
            entry->key = ' ';

        /* Unacceptable items are skipped in the main window */
        else continue;

        /* Save the object */
        entry->object = obj;

        /* Format the label */
        if (entry->key != ' ') strnfmt(entry->label, sizeof(entry->label), "%c) ", entry->key);
        else my_strcpy(entry->label, "   ", sizeof(entry->label));

        /* Format equipment slot labels (or quiver in subwindow) */
        if (equip)
        {
            const char *mention = equip_mention(player, i);
            size_t u8len = utf8_strlen(mention);

            if (u8len < 14)
            {
                strnfmt(entry->equip_label, sizeof(entry->equip_label), "%s%*s", mention,
                    (int)(14 - u8len), " ");
            }
            else
            {
                char *mention_copy = string_make(mention);

                if (u8len > 14) utf8_clipto(mention_copy, 14);
                strnfmt(entry->equip_label, sizeof(entry->equip_label), "%s", mention_copy);
                string_free(mention_copy);
            }

            my_strcap(entry->equip_label);
        }
        else if (in_term && quiver)
        {
            strnfmt(entry->equip_label, sizeof(entry->equip_label), "%-14s: ",
                format("In quiver [f%d]", i));
        }
        else
            entry->equip_label[0] = 0;

        num_obj++;
    }
}


/*
 * Set object names and get their maximum length.
 * Only makes sense after building the object list.
 */
static void set_obj_names(bool terse, int mode)
{
    int i;
    struct object *obj;
    bool in_term = ((mode & OLIST_WINDOW)? true: false);

    /* Calculate name offset and max name length */
    for (i = 0; i < num_obj; i++)
    {
        obj = items[i].object;

        /* Null objects are used to skip lines, or display only a label */
        if (!obj)
        {
            if (i < num_head)
                strnfmt(items[i].o_name, sizeof(items[i].o_name), "%s", "");
            else
                strnfmt(items[i].o_name, sizeof(items[i].o_name), "%s", "(nothing)");
        }
        else if (terse)
            my_strcpy(items[i].o_name, obj->info_xtra.name_terse, sizeof(items[0].o_name));
        else
            my_strcpy(items[i].o_name, obj->info_xtra.name, sizeof(items[0].o_name));

        /* Max length of label + object name */
        max_len = MAX(max_len, strlen(items[i].label) + strlen(items[i].equip_label) +
            strlen(items[i].o_name));
    }

    if (mode & OLIST_QUIVER)
    {
        int count, j;
        char tmp_val[NORMAL_WID];
        int quiver_slots = (player->upkeep->quiver_cnt + z_info->quiver_slot_size - 1) /
            z_info->quiver_slot_size;

        for (j = 0; j < quiver_slots; j++, i++)
        {
            if (j == quiver_slots - 1)
                count = player->upkeep->quiver_cnt - z_info->quiver_slot_size * (quiver_slots - 1);
            else
                count = z_info->quiver_slot_size;

            strnfmt(tmp_val, sizeof(tmp_val), "%c) in Quiver: %d missile%s",
                all_letters_nohjkl[in_term? i - 1: i], count, PLURAL(count));

            /* Max length of label + object name */
            max_len = MAX(max_len, strlen(tmp_val));
        }
    }
}


/*
 * Display a list of objects. Each object may be prefixed with a label.
 * Used by show_inven(), show_equip(), show_quiver() and show_floor().
 */
static void show_obj_list(int mode)
{
    int i, row = 0, col = 0;
    char tmp_val[NORMAL_WID];
    bool in_term = (mode & OLIST_WINDOW)? true: false;
    bool terse = false;

    /* Initialize */
    max_len = 0;
    ex_width = 0;
    ex_offset = 0;

    if (in_term) max_len = 40;
    if (in_term && (Term->wid < 50)) mode &= ~(OLIST_WEIGHT);

    if (Term->wid < 60) terse = true;

    /* Set the names and get the max length */
    set_obj_names(terse, mode);

    /* Take the quiver message into consideration */
    if ((mode & OLIST_QUIVER) && (player->upkeep->quiver[0] != NULL))
        max_len = MAX(max_len, NORMAL_HGT);

    /* Width of extra fields */
    if (mode & OLIST_WEIGHT) ex_width += 9;
    if (mode & OLIST_PRICE) ex_width += 9;
    if (mode & OLIST_FAIL) ex_width += 10;

    /* Determine beginning row and column */
    if (in_term)
    {
        /* Term window */
        row = 0;
        col = 0;
    }
    else
    {
        /* Main window */
        row = 1;
        col = Term->wid - 1 - max_len - ex_width;

        if (col < 3) col = 0;

        /* Hack -- full icky screen */
        if (full_icky_screen && (col > COL_MAP + 2)) col = COL_MAP + 2;
    }

    /* Column offset of the first extra field */
    ex_offset = MIN(max_len, (size_t)(Term->wid - 1 - ex_width - col));

    /* Output the list */
    for (i = 0; i < num_obj; i++)
        show_obj(i, row, col, false, mode);

    /* For the inventory: print the quiver count */
    if (mode & OLIST_QUIVER)
    {
        int count, j;
        int quiver_slots = (player->upkeep->quiver_cnt + z_info->quiver_slot_size - 1) /
            z_info->quiver_slot_size;

        /* Quiver may take multiple lines */
        for (j = 0; j < quiver_slots; j++, i++)
        {
            const char *fmt = "in Quiver: %d missile%s";
            char letter = all_letters_nohjkl[in_term? i - 1: i];

            /* Number of missiles in this "slot" */
            if (j == quiver_slots - 1)
                count = player->upkeep->quiver_cnt - z_info->quiver_slot_size * (quiver_slots - 1);
            else
                count = z_info->quiver_slot_size;

            /* Clear the line */
            prt("", row + i, MAX(col - 2, 0));

            /* Print the (disabled) label */
            strnfmt(tmp_val, sizeof(tmp_val), "%c) ", letter);
            c_put_str(COLOUR_SLATE, tmp_val, row + i, col);

            /* Print the count */
            strnfmt(tmp_val, sizeof(tmp_val), fmt, count, PLURAL(count));
            c_put_str(COLOUR_L_UMBER, tmp_val, row + i, col + 3);
        }
    }

    /* Clear term windows */
    if (in_term)
    {
        for (; i < Term->hgt; i++) prt("", row + i, MAX(col - 2, 0));
    }

    /* Hack -- full icky screen */
    else if (full_icky_screen)
    {
        for (; (i > 0 && row + i < NORMAL_HGT); i++) prt("", row + i, MAX(col - 2, 0));
    }

    /* Print a drop shadow for the main window if necessary */
    else if (i > 0 && row + i < NORMAL_HGT)
        prt("", row + i, MAX(col - 2, 0));
}


/*
 * Display the inventory. Builds a list of objects and passes them
 * off to show_obj_list() for display.
 */
void show_inven(int mode, item_tester tester)
{
    int i, last_slot = -1;
    int diff = get_diff(player);
    bool in_term = ((mode & OLIST_WINDOW)? true: false);

    /* Initialize */
    wipe_obj_list();

    /* Include burden for term windows */
    if (in_term)
    {
        strnfmt(items[num_obj].label, sizeof(items[num_obj].label), "Burden %d.%d lb (%d.%d lb %s) ",
            player->upkeep->total_weight / 10, player->upkeep->total_weight % 10,
            abs(diff) / 10, abs(diff) % 10, ((diff < 0)? "overweight": "remaining"));

        items[num_obj].object = NULL;
        num_obj++;
    }

    /* Find the last occupied inventory slot */
    for (i = 0; i < z_info->pack_size; i++)
    {
        if (player->upkeep->inven[i] != NULL) last_slot = i;
    }

    /* Build the object list */
    build_obj_list(last_slot, player->upkeep->inven, tester, mode);

    /* Term window starts with a burden header */
    num_head = (in_term? 1: 0);

    /* Display the object list */
    show_obj_list(mode);
}


/*
 * Display the quiver. Builds a list of objects and passes them
 * off to show_obj_list() for display.
 */
void show_quiver(int mode, item_tester tester)
{
    int i, last_slot = -1;

    /* Initialize */
    wipe_obj_list();

    /* Find the last occupied quiver slot */
    for (i = 0; i < z_info->quiver_size; i++)
    {
        if (player->upkeep->quiver[i] != NULL) last_slot = i;
    }

    /* Build the object list */
    build_obj_list(last_slot, player->upkeep->quiver, tester, mode);

    /* Display the object list */
    num_head = 0;
    show_obj_list(mode);
}


/*
 * Display the equipment. Builds a list of objects and passes them
 * off to show_obj_list() for display.
 */
void show_equip(int mode, item_tester tester)
{
    int i;
    bool in_term = ((mode & OLIST_WINDOW)? true: false);

    /* Initialize */
    wipe_obj_list();

    /* Build the object list */
    build_obj_list(player->body.count - 1, NULL, tester, mode);

    /* Show the quiver in subwindows */
    if (in_term)
    {
        int last_slot = -1;

        /* Add a spacer between equipment and quiver */
        my_strcpy(items[num_obj].label, "", sizeof(items[num_obj].label));
        items[num_obj].object = NULL;
        num_obj++;

        /* Find the last occupied quiver slot */
        for (i = 0; i < z_info->quiver_size; i++)
        {
            if (player->upkeep->quiver[i] != NULL) last_slot = i;
        }

        /* Extend the object list */
        build_obj_list(last_slot, player->upkeep->quiver, tester, mode);
    }

    /* Hack -- display the first floor item */
    if ((mode & OLIST_FLOOR) && floor_items[0])
    {
        /* Add a spacer between equipment and floor item */
        my_strcpy(items[num_obj].label, "", sizeof(items[num_obj].label));
        items[num_obj].object = NULL;
        num_obj++;

        /* Save the object */
        my_strcpy(items[num_obj].label, "-) ", sizeof(items[num_obj].label));
        my_strcpy(items[num_obj].equip_label, "On the floor  : ", sizeof(items[num_obj].equip_label));
        items[num_obj].object = floor_items[0];
        num_obj++;
    }

    /* Display the object list */
    num_head = 0;
    show_obj_list(mode);
}


/*
 * Display the floor. Builds a list of objects and passes them
 * off to show_obj_list() for display.
 */
void show_floor(int mode, item_tester tester)
{
    /* Initialize */
    wipe_obj_list();

    if (floor_num > z_info->floor_size) floor_num = z_info->floor_size;

    /* Build the object list */
    build_obj_list(floor_num - 1, floor_items, tester, mode);

    /* Display the object list */
    num_head = 0;
    show_obj_list(mode);
}


/*
 * Variables for object selection
 */


static struct object *selection;
static char header[NORMAL_WID];
static int i1, i2;
static int e1, e2;
static int q1, q2;
static int f1, f2;
static int throwing_num;
static int olist_mode;
static cmd_code item_cmd;
static bool newmenu = false;
static int16_t command_wrk;
static bool hidden;


/*
 * Object selection utilities
 */


/*
 * Find the first object in the object list with the given "tag". The object
 * list needs to be built before this function is called.
 *
 * A "tag" is a char "n" appearing as "@n" anywhere in the
 * inscription of an object.
 *
 * Also, the tag "@xn" will work as well, where "n" is a tag-char,
 * and "x" is the action that tag will work for.
 */
static bool get_tag(struct object **tagged_obj, char tag, cmd_code cmd, bool quiver_tags)
{
    int i;
    int mode = (OPT(player, rogue_like_commands)? KEYMAP_MODE_ROGUE: KEYMAP_MODE_ORIG);

    /* (f)ire is handled differently from all others, due to the quiver */
    if (quiver_tags)
    {
        i = tag - '0';
        if (player->upkeep->quiver[i])
        {
            *tagged_obj = player->upkeep->quiver[i];
            return true;
        }
    }

    /* Check every object in the object list */
    for (i = 0; i < num_obj; i++)
    {
        const char *s;
        struct object *obj = items[i].object;
        char *buf, *buf2;

        /* Skip non-objects */
        if (!obj) continue;

        buf = obj->info_xtra.name;

        /* Skip empty objects */
        if (!buf[0]) continue;

        /* Skip empty inscriptions */
        buf2 = strchr(buf, '{');
        if (!buf2) continue;

        /* Find a '@' */
        s = strchr(buf2, '@');

        /* Process all tags */
        while (s)
        {
            unsigned char cmdkey;

            /* Check the normal tags */
            if (s[1] == tag)
            {
                /* Save the actual object */
                *tagged_obj = obj;

                /* Success */
                return true;
            }

            cmdkey = cmd_lookup_key_unktrl(cmd, mode);

            /* Check the special tags */
            if ((s[1] == cmdkey) && (s[2] == tag))
            {
                /* Save the actual object */
                *tagged_obj = obj;

                /* Success */
                return true;
            }

            /* Find another '@' */
            s = strchr(s + 1, '@');
        }
    }

    /* No such tag */
    return false;
}


/*
 * Prompt player for a string, then try to find an item matching it
 *
 * Returns "0" if an item was found, "1" if user has aborted input, and
 * "-1" if nothing could be found.
 */
static errr get_item_by_name(int *k)
{
    char buf[256];
    char *tok;
    int i;
    size_t len;
    char *prompt = "Item name: ";

    /* Hack -- spellcasting mode (select book by spell) */
    if (spellcasting)
    {
        int sn = -1;
        errr failed = get_spell_by_name(k, &sn);

        /* Remember spell index */
        spellcasting_spell = sn;

        /* Don't do any other tests */
        return failed;
    }

    /* Hack -- show opening quote symbol */
    if (prompt_quote_hack) prompt = "Item name: \"";

    buf[0] = '\0';
    if (!get_string(prompt, buf, NORMAL_WID)) return 1;

    /* Hack -- remove final quote */
    len = strlen(buf);
    if (len == 0) return 1;
    if (buf[len - 1] == '"') buf[len - 1] = '\0';

    /* Split entry */
    tok = strtok(buf, "|");
    while (tok)
    {
        if (STRZERO(tok)) continue;

        /* Match against valid items */
        for (i = 0; i < num_obj; i++)
        {
            if (!items[i].object) continue;

            if (my_stristr(items[i].o_name, tok))
            {
                (*k) = i;
                return 0;
            }
        }
        tok = strtok(NULL, "|");
    }

    return -1;
}


/*
 * Object selection menu
 */


/*
 * Make the correct header for the selection menu
 */
static void menu_header(void)
{
    char tmp_val[75];
    char out_val[75];

    /* Viewing inventory */
    if (command_wrk == USE_INVEN)
    {
        /* Begin the header */
        strnfmt(out_val, sizeof(out_val), "%s", "Inven:");

        /* List choices */
        if (i1 <= i2)
        {
            /* Build the header */
            strnfmt(tmp_val, sizeof(tmp_val), " %c-%c,", all_letters_nohjkl[i1],
                all_letters_nohjkl[i2]);

            /* Append */
            my_strcat(out_val, tmp_val, sizeof(out_val));
        }

        /* Indicate legality of equipment */
        if (e1 <= e2)
            my_strcat(out_val, " / for Equip,", sizeof(out_val));

        /* Indicate legality of quiver */
        if (q1 <= q2)
            my_strcat(out_val, " | for Quiver,", sizeof(out_val));

        /* Indicate legality of the "floor" */
        if (f1 <= f2)
            my_strcat(out_val, " - for floor,", sizeof(out_val));
    }

    /* Viewing equipment */
    else if (command_wrk == USE_EQUIP)
    {
        /* Begin the header */
        strnfmt(out_val, sizeof(out_val), "%s", "Equip:");

        /* List choices */
        if (e1 <= e2)
        {
            /* Build the header */
            strnfmt(tmp_val, sizeof(tmp_val), " %c-%c,", all_letters_nohjkl[e1],
                all_letters_nohjkl[e2]);

            /* Append */
            my_strcat(out_val, tmp_val, sizeof(out_val));
        }

        /* Indicate legality of inventory */
        if (i1 <= i2)
            my_strcat(out_val, " / for Inven,", sizeof(out_val));

        /* Indicate legality of quiver */
        if (q1 <= q2)
            my_strcat(out_val, " | for Quiver,", sizeof(out_val));

        /* Indicate legality of the "floor" */
        if (f1 <= f2)
            my_strcat(out_val, " - for floor,", sizeof(out_val));
    }

    /* Viewing quiver */
    else if (command_wrk == USE_QUIVER)
    {
        /* Begin the header */
        strnfmt(out_val, sizeof(out_val), "%s", "Quiver:");

        /* List choices */
        if (q1 <= q2)
        {
            /* Build the header */
            strnfmt(tmp_val, sizeof(tmp_val), " %d-%d,", q1, q2);

            /* Append */
            my_strcat(out_val, tmp_val, sizeof(out_val));
        }

        /* Indicate legality of inventory */
        if (i1 <= i2)
            my_strcat(out_val, " / for Inven,", sizeof(out_val));

        /* Indicate legality of equipment */
        else if (e1 <= e2)
            my_strcat(out_val, " / for Equip,", sizeof(out_val));

        /* Indicate legality of the "floor" */
        if (f1 <= f2)
            my_strcat(out_val, " - for floor,", sizeof(out_val));
    }

    /* Viewing throwing */
    else if (command_wrk == SHOW_THROWING)
    {
        /* Begin the header */
        strnfmt(out_val, sizeof(out_val), "%s", "Throwing items:");

        /* List choices */
        if (throwing_num)
        {
            /* Build the header */
            strnfmt(tmp_val, sizeof(tmp_val), " a-%c,", all_letters_nohjkl[throwing_num - 1]);

            /* Append */
            my_strcat(out_val, tmp_val, sizeof(out_val));
        }

        /* Indicate legality of inventory */
        if (i1 <= i2)
            my_strcat(out_val, " / for Inven,", sizeof(out_val));

        /* Indicate legality of quiver */
        if (q1 <= q2)
            my_strcat(out_val, " | for Quiver,", sizeof(out_val));

        /* Indicate legality of the "floor" */
        if (f1 <= f2)
            my_strcat(out_val, " - for floor,", sizeof(out_val));
    }

    /* Viewing floor */
    else
    {
        /* Begin the header */
        strnfmt(out_val, sizeof(out_val), "%s", "Floor:");

        /* List choices */
        if (f1 <= f2)
        {
            /* Build the header */
            strnfmt(tmp_val, sizeof(tmp_val), " %c-%c,", all_letters_nohjkl[f1],
                all_letters_nohjkl[f2]);

            /* Append */
            my_strcat(out_val, tmp_val, sizeof(out_val));
        }

        /* Indicate legality of inventory */
        if (i1 <= i2)
            my_strcat(out_val, " / for Inven,", sizeof(out_val));

        /* Indicate legality of equipment */
        else if (e1 <= e2)
            my_strcat(out_val, " / for Equip,", sizeof(out_val));

        /* Indicate legality of quiver */
        if (q1 <= q2)
            my_strcat(out_val, " | for Quiver,", sizeof(out_val));
    }

    /* Finish the header */
    my_strcat(out_val, " ESC", sizeof(out_val));

    /* Build the header */
    strnfmt(header, sizeof(header), "(%s)", out_val);
}


/*
 * Get an item tag
 */
static char get_item_tag(struct menu *menu, int oid)
{
    struct object_menu_data *choice = menu_priv(menu);

    return choice[oid].key;
}


/*
 * Determine if an item is a valid choice
 */
static int get_item_validity(struct menu *menu, int oid)
{
    struct object_menu_data *choice = menu_priv(menu);

    return ((choice[oid].object != NULL)? 1: 0);
}


/*
 * Display an entry on the item menu
 */
static void get_item_display(struct menu *menu, int oid, bool cursor, int row, int col, int width)
{
    show_obj(oid, row - oid, col, cursor, olist_mode);
}


/*
 * Deal with events on the get_item menu
 */
static bool get_item_action(struct menu *menu, const ui_event *event, int oid)
{
    struct object_menu_data *choice = menu_priv(menu);
    char key = event->key.code;

    if (event->type == EVT_SELECT)
        selection = choice[oid].object;

    if (event->type == EVT_KBRD)
    {
        switch (key)
        {
            case '/':
            {
                /* Toggle if allowed */
                if ((i1 <= i2) && (command_wrk != USE_INVEN))
                {
                    command_wrk = USE_INVEN;
                    newmenu = true;
                }
                else if ((e1 <= e2) && (command_wrk != USE_EQUIP))
                {
                    command_wrk = USE_EQUIP;
                    newmenu = true;
                }
                else
                {
                    bell("Cannot switch item selector!");

                    /* Macros are supposed to be accurate */
                    if (hidden) return true;
                }

                break;
            }

            case '|':
            {
                /* No toggle allowed */
                if (q1 > q2)
                {
                    bell("Cannot select quiver!");

                    /* Macros are supposed to be accurate */
                    if (hidden) return true;
                }
                else
                {
                    /* Toggle to quiver */
                    command_wrk = USE_QUIVER;
                    newmenu = true;
                }

                break;
            }

            case '-':
            {
                /* No toggle allowed */
                if (f1 > f2)
                {
                    bell("Cannot select floor!");

                    /* Macros are supposed to be accurate */
                    if (hidden) return true;
                }
                else
                {
                    /* Toggle to floor */
                    command_wrk = USE_FLOOR;
                    newmenu = true;
                }

                break;
            }

            case '"':
            {
                /* Allow '"' to be used as terminator */
                prompt_quote_hack = true;

                /* fallthrough */
            }

            case '@':
            {
                int k;
                errr r;

                /* Lookup item by name */
                r = get_item_by_name(&k);
                if (!r)
                {
                    /* Hack -- spellcasting mode (select book by spell) */
                    if (spellcasting)
                    {
                        int i;
                        struct object *menu_obj;

                        /* Find the book in the list of choices */
                        struct class_book *book = &player->clazz->magic.books[k];

                        for (i = 0; i < menu->count; i++)
                        {
                            menu_obj = choice[i].object;
                            if ((menu_obj->tval == book->tval) && (menu_obj->sval == book->sval))
                            {
                                /* Found it */
                                selection = menu_obj;
                                break;
                            }
                        }
                    }
                    else
                        selection = choice[k].object;

                    return true;
                }
                else if (r < 0)
                {
                    bell("Cannot select item!");

                    /* Macros are supposed to be accurate */
                    if (hidden) return true;
                }

                break;
            }
        }
    }

    return false;
}


/* Hack -- last row of text on the screen */
static int last_row;


/*
 * Show quiver missiles in full inventory
 */
static void item_menu_browser(int oid, void *data, const region *area)
{
    char tmp_val[NORMAL_WID];
    int count, j, i = num_obj;
    int x, y;
    int quiver_slots = (player->upkeep->quiver_cnt + z_info->quiver_slot_size - 1) /
        z_info->quiver_slot_size;

    /* Set up to output below the menu */
    prt("", area->row + area->page_rows, MAX(0, area->col - 1));
    Term_gotoxy(area->col, area->row + area->page_rows);

    /* If we're printing pack slots the quiver takes up */
    if ((olist_mode & OLIST_QUIVER) && (command_wrk == USE_INVEN))
    {
        /* Quiver may take multiple lines */
        for (j = 0; j < quiver_slots; j++, i++)
        {
            const char *fmt = "in Quiver: %d missile%s\n";
            char letter = all_letters_nohjkl[i];

            /* Number of missiles in this "slot" */
            if (j == quiver_slots - 1)
                count = player->upkeep->quiver_cnt - z_info->quiver_slot_size * (quiver_slots - 1);
            else
                count = z_info->quiver_slot_size;

            /* Print the (disabled) label */
            strnfmt(tmp_val, sizeof(tmp_val), "%c) ", letter);
            Term_gotoxy(area->col, area->row + i);
            text_out_to_screen(COLOUR_SLATE, tmp_val);

            /* Print the count */
            strnfmt(tmp_val, sizeof(tmp_val), fmt, count, PLURAL(count));
            Term_gotoxy(area->col + 3, area->row + i);
            text_out_to_screen(COLOUR_L_UMBER, tmp_val);
        }
    }

    /* Always print a blank line */
    prt("", area->row + i, MAX(0, area->col - 1));

    Term_locate(&x, &y);

    /* Hack -- always finish at the end of a tile in bigtile mode */
    if (tile_height > 1)
    {
        int ymax = ((y - ROW_MAP) / tile_height) * tile_height + ROW_MAP + tile_height - 1;

        while (++y <= ymax) Term_erase(x - 1, y, 255);
    }

    /* Hack -- if we use a distorted display, don't refresh the last rows */
    if (Term->max_hgt != Term->hgt)
    {
        if (--y > last_row)
            last_row = y;
        else if (y < last_row)
        {
            while (++y <= last_row) Term_erase(x - 1, y, 255);
        }
    }
}


/*
 * Display list items to choose from
 */
static struct object *item_menu(cmd_code cmd, int prompt_size, int mode)
{
    menu_iter menu_f = {get_item_tag, get_item_validity, get_item_display, get_item_action, 0};
    struct menu *m = menu_new(MN_SKIN_OBJECT, &menu_f);
    ui_event evt;
    int ex_offset_ctr = 0;
    int row, inscrip;
    struct object *obj = NULL;
    region area;

    /* Set up the menu */
    menu_setpriv(m, num_obj, items);
    if (command_wrk == USE_QUIVER)
        m->selections = "01234567";
    else
        m->selections = all_letters_nohjkl;
    m->switch_keys = "/|-@\"";
    m->flags = (MN_PVT_TAGS | MN_INSCRIP_TAGS);
    m->browse_hook = item_menu_browser;

    /* Get inscriptions */
    m->inscriptions = mem_zalloc(10 * sizeof(char));
    for (inscrip = 0; inscrip < 10; inscrip++)
    {
        /* Look up the tag */
        if (get_tag(&obj, (char)inscrip + '0', item_cmd, mode & QUIVER_TAGS))
        {
            int i;

            for (i = 0; i < num_obj; i++)
            {
                if (items[i].object == obj) break;
            }

            if (i < num_obj)
                m->inscriptions[inscrip] = get_item_tag(m, i);
        }
    }

    /* Set up the item list variables */
    selection = NULL;
    set_obj_names(false, mode);

    /* Take the quiver message into consideration */
    if ((mode & OLIST_QUIVER) && (player->upkeep->quiver[0] != NULL))
        max_len = MAX(max_len, NORMAL_HGT);

    /* Width of extra fields */
    if (olist_mode & OLIST_WEIGHT) {ex_width += 9; ex_offset_ctr += 9;}
    if (olist_mode & OLIST_PRICE) {ex_width += 9; ex_offset_ctr += 9;}
    if (olist_mode & OLIST_FAIL) {ex_width += 10; ex_offset_ctr += 10;}

    /* Set up the menu region */
    area.page_rows = m->count;
    area.row = 1;
    area.col = MIN(Term->wid - 1 - (int) max_len - ex_width, prompt_size - 2);
    if (area.col <= 3) area.col = 0;
    ex_offset = MIN(max_len, (size_t)(Term->wid - 1 - ex_width - area.col));
    while (strlen(header) < max_len + ex_width + ex_offset_ctr)
    {
        my_strcat(header, " ", sizeof(header));
        if (strlen(header) > sizeof(header) - 2) break;
    }
    area.width = MAX(max_len, strlen(header));

    for (row = area.row; row < area.row + area.page_rows; row++)
        prt("", row, MAX(0, area.col - 1));

    menu_layout(m, &area);

    /* Hack -- reset last row */
    last_row = 0;

    /* Choose */
    evt = menu_select(m, 0, true);

    /* Clean up */
    mem_free(m->inscriptions);
    mem_free(m);

    /* Deal with menu switch */
    if ((evt.type == EVT_SWITCH) && !newmenu)
    {
        bool left = (evt.key.code == ARROW_LEFT);

        if (command_wrk == USE_EQUIP)
        {
            if (left)
            {
                if (f1 <= f2) command_wrk = USE_FLOOR;
                else if (q1 <= q2) command_wrk = USE_QUIVER;
                else if (i1 <= i2) command_wrk = USE_INVEN;
            }
            else
            {
                if (i1 <= i2) command_wrk = USE_INVEN;
                else if (q1 <= q2) command_wrk = USE_QUIVER;
                else if (f1 <= f2) command_wrk = USE_FLOOR;
            }
        }
        else if (command_wrk == USE_INVEN)
        {
            if (left)
            {
                if (e1 <= e2) command_wrk = USE_EQUIP;
                else if (f1 <= f2) command_wrk = USE_FLOOR;
                else if (q1 <= q2) command_wrk = USE_QUIVER;
            }
            else
            {
                if (q1 <= q2) command_wrk = USE_QUIVER;
                else if (f1 <= f2) command_wrk = USE_FLOOR;
                else if (e1 <= e2) command_wrk = USE_EQUIP;
            }
        }
        else if (command_wrk == USE_QUIVER)
        {
            if (left)
            {
                if (i1 <= i2) command_wrk = USE_INVEN;
                else if (e1 <= e2) command_wrk = USE_EQUIP;
                else if (f1 <= f2) command_wrk = USE_FLOOR;
            }
            else
            {
                if (f1 <= f2) command_wrk = USE_FLOOR;
                else if (e1 <= e2) command_wrk = USE_EQUIP;
                else if (i1 <= i2) command_wrk = USE_INVEN;
            }
        }
        else if (command_wrk == SHOW_THROWING)
        {
            if (left)
            {
                if (f1 <= f2) command_wrk = USE_FLOOR;
                else if (q1 <= q2) command_wrk = USE_QUIVER;
                else if (i1 <= i2) command_wrk = USE_INVEN;
            }
            else
            {
                if (i1 <= i2) command_wrk = USE_INVEN;
                else if (q1 <= q2) command_wrk = USE_QUIVER;
                else if (f1 <= f2) command_wrk = USE_FLOOR;
            }
        }
        else if (command_wrk == USE_FLOOR)
        {
            if (left)
            {
                if (q1 <= q2) command_wrk = USE_QUIVER;
                else if (i1 <= i2) command_wrk = USE_INVEN;
                else if (e1 <= e2) command_wrk = USE_EQUIP;
            }
            else
            {
                if (e1 <= e2) command_wrk = USE_EQUIP;
                else if (i1 <= i2) command_wrk = USE_INVEN;
                else if (q1 <= q2) command_wrk = USE_QUIVER;
            }
        }

        newmenu = true;
    }

    /* Result */
    return selection;
}


/*
 * Get the indexes of objects at a given floor location.
 *
 * If size is passed, checks that the floor didn't change.
 */
static bool scan_floor(struct object **floor_list, int *size)
{
    int i;
    bool changed = false;

    if (floor_num > z_info->floor_size) floor_num = z_info->floor_size;

    /* Check number of items */
    if ((*size >= 0) && (floor_num != *size)) changed = true;

    /* Scan all objects in the grid */
    for (i = 0; i < floor_num; i++)
    {
        /* Check this item */
        if ((*size >= 0) && (floor_items[i]->oidx != floor_list[i]->oidx)) changed = true;

        /* Accept this item */
        floor_list[i] = floor_items[i];
    }

    /* Set size */
    *size = floor_num;

    /* The floor may have changed */
    return changed;
}


static int scan_throwable(struct object **item_list, size_t item_max)
{
    int i;
    size_t item_num = 0;

    for (i = 0; ((i < z_info->pack_size) && (item_num < item_max)); i++)
    {
        if (object_test(player, obj_is_throwing, player->upkeep->inven[i]))
            item_list[item_num++] = player->upkeep->inven[i];
    }

    for (i = 0; ((i < z_info->quiver_size) && (item_num < item_max)); i++)
    {
        if (object_test(player, obj_is_throwing, player->upkeep->quiver[i]))
            item_list[item_num++] = player->upkeep->quiver[i];
    }

    for (i = 0; ((i < floor_num) && (item_num < item_max)); i++)
    {
        if (object_test(player, obj_is_throwing, floor_items[i]))
            item_list[item_num++] = floor_items[i];
    }

    return item_num;
}


/*
 * Let the user select an object, save its address
 *
 * Return true only if an acceptable item was chosen by the user.
 *
 * The user is allowed to choose acceptable items from the equipment,
 * inventory, quiver, or floor, respectively, if the proper flag was given,
 * and there are any acceptable items in that location.
 *
 * The equipment, inventory or quiver are displayed (even if no acceptable
 * items are in that location) if the proper flag was given.
 *
 * If there are no acceptable items available anywhere, and "str" is
 * not NULL, then it will be used as the text of a warning message
 * before the function returns.
 *
 * If a legal item is selected, we save it in "choice" and return true.
 *
 * If no item is available, we do nothing to "choice", and we display a
 * warning message, using "str" if available, and return false.
 *
 * If no item is selected, we do nothing to "choice", and return false.
 *
 * "command_wrk" is used to choose between equip/inven/quiver/floor listings.
 * It is equal to USE_INVEN or USE_EQUIP or USE_QUIVER or USE_FLOOR, and set to USE_INVEN
 * by default.
 *
 * "inkey_next" is used to disable item selection during keymaps.
 *
 * We always erase the prompt when we are done, leaving a blank line,
 * or a warning message, if appropriate, if no items are available.
 */
bool textui_get_item(struct object **choice, const char *pmt, const char *str, cmd_code cmd,
    item_tester tester, int mode)
{
    bool toggle = false;
    int floor_max = z_info->floor_size;
    int floor_nb = -1;
    int throwing_max = z_info->pack_size + z_info->quiver_size + z_info->floor_size;
    struct object **floor_list = mem_zalloc(floor_max * sizeof(*floor_list));
    struct object **throwing_list = mem_zalloc(throwing_max * sizeof(*throwing_list));
    bool equip_up, inven_up;

    olist_mode = 0;
    item_cmd = cmd;
    hidden = (inkey_next? true: false);

    /* Object list display modes */
    if (mode & SHOW_FAIL) olist_mode |= OLIST_FAIL;
    else olist_mode |= OLIST_WEIGHT;
    if (mode & SHOW_PRICES) olist_mode |= OLIST_PRICE;
    if (mode & SHOW_EMPTY) olist_mode |= OLIST_SEMPTY;
    if (mode & SHOW_QUIVER) olist_mode |= OLIST_QUIVER;
    if (mode & BOOK_TAGS) olist_mode |= OLIST_BOOK_TAGS;
    if (mode & SHOW_RECHARGE) olist_mode |= OLIST_RECHARGE;

    /* No window updates needed */
    equip_up = inven_up = false;

    /* Full inventory */
    i1 = 0;
    i2 = z_info->pack_size - 1;

    /* Forbid inventory */
    if (!(mode & USE_INVEN)) i2 = -1;

    /* Restrict inventory indexes */
    while ((i1 <= i2) && !object_test(player, tester, player->upkeep->inven[i1])) i1++;
    while ((i1 <= i2) && !object_test(player, tester, player->upkeep->inven[i2])) i2--;

    /* Update window (later, twice) */
    if ((i1 != 0) || (i2 != z_info->pack_size - 1)) inven_up = true;

    /* Full equipment */
    e1 = 0;
    e2 = player->body.count - 1;

    /* Forbid equipment */
    if (!(mode & USE_EQUIP)) e2 = -1;

    /* Restrict equipment indexes unless starting with no command */
    if ((cmd != CMD_NULL) || (tester != NULL))
    {
        while ((e1 <= e2) && !object_test(player, tester, slot_object(player, e1))) e1++;
        while ((e1 <= e2) && !object_test(player, tester, slot_object(player, e2))) e2--;
    }

    /* Update window (later, twice) */
    if ((e1 != 0) || (e2 != player->body.count)) equip_up = true;

    /* Restrict quiver indexes */
    q1 = 0;
    q2 = z_info->quiver_size - 1;

    /* Forbid quiver */
    if (!(mode & USE_QUIVER)) q2 = -1;

    /* Restrict quiver indexes */
    while ((q1 <= q2) && !object_test(player, tester, player->upkeep->quiver[q1])) q1++;
    while ((q1 <= q2) && !object_test(player, tester, player->upkeep->quiver[q2])) q2--;

    /* Update window (later, twice) */
    if ((q1 != 0) || (q2 != z_info->quiver_size - 1)) equip_up = true;

    /* Scan all non-gold objects in the grid */
    scan_floor(floor_list, &floor_nb);

    /* Full floor */
    f1 = 0;
    f2 = floor_nb - 1;

    /* Forbid floor */
    if (!(mode & USE_FLOOR)) f2 = -1;

    /* Restrict floor indexes */
    while ((f1 <= f2) && !object_test(player, tester, floor_list[f1])) f1++;
    while ((f1 <= f2) && !object_test(player, tester, floor_list[f2])) f2--;

    /* Scan all throwing objects in reach */
    throwing_num = scan_throwable(throwing_list, throwing_max);

    /* Require at least one legal choice */
    if ((i1 <= i2) || (e1 <= e2) || (q1 <= q2) || (f1 <= f2))
    {
        /* Use throwing menu if at all possible */
        if ((mode & SHOW_THROWING) && throwing_num) command_wrk = SHOW_THROWING;

        /* Start where requested if possible */
        else if ((mode & START_EQUIP) && (e1 <= e2)) command_wrk = USE_EQUIP;
        else if ((mode & START_INVEN) && (i1 <= i2)) command_wrk = USE_INVEN;
        else if ((mode & START_QUIVER) && (q1 <= q2)) command_wrk = USE_QUIVER;

        /* If we are obviously using the quiver then start on quiver */
        else if ((mode & QUIVER_TAGS) && (q1 <= q2)) command_wrk = USE_QUIVER;

        /* Otherwise choose whatever is allowed */
        else if (i1 <= i2) command_wrk = USE_INVEN;
        else if (e1 <= e2) command_wrk = USE_EQUIP;
        else if (q1 <= q2) command_wrk = USE_QUIVER;
        else if (f1 <= f2) command_wrk = USE_FLOOR;

        /* If nothing to choose, use (empty) inventory */
        else command_wrk = USE_INVEN;

        /* Hack -- display the first floor item */
        if (f1 <= f2) olist_mode |= (OLIST_FLOOR);

        while (true)
        {
            int j;
            int ni = 0;
            int ne = 0;
            bool hack_no_wield;

            /*
             * If inven or equip is on the main screen, and only one of them
             * is slated for a subwindow, we should show the opposite there
             */
            for (j = 0; j < ANGBAND_TERM_MAX; j++)
            {
                /* Unused */
                if (!angband_term[j]) continue;

                /* Count windows displaying inven */
                if (window_flag[j] & PW_INVEN) ni++;

                /* Count windows displaying equip */
                if (window_flag[j] & PW_EQUIP) ne++;
            }

            /* Are we in the situation where toggling makes sense? */
            if ((ni && !ne) || (!ni && ne))
            {
                /* Main screen is equipment, so is subwindow */
                if (command_wrk == USE_EQUIP)
                {
                    if ((ne && !toggle) || (ni && toggle))
                    {
                        toggle_inven_equip();
                        toggle = !toggle;
                    }
                }

                /* Main screen is inventory, so is subwindow */
                else if (command_wrk == USE_INVEN)
                {
                    if ((ni && !toggle) || (ne && toggle))
                    {
                        toggle_inven_equip();
                        toggle = !toggle;
                    }
                }

                /* Quiver or floor, go back to the original */
                else
                {
                    if (toggle)
                    {
                        toggle_inven_equip();
                        toggle = !toggle;
                    }
                }
            }

            /* Redraw */
            if (inven_up) event_signal(EVENT_INVENTORY);
            if (equip_up) event_signal(EVENT_EQUIPMENT);

            /* Save screen */
            screen_save();

            /* Build object list */
            wipe_obj_list();
            if (command_wrk == USE_INVEN)
                build_obj_list(i2, player->upkeep->inven, tester, olist_mode);
            else if (command_wrk == USE_EQUIP)
                build_obj_list(e2, NULL, tester, olist_mode);
            else if (command_wrk == USE_QUIVER)
                build_obj_list(q2, player->upkeep->quiver, tester, olist_mode);
            else if (command_wrk == USE_FLOOR)
                build_obj_list(f2, floor_list, tester, olist_mode);
            else if (command_wrk == SHOW_THROWING)
                build_obj_list(throwing_num, throwing_list, tester, olist_mode);

            /* Always add floor items for macros */
            if (hidden && (mode & USE_FLOOR) && !(command_wrk == USE_FLOOR))
                build_obj_list(f2, floor_list, tester, olist_mode);

            /* Show the prompt */
            menu_header();
            if (pmt)
            {
                prt(pmt, 0, 0);
                prt(header, 0, strlen(pmt) + 1);
            }

            /* No menu change request */
            newmenu = false;

            /* The top line is icky */
            topline_icky = true;

            /* Hack -- disable quick floor on wield if no wieldable item in inventory */
            hack_no_wield = ((cmd == CMD_WIELD) && (i1 > i2));

            /* Hack -- quick floor for single items (except on pickup) */
            if (OPT(player, quick_floor) && (command_wrk == USE_FLOOR) && (f1 == f2) &&
                (cmd != CMD_PICKUP) && !hack_no_wield)
            {
                *choice = floor_list[f2];
            }

            /* Get an item choice */
            else
                *choice = item_menu(cmd, MAX((pmt? strlen(pmt): 0), 15), mode);

            /* Fix the top line */
            topline_icky = false;

            /* Paranoia: floor may have changed in the meantime */
            if (command_wrk == USE_FLOOR)
            {
                /* Scan all non-gold objects in the grid again */
                bool changed = scan_floor(floor_list, &floor_nb);

                /* Floor has changed: don't parse the key */
                if (changed)
                {
                    /* Full floor */
                    f1 = 0;
                    f2 = floor_nb - 1;

                    /* Forbid floor */
                    if (!(mode & USE_FLOOR)) f2 = -1;

                    /* Restrict floor indexes */
                    while ((f1 <= f2) && !object_test(player, tester, floor_list[f1])) f1++;
                    while ((f1 <= f2) && !object_test(player, tester, floor_list[f2])) f2--;

                    /* Restore the screen */
                    screen_load(newmenu? (store_ctx == NULL): false);

                    continue;
                }
            }

            /* Fix the screen */
            screen_load(newmenu? (store_ctx == NULL): false);

            /* Redraw */
            if (inven_up) event_signal(EVENT_INVENTORY);
            if (equip_up) event_signal(EVENT_EQUIPMENT);

            /* Clear the prompt line */
            prt("", 0, 0);

            /* We have a selection, or are backing out */
            if (*choice || !newmenu)
            {
                if (toggle) toggle_inven_equip();
                break;
            }
        }
    }
    else
    {
        /* Warning if needed */
        if (str) c_msg_print(str);
        *choice = NULL;
    }

    /* Flush any events */
    if (!store_ctx) Flush_queue();

    /* Clean up */
    command_wrk = 0;
    mem_free(throwing_list);
    mem_free(floor_list);

    /* Result */
    return ((*choice != NULL)? true: false);
}


/*
 * Object ignore interface
 */


enum
{
    IGNORE_THIS_ITEM,
    UNIGNORE_THIS_ITEM,

    /* PWMAngband: keep destruction as an option */
    DESTROY_THIS_ITEM,

    IGNORE_THIS_FLAVOR,
    UNIGNORE_THIS_FLAVOR,
    IGNORE_THIS_EGO,
    UNIGNORE_THIS_EGO,
    IGNORE_THIS_QUALITY
};


static int index_from_oidx(int oidx)
{
    int i, size = z_info->pack_size + player->body.count + z_info->quiver_size;

    for (i = 0; i < size; i++)
    {
        struct object *obj = &player->gear[i];

        if (obj->oidx == oidx) return 1 + i;
    }

    for (i = 0; i < floor_num; i++)
    {
        struct object *obj = floor_items[i];

        if (obj->oidx == oidx) return 0 - (1 + i);
    }

    return 0;
}


static struct object *object_from_index(int idx)
{
    if ((idx > 0) && (idx <= z_info->pack_size + player->body.count + z_info->quiver_size))
        return &player->gear[idx - 1];

    if ((idx < 0) && (0 - idx <= floor_num))
        return floor_items[0 - (1 + idx)];

    return NULL;
}


void textui_cmd_ignore_menu(struct object *obj)
{
    char out_val[160];
    struct menu *m;
    int selected;
    uint8_t value;
    int type;
    bool artifact;
    int idx;

    if (!obj) return;

    m = menu_dynamic_new();
    m->selections = all_letters_nohjkl;

    /* Basic ignore option */
    if (!obj->ignore_protect)
    {
        if (!obj->notice)
            menu_dynamic_add(m, "Ignore this item", IGNORE_THIS_ITEM);
        else
            menu_dynamic_add(m, "Unignore this item", UNIGNORE_THIS_ITEM);
    }

    /* PWMAngband: keep destruction as an option */
    menu_dynamic_add(m, "Destroy this item", DESTROY_THIS_ITEM);

    /* Flavour-aware ignoring */
    if (!obj->ignore_protect)
    {
        artifact = (kf_has(obj->kind->kind_flags, KF_INSTA_ART) ||
            kf_has(obj->kind->kind_flags, KF_QUEST_ART));
        if (ignore_tval(obj->tval) && player->kind_aware[obj->kind->kidx] &&
            !artifact)
        {
            if (!player->kind_ignore[obj->kind->kidx])
            {
                strnfmt(out_val, sizeof(out_val), "Ignore all %s", obj->info_xtra.name_base);
                menu_dynamic_add(m, out_val, IGNORE_THIS_FLAVOR);
            }
            else
            {
                strnfmt(out_val, sizeof(out_val), "Unignore all %s", obj->info_xtra.name_base);
                menu_dynamic_add(m, out_val, UNIGNORE_THIS_FLAVOR);
            }
        }
    }

    type = ignore_type_of(obj);

    /* Ego ignoring */
    if ((obj->info_xtra.eidx >= 0) && (obj->info_xtra.eidx < z_info->e_max) &&
        !obj->ignore_protect && (type != ITYPE_MAX))
    {
        struct ego_desc choice;
        char tmp[NORMAL_WID] = "";

        choice.e_idx = obj->info_xtra.eidx;
        choice.itype = type;
        choice.short_name = "";
        ego_item_name(tmp, sizeof(tmp), &choice);
        if (!player->ego_ignore_types[choice.e_idx][choice.itype])
        {
            strnfmt(out_val, sizeof out_val, "Ignore all %s", tmp + 4);
            menu_dynamic_add(m, out_val, IGNORE_THIS_EGO);
        }
        else
        {
            strnfmt(out_val, sizeof out_val, "Unignore all %s", tmp + 4);
            menu_dynamic_add(m, out_val, UNIGNORE_THIS_EGO);
        }
    }

    /* Quality ignoring */
    if (!obj->ignore_protect)
    {
        value = obj->info_xtra.quality_ignore;
        if ((value != IGNORE_MAX) && (type != ITYPE_MAX))
        {
            strnfmt(out_val, sizeof(out_val), "Ignore all %s %s", quality_name_for_value(value),
                ignore_name_for_type(type));

            menu_dynamic_add(m, out_val, IGNORE_THIS_QUALITY);
        }
    }

    menu_dynamic_calc_location(m);

    /* Hack -- save object index */
    idx = index_from_oidx(obj->oidx);

    prt("(Enter to select, ESC) Ignore:", 0, 0);
    selected = menu_dynamic_select(m);

    menu_dynamic_free(m);
    screen_load(false);

    /* Hack --  make sure the object still exists (corpses may have decomposed, ...) */
    obj = object_from_index(idx);
    if (!obj) return;

    if ((selected == IGNORE_THIS_ITEM) || (selected == UNIGNORE_THIS_ITEM))
        Send_destroy(obj, false);
    else if (selected == DESTROY_THIS_ITEM)
        Send_destroy(obj, true);
    else if (selected == IGNORE_THIS_FLAVOR)
    {
        player->kind_ignore[obj->kind->kidx] = 1;
        Send_ignore();
    }
    else if (selected == UNIGNORE_THIS_FLAVOR)
    {
        player->kind_ignore[obj->kind->kidx] = 0;
        Send_ignore();
    }
    else if (selected == IGNORE_THIS_EGO)
    {
        player->ego_ignore_types[obj->info_xtra.eidx][ignore_type_of(obj)] = 1;
        Send_ignore();
    }
    else if (selected == UNIGNORE_THIS_EGO)
    {
        player->ego_ignore_types[obj->info_xtra.eidx][ignore_type_of(obj)] = 0;
        Send_ignore();
    }
    else if (selected == IGNORE_THIS_QUALITY)
    {
        player->opts.ignore_lvl[type] = value;
        Send_ignore();
    }
}


void textui_cmd_ignore(void)
{
    struct object *obj;
    const char *q = "Ignore which item? ";
    const char *s = "You have nothing to ignore.";

    /* Get an item */
    if (!get_item(&obj, q, s, CMD_IGNORE, NULL, (USE_EQUIP | USE_INVEN | USE_QUIVER | USE_FLOOR)))
        return;

    textui_cmd_ignore_menu(obj);
}


void textui_cmd_toggle_ignore(void)
{
    Send_toggle_ignore();
}