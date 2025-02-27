/*
 * File: buildid.c
 * Purpose: Version strings
 *
 * Copyright (c) 2011 Andi Sidwell
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


#include "angband.h"


/*
 * Define for Beta version, undefine for stable build
 */
/*#define VERSION_BETA*/


bool beta_version(void)
{
#ifdef VERSION_BETA
    return true;
#else
    return false;
#endif
}


/*
 * Current version number of PWMAngband
 */
#define VERSION_MAJOR   1
#define VERSION_MINOR   6
#define VERSION_PATCH   1
#define VERSION_EXTRA   1
#define VERSION_TANGARIA   26


// Note that it's uint16_t, so max version after << operations might be 65535.. 
// ..which new T version marker will overflow. So for comparison we will use just T version
uint16_t current_version(void)
{
/*
    return ((VERSION_MAJOR << 16) | (VERSION_MINOR << 12) | (VERSION_PATCH << 8) |
             VERSION_EXTRA  << 4 | VERSION_TANGARIA);
*/
    return VERSION_TANGARIA;
}


/*
 * Minimum version number of PWMAngband client allowed
 */
#define MIN_VERSION_MAJOR   1
#define MIN_VERSION_MINOR   6
#define MIN_VERSION_PATCH   1
#define MIN_VERSION_EXTRA   1
#define MIN_VERSION_TANGARIA   26


uint16_t min_version(void)
{
/*
    return ((MIN_VERSION_MAJOR << 16) | (MIN_VERSION_MINOR << 12) |
        (MIN_VERSION_PATCH << 8) | MIN_VERSION_EXTRA  << 4 | MIN_VERSION_TANGARIA);
*/
    return MIN_VERSION_TANGARIA;
}


static char version[32];


char *version_build(const char *label, bool build)
{
    if (label && build)
    {
        strnfmt(version, sizeof(version), "%s %d.%d.%d (%s %d) T-%d", label, VERSION_MAJOR,
            VERSION_MINOR, VERSION_PATCH, (beta_version()? "Beta": "Build"), VERSION_EXTRA, VERSION_TANGARIA);
    }
    else if (label)
    {
        strnfmt(version, sizeof(version), "%s %d.%d.%d T-%d", label, VERSION_MAJOR, VERSION_MINOR,
            VERSION_PATCH, VERSION_TANGARIA);
    }
    else if (build)
    {
        strnfmt(version, sizeof(version), "%d.%d.%d (%s %d) T-%d", VERSION_MAJOR, VERSION_MINOR,
            VERSION_PATCH, (beta_version()? "Beta": "Build"), VERSION_EXTRA, VERSION_TANGARIA);
    }
    else
        strnfmt(version, sizeof(version), "%d.%d.%d T-%d", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH,
            VERSION_TANGARIA);

    return version;
}




