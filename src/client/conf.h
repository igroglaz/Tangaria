/*
 * File: conf.h
 * Purpose: INI file configuration
 */

#ifndef INCLUDED_CONF_H
#define INCLUDED_CONF_H

extern void conf_init(void* param);
extern void conf_done(void);
extern void conf_save(void);
extern void conf_default_save(void);
extern bool conf_section_exists(const char *section);
extern const char *conf_get_string(const char *section, const char *name, const char *default_value);
extern int32_t conf_get_int(const char *section, const char *name, int32_t default_value);
extern void conf_set_string(const char *section, const char *name, const char *value);
extern void conf_set_int(const char *section, const char *name, int32_t value);
extern void conf_append_section(const char *sectionFrom, const char *sectionTo, const char *filename);
extern void conf_timer(int ticks);
extern bool conf_exists(void);
extern void clia_init(int argc, const char **argv);
extern bool clia_read_string(char *dst, int len, const char *key);
extern bool clia_read_int(int32_t *dst, const char *key);

#endif /* INCLUDED_CONF_H */
