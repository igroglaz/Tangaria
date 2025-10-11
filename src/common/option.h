/*
 * File: option.h
 * Purpose: Options table and definitions.
 */

#ifndef INCLUDED_OPTIONS_H
#define INCLUDED_OPTIONS_H

/*
 * Option types
 */
enum
{
    OP_INTERFACE = 0,
    OP_MANGBAND,
    OP_BIRTH,
    OP_ADVANCED,
    OP_EXTRA,

    OP_MAX
};

/*
 * Option indexes
 */
enum
{
    #define OP(a, b, c, d, e) OPT_##a,
    #include "list-options.h"
    #undef OP
    OPT_MAX
};

#define OPT(P, opt_name) (P)->opts.opt[OPT_##opt_name]

/*
 * List of kinds of item, for pseudo-id and ego ignoring.
 */
typedef enum
{
    ITYPE_NONE,
    #define ITYPE(a, b) ITYPE_##a,
    #include "list-ignore-types.h"
    #undef ITYPE

    ITYPE_MAX
} ignore_type_t;

#define ITYPE_SIZE          FLAG_SIZE(ITYPE_MAX)

#define itype_has(f, flag)  flag_has_dbg(f, ITYPE_SIZE, flag, #f, #flag)
#define itype_on(f, flag)   flag_on_dbg(f, ITYPE_SIZE, flag, #f, #flag)
#define itype_wipe(f)       flag_wipe(f, ITYPE_SIZE)

/*
 * The option data structures
 */
struct player_options
{
    bool opt[OPT_MAX];              /* Options */
    uint8_t hitpoint_warn;             /* Hitpoint warning (0 to 9) */
    uint8_t hitpoint_warn_toggle;
    uint8_t lazymove_delay;            /* Delay in cs before moving to allow another keypress */
    uint8_t delay_factor;              /* Delay factor (0 to 255) */
    uint8_t ignore_lvl[ITYPE_MAX];     /* Auto-ignore level (0 to 4) */
};

extern int *option_page[OP_MAX];

/*
 * Functions
 */
extern const char *option_type_name(int page);
extern const char *option_name(int opt);
extern const char *option_desc(int opt);
extern int option_type(int opt);
extern bool option_normal(int opt);
extern bool option_server(int opt);
extern void option_init(void);
extern void option_free(void);

#endif /* INCLUDED_OPTIONS_H */
