/*
 * File: sound.h
 * Purpose: Sound handling
 */

#ifndef INCLUDED_SOUND_H
#define INCLUDED_SOUND_H

/*
 * Structure to held data relating to a sound.
 *  name :      Base name of the sound (no path or file extension)
 *  hash :      Used to speed up searches
 *  status :    If nothing has yet been done with the sound, this will be
 *              SOUND_ST_UNKNOWN. If the sound file exists but the platform's
 *              sound module did not have enough information to be able to play
 *              the sound (that is up to the platform's module; it could mean
 *              that the sound has been loaded into memory or that the full
 *              file name has been stored in the platform's data), this will be
 *              SOUND_ST_ERROR. Otherwise, this will be SOUND_ST_LOADED.
 *              The core sound module will check if status is SOUND_ST_LOADED
 *              before attempting to play it.
 *  plat_data : Platform specific structure used to store any additional
 *              data the platform's sound module needs in order to play the
 *              sound (and release resources when shut down)
 */
enum sound_status
{
    SOUND_ST_UNKNOWN = 0,
    SOUND_ST_ERROR,
    SOUND_ST_LOADED
};

struct sound_data
{
    char *name;
    uint32_t hash;
    enum sound_status status;
    void *plat_data;
};

struct sound_file_type
{
    const char *extension;
    int type;
};

struct sound_hooks
{
    bool (*open_audio_hook)(void);
    const struct sound_file_type *(*supported_files_hook)(void);
    bool (*close_audio_hook)(void);
    bool (*load_sound_hook)(const char *filename, int ft, struct sound_data *data);
    bool (*unload_sound_hook)(struct sound_data *data);
    bool (*play_sound_hook)(struct sound_data *data);
};

extern int current_sound_volume;
extern int sound_volume;
extern int current_music_volume;
extern int music_volume;

extern errr init_sound(void);
extern void close_sound(void);
extern void play_intro_music(void);
extern bool is_sound_inited(void);
extern errr register_sound_pref_parser(struct parser *p);

#endif /* INCLUDED_SOUND_H */
