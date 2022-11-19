/*
 * File: account.c
 * Purpose: Account management
 *
 * Copyright (c) 2022 PWMAngband Developers
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


static int get_attempts(const char *name)
{
    char filename[MSG_LEN];
    ang_file *fh;
    char filebuf[MSG_LEN];

    path_build(filename, sizeof(filename), ANGBAND_DIR_SAVE, format("%s.lock", name));
    fh = file_open(filename, MODE_READ, FTYPE_TEXT);
    if (!fh) return 0;
    file_getl(fh, filebuf, sizeof(filebuf));
    file_close(fh);
    return atoi(filebuf);
}


static void update_attempts(const char *name, int attempts)
{
    char filename[MSG_LEN];
    ang_file *fh;

    path_build(filename, sizeof(filename), ANGBAND_DIR_SAVE, format("%s.lock", name));
    fh = file_open(filename, MODE_WRITE, FTYPE_TEXT);
    if (!fh)
    {
        plog("Failed to open lock file!");
        return;
    }
    file_putf(fh, "%d", attempts);
    file_close(fh);
}


static bool add_account(char *filename, const char *name, const char *pass)
{
    ang_file *fh;
    char filebuf[MSG_LEN], *str;

    /* Append to the file */
    fh = file_open(filename, MODE_APPEND, FTYPE_TEXT);
    if (!fh)
    {
        plog("Failed to open account file!");
        return false;
    }

    /* Lowercase account name */
    my_strcpy(filebuf, name, sizeof(filebuf));
    for (str = filebuf; *str; str++) *str = tolower((unsigned char)*str);

    /* Create new account */
    file_putf(fh, "%s\n", filebuf);
    file_putf(fh, "%s\n", pass);

    /* Close */
    file_close(fh);

    /* Create new lock file */
    update_attempts(name, 0);

    return true;
}


uint32_t get_account(const char *name, const char *pass)
{
    char filename[MSG_LEN];
    ang_file *fh;
    char filebuf[MSG_LEN];
    uint32_t account_id = 1L;

    /* Get lock file */
    int attempts = get_attempts(name);

    /* Check attempts */
    if (attempts == 42)
    {
        plog("Account is locked!");
        return 0L;
    }

    /* Check account file */
    path_build(filename, sizeof(filename), ANGBAND_DIR_SAVE, "account");
    if (!file_exists(filename))
    {
        if (add_account(filename, name, pass)) return 1L;
        return 0L;
    }

    /* Open the file */
    fh = file_open(filename, MODE_READ, FTYPE_TEXT);
    if (!fh)
    {
        plog("Failed to open account file!");
        return 0L;
    }

    /* Process the file */
    while (file_getl(fh, filebuf, sizeof(filebuf)))
    {
        /* Get account name */
        if (!my_stricmp(filebuf, name))
        {
            /* Get account password */
            file_getl(fh, filebuf, sizeof(filebuf));
            file_close(fh);

            /* Check account password */
            if (streq(filebuf, pass))
            {
                if (attempts > 0) update_attempts(name, 0);
                return account_id;
            }

            /* Incorrect password */
            plog("Incorrect password!");
            update_attempts(name, attempts + 1);
            return 0L;
        }

        file_getl(fh, filebuf, sizeof(filebuf));
        account_id++;
    }

    /* Close the file */
    file_close(fh);

    if (add_account(filename, name, pass)) return account_id;
    return 0L;
}
