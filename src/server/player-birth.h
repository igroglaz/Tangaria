/*
 * File: player-birth.h
 * Purpose: Character creation
 */

#ifndef PLAYER_BIRTH_H
#define PLAYER_BIRTH_H

extern s32b player_id;

extern bool savefile_set_name(struct player *p);
extern const char *savefile_get_name(char *savefile, char *panicfile);
extern struct player *player_birth(int id, u32b account, const char *name, const char *pass,
    int conn, byte ridx, byte cidx, byte psex, s16b* stat_roll, bool options[OPT_MAX]);
extern void server_birth(void);
extern u16b connection_type_ok(u16b conntype);

#endif /* PLAYER_BIRTH_H */
