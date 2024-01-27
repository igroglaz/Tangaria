/*
 * File: c-option.c
 * Purpose: Options table and definitions.
 *
 * Copyright (c) 1997 Ben Harrison
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
 * Set an option, return true if successful
 */
bool option_set(bool *opts, const char *name, size_t val)
{
    size_t opt;

    /* Try normal options first */
    if (opts)
    {
        for (opt = 0; opt < OPT_MAX; opt++)
        {
            if (!option_name(opt) || !streq(option_name(opt), name)) continue;
            opts[opt] = (val? true: false);

            return true;
        }

        return false;
    }

    if (streq(name, "hp_warn_factor"))
    {
        /* Bounds */
        if (val > 9) val = 9;
        player->opts.hitpoint_warn = val;

        return true;
    }
    if (streq(name, "delay_factor"))
    {
        /* Bounds */
        if (val > 255) val = 255;
        player->opts.delay_factor = val;

        return true;
    }
    if (streq(name, "lazymove_delay"))
    {
        /* Bounds */
        if (val > 9) val = 9;
        player->opts.lazymove_delay = val;

        return true;
    }

    return false;
}


/*
 * Set player default options
 */
void options_init_defaults(struct player_options *opts)
{
    int i;

    /* 40ms for the delay factor */
    opts->delay_factor = 40;

    /* 30% of HP */
    opts->hitpoint_warn = 3;

    /* Initialize extra parameters */
    for (i = ITYPE_NONE; i < ITYPE_MAX; i++) opts->ignore_lvl[i] = IGNORE_NONE;
}


/*
 * Record the options of type, page, for later recall.
 *
 * Return true if successful. Return false if the operation failed.
 */
bool options_save_custom(bool *opts, int page)
{
    const char *page_name = option_type_name(page);
    bool success = true;
    char path[MSG_LEN], file_name[NORMAL_WID];
    ang_file *f;

    strnfmt(file_name, sizeof(file_name), "customized_%s_options.txt", page_name);
    path_build(path, sizeof(path), ANGBAND_DIR_USER, file_name);
    f = file_open(path, MODE_WRITE, FTYPE_TEXT);
    if (f)
    {
        int opt;

        if (!file_putf(f, "# These are customized defaults for the %s options.\n", page_name))
            success = false;
        if (!file_put(f, "# All lines begin with \"option:\" followed by the internal option name.\n"))
            success = false;
        if (!file_put(f, "# After the name is a colon followed by yes or no for the option's state.\n"))
            success = false;
        for (opt = 0; opt < OPT_MAX; opt++)
        {
            if (option_type(opt) == page)
            {
                if (!file_putf(f, "# %s\n", option_desc(opt)))
                    success = false;
                if (!file_putf(f, "option:%s:%s\n", option_name(opt), (opts[opt]? "yes": "no")))
                    success = false;
            }
        }
        if (!file_close(f))
            success = false;
    }
    else
        success = false;

    return success;
}


/*
 * Reset the options of type, page, to the customized defaults.
 *
 * Return true if successful. That includes the case where no customized
 * defaults are available. When that happens, the options are reset to the
 * maintainer's defaults. Return false if the customized defaults are
 * present but unreadable.
 */
bool options_restore_custom(bool *opts, int page)
{
    const char *page_name = option_type_name(page);
    char path[MSG_LEN], buf[MSG_LEN], file_name[NORMAL_WID];
    ang_file *f;
    int linenum;

    strnfmt(file_name, sizeof(file_name), "customized_%s_options.txt", page_name);
    path_build(path, sizeof(path), ANGBAND_DIR_USER, file_name);
    if (!file_exists(path))
    {
        options_restore_maintainer(opts, page);
        return true;
    }

    /* Could use run_parser(), but that exits the application if there are syntax errors */
    f = file_open(path, MODE_READ, FTYPE_TEXT);
    if (!f) return false;
    linenum = 1;
    while (file_getl(f, buf, sizeof(buf)))
    {
        char *sub = strstr(buf, "option:"), *com;
        int opt;

        if (!sub)
        {
            /* If it isn't an option, it should be a comment or whitespace. */
            sub = strchr(buf, '#');

            if (sub) *sub = '\0';
            if (!contains_only_spaces(buf))
            {
                plog_fmt("Line %d of the customized %s options is not parseable.", linenum,
                    page_name);
            }
            ++linenum;
            continue;
        }

        *sub = '\0';

        /* Ignore if the "option:" is embedded in a comment. */
        com = strchr(buf, '#');
        if (com)
        {
            *com = '\0';
            if (!contains_only_spaces(buf))
            {
                plog_fmt("Line %d of the customized %s options is not parseable.", linenum,
                    page_name);
            }
            ++linenum;
            continue;
        }
        if (!contains_only_spaces(buf))
        {
            plog_fmt("Line %d of the customized %s options is not parseable.", linenum,
                page_name);
            ++linenum;
            continue;
        }

        /* Try to find the option. */
        sub += 7;
        opt = 0;
        while (1)
        {
            size_t lname;

            if (opt >= OPT_MAX)
            {
                plog_fmt("Unrecognized option at line %d of the customized %s options.", linenum,
                    page_name);
                break;
            }
            if ((option_type(opt) != page) || !option_name(opt))
            {
                ++opt;
                continue;
            }
            lname = strlen(option_name(opt));
            if (strncmp(option_name(opt), sub, lname) == 0 && sub[lname] == ':')
            {
                if (strncmp("yes", sub + lname + 1, 3) == 0 &&
                    contains_only_spaces(sub + lname + 4))
                {
                    opts[opt] = true;
                }
                else if (strncmp("no", sub + lname + 1, 2) == 0 &&
                    contains_only_spaces(sub + lname + 3))
                {
                    opts[opt] = false;
                }
                else
                {
                    plog_fmt("Value at line %d of the customized %s options is not yes or no.",
                        linenum, page_name);
                }
                break;
            }
            ++opt;
        }
        ++linenum;
    }

    if (!file_close(f)) return false;
    return true;
}


/*
 * Reset the options of type, page, to the maintainer's defaults.
 */
void options_restore_maintainer(bool *opts, int page)
{
    int opt;

    for (opt = 0; opt < OPT_MAX; opt++)
    {
        if (option_type(opt) == page) opts[opt] = option_normal(opt);
    }
}


/*
 * Initialise options package
 */
void init_options(bool *opts)
{
    /* Allocate options to pages */
    option_init();

    /* Set defaults */
    options_restore_maintainer(opts, OP_BIRTH);
    options_restore_maintainer(opts, OP_INTERFACE);
    options_restore_maintainer(opts, OP_MANGBAND);
    options_restore_maintainer(opts, OP_ADVANCED);
    options_restore_maintainer(opts, OP_EXTRA);

    /* Override with customized birth options. */
    options_restore_custom(opts, OP_BIRTH);
    options_restore_custom(opts, OP_INTERFACE);
    options_restore_custom(opts, OP_MANGBAND);
    options_restore_custom(opts, OP_ADVANCED);
    options_restore_custom(opts, OP_EXTRA);
}
