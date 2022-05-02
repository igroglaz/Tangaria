/*
 * File: snd-sdl.c
 * Purpose: SDL sound support
 *
 * Copyright (c) 2004 Brendon Oliver <brendon.oliver@gmail.com>
 * Copyright (c) 2007 Andi Sidwell <andi@takkaria.org>
 * Copyright (c) 2016 Graeme Russ <graeme.russ@gmail.com>
 * Copyright (c) 2022 MAngband and PWMAngband Developers
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

#ifdef SOUND_SDL
# ifdef WINDOWS
#  include "..\_SDL\SDL.h"
#  include "..\_SDL\SDL_mixer.h"
# else
#  include <SDL/SDL.h>
#  include <SDL/SDL_mixer.h>
# endif
#endif /* USE_SDL */

#ifdef SOUND_SDL2
# ifdef WINDOWS
#  include "..\_SDL2\SDL.h"
#  include "..\_SDL2\SDL_mixer.h"
# else
#  include <SDL.h>
#  include <SDL_mixer.h>
# endif
#endif /* USE_SDL2 */

/* Supported file types */
enum
{
    SDL_NULL = 0,
    SDL_CHUNK_LOOP,
    SDL_CHUNK,
    SDL_MUSIC
};


static const struct sound_file_type supported_sound_files[] =
{
    {".ogg", SDL_CHUNK},
    {".ogg0", SDL_CHUNK_LOOP},
    {".mp3", SDL_MUSIC},
    {"", SDL_NULL}
};


/*
 * Struct representing all data about an event sample
 */
typedef struct
{
    union
    {
        Mix_Chunk *chunk;   /* Sample in WAVE format */
        Mix_Chunk *chunk_loop;   /* Sample in WAVE format with looping */
        Mix_Music *music;   /* Sample in MP3 format */
    } sample_data;
    int sample_type;
} sdl_sample;


static bool use_init = false;
static Mix_Music *music = NULL;


static bool play_music_aux(const char *dirpath)
{
    ang_dir *dir;
    char buf[MSG_LEN];
    int count = 0, pick;
    char musicpath[MSG_LEN];

    /* Check directory existence */
    if (!dir_exists(dirpath)) return false;
    dir = my_dopen(dirpath);
    if (!dir) return false;

    /* Count every music file */
    while (my_dread(dir, buf, sizeof(buf)))
    {
        /* Check for file extension */
        if (suffix(buf, ".mp3") || suffix(buf, ".MP3") || suffix(buf, ".ogg") || suffix(buf, ".OGG"))
            count++;
    }
    my_dclose(dir);
    if (!count) return false;

    /* Pick a file */
    pick = randint1(count);
    count = 0;
    dir = my_dopen(dirpath);
    while (my_dread(dir, buf, sizeof(buf)))
    {
        if (suffix(buf, ".mp3") || suffix(buf, ".MP3") || suffix(buf, ".ogg") || suffix(buf, ".OGG"))
            count++;
        if (count == pick) break;
    }
    my_dclose(dir);

    /* Load music file */
    if (music) Mix_FreeMusic(music);
    path_build(musicpath, sizeof(musicpath), dirpath, buf);
    music = Mix_LoadMUS(musicpath);
    if (!music) return false;

    /* Adjust music volume if needed */
    if (music_volume != current_music_volume)
    {
        current_music_volume = music_volume;
        Mix_VolumeMusic((music_volume * MIX_MAX_VOLUME) / 100);
    }

    /* Play music file (once) */
    Mix_PlayMusic(music, 1);
    return true;
}


static void play_music_sdl(void)
{
    char dirpath[MSG_LEN];
    bool played = false;

    /* If music_volume = 0 then disable music playback */
    if (music_volume == 0) return;

    /* Check main music directory */
    path_build(dirpath, sizeof(dirpath), ANGBAND_DIR_MUSIC, "");
    if (!dir_exists(dirpath)) return;

    /* Check location */
    if (!STRZERO(player->locname))
    {
        /* Play music from corresponding music subdirectory */
        path_build(dirpath, sizeof(dirpath), ANGBAND_DIR_MUSIC, player->locname);
        played = play_music_aux(dirpath);

        /* Hack -- don't fall back for intro music */
        if (streq(player->locname, "intro")) return;

        /* If this didn't work, try default music subdirectory */
        if (!played)
        {
            /* For the dungeons, try generic subdirectory */
            if (player->wpos.depth > 0)
            {
                path_build(dirpath, sizeof(dirpath), ANGBAND_DIR_MUSIC, "generic-dungeon");
                played = play_music_aux(dirpath);
            }

            /* For the towns, try daytime/nighttime subdirectory first */
            else
            {
                if (player->no_disturb_icky)
                    path_build(dirpath, sizeof(dirpath), ANGBAND_DIR_MUSIC, "town-day");
                else
                    path_build(dirpath, sizeof(dirpath), ANGBAND_DIR_MUSIC, "town-night");
                played = play_music_aux(dirpath);

                /* If this didn't work, try generic subdirectory */
                if (!played)
                {
                    path_build(dirpath, sizeof(dirpath), ANGBAND_DIR_MUSIC, "generic-town");
                    played = play_music_aux(dirpath);
                }
            }
        }
    }

    /* If we still didn't play music yet, try main music directory */
    if (!played)
    {
        path_build(dirpath, sizeof(dirpath), ANGBAND_DIR_MUSIC, "");
        play_music_aux(dirpath);
    }
}


/*
 * Initialize SDL and open the mixer.
 */
static bool open_audio_sdl(void)
{
    int audio_rate;
    Uint16 audio_format;
    int audio_channels;

    /* Initialize variables */
    audio_rate = 22050;
    audio_format = AUDIO_S16;
    audio_channels = 2;

    /* Initialize the SDL library */
    if (SDL_Init(SDL_INIT_AUDIO) < 0)
    {
        plog_fmt("Couldn't initialize SDL: %s", SDL_GetError());
        return false;
    }

    /* Try to open the audio */
    if (Mix_OpenAudio(audio_rate, audio_format, audio_channels, 4096) < 0)
    {
        plog_fmt("Couldn't open mixer: %s", SDL_GetError());
        return false;
    }

    /* Callback for music */
    Mix_HookMusicFinished(play_music_sdl);

    /* Success */
    return true;
}


/*
 * Load a sound from file.
 */
static bool load_sample_sdl(const char *filename, int ft, sdl_sample *sample)
{
    switch (ft)
    {
        case SDL_CHUNK:
        {
            if (!use_init)
            {
                Mix_Init(MIX_INIT_OGG);
                use_init = true;
            }

            sample->sample_data.chunk = Mix_LoadWAV(filename);
            if (sample->sample_data.chunk) return true;
            break;
        }

        case SDL_CHUNK_LOOP:
        {
            if (!use_init)
            {
                Mix_Init(MIX_INIT_OGG);
                use_init = true;
            }

            sample->sample_data.chunk_loop = Mix_LoadWAV(filename);
            if (sample->sample_data.chunk_loop) return true;
            break;
        }

        case SDL_MUSIC:
        {
            if (sample->sample_data.music) Mix_FreeMusic(sample->sample_data.music);
            sample->sample_data.music = Mix_LoadMUS(filename);
            if (sample->sample_data.music) return true;
            break;
        }

        default:
            plog("Oops - Unsupported file type");
            break;
    }

    return false;
}


/*
 * Load a sound and return a pointer to the associated SDL Sound data
 * structure back to the core sound module.
 */
static bool load_sound_sdl(const char *filename, int ft, struct sound_data *data)
{
    sdl_sample *sample = (sdl_sample *)(data->plat_data);

    if (!sample) sample = mem_zalloc(sizeof(*sample));

    /* Try and load the sample file */
    data->loaded = load_sample_sdl(filename, ft, sample);

    if (data->loaded)
        sample->sample_type = ft;
    else
    {
        mem_free(sample);
        sample = NULL;
    }

    data->plat_data = (void *)sample;

    return (NULL != sample);
}


/*
 * Play the sound stored in the provided SDL Sound data structure.
 */
static bool play_sound_sdl(struct sound_data *data)
{
    sdl_sample *sample;

    /* Play some music */
    if (data == NULL)
    {
        play_music_sdl();
        return true;
    }

    /* If sound name is 'silent' then stop playing */
    if (streq(data->name, "silent"))
    {
        /* Halt playback on all channels */
        Mix_HaltChannel(-1);
    }
    else if (streq(data->name, "silent0"))
    {
        /* Stop looped playback */
        if (Mix_Playing(7)) Mix_HaltChannel(7);
    }

    sample = (sdl_sample *)(data->plat_data);
    if (sample)
    {
        switch (sample->sample_type)
        {
            case SDL_CHUNK:
            {
                /* If another sound is currently playing, stop it */
                /*Mix_HaltChannel(-1);*/

                if (sample->sample_data.chunk)
                {
                    /* Adjust sound volume if needed */
                    if (sound_volume != current_sound_volume)
                    {
                        current_sound_volume = sound_volume;
                        Mix_Volume(-1, (sound_volume * MIX_MAX_VOLUME) / 100);
                    }

                    return (0 == Mix_PlayChannel(-1, sample->sample_data.chunk, 0));
                }
                break;
            }

            case SDL_CHUNK_LOOP:
            {
                if (sample->sample_data.chunk_loop)
                {
                    /* Adjust sound volume if needed */
                    if (sound_volume != current_sound_volume)
                    {
                        current_sound_volume = sound_volume;
                        Mix_Volume(-1, (sound_volume * MIX_MAX_VOLUME) / 100);
                    }

                    return (0 == Mix_PlayChannel(7, sample->sample_data.chunk_loop, -1));
                }
                break;
            }

            case SDL_MUSIC:
            {
                /* Hack -- force reload next time a sound is played */
                data->loaded = false;

                if (sample->sample_data.music)
                {
                    /* Adjust sound volume if needed */
                    if (music_volume != current_music_volume)
                    {
                        current_music_volume = music_volume;
                        Mix_VolumeMusic((music_volume * MIX_MAX_VOLUME) / 100);
                    }

                    return (0 == Mix_PlayMusic(sample->sample_data.music, 1));
                }
                break;
            }

            default: break;
        }
    }

    return false;
}


/*
 * Free resources referenced in the provided SDL Sound data structure.
 */
static bool unload_sound_sdl(struct sound_data *data)
{
    sdl_sample *sample = (sdl_sample *)(data->plat_data);

    if (sample)
    {
        switch (sample->sample_type)
        {
            case SDL_CHUNK:
            {
                if (sample->sample_data.chunk)
                    Mix_FreeChunk(sample->sample_data.chunk);
                break;
            }

            case SDL_CHUNK_LOOP:
            {
                if (sample->sample_data.chunk_loop)
                    Mix_FreeChunk(sample->sample_data.chunk_loop);
                break;
            }

            case SDL_MUSIC:
            {
                if (sample->sample_data.music)
                    Mix_FreeMusic(sample->sample_data.music);
                break;
            }

            default: break;
        }

        mem_free(sample);
        data->plat_data = NULL;
        data->loaded = false;
    }

    return true;
}


/*
 * Shut down the SDL sound module and free resources.
 */
static bool close_audio_sdl(void)
{
    Mix_HookMusicFinished(NULL);
    if (music) Mix_FreeMusic(music);
    music = NULL;
    if (use_init) Mix_Quit();

    /*
     * Close the audio
     *
     * NOTE: All samples will have been free'd by the sound subsystem
     * calling unload_sound_sdl() for every sample that was loaded.
     */
    Mix_CloseAudio();

    SDL_QuitSubSystem(SDL_INIT_AUDIO);

    return true;
}


static const struct sound_file_type *supported_files_sdl(void)
{
    return supported_sound_files;
}


/*
 * Init the SDL sound "module".
 */
errr init_sound_sdl(struct sound_hooks *hooks)
{
    hooks->open_audio_hook = open_audio_sdl;
    hooks->supported_files_hook = supported_files_sdl;
    hooks->close_audio_hook = close_audio_sdl;
    hooks->load_sound_hook = load_sound_sdl;
    hooks->unload_sound_hook = unload_sound_sdl;
    hooks->play_sound_hook = play_sound_sdl;

    /* Success */
    return (0);
}
