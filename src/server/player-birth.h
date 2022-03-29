/*
 * File: player-birth.h
 * Purpose: Character creation
 */

#ifndef PLAYER_BIRTH_H
#define PLAYER_BIRTH_H

extern int32_t player_id;

extern bool savefile_set_name(struct player *p);
extern const char *savefile_get_name(char *savefile, char *panicfile);
extern struct player *player_birth(int id, uint32_t account, const char *name, const char *pass,
    int conn, uint8_t ridx, uint8_t cidx, uint8_t psex, int16_t* stat_roll, bool options[OPT_MAX]);
extern void server_birth(void);
extern uint16_t connection_type_ok(uint16_t conntype);

#endif /* PLAYER_BIRTH_H */
