/*
 * File: keymap.c
 * Purpose: Keymap handling
 *
 * Copyright (c) 2011 Andi Sidwell
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
 * Keymap implementation.
 *
 * Keymaps are defined in pref files and map onto the internal game keyset,
 * which is roughly what you get if you have roguelike keys turned off.
 *
 * We store keymaps by pairing triggers with actions; the trigger is a single
 * keypress and the action is stored as a string of keypresses, terminated
 * with a keypress with type == EVT_NONE.
 *
 * XXX We should note when we read in keymaps that are "official game" keymaps
 * and ones which are user-defined.  Then we can avoid writing out official
 * game ones and messing up everyone's pref files with a load of junk.
 */


/*
 * Struct for a keymap.
 */
struct keymap
{
    struct keypress key;
    struct keypress *actions;
    bool user;  /* User-defined keymap */
    struct keymap *next;
};


/*
 * List of keymaps.
 */
static struct keymap *keymaps[KEYMAP_MODE_MAX];


/*
 * Find a keymap, given a keypress.
 */
const struct keypress *keymap_find(int keymap, struct keypress kc)
{
    struct keymap *k;

    assert((keymap >= 0) && (keymap < KEYMAP_MODE_MAX));

    for (k = keymaps[keymap]; k; k = k->next)
    {
        if ((k->key.code == kc.code) && (k->key.mods == kc.mods)) return k->actions;
    }

    return NULL;
}


/*
 * Duplicate a given keypress string and return the duplicate.
 */
static struct keypress *keymap_make(const struct keypress *actions)
{
    struct keypress *newk;
    size_t n = 0;

    while (actions[n].type) n++;

    /* Make room for the terminator */
    n += 1;

    newk = mem_zalloc(sizeof(*newk) * n);
    memcpy(newk, actions, sizeof(*newk) * n);

    newk[n - 1].type = EVT_NONE;

    return newk;
}


/*
 * Add a keymap to the mappings table.
 */
void keymap_add(int keymap, struct keypress trigger, struct keypress *actions, bool user)
{
    struct keymap *k = mem_zalloc(sizeof(*k));

    assert((keymap >= 0) && (keymap < KEYMAP_MODE_MAX));

    keymap_remove(keymap, trigger);

    k->key = trigger;
    k->actions = keymap_make(actions);
    k->user = user;

    k->next = keymaps[keymap];
    keymaps[keymap] = k;
}


/*
 * Remove a keymap.  Return true if one was removed.
 */
bool keymap_remove(int keymap, struct keypress trigger)
{
    struct keymap *k;
    struct keymap *prev = NULL;

    assert((keymap >= 0) && (keymap < KEYMAP_MODE_MAX));

    for (k = keymaps[keymap]; k; k = k->next)
    {
        if ((k->key.code == trigger.code) && (k->key.mods == trigger.mods))
        {
            mem_free(k->actions);
            if (prev)
                prev->next = k->next;
            else
                keymaps[keymap] = k->next;
            mem_free(k);
            return true;
        }

        prev = k;
    }

    return false;
}


/*
 * Forget and free all keymaps.
 */
void keymap_free(void)
{
    size_t i;
    struct keymap *k;

    for (i = 0; i < N_ELEMENTS(keymaps); i++)
    {
        k = keymaps[i];
        while (k)
        {
            struct keymap *next = k->next;

            mem_free(k->actions);
            mem_free(k);
            k = next;
        }
    }
}


/*
 * Append active keymaps to a given file.
 */
void keymap_dump(ang_file *fff)
{
    int mode;
    struct keymap *k;

    if (OPT(player, rogue_like_commands))
        mode = KEYMAP_MODE_ROGUE;
    else
        mode = KEYMAP_MODE_ORIG;

    for (k = keymaps[mode]; k; k = k->next)
    {
        char buf[MSG_LEN];
        struct keypress key[2];

        if (!k->user) continue;

        memset(key, 0, 2 * sizeof(struct keypress));

        /* Encode the action */
        keypress_to_text(buf, sizeof(buf), k->actions, false);
        file_putf(fff, "keymap-act:%s\n", buf);

        /* Convert the key into a string */
        key[0] = k->key;
        keypress_to_text(buf, sizeof(buf), key, true);
        file_putf(fff, "keymap-input:%d:%s\n", mode, buf);

        file_put(fff, "\n");
    }
}


int keymap_browse(int o, int *j)
{
    int mode;
    struct keymap *k;
    int total;
    int hgt = Term->max_hgt - 4;

    if (OPT(player, rogue_like_commands))
        mode = KEYMAP_MODE_ROGUE;
    else
        mode = KEYMAP_MODE_ORIG;

    for (k = keymaps[mode], total = 0; k; k = k->next)
    {
        char buf[MSG_LEN];
        struct keypress key[2];
        char act[MSG_LEN];
        int i = total;
        char a;

        memset(key, 0, 2 * sizeof(struct keypress));

        /* Encode the action */
        keypress_to_text(act, sizeof(act), k->actions, false);

        /* Convert the key into a string */
        key[0] = k->key;
        keypress_to_text(buf, sizeof(buf), key, true);

        /* It's ok */
        total++;

        /* Too early */
        if (i < o) continue;

        /* Too late */
        if (i - o >= hgt - 2) continue;

        /* Selected */
        a = COLOUR_WHITE;
        if (*j == i) a = COLOUR_L_BLUE;

        /* Dump the key */
        Term_putstr(0, 2 + i - o, -1, a, buf);

        /* Dump the action */
        Term_putstr(20, 2 + i - o, -1, a, act);
    }

    /* Move cursor there */
    Term_gotoxy(0, 2 + *j - o);

    return total;
}


void command_as_keystroke(unsigned char cmd, struct keypress *kp, size_t *n)
{
    struct keypress c;

    if (*n == KEYMAP_ACTION_MAX) return;

    c.type = EVT_KBRD;
    c.code = cmd;
    c.mods = 0;

    kp[(*n)++] = c;
}


static cmd_code command_by_item_aux(struct object *obj)
{
    if (obj_can_cast_from(player, obj)) return CMD_CAST;
    if (obj_is_useable(player, obj))
    {
        if (tval_is_wand(obj)) return CMD_USE_WAND;
        if (tval_is_rod(obj)) return CMD_USE_ROD;
        if (tval_is_staff(obj)) return CMD_USE_STAFF;
        if (tval_is_scroll(obj)) return CMD_READ_SCROLL;
        if (tval_is_potion(obj)) return CMD_QUAFF;
        if (tval_is_edible(obj)) return CMD_EAT;
        if (obj_is_activatable(player, obj)) return CMD_ACTIVATE;
        if (item_tester_hook_fire(player, obj)) return CMD_FIRE;
        return CMD_USE;
    }
    if (tval_is_ammo(obj)) return CMD_THROW;
    if (obj_can_wear(player, obj)) return CMD_WIELD;
    return CMD_NULL;
}


unsigned char command_by_item(struct object *obj, int mode)
{
    cmd_code lookup_cmd = command_by_item_aux(obj);

    if (lookup_cmd == CMD_NULL) return 0;
    return cmd_lookup_key(lookup_cmd, mode);
}


/*
 * Given an "item", return keystrokes that could be used to select this item.
 *
 * For items tagged @x1, the tagged version will be returned.
 * For non-tagged items, full item name in quotes will be returned.
 */
void item_as_keystroke(struct object *obj, unsigned char cmd, struct keypress *kp, size_t *n)
{
    /* Step one, see if it's tagged */
    char buf[MSG_LEN];
    char *s, *p;
    int tag = -1;
    cmd_code lookup_cmd;

    command_as_keystroke(cmd, kp, n);

    /* Find a '@' */
    my_strcpy(buf, obj->info_xtra.name, sizeof(buf));
    s = strchr(buf, '@');

    /* Process all tags */
    while (s)
    {
        /* Check the special tags */
        if ((s[1] == cmd) && isdigit(s[2]))
        {
            tag = s[2] - '0';

            /* Success */
            break;
        }

        /* Find another '@' */
        s = strchr(s + 1, '@');
    }

    /* We have a nice tag */
    if (tag > -1)
    {
        command_as_keystroke('0' + tag, kp, n);
        return;
    }

    /* PWMAngband: let's keep it simple, just use basic kind name */
    s = obj->kind->name;
    p = buf;
    while (*s)
    {
        if (*s == '&') s += 2;
        else if (*s == '~') s++;
        else
        {
            *p = *s;
            p++;
            s++;
        }
    }
    *p = '\0';

    command_as_keystroke('\"', kp, n);

    p = buf;
    while (*p)
    {
        command_as_keystroke(*p, kp, n);
        p++;
    }

    command_as_keystroke('\"', kp, n);

    /* PWMAngband: add targeting */
    lookup_cmd = command_by_item_aux(obj);
    switch (lookup_cmd)
    {
        case CMD_USE_ROD:
        case CMD_QUAFF:
        case CMD_ACTIVATE:
        case CMD_USE:
        {
            if (need_dir(obj) == DIR_SKIP) break;

            /* fallthrough */
        }

        case CMD_USE_WAND:
        case CMD_FIRE:
        case CMD_THROW:
        {
            command_as_keystroke('\'', kp, n);
            break;
        }

        default: break;
    }
}


/*
 * Given a "spell" in item "book", return keystrokes that could be used to select this spell.
 */
void spell_as_keystroke(int book, int spell, bool project, unsigned char cmd, struct keypress *kp,
    size_t *n)
{
    char buf[MSG_LEN];
    char *s, *p;
    spell_flags flag;

    command_as_keystroke(cmd, kp, n);

    /* Trim full name */
    my_strcpy(buf, book_info[book].spell_info[spell].info, sizeof(buf));
    s = strstr(buf, "  ");
    if (s) *s = '\0';

    command_as_keystroke('\"', kp, n);

    p = buf;
    while (*p)
    {
        command_as_keystroke(*p, kp, n);
        p++;
    }

    command_as_keystroke('\"', kp, n);

    /* PWMAngband: add targeting */
    flag = book_info[book].spell_info[spell].flag;
    if (flag.proj_attr && project)
    {
        command_as_keystroke('(', kp, n);
        command_as_keystroke('t', kp, n);
    }
    else if (flag.dir_attr)
        command_as_keystroke('\'', kp, n);
}
