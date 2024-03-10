/*
 * File: player-timed.h
 * Purpose: Timed effects handling
 */

#ifndef PLAYER_TIMED_H
#define PLAYER_TIMED_H

/*
 * Effect failure flag types
 */
enum
{
    TMD_FAIL_FLAG_OBJECT = 1,
    TMD_FAIL_FLAG_RESIST,
    TMD_FAIL_FLAG_VULN,
    TMD_FAIL_FLAG_PLAYER,
    TMD_FAIL_FLAG_TIMED_EFFECT
};

/*
 * Bits in timed_effect_data's flags field
 */
enum
{
    /* Increases to duration will be blocked if effect is already active */
    TMD_FLAG_NONSTACKING = 0x01
};

struct timed_failure
{
    struct timed_failure *next;
    int code; /* one of the TMD_FAIL_FLAG_* constants */
    int idx; /* index for object or player flag, timed effect, element */
};

/*
 * Data struct
 */
struct timed_effect_data
{
    const char *name;
    uint32_t flag_redraw;
    uint32_t flag_update;

    char *desc;
    char *on_end;
    char *on_increase;
    char *on_decrease;
    char *near_begin;
    char *near_end;
    int msgt;
    struct timed_failure *fail;
    struct timed_grade *grade;

    /* This effect chain is triggered when the timed effect starts. */
    struct effect *on_begin_effect;

    /* This effect chain is triggered when the timed effect lapses. */
    struct effect *on_end_effect;

    bitflag flags;
    int lower_bound;
    int oflag_dup;
    bool oflag_syn;
    int temp_resist;
    int temp_brand;
    int temp_slay;
};

/*
 * Holds state while parsing.
 */
struct timed_effect_parse_state
{
    /* Points to timed effect being populated. May be NULL. */
    struct timed_effect_data *t;

    /* Points to the most recent effect chain being modified in the timed effect. May be NULL. */
    struct effect *e;
};

/*
 * Player food values
 */
extern int PY_FOOD_MAX;
extern int PY_FOOD_FULL;
extern int PY_FOOD_HUNGRY;
extern int PY_FOOD_WEAK;
extern int PY_FOOD_FAINT;
extern int PY_FOOD_STARVE;

extern struct file_parser player_timed_parser;
extern struct timed_effect_data timed_effects[];

extern int timed_name_to_idx(const char *name);
extern bool player_set_timed(struct player *p, int idx, int v, bool notify);
extern bool player_inc_check(struct player *p, struct monster *mon, int idx, bool lore);
extern bool player_inc_timed_aux(struct player *p, struct monster *mon, int idx, int v,
    bool notify, bool check);
extern bool player_inc_timed(struct player *p, int idx, int v, bool notify, bool check);
extern bool player_dec_timed(struct player *p, int idx, int v, bool notify);
extern bool player_clear_timed(struct player *p, int idx, bool notify);
extern bool player_timed_grade_eq(struct player *p, int idx, const char *match);

#endif /* PLAYER_TIMED_H */
