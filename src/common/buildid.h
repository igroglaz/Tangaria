/*
 * File: buildid.h
 * Purpose: Version strings
 */

/*
 * Name of this Angband variant
 */
#ifndef BUILDID
#define BUILDID

#define VERSION_NAME    "Tangaria"

extern bool beta_version(void);
extern uint16_t current_version(void);
extern uint16_t min_version(void);
extern char *version_build(const char *label, bool build);

#endif /* BUILDID */
