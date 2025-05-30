/*
 * File: ui-event.c
 * Purpose: Utility functions relating to UI events
 *
 * Copyright (c) 2011 Andi Sidwell
 * Copyright (c) 2025 MAngband and PWMAngband Developers
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


/*
 * Map keycodes to their textual equivalent.
 */
static const struct
{
    keycode_t code;
    const char *desc;
} mappings[] =
{
    { ESCAPE, "Escape" },
    { KC_ENTER, "Enter" },
    { KC_TAB, "Tab" },
    { KC_DELETE, "Delete" },
    { KC_BACKSPACE, "Backspace" },
    { ARROW_DOWN, "Down" },
    { ARROW_LEFT, "Left" },
    { ARROW_RIGHT, "Right" },
    { ARROW_UP, "Up" },
    { KC_F1, "F1" },
    { KC_F2, "F2" },
    { KC_F3, "F3" },
    { KC_F4, "F4" },
    { KC_F5, "F5" },
    { KC_F6, "F6" },
    { KC_F7, "F7" },
    { KC_F8, "F8" },
    { KC_F9, "F9" },
    { KC_F10, "F10" },
    { KC_F11, "F11" },
    { KC_F12, "F12" },
    { KC_F13, "F13" },
    { KC_F14, "F14" },
    { KC_F15, "F15" },
    { KC_F16, "F16" },
    { KC_F17, "F17" },
    { KC_F18, "F18" },
    { KC_F19, "F19" },
    { KC_F20, "F20" },
    { KC_F21, "F21" },
    { KC_F22, "F22" },
    { KC_F23, "F23" },
    { KC_F24, "F24" },
    { KC_HELP, "Help" },
    { KC_HOME, "Home" },
    { KC_PGUP, "PageUp" },
    { KC_END, "End" },
    { KC_PGDOWN, "PageDown" },
    { KC_INSERT, "Insert" },
    { KC_PAUSE, "Pause" },
    { KC_BREAK, "Break" },
    { KC_BEGIN, "Begin" }
};


/*
 * Given a string, try and find it in "mappings".
 */
keycode_t keycode_find_code(const char *str, size_t len)
{
    size_t i;

    for (i = 0; i < N_ELEMENTS(mappings); i++)
    {
        if (strncmp(str, mappings[i].desc, len) == 0)
            return mappings[i].code;
    }
    return 0;
}


/*
 * Given a keycode, return its textual mapping.
 */
const char *keycode_find_desc(keycode_t kc)
{
    size_t i;

    for (i = 0; i < N_ELEMENTS(mappings); i++)
    {
        if (mappings[i].code == kc)
            return mappings[i].desc;
    }
    return NULL;
}


/*
 * Convert a hexidecimal-digit into a decimal
 */
static int dehex(char c)
{
    if (isdigit((unsigned char)c)) return (D2I(c));
    if (isalpha((unsigned char)c)) return (A2I(tolower((unsigned char)c)) + 10);
    return (0);
}


/*
 * Convert an encoding of a set of keypresses into actual keypresses.
 */
void keypress_from_text(struct keypress *buf, size_t len, const char *str)
{
    size_t cur = 0;
    uint8_t mods = 0;

    memset(buf, 0, len * sizeof(*buf));

#define STORE(buffer, pos, mod, cod) \
    { \
        int p = (pos); \
        keycode_t c = (cod); \
        uint8_t m = (mod); \
\
        if ((m & KC_MOD_CONTROL) && ENCODE_KTRL(c)) \
        { \
            m &= ~KC_MOD_CONTROL; \
            c = KTRL(c); \
        } \
\
        buffer[p].mods = m; \
        buffer[p].code = c; \
    }

    /* Analyze the "ascii" string */
    while (*str && (cur < len))
    {
        buf[cur].type = EVT_KBRD;

        if (*str == '\\')
        {
            str++;
            if (*str == '\0') break;

            switch (*str)
            {
                /* Hex-mode */
                case 'x':
                {
                    if (isxdigit((unsigned char)(*(str + 1))) &&
                        isxdigit((unsigned char)(*(str + 2))))
                    {
                        int v1 = dehex(*++str) * 16;
                        int v2 = dehex(*++str);

                        /* Store a nice hex digit */
                        STORE(buf, cur++, mods, v1 + v2);
                    }
                    else
                    {
                        /* Invalids get ignored */
                        STORE(buf, cur++, mods, '?');
                    }
                    break;
                }

                case 'a': STORE(buf, cur++, mods, '\a'); break;
                case '\\': STORE(buf, cur++, mods, '\\'); break;
                case '^': STORE(buf, cur++, mods, '^'); break;
                case '[': STORE(buf, cur++, mods, '['); break;
                case '{': STORE(buf, cur++, mods, '{'); break;
                default: STORE(buf, cur++, mods, *str); break;
            }

            mods = 0;

            /* Skip the final char */
            str++;
        }
        else if (*str == '[')
        {
            /* Parse non-ascii keycodes */
            char *end;
            keycode_t kc;

            if (*str++ == 0) return;

            end = strchr(str, (unsigned char)']');
            if (!end) return;

            kc = keycode_find_code(str, (size_t)(end - str));
            if (!kc) return;

            STORE(buf, cur++, mods, kc);
            mods = 0;
            str = end + 1;
        }
        else if (*str == '{')
        {
            /* Specify modifier for next character */
            str++;

            if ((*str == '\0') || !strchr(str, (unsigned char)'}'))
                return;

            /* Analyze modifier chars */
            while (*str != '}')
            {
                switch (*str)
                {
                    case '^': mods |= KC_MOD_CONTROL; break;
                    case 'S': mods |= KC_MOD_SHIFT; break;
                    case 'A': mods |= KC_MOD_ALT; break;
                    case 'M': mods |= KC_MOD_META; break;
                    case 'K': mods |= KC_MOD_KEYPAD; break;
                    default: return;
                }

                str++;
            }

            /* Skip ending bracket */
            str++;
        }
        else if (*str == '^')
        {
            mods |= KC_MOD_CONTROL;
            str++;
        }
        else
        {
            /* Everything else */
            STORE(buf, cur++, mods, *str++);
            mods = 0;
        }
    }

    /* Terminate */
    cur = MIN(cur, len - 1);
    buf[cur].type = EVT_NONE;
}


/*
 * Convert a string of keypresses into their textual equivalent.
 */
void keypress_to_text(char *buf, size_t len, const struct keypress *src, bool expand_backslash)
{
    size_t cur = 0;
    size_t end = 0;

    while (src[cur].type == EVT_KBRD)
    {
        keycode_t i = src[cur].code;
        int mods = src[cur].mods;
        const char *desc = keycode_find_desc(i);

        /*
         * Un-ktrl control characters if they don't have a description
         * This is so that Tab (^i) doesn't get turned into ^i but gets
         * displayed as [Tab]
         */
        if (i < 0x20 && !desc)
        {
            mods |= KC_MOD_CONTROL;
            i = UN_KTRL(i);
        }

        if (mods)
        {
            if (mods & KC_MOD_CONTROL && !(mods & ~KC_MOD_CONTROL))
                strnfcat(buf, len, &end, "%s", "^");
            else
            {
                strnfcat(buf, len, &end, "%s", "{");
                if (mods & KC_MOD_CONTROL) strnfcat(buf, len, &end, "%s", "^");
                if (mods & KC_MOD_SHIFT) strnfcat(buf, len, &end, "%s", "S");
                if (mods & KC_MOD_ALT) strnfcat(buf, len, &end, "%s", "A");
                if (mods & KC_MOD_META) strnfcat(buf, len, &end, "%s", "M");
                if (mods & KC_MOD_KEYPAD) strnfcat(buf, len, &end, "%s", "K");
                strnfcat(buf, len, &end, "%s", "}");
            }
        }

        if (desc)
            strnfcat(buf, len, &end, "[%s]", desc);
        else
        {
            switch (i)
            {
                case '\a': strnfcat(buf, len, &end, "%s", "\a"); break;
                case '\\':
                {
                    if (expand_backslash)
                        strnfcat(buf, len, &end, "%s", "\\\\");
                    else
                        strnfcat(buf, len, &end, "%s", "\\");
                    break;
                }
                case '^': strnfcat(buf, len, &end, "%s", "\\^"); break;
                case '[': strnfcat(buf, len, &end, "%s", "\\["); break;
                case '{': strnfcat(buf, len, &end, "%s", "\\{"); break;
                default:
                {
                    if (i < 127)
                        strnfcat(buf, len, &end, "%c", (int)i);
                    else
                        strnfcat(buf, len, &end, "\\x%02lx", (unsigned long)i);
                    break;
                }
            }
        }

        cur++;
    }

    /* Terminate */
    buf[end] = '\0';
}


/*
 * Convert a keypress into something readable.
 */
void keypress_to_readable(char *buf, size_t len, struct keypress src)
{
    size_t end = 0;
    keycode_t i = src.code;
    int mods = src.mods;
    const char *desc = keycode_find_desc(i);

    /*
     * Un-KTRL control characters if they don't have a description
     * This is so that Tab (^i) doesn't get turned into ^i but gets
     * displayed as [Tab]
     */
    if (i < 0x20 && !desc)
    {
        mods |= KC_MOD_CONTROL;
        i = UN_KTRL(i);
    }

    if (mods)
    {
        if (mods & KC_MOD_CONTROL && !(mods & ~KC_MOD_CONTROL) && (i != '^'))
            strnfcat(buf, len, &end, "%s", "^");
        else
        {
            if (mods & KC_MOD_CONTROL) strnfcat(buf, len, &end, "%s", "Control-");
            if (mods & KC_MOD_SHIFT) strnfcat(buf, len, &end, "%s", "Shift-");
            if (mods & KC_MOD_ALT) strnfcat(buf, len, &end, "%s", "Alt-");
            if (mods & KC_MOD_META) strnfcat(buf, len, &end, "%s", "Meta-");
            if (mods & KC_MOD_KEYPAD) strnfcat(buf, len, &end, "%s", "Keypad-");
        }
    }

    if (desc)
        strnfcat(buf, len, &end, "%s", desc);
    else
    {
        /* XXX utf32_to_utf8 XXX */
        strnfcat(buf, len, &end, "%c", i);
    }

    /* Terminate */
    buf[end] = '\0';
}
