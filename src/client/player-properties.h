/*
 * File: player-properties.h
 * Purpose: Class and race abilities
 */

#ifndef INCLUDED_PLAYER_PROPERTIES_H
#define INCLUDED_PLAYER_PROPERTIES_H

enum {
    PLAYER_FLAG_NONE,
    PLAYER_FLAG_SPECIAL,
    PLAYER_FLAG_RACE,
    PLAYER_FLAG_CLASS
};

extern void do_cmd_abilities(void);
extern void do_cmd_race_stats(struct player_race *r);
extern void do_cmd_class_stats(struct player_class *c);

#endif /* INCLUDED_PLAYER_PROPERTIES_H */
