/*
 * File: effects.h
 * Purpose: Effect handling
 */

#ifndef INCLUDED_EFFECTS_H
#define INCLUDED_EFFECTS_H

/* Types of effect */
typedef enum
{
    EF_NONE,
    #define EFFECT(x, a, b, c, d, e) EF_##x,
    #include "list-effects.h"
    #undef EFFECT
    EF_MAX
} effect_index;

/* Functions */
extern bool effect_aim(const struct effect *effect);
extern const char *effect_info(const struct effect *effect, const char *name);
extern const char *effect_desc(const struct effect *effect);
extern effect_index effect_lookup(const char *name);
extern int effect_subtype(int index, const char *type);
extern expression_base_value_f effect_value_base_by_name(const char *name);
extern bool effect_do(struct effect *effect, struct source *origin, bool *ident, bool aware,
    int dir, struct beam_info *beam, int boost, quark_t note, struct monster *target_mon);
extern bool effect_simple(int index, struct source *origin, const char *dice_string, int subtype,
    int radius, int other, int y, int x, bool *ident);

#endif /* INCLUDED_EFFECTS_H */
