/*
 * File: h-basic.h
 * Purpose: The most basic "include" file
 */

#ifndef INCLUDED_H_BASIC_H
#define INCLUDED_H_BASIC_H

/*** Autodetect platform ***/
/**
 * Include autoconf autodetections, otherwise try to autodetect ourselves
 */
#ifdef HAVE_CONFIG_H

# include "autoconf.h"

#else

/**
 * Everyone except RISC OS has fcntl.h and sys/stat.h
 */
#define HAVE_FCNTL_H
#define HAVE_STAT

#endif

/**
 * Extract the "WINDOWS" flag from the compiler
 */
# if defined(_Windows) || defined(__WINDOWS__) || \
     defined(__WIN32__) || defined(WIN32) || \
     defined(__WINNT__) || defined(__NT__)
#  ifndef WINDOWS
#   define WINDOWS
#  endif
# endif

/* ANDROID */
#ifdef ON_ANDROID
/* Any files you put in the "app/src/main/assets" directory of your project 
 * directory will get bundled into the application package and you can load 
 * them using the standard functions in SDL_rwops.h. */
/* #define USE_SDL_RWOPS */

/* Path to the game's configuration data */
#define DEFAULT_CONFIG_PATH "/lib/"
/* Path to the game's variable data */
#define DEFAULT_DATA_PATH "/lib/"
/* Path to the game's lib directory */
#define DEFAULT_LIB_PATH "/lib/"
/* Define to 1 if you have the <dirent.h> header file, and it defines `DIR'. */
#define HAVE_DIRENT_H 1
/* Define to 1 if you have the <fcntl.h> header file. */
#define HAVE_FCNTL_H 1
/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1
/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1
/* Define to 1 if you have the `mkdir' function. */
#define HAVE_MKDIR 1
/* Define to 1 if you have the `setegid' function. */
#define HAVE_SETEGID 1
/* Define to 1 if you have the `setresgid' function. */
#define HAVE_SETRESGID 1
/* Define to 1 if you have the `stat' function. */
#define HAVE_STAT 1
/* Define to 1 if stdbool.h conforms to C99. */
#define HAVE_STDBOOL_H 1
/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1
/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1
/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1
/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1
/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1
/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1
/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1
/* Define to 1 if the system has the type `_Bool'. */
#define HAVE__BOOL 1
/* Define as the return type of signal handlers (`int' or `void'). */
#define RETSIGTYPE void
/* Define to 1 if including sound support. */
#define SOUND 1
/* Define to 1 if using SDL2_mixer sound support and it's found. */
#define SOUND_SDL2 1
/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1
/* Define to 1 if using the SDL2 interface and SDL2 is found. */
#define USE_SDL2 1
/*
#include <android/log.h>
#define LOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, "PWMAngband", __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG  , "PWMAngband", __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO   , "PWMAngband", __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN   , "PWMAngband", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR  , "PWMAngband", __VA_ARGS__)
#define LOG(...) __android_log_print(ANDROID_LOG_DEBUG  , "PWMAngband", __VA_ARGS__)
*/
#endif

/**
 * Define UNIX if our OS is UNIXy
 */
#if !defined(WINDOWS) && !defined(GAMEBOY) && !defined(NDS)
# define UNIX

# ifndef HAVE_DIRENT_H
#  define HAVE_DIRENT_H
# endif
#endif


/*
 * Using C99, assume we have stdint and stdbool
 *
 * Note: I build PWMAngband with C++ Builder, which DOES NOT have stdbool
 */

/*
 * Every system seems to use its own symbol as a path separator.
 */
#undef PATH_SEP
#undef PATH_SEPC
#ifdef WINDOWS
#define PATH_SEP "\\"
#define PATH_SEPC '\\'
#else
#define PATH_SEP "/"
#define PATH_SEPC '/'
#endif

#ifdef WINDOWS
#define EWOULDBLOCK WSAEWOULDBLOCK
#define ECONNRESET WSAECONNRESET
#endif

/*
 * Include the library header files
 */

/** ANSI C headers **/

#include <ctype.h>
#include <stdint.h>
/*#include <stdbool.h>*/
#include <errno.h>
#include <assert.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>

/** Other headers **/

#ifdef WINDOWS
#include <io.h>
#include <fcntl.h>
#endif

/* Basic networking stuff */
#include "h-net.h"

#ifndef WINDOWS

/* Use various POSIX functions if available */
#undef _GNU_SOURCE
#define _GNU_SOURCE

/** ANSI C headers **/
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <errno.h>
#include <limits.h>
#include <assert.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <time.h>

#include <wchar.h>
#include <wctype.h>

/** POSIX headers **/
#define UNIX
#ifdef UNIX
# include <pwd.h>
# include <sys/stat.h>
# include <unistd.h>
#endif

#endif

/*
 * Define the basic game types
 */

typedef int errr;

/* Use a char otherwise */
#ifndef bool
typedef char bool;
#endif

#define true    1
#define false   0

/* Use guaranteed-size types */
typedef uint8_t byte;

typedef uint16_t u16b;
typedef int16_t s16b;

typedef uint32_t u32b;
typedef int32_t s32b;

typedef uint64_t u64b;
typedef int64_t s64b;

/* MAngband hacks */
#if (UINT_MAX == 0xFFFFFFFFUL) && (ULONG_MAX > 0xFFFFFFFFUL)
    #define PRId32 "d"
#else
    #define PRId32 "ld"
#endif

/* MAngband types */
typedef int sint;
typedef unsigned int uint;
typedef byte *byte_ptr;

/* Turn counter type "huge turn" (largest number ever) */
#define HTURN_ERA_FLIP 1000000
#define HTURN_ERA_MAX_DIV 1000

typedef struct
{
    u32b era;
    u32b turn;
} hturn;

/*
 * Basic math macros
 */

#undef MIN
#undef MAX
#undef ABS
#undef SGN
#undef CMP

#define MIN(a,b)    (((a) > (b))? (b)   : (a))
#define MAX(a,b)    (((a) < (b))? (b)   : (a))
#define ABS(a)      (((a) < 0)  ? (-(a)): (a))
#define SGN(a)      (((a) < 0)  ? (-1)  : ((a) != 0))
#define CMP(a,b)    (((a) < (b))? (-1)  : (((b) < (a))? 1: 0))

/*
 * Useful fairly generic macros
 */

/*
 * Given an array, determine how many elements are in it.
 */
#define N_ELEMENTS(a) (sizeof(a) / sizeof((a)[0]))

/*
 * Some hackish character manipulation
 */

/*
 * Note that all "index" values must be "lowercase letters", while
 * all "digits" must be "digits".  Control characters can be made
 * from any legal characters.  XXX XXX XXX
 */
#define A2I(X)      ((X) - 'a')
#define I2A(X)      ((X) + 'a')
#define D2I(X)      ((X) - '0')
#define I2D(X)      ((X) + '0')

/*
 * Force a character to uppercase
 */
#define FORCEUPPER(A)  ((islower((A)))? toupper((A)): (A))

#ifndef WINDOWS
#ifndef MSG_LEN
# define MSG_LEN 256
#endif

#endif

#endif
