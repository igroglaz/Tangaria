/*
 * File: c-option.h
 * Purpose: Options table and definitions.
 */

#ifndef INCLUDED_C_OPTIONS_H
#define INCLUDED_C_OPTIONS_H

/*
 * Functions
 */
extern bool option_set(bool *opts, const char *opt, size_t val);
extern void options_init_defaults(struct player_options *opts);
extern bool options_save_custom(bool *opts, int page);
extern bool options_restore_custom(bool *opts, int page);
extern void options_restore_maintainer(bool *opts, int page);
extern void init_options(bool *opts);

#endif /* INCLUDED_C_OPTIONS_H */
