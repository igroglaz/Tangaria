/*
 * File: ui-init.c
 * Purpose: Various game initialisation routines
 *
 * Copyright (c) 1997 Ben Harrison
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

/*
 * This file should contain non-system-specific code.  If a
 * specific system needs its own "main" function (such as
 * Windows), then it should be placed in the "main-???.c" file.
 */


#include "c-angband.h"


struct feature *f_info;


/* Connection parameters */
char meta_address[NORMAL_WID];
int meta_port;
char account[NORMAL_WID];
char nick[NORMAL_WID];
char pass[NORMAL_WID];
char stored_pass[NORMAL_WID];
char real_name[NORMAL_WID];
char server_name[NORMAL_WID];
int server_port;
bool play_again = false;


/* Character list */
uint16_t max_account_chars;
uint16_t char_num;
char **char_name;
char *char_expiry;


static int Socket;


/* Free the sub-paths */
static void free_file_paths(void)
{
    string_free(ANGBAND_DIR_CUSTOMIZE);
    ANGBAND_DIR_CUSTOMIZE = NULL;
    string_free(ANGBAND_DIR_SCREENS);
    ANGBAND_DIR_SCREENS = NULL;
    string_free(ANGBAND_DIR_FONTS);
    ANGBAND_DIR_FONTS = NULL;
    string_free(ANGBAND_DIR_TILES);
    ANGBAND_DIR_TILES = NULL;
    string_free(ANGBAND_DIR_SOUNDS);
    ANGBAND_DIR_SOUNDS = NULL;
    string_free(ANGBAND_DIR_MUSIC);
    ANGBAND_DIR_MUSIC = NULL;
    string_free(ANGBAND_DIR_ICONS);
    ANGBAND_DIR_ICONS = NULL;
    string_free(ANGBAND_DIR_USER);
    ANGBAND_DIR_USER = NULL;
}


/*
 * Find the default paths to all of our important sub-directories.
 *
 * All of the sub-directories should, for a single-user install, be
 * located inside the main directory, whose location is very system-dependent.
 * For shared installations, typically on Unix or Linux systems, the
 * directories may be scattered - see config.h for more info.
 *
 * This function takes buffers, holding the paths to the "config", "lib",
 * and "data" directories (for example, those could be "/etc/angband/",
 * "/usr/share/angband", and "/var/games/angband"). Some system-dependent
 * expansion/substitution may be done when copying those base paths to the
 * paths Angband uses: see path_process() in z-file.c for details (Unix
 * implementations, for instance, try to replace a leading ~ or ~username with
 * the path to a home directory).
 *
 * Various command line options may allow some of the important
 * directories to be changed to user-specified directories, most
 * importantly, the "scores" and "user" and "save" directories,
 * but this is done after this function, see "main.c".
 *
 * In general, the initial path should end in the appropriate "PATH_SEP"
 * string.  All of the "sub-directory" paths (created below or supplied
 * by the user) will NOT end in the "PATH_SEP" string, see the special
 * "path_build()" function in "util.c" for more information.
 *
 * Hack -- first we free all the strings, since this is known
 * to succeed even if the strings have not been allocated yet,
 * as long as the variables start out as "NULL".  This allows
 * this function to be called multiple times, for example, to
 * try several base "path" values until a good one is found.
 */
void init_file_paths(const char *configpath, const char *libpath, const char *datapath)
{
    char buf[MSG_LEN];

    /*** Free everything ***/

    /* Free the sub-paths */
    free_file_paths();

    /*** Prepare the paths ***/

#define BUILD_DIRECTORY_PATH(dest, basepath, dirname) \
    path_build(buf, sizeof(buf), (basepath), (dirname)); \
    dest = string_make(buf);

    /* Paths generally containing configuration data for Angband. */
    BUILD_DIRECTORY_PATH(ANGBAND_DIR_CUSTOMIZE, configpath, "customize");
    BUILD_DIRECTORY_PATH(ANGBAND_DIR_SCREENS, libpath, "screens");
    BUILD_DIRECTORY_PATH(ANGBAND_DIR_FONTS, libpath, "fonts");
    BUILD_DIRECTORY_PATH(ANGBAND_DIR_TILES, libpath, "tiles");
    BUILD_DIRECTORY_PATH(ANGBAND_DIR_SOUNDS, libpath, "sounds");
    BUILD_DIRECTORY_PATH(ANGBAND_DIR_MUSIC, libpath, "music");
    BUILD_DIRECTORY_PATH(ANGBAND_DIR_ICONS, libpath, "icons");

#ifdef PRIVATE_USER_PATH
	/* Build the path to the user specific directory */
	if (strncmp(ANGBAND_SYS, "test", 4) == 0)
		path_build(buf, sizeof(buf), PRIVATE_USER_PATH, "Test");
	else
		path_build(buf, sizeof(buf), PRIVATE_USER_PATH, VERSION_NAME);
	ANGBAND_DIR_USER = string_make(buf);
#else /* !PRIVATE_USER_PATH */
#ifdef MACH_O_CARBON
	/* Remove any trailing separators, since some deeper path creation functions
	 * don't like directories with trailing slashes. */
	if (suffix(datapath, PATH_SEP)) {
		/* Hacky way to trim the separator. Since this is just for OS X, we can
		 * assume a one char separator. */
		int last_char_index = strlen(datapath) - 1;
		my_strcpy(buf, datapath, sizeof(buf));
		buf[last_char_index] = '\0';
		ANGBAND_DIR_USER = string_make(buf);
	}
	else {
		ANGBAND_DIR_USER = string_make(datapath);
	}
#else /* !MACH_O_CARBON */
	BUILD_DIRECTORY_PATH(ANGBAND_DIR_USER, datapath, "user");
#endif /* MACH_O_CARBON */
#endif /* PRIVATE_USER_PATH */

#undef BUILD_DIRECTORY_PATH
}


/*
 * Create any missing directories.
 *
 * We create only those dirs which may be empty. The others are assumed
 * to contain required files and therefore must exist at startup.
 */
void create_needed_dirs(void)
{
    char dirpath[MSG_LEN];

    path_build(dirpath, sizeof(dirpath), ANGBAND_DIR_USER, "");
    if (!dir_create(dirpath)) quit_fmt("Cannot create '%s'", dirpath);
}


static void init_arrays(void)
{
    /* Message variables */
    messages_init();

    /* Initialize the current store */
    memset(&current_store, 0, sizeof(current_store));
    current_store.owner = mem_zalloc(sizeof(struct owner));

    /* Clear Client_setup.k_attr */
    Client_setup.k_attr = NULL;
}


/* Init minor arrays */
static void init_minor(void)
{
    int i;

    /* Chat channels */
    player->on_channel = mem_zalloc(MAX_CHANNELS * sizeof(uint8_t));
    for (i = 0; i < MAX_CHANNELS; i++)
    {
        channels[i].name[0] = '\0';
        channels[i].id = 0;
        channels[i].num = 0;
        player->on_channel[i] = 0;
    }
    player->main_channel = 0;

    /* Term channels */
    player->remote_term = NTERM_WIN_OVERHEAD;
}


/*
 * Initialize player struct
 */
static void init_player(void)
{
    /* Create the player array, initialised with 0 */
    player = mem_zalloc(sizeof(*player));

    /* Allocate player sub-structs */
    player->upkeep = mem_zalloc(sizeof(struct player_upkeep));
    player->timed = mem_zalloc(TMD_MAX * sizeof(int16_t));

    options_init_defaults(&player->opts);
}


/*
 * Initialize and verify the file paths.
 *
 * Use the configured DEFAULT_*_PATH constants. Be sure to
 * set those properly when building...
 *
 * We must ensure that the path ends with "PATH_SEP" if needed,
 * since the "init_file_paths()" function will simply append the
 * relevant "sub-directory names" to the given path.
 */
void init_stuff(void)
{
    char configpath[MSG_LEN];
    char libpath[MSG_LEN];
    char datapath[MSG_LEN];

#ifdef ON_ANDROID
    char app_configpath[MSG_LEN];
    char app_libpath[MSG_LEN];
    char app_datapath[MSG_LEN];

    if (getenv("ANDROID_APP_PATH"))
    {
        my_strcpy(app_configpath, getenv("ANDROID_APP_PATH"), 1024);
        my_strcat(app_configpath, DEFAULT_CONFIG_PATH, 1024);
        my_strcpy(app_libpath, getenv("ANDROID_APP_PATH"), 1024);
        my_strcat(app_libpath, DEFAULT_LIB_PATH, 1024);
        my_strcpy(app_datapath, getenv("ANDROID_APP_PATH"), 1024);
        my_strcat(app_datapath, DEFAULT_DATA_PATH, 1024);
    }

    my_strcpy(configpath, app_configpath, sizeof(configpath));
    my_strcpy(libpath, app_libpath, sizeof(libpath));
    my_strcpy(datapath, app_datapath, sizeof(datapath));
    /* LOGD("app_configpath: %s", app_configpath); */
    /* LOGD("app_libpath: %s", app_libpath); */
    /* LOGD("app_datapath: %s", app_datapath); */
#else
    /* Use default */
    my_strcpy(configpath, DEFAULT_CONFIG_PATH, sizeof(configpath));
    my_strcpy(libpath, DEFAULT_LIB_PATH, sizeof(libpath));
    my_strcpy(datapath, DEFAULT_DATA_PATH, sizeof(datapath));
#endif
    /* Hack -- add a path separator (only if needed) */
    if (!suffix(configpath, PATH_SEP))
        my_strcat(configpath, PATH_SEP, sizeof(configpath));
    if (!suffix(libpath, PATH_SEP))
        my_strcat(libpath, PATH_SEP, sizeof(libpath));
    if (!suffix(datapath, PATH_SEP))
        my_strcat(datapath, PATH_SEP, sizeof(datapath));

    /* Initialize */
    init_file_paths(configpath, libpath, datapath);

    /* Create any missing directories */
    create_needed_dirs();
}


/*
 * Open all relevant pref files.
 */
static void initialize_all_pref_files(void)
{
    char buf[MSG_LEN];

    /* Process the "pref.prf" file */
    process_pref_file("pref.prf", false, false);

    /* Reset visuals */
    reset_visuals(true);

    /* Process the "window.prf" file */
    process_pref_file("window.prf", true, true);

    /* Process the "user.prf" file */
    process_pref_file("user.prf", true, true);

    /* Get the "PLAYER.prf" filename */
    strnfmt(buf, sizeof(buf), "%s.prf", strip_suffix(nick));

    /* Process the "PLAYER.prf" file, using the character name */
    process_pref_file(buf, true, true);

    /* Set up the subwindows */
    subwindows_reinit_flags();
    subwindows_init_flags();
}


/* Loop callback */
static void input_callback_end(bool inmap)
{
    /* Process any commands we got */
    textui_process_command();

    if (!inmap) return;

    /* Hack -- don't redraw the screen until we have all of it */
    if (last_line_info != -1) return;

    /* Flush input (now!) */
    flush_now();
}


/*
 * Loop, looking for net input and responding to keypresses.
 */
static void Input_loop(void)
{
    if (Net_flush() == -1)
    {
        send_quit = false;
        return;
    }

    /* Loop, looking for net input and responding to keypresses */
    Net_loop(NULL, NULL, input_callback_end, SCAN_OFF, false);
}


bool gather_settings(void)
{
    int i;
    int16_t new_settings[SETTING_MAX];
    bool changed = false;

    /* Initialize */
    for (i = 0; i < SETTING_MAX; i++) new_settings[i] = 0;

    /* Gather */
    new_settings[SETTING_USE_GRAPHICS] = use_graphics;
    new_settings[SETTING_SCREEN_COLS] = Term->wid - COL_MAP - 1;
    new_settings[SETTING_SCREEN_ROWS] = Term->hgt - ROW_MAP - 1;
    new_settings[SETTING_TILE_WID] = tile_width;
    new_settings[SETTING_TILE_HGT] = tile_height;
    new_settings[SETTING_TILE_DISTORTED] = tile_distorted;
    new_settings[SETTING_MAX_HGT] = Term->max_hgt;
    new_settings[SETTING_WINDOW_FLAG] = 0;
    for (i = 0; i < 8; i++) new_settings[SETTING_WINDOW_FLAG] |= window_flag[i];
    new_settings[SETTING_HITPOINT_WARN] = player->opts.hitpoint_warn;

    /* Change */
    for (i = 0; i < SETTING_MAX; i++)
    {
        if (Client_setup.settings[i] != new_settings[i])
        {
            Client_setup.settings[i] = new_settings[i];
            changed = true;
        }
    }

    /* Return */
    return changed;
}


/*
 * Client is ready to play call-back
 */
void client_ready(bool newchar)
{
    bool options[OPT_MAX];
    size_t opt;
    int i;

    /* Save birth options for new characters */
    for (opt = 0; newchar && (opt < OPT_MAX); opt++)
        options[opt] = player->opts.opt[opt];

    /* Initialize default option values */
    init_options(player->opts.opt);

    /* Initialize window options that will be overridden by the savefile */
    memset(window_flag, 0, sizeof(uint32_t) * ANGBAND_TERM_MAX);
    window_flag[0] |= (PW_PLAYER_2 | PW_STATUS);
    window_flag[1] |= (PW_MESSAGE);
    window_flag[2] |= (PW_EQUIP);
    window_flag[3] |= (PW_INVEN);
    window_flag[4] |= (PW_MESSAGE_CHAT);
    window_flag[5] |= (PW_MONLIST);
    window_flag[6] |= (PW_ITEMLIST);
    window_flag[7] |= (PW_MAP);

    /* Initialize the pref files */
    initialize_all_pref_files();

    /* Keep birth options for new characters */
    for (opt = 0; newchar && (opt < OPT_MAX); opt++)
    {
        const char *name = option_name(opt);

        if (name && strstr(name, "birth_"))
            player->opts.opt[opt] = options[opt];
    }

    /* Send event */
    Term_xtra(TERM_XTRA_REACT, use_graphics);

    gather_settings();

    /* Sneakily init command list */
    cmd_init();

    Send_options(true);
    Send_autoinscriptions();

    /* Send visual preferences */
    for (i = 0; i < 5; i++) Send_verify(i);

    loading_screen(0);

    /* Send request for features to read */
    Send_features(0, 0);
}


/*
 * Free player struct
 */
static void cleanup_player(void)
{
    int i;
    if (!player) return; /* Never initialised in the first place. */

    /* Free the things that are always initialised */
    mem_free(player->timed);
    if (player->upkeep)
    {
        mem_free(player->upkeep->inven);
        mem_free(player->upkeep->quiver);
    }
    mem_free(player->upkeep);
    player->upkeep = NULL;

    /* Free the things that are only there if there is a loaded player */
    mem_free(player->gear);
    free_body(player);

    /* PWMAngband */
    for (i = 0; player->scr_info && (i < Setup.max_row + ROW_MAP + 1); i++)
    {
        mem_free(player->scr_info[i]);
        mem_free(player->trn_info[i]);
    }
    mem_free(player->scr_info);
    mem_free(player->trn_info);
    for (i = 0; i < N_HISTORY_FLAGS; i++)
        mem_free(player->hist_flags[i]);
    mem_free(player->kind_aware);
    mem_free(player->kind_ignore);
    mem_free(player->kind_everseen);
    for (i = 0; player->ego_ignore_types && (i < z_info->e_max); i++)
        mem_free(player->ego_ignore_types[i]);
    mem_free(player->ego_ignore_types);
    mem_free(player->ego_everseen);

    mem_free(player->on_channel);

    /* Free the basic player struct */
    mem_free(player);
    player = NULL;
}


static char *server_version(uint16_t version, uint16_t beta)
{
    return format("%d", version);
/*
    uint16_t major, minor, patch, extra;

    major = version >> 12;
    minor = (version % (1 << 12)) >> 8;
    patch = ((version % (1 << 12)) % (1 << 8)) >> 4;
    extra = ((version % (1 << 12)) % (1 << 8)) % (1 << 4);

    return format("%d.%d.%d.%d%s", major, minor, patch, extra, (beta? " beta": ""));
*/
}


/*
 * Initialize everything, contact the server, and start the loop.
 */
void client_init(bool new_game)
{
    sockbuf_t ibuf;
    char status, num_types, expiry;
    int trycount;
    char host_name[NORMAL_WID], trymsg[NORMAL_WID];
    uint16_t conntype = CONNTYPE_PLAYER;
    char buffer[NORMAL_WID];
#ifdef WINDOWS
    DWORD nSize = NORMAL_WID;
#endif
    bool done = false;
    uint16_t num, max;
    uint32_t num_name;
    size_t i, j;
    struct keypress c;
    int32_t intro;

    if (new_game)
    {
        /* Initialize input hooks */
        textui_input_init();

        /* Initialise sound */
        init_sound();

        /* Set up the display handlers and things. */
        init_display();

        /* Load in basic data */
        init_arrays();
    }

    /* Initialize player */
    init_player();

    /* Initialize minor arrays */
    init_minor();

    GetLocalHostName(host_name, NORMAL_WID);

    /* Play intro music */
    intro = conf_get_int("MAngband", "IntroMusic", 0);
    if (intro)
    {
        /* Hack -- set location to "intro" */
        my_strcpy(player->locname, "intro", sizeof(player->locname));

        play_intro_music();
    }

    if (new_game)
    {
        /* Default server host and port */
        server_port = conf_get_int("MAngband", "port", 18346);
        my_strcpy(server_name, conf_get_string("MAngband", "host", ""), sizeof(server_name));

        /* Read host/port from the command line */
        clia_read_string(server_name, sizeof(server_name), "host");
        clia_read_int(&server_port, "port");

        /* Check whether we should query the metaserver */
        if (STRZERO(server_name))
        {
            /* Query metaserver */
            if (!get_server_name())
                quit("No server specified.");
        }

        /* Fix "localhost" */
        if (streq(server_name, "localhost"))
            my_strcpy(server_name, host_name, sizeof(server_name));

        /* Default nickname and password */
        my_strcpy(nick, conf_get_string("MAngband", "nick", nick), sizeof(nick));
        my_strcpy(pass, conf_get_string("MAngband", "pass", pass), sizeof(pass));

        /* Capitalize the name */
        my_strcap(nick);

        /* Get account name and pass */
        get_account_name();

        /* Hack -- save account name */
        my_strcpy(account, nick, sizeof(account));
    }
    else
    {
        /* Hack -- restore account name */
        my_strcpy(nick, account, sizeof(nick));
    }

    /* Create the net socket and make the TCP connection */
    if ((Socket = CreateClientSocket(server_name, server_port)) == -1)
    {
        /* Display the socket error message */
#ifdef WINDOWS
        /* TODO: similar function for unix */
        put_str(GetSocketErrorMessage(), 19, 1);
#endif

        while (!done)
        {
            /* Prompt for auto-retry */
            put_str("Couldn't connect to server, keep trying? [Y/N]", 21, 1);

            /* Make sure the message is shown */
            Term_fresh();
            memset(&c, 0, sizeof(c));
            while ((c.code != 'Y') && (c.code != 'y') && (c.code != 'N') && (c.code != 'n'))
            {
                /* Get a key */
                c = inkey();
            }

            /* If we dont want to retry, exit with error */
            if ((c.code == 'N') || (c.code == 'n'))
                quit("That server either isn't up, or you mistyped the hostname.");

            /* ...else, keep trying until socket connected */
            trycount = 1;
            while ((Socket = CreateClientSocket(server_name, server_port)) == -1)
            {
                if (trycount > 200) break;

                /* Progress Message */
                strnfmt(trymsg, sizeof(trymsg),
                    "Connecting to server [%i]                      ", trycount++);
                put_str(trymsg, 21, 1);
                
                /* Make sure the message is shown */
                Term_redraw(); /* Hmm maybe not the proper way to force an os poll */
                Term_flush();
            }
            if (Socket != -1) done = true;
        }
    }

    /* Create a socket buffer */
    if (Sockbuf_init(&ibuf, Socket, CLIENT_SEND_SIZE, SOCKBUF_READ | SOCKBUF_WRITE) == -1)
        quit("No memory for socket buffer");

    /* Clear it */
    Sockbuf_clear(&ibuf);

#ifdef WINDOWS
    /* Get user name */
    if (GetUserName(buffer, &nSize))
    {
        buffer[16] = '\0';
        my_strcpy(real_name, buffer, sizeof(real_name));
    }
#endif

    /* Put the contact info in it */
    Packet_printf(&ibuf, "%hu", (unsigned)conntype);
    Packet_printf(&ibuf, "%hu%c", (unsigned)current_version(), (int)beta_version());
    Packet_printf(&ibuf, "%s%s%s%s", real_name, host_name, nick, stored_pass);

    /* Send it */
    if (!Net_Send(Socket, &ibuf))
        quit("Couldn't send contact information");

    /* Wait for reply */
    if (!Net_WaitReply(Socket, &ibuf, 10))
        quit("Server didn't respond!");

    /* Read what he sent */
    Packet_scanf(&ibuf, "%c", &status);
    Packet_scanf(&ibuf, "%hu", &num);
    Packet_scanf(&ibuf, "%hu", &max);

    /* Check for error */
    switch (status)
    {
        case SUCCESS: break;

        /* The server didn't like us.... */
        case E_VERSION_OLD:
        {
            quit_fmt("Outdated game version! Please update Tangaria to T-%s version.",
                server_version(num, max));
        }
        case E_INVAL:
            quit("The server didn't like your nickname, realname, or hostname.");
        case E_ACCOUNT:
            quit("The password you supplied for the account is incorrect or the account is locked.");
        case E_GAME_FULL:
            quit("Sorry, the game is full. Try again later.");
        case E_SOCKET:
            quit("Socket error.");
        case E_VERSION_NEW:
        {
            quit_fmt("Incompatible game version! Please update Tangaria to T-%s version.",
                server_version(num, max));
        }
        default:
            quit("Your client will not work on that server (not a Tangaria server).");
    }

    max_account_chars = max;
    char_num = num;
    char_name = NULL;
    char_expiry = NULL;
    if (char_num)
    {
        char_name = mem_zalloc(char_num * sizeof(char*));
        char_expiry = mem_zalloc(char_num * sizeof(char));
    }
    for (i = 0; i < (size_t)char_num; i++)
    {
        Packet_scanf(&ibuf, "%c", &expiry);
        char_expiry[i] = expiry;
        Packet_scanf(&ibuf, "%s", buffer);
        char_name[i] = string_make(buffer);
    }
    Packet_scanf(&ibuf, "%c", &num_types);

    /* Something went wrong... */
    if ((num_types != RANDNAME_NUM_TYPES) && !status)
        quit("Failed to retrieve random name fragments.");

    /* Initialize the random name fragments */
    num_names = mem_zalloc(RANDNAME_NUM_TYPES * sizeof(uint32_t));
    name_sections = mem_zalloc(sizeof(char**) * RANDNAME_NUM_TYPES);
    for (i = 0; i < RANDNAME_NUM_TYPES; i++)
    {
        Packet_scanf(&ibuf, "%lu", &num_name);
        num_names[i] = num_name;
        name_sections[i] = mem_alloc(sizeof(char*) * (num_names[i] + 1));
        for (j = 0; j < num_names[i]; j++)
        {
            Packet_scanf(&ibuf, "%s", buffer);
            name_sections[i][j] = string_make(buffer);
        }
    }

    /* Server agreed to talk, initialize the buffers */
    if (Net_init(Socket) == -1)
        quit("Network initialization failed!");

    /* Get character name and pass */
    get_char_name();

    /* Send request for anything needed for play */
    Send_play(0);

    /* Main loop */
    Input_loop();

    /* Hack -- restart game without quitting */
    if (play_again)
    {
        play_again = false;
        cleanup_player();
        Setup.initialized = false;
        Setup.ready = false;
        client_init(false);
    }
}


void cleanup_floor(void)
{
    int i;

    /* Paranoia */
    if (!floor_items) return;

    for (i = 0; i < z_info->floor_size; i++)
    {
        if (floor_items[i]) mem_free(floor_items[i]);
        floor_items[i] = NULL;
    }
}


/*
 * Free all the stuff initialised in init_angband()
 */
void cleanup_angband(void)
{
    int i;

    /* Free the menus */
    free_command_menu();

    cleanup_player();

    /* Free the floor items */
    cleanup_floor();
    mem_free(floor_items);
    floor_items = NULL;

    event_remove_all_handlers();

    /* Free the current store */
    mem_free(current_store.stock);
    current_store.stock = NULL;
    if (current_store.owner) string_free(current_store.owner->name);
    mem_free(current_store.owner);
    current_store.owner = NULL;
    string_free(current_store_name);
    current_store_name = NULL;
    mem_free(store_names);
    store_names = NULL;

    /* Free the character list */
    for (i = 0; i < char_num; i++) string_free(char_name[i]);
    mem_free(char_name);
    char_name = NULL;
    mem_free(char_expiry);
    char_expiry = NULL;

    /* Free the random name fragments */
    strings_free(name_sections, num_names, RANDNAME_NUM_TYPES);
    name_sections = NULL;
    num_names = NULL;

    /* Free attr/chars */
    mem_free(Client_setup.k_attr);
    Client_setup.k_attr = NULL;
    mem_free(Client_setup.k_char);
    Client_setup.k_char = NULL;
    mem_free(Client_setup.r_attr);
    Client_setup.r_attr = NULL;
    mem_free(Client_setup.r_char);
    Client_setup.r_char = NULL;
    mem_free(Client_setup.f_attr);
    Client_setup.f_attr = NULL;
    mem_free(Client_setup.f_char);
    Client_setup.f_char = NULL;
    mem_free(Client_setup.t_attr);
    Client_setup.t_attr = NULL;
    mem_free(Client_setup.t_char);
    Client_setup.t_char = NULL;
    mem_free(Client_setup.flvr_x_attr);
    Client_setup.flvr_x_attr = NULL;
    mem_free(Client_setup.flvr_x_char);
    Client_setup.flvr_x_char = NULL;
    mem_free(Client_setup.note_aware);
    Client_setup.note_aware = NULL;

    /* Free the messages */
    messages_free();

    /* Free the info arrays */
    for (i = 0; k_info && (i < z_info->k_max); i++) string_free(k_info[i].name);
    mem_free(k_info);
    k_info = NULL;
    for (i = 0; e_info && (i < z_info->e_max); i++)
    {
        struct poss_item *poss, *pn;

        string_free(e_info[i].name);
        poss = e_info[i].poss_items;
        while (poss)
        {
            pn = poss->next;
            mem_free(poss);
            poss = pn;
        }
    }
    mem_free(e_info);
    e_info = NULL;
    cleanup_p_race();
    cleanup_realm();
    cleanup_class();
    cleanup_body();
    for (i = 0; soc_info && (i < z_info->soc_max); i++) string_free(soc_info[i].name);
    mem_free(soc_info);
    soc_info = NULL;
    for (i = 0; r_info && (i < z_info->r_max); i++) string_free(r_info[i].name);
    mem_free(r_info);
    r_info = NULL;
    while (rb_info)
    {
        struct monster_base *rb = rb_info->next;

        string_free(rb_info->name);
        mem_free(rb_info);
        rb_info = rb;
    }
    for (i = 0; curses && (i < z_info->curse_max); i++)
    {
        string_free(curses[i].name);
        string_free(curses[i].desc);
    }
    mem_free(curses);
    curses = NULL;
    for (i = 0; f_info && (i < FEAT_MAX); i++) string_free(f_info[i].name);
    mem_free(f_info);
    f_info = NULL;
    for (i = 0; trap_info && (i < z_info->trap_max); i++) string_free(trap_info[i].desc);
    mem_free(trap_info);
    trap_info = NULL;
    for (i = 0; i < TMD_MAX; i++)
    {
        struct timed_grade *grade = timed_grades[i];

        while (grade)
        {
            struct timed_grade *next = grade->next;

            string_free(grade->name);
            mem_free(grade);
            grade = next;
        }
    }

    /* Free the format() buffer */
    vformat_kill();

    /* Release config */
    conf_done();

    /* Free the directories */
    free_file_paths();
}


/*
 * Clean up UI
 */
void textui_cleanup(void)
{
    /* Cleanup any options menus */
    cleanup_options();

    keymap_free();
}
