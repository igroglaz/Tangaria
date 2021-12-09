/*
 * File: effect-handler.h
 * Purpose: Internal header for effect handler functions
 */

#ifndef INCLUDED_EFFECT_HANDLER_H
#define INCLUDED_EFFECT_HANDLER_H

/*
 * Bit flags for the "enchant()" function
 */
#define ENCH_TOHIT   0x01
#define ENCH_TODAM   0x02
#define ENCH_TOBOTH  0x03
#define ENCH_TOAC    0x04

typedef struct effect_handler_context_s
{
    effect_index effect;
    struct source *origin;
    struct chunk *cave;
    bool aware;
    int dir;
    struct beam_info beam;
    int boost;
    random_value value;
    int subtype, radius, other, y, x;
    const char *self_msg;
    bool ident;
    quark_t note;
    int flag;
    struct monster *target_mon;
} effect_handler_context_t;

typedef bool (*effect_handler_f)(effect_handler_context_t *);

/*
 * Structure for effects
 */
struct effect_kind
{
    u16b index;                 /* Effect index */
    bool aim;                   /* Whether the effect requires aiming */
    const char *info;           /* Effect info (for spell tips) */
    effect_handler_f handler;   /* Function to perform the effect */
    const char *desc;           /* Effect description */
};

/* Prototype every effect handler */
#define EFFECT(x, a, b, c, d, e) extern bool effect_handler_##x(effect_handler_context_t *context);
#include "list-effects.h"
#undef EFFECT

/* effect-handler-attack.c */
extern bool project_aimed(struct source *origin, int typ, int dir, int dam, int flg,
    const char *what);
extern bool project_los(effect_handler_context_t *context, int typ, int dam, bool obvious);
extern bool fire_bolt(struct source *origin, int typ, int dir, int dam, bool obvious);
extern bool fire_ball(struct player *p, int typ, int dir, int dam, int rad, bool obvious,
    bool constant);

/* effect-handler-general.c */
extern int effect_calculate_value(effect_handler_context_t *context, bool use_boost);
extern const char *desc_stat(int stat, bool positive);

#endif /* INCLUDED_EFFECT_HANDLER_H */
