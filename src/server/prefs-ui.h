/*
 * File: prefs-ui.h
 * Purpose: Pref file handling code
 */

#ifndef PREFS_UI_H
#define PREFS_UI_H

extern uint8_t *monster_x_attr;
extern char *monster_x_char;
extern uint8_t *kind_x_attr;
extern char *kind_x_char;
extern uint8_t (*feat_x_attr)[LIGHTING_MAX];
extern char (*feat_x_char)[LIGHTING_MAX];
extern uint8_t (*trap_x_attr)[LIGHTING_MAX];
extern char (*trap_x_char)[LIGHTING_MAX];
extern uint8_t *flavor_x_attr;
extern char *flavor_x_char;

extern bool process_pref_file(const char *name, bool quiet);
extern void textui_prefs_init(void);
extern void textui_prefs_free(void);

#endif /* PREFS_UI_H */
