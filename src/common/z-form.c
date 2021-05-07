/*
 * File: z-form.c
 * Purpose: Low-level text formatting (snprintf() replacement)
 *
 * Copyright (c) 1997 Ben Harrison
 * Copyright (c) 2021 MAngband and PWMAngband Developers
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


#include "angband.h"


/*
 * Here is some information about the routines in this file.
 *
 * In general, the following routines take a "buffer", a "max length",
 * a "format string", and some "arguments", and use the format string
 * and the arguments to create a (terminated) string in the buffer
 * (using only the first "max length" bytes), and return the "length"
 * of the resulting string, not including the (mandatory) terminator.
 *
 * The format strings allow the basic "sprintf()" format sequences, though
 * some of them are processed slightly more carefully or portably, as well
 * as a few "special" sequences, including the "capitalization" sequences of
 * "%C" and "%S".
 *
 * Note that some "limitations" are enforced by the current implementation,
 * for example, no "format sequence" can exceed 100 characters, including any
 * "length" restrictions, and the result of combining and "format sequence"
 * with the relevent "arguments" must not exceed 1000 characters.
 *
 * These limitations could be fixed by stealing some of the code from,
 * say, "vsprintf()" and placing it into my "vstrnfmt()" function.
 *
 * Note that a "*" inside a "format sequence" is removed from the "format sequence",
 * and replaced by the textual form of the next argument in the argument list.
 * See examples below.
 *
 * Legal format characters: %,b,n,p,c,s,d,i,o,u,X,x,E,e,F,f,G,g,r,v.
 *
 * Format("%%")
 *   Append the literal "%".
 *   No legal modifiers.
 *
 * Format("%n", size_t *np)
 *   Save the current length into (*np).
 *   No legal modifiers.
 *
 * Format("%p", void *v)
 *   Append the pointer "v" (implementation varies).
 *   No legal modifiers.
 *
 * Format("%b", int b)
 *   Append the integer formatted as binary.
 *   If a modifier of 1, 2, 3 or 4 is provided, then only append 2**n bits, not
 *   all 32.
 *
 * Format("%E", double r)
 * Format("%F", double r)
 * Format("%G", double r)
 * Format("%e", double r)
 * Format("%f", double r)
 * Format("%g", double r)
 *   Append the double "r", in various formats.
 *
 * Format("%ld", long int i)
 *   Append the long integer "i".
 *
 * Format("%d", int i)
 *   Append the integer "i".
 *
 * Format("%lu", unsigned long int i)
 *   Append the unsigned long integer "i".
 *
 * Format("%u", unsigned int i)
 *   Append the unsigned integer "i".
 *
 * Format("%lo", unsigned long int i)
 *   Append the unsigned long integer "i", in octal.
 *
 * Format("%o", unsigned int i)
 *   Append the unsigned integer "i", in octal.
 *
 * Format("%lX", unsigned long int i)
 *   Note -- use all capital letters
 * Format("%lx", unsigned long int i)
 *   Append the unsigned long integer "i", in hexidecimal.
 *
 * Format("%X", unsigned int i)
 *   Note -- use all capital letters
 * Format("%x", unsigned int i)
 *   Append the unsigned integer "i", in hexidecimal.
 *
 * Format("%c", char c)
 *   Append the character "c".
 *   Do not use the "+" or "0" flags.
 *
 * Format("%s", const char *s)
 *   Append the string "s".
 *   Do not use the "+" or "0" flags.
 *   Note that a "NULL" value of "s" is converted to the empty string.
 *
 * For examples below, assume "int n = 0; int m = 100; char buf[100];",
 * plus "char *s = NULL;", and unknown values "char *txt; int i;".
 *
 * For example: "n = strnfmt(buf, -1, "(Max %d)", i);" will have a
 * similar effect as "sprintf(buf, "(Max %d)", i); n = strlen(buf);".
 *
 * For example: "(void)strnfmt(buf, 16, "%s", txt);" will have a similar
 * effect as "strncpy(buf, txt, 16); buf[15] = '\0';".
 *
 * For example: "if (strnfmt(buf, 16, "%s", txt) < 16) ..." will have
 * a similar effect as "strcpy(buf, txt)" but with bounds checking.
 *
 * For example: "s = buf; s += vstrnfmt(s, -1, ...); ..." will allow
 * multiple "appends" to "buf" (at the cost of losing the max-length info).
 *
 * For example: "s = buf; n = vstrnfmt(s+n, 100-n, ...); ..." will allow
 * multiple bounded "appends" to "buf", with constant access to "strlen(buf)".
 *
 * For example: "format("%-.*s", i, txt)" will produce a string containing
 * the first "i" characters of "txt", left justified.
 */


/*
 * Basic "vararg" format function.
 *
 * This function takes a buffer, a max byte count, a format string, and
 * a va_list of arguments to the format string, and uses the format string
 * and the arguments to create a string to the buffer.  The string is
 * derived from the format string and the arguments in the manner of the
 * "sprintf()" function, but with some extra "format" commands.  Note that
 * this function will never use more than the given number of bytes in the
 * buffer, preventing messy invalid memory references.  This function then
 * returns the total number of non-null bytes written into the buffer.
 *
 * Method: Let "str" be the (unlimited) created string, and let "len" be the
 * smaller of "max-1" and "strlen(str)".  We copy the first "len" chars of
 * "str" into "buf", place "\0" into buf[len], and return "len".
 *
 * In English, we do a sprintf() into "buf", a buffer with size "max",
 * and we return the resulting value of "strlen(buf)", but we allow some
 * special format commands, and we are more careful than "sprintf()".
 *
 * Typically, "max" is in fact the "size" of "buf", and thus represents
 * the "number" of chars in "buf" which are ALLOWED to be used.  An
 * alternative definition would have required "buf" to hold at least
 * "max+1" characters, and would have used that extra character only
 * in the case where "buf" was too short for the result.  This would
 * give an easy test for "overflow", but a less "obvious" semantics.
 *
 * Note that if the buffer was "too short" to hold the result, we will
 * always return "max-1", but we also return "max-1" if the buffer was
 * "just long enough".  We could have returned "max" if the buffer was
 * too short, not written a null, and forced the programmer to deal with
 * this special case, but I felt that it is better to at least give a
 * "usable" result when the buffer was too long instead of either giving
 * a memory overwrite like "sprintf()" or a non-terminated string like
 * "strncpy()".  Note that "strncpy()" also "null-pads" the result.
 *
 * Note that in most cases "just long enough" is probably "too short".
 *
 * We use snprintf (for safety, and to quieten picky compilers).
 *
 * We should also consider extracting and processing the "width" and other
 * "flags" by hand, it might be more "accurate", and it would allow us to
 * remove the limit (1000 chars) on the result of format sequences.
 *
 * Also, some sequences, such as "%+d" by hand, do not work on all machines,
 * and could thus be correctly handled here.
 *
 * Error detection in this routine is not very graceful, in particular,
 * if an error is detected in the format string, we simply "pre-terminate"
 * the given buffer to a length of zero, and return a "length" of zero.
 * The contents of "buf", except for "buf[0]", may then be undefined.
 */
size_t vstrnfmt(char *buf, size_t max, const char *fmt, va_list vp)
{
    const char *s;

    /* The argument is "short" */
    bool do_short;

    /* The argument is "long" */
    bool do_long;

    /* The argument is "long long" */
    bool do_long_long;

    /* Bytes used in buffer */
    size_t n;

    /* Bytes used in format sequence */
    size_t q;

    /* Format sequence */
    char aux[128];

    /* Resulting string */
    char tmp[MSG_LEN];

    my_assert(max);
    my_assert(fmt);

    /* Begin the buffer */
    n = 0;

    /* Begin the format string */
    s = fmt;

    /* Scan the format string */
    while (true)
    {
        /* All done */
        if (!*s) break;

        /* Normal character */
        if (*s != '%')
        {
            /* Check total length */
            if (n == max - 1) break;

            /* Save the character */
            buf[n++] = *s++;

            /* Continue */
            continue;
        }

        /* Skip the "percent" */
        s++;

        /* Pre-process "%%" */
        if (*s == '%')
        {
            /* Check total length */
            if (n == max - 1) break;

            /* Save the percent */
            buf[n++] = '%';

            /* Skip the "%" */
            s++;

            /* Continue */
            continue;
        }

        /* Pre-process "%n" */
        if (*s == 'n')
        {
            size_t *arg;

            /* Get the next argument */
            arg = va_arg(vp, size_t *);

            /* Save the current length */
            (*arg) = n;

            /* Skip the "n" */
            s++;

            /* Continue */
            continue;
        }

        /* Begin the "aux" string */
        q = 0;

        /* Save the "percent" */
        aux[q++] = '%';

        /* Assume no "short" argument */
        do_short = false;

        /* Assume no "long" argument */
        do_long = false;

        /* Assume no "long long" argument */
        do_long_long = false;

        /* Build the "aux" string */
        while (true)
        {
            /* Error -- format sequence is not terminated */
            if (!*s)
            {
                /* Terminate the buffer */
                buf[0] = '\0';

                /* Return "error" */
                return (0);
            }

            /* Error -- format sequence may be too long */
            if (q > 100)
            {
                /* Terminate the buffer */
                buf[0] = '\0';

                /* Return "error" */
                return (0);
            }

            /* Handle "alphabetic" or "non-alphabetic"chars */
            if (isalpha((unsigned char)*s))
            {
                /* Hack -- handle "short" request */
                if (*s == 'h')
                {
                    /* Save the character */
                    aux[q++] = *s++;

                    /* Note the "short" flag */
                    do_short = true;
                }

                /* Hack -- handle "long" request */
                else if (*s == 'l')
                {
                    /* Save the character */
                    aux[q++] = *s++;

                    /* Note the "long" flag */
                    if (do_long)
                    {
                        /* Already long, become "extra-long" */
                        do_long = false;
                        do_long_long = true;
                    }
                    else
                        do_long = true;
                }

                /* Hack -- handle "extra-long" request */
                else if (*s == 'L')
                {
                    /* Error -- illegal format char */
                    buf[0] = '\0';

                    /* Return "error" */
                    return (0);
                }

                /* Hack -- handle I64 request */
                else if (*s == 'I')
                {
                    int numbits = 0;
                    char bitorder[4] = {0};

                    /* Save the character */
                    aux[q++] = *s++;

                    while (*s && isdigit(*s))
                    {
                        bitorder[numbits++] = *s;
                        aux[q++] = *s++;
                        if (numbits > 2) break;
                    }
                    if (numbits == 2 && bitorder[0] == '6' && bitorder[1] == '4')
                        do_long_long = true;
                    else if (numbits == 2 && bitorder[0] == '3' && bitorder[1] == '2')
                    {
                        /* Just an int */
                    }
                    else if (numbits == 1 && bitorder[0] == '8')
                        do_short = true;
                    else
                    {
                        /* Error -- illegal format char */
                        buf[0] = '\0';

                        /* Return "error" */
                        return (0);
                    }

                    if (*s == 'd' || *s == 'u')
                        aux[q++] = *s++;
                    else
                    {
                        /* Error -- illegal format char */
                        buf[0] = '\0';

                        /* Return "error" */
                        return (0);
                    }

                    /* Eh... */
                    do_long_long = true;

                    /* Stop processing the format sequence */
                    break;
                }

                /* Handle normal end of format sequence */
                else
                {
                    /* Save the character */
                    aux[q++] = *s++;

                    /* Stop processing the format sequence */
                    break;
                }
            }
            else
            {
                /* Hack -- handle 'star' (for "variable length" argument) */
                if (*s == '*')
                {
                    int arg;

                    /* Get the next argument */
                    arg = va_arg(vp, int);

                    /* Hack -- append the "length" */
                    snprintf(aux + q, sizeof(aux) - q, "%d", arg);

                    /* Hack -- accept the "length" */
                    while (aux[q]) q++;

                    /* Skip the "*" */
                    s++;
                }
                else
                {
                    /* Save the character */
                    aux[q++] = *s++;
                }
            }
        }

        /* Terminate "aux" */
        aux[q] = '\0';

        /* Clear "tmp" */
        tmp[0] = '\0';

        /* Process the "format" symbol */
        switch (aux[q - 1])
        {
            /* Simple Character -- standard format */
            case 'c':
            {
                int arg;

                /* Get the next argument */
                arg = va_arg(vp, int);

                /* Format the argument */
                snprintf(tmp, sizeof(tmp), aux, arg);

                /* Done */
                break;
            }

            /* Signed Integers -- standard format */
            case 'd': case 'i':
            {
                if (do_short)
                {
                    short arg;

                    /* Get the next argument */
                    arg = va_arg(vp, short);

                    /* Format the argument */
                    snprintf(tmp, sizeof(tmp), aux, arg);
                }
                else if (do_long)
                {
                    long arg;

                    /* Get the next argument */
                    arg = va_arg(vp, long);

                    /* Format the argument */
                    snprintf(tmp, sizeof(tmp), aux, arg);
                }
                else if (do_long_long)
                {
                    long long arg;

                    /* Get the next argument */
                    arg = va_arg(vp, long long);

                    /* Format the argument */
                    snprintf(tmp, sizeof(tmp), aux, arg);
                }
                else
                {
                    int arg;

                    /* Get the next argument */
                    arg = va_arg(vp, int);

                    /* Format the argument */
                    snprintf(tmp, sizeof(tmp), aux, arg);
                }

                /* Done */
                break;
            }

            /* Unsigned Integers -- various formats */
            case 'u': case 'o': case 'x': case 'X':
            {
                if (do_short)
                {
                    unsigned short arg;

                    /* Get the next argument */
                    arg = va_arg(vp, unsigned short);

                    /* Format the argument */
                    snprintf(tmp, sizeof(tmp), aux, arg);
                }
                else if (do_long)
                {
                    unsigned long arg;

                    /* Get the next argument */
                    arg = va_arg(vp, unsigned long);

                    /* Format the argument */
                    snprintf(tmp, sizeof(tmp), aux, arg);
                }
                else if (do_long_long)
                {
                    unsigned long long arg;

                    /* Get the next argument */
                    arg = va_arg(vp, unsigned long long);

                    /* Format the argument */
                    snprintf(tmp, sizeof(tmp), aux, arg);
                }
                else
                {
                    unsigned int arg;

                    /* Get the next argument */
                    arg = va_arg(vp, unsigned int);

                    /* Format the argument */
                    snprintf(tmp, sizeof(tmp), aux, arg);
                }

                /* Done */
                break;
            }

            /* Floating Point -- various formats */
            case 'f':
            case 'e': case 'E':
            case 'g': case 'G':
            {
                double arg;

                /* Get the next argument */
                arg = va_arg(vp, double);

                /* Format the argument */
                snprintf(tmp, sizeof(tmp), aux, arg);

                /* Done */
                break;
            }

            /* Pointer -- implementation varies */
            case 'p':
            {
                void *arg;

                /* Get the next argument */
                arg = va_arg(vp, void*);

                /* Format the argument */
                snprintf(tmp, sizeof(tmp), aux, arg);

                /* Done */
                break;
            }

            /* String */
            case 's':
            {
                const char *arg;
                char arg2[MSG_LEN];

                /* Get the next argument */
                arg = va_arg(vp, const char *);

                /* Hack -- convert NULL to EMPTY */
                if (!arg) arg = "";

                /* Prevent buffer overflows */
                my_strcpy(arg2, arg, sizeof(arg2));

                /* Format the argument */
                snprintf(tmp, sizeof(tmp), aux, arg2);

                /* Done */
                break;
            }

            /* Binary */
            case 'b':
            {
                int arg;
                size_t i, max = 32;
                u32b bitmask;
                char out[32 + 1];

                /* Get the next argument */
                arg = va_arg(vp, int);

                /* Check our aux string */
                switch (aux[0])
                {
                    case '1': max = 2;  break;
                    case '2': max = 4;  break;
                    case '3': max = 8;  break;
                    case '4': max = 16; break;
                    default:  max = 32; break;
                }

                /* Format specially */
                for (i = 1; i <= max; i++, bitmask *= 2)
                {
                    if (arg & bitmask) out[max - i] = '1';
                    else out[max - i] = '0';
                }

                /* Terminate */
                out[max] = '\0';

                /* Append the argument */
                my_strcpy(tmp, out, sizeof(tmp));

                /* Done */
                break;
            }

            /* Oops */
            default:
            {
                /* Error -- illegal format char */
                buf[0] = '\0';

                /* Return "error" */
                return (0);
            }
        }

        /* Now append "tmp" to "buf" */
        for (q = 0; tmp[q]; q++)
        {
            /* Check total length */
            if (n == max - 1) break;

            /* Save the character */
            buf[n++] = tmp[q];
        }
    }

    /* Terminate buffer */
    buf[n] = '\0';

    /* Return length */
    return (n);
}


/*
 * Add a formatted string to the end of a string
 */
void strnfcat(char *str, size_t max, size_t *end, const char *fmt, ...)
{
    size_t len;

    va_list vp;

    /* Paranoia */
    if (*end >= max) return;

    /* Begin the Varargs Stuff */
    va_start(vp, fmt);

    /* Build the string */
    len = vstrnfmt(&str[*end], max - *end, fmt, vp);

    /* End the Varargs Stuff */
    va_end(vp);

    /* Change the end value */
    *end += len;
}


#define FORMAT_CYCLE_MAX 5
static int format_cycle = 0;
static char *format_buf[FORMAT_CYCLE_MAX] = { NULL };
static size_t format_len[FORMAT_CYCLE_MAX] = { 0 };


#ifdef WINDOWS
#define VA_COPY(DST, SRC) (DST) = (SRC)
#else
#define VA_COPY(DST, SRC) va_copy(DST, SRC)
#endif


/*
 * Do a vstrnfmt (see above) into a (growable) static buffer.
 * This buffer is usable for very short term formatting of results.
 */
char *vformat(const char *fmt, va_list vp)
{
    char *ret;

    /* Initial allocation */
    if (!format_buf[format_cycle])
    {
        format_len[format_cycle] = 1024;
        format_buf[format_cycle] = mem_zalloc(format_len[format_cycle]);
    }

    /* Null format yields last result */
    if (!fmt) return (format_buf[format_cycle]);

    /* Keep going until successful */
    while (1)
    {
        va_list args;
        size_t len;

        /* Build the string */
        VA_COPY(args, vp);
        len = vstrnfmt(format_buf[format_cycle], format_len[format_cycle], fmt, args);
        va_end(args);

        /* Success */
        if (len < format_len[format_cycle] - 1) break;

        /* Grow the buffer */
        format_len[format_cycle] = format_len[format_cycle] * 2;
        format_buf[format_cycle] = mem_realloc(format_buf[format_cycle], format_len[format_cycle]);
    }

    /* Increase cycle counter */
    ret = format_buf[format_cycle];
    format_cycle++;
    if (format_cycle >= FORMAT_CYCLE_MAX) format_cycle = 0;

    /* Return the new buffer */
    return (ret);
}


void vformat_kill(void)
{
    int i;

    for (i = 0; i < FORMAT_CYCLE_MAX; i++)
        mem_free(format_buf[i]);
}


/*
 * Do a vstrnfmt (see above) into a buffer of a given size.
 */
size_t strnfmt(char *buf, size_t max, const char *fmt, ...)
{
    size_t len;
    va_list vp;

    /* Begin the Varargs Stuff */
    va_start(vp, fmt);

    /* Do the va_arg fmt to the buffer */
    len = vstrnfmt(buf, max, fmt, vp);

    /* End the Varargs Stuff */
    va_end(vp);

    /* Return the number of bytes written */
    return (len);
}


/*
 * Do a vstrnfmt() into (see above) into a (growable) static buffer.
 * This buffer is usable for very short term formatting of results.
 * Note that the buffer is (technically) writable, but only up to
 * the length of the string contained inside it.
 */
char *format(const char *fmt, ...)
{
    char *res;
    va_list vp;

    /* Begin the Varargs Stuff */
    va_start(vp, fmt);

    /* Format the args */
    res = vformat(fmt, vp);

    /* End the Varargs Stuff */
    va_end(vp);

    /* Return the result */
    return (res);
}


/*
 * Vararg interface to plog()
 */
void plog_fmt(const char *fmt, ...)
{
    char *res;
    va_list vp;

    /* Begin the Varargs Stuff */
    va_start(vp, fmt);

    /* Format the args */
    res = vformat(fmt, vp);

    /* End the Varargs Stuff */
    va_end(vp);

    /* Call plog */
    plog(res);
}


/*
 * Vararg interface to quit()
 */
void quit_fmt(const char *fmt, ...)
{
    char *res;
    va_list vp;

    /* Begin the Varargs Stuff */
    va_start(vp, fmt);

    /* Format */
    res = vformat(fmt, vp);

    /* End the Varargs Stuff */
    va_end(vp);

    /* Call quit() */
    quit(res);
}
