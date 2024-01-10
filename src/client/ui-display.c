/*
 * File: ui-display.c
 * Purpose: Handles the setting up updating, and cleaning up of the game display.
 *
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 * Copyright (c) 2007 Antony Sidwell
 * Copyright (c) 2024 MAngband and PWMAngband Developers
 *
 * This work is free software; you can redistribute it and/or modify it
 * under the terms of either:
 *
 * a) the GNU General Public License as published by the Free Software
 *    Foundation, version 2, or
 *
 * b) the "Angband licence":
 *    This software may be copied and distributed for educational, research,
 *    and not for profit purposes provided that this copyright and statement
 *    are included in all such copies.  Other copyrights may also apply.
 */


#include "c-angband.h"


/* Maximum amount of "special" info */
int16_t max_line;


/* Current displayed line of "special" info */
int16_t cur_line;


/* Health bar parameters */
int health_amt;
uint8_t health_attr;


/* Lag bar parameters */
int lag_mark;


/* Chat channels */
int16_t view_channel = 0;


/* Remote info display */
cave_view_type remote_info[ANGBAND_TERM_MAX][MAX_TXT_INFO][NORMAL_WID];
int16_t last_remote_line[ANGBAND_TERM_MAX];


typedef struct
{
    uint32_t flag;
    game_event_type event;
} flag_event_trigger;


/*
 * There are a few functions installed to be triggered by several 
 * of the basic player events.  For convenience, these have been grouped 
 * in this list.
 */
static game_event_type player_events[] =
{
    EVENT_RACE_CLASS,
    EVENT_PLAYERTITLE,
    EVENT_EXPERIENCE,
    EVENT_PLAYERLEVEL,
    EVENT_GOLD,
    EVENT_EQUIPMENT,
    EVENT_STATS,
    EVENT_AC,
    EVENT_MANA,
    EVENT_HP,
    EVENT_MONSTERHEALTH,
    EVENT_PLAYERSPEED,
    EVENT_DUNGEONLEVEL,
    EVENT_PLUSSES,
    EVENT_OTHER,
    EVENT_LAG
};


static game_event_type statusline_events[] =
{
    EVENT_STATE,
    EVENT_STATUS,
    EVENT_DETECTIONSTATUS,
    EVENT_STUDYSTATUS
};


/* Monster subwindow witdh */
static int monwidth = -1;


//// Slash fx ////
bool sfx_effect = false; // slash effect
uint8_t sfx_dir = 0; // slash fx direction
uint16_t sfx_info_a[256][256]; // 'attr' info
char sfx_info_c[256][256]; // 'char' info
uint8_t sfx_info_d[256][256]; // direction info
uint8_t sfx_move[256][256]; // move timer


//// Animations ////
static uint16_t opt_anim_obj_w = 1;
static uint16_t opt_anim_obj = 1;
static uint16_t opt_anim_npc = 1;
static uint16_t s_obj[1024][256]; // search objects
static uint16_t anim_obj_a[1024][256]; // animate objects 'a'
static char anim_obj_c[1024][256]; // animate objects 'c'


/*** Sidebar display functions ***/


/*
 * Print character info at given row, column in a 13 char field
 */
static void prt_field(const char *info, int row, int col)
{
    /* Dump 13 spaces to clear */
    c_put_str(COLOUR_WHITE, "             ", row, col);

    /* Dump the info itself */
    c_put_str(COLOUR_L_BLUE, info, row, col);
}


/*
 * Print character stat in given row, column
 */
static void prt_stat(int stat, int row, int col)
{
    char tmp[32];

    /* Injured or healthy stat */
    if (player->state.stat_use[stat] < player->state.stat_top[stat])
    {
        put_str(stat_names_reduced[stat], row, col);
        cnv_stat(player->state.stat_use[stat], tmp, sizeof(tmp));
        c_put_str(COLOUR_YELLOW, tmp, row, col + 6);
    }
    else
    {
        put_str(stat_names[stat], row, col);
        cnv_stat(player->state.stat_use[stat], tmp, sizeof(tmp));
        c_put_str(COLOUR_L_GREEN, tmp, row, col + 6);
    }

    /* Indicate natural maximum */
    if (player->stat_max[stat] == 18+100)
        put_str("!", row, col + 3);
}


/*
 * Prints "title", including "wizard" or "winner" as needed.
 */
static void prt_title(int row, int col)
{
    /* Wizard, winner or neither */
    prt_field(title, row, col);
}


/*
 * Prints level
 */
static void prt_level(int row, int col)
{
    char tmp[32];

    strnfmt(tmp, sizeof(tmp), "%6d", player->lev);

    if (player->lev >= player->max_lev)
    {
        put_str("LEVEL ", row, col);
        c_put_str(COLOUR_L_GREEN, tmp, row, col + 6);
    }
    else
    {
        put_str("Level ", row, col);
        c_put_str(COLOUR_YELLOW, tmp, row, col + 6);
    }
}


/*
 * Display the experience
 */
static void prt_exp(int row, int col)
{
    char out_val[32];
    bool lev50 = (player->lev == PY_MAX_LEVEL);
    long xp = (long)player->exp;

    /* Calculate XP for next level */
    if (!lev50) xp = (long)adv_exp(player->lev, player->expfact) - xp;

    /* Format XP */
    strnfmt(out_val, sizeof(out_val), "%8ld", xp);

    if (player->exp >= player->max_exp)
    {
        put_str((lev50 ? "EXP" : "NXT"), row, col);
        c_put_str(COLOUR_L_GREEN, out_val, row, col + 4);
    }
    else
    {
        put_str((lev50 ? "Exp" : "Nxt"), row, col);
        c_put_str(COLOUR_YELLOW, out_val, row, col + 4);
    }
}


/*
 * Prints current gold
 */
static void prt_gold(int row, int col)
{
    char tmp[32];

    put_str("AU ", row, col);
    strnfmt(tmp, sizeof(tmp), "%9ld", (long)player->au);
    c_put_str(COLOUR_L_GREEN, tmp, row, col + 3);
}


/*
 * Equippy chars (ASCII representation of gear in equipment slot order)
 */
static void prt_equippy(int row, int col)
{
    int i;
    uint8_t a;
    char c;

    /* Dump equippy chars */
    for (i = 0; i < player->body.count; i++)
    {
        /* Get attr/char for display */
        a = player->hist_flags[0][i].a;
        c = player->hist_flags[0][i].c;

        /* Dump */
        Term_putch(col + i, row, a, c);
    }
}


/*
 * Prints current AC
 */
static void prt_ac(int row, int col)
{
    char tmp[32];

    put_str("Cur AC ", row, col);
    strnfmt(tmp, sizeof(tmp), "%5d", player->known_state.ac + player->known_state.to_a);
    c_put_str(COLOUR_L_GREEN, tmp, row, col + 7);
}


/*
 * Prints current hitpoints
 */
static void prt_hp(int row, int col)
{
    char cur_hp[32], max_hp[32];
    uint8_t color;

    put_str("HP ", row, col);

    strnfmt(max_hp, sizeof(max_hp), "%4d", player->mhp);
    strnfmt(cur_hp, sizeof(cur_hp), "%4d", player->chp);

    if (player->chp >= player->mhp)
        color = COLOUR_L_GREEN;
    else if (player->chp > (player->mhp * player->opts.hitpoint_warn) / 10)
        color = COLOUR_YELLOW;
    else
        color = COLOUR_RED;

    c_put_str(color, cur_hp, row, col + 3);
    c_put_str(COLOUR_WHITE, "/", row, col + 7);
    c_put_str(COLOUR_L_GREEN, max_hp, row, col + 8);
}


/*
 * Prints players max/cur spell points
 */
static void prt_sp(int row, int col)
{
    char cur_sp[32], max_sp[32];
    uint8_t color;

    /* Erase the mana display */
    Term_erase(col, row, 12);

    /* Do not show mana unless we should have some */
    if (!player->clazz->magic.total_spells || (player->lev < player->clazz->magic.spell_first))
    {
        /* But clear if experience drain may have left no points after having points. */
        if (player->clazz->magic.total_spells && player->exp < player->max_exp)
            put_str("            ", row, col);
        return;
    }

    put_str("SP ", row, col);

    strnfmt(max_sp, sizeof(max_sp), "%4d", player->msp);
    strnfmt(cur_sp, sizeof(cur_sp), "%4d", player->csp);

    if (player->csp >= player->msp)
        color = COLOUR_L_GREEN;
    else if (player->csp > (player->msp * player->opts.hitpoint_warn) / 10)
        color = COLOUR_YELLOW;
    else
        color = COLOUR_RED;

    /* Show mana */
    c_put_str(color, cur_sp, row, col + 3);
    c_put_str(COLOUR_WHITE, "/", row, col + 7);
    c_put_str(COLOUR_L_GREEN, max_sp, row, col + 8);
}


/*
 * Redraw the "monster health bar"
 *
 * The "monster health bar" provides visual feedback on the "health"
 * of the monster currently being "tracked".  There are several ways
 * to "track" a monster, including targeting it, attacking it, and
 * affecting it (and nobody else) with a ranged attack.  When nothing
 * is being tracked, we clear the health bar.  If the monster being
 * tracked is not currently visible, a special health bar is shown.
 */
static void prt_health(int row, int col)
{
    /* Not tracking */
    if (!health_attr)
    {
        /* Erase the health bar */
        Term_erase(col, row, 12);
    }

    /* Tracking something */
    else
    {
        /* Default to "unknown" */
        Term_putstr(col, row, 12, COLOUR_WHITE, "[----------]");

        /* Dump the current "health" (use '*' symbols) */
        Term_putstr(col + 1, row, health_amt, health_attr, "**********");
    }
}


/*
 * Redraw the lag bar
 */
static void prt_lag(int row, int col)
{
    uint8_t attr;

    /* Default to "unknown" */
    Term_erase(col, row, 12);
    Term_putstr(col, row, 12, COLOUR_L_DARK, "LAG:[------]");

    /* Time Out */
    if (lag_mark == 10)
    {
        c_msg_print("Time Out");
        attr = COLOUR_VIOLET;
    }

    /* Normal lag */
    else
    {
        attr = COLOUR_L_GREEN;
        if (lag_mark > 3) attr = COLOUR_YELLOW;
        if (lag_mark > 5) attr = COLOUR_RED;
    }

    /* Display */
    Term_putstr(col + 5, row, MIN(lag_mark, 6), attr, "******");
}  


/*
 * Prints the speed of a character.
 */
static void prt_speed(int row, int col)
{
    int16_t speed = get_speed(player);
    uint8_t attr = COLOUR_WHITE;
    const char *type = NULL;
    char buf[32] = "";

    /* 0 is normal speed, and requires no display */
    if (speed > 0)
    {
        attr = COLOUR_L_GREEN;
        type = "Fast";
    }
    else if (speed < 0)
    {
        attr = COLOUR_L_UMBER;
        type = "Slow";
    }

    if (type)
    {
        if (OPT(player, effective_speed))
        {
            int multiplier = player->state.ammo_mult;

            strnfmt(buf, sizeof(buf), "%s (%d.%dx)", type, multiplier / 10, multiplier % 10);
        }
        else
            strnfmt(buf, sizeof(buf), "%s (%+d)", type, speed);
    }

    /* Display the speed */
    c_put_str(attr, format("%-11s", buf), row, col);
}


/*
 * Prints depth of a character.
 */
static void prt_depth(int row, int col)
{
    /* Set the hooks */
    put_str_hook = Term_putstr;

    /* Display the depth */
    display_depth(player, row, col);
}


/*
 * Some simple wrapper functions
 */
static void prt_str(int row, int col) {prt_stat(STAT_STR, row, col);}
static void prt_dex(int row, int col) {prt_stat(STAT_DEX, row, col);}
static void prt_wis(int row, int col) {prt_stat(STAT_WIS, row, col);}
static void prt_int(int row, int col) {prt_stat(STAT_INT, row, col);}
static void prt_con(int row, int col) {prt_stat(STAT_CON, row, col);}
static void prt_chr(int row, int col) {prt_stat(STAT_CHR, row, col);}
static void prt_race(int row, int col) {prt_field(player->race->name, row, col);}
static void prt_class(int row, int col) {prt_field(player->clazz->name, row, col);}


/*
 * Struct of sidebar handlers.
 */
static const struct side_handler_t
{
    void (*hook)(int, int); /* int row, int col */
    int priority;           /* 1 is most important (always displayed) */
    game_event_type type;   /* ui_* event this corresponds to */
} side_handlers[] =
{
    {prt_race, 19, EVENT_RACE_CLASS},
    {prt_title, 18, EVENT_PLAYERTITLE},
    {prt_class, 22, EVENT_RACE_CLASS},
    {prt_level, 10, EVENT_PLAYERLEVEL},
    {prt_exp, 16, EVENT_EXPERIENCE},
    {prt_gold, 11, EVENT_GOLD},
    {prt_equippy, 17, EVENT_EQUIPMENT},
    {prt_str, 6, EVENT_STATS},
    {prt_int, 5, EVENT_STATS},
    {prt_wis, 4, EVENT_STATS},
    {prt_dex, 3, EVENT_STATS},
    {prt_con, 2, EVENT_STATS},
    {prt_chr, 1, EVENT_STATS},
    {NULL, 15, 0},
    {prt_ac, 7, EVENT_AC},
    {prt_hp, 8, EVENT_HP},
    {prt_sp, 9, EVENT_MANA},
    {NULL, 21, 0},
    {prt_health, 12, EVENT_MONSTERHEALTH},
    {prt_lag, 20, EVENT_LAG},
    {NULL, 23, 0},
    {prt_depth, 13, EVENT_DUNGEONLEVEL},
    {prt_speed, 14, EVENT_PLAYERSPEED}
};


/*
 * This prints the sidebar, using a clever method which means that it will only
 * print as much as can be displayed on <24-line screens.
 *
 * Each row is given a priority; the least important higher numbers and the most
 * important lower numbers.  As the screen gets smaller, the rows start to
 * disappear in the order of lowest to highest importance.
 */
static void update_sidebar(game_event_type type, game_event_data *data, void *user)
{
    int x, y, row;
    int max_priority;
    size_t i;

    /* Character is shopping */
    if (store_ctx) return;

    /* Player is currently looking at the full map */
    if (map_active) return;

    Term_get_size(&x, &y);

    /* Keep the top line clear */
    max_priority = y - 1;

    /* Display list entries */
    for (i = 0, row = 1; i < N_ELEMENTS(side_handlers); i++)
    {
        const struct side_handler_t *hnd = &side_handlers[i];

        /* If this is high enough priority, display it */
        if (hnd->priority <= max_priority)
        {
            if ((hnd->type == type) && hnd->hook)
            {
                bool cursor_icky = Term->cursor_icky;

                Term->cursor_icky = true;
                hnd->hook(row, 0);
                Term->cursor_icky = cursor_icky;
            }

            /* Increment for next time */
            row++;
        }
    }
}


/*** Status line display functions ***/


/*
 * Print the status line.
 */
static void update_statusline(game_event_type type, game_event_data *data, void *user)
{
    int row = Term->hgt - 1;
    int col = COL_MAP;
    bool cursor_icky = Term->cursor_icky;

    /* Character is shopping */
    if (store_ctx) return;

    Term->cursor_icky = true;

    /* Set the hooks */
    put_str_hook = Term_putstr;

    /* Clear the remainder of the line */
    prt("", row, col);

    /* Display the status line */
    display_statusline(player, row, col);

    Term->cursor_icky = cursor_icky;
}


/*** Utility display functions ***/


/*
 * Display the character on the screen (three different modes)
 *
 * The top two lines, and the bottom line (or two) are left blank.
 *
 * Mode 0 = standard display with skills/history
 * Mode 1 = special display with equipment flags
 * Mode 2 = special display with equipment flags (ESP flags)
 */
void display_player_screen(uint8_t mode)
{
    /* Set the hooks */
    clear_hook = Term_clear;
    region_erase_hook = region_erase;
    put_ch_hook = Term_putch;
    put_str_hook = Term_putstr;
    use_bigtile_hook = tile_distorted;

    /* Display the character on the screen */
    display_player(player, mode);
}


/*** Subwindow display functions ***/


/*
 * true when we're supposed to display the equipment in the inventory
 * window, or vice-versa.
 */
static bool flip_inven;


static void update_inven_subwindow(game_event_type type, game_event_data *data, void *user)
{
    term *old = Term;
    term *inv_term = user;

    /* Activate */
    Term_activate(inv_term);

    if (!flip_inven) show_inven(OLIST_WINDOW | OLIST_WEIGHT | OLIST_QUIVER, NULL);
    else show_equip(OLIST_WINDOW | OLIST_WEIGHT | OLIST_FLOOR, NULL);

    Term_fresh();

    /* Restore */
    Term_activate(old);
}


static void update_equip_subwindow(game_event_type type, game_event_data *data, void *user)
{
    term *old = Term;
    term *inv_term = user;

    /* Activate */
    Term_activate(inv_term);

    if (!flip_inven) show_equip(OLIST_WINDOW | OLIST_WEIGHT | OLIST_FLOOR, NULL);
    else show_inven(OLIST_WINDOW | OLIST_WEIGHT | OLIST_QUIVER, NULL);

    Term_fresh();
    
    /* Restore */
    Term_activate(old);
}


/*
 * Flip "inven" and "equip" in any sub-windows
 */
void toggle_inven_equip(void)
{
    term *old = Term;
    int i;

    /* Change the actual setting */
    flip_inven = !flip_inven;

    /* Redraw any subwindows showing the inventory/equipment lists */
    for (i = 0; i < ANGBAND_TERM_MAX; i++)
    {
        if (!angband_term[i]) continue;

        Term_activate(angband_term[i]);

        if (window_flag[i] & PW_INVEN)
        {
            if (!flip_inven) show_inven(OLIST_WINDOW | OLIST_WEIGHT | OLIST_QUIVER, NULL);
            else show_equip(OLIST_WINDOW | OLIST_WEIGHT | OLIST_FLOOR, NULL);
            Term_fresh();
        }
        else if (window_flag[i] & PW_EQUIP)
        {
            if (!flip_inven) show_equip(OLIST_WINDOW | OLIST_WEIGHT | OLIST_FLOOR, NULL);
            else show_inven(OLIST_WINDOW | OLIST_WEIGHT | OLIST_QUIVER, NULL);
            Term_fresh();
        }
    }

    Term_activate(old);
}


/*
 * Display player in sub-windows (mode 0)
 */
static void update_player0_subwindow(game_event_type type, game_event_data *data, void *user)
{
    term *old = Term;
    term *inv_term = user;

    /* Activate */
    Term_activate(inv_term);

    /* Display flags */
    display_player_screen(0);

    Term_fresh();

    /* Restore */
    Term_activate(old);
}


/*
 * Display player in sub-windows (mode 1)
 */
static void update_player1_subwindow(game_event_type type, game_event_data *data, void *user)
{
    term *old = Term;
    term *inv_term = user;

    /* Activate */
    Term_activate(inv_term);

    /* Display flags */
    display_player_screen(1);

    Term_fresh();

    /* Restore */
    Term_activate(old);
}


/*
 * Display the left-hand-side of the main term, in more compact fashion.
 */
static void update_player_compact_subwindow(game_event_type type, game_event_data *data, void *user)
{
    int row = 0;
    int col = 0;
    int i;
    term *old = Term;
    term *inv_term = user;

    /* Activate */
    Term_activate(inv_term);

    /* Race, Title, Class */
    prt_field(player->race->name, row++, col);
    prt_title(row++, col);
    prt_field(player->clazz->name, row++, col);

    /* Level/Experience */
    prt_level(row++, col);
    prt_exp(row++, col);

    /* Gold */
    prt_gold(row++, col);

    /* Equippy chars */
    prt_equippy(row++, col);

    /* All Stats */
    for (i = 0; i < STAT_MAX; i++) prt_stat(i, row++, col);

    /* Empty row */
    row++;

    /* Armor */
    prt_ac(row++, col);

    /* Hitpoints */
    prt_hp(row++, col);

    /* Spellpoints */
    prt_sp(row++, col);

    /* Empty row */
    row++;

    /* Monster health */
    prt_health(row++, col);

    /* Empty row */
    row++;

    /* Depth, Speed */
    prt_depth(row++, col);
    prt_speed(row, col);

    Term_fresh();

    /* Restore */
    Term_activate(old);
}


#define TYPE_BROADCAST(type) \
    (((type) >= MSG_BROADCAST_ENTER_LEAVE) && ((type) <= MSG_BROADCAST_STORE))


static void update_messages_subwindow(game_event_type type, game_event_data *data, void *user)
{
    term *old = Term;
    term *inv_term = user;
    int i;
    int w, h;
    int x, y;
    const char *msg;
    int line = 0;

    /* Activate */
    Term_activate(inv_term);

    /* Get size */
    Term_get_size(&w, &h);

    /* Dump messages */
    for (i = 0; line < h; i++)
    {
        uint8_t color = message_color(i);
        uint16_t count = message_count(i);
        const char *str = message_str(i);
        uint16_t type = message_type(i);

        if (count == 1) msg = str;
        else if (count == 0) msg = " ";
        else msg = format("%s <%dx>", str, count);

        /* Hack -- re-color message from string template */
        message_color_hack(msg, &color);

#if defined(USE_GCU) || defined(USE_SDL) || defined(USE_SDL2)
        if (term_chat && ((type >= MSG_WHISPER) || TYPE_BROADCAST(type))) continue;
#else
        if (term_chat->user && ((type >= MSG_WHISPER) || TYPE_BROADCAST(type))) continue;
#endif

        /* No wrapping, do it the old way */
        if (!OPT(player, wrap_messages))
        {
            /* Dump the message on the appropriate line */
            Term_putstrex(0, (h - 1) - line, -1, color, msg);

            /* Cursor */
            Term_locate(&x, &y);

            /* Clear to end of line */
            Term_erase(x, y, 255);

            line++;
        }

        /* Dump the message on the appropriate line(s) */
        else
            line += prt_multi(0, (h - 1) - line, -1, -(h - line), color, msg, true);
    }

    Term_fresh();

    /* Restore */
    Term_activate(old);
}


static void update_message_chat_subwindow(game_event_type type, game_event_data *data, void *user)
{
    term *old = Term;
    term *inv_term = user;
    int i;
    int w, h;
    int x = 0, y = 0;
    const char *msg;
    int line = 0, l = 0;
    int xoff = 0, yoff = 0; /* Hor. & Vert. Offsets */
    int tab;
    char text[MSG_LEN];
    message_iter iter;

    /* Activate */
    Term_activate(inv_term);

    /* Get size */
    Term_get_size(&w, &h);

    /* Dump header */
    for (i = 0; i < MAX_CHANNELS; i++)
    {
        uint8_t a;

        /* Skip empty */
        if (STRZERO(channels[i].name)) continue;

        /* Color */
        a = COLOUR_L_DARK;
        if (player->on_channel[i] == 1) a = COLOUR_WHITE;
        if (view_channel == i) a = COLOUR_L_BLUE;

        /* Carriage return */
        if ((int)strlen(channels[i].name) + xoff + 1 >= w)
        {
            /* Clear to end of line */
            Term_erase(x, y, 255);

            xoff = 0;
            yoff++;
        }

        /* Dump the message on the appropriate line */
        Term_putstr(0 + xoff, 0 + yoff, -1, a, channels[i].name);

        /* Whitespace */
        Term_locate(&x, &y);
        Term_putstr(x, y, -1, COLOUR_WHITE, " ");

        Term_locate(&x, &y);
        xoff = x;
    }

    /* Clear to end of line */
    Term_erase(x, y, 255);

    /* Dump messages in an efficient way (using an iterator) */
    for (message_first(&iter); l < h - (yoff + 1); message_next(&iter))
    {
        uint8_t color = iter.color;
        uint16_t count = iter.count;
        const char *str = iter.str;
        uint16_t type = iter.type;

        /* No message */
        if (str[0] == 0)
        {
            l++;
            continue;
        }

        if (count <= 1) msg = str;
        else msg = format("%s <%dx>", str, count);

        /* Hack -- re-color message from string template */
        message_color_hack(msg, &color);

        /* Filters */
        if (type == MSG_WHISPER)
        {
            tab = find_whisper_tab(msg, text, sizeof(text));
            if (tab && tab != view_channel) continue;
            if (tab) msg = text;
        }
        else if (type >= MSG_CHAT)
        {
            if ((type - MSG_CHAT) != channels[view_channel].id) continue;
        }
        else if (type == MSG_TALK)
        {
            /* Hack -- "&say" */
            tab = find_whisper_tab("&say", text, sizeof(text));
            if (!tab || tab != view_channel) continue;
        }
        else if (type == MSG_YELL)
        {
            /* Hack -- "&yell" */
            tab = find_whisper_tab("&yell", text, sizeof(text));
            if (!tab || tab != view_channel) continue;
        }
        else if (!TYPE_BROADCAST(type))
            continue;

        l++;

        /* Dump the message on the appropriate line(s) */
        line += prt_multi(0, (h - 1) - line, -1, -(h - 1 - (yoff + 1) - line), color, msg, false);
    }

    /* Erase rest */
    while (line < h - (yoff + 1))
    {
        /* Clear line */
        Term_erase(0, (h - 1) - line, 255);
        line++;
    }

    Term_fresh();

    /* Restore */
    Term_activate(old);
}


/*
 * Hack -- display dungeon map view in sub-windows.
 */
static void update_minimap_subwindow(game_event_type type, game_event_data *data, void *user)
{
    int y;
    int w, h;
    term *old = Term;
    term *inv_term = user;

    /* Activate */
    Term_activate(inv_term);

    /* This signals a whole-map redraw. */
    if ((data->point.x == -1) && (data->point.y == -1))
    {
        /* Get size */
        Term_get_size(&w, &h);

        /* Print map */
        for (y = 0; y <= last_remote_line[NTERM_WIN_MAP]; y++)
            caveprt(remote_info[NTERM_WIN_MAP][y], w, 0, y);

        /* Erase rest */
        clear_from(last_remote_line[NTERM_WIN_MAP] + 1);
    }

    /* Single point to be redrawn */
    else
        caveprt(remote_info[NTERM_WIN_MAP][data->point.y], 1, data->point.x, data->point.y);

    /* Refresh the main screen */
    Term_fresh();

    /* Restore */
    Term_activate(old);
}


/*
 * Print the status display subwindow
 */
static void update_status_subwindow(game_event_type type, game_event_data *data, void *user)
{
    int row = 0;
    int col = 0;
    term *old = Term;
    term *inv_term = user;

    /* Activate */
    Term_activate(inv_term);

    /* Set the hooks */
    put_str_hook = Term_putstr;

    display_status_subwindow(player, row, col);

    Term_fresh();

    /* Restore */
    Term_activate(old);
}


/*
 * Hack -- display some recall in some sub-windows
 */
static void fix_remote_term(uint8_t rterm)
{
    int y;
    int w, h;
    int last = last_remote_line[rterm];

    /* Get size */
    Term_get_size(&w, &h);

    /* Display special title */
    Term_erase(0, 0, 255);
    c_put_str(COLOUR_YELLOW, special_line_header[rterm], 0, 0);
    Term_erase(0, 1, 255);

    /* Print data */
    for (y = 0; y <= last; y++)
        caveprt(remote_info[rterm][y], w, 0, y + 2);

    /* Erase rest */
    for (y = last + 1; y <= h - 2; y++) Term_erase(0, y + 2, 255);
}


static void update_object_subwindow(game_event_type type, game_event_data *data, void *user)
{
    term *old = Term;
    term *inv_term = user;

    /* Activate */
    Term_activate(inv_term);

    fix_remote_term(NTERM_WIN_OBJECT);

    Term_fresh();

    /* Restore */
    Term_activate(old);
}


static void update_monster_subwindow(game_event_type type, game_event_data *data, void *user)
{
    term *old = Term;
    term *inv_term = user;

    /* Activate */
    Term_activate(inv_term);

    fix_remote_term(NTERM_WIN_MONSTER);

    Term_fresh();

    /* Restore */
    Term_activate(old);
}


static void update_itemlist_subwindow(game_event_type type, game_event_data *data, void *user)
{
    term *old = Term;
    term *inv_term = user;

    /* Activate */
    Term_activate(inv_term);

    fix_remote_term(NTERM_WIN_OBJLIST);

    Term_fresh();

    /* Restore */
    Term_activate(old);
}


static void update_monlist_subwindow(game_event_type type, game_event_data *data, void *user)
{
    term *old = Term;
    term *inv_term = user;

    /* Activate */
    Term_activate(inv_term);

    /* Send new width for dynamic resizing (but only if it changed!) */
    if (Term->wid != monwidth)
    {
        Send_monwidth(Term->wid);
        monwidth = Term->wid;
    }

    fix_remote_term(NTERM_WIN_MONLIST);

    Term_fresh();

    /* Restore */
    Term_activate(old);
}


static void update_special_info_subwindow(game_event_type type, game_event_data *data, void *user)
{
    term *old = Term;
    term *inv_term = user;

    /* Activate */
    Term_activate(inv_term);

    fix_remote_term(NTERM_WIN_SPECIAL);

    Term_fresh();

    /* Restore */
    Term_activate(old);
}


static int dump_spells(int book, int y, int col)
{
    int i = 0;
    int w, h;
    uint8_t line_attr;
    char out_val[160];

    /* Get size */
    Term_get_size(&w, &h);

    /* Check for end of the book */
    while (book_info[book].spell_info[i].info[0] != '\0')
    {
        /* End of terminal */
        if (y >= h) break;

        /* Dump the info */
        line_attr = book_info[book].spell_info[i].flag.line_attr;
        if ((line_attr == COLOUR_WHITE) || (line_attr == COLOUR_L_GREEN))
        {
            strnfmt(out_val, sizeof(out_val), "%c-%c) %s", I2A(book), I2A(i),
                book_info[book].spell_info[i].info);
            c_prt(line_attr, out_val, y, col);
            y++;
        }

        i++;
    }

    return y;
}


/*
 * Hack -- display spell list in sub-windows.
 */
static void update_spell_subwindow(game_event_type type, game_event_data *data, void *user)
{
    int y = 0, col;
    term *old = Term;
    term *inv_term = user;

    /* Activate */
    Term_activate(inv_term);

    /* Print column */
    col = 1;

    /* Header */
    prt("", y, col);
    put_str("Name                          Lv Mana Fail Info", y, col + 5);
    y++;

    /* Ghost abilities */
    if (player->ghost && !player_can_undead(player))
        y = dump_spells(0, y, col);

    /* Monster spells */
    else if (player_has(player, PF_MONSTER_SPELLS))
    {
        int page;

        for (page = 0; page < MAX_PAGES; page++)
            y = dump_spells(page, y, col);
    }

    /* Normal spells */
    else
    {
        int book;

        for (book = 0; book < player->clazz->magic.num_books; book++)
            y = dump_spells(book, y, col);
    }

    /* Erase rest */
    clear_from(y);

    Term_fresh();

    /* Restore */
    Term_activate(old);
}


/*** Generic "deal with" functions ***/


/*
 * Events triggered by the various flags.
 */
static const flag_event_trigger redraw_events[] =
{
    {PR_MISC, EVENT_RACE_CLASS},
    {PR_TITLE, EVENT_PLAYERTITLE},
    {PR_LEV, EVENT_PLAYERLEVEL},
    {PR_EXP, EVENT_EXPERIENCE},
    {PR_STATS, EVENT_STATS},
    {PR_ARMOR, EVENT_AC},
    {PR_HP, EVENT_HP},
    {PR_MANA, EVENT_MANA},
    {PR_GOLD, EVENT_GOLD},
    {PR_OTHER, EVENT_OTHER},
    {PR_ITEMLIST, EVENT_ITEMLIST},
    {PR_HEALTH, EVENT_MONSTERHEALTH},
    {PR_SPEED, EVENT_PLAYERSPEED},
    {PR_STUDY, EVENT_STUDYSTATUS},
    {PR_DEPTH, EVENT_DUNGEONLEVEL},
    {PR_STATUS, EVENT_STATUS},
    {PR_DTRAP, EVENT_DETECTIONSTATUS},
    {PR_STATE, EVENT_STATE},
    {PR_INVEN, EVENT_INVENTORY},
    {PR_EQUIP, EVENT_EQUIPMENT},
    {PR_MESSAGE, EVENT_MESSAGE},
    {PR_MONSTER, EVENT_MONSTERTARGET},
    {PR_OBJECT, EVENT_OBJECTTARGET},
    {PR_MONLIST, EVENT_MONSTERLIST},
    {PR_MESSAGE_CHAT, EVENT_MESSAGE_CHAT},
    {PR_SPELL, EVENT_SPELL},
    {PR_SPECIAL_INFO, EVENT_SPECIAL_INFO},
    {PR_LAG, EVENT_LAG},
    {PR_PLUSSES, EVENT_PLUSSES}
};


/*
 * Handle "player->upkeep->redraw"
 */
void redraw_stuff(void)
{
    size_t i;
    uint32_t redraw = player->upkeep->redraw;

    /* Redraw stuff */
    if (!redraw) return;

    /* Map is not shown, subwindow updates only (except when shopping) */
    if (player->screen_save_depth && !store_ctx)
    {
        redraw &= PR_SUBWINDOW;

        /* Don't redraw monster list during monster detection */
        if (player->mlist_icky) redraw &= ~PR_MONLIST;
    }

    /* For each listed flag, send the appropriate signal to the UI */
    for (i = 0; i < N_ELEMENTS(redraw_events); i++)
    {
        const flag_event_trigger *hnd = &redraw_events[i];
        if (redraw & hnd->flag)
            event_signal(hnd->event);
    }

    /* Then the ones that require parameters to be supplied. */
    if (redraw & PR_MAP)
    {
        /* Mark the whole map to be redrawn */
        event_signal_point(EVENT_MAP, -1, -1);
    }

    player->upkeep->redraw &= ~redraw;

    /* Map is not shown, subwindow updates only (except when shopping) */
    if (player->screen_save_depth && !store_ctx) return;

    /* Do any plotting, etc. delayed from earlier - this set of updates is over. */
    event_signal(EVENT_END);
}


/*
 * Certain "screens" always use the main screen, including News, Birth,
 * Dungeon, Tomb-stone, High-scores, Macros, Colors, Visuals, Options.
 *
 * Later, special flags may allow sub-windows to "steal" stuff from the
 * main window, including File dump (help), File dump (artifacts, uniques),
 * Character screen, Small scale map, Previous Messages, Store screen, etc.
 */
const char *window_flag_desc[PW_MAX_FLAGS] =
{
    "Display inven/equip",
    "Display equip/inven",
    "Display player (basic)",
    "Display player (extra)",
    "Display player (compact)",
    "Display map view",
    "Display messages",
    NULL, /* "Display overhead view", */
    "Display monster recall",
    "Display object recall",
    "Display monster list",
    "Display status",
    "Display chat messages",
    "Display spell list",
    "Display item list",
    "Display special info"
};


static void subwindow_flag_changed(int win_idx, uint32_t flag, bool new_state)
{
    void (*register_or_deregister)(game_event_type type, game_event_handler *fn, void *user);
    void (*set_register_or_deregister)(game_event_type *type, size_t n_events,
        game_event_handler *fn, void *user);

    /* Skip the main window */
    if (win_idx == 0) return;

    /* Decide whether to register or deregister an event handler */
    if (new_state == false)
    {
        register_or_deregister = event_remove_handler;
        set_register_or_deregister = event_remove_handler_set;
    }
    else
    {
        register_or_deregister = event_add_handler;
        set_register_or_deregister = event_add_handler_set;
    }

    switch (flag)
    {
        case PW_INVEN:
        {
            register_or_deregister(EVENT_INVENTORY, update_inven_subwindow, angband_term[win_idx]);
            break;
        }

        case PW_EQUIP:
        {
            register_or_deregister(EVENT_EQUIPMENT, update_equip_subwindow, angband_term[win_idx]);
            break;
        }

        case PW_PLAYER_0:
        {
            set_register_or_deregister(player_events, N_ELEMENTS(player_events),
                update_player0_subwindow, angband_term[win_idx]);
            break;
        }

        case PW_PLAYER_1:
        {
            set_register_or_deregister(player_events, N_ELEMENTS(player_events),
                update_player1_subwindow, angband_term[win_idx]);
            break;
        }

        case PW_PLAYER_2:
        {
            set_register_or_deregister(player_events, N_ELEMENTS(player_events),
                update_player_compact_subwindow, angband_term[win_idx]);
            break;
        }

        case PW_MAP:
        {
            register_or_deregister(EVENT_MAP, update_minimap_subwindow, angband_term[win_idx]);
            angband_term[win_idx]->minimap_active = new_state;
            break;
        }

        case PW_MESSAGE:
        {
            register_or_deregister(EVENT_MESSAGE, update_messages_subwindow, angband_term[win_idx]);
            break;
        }

        case PW_MONSTER:
        {
            register_or_deregister(EVENT_MONSTERTARGET, update_monster_subwindow,
                angband_term[win_idx]);
            break;
        }

        case PW_OBJECT:
        {
            register_or_deregister(EVENT_OBJECTTARGET, update_object_subwindow,
                angband_term[win_idx]);
            break;
        }

        case PW_MONLIST:
        {
            register_or_deregister(EVENT_MONSTERLIST, update_monlist_subwindow,
                angband_term[win_idx]);
            break;
        }

        case PW_STATUS:
        {
            set_register_or_deregister(statusline_events, N_ELEMENTS(statusline_events),
                update_status_subwindow, angband_term[win_idx]);
            break;
        }

        case PW_MESSAGE_CHAT:
        {
            register_or_deregister(EVENT_MESSAGE_CHAT, update_message_chat_subwindow,
                angband_term[win_idx]);
            break;
        }

        case PW_SPELL:
        {
            register_or_deregister(EVENT_SPELL, update_spell_subwindow, angband_term[win_idx]);
            break;
        }

        case PW_ITEMLIST:
        {
            register_or_deregister(EVENT_ITEMLIST, update_itemlist_subwindow, angband_term[win_idx]);
            break;
        }

        case PW_SPECIAL_INFO:
        {
            register_or_deregister(EVENT_SPECIAL_INFO, update_special_info_subwindow,
                angband_term[win_idx]);
            break;
        }
    }
}


/*
 * Set the flags for one Term, calling "subwindow_flag_changed" with each flag that
 * has changed setting so that it can do any housekeeping to do with 
 * displaying the new thing or no longer displaying the old one.
 */
static void subwindow_set_flags(int win_idx, uint32_t new_flags)
{
    term *old = Term;
    int i;

    /* Deal with the changed flags by seeing what's changed */
    for (i = 0; i < PW_MAX_FLAGS; i++)
    {
        /* Only process valid flags */
        if (window_flag_desc[i])
        {
            uint32_t flag = ((uint32_t)1) << i;

            if ((new_flags & flag) != (window_flag[win_idx] & flag))
                subwindow_flag_changed(win_idx, flag, (new_flags & flag) != 0);
        }
    }

    /* Store the new flags */
    window_flag[win_idx] = new_flags;

    /* Activate */
    Term_activate(angband_term[win_idx]);

    /* Hack -- request resize for dungeon */
    if (win_idx == 0) net_term_resize(0, 0, 0);
    
    /* Erase */
    Term_clear();
    
    /* Refresh */
    Term_fresh();

    /* Restore */
    Term_activate(old);
}


/*
 * Called with an array of the new flags for all the subwindows, in order
 * to set them to the new values, with a chance to perform housekeeping.
 */
void subwindows_set_flags(uint32_t *new_flags, size_t n_subwindows)
{
    size_t j;

    /* Update all windows */
    for (j = 0; j < n_subwindows; j++)
    {
        /* Dead window */
        if (!angband_term[j]) continue;

        /* Ignore non-changes */
        if (window_flag[j] != new_flags[j])
            subwindow_set_flags(j, new_flags[j]);
    }
}


void subwindows_init_flags(void)
{
    size_t j;
    int i;

    /* Initialize all windows */
    for (j = 0; j < ANGBAND_TERM_MAX; j++)
    {
        /* Dead window */
        if (!angband_term[j]) continue;

        /* Reinitialize all flags */
        for (i = 0; i < PW_MAX_FLAGS; i++)
        {
            /* Only process valid flags */
            if (!window_flag_desc[i]) continue;

            subwindow_flag_changed(j, (1L << i), (window_flag[j] & (1L << i)) != 0);
        }
    }
}


void subwindows_reinit_flags(void)
{
    size_t j;
    int i;

    /* Reinitialize all windows */
    for (j = 0; j < ANGBAND_TERM_MAX; j++)
    {
        /* Dead window */
        if (!angband_term[j]) continue;

        /* Reinitialize all flags */
        for (i = 0; i < PW_MAX_FLAGS; i++)
        {
            /* Only process valid flags */
            if (!window_flag_desc[i]) continue;

            subwindow_flag_changed(j, (1L << i), false);
        }
    }
}


/*** Initializing ***/


void init_display(void)
{
    /* Because of the "flexible" sidebar, all these things trigger the same function. */
    event_add_handler_set(player_events, N_ELEMENTS(player_events),
        update_sidebar, term_screen);

    /* The flexible statusbar has similar requirements, so is also trigger by a large set of events. */
    event_add_handler_set(statusline_events, N_ELEMENTS(statusline_events),
        update_statusline, term_screen);

    /* Display a message and make a noise to the player */
    event_add_handler(EVENT_BELL, bell_message, NULL);

    /* Tell the UI to ignore all pending input */
    event_add_handler(EVENT_INPUT_FLUSH, flush, NULL);
}


/* Determine message color based on string templates */
void message_color_hack(const char *msg, uint8_t *ap)
{
    char from_us[30];

    /* Determine what messages from us are prefixed with */
    strnfmt(from_us, sizeof(from_us), "[%s]", player->name);

    if (msg[0] == '[')
    {
        *ap = COLOUR_L_BLUE;
        if ((strstr(msg, player->name) != NULL) && (strstr(msg, from_us) == NULL))
            *ap = COLOUR_L_GREEN;
    }
}


/*
 * When we got a private message in format "[Recepient:Sender] Message"
 * this function could be used to determine if it relates to any of the
 * chat tabs opened
 */
int find_whisper_tab(const char *msg, char *text, size_t len)
{
    char from_us[30], to_us[30], buf[NORMAL_WID];
    int i, tab = 0;
    const char *offset, *pmsg;

    buf[0] = '\0';
    strnfmt(from_us, sizeof(from_us), ":%s]", player->name);
    strnfmt(to_us, sizeof(to_us), "[%s:", player->name);

    /* Message From Us */
    if ((offset = strstr(msg, from_us)) != NULL)
    {
        /* To who */
        my_strcpy(buf, msg + 1, sizeof(buf));
        buf[offset - msg - 1] = '\0';

        /* Short text */
        pmsg = msg + (offset - msg) + strlen(from_us) + 1;
        strnfmt(text, len, "[%s] %s", player->name, pmsg);
    }

    /* Message To Us */
    else if (strstr(msg, to_us) != NULL)
    {
        /* From who */
        my_strcpy(buf, msg + strlen(to_us), sizeof(buf));
        offset = strstr(msg, "]");
        buf[offset - msg - strlen(to_us)] = '\0';

        /* Short text */
        strnfmt(text, len, "[%s] %s", buf, offset + 2);
    }

    /* Some other kind of message (probably to Your Party) */
    else if ((offset = strstr(msg, ":")) != NULL)
    {
        /* Destination */
        my_strcpy(buf, msg + 1, sizeof(buf));
        buf[offset - msg - 1] = '\0';

        /* Sender */
        my_strcpy(from_us, offset + 1, sizeof(buf));
        pmsg = strstr(offset, "]");
        from_us[pmsg - offset - 1] = '\0';

        /* Short text */
        pmsg = msg + (pmsg - msg) + 2;
        strnfmt(text, len, "[%s] %s", from_us, pmsg);
    }

    /* Hack -- "&xxx" */
    else if (msg[0] == '&')
    {
        /* Dest. */
        my_strcpy(buf, msg, sizeof(buf));
        buf[strlen(msg)] = '\0';
    }

    if (STRZERO(buf)) return 0;

    /* Find related tab */
    for (i = 0; i < MAX_CHANNELS; i++)
    {
        if (STRZERO(channels[i].name)) continue;
        if (channels[i].id != MAX_CHANNELS) continue;
        if (strcmp(channels[i].name, buf)) continue;

        tab = i;
        break;
    }

    return tab;
}


/*
 * Show the splash screen.
 */
void show_splashscreen(void)
{
    int i, yoffset, xoffset;
    ui_event ch;

    /* Hack -- full icky screen */
    full_icky_screen = conf_get_int("MAngband", "FullIckyScreen", 0);
    player->screen_save_depth++;

    /* Clear the screen */
    Term_clear();

    /* Center the splashscreen -- assume news.txt has width 80, height 23 */
    xoffset = (Term->wid - 80) / 2;
    yoffset = (Term->hgt - 23) / 5;

    /* Show each line */
    for (i = 0; i < TEXTFILE__HGT; i++)
        text_out_e(&Setup.text_screen[TEXTFILE_MOTD][i * TEXTFILE__WID], i + yoffset, xoffset);

    Term_putstr(Term->wid / 2 - 12, Term->hgt - 1, -1, COLOUR_WHITE, "Press SPACE key to continue");

    /* Show it */
    Term_fresh();

    /* Wait for a keypress */
    do
    {
        Term_inkey(&ch, true, true);
    }
    while (ch.key.code != ' ');

    player->screen_save_depth--;

    /* Clear the screen again */
    Term_clear();

    player->locname[0] = '\0';
}


/*
 * Peruse a file sent by the server.
 */
bool peruse_file(void)
{
    ui_event ke;
    bool more = true;
    int max_hgt = Client_setup.settings[SETTING_MAX_HGT];

    /* Initialize */
    cur_line = 0;
    max_line = 0;

    /* Save the screen */
    screen_save();

    /* The top line is "icky" */
    topline_icky = true;

    /* Show the stuff */
    while (more)
    {
        /* Send the command */
        Send_special_line(special_line_type, cur_line);

        /* Get a keypress */
        ke = inkey_ex();

        /* Exit on abort */
        if (is_abort(ke)) break;

        /* Hack -- make any key escape if we're in popup mode */
        if ((max_line < max_hgt - 4) && (special_line_type == SPECIAL_FILE_OTHER))
        {
            ke.type = EVT_KBRD;
            ke.key.code = ESCAPE;
            ke.key.mods = 0;
        }

        if (ke.type == EVT_KBRD)
        {
            switch (ke.key.code)
            {
                /* Go to a specific line */
                case '#':
                {
                    char tmp[NORMAL_WID];
                    int res;

                    prt("Goto Line: ", max_hgt - 1, 0);
                    my_strcpy(tmp, "0", sizeof(tmp));
                    res = askfor_ex(tmp, sizeof(tmp), NULL, false);
                    if (res == 1) more = false;
                    else if (!res) cur_line = atoi(tmp);

                    break;
                }

                /* Up a line */
                case ARROW_UP:
                case '8':
                {
                    cur_line--;
                    if (cur_line < 0) cur_line = 0;
                    break;
                }

                /* Up a page */
                case KC_PGUP:
                case '9':
                case '-':
                {
                    cur_line -= 20;
                    if (cur_line < 0) cur_line = 0;
                    break;
                }

                /* Home */
                case KC_HOME:
                case '7':
                {
                    cur_line = 0;
                    break;
                }

                /* Down a line */
                case ARROW_DOWN:
                case '2':
                case KC_ENTER:
                {
                    cur_line++;
                    break;
                }

                /* Down a page */
                case KC_PGDOWN:
                case '3':
                case ' ':
                {
                    cur_line += 20;
                    break;
                }

                /* End */
                case KC_END:
                case '1':
                {
                    if (max_line)
                    {
                        cur_line = max_line - 20;
                        if (cur_line < 0) cur_line = 0;
                    }
                    break;
                }
            }
        }

        /* Exit on escape */
        if (is_escape(ke)) break;

        /* Check maximum line */
        if ((cur_line > max_line) || (cur_line < 0))
            cur_line = 0;
    }

    /* Tell the server we're done looking */
    Send_special_line(SPECIAL_FILE_NONE, 0);

    /* No longer using file perusal */
    special_line_type = SPECIAL_FILE_NONE;

    /* The top line is OK again */
    topline_icky = false;

    /* Restore the screen */
    screen_load(true);

    /* Flush messages */
    c_msg_print(NULL);

    /* Did we get kicked out of a store? */
    check_store_leave(true);

    return !is_abort(ke);
}


//////////////////////////////////////////////////
// Handle weather (rain and snow) client-side
// weather_type - stop(255)/none(0)/rain(1)/snow(2)
// weather_wind - current gust of wind 
//                (1 west, 2 east, 3 strong west, 4 strong east)
// weather_intensity - density of raindrops/snowflakes low(1)/med(2)/hi(3)
//
void do_weather(void)
{
    term *main_term = angband_term[0];
    term *old = Term;

    int i, j;
    int w, h;
    uint16_t w_attr;
    char w_char;
    uint16_t a;
    char c;
    uint16_t ta;
    char tc;

    // number of weather elements
    const int weather_elements = 1024;
    static int weather_element_x[1024];
    static int weather_element_y[1024];
    static int weather_frame_x[1024];
    static int weather_frame_y[1024];

    static int weather_speed_ticks = 0;
    static int weather_strength = 0;
    static bool weather_clear = true;
    bool make_weather = false;
    bool skip_weather = false;

    // Check weather
    if (player->weather_type == 0) return;

    // Stop weather (weather_display = no)
    if (player->weather_type == 255 && !OPT(player, weather_display))
    {
        player->weather_type = 0;
        weather_strength = 0;
        weather_clear = true;
        return;
    }

    // Show weather - options
    if (!OPT(player, weather_display)) return;

    // Hack -- if the screen is already icky, ignore this command
    if (player->screen_save_depth) return;

    // Weather speed
    weather_speed_ticks++;

    // Rain speed
    if (player->weather_type == 1)
    {
        // check to see if it has been 40ms
        if (weather_speed_ticks > 4) weather_speed_ticks = 0;
        else return;
    }
    // Snow speed
    else if (player->weather_type == 2)
    {
        // check to see if it has been 100ms
        if (weather_speed_ticks > 10) weather_speed_ticks = 0;
        else return;
    }

    // Activate the term
    Term_activate(main_term);

    // Get size
    w = (main_term->wid - COL_MAP - 1) / tile_width;
    h = (main_term->hgt - ROW_MAP - 1) / tile_height;

    if (weather_clear)
    {
        // Clear array weather elements
        for (i = 0; i < weather_elements; i++)
        {
            weather_element_x[i] = -1;
            weather_element_y[i] = -1;
            weather_frame_x[i] = -1;
            weather_frame_y[i] = -1;
        }

        weather_clear = false;
    }

    //// Redraw - restore old tile before moving the weather element
    for (i = 0; i < weather_elements; i++)
    {
        // Only for elements within visible panel screen area
        if (weather_frame_x[i] >= 0 && weather_frame_x[i] < w &&
            weather_frame_y[i] >= 0 && weather_frame_y[i] < h)
        {
            // Check characters
            Term_info(COL_MAP + weather_frame_x[i] * tile_width, 
                ROW_MAP + weather_frame_y[i] * tile_height, &a, &c, &ta, &tc);

            // Display
            if (use_graphics)
            {
                // Check background tile
                if (a == ta && c == tc)
                    (void)((*main_term->pict_hook)(COL_MAP + weather_frame_x[i] * tile_width, 
                        ROW_MAP + weather_frame_y[i] * tile_height, 1, &a, &c, &ta, &tc));
            }
            else
            {
                if (c != '@')
                    (void)((*main_term->text_hook)(COL_MAP + weather_frame_x[i], 
                        ROW_MAP + weather_frame_y[i], 1, a, &c));
            }
        }

        // Clear weather_frame array
        if (weather_frame_x[i] != -1 && weather_frame_y[i] != -1)
        {
            weather_frame_x[i] = -1;
            weather_frame_y[i] = -1;
        }
    }

    // Stop weather (weather_display = yes)
    if (player->weather_type == 255 && OPT(player, weather_display))
    {
        player->weather_type = 0;
        weather_strength = 0;
        weather_clear = true;
        return;
    }

    //// Weather ////
    switch (player->weather_type)
    {
        // Rain
        case 1:
        {
            switch (player->weather_wind)
            {
                // Wind - west
                case 1:
                {
                    if (use_graphics)
                    {
                        w_attr = 0x82;
                        switch (randint1(5))
                        {
                            case 1: w_char = '\xdf'; break;
                            case 2: w_char = '\xe0'; break;
                            case 3: w_char = '\xe1'; break;
                            case 4: w_char = '\xe2'; break;
                            case 5: w_char = '\xe3'; break;
                        }
                    }
                    else
                    {
                        w_attr = COLOUR_L_BLUE;
                        w_char = '/';
                    }
                    break;
                }
                // Wind - east
                case 2:
                {
                    if (use_graphics)
                    {
                        w_attr = 0x82;
                        switch (randint1(5))
                        {
                            case 1: w_char = '\xda'; break;
                            case 2: w_char = '\xdb'; break;
                            case 3: w_char = '\xdc'; break;
                            case 4: w_char = '\xdd'; break;
                            case 5: w_char = '\xde'; break;
                        }
                    }
                    else
                    {
                        w_attr = COLOUR_L_BLUE;
                        w_char = '\\';
                    }
                    break;
                }
                // Wind - strong west
                case 3:
                {
                    if (use_graphics)
                    {
                        w_attr = 0x82;
                        switch (randint1(5))
                        {
                            case 1: w_char = '\xda'; break;
                            case 2: w_char = '\xdb'; break;
                            case 3: w_char = '\xdc'; break;
                            case 4: w_char = '\xdd'; break;
                            case 5: w_char = '\xde'; break;
                        }
                    }
                    else
                    {
                        w_attr = COLOUR_BLUE;
                        w_char = '/';
                    }
                    break;
                }
                // Wind - strong east
                case 4:
                {
                    if (use_graphics)
                    {
                        w_attr = 0x82;
                        switch (randint1(5))
                        {
                            case 1: w_char = '\xdf'; break;
                            case 2: w_char = '\xe0'; break;
                            case 3: w_char = '\xe1'; break;
                            case 4: w_char = '\xe2'; break;
                            case 5: w_char = '\xe3'; break;
                        }
                    }
                    else
                    {
                        w_attr = COLOUR_BLUE;
                        w_char = '\\';
                    }
                    break;
                }
            }
            break;
        }
        // Snow
        case 2:
        {
            if (use_graphics)
            {
                w_attr = 0x82;
                switch (randint1(4))
                {
                    case 1: w_char = '\xd6'; break;
                    case 2: w_char = '\xd7'; break;
                    case 3: w_char = '\xd8'; break;
                    case 4: w_char = '\xd9'; break;
                }
            }
            else
            {
                w_attr = COLOUR_WHITE;
                w_char = '*';
            }
            break;
        }
    }

    //// Generate new weather elements ////
    for (i = 0; i < w; i++)
    {
        if (!skip_weather)
        {
            // Weather density
            switch (player->weather_intensity)
            {
                case 1:
                {
                    switch (weather_strength)
                    {
                        case 0:
                            if (one_in_(256)) make_weather = true;
                            break;
                        case 1:
                            if (one_in_(128)) make_weather = true;
                            break;
                        case 2:
                            if (one_in_(64)) make_weather = true;
                            break;
                        default:
                            if (one_in_(32)) make_weather = true;
                            break;
                    }
                    break;
                }
                case 2:
                {
                    switch (weather_strength)
                    {
                        case 0:
                            if (one_in_(128)) make_weather = true;
                            break;
                        case 1:
                            if (one_in_(64)) make_weather = true;
                            break;
                        case 2:
                            if (one_in_(32)) make_weather = true;
                            break;
                        default:
                            if (one_in_(16)) make_weather = true;
                            break;
                    }
                    break;
                }
                case 3:
                {
                    switch (weather_strength)
                    {
                        case 0:
                            if (one_in_(64)) make_weather = true;
                            break;
                        case 1:
                            if (one_in_(32)) make_weather = true;
                            break;
                        case 2:
                            if (one_in_(16)) make_weather = true;
                            break;
                        default:
                            if (one_in_(8)) make_weather = true;
                            break;
                    }
                    break;
                }
            }
        }

        if (skip_weather) skip_weather = false;

        // Create weather elements
        if (make_weather)
        {
            for (j = 0; j < weather_elements; j++)
            {
                if (weather_element_x[j] == -1 && 
                    weather_element_y[j] == -1)
                {
                    weather_element_x[j] = i;
                    weather_element_y[j] = 0;
                    break;
                }
            }

            // Skip creating weather elements if they are nearby './././'
            skip_weather = true;

            make_weather = false;
        }
    }

    //// Draw the weather ////
    for (i = 0; i < weather_elements; i++)
    {
        // Only for elements within visible panel screen area
        if (weather_element_x[i] >= 0 && weather_element_x[i] < w &&
            weather_element_y[i] >= 0 && weather_element_y[i] < h)
        {
            // Check characters
            Term_info(COL_MAP + weather_element_x[i] * tile_width, 
                ROW_MAP + weather_element_y[i] * tile_height, &a, &c, &ta, &tc);

            // Display weather elements
            if (use_graphics)
            {
                // Check background tile
                if (a == ta && c == tc)
                    (void)((*main_term->pict_hook)(COL_MAP + weather_element_x[i] * tile_width, 
                        ROW_MAP + weather_element_y[i] * tile_height, 1, &w_attr, &w_char, &ta, &tc));
            }
            else
            {
                if (c != '@')
                    (void)((*main_term->text_hook)(COL_MAP + weather_element_x[i], 
                        ROW_MAP + weather_element_y[i], 1, w_attr, &w_char));
            }

            // Save weather element frame
            weather_frame_x[i] = weather_element_x[i];
            weather_frame_y[i] = weather_element_y[i];

            //// Move weather elements ////

            // Check wind - west, strong west
            if ((player->weather_wind == 1) || (player->weather_wind == 3))
            {
                weather_element_x[i] = weather_element_x[i] - 
                    (player->weather_type == 2 ? randint0(2) : 1); // <-- 2 is snowflake / raindrop
                weather_element_y[i] = weather_element_y[i] + 1;
            }
            // Check wind - east, strong east
            else if ((player->weather_wind == 2) || (player->weather_wind == 4))
            {
                weather_element_x[i] = weather_element_x[i] + 
                    (player->weather_type == 2 ? randint0(2) : 1); // <-- 2 is snowflake / raindrop
                weather_element_y[i] = weather_element_y[i] + 1;
            }

            // Pac-Man effect for leaving screen to the left/right
            if (weather_element_x[i] < 0) weather_element_x[i] = w - 1;
            else if (weather_element_x[i] > w - 1) weather_element_x[i] = 0;

            // Not all weather elements fall to bottom of screen
            if (one_in_(h))
            {
                weather_element_x[i] = -1;
                weather_element_y[i] = -1;
            }
        }
        else
        {
            // Weather element has reached bottom of screen? terminate it
            if (weather_element_x[i] != -1 && weather_element_y[i] != -1)
            {
                weather_element_x[i] = -1;
                weather_element_y[i] = -1;
            }
        }
    }

    if (weather_strength < 3)
    {
        // When the weather element[1] reaches bottom of screen, intensify weather
        if (weather_element_y[1] > h - 1) weather_strength++;
    }

    // Actually flush the output
    Term_xtra(TERM_XTRA_FRESH, 0);

    // Restore the term
    Term_activate(old);
}


static enum parser_error parse_prefs_opt_anim_obj_w(struct parser *p)
{
    opt_anim_obj_w = (uint16_t)parser_getint(p, "attr");

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_prefs_opt_anim_obj(struct parser *p)
{
    opt_anim_obj = (uint16_t)parser_getint(p, "attr");

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_prefs_opt_anim_npc(struct parser *p)
{
    opt_anim_npc = (uint16_t)parser_getint(p, "attr");

    return PARSE_ERROR_NONE;
}


static enum parser_error parse_prefs_anim_obj(struct parser *p)
{
    int na;
    int nc;

    na = parser_getint(p, "attr");
    nc = parser_getint(p, "char");

    s_obj[na][nc] = parser_getint(p, "n");

    anim_obj_a[na][nc] = (uint16_t)parser_getint(p, "anim_attr");
    anim_obj_c[na][nc] = (char)parser_getint(p, "anim_char");

    return PARSE_ERROR_NONE;
}


static struct parser *init_parse_animation(void)
{
    struct parser *p = parser_new();

    parser_reg(p, "opt-anim-obj_w int attr", parse_prefs_opt_anim_obj_w);
    parser_reg(p, "opt-anim-obj int attr", parse_prefs_opt_anim_obj);
    parser_reg(p, "opt-anim-npc int attr", parse_prefs_opt_anim_npc);
    parser_reg(p, "anim-obj int attr int char int n int anim_attr int anim_char", parse_prefs_anim_obj);

    return p;
}


static bool read_animation_file(void)
{
    graphics_mode *mode = get_graphics_mode(use_graphics, true);

    char buf[MSG_LEN];
    ang_file *f;
    struct parser *p;
    errr e = 0;
    int line_no = 0;

    // Build the filename
    path_build(buf, sizeof(buf), mode->path, "animation.prf");

    f = file_open(buf, MODE_READ, FTYPE_TEXT);
    if (!f)
    {
        plog_fmt("Cannot open '%s'.", buf);

        // Signal failure to callers
        e = PARSE_ERROR_INTERNAL;
    }
    else
    {
        char line[MSG_LEN];

        p = init_parse_animation();
        while (file_getl(f, line, sizeof line))
        {
            line_no++;

            e = parser_parse(p, line);
            if (e != PARSE_ERROR_NONE)
            {
                print_error_simple(buf, p);
                break;
            }
        }

        file_close(f);
        parser_destroy(p);
    }

    // Result
    return e == PARSE_ERROR_NONE;
}


//// Animations ////
void do_animations(void)
{
    term *main_term = angband_term[0];
    term *old = Term;

    int i, j, k;
    int w, h;

    uint16_t a;
    char c;
    uint16_t ta;
    char tc;
    uint16_t nc;
    uint16_t ntc;

    static bool animation_file = false;
    static int animation_frame = 0;

    //// Read animation pref file ////
    if (!animation_file)
    {
        read_animation_file();
        animation_file = true;
    }

    // Check options, if all opt = 0 then don't animate
    if (opt_anim_obj_w == 0 && opt_anim_obj == 0 && opt_anim_npc == 0) return;

#ifndef USE_SDL2
    // Hack -- SDL and Windows client, disable opt_anim_obj_w
    if (opt_anim_obj_w != 0) opt_anim_obj_w = 0;
#endif

    // Hack -- if the screen is already icky, ignore this command
    if (player->screen_save_depth) return;

    // If the weather type is rain/snow then disable animations
    if (OPT(player, weather_display) && player->weather_type != 0) return;

    // If monsters attack then disable animations
    if (OPT(player, slash_fx) && sfx_effect) return;

    // Activate the term
    Term_activate(main_term);

    // Get screen size
    w = (main_term->wid - COL_MAP - 1) / tile_width;
    h = (main_term->hgt - ROW_MAP - 1) / tile_height;

    //// Animate entire screen characters/objects ////
    for (k = 1; k < 4; k++)
    {
        // If options = 0 then don't animate
        if (k == 1 && opt_anim_obj_w == 0) continue;
        else if (k == 2 && opt_anim_obj == 0) continue;
        else if (k == 3 && opt_anim_npc == 0) continue;

        // For the dungeons, if options = 1 then don't animate
        if (k == 1 && opt_anim_obj_w == 1 && player->wpos.depth > 0) continue;
        else if (k == 2 && opt_anim_obj == 1 && player->wpos.depth > 0) continue;
        else if (k == 3 && opt_anim_npc == 1 && player->wpos.depth > 0) continue;

        // Screen
        for (i = 0; i < h; i++)
        {
            for (j = 0; j < w; j++)
            {
                // Check characters
                // Term_info(COL_MAP + j * tile_width, 
                //     ROW_MAP + i * tile_height, &a, &c, &ta, &tc);
                a = main_term->scr->a[ROW_MAP + i * tile_height][COL_MAP + j * tile_width];
                c = main_term->scr->c[ROW_MAP + i * tile_height][COL_MAP + j * tile_width];
                ta = main_term->scr->ta[ROW_MAP + i * tile_height][COL_MAP + j * tile_width];
                tc = main_term->scr->tc[ROW_MAP + i * tile_height][COL_MAP + j * tile_width];

                // Convert char to uint8_t [0 - 255]
                nc = (uint8_t) c;

                // Check for overflow s_obj[1024][256]
                if (a > 1024 || nc > 256) continue;

                // Convert char to uint8_t [0 - 255]
                ntc = (uint8_t) tc;

                // Check for overflow s_obj[1024][256]
                if (ta > 1024 || ntc > 256) continue;

                // Check foreground/background tile
                if (a == ta && c == tc)
                {
                    // If found then animate
                    if (s_obj[a][nc] == k)
                    {
                        if (animation_frame == 0)
                        {
                            // Restore display
                            (void)((*main_term->pict_hook)(COL_MAP + j * tile_width, 
                                ROW_MAP + i * tile_height, 1, &a, &c, &ta, &tc));
                        }
                        else if (animation_frame == 1)
                        {
                            if (k == 3 && !one_in_(3)) continue;
                            // Draw display
                            (void)((*main_term->pict_hook)(COL_MAP + j * tile_width, 
                                ROW_MAP + i * tile_height, 1, &anim_obj_a[a][nc], &anim_obj_c[a][nc], &ta, &tc));
                        }
                    }
                }
                else
                {
                    // If found then animate
                    if ((s_obj[a][nc] == k && s_obj[ta][ntc] == 1) ||
                        (s_obj[a][nc] == k && s_obj[ta][ntc] == 2))
                    {
                        if (animation_frame == 0)
                        {
                            // Restore display
                            (void)((*main_term->pict_hook)(COL_MAP + j * tile_width, 
                                ROW_MAP + i * tile_height, 1, &a, &c, &ta, &tc));
                        }
                        else if (animation_frame == 1)
                        {
                            if (k == 3 && !one_in_(3)) continue;
                            // Draw display
                            (void)((*main_term->pict_hook)(COL_MAP + j * tile_width, 
                                ROW_MAP + i * tile_height, 1, &anim_obj_a[a][nc], &anim_obj_c[a][nc], 
                                    &anim_obj_a[ta][ntc], &anim_obj_c[ta][ntc]));
                        }
                    }
                    else if (s_obj[a][nc] == k)
                    {
                        if (animation_frame == 0)
                        {
                            // Restore display
                            (void)((*main_term->pict_hook)(COL_MAP + j * tile_width, 
                                ROW_MAP + i * tile_height, 1, &a, &c, &ta, &tc));
                        }
                        else if (animation_frame == 1)
                        {
                            if (k == 3 && !one_in_(3)) continue;
                            // Draw display
                            (void)((*main_term->pict_hook)(COL_MAP + j * tile_width, 
                                ROW_MAP + i * tile_height, 1, &anim_obj_a[a][nc], &anim_obj_c[a][nc], 
                                    &ta, &tc));
                        }
                    }
                    else if (s_obj[ta][ntc] == k)
                    {
                        if (animation_frame == 0)
                        {
                            // Restore display
                            (void)((*main_term->pict_hook)(COL_MAP + j * tile_width, 
                                ROW_MAP + i * tile_height, 1, &a, &c, &ta, &tc));
                        }
                        else if (animation_frame == 1)
                        {
                            if (k == 3 && !one_in_(3)) continue;
                            // Draw display
                            (void)((*main_term->pict_hook)(COL_MAP + j * tile_width, 
                                ROW_MAP + i * tile_height, 1, &a, &c, 
                                    &anim_obj_a[ta][ntc], &anim_obj_c[ta][ntc]));
                        }
                    }
                }
            }
        }
    }

    animation_frame++;
    if (animation_frame > 1) animation_frame = 0;

    // Actually flush the output
    Term_xtra(TERM_XTRA_FRESH, 0);

    // Restore the term
    Term_activate(old);
}


//// Handle the slash effects ////


void slashfx_dir_offset(int *x, int *y, int dir)
{
    const int dir_offset_x[9] = { -1, 0, 1, -1, 0, 1, -1, 0, 1 };
    const int dir_offset_y[9] = { -1,-1,-1,  0, 0, 0,  1, 1, 1 };
    *x = dir_offset_x[dir - 1];
    *y = dir_offset_y[dir - 1];
}


void do_slashfx(void)
{
    term *main_term = angband_term[0];
    term *old = Term;

    int i, j;
    int w, h;
    int ox, oy;

    uint16_t a;
    char c;
    uint16_t ta;
    char tc;

    // Hack -- if the screen is already icky, ignore this command
    if (player->screen_save_depth) return;

    // Activate the term
    Term_activate(main_term);

    // Get screen size
    w = (main_term->wid - COL_MAP - 1) / tile_width;
    h = (main_term->hgt - ROW_MAP - 1) / tile_height;

    // Maximum number grids on a level
    for (i = 0; i < Setup.max_row; i++)
    {
        for (j = 0; j < Setup.max_col; j++)
        {
            // Only for sfx_move within visible panel screen area
            if ((j - player->offset_grid.x) >= 0 && (j - player->offset_grid.x) < w && 
                (i - player->offset_grid.y) >= 0 && (i - player->offset_grid.y) < h)
            {
                if (sfx_move[i][j] == 0) continue;

                if (sfx_move[i][j] == 2 || sfx_move[i][j] == 4)
                {
                    // Check characters
                    Term_info(COL_MAP + (j - player->offset_grid.x) * tile_width, 
                        ROW_MAP + (i - player->offset_grid.y) * tile_height, &a, &c, &ta, &tc);

                    if (sfx_info_a[i][j] == a && sfx_info_c[i][j] == c)
                    {
                        sfx_effect = true;
                        sfx_dir = sfx_info_d[i][j];

                        // Display
                        (void)((*main_term->pict_hook)(COL_MAP + (j - player->offset_grid.x) * tile_width, 
                            ROW_MAP + (i - player->offset_grid.y) * tile_height, 1, &a, &c, &ta, &tc));
                    }
                }
                else if (sfx_move[i][j] == 1 || sfx_move[i][j] == 3)
                {
                    // Refresh slash effects. We will draw 4 nearby tiles.
                    // sfx_info_d[y][x] = dir
                    //  |1|2|3|
                    //  |4|5|6|
                    //  |7|8|9|

                    slashfx_dir_offset(&ox, &oy, sfx_info_d[i][j]);

                    if ((j - player->offset_grid.x + ox) >= 0 && (j - player->offset_grid.x + ox) < w && 
                        (i - player->offset_grid.y + oy) >= 0 && (i - player->offset_grid.y + oy) < h)
                    {
                        Term_info(COL_MAP + (j - player->offset_grid.x + ox) * tile_width, 
                            ROW_MAP + (i - player->offset_grid.y + oy) * tile_height, &a, &c, &ta, &tc);
                        (void)((*main_term->pict_hook)(COL_MAP + (j - player->offset_grid.x + ox) * tile_width, 
                            ROW_MAP + (i - player->offset_grid.y + oy) * tile_height, 1, &a, &c, &ta, &tc));
                    }
                    if ((j - player->offset_grid.x + ox * 0) >= 0 && (j - player->offset_grid.x + ox * 0) < w && 
                        (i - player->offset_grid.y + oy) >= 0 && (i - player->offset_grid.y + oy) < h)
                    {
                        Term_info(COL_MAP + (j - player->offset_grid.x + ox * 0) * tile_width, 
                            ROW_MAP + (i - player->offset_grid.y + oy) * tile_height, &a, &c, &ta, &tc);
                        (void)((*main_term->pict_hook)(COL_MAP + (j - player->offset_grid.x + ox * 0) * tile_width, 
                            ROW_MAP + (i - player->offset_grid.y + oy) * tile_height, 1, &a, &c, &ta, &tc));
                    }
                    if ((j - player->offset_grid.x + ox) >= 0 && (j - player->offset_grid.x + ox) < w && 
                        (i - player->offset_grid.y + oy * 0) >= 0 && (i - player->offset_grid.y + oy * 0) < h)
                    {
                        Term_info(COL_MAP + (j - player->offset_grid.x + ox) * tile_width, 
                            ROW_MAP + (i - player->offset_grid.y + oy * 0) * tile_height, &a, &c, &ta, &tc);
                        (void)((*main_term->pict_hook)(COL_MAP + (j - player->offset_grid.x + ox) * tile_width, 
                            ROW_MAP + (i - player->offset_grid.y + oy * 0) * tile_height, 1, &a, &c, &ta, &tc));
                    }

                    Term_info(COL_MAP + (j - player->offset_grid.x) * tile_width, 
                        ROW_MAP + (i - player->offset_grid.y) * tile_height, &a, &c, &ta, &tc);

                    if (sfx_info_a[i][j] == a && sfx_info_c[i][j] == c)
                    {
                        (void)((*main_term->pict_hook)(COL_MAP + (j - player->offset_grid.x) * tile_width, 
                            ROW_MAP + (i - player->offset_grid.y) * tile_height, 1, &a, &c, &ta, &tc));
                    }
                }

                sfx_move[i][j] -= 1;
                if (sfx_move[i][j] <= 0)
                {
                    sfx_move[i][j] = 0;
                    sfx_info_a[i][j] = 0;
                    sfx_info_c[i][j] = 0;
                    sfx_info_d[i][j] = 0;
                }
            }
        }
    }

    // Actually flush the output
    Term_xtra(TERM_XTRA_FRESH, 0);

    // Restore the term
    Term_activate(old);
}


void slashfx_refresh_char(int x, int y)
{
    term *main_term = angband_term[0];
    term *old = Term;

    int w, h;
    int ox, oy;

    uint16_t a;
    char c;
    uint16_t ta;
    char tc;

    // Hack -- if the screen is already icky, ignore this command
    if (player->screen_save_depth) return;

    // Activate the term
    Term_activate(main_term);

    // Get screen size
    w = (main_term->wid - COL_MAP - 1) / tile_width;
    h = (main_term->hgt - ROW_MAP - 1) / tile_height;

    // Refresh character. We will draw 4 nearby tiles.
    // sfx_info_d[y][x] = dir
    //  |1|2|3|
    //  |4|5|6|
    //  |7|8|9|

    slashfx_dir_offset(&ox, &oy, sfx_info_d[y][x]);

    if ((x - player->offset_grid.x + ox) >= 0 && (x - player->offset_grid.x + ox) < w && 
        (y - player->offset_grid.y + oy) >= 0 && (y - player->offset_grid.y + oy) < h)
    {
        Term_info(COL_MAP + (x - player->offset_grid.x + ox) * tile_width, 
            ROW_MAP + (y - player->offset_grid.y + oy) * tile_height, &a, &c, &ta, &tc);
        (void)((*main_term->pict_hook)(COL_MAP + (x - player->offset_grid.x + ox) * tile_width, 
            ROW_MAP + (y - player->offset_grid.y + oy) * tile_height, 1, &a, &c, &ta, &tc));
    }
    if ((x - player->offset_grid.x + ox * 0) >= 0 && (x - player->offset_grid.x + ox * 0) < w && 
        (y - player->offset_grid.y + oy) >= 0 && (y - player->offset_grid.y + oy) < h)
    {
        Term_info(COL_MAP + (x - player->offset_grid.x + ox * 0) * tile_width, 
            ROW_MAP + (y - player->offset_grid.y + oy) * tile_height, &a, &c, &ta, &tc);
        (void)((*main_term->pict_hook)(COL_MAP + (x - player->offset_grid.x + ox * 0) * tile_width, 
            ROW_MAP + (y - player->offset_grid.y + oy) * tile_height, 1, &a, &c, &ta, &tc));
    }
    if ((x - player->offset_grid.x + ox) >= 0 && (x - player->offset_grid.x + ox) < w && 
        (y - player->offset_grid.y + oy * 0) >= 0 && (y - player->offset_grid.y + oy * 0) < h)
    {
        Term_info(COL_MAP + (x - player->offset_grid.x + ox) * tile_width, 
            ROW_MAP + (y - player->offset_grid.y + oy * 0) * tile_height, &a, &c, &ta, &tc);
        (void)((*main_term->pict_hook)(COL_MAP + (x - player->offset_grid.x + ox) * tile_width, 
            ROW_MAP + (y - player->offset_grid.y + oy * 0) * tile_height, 1, &a, &c, &ta, &tc));
    }
    if ((x - player->offset_grid.x) >= 0 && (x - player->offset_grid.x) < w && 
        (y - player->offset_grid.y) >= 0 && (y - player->offset_grid.y) < h)
    {
        Term_info(COL_MAP + (x - player->offset_grid.x) * tile_width, 
            ROW_MAP + (y - player->offset_grid.y) * tile_height, &a, &c, &ta, &tc);
        (void)((*main_term->pict_hook)(COL_MAP + (x - player->offset_grid.x) * tile_width, 
            ROW_MAP + (y - player->offset_grid.y) * tile_height, 1, &a, &c, &ta, &tc));
    }

    // Actually flush the output
    Term_xtra(TERM_XTRA_FRESH, 0);

    // Restore the term
    Term_activate(old);
}


void do_slashfx_ascii(void)
{
    term *main_term = angband_term[0];
    term *old = Term;

    int i, j;
    int w, h;

    uint16_t a;
    char c;
    uint16_t ta;
    char tc;

    // Hack -- if the screen is already icky, ignore this command
    if (player->screen_save_depth) return;

    // Activate the term
    Term_activate(main_term);

    // Get screen size
    w = main_term->wid - COL_MAP - 1;
    h = main_term->hgt - ROW_MAP - 1;

    // Maximum number grids on a level
    for (i = 0; i < Setup.max_row; i++)
    {
        for (j = 0; j < Setup.max_col; j++)
        {
            // Only for sfx_move within visible panel screen area
            if ((j - player->offset_grid.x) >= 0 && (j - player->offset_grid.x) < w && 
                (i - player->offset_grid.y) >= 0 && (i - player->offset_grid.y) < h)
            {
                if (sfx_move[i][j] == 0) continue;

                if (sfx_move[i][j] > 1)
                {
                    // Check characters
                    Term_info(COL_MAP + (j - player->offset_grid.x), 
                        ROW_MAP + (i - player->offset_grid.y), &a, &c, &ta, &tc);

                    if (sfx_info_a[i][j] == a && sfx_info_c[i][j] == c)
                    {
                        //// Fire slash fx ////
                        switch (randint1(4))
                        {
                            case 1: a = COLOUR_RED; break;
                            case 2: a = COLOUR_L_RED; break;
                            case 3: a = COLOUR_YELLOW; break;
                            case 4: a = COLOUR_ORANGE; break;
                        }

                        if (sfx_info_d[i][j] == 1 || sfx_info_d[i][j] == 9) c = '\\';
                        else if (sfx_info_d[i][j] == 2 || sfx_info_d[i][j] == 8) c = '|';
                        else if (sfx_info_d[i][j] == 3 || sfx_info_d[i][j] == 7) c = '/';
                        else if (sfx_info_d[i][j] == 4 || sfx_info_d[i][j] == 6) c = '-';
                        else if (sfx_info_d[i][j] == 5) c = '+';

                        // Display
                        (void)((*main_term->text_hook)(COL_MAP + (j - player->offset_grid.x), 
                            ROW_MAP + (i - player->offset_grid.y), 1, a, &c));
                    }
                }
                else if (sfx_move[i][j] == 1)
                {
                    Term_info(COL_MAP + (j - player->offset_grid.x), 
                        ROW_MAP + (i - player->offset_grid.y), &a, &c, &ta, &tc);

                    if (sfx_info_a[i][j] == a && sfx_info_c[i][j] == c)
                    {
                        // Display
                        (void)((*main_term->text_hook)(COL_MAP + (j - player->offset_grid.x), 
                            ROW_MAP + (i - player->offset_grid.y), 1, a, &c));
                    }
                }

                sfx_move[i][j] -= 1;
                if (sfx_move[i][j] <= 0)
                {
                    sfx_move[i][j] = 0;
                    sfx_info_a[i][j] = 0;
                    sfx_info_c[i][j] = 0;
                    sfx_info_d[i][j] = 0;
                }
            }
        }
    }

    // Actually flush the output
    Term_xtra(TERM_XTRA_FRESH, 0);

    // Restore the term
    Term_activate(old);
}


void slashfx_refresh_char_ascii(int x, int y)
{
    term *main_term = angband_term[0];
    term *old = Term;

    int w, h;

    uint16_t a;
    char c;
    uint16_t ta;
    char tc;

    // Hack -- if the screen is already icky, ignore this command
    if (player->screen_save_depth) return;

    // Activate the term
    Term_activate(main_term);

    // Get screen size
    w = main_term->wid - COL_MAP - 1;
    h = main_term->hgt - ROW_MAP - 1;

    if ((x - player->offset_grid.x) >= 0 && (x - player->offset_grid.x) < w && 
        (y - player->offset_grid.y) >= 0 && (y - player->offset_grid.y) < h)
    {
        Term_info(COL_MAP + (x - player->offset_grid.x), 
            ROW_MAP + (y - player->offset_grid.y), &a, &c, &ta, &tc);
        (void)((*main_term->text_hook)(COL_MAP + (x - player->offset_grid.x), 
            ROW_MAP + (y - player->offset_grid.y), 1, a, &c));
    }

    // Actually flush the output
    Term_xtra(TERM_XTRA_FRESH, 0);

    // Restore the term
    Term_activate(old);
}


// Remember attr/char information
void slashfx_save_char(int x, int y)
{
    term *main_term = angband_term[0];
    term *old = Term;

    int w, h;

    uint16_t a;
    char c;
    uint16_t ta;
    char tc;

    // Hack -- if the screen is already icky, ignore this command
    if (player->screen_save_depth) return;

    // Activate the term
    Term_activate(main_term);

    // Get screen size
    w = (main_term->wid - COL_MAP - 1) / tile_width;
    h = (main_term->hgt - ROW_MAP - 1) / tile_height;

    if ((x - player->offset_grid.x) >= 0 && (x - player->offset_grid.x) < w && 
        (y - player->offset_grid.y) >= 0 && (y - player->offset_grid.y) < h)
    {
        Term_info(COL_MAP + (x - player->offset_grid.x) * tile_width, 
            ROW_MAP + (y - player->offset_grid.y) * tile_height, &a, &c, &ta, &tc);

        if (use_graphics)
        {
            // Check character visibility
            if (a != ta && c != tc)
            {
                sfx_info_a[y][x] = a;
                sfx_info_c[y][x] = c;
            }
            else
            {
                sfx_move[y][x] = 0;
                sfx_info_a[y][x] = 0;
                sfx_info_c[y][x] = 0;
                sfx_info_d[y][x] = 0;
            }
        }
        else
        {
            // Check character
            if ((c != ' ') && (c != '.'))
            {
                sfx_info_a[y][x] = a;
                sfx_info_c[y][x] = c;
            }
            else
            {
                sfx_move[y][x] = 0;
                sfx_info_a[y][x] = 0;
                sfx_info_c[y][x] = 0;
                sfx_info_d[y][x] = 0;
            }
        }
    }

    // Restore the term
    Term_activate(old);
}

