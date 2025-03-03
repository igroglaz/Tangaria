/*
 * File: main.h
 * Purpose: Core game initialisation
 */

#ifndef INCLUDED_MAIN_H
#define INCLUDED_MAIN_H

struct module
{
    const char *name;
    errr (*init)(int argc, char **argv);
};

#ifdef USE_SDL
extern errr init_sdl(int argc, char **argv);
#endif
#ifdef USE_SDL2
extern errr init_sdl2(int argc, char **argv);
#endif
#ifdef USE_GCU
extern errr init_gcu(int argc, char **argv);
#endif

#endif /* INCLUDED_MAIN_H */
