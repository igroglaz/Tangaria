/*
 * File: main-sdl.c
 * Purpose: Angband SDL port
 *
 * Copyright (c) 2007 Ben Harrison, Gregory Velichansky, Eric Stevens,
 * Leon Marrick, Iain McFall, and others
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

#ifdef USE_SDL

#ifdef WINDOWS
#include "..\_SDL\SDL.h"
#include "..\_SDL\SDL_ttf.h"
#include "..\_SDL\SDL_image.h"
#else
#include <SDL/SDL.h>
#include <SDL/SDL_ttf.h>
#include <SDL/SDL_image.h>
#endif

#define MIN_SCREEN_WIDTH    640
#define MIN_SCREEN_HEIGHT   480

/* SDL flags used for the main window surface */
static Uint32 vflags = SDL_ANYFORMAT;

/* Current screen dimensions */
static int screen_w = 800;
static int screen_h = 600;

/* Fullscreen dimensions */
static int full_w;
static int full_h;

/* Want fullscreen? */
static bool fullscreen = false;

/* Want nice graphics? */
static bool nicegfx = false;

static int overdraw = 0;
static int overdraw_max = 0;

/*
 * Status bar color:
 *   0 = default color
 *   COLOUR_WHITE to COLOUR_DEEP_L_BLUE = colored status bar
 */
static int statusbar_color = 0;

static char *sdl_settings_file;

/* Default point size for scalable fonts */
#define DEFAULT_POINT_SIZE (10)

/* Minimum allowed point size for scalable fonts */
#define MIN_POINT_SIZE (4)

/* Maximum allowed point size for scalable fonts */
#define MAX_POINT_SIZE (64)

#define MAX_FONTS 60
char *FontList[MAX_FONTS];
static int num_fonts = 0;

/* Holds details about requested properties for a terminal's font. */
typedef struct term_font term_font;
struct term_font
{
    const char *name;   /* final component of path if one of the preset fonts; full path if not a preset font */
    char *alloc_name;   /* same as name if dynamically allocated; otherwise, it is NULL */
    int size;           /* requested point size for the file; zero for bitmapped fonts */
    bool preset;        /* true if this is a font included in the lib/fonts directory for the game */
    bool bitmapped;     /* true if this is a bitmapped (.fon; case-insensitive) font that can't be scaled */
};

/* Used as 'system' font. */
static const term_font default_term_font = { "6x10x.fon", NULL, 0, true, true };

/*
 * Used by the 'Point Size' and 'Font Browser' panels to accumulate
 * information about a new requested font.
 */
static term_font new_font = { NULL, NULL, 0, false, false };

/*
 * A font structure
 * Note that the data is only valid for a surface with matching
 * values for pitch & bpp. If a surface is resized the data _must_ be
 * recalculated.
 */
typedef struct sdl_Font sdl_Font;
struct sdl_Font
{
    int width;              /* The dimensions of this font (in pixels) */
    int height;
    char name[32];          /* Label in menu used to select the font */
    Uint16 pitch;           /* Pitch of the surface this font is made for */
    Uint8 bpp;              /* Bytes per pixel of the surface */
    Uint8 something;        /* Padding */
    const term_font *req;   /* Backreference to the request for this font */
    int *data;              /* The data */
    TTF_Font *sdl_font;     /* The native font */
};

static sdl_Font SystemFont;

/*
 * Window information
 * Each window has its own surface and coordinates
 *
 * Window border:
 *   COLOUR_DARK = no border
 *   COLOUR_WHITE to COLOUR_DEEP_L_BLUE = colored border
 *   COLOUR_SHADE (BASIC_COLORS) = default border
 */
typedef struct term_window term_window;
struct term_window
{
    term term_data;
    SDL_Surface *surface;   /* The surface for this window */
    SDL_Surface *tiles;     /* The appropriately sized tiles for this window */
    uint8_t Term_idx;       /* Index of term that relates to this */
    int top;                /* Window Coordinates on the main screen */
    int left;
    int keys;               /* Size of keypress storage */
    sdl_Font font;          /* Font info */
    term_font req_font;     /* Requested font */
    int windowborders;      /* Window borders */
    int rows;               /* Dimension in tiles */
    int cols;
    int border;             /* Border width */
    int title_height;       /* Height of title bar */
    int width;              /* Dimension in pixels == tile_wid * cols + 2 x border */
    int height;
    int tile_wid;           /* Size in pixels of a char */
    int tile_hgt;
    bool visible;           /* Can we see this window? */
    SDL_Rect uRect;         /* The part that needs to be updated */
    bool minimap_active;    /* Are we looking at the minimap? */
    int max_rows;           /* Maximum number of lines */
};

typedef struct mouse_info mouse_info;
struct mouse_info
{
    int left;       /* Is it pressed? */
    int right;
    int leftx;      /* _IF_ left button is pressed these */
    int lefty;      /* show where it was pressed */
    int rightx;
    int righty;
    int x;          /* Current position of mouse */
    int y;
};

#define WINDOW_DRAW (SDL_USEREVENT + 1)

/*
 * The basic angband text colours in an sdl friendly form
 */
static SDL_Color text_colours[MAX_COLORS];

SDL_Color back_colour;    /* Background colour */
Uint32 back_pixel_colour;

/* Default color for button captions */
SDL_Color DefaultCapColour = { 0, 0, 0 };

typedef struct sdl_ButtonBank sdl_ButtonBank;
typedef struct sdl_Button sdl_Button;
typedef struct sdl_Window sdl_Window;

typedef void (*button_press_func)(sdl_Button *sender);
struct sdl_Button
{
    SDL_Rect pos;               /* Position & Size */
    bool selected;              /* Selected? */
    bool visible;               /* Visible? */
    button_press_func activate; /* A function to call when pressed */
    sdl_ButtonBank *owner;      /* Which bank is this in? */
    char caption[50];           /* Text for this button */
    SDL_Color unsel_colour;     /* Button unselected colour */
    SDL_Color sel_colour;       /* Selected colour */
    SDL_Color cap_colour;       /* Caption colour */
    void *data;                 /* Something */
    int tag;                    /* Something */
};

struct sdl_ButtonBank
{
    sdl_Button *buttons;        /* A collection of buttons */
    bool *used;                 /* What buttons are available? */
    sdl_Window *window;         /* The window that these buttons are on */
    bool need_update;
};

/*
 * Other 'windows' (basically a surface with a position and buttons on it)
 * Currently used for the top status bar and popup windows
 */
typedef void (*sdl_WindowCustomDraw)(sdl_Window *window);
struct sdl_Window
{
    int top;                            /* Position on main window */
    int left;
    int width;                          /* Dimensions */
    int height;
    bool visible;                       /* Visible? */
    SDL_Surface *surface;               /* SDL surface info */
    sdl_ButtonBank buttons;             /* Buttons */
    sdl_Font font;                      /* Font */
    SDL_Surface *owner;                 /* Who shall I display on */
    sdl_WindowCustomDraw draw_extra;    /* Stuff to draw on the surface */
    bool need_update;
};

/*
 * The main surface of the application
 */
static SDL_Surface *AppWin;

/*
 * The status bar
 */
static sdl_Window StatusBar;

/*
 * The Popup window
 */
static sdl_Window PopUp;
static bool popped;

/*
 * Term windows
 */
static term_window windows[ANGBAND_TERM_MAX];
static int Zorder[ANGBAND_TERM_MAX];

/*
 * Keep track of the mouse status
 */
static mouse_info mouse;

/*
 * The number pad consists of 10 keys, each with an SDL identifier
 */
#define is_numpad(k) \
((k == SDLK_KP0) || (k == SDLK_KP1) || (k == SDLK_KP2) || (k == SDLK_KP3) || \
 (k == SDLK_KP4) || (k == SDLK_KP5) || (k == SDLK_KP6) || \
 (k == SDLK_KP7) || (k == SDLK_KP8) || (k == SDLK_KP9) || (k == SDLK_KP_ENTER))

static int SnapRange = 5;   /* Window snap range (pixels) */
static int StatusHeight;    /* The height in pixels of the status bar */
static int SelectedTerm;    /* Current selected Term */

static int AboutSelect;     /* About button */
static int TermSelect;      /* Term selector button */
static int FontSelect;      /* Font selector button */
static int VisibleSelect;   /* Hide/unhide window button*/
static int MoreSelect;      /* Other options button */
static int QuitSelect;      /* Quit button */

/* For saving the icon for the About Box */
static SDL_Surface *mratt = NULL;

/*
 * Unselected colour used on the 'About', 'More', 'Point Size', and
 * 'Font Browser' panels; also used to highlight the currently selected font
 * in the font menu
 */
static SDL_Color AltUnselColour = { 160, 60, 60 };

/* Selected colour used on the 'More', 'Point Size', and 'Font Browser' panels */
SDL_Color AltSelColour = { 210, 110, 110 };

/*
 * Used to highlight the currently selected font in the font menu and
 * 'Font Browser' panel
 */
SDL_Color AltCapColour = { 95, 95, 195 };

/* Buttons on the 'More' panel */
static int MoreOK;                  /* Accept changes */
static int MoreFullscreen;          /* Fullscreen toggle button */
static int MoreNiceGfx;             /* Nice graphics toggle button */
static int MoreSnapPlus;            /* Increase snap range */
static int MoreSnapMinus;           /* Decrease snap range */
static int MoreSoundVolumePlus;     /* Increase sound volume */
static int MoreSoundVolumeMinus;    /* Decrease sound volume */
static int MoreMusicVolumePlus;     /* Increase music volume */
static int MoreMusicVolumeMinus;    /* Decrease music volume */
static int MoreWindowBordersPlus;   /* Increase window borders */
static int MoreWindowBordersMinus;  /* Decrease window borders */

/* Buttons on the 'Point Size' panel */
static int PointSizeBigDec;	/* decrease point size by 10 */
static int PointSizeDec;	/* decrease point size by 1 */
static int PointSizeInc;	/* increase point size by 1 */
static int PointSizeBigInc;	/* increase point size by 10 */
static int PointSizeOk;		/* accept current point size */
static int PointSizeCancel;	/* cancel point size selection */

/* Width of the border box about the 'Point Size' panel */
#define POINT_SIZE_BORDER (5)

/* Width of margin within the border box about the 'Point Size' panel */
#define POINT_SIZE_MARGIN (2)

/*
 * Number of directories and files shown at a time in the 'Font Browser' panel;
 * should be at least 3 so that there's vertical space for the scrolling
 * controls; if making this larger than 15, you'll likely have to increase
 * MAX_BUTTONS as well
 */
#define FONT_BROWSER_PAGE_ENTRIES (15)

/* Number of characters to show for each directory in the 'Font Browser' panel */
#define FONT_BROWSER_DIR_LENGTH (15)

/* Number of characters to show for each file in the 'Font Browser' panel */
#define FONT_BROWSER_FILE_LENGTH (25)

/* Buttons on the 'Font Browser' panel */
static int FontBrowserDirUp;
static int FontBrowserDirectories[FONT_BROWSER_PAGE_ENTRIES];
static int FontBrowserDirPageBefore;
static int FontBrowserDirPageAfter;
static int FontBrowserDirPageDummy;
static int FontBrowserFiles[FONT_BROWSER_PAGE_ENTRIES];
static int FontBrowserFilePageBefore;
static int FontBrowserFilePageAfter;
static int FontBrowserFilePageDummy;
static int FontBrowserPtSizeBigDec;
static int FontBrowserPtSizeDec;
static int FontBrowserPtSizeInc;
static int FontBrowserPtSizeBigInc;
static int FontBrowserOk;
static int FontBrowserRefresh;
static int FontBrowserCancel;

/* Height of the preview part of the 'Font Browser' panel */
#define FONT_BROWSER_PREVIEW_HEIGHT (80)

/* Width of the border around the 'Font Browser' panel */
#define FONT_BROWSER_BORDER (5)

/* Width of margin within the border box around the 'Font Browser' panel */
#define FONT_BROWSER_MARGIN (2)

/* Width of the border around the directory and file subpanels of the 'Font Browser' panel */
#define FONT_BROWSER_SUB_BORDER (3)

/*
 * Width of margin within the border box aroudn the directory and file
 * subpanels of the 'Font Browser' panel
 */
#define FONT_BROWSER_SUB_MARGIN (1)

/*
 * Width of space to separate the scroll controls in the 'Font Browser' panel
 * and the subpanel they affect.
 */
#define FONT_BROWSER_HOR_SPACE (2)

/*
 * In the 'Font Browser' panel, the height of space to separate the point size
 * control from file and directory subpanels and the preview area from the
 * point size control
 */
#define FONT_BROWSER_VER_SPACE (4)

/*
 * Current directory being browsed by the 'Font Browser' panel; if not NULL,
 * should end with a path separator
 */
static char *FontBrowserCurDir = NULL;

/*
 * The length of the root portion (the part that shouldn't be backed over
 * when going up the directory tree) of FontBrowerCurDir
 */
static size_t FontBrowserRootSz = 0;

/*
 * Last directory browsed by the previous instance of the 'Font Browser' panel
 */
static char *FontBrowserLastDir = NULL;

/*
 * Value for the length of the root portion of FontBrowserLastDir
 */
static size_t FontBrowserLastRootSz = 0;

/*
 * Array of the unabbreviated directory names in the current directory being
 * browsed by the 'Font Browser' panel
 */
static char **FontBrowserDirEntries = NULL;

/*
 * Number of directories in the directory currently being browsed by the
 * 'Font Browser'
 */
static size_t FontBrowserDirCount = 0;

/* Number of entries allocated in the FontBrowserDirEntries array */
static size_t FontBrowserDirAlloc = 0;

/*
 * Current page (each with FONT_BROWSER_PAGE_ENTRIES) of directories viewed
 * by the 'Font Browser' panel
 */
static size_t FontBrowserDirPage = 0;

/*
 * Array of the unabbreviated fixed-width font files in the current directory
 * being browsed by the 'Font Browser' panel
 */
static char **FontBrowserFileEntries = NULL;

/*
 * Number of usable files in the directory currently being browsed by the
 * 'Font Browser'
 */
static size_t FontBrowserFileCount = 0;

/* Number of entries allocated in the FontBrowserFileEntries array */
static size_t FontBrowserFileAlloc = 0;

/*
 * Current page (each with FONT_BROWSER_PAGE_ENTRIES) of files viewed
 * by the 'Font Browser' panel
 */
static size_t FontBrowserFilePage = 0;

/*
 * Currently selected file entry in the 'Font Browser' panel or (size_t) -1
 * if there isn't a current selection
 */
static size_t FontBrowserFileCur = (size_t) -1;

/* Font data used when rendering the preview in the 'Font Browser' panel */
static sdl_Font *FontBrowserPreviewFont = NULL;

static bool Moving;             /* Moving a window */
static bool Sizing;             /* Sizing a window */
static SDL_Rect SizingSpot;     /* Rect to describe the sizing area */
static bool Sizingshow = false; /* Is the resize thingy displayed? */
static SDL_Rect SizingRect;     /* Rect to describe the current resize window */

static SDL_Surface *GfxSurface = NULL;  /* A surface for the graphics */

static int MoreWidthPlus;   /* Increase tile width */
static int MoreWidthMinus;  /* Decrease tile width */
static int MoreHeightPlus;  /* Increase tile height */
static int MoreHeightMinus; /* Decrease tile height */
static int *GfxButtons;     /* Graphics mode buttons */
static int SelectedGfx;     /* Current selected gfx */


/*
 * Verify if the given path refers to a font file that can be used.
 */
static bool is_font_file(const char *path)
{
	bool result = false;
	TTF_Font *font = TTF_OpenFont(path, 1);

	if (font)
    {
		if (TTF_FontFaceIsFixedWidth(font)) result = true;
		TTF_CloseFont(font);
	}
	return result;
}


/*
 * Fill a buffer with the short name for a font.
 */
static void get_font_short_name(char *buf, size_t bufsz, const term_font *font)
{
	if (font->bitmapped)
		my_strcpy(buf, font->name + path_filename_index(font->name), bufsz);
	else
		strnfmt(buf, bufsz, "%dpt %s", font->size, font->name + path_filename_index(font->name));
}


/*
 * Fill in an SDL_Rect structure.
 * Note it also returns the value adjusted
 */
static SDL_Rect *sdl_RECT(int x, int y, int w, int h, SDL_Rect *rect)
{
    rect->x = x;
    rect->y = y;
    rect->w = w;
    rect->h = h;
    
    return rect;
}


/*
 * Is a point(x, y) in a rectangle?
 */
static bool point_in(SDL_Rect *rect, int x, int y)
{
    if (x < rect->x) return false;
    if (y < rect->y) return false;
    if (x >= rect->x + rect->w) return false;
    if (y >= rect->y + rect->h) return false;
    
    /* Must be inside */
    return true;
}


/*
 * Draw an outline box
 * Given the top, left, width & height
 */
static void sdl_DrawBox(SDL_Surface *surface, SDL_Rect *rect, SDL_Color colour, int width)
{
    SDL_Rect rc;
    int left = rect->x;
    int right = rect->x + rect->w - width;
    int top = rect->y;
    int bottom = rect->y + rect->h - width;
    Uint32 pixel_colour = SDL_MapRGB(surface->format, colour.r, colour.g, colour.b);
    
    /* Top left -> Top Right */
    sdl_RECT(left, top, rect->w, width, &rc);
    SDL_FillRect(surface, &rc, pixel_colour);
    
    /* Bottom left -> Bottom Right */
    sdl_RECT(left, bottom, rect->w, width, &rc);
    SDL_FillRect(surface, &rc, pixel_colour);
    
    /* Top left -> Bottom left */
    sdl_RECT(left, top, width, rect->h, &rc);
    SDL_FillRect(surface, &rc, pixel_colour);
    
    /* Top right -> Bottom right */
    sdl_RECT(right, top, width, rect->h, &rc);
    SDL_FillRect(surface, &rc, pixel_colour);
}


/*
 * Get the width and height of a given font file
 */
static errr sdl_CheckFont(const term_font *req_font, int *width, int *height)
{
    TTF_Font *ttf_font;
	errr result;

	if (req_font->preset)
    {
		char buf[MSG_LEN];

		/* Build the path */
		path_build(buf, sizeof(buf), ANGBAND_DIR_FONTS, req_font->name);

		/* Attempt to load it */
		ttf_font = TTF_OpenFont(buf, req_font->size);
	}
    else
    {
		/* Attempt to load it */
		ttf_font = TTF_OpenFont(req_font->name, req_font->size);
	}

	/* Bugger */
	if (!ttf_font) return (-1);

	/* Get the size */
	if (!TTF_FontFaceIsFixedWidth(ttf_font) || TTF_SizeText(ttf_font, "M", width, height))
		result = -1;
	else
		result = 0;

	/* Finished with the font */
	TTF_CloseFont(ttf_font);

	return result;
}


/*
 * The sdl_Font routines
 */


/*
 * Free any memory assigned by Create()
 */
static void sdl_FontFree(sdl_Font *font)
{
    /* Finished with the font */
    TTF_CloseFont(font->sdl_font);
    font->sdl_font = NULL;
}


/*
 * Create new font data with font fontname, optimizing the data
 * for the surface given
 */
static errr sdl_FontCreate(sdl_Font *font, const term_font *req_font, SDL_Surface *surface)
{
    TTF_Font *ttf_font;

	if (req_font->preset)
    {
		char buf[MSG_LEN];

		/* Build the path */
		path_build(buf, sizeof(buf), ANGBAND_DIR_FONTS, req_font->name);

		/* Attempt to load it */
		ttf_font = TTF_OpenFont(buf, req_font->size);
    }
	else
		ttf_font = TTF_OpenFont(req_font->name, req_font->size);

	/* Bugger */
	if (!ttf_font) return -1;

	/* Get the size */
	if (TTF_SizeText(ttf_font, "M", &font->width, &font->height))
    {
		TTF_CloseFont(ttf_font);
		return -1;
	}

	/* Fill in some of the font struct */
	get_font_short_name(font->name, sizeof(font->name), req_font);
	font->req = req_font;
	font->pitch = surface->pitch;
	font->bpp = surface->format->BytesPerPixel;
	font->sdl_font = ttf_font;

	/* Success */
	return 0;
}


/*
 * Draw some text onto a surface, allowing shaded backgrounds.
 * The surface is first checked to see if it is compatible with
 * this font, if it isn't then the font will be 're-precalculated'.
 *
 * You can, I suppose, use one font on many surfaces, but it is
 * definitely not recommended. One font per surface is good enough.
 */
static errr sdl_mapFontDraw(sdl_Font *font, SDL_Surface *surface, SDL_Color colour, SDL_Color bg,
    int x, int y, int n, const char *s)
{
    Uint8 bpp = surface->format->BytesPerPixel;
    Uint16 pitch = surface->pitch;
    SDL_Rect rc;
    SDL_Surface *text;

    if ((bpp != font->bpp) || (pitch != font->pitch))
        sdl_FontCreate(font, font->req, surface);

    /* Lock the window surface (if necessary) */
    if (SDL_MUSTLOCK(surface))
    {
        if (SDL_LockSurface(surface) < 0) return (-1);
    }

    sdl_RECT(x, y, n * font->width, font->height, &rc);
    text = TTF_RenderText_Shaded(font->sdl_font, s, colour, bg);
    if (text)
    {
        SDL_BlitSurface(text, NULL, surface, &rc);
        SDL_FreeSurface(text);
    }

    /* Unlock the surface */
    if (SDL_MUSTLOCK(surface)) SDL_UnlockSurface(surface);

    /* Success */
    return (0);
}


/*
 * Draw some text onto a surface
 * The surface is first checked to see if it is compatible with
 * this font, if it isn't then the font will be 're-precalculated'
 *
 * You can, I suppose, use one font on many surfaces, but it is
 * definitely not recommended. One font per surface is good enough.
 */
static errr sdl_FontDraw(sdl_Font *font, SDL_Surface *surface, SDL_Color colour, int x, int y,
    int n, const char *s)
{
    Uint8 bpp = surface->format->BytesPerPixel;
    Uint16 pitch = surface->pitch;
    SDL_Rect rc;
    SDL_Surface *text;

    if ((bpp != font->bpp) || (pitch != font->pitch))
        sdl_FontCreate(font, font->req, surface);

    /* Lock the window surface (if necessary) */
    if (SDL_MUSTLOCK(surface))
    {
        if (SDL_LockSurface(surface) < 0) return (-1);
    }

    sdl_RECT(x, y, n * font->width, font->height, &rc);
    text = TTF_RenderText_Solid(font->sdl_font, s, colour);
    if (text)
    {
        SDL_BlitSurface(text, NULL, surface, &rc);
        SDL_FreeSurface(text);
    }

    /* Unlock the surface */
    if (SDL_MUSTLOCK(surface)) SDL_UnlockSurface(surface);

    /* Success */
    return (0);
}


/*
 * Draw a button on the screen
 */
static void sdl_ButtonDraw(sdl_Button *button)
{
    SDL_Surface *surface = button->owner->window->surface;
    sdl_Font *font = &button->owner->window->font;
    SDL_Color colour = button->selected? button->sel_colour: button->unsel_colour;
    
    if (!button->visible) return;

    SDL_FillRect(surface, &button->pos, SDL_MapRGB(surface->format, colour.r, colour.g, colour.b));

    if (strlen(button->caption))
    {
        size_t len = strlen(button->caption);
        unsigned max = button->pos.w / font->width;
        int n = MIN(len, max);
        int l = n * font->width / 2;
        int x = button->pos.x + ((button->pos.w) / 2) - l;
        
        sdl_FontDraw(font, surface, button->cap_colour, x, button->pos.y + 1, n, button->caption);
    }
}


/*
 * Adjust the position of a button
 */
static void sdl_ButtonMove(sdl_Button *button, int x, int y)
{
    button->pos.x = x;
    button->pos.y = y;
    button->owner->need_update = true;
}


/*
 * Adjust the size of a button
 */
static void sdl_ButtonSize(sdl_Button *button, int w, int h)
{
    button->pos.w = w;
    button->pos.h = h;
    button->owner->need_update = true;
}


/*
 * Set the caption
 */
static void sdl_ButtonCaption(sdl_Button *button, const char *s)
{
    my_strcpy(button->caption, s, sizeof(button->caption));
    button->owner->need_update = true;
}


/*
 * Set the visibility of a button
 */
static void sdl_ButtonVisible(sdl_Button *button, bool visible)
{
    if (button->visible != visible)
    {
        button->visible = visible;
        
        button->owner->need_update = true;
    }
}


/*
 * Maximum amount of buttons in a bank
 */
#define MAX_BUTTONS 70


/*
 * The button_bank package
 */


/*
 * Initialize it
 */
static void sdl_ButtonBankInit(sdl_ButtonBank *bank, sdl_Window *window)
{
    bank->window = window;
    bank->buttons = mem_zalloc(MAX_BUTTONS * sizeof(sdl_Button));
    bank->used = mem_zalloc(MAX_BUTTONS * sizeof(bool));
    bank->need_update = true;
}


/*
 * Clear the bank
 */
static void sdl_ButtonBankFree(sdl_ButtonBank *bank)
{
    mem_free(bank->buttons);
    mem_free(bank->used);
}


/*
 * Draw all the buttons on the screen
 */
static void sdl_ButtonBankDrawAll(sdl_ButtonBank *bank)
{
    int i;
    
    for (i = 0; i < MAX_BUTTONS; i++)
    {
        sdl_Button *button = &bank->buttons[i];
        
        if (!bank->used[i]) continue;
        if (!button->visible) continue;
        
        sdl_ButtonDraw(button);
    }
    bank->need_update = false;
}


/*
 * Get a new button index
 */
static int sdl_ButtonBankNew(sdl_ButtonBank *bank)
{
    int i = 0;
    sdl_Button *new_button;

    while (bank->used[i] && (i < MAX_BUTTONS)) i++;

    if (i == MAX_BUTTONS)
    {
        /* Bugger! */
        return (-1);
    }

    /* Get the button */
    new_button = &bank->buttons[i];

    /* Mark the button as used */
    bank->used[i] = true;

    /* Clear it */
    memset(new_button, 0, sizeof(sdl_Button));

    /* Mark it as mine */
    new_button->owner = bank;

    /* Default colours */
    if ((statusbar_color > 0) && (statusbar_color < BASIC_COLORS))
    {
        new_button->unsel_colour.r = text_colours[statusbar_color].r;
        new_button->unsel_colour.g = text_colours[statusbar_color].g;
        new_button->unsel_colour.b = text_colours[statusbar_color].b;
    }
    else
    {
        new_button->unsel_colour.r = AltUnselColour.r;
        new_button->unsel_colour.g = AltUnselColour.g;
        new_button->unsel_colour.b = AltUnselColour.b;
    }

    new_button->sel_colour.r = AltSelColour.r;
    new_button->sel_colour.g = AltSelColour.g;
    new_button->sel_colour.b = AltSelColour.b;
    new_button->cap_colour.r = DefaultCapColour.r;
	new_button->cap_colour.g = DefaultCapColour.g;
	new_button->cap_colour.b = DefaultCapColour.b;

    /* Success */
    return (i);
}


/*
 * Retrieve button 'idx' or NULL
 */
static sdl_Button *sdl_ButtonBankGet(sdl_ButtonBank *bank, int idx)
{
    /* Check the index */
    if ((idx < 0) || (idx >= MAX_BUTTONS)) return (NULL);
    if (!bank->used[idx]) return (NULL);
    
    /* Return it */
    return &bank->buttons[idx];
}


/*
 * Examine and respond to mouse presses
 * Return if we 'handled' the click
 */
static bool sdl_ButtonBankMouseDown(sdl_ButtonBank *bank, int x, int y)
{
    int i;
    
    /* Check every button */
    for (i = 0; i < MAX_BUTTONS; i++)
    {
        sdl_Button *button = &bank->buttons[i];
        
        /* Discard some */
        if (!bank->used[i]) continue;
        if (!button->visible) continue;
        
        /* Check the coordinates */
        if (point_in(&button->pos, x, y))
        {
            button->selected = true;
            
            /* Draw it */
            bank->need_update = true;
            
            return true;
        }
    }
    return false;
}


/*
 * Respond to a mouse button release
 */
static bool sdl_ButtonBankMouseUp(sdl_ButtonBank *bank, int x, int y)
{
    int i;
    
    /* Check every button */
    for (i = 0; i < MAX_BUTTONS; i++)
    {
        sdl_Button *button = &bank->buttons[i];
        
        /* Discard some */
        if (!bank->used[i]) continue;
        if (!button->visible) continue;
        
        /* Check the coordinates */
        if (point_in(&button->pos, x, y))
        {
            /* Has this button been 'selected'? */
            if (button->selected)
            {
                /*
				 * Do these before performing the callback (the
				 * button is no longer selected and needs to
				 * be redrawn) since the callback could remove
				 * the button.
				 */
				button->selected = false;
				bank->need_update = true;
				
				/* Activate the button (usually) */
				if (button->activate) (*button->activate)(button);

				return (true);
            }
        }
        else
        {
            /* This button was 'selected' but the release of the */
            /* mouse button was outside the area of this button */
            if (button->selected)
            {
                /* Now not selected */
                button->selected = false;
                
                /* Draw it */
                bank->need_update = true;
            }
        }
    }
    
    return false;
}


/*
 * sdl_Window functions
 */
static void sdl_WindowFree(sdl_Window* window)
{
    if (window->surface)
    {
        SDL_FreeSurface(window->surface);
        sdl_ButtonBankFree(&window->buttons);
        sdl_FontFree(&window->font);
        memset(window, 0, sizeof(sdl_Window));
    }
}


/*
 * Initialize a window
 */
static void sdl_WindowInit(sdl_Window* window, int w, int h, SDL_Surface *owner,
    const term_font *req_font)
{
    sdl_WindowFree(window);
    window->owner = owner;
    window->width = w;
    window->height = h;
    window->surface = SDL_CreateRGBSurface(SDL_SWSURFACE, w, h,
        owner->format->BitsPerPixel, owner->format->Rmask, owner->format->Gmask,
        owner->format->Bmask, owner->format->Amask);
    sdl_ButtonBankInit(&window->buttons, window);
    sdl_FontCreate(&window->font, req_font, window->surface);
    window->visible = true;
    window->need_update = true;
}


static void sdl_WindowBlit(sdl_Window* window)
{
    SDL_Rect rc;
    
    if (!window->visible) return;
    
    sdl_RECT(window->left, window->top, window->width, window->height, &rc);
    
    SDL_BlitSurface(window->surface, NULL, window->owner, &rc);
    SDL_UpdateRects(window->owner, 1, &rc);
}


static void sdl_WindowText(sdl_Window* window, SDL_Color c, int x, int y, const char *s)
{
    sdl_FontDraw(&window->font, window->surface, c, x, y, strlen(s), s);
}


static void sdl_WindowUpdate(sdl_Window* window)
{
    if ((window->need_update || window->buttons.need_update) && (window->visible))
    {
        SDL_Event Event;
        
        SDL_FillRect(window->surface, NULL, back_pixel_colour);
        
        if (window->draw_extra) (*window->draw_extra)(window);
        
        sdl_ButtonBankDrawAll(&window->buttons);
        
        window->need_update = false;
        
        memset(&Event, 0, sizeof(SDL_Event));
        
        Event.type = WINDOW_DRAW;
        
        Event.user.data1 = (void*)window;
        
        SDL_PushEvent(&Event);
    }
}


static void term_windowFree(term_window* win)
{
    if (win->surface)
    {
        SDL_FreeSurface(win->surface);
        win->surface = NULL;

        /* Invalidate the gfx surface */
        if (win->tiles)
        {
            SDL_FreeSurface(win->tiles);
            win->tiles = NULL;
        }

        term_nuke(&win->term_data);
    }
    
    sdl_FontFree(&win->font);
}


static errr save_prefs(void);


/*
 * Display warning message (see "z-util.c")
 */
static void hook_plog(const char *str)
{
#ifdef WINDOWS
    /* Warning */
    if (str) MessageBox(NULL, str, "Warning", MB_ICONEXCLAMATION | MB_OK);
#else
    printf("%s\n", str);
#endif
}


static void hook_quit(const char *str)
{
    int i;
    static bool quitting = false;

    /* Don't re-enter if already quitting */
    if (quitting) return;
    quitting = true;
    
    save_prefs();
    
    string_free(sdl_settings_file);
    sdl_settings_file = NULL;
    
    /* Free the surfaces of the windows */
    for (i = 0; i < ANGBAND_TERM_MAX; i++)
    {
        term_windowFree(&windows[i]);
        string_free(windows[i].req_font.alloc_name);
        windows[i].req_font.name = NULL;
        windows[i].req_font.alloc_name = NULL;
    }

    /* Free the graphics surfaces */
    if (GfxSurface) SDL_FreeSurface(GfxSurface);

    /* Free the 'System font' */
    sdl_FontFree(&SystemFont);
    
    /* Free the statusbar window */
    sdl_WindowFree(&StatusBar);
    
    /* Free the popup window */
    sdl_WindowFree(&PopUp);
    
    /* Free the main surface */
    SDL_FreeSurface(AppWin);

    mem_free(GfxButtons);
    GfxButtons = NULL;
    close_graphics_modes();

    /* Shut down the font library */
    TTF_Quit();
    
    /* Shut down SDL */
    SDL_Quit();
    
    for (i = 0; i < MAX_FONTS; i++)
    {
        string_free(FontList[i]);
        FontList[i] = NULL;
    }

    /* Free resources */
    textui_cleanup();
    cleanup_angband();
    close_sound();

    /* Cleanup network stuff */
    Net_cleanup();

#ifdef WINDOWS
    /* Cleanup WinSock */
    WSACleanup();
#endif
}


#ifdef WINDOWS
static BOOL CtrlHandler(DWORD fdwCtrlType)
{
    switch (fdwCtrlType)
    {
        case CTRL_CLOSE_EVENT:
            quit(NULL);
            return FALSE;
        default:
            return FALSE;
    }
}
#endif


static void BringToTop(void)
{
    int i, idx;
    
    for (idx = 0; idx < ANGBAND_TERM_MAX; idx++)
    {
        if (Zorder[idx] == SelectedTerm) break;
    }

    if (idx == ANGBAND_TERM_MAX) return;
    
    for (i = idx; i < ANGBAND_TERM_MAX - 1; i++)
    {
        Zorder[i] = Zorder[i + 1];
    }
    
    Zorder[ANGBAND_TERM_MAX - 1] = SelectedTerm;
}


/*
 * Validate a file
 */
static void validate_file(const char *s)
{
    if (!file_exists(s))
        quit_fmt("cannot find required file:\n%s", s);
}


/*
 * Find a window that is under the points x,y on
 * the main screen
 */

static int sdl_LocateWin(int x, int y)
{
    int i;
    
    for (i = ANGBAND_TERM_MAX - 1; i >= 0; i--)
    {
        term_window *win = &windows[Zorder[i]];
        SDL_Rect rc;
        
        if (!win->visible) continue;
        if (!point_in(sdl_RECT(win->left, win->top, win->width, win->height, &rc), x, y))
            continue;
        
        return (Zorder[i]);
    }
    
    return (-1);
}


static void draw_statusbar(sdl_Window *window)
{
    char buf[128];
    term_window *win = &windows[SelectedTerm];
    int fw = window->font.width;
    int x = 1;
    sdl_Button *button;
    SDL_Rect rc;
    SDL_Color c;

    sdl_RECT(0, StatusBar.height - 1, StatusBar.width, 1, &rc);

    c.r = AltUnselColour.r;
    c.g = AltUnselColour.g;
    c.b = AltUnselColour.b;

    if ((statusbar_color > 0) && (statusbar_color < BASIC_COLORS))
    {
        c.r = text_colours[statusbar_color].r;
        c.g = text_colours[statusbar_color].g;
        c.b = text_colours[statusbar_color].b;
    }
    SDL_FillRect(StatusBar.surface, &rc, SDL_MapRGB(StatusBar.surface->format, c.r, c.g, c.b));

    button = sdl_ButtonBankGet(&StatusBar.buttons, AboutSelect);
    x += button->pos.w + 20;

    sdl_WindowText(&StatusBar, c, x, 1, "Term:");
    x += 5 * fw;
    
    button = sdl_ButtonBankGet(&StatusBar.buttons, TermSelect);
    button->pos.x = x;
    x += button->pos.w + 10;

    strnfmt(buf, sizeof(buf), "(%dx%d)", win->cols, win->rows);
    sdl_WindowText(&StatusBar, c, x, 1, buf);
    x += strlen(buf) * fw + 20;
    
    sdl_WindowText(&StatusBar, c, x, 1, "Visible:");
    x += 8 * fw;
    
    button = sdl_ButtonBankGet(&StatusBar.buttons, VisibleSelect);
    button->pos.x = x;
    x += button->pos.w + 20;
    
    button = sdl_ButtonBankGet(&StatusBar.buttons, FontSelect);
    if (button->visible) sdl_WindowText(&StatusBar, c, x, 1, "Font:");
    x += 5 * fw;
    
    button->pos.x = x;
    x += button->pos.w + 20;
    
    button = sdl_ButtonBankGet(&StatusBar.buttons, MoreSelect);
    button->pos.x = x;
    
    x += button->pos.w + 20;
}


static void sdl_BlitWin(term_window *win)
{
    SDL_Rect rc;

    if (!win->surface) return;
    if (!win->visible) return;
    if (win->uRect.x == -1) return;

    /* Select the area to be updated */
    sdl_RECT(win->left + win->uRect.x, win->top + win->uRect.y, win->uRect.w, win->uRect.h, &rc);
    
    SDL_BlitSurface(win->surface, &win->uRect, AppWin, &rc);
    SDL_UpdateRects(AppWin, 1, &rc);

    /* Mark the update as complete */
    win->uRect.x = -1;
}


static void sdl_SizingSpot(term_window *win, bool relative, SDL_Rect *prc)
{
    int xoffset = (relative? 0: win->left);
    int yoffset = (relative? 0: win->top);

    sdl_RECT(xoffset + win->width - 10, yoffset + win->height - 10, 8, 8, prc);
}


static void sdl_BlitAll(void)
{
    SDL_Rect rc;
    sdl_Window *window = &StatusBar;
    int i;
    SDL_Color colour;

    colour.r = AltUnselColour.r;
    colour.g = AltUnselColour.g;
    colour.b = AltUnselColour.b;

    SDL_FillRect(AppWin, NULL, back_pixel_colour);

    for (i = 0; i < ANGBAND_TERM_MAX; i++)
    {
        term_window *win = &windows[Zorder[i]];
        
        if (!win->surface) continue;
        if (!win->visible) continue;

        sdl_RECT(win->left, win->top, win->width, win->height, &rc);
        SDL_BlitSurface(win->surface, NULL, AppWin, &rc);

        if (Zorder[i] == SelectedTerm)
        {
            sdl_SizingSpot(win, false, &SizingSpot);

            if (Sizing)
            {
                int grabsize = 10;

                rc = SizingRect;
                sdl_RECT(SizingRect.x + SizingRect.w - grabsize,
                    SizingRect.y + SizingRect.h - grabsize, grabsize, grabsize, &SizingSpot);
            }
        }

        /* Paranoia: always redraw the borders of the window */
        if ((win->windowborders >= 0) && (win->windowborders < BASIC_COLORS))
            sdl_DrawBox(AppWin, &rc, text_colours[win->windowborders], win->border);
        else
            sdl_DrawBox(AppWin, &rc, colour, win->border);
    }

    sdl_RECT(window->left, window->top, window->width, window->height, &rc);

    SDL_BlitSurface(window->surface, NULL, AppWin, &rc);

    SDL_UpdateRect(AppWin, 0, 0, AppWin->w, AppWin->h);
}


static void RemovePopUp(void)
{
    PopUp.visible = false;
    popped = false;
    sdl_BlitAll();

    // Hack -- weather
    if (player->weather_type == 0) player->weather_type = 255;
}


static void QuitActivate(sdl_Button *sender)
{
    SDL_Event Event;
    
    Event.type = SDL_QUIT;
    
    SDL_PushEvent(&Event);
}


static void SetStatusButtons(void)
{
    term_window *win = &windows[SelectedTerm];
    sdl_Button *button = sdl_ButtonBankGet(&StatusBar.buttons, TermSelect);
    sdl_Button *fontbutton = sdl_ButtonBankGet(&StatusBar.buttons, FontSelect);
    sdl_Button *visbutton = sdl_ButtonBankGet(&StatusBar.buttons, VisibleSelect);
    
    sdl_ButtonCaption(button, angband_term_name[SelectedTerm]);
    
    if (!win->visible)
    {
        sdl_ButtonVisible(fontbutton, false);
        sdl_ButtonCaption(visbutton, "No");
    }
    else
    {
        sdl_ButtonVisible(fontbutton, true);
        sdl_ButtonCaption(fontbutton, win->font.name);
        sdl_ButtonCaption(visbutton, "Yes");
    }
}


static void TermFocus(int idx)
{
    if (SelectedTerm == idx) return;
    
    SelectedTerm = idx;
    
    BringToTop();
    
    SetStatusButtons();
    
    sdl_BlitAll();
}


static void AboutDraw(sdl_Window *win)
{
    SDL_Rect rc;
    SDL_Rect icon;

    sdl_RECT(0, 0, win->width, win->height, &rc);
    
    /* Draw a nice box */
    SDL_FillRect(win->surface, &win->surface->clip_rect,
        SDL_MapRGB(win->surface->format, 255, 255, 255));
    sdl_DrawBox(win->surface, &win->surface->clip_rect, AltUnselColour, 5);
    if (mratt)
    {
        sdl_RECT((win->width - mratt->w) / 2, 5, mratt->w, mratt->h, &icon);
        SDL_BlitSurface(mratt, NULL, win->surface, &icon);
    }
    sdl_WindowText(win, AltUnselColour, 20, 150,
        format("You are playing %s", version_build(VERSION_NAME, true)));
    sdl_WindowText(win, AltUnselColour, 20, 160, "See https://tangaria.com");
}


static void AboutActivate(sdl_Button *sender)
{
    int width = 350;
    int height = 200;
    
    sdl_WindowInit(&PopUp, width, height, AppWin, StatusBar.font.req);
    PopUp.left = (AppWin->w / 2) - width / 2;
    PopUp.top = (AppWin->h / 2) - height / 2;
    PopUp.draw_extra = AboutDraw;
    
    popped = true;
}


static void SelectTerm(sdl_Button *sender)
{
    RemovePopUp();
    
    TermFocus(sender->tag);
}


static int get_term_namewidth(void)
{
    int i, maxl = 0;

    for (i = 0; i < ANGBAND_TERM_MAX; i++)
    {
        int l = strlen(angband_term_name[i]);
        if (l > maxl) maxl = l;
    }

    return (maxl * StatusBar.font.width + 20);
}


static void TermActivate(sdl_Button *sender)
{
    int i;
    int width, height = ANGBAND_TERM_MAX * (StatusBar.font.height + 1);
    
    width = get_term_namewidth();

    sdl_WindowInit(&PopUp, width, height, AppWin, StatusBar.font.req);
    PopUp.left = sender->pos.x;
    PopUp.top = sender->pos.y;

    for (i = 0; i < ANGBAND_TERM_MAX; i++)
    {
        int h = PopUp.font.height;
        int b = sdl_ButtonBankNew(&PopUp.buttons);
        sdl_Button *button = sdl_ButtonBankGet(&PopUp.buttons, b);
        sdl_ButtonSize(button, width - 2 , h);
        sdl_ButtonMove(button, 1, i * (h + 1));
        sdl_ButtonCaption(button, angband_term_name[i]);
        sdl_ButtonVisible(button, true);
        button->tag = i;
        button->activate = SelectTerm;
    }
    popped = true;
}


static void ResizeWin(term_window* win, int w, int h);
static void term_data_link_sdl(term_window *win);


static void VisibleActivate(sdl_Button *sender)
{
    term_window *window = &windows[SelectedTerm];
    
    if (SelectedTerm == 0) return;

    /* Reinitialize all subwindows */
    subwindows_reinit_flags();

    if (window->visible)
    {
        window->visible = false;
        term_windowFree(window);
        angband_term[SelectedTerm] = NULL;
    }
    else
    {
        window->visible = true;
        ResizeWin(window, window->width, window->height);
    }

    /* Set up the subwindows */
    subwindows_init_flags();
    
    SetStatusButtons();
    sdl_BlitAll();

    /* Push a key to force redraw */
    Term_key_push(ESCAPE);
}


static void HelpWindowFontChange(term_window *window)
{
	int w, h;

	if (sdl_CheckFont(&window->req_font, &w, &h))
		quit_fmt("could not use the requested font %s", window->req_font.name);

	/* Invalidate the gfx surface */
	if (window->tiles)
    {
		SDL_FreeSurface(window->tiles);
		window->tiles = NULL;
	}

	ResizeWin(window, (w * window->cols) + (2 * window->border),
		(h * window->rows) + window->border + window->title_height);

	SetStatusButtons();

    /* Hack -- set ANGBAND_FONTNAME for main window */
    if (window->Term_idx == 0) ANGBAND_FONTNAME = window->req_font.name;
}


static void SelectPresetBitmappedFont(sdl_Button *sender)
{
	term_window *window = &windows[SelectedTerm];

	sdl_FontFree(&window->font);
	string_free(window->req_font.alloc_name);
	window->req_font.alloc_name = string_make(sender->caption);
    window->req_font.name = window->req_font.alloc_name;
	window->req_font.size = 0;
	window->req_font.preset = true;
	window->req_font.bitmapped = true;
	HelpWindowFontChange(window);

	RemovePopUp();
}


static void ChangePointSize(sdl_Button *sender)
{
	sdl_Button *button;

	if ((new_font.size == MIN_POINT_SIZE && sender->tag < 0) ||
        (new_font.size == MAX_POINT_SIZE && sender->tag > 0))
    {
		/* Size does not change. */
		return;
	}

	new_font.size = MAX(MIN_POINT_SIZE, MIN(MAX_POINT_SIZE, new_font.size + sender->tag));

	/* Change colors on the buttons. */
	if (new_font.size > MIN_POINT_SIZE)
    {
		button = sdl_ButtonBankGet(&PopUp.buttons, PointSizeBigDec);
		button->unsel_colour = AltUnselColour;
		button->sel_colour = AltSelColour;
		button->cap_colour = DefaultCapColour;
		button = sdl_ButtonBankGet(&PopUp.buttons, PointSizeDec);
		button->unsel_colour = AltUnselColour;
		button->sel_colour = AltSelColour;
		button->cap_colour = DefaultCapColour;
	}
    else
    {
		button = sdl_ButtonBankGet(&PopUp.buttons, PointSizeBigDec);
		button->unsel_colour = back_colour;
		button->sel_colour = back_colour;
		button->cap_colour = back_colour;
		button = sdl_ButtonBankGet(&PopUp.buttons, PointSizeDec);
		button->unsel_colour = back_colour;
		button->sel_colour = back_colour;
		button->cap_colour = back_colour;
	}
	if (new_font.size < MAX_POINT_SIZE)
    {
		button = sdl_ButtonBankGet(&PopUp.buttons, PointSizeInc);
		button->unsel_colour = AltUnselColour;
		button->sel_colour = AltSelColour;
		button->cap_colour = DefaultCapColour;
		button = sdl_ButtonBankGet(&PopUp.buttons, PointSizeBigInc);
		button->unsel_colour = AltUnselColour;
		button->sel_colour = AltSelColour;
		button->cap_colour = DefaultCapColour;
	}
    else
    {
		button = sdl_ButtonBankGet(&PopUp.buttons, PointSizeInc);
		button->unsel_colour = back_colour;
		button->sel_colour = back_colour;
		button->cap_colour = back_colour;
		button = sdl_ButtonBankGet(&PopUp.buttons, PointSizeBigInc);
		button->unsel_colour = back_colour;
		button->sel_colour = back_colour;
		button->cap_colour = back_colour;
	}
	PopUp.need_update = true;
}


static void AcceptPointSize(sdl_Button *sender)
{
	term_window *window = &windows[SelectedTerm];

	sdl_FontFree(&window->font);
	string_free(window->req_font.alloc_name);
	window->req_font.alloc_name = string_make(new_font.name);
    window->req_font.name = window->req_font.alloc_name;
	window->req_font.size = new_font.size;
	window->req_font.preset = new_font.preset;
	assert(!new_font.bitmapped);
	window->req_font.bitmapped = false;
	HelpWindowFontChange(window);

	RemovePopUp();
}


static void CancelPointSize(sdl_Button *sender)
{
	string_free(new_font.alloc_name);
	new_font.name = NULL;
    new_font.alloc_name = NULL;
	new_font.size = 0;
	new_font.preset = false;
	new_font.bitmapped = false;
	RemovePopUp();
}


static void DrawPointSize(sdl_Window *win)
{
	SDL_Rect rc;

	sdl_RECT(0, 0, win->width, win->height, &rc);
	sdl_DrawBox(win->surface, &rc, AltUnselColour, POINT_SIZE_BORDER);

	sdl_WindowText(win, AltUnselColour, win->width / 2
		- 5 * PopUp.font.width, POINT_SIZE_BORDER + POINT_SIZE_MARGIN,
		"Point Size");
	sdl_WindowText(win, AltUnselColour, 12 * PopUp.font.width
		+ POINT_SIZE_BORDER + POINT_SIZE_MARGIN,
		POINT_SIZE_BORDER + POINT_SIZE_MARGIN + PopUp.font.height + 6,
		format("%d pt", new_font.size));
}


/*
 * For this panel, buttons are enabled or disabled based on the state of the
 * point size selection.  Don't use sdl_ButtonVisible() to do that because
 * clicks in invisible buttons aren't handled and end up dismissing the panel.
 * That seems likely to frustrate uses, so alter the coloring of the buttons
 * instead while leaving the "visible" field of the button true.
 */
static void ActivatePointSize(sdl_Button *sender)
{
	int left = sender->pos.x + sender->owner->window->left;
	int top = sender->pos.y + sender->owner->window->top;
	int height = 4 * (PopUp.font.height + 2) + 4 + 2 * (POINT_SIZE_BORDER
		+ POINT_SIZE_MARGIN);
	int width = ((int) strlen(format("%d pt", MAX_POINT_SIZE)) + 24)
		* PopUp.font.width + 2 * (POINT_SIZE_BORDER
		+ POINT_SIZE_MARGIN);
	sdl_Button *button;

	sdl_WindowInit(&PopUp, width, height, AppWin, StatusBar.font.req);
	PopUp.left = left;
	PopUp.top = top;
	PopUp.draw_extra = DrawPointSize;

	PointSizeBigDec = sdl_ButtonBankNew(&PopUp.buttons);
	button = sdl_ButtonBankGet(&PopUp.buttons, PointSizeBigDec);
	if (new_font.size > MIN_POINT_SIZE)
    {
		button->unsel_colour = AltUnselColour;
		button->sel_colour = AltSelColour;
		button->cap_colour = DefaultCapColour;
	}
    else
    {
		button->unsel_colour = back_colour;
		button->sel_colour = back_colour;
		button->cap_colour = back_colour;
	}
	sdl_ButtonSize(button, 4 * PopUp.font.width, PopUp.font.height + 2);
	sdl_ButtonVisible(button, true);
	sdl_ButtonCaption(button, "--");
	sdl_ButtonMove(button, POINT_SIZE_BORDER + POINT_SIZE_MARGIN,
		POINT_SIZE_BORDER + POINT_SIZE_MARGIN + PopUp.font.height + 6);
	button->tag = -10;
	button->activate = ChangePointSize;

	PointSizeDec = sdl_ButtonBankNew(&PopUp.buttons);
	button = sdl_ButtonBankGet(&PopUp.buttons, PointSizeDec);
	if (new_font.size > MIN_POINT_SIZE)
    {
		button->unsel_colour = AltUnselColour;
		button->sel_colour = AltSelColour;
		button->cap_colour = DefaultCapColour;
	}
    else
    {
		button->unsel_colour = back_colour;
		button->sel_colour = back_colour;
		button->cap_colour = back_colour;
	}
	sdl_ButtonSize(button, 4 * PopUp.font.width, PopUp.font.height + 2);
	sdl_ButtonVisible(button, true);
	sdl_ButtonCaption(button, " -");
	sdl_ButtonMove(button, 6 * PopUp.font.width + POINT_SIZE_BORDER
		+ POINT_SIZE_MARGIN, POINT_SIZE_BORDER + POINT_SIZE_MARGIN
		+ PopUp.font.height + 6);
	button->tag = -1;
	button->activate = ChangePointSize;

	PointSizeInc = sdl_ButtonBankNew(&PopUp.buttons);
	button = sdl_ButtonBankGet(&PopUp.buttons, PointSizeInc);
	if (new_font.size < MAX_POINT_SIZE)
    {
		button->unsel_colour = AltUnselColour;
		button->sel_colour = AltSelColour;
		button->cap_colour = DefaultCapColour;
	}
    else
    {
		button->unsel_colour = back_colour;
		button->sel_colour = back_colour;
		button->cap_colour = back_colour;
	}
	sdl_ButtonSize(button, 4 * PopUp.font.width, PopUp.font.height + 2);
	sdl_ButtonVisible(button, true);
	sdl_ButtonCaption(button, " +");
	sdl_ButtonMove(button, width - 10 * PopUp.font.width
		- POINT_SIZE_BORDER - POINT_SIZE_MARGIN, POINT_SIZE_BORDER
		+ POINT_SIZE_MARGIN + PopUp.font.height + 6);
	button->tag = 1;
	button->activate = ChangePointSize;

	PointSizeBigInc = sdl_ButtonBankNew(&PopUp.buttons);
	button = sdl_ButtonBankGet(&PopUp.buttons, PointSizeBigInc);
	if (new_font.size < MAX_POINT_SIZE)
    {
		button->unsel_colour = AltUnselColour;
		button->sel_colour = AltSelColour;
		button->cap_colour = DefaultCapColour;
	}
    else
    {
		button->unsel_colour = back_colour;
		button->sel_colour = back_colour;
		button->cap_colour = back_colour;
	}
	sdl_ButtonSize(button, 4 * PopUp.font.width, PopUp.font.height + 2);
	sdl_ButtonVisible(button, true);
	sdl_ButtonCaption(button, "++");
	sdl_ButtonMove(button, width - 4 * PopUp.font.width
		- POINT_SIZE_BORDER - POINT_SIZE_MARGIN, POINT_SIZE_BORDER
		+ POINT_SIZE_MARGIN + PopUp.font.height + 6);
	button->tag = 10;
	button->activate = ChangePointSize;

	PointSizeOk = sdl_ButtonBankNew(&PopUp.buttons);
	button = sdl_ButtonBankGet(&PopUp.buttons, PointSizeOk);
	button->unsel_colour = AltUnselColour;
	button->sel_colour = AltSelColour;
	sdl_ButtonSize(button, 8 * PopUp.font.width, PopUp.font.height + 2);
	sdl_ButtonVisible(button, true);
	sdl_ButtonCaption(button, "OK");
	sdl_ButtonMove(button, width / 2 - 10 * PopUp.font.width,
		height - PopUp.font.height - 2 - POINT_SIZE_BORDER
		- POINT_SIZE_MARGIN);
	button->activate = AcceptPointSize;

	PointSizeCancel = sdl_ButtonBankNew(&PopUp.buttons);
	button = sdl_ButtonBankGet(&PopUp.buttons, PointSizeCancel);
	button->unsel_colour = AltUnselColour;
	button->sel_colour = AltSelColour;
	sdl_ButtonSize(button, 8 * PopUp.font.width, PopUp.font.height + 2);
	sdl_ButtonVisible(button, true);
	sdl_ButtonCaption(button, "Cancel");
	sdl_ButtonMove(button, width / 2 + 2 * PopUp.font.width,
		height - PopUp.font.height - 2 - POINT_SIZE_BORDER
		- POINT_SIZE_MARGIN);
	button->activate = CancelPointSize;

	popped = true;
}


static void SelectPresetScalableFont(sdl_Button *sender)
{
	term_window *window = &windows[SelectedTerm];

	RemovePopUp();
	string_free(new_font.alloc_name);
	new_font.alloc_name = string_make(sender->caption);
    new_font.name = new_font.alloc_name;
	new_font.size = (window->req_font.size > 0) ?
		window->req_font.size : DEFAULT_POINT_SIZE;
	new_font.preset = true;
	new_font.bitmapped = false;
	ActivatePointSize(sender);
}


static void AlterNonPresetFontSize(sdl_Button *sender)
{
	term_window *window = &windows[SelectedTerm];

	assert(!window->req_font.preset);
	RemovePopUp();
	if (!window->req_font.bitmapped)
    {
		assert(window->req_font.size >= MIN_POINT_SIZE
			&& window->req_font.size <= MAX_POINT_SIZE);
		string_free(new_font.alloc_name);
		new_font.alloc_name = string_make(window->req_font.name);
        new_font.name = new_font.alloc_name;
		new_font.size = window->req_font.size;
		new_font.preset = false;
		new_font.bitmapped = false;
		ActivatePointSize(sender);
	}
}


static void HelpFontBrowserClose(void)
{
	size_t i;

	RemovePopUp();

	/* Remember the directory where the browser was. */
	string_free(FontBrowserLastDir);
	FontBrowserLastDir = FontBrowserCurDir;
    FontBrowserLastRootSz = FontBrowserRootSz;
	FontBrowserCurDir = NULL;
	FontBrowserRootSz = 0;

	/* Clear the other state that doesn't need to be retained. */
	for (i = 0; i < FontBrowserDirCount; ++i)
		string_free(FontBrowserDirEntries[i]);
	mem_free(FontBrowserDirEntries);
	FontBrowserDirEntries = NULL;
	FontBrowserDirCount = 0;
	FontBrowserDirAlloc = 0;
	FontBrowserDirPage = 0;
	for (i = 0; i < FontBrowserFileCount; ++i)
		string_free(FontBrowserFileEntries[i]);
	mem_free(FontBrowserFileEntries);
	FontBrowserFileEntries = NULL;
	FontBrowserFileCount = 0;
	FontBrowserFileAlloc = 0;
	FontBrowserFilePage = 0;
	FontBrowserFileCur = (size_t) -1;
	if (FontBrowserPreviewFont)
    {
		sdl_FontFree(FontBrowserPreviewFont);
		mem_free(FontBrowserPreviewFont);
		FontBrowserPreviewFont = NULL;
	}
	string_free(new_font.alloc_name);
	new_font.name = NULL;
    new_font.alloc_name = NULL;
	new_font.size = 0;
	new_font.preset = false;
	new_font.bitmapped = false;
}


static void AcceptFontBrowser(sdl_Button *sender)
{
	if (FontBrowserPreviewFont)
    {
		term_window *window = &windows[SelectedTerm];

		sdl_FontFree(&window->font);
		string_free(window->req_font.alloc_name);
		assert(new_font.name);
		window->req_font.alloc_name = string_make(new_font.name);
        window->req_font.name = window->req_font.alloc_name;
		window->req_font.size = new_font.size;
		window->req_font.preset = new_font.preset;
		window->req_font.bitmapped = new_font.bitmapped;
		HelpWindowFontChange(window);
	}
	HelpFontBrowserClose();
}


/*
 * Name sorting function
 */
static int cmp_name(const void *f1, const void *f2)
{
	const char *name1 = *(const char**)f1;
	const char *name2 = *(const char**)f2;

	return strcmp(name1, name2);
}


static void ChangeDirPageFontBrowser(sdl_Button *sender)
{
	/*
	 * Each page overlaps by one entry with the previous; thus the
	 * use of FONT_BROWSER_PAGE_ENTRIES - 1 in the lines below.
	 * If the count is greater than zero, max_page satisfies
	 * max_page * (FONT_BROWSER_PAGE_ENTRIES - 1) < count and
	 * max_page * (FONT_BROWSER_PAGE_ENTRIES - 1)
	 * + FONT_BROWSER_PAGE_ENTRIES >= count.
	 */
	size_t max_page = (FontBrowserDirCount > 0) ?
		(FontBrowserDirCount - 1) / (FONT_BROWSER_PAGE_ENTRIES - 1) : 0;
	sdl_Button *button;
	size_t page_start, i;

	if ((FontBrowserDirPage == 0 && sender->tag < 0) ||
        (FontBrowserDirPage == max_page && sender->tag > 0))
    {
		/* Scrolling that direction does nothing. */
		return;
	}

	FontBrowserDirPage = MAX(0, MIN(max_page, FontBrowserDirPage
		+ sender->tag));

	/* Redo the captions and colours of the directory buttons. */
	page_start = FontBrowserDirPage * (FONT_BROWSER_PAGE_ENTRIES - 1);
	for (i = 0; i < FONT_BROWSER_PAGE_ENTRIES; ++i)
    {
		button = sdl_ButtonBankGet(&PopUp.buttons,
			FontBrowserDirectories[i]);
		if (i + page_start < FontBrowserDirCount)
        {
			/*
			 * Truncate displayed name to at most
			 * FONT_BROWSER_DIR_LENGTH Unicode code points.
			 */
			char *caption;

			assert(FontBrowserDirEntries
				&& FontBrowserDirEntries[i + page_start]);
			caption = string_make(
				FontBrowserDirEntries[i + page_start]);
			utf8_clipto(caption, FONT_BROWSER_DIR_LENGTH);
			sdl_ButtonCaption(button, caption);
			string_free(caption);
			button->unsel_colour = AltUnselColour;
			button->sel_colour = AltSelColour;
			button->cap_colour = DefaultCapColour;
		}
        else
        {
			sdl_ButtonCaption(button, "");
			button->unsel_colour = back_colour;
			button->sel_colour = back_colour;
			button->cap_colour = back_colour;
		}
	}

	/* Update the colors of the scroll controls if relevant. */
	button = sdl_ButtonBankGet(&PopUp.buttons, FontBrowserDirPageBefore);
	if (FontBrowserDirPage > 0)
    {
		button->unsel_colour = AltUnselColour;
		button->sel_colour = AltSelColour;
		button->cap_colour = DefaultCapColour;
	}
    else
    {
		button->unsel_colour = back_colour;
		button->sel_colour = back_colour;
		button->cap_colour = back_colour;
	}
	button = sdl_ButtonBankGet(&PopUp.buttons, FontBrowserDirPageAfter);
	if (FontBrowserDirPage < max_page)
    {
		button->unsel_colour = AltUnselColour;
		button->sel_colour = AltSelColour;
		button->cap_colour = DefaultCapColour;
	}
    else
    {
		button->unsel_colour = back_colour;
		button->sel_colour = back_colour;
		button->cap_colour = back_colour;
	}

	PopUp.need_update = true;
}


static void ChangeFilePageFontBrowser(sdl_Button *sender)
{
	/*
	 * Each page overlaps by one entry with the previous; thus the
	 * use of FONT_BROWSER_PAGE_ENTRIES - 1 in the lines below.
	 * If the count is greater than zero, max_page satisfies
	 * max_page * (FONT_BROWSER_PAGE_ENTRIES - 1) < count and
	 * max_page * (FONT_BROWSER_PAGE_ENTRIES - 1)
	 * + FONT_BROWSER_PAGE_ENTRIES >= count.
	 */
	size_t max_page = (FontBrowserFileCount > 0) ? (FontBrowserFileCount
		- 1) / (FONT_BROWSER_PAGE_ENTRIES - 1) : 0;
	sdl_Button *button;
	size_t page_start, i;

	if ((FontBrowserFilePage == 0 && sender->tag < 0) ||
			(FontBrowserFilePage == max_page && sender->tag > 0))
    {
		/* Scrolling that direction does nothing. */
		return;
	}

	FontBrowserFilePage = MAX(0, MIN(max_page, FontBrowserFilePage
		+ sender->tag));

	/* Redo the captions and colours of the file buttons. */
	page_start = FontBrowserFilePage * (FONT_BROWSER_PAGE_ENTRIES - 1);
	for (i = 0; i < FONT_BROWSER_PAGE_ENTRIES; ++i)
    {
		button = sdl_ButtonBankGet(&PopUp.buttons,
			FontBrowserFiles[i]);
		if (i + page_start < FontBrowserFileCount)
        {
			/*
			 * Truncate displayed name to at most
			 * FONT_BROWSER_FILE_LENGTH characters.
			 */
			char *caption;

			assert(FontBrowserFileEntries &&
				FontBrowserFileEntries[i + page_start]);
			caption = string_make(
				FontBrowserFileEntries[i + page_start]);
			utf8_clipto(caption, FONT_BROWSER_FILE_LENGTH);
			sdl_ButtonCaption(button, caption);
			string_free(caption);
			button->unsel_colour = AltUnselColour;
			button->sel_colour = AltSelColour;
			if (i + page_start == FontBrowserFileCur)
				button->cap_colour = AltCapColour;
            else
				button->cap_colour = DefaultCapColour;
		}
        else
        {
			sdl_ButtonCaption(button, "");
			button->unsel_colour = back_colour;
			button->sel_colour = back_colour;
			button->cap_colour = back_colour;
		}
	}

	/* Change the colors on the scroll controls if relevant. */
	button = sdl_ButtonBankGet(&PopUp.buttons, FontBrowserFilePageBefore);
	if (FontBrowserFilePage > 0)
    {
		button->unsel_colour = AltUnselColour;
		button->sel_colour = AltSelColour;
		button->cap_colour = DefaultCapColour;
	}
    else
    {
		button->unsel_colour = back_colour;
		button->sel_colour = back_colour;
		button->cap_colour = back_colour;
	}
	button = sdl_ButtonBankGet(&PopUp.buttons, FontBrowserFilePageAfter);
	if (FontBrowserFilePage < max_page)
    {
		button->unsel_colour = AltUnselColour;
		button->sel_colour = AltSelColour;
		button->cap_colour = DefaultCapColour;
	}
    else
    {
		button->unsel_colour = back_colour;
		button->sel_colour = back_colour;
		button->cap_colour = back_colour;
	}

	PopUp.need_update = true;
}


static void RefreshFontBrowser(sdl_Button *sender)
{
	char *oldcur;
	ang_dir *dir;
	size_t i;
	char file_part[MSG_LEN], full_path[MSG_LEN];

	/*
	 * If there is a currently selected file, remember it so, if it
	 * is still present, it can be used as the current selection.
	 */
	if (FontBrowserFileCur != (size_t) -1)
    {
		assert(FontBrowserFileEntries
			&& FontBrowserFileCur < FontBrowserFileCount
			&& FontBrowserFileEntries[FontBrowserFileCur]);
		oldcur = FontBrowserFileEntries[FontBrowserFileCur];
		FontBrowserFileEntries[FontBrowserFileCur] = NULL;
		FontBrowserFileCur = (size_t) -1;
	}
    else
		oldcur = NULL;

	/* Clear the old entries in the lists. */
	for (i = 0; i < FontBrowserDirCount; ++i)
    {
		string_free(FontBrowserDirEntries[i]);
		FontBrowserDirEntries[i] = NULL;
	}
	FontBrowserDirCount = 0;
	FontBrowserDirPage = 0;
	for (i = 0; i < FontBrowserFileCount; ++i)
    {
		string_free(FontBrowserFileEntries[i]);
		FontBrowserFileEntries[i] = NULL;
	}
	FontBrowserFileCount = 0;
	FontBrowserFilePage = 0;

	/*
	 * Build up the lists of directories and fixed-width font files
	 * present.
	 */
	assert(FontBrowserCurDir);
	dir = my_dopen(FontBrowserCurDir);
	if (!dir)
    {
		size_t sz1, sz2;
		int nresult;

		/* Try ANGBAND_DIR_FONTS instead. */
		dir = my_dopen(ANGBAND_DIR_FONTS);
		if (dir == NULL)
        {
			quit_fmt("could not read the directories %s and %s",
				FontBrowserCurDir, ANGBAND_DIR_FONTS);
		}
		mem_free(FontBrowserCurDir);

		/*
		 * Normalize the path (make it absolute with no relative
		 * parts and no redundant path separators).  That simplifies
		 * how the font browser works up the directory tree.
		 */
		sz1 = MSG_LEN;
		FontBrowserCurDir = mem_alloc(sz1);
		nresult = path_normalize(FontBrowserCurDir, sz1,
			ANGBAND_DIR_FONTS, true, &sz2, &FontBrowserRootSz);
		if (nresult == 1)
        {
			/* Try again with a buffer that should hold it. */
			assert(sz2 > sz1);
			FontBrowserCurDir = mem_realloc(FontBrowserCurDir, sz2);
			nresult = path_normalize(FontBrowserCurDir, sz2,
				ANGBAND_DIR_FONTS, true, NULL, &FontBrowserRootSz);
		}
		if (nresult != 0)
        {
			/* Could not normalize it. */
			quit_fmt("could not normalize %s", ANGBAND_DIR_FONTS);
		}
		dir = my_dopen(FontBrowserCurDir);
		if (!dir)
			quit_fmt("could not open the directory, %s", FontBrowserCurDir);
	}
	alter_ang_dir_only_files(dir, false);
	while (my_dread(dir, file_part, sizeof(file_part)))
    {
		path_build(full_path, sizeof(full_path), FontBrowserCurDir, file_part);
		if (dir_exists(full_path))
        {
			/*
			 * It's a directory.  If it's readable and not "."
			 * nor "..", put it in the directory list.
			 */
			if (!streq(file_part, ".") && !streq(file_part, ".."))
            {
				ang_dir *dir2 = my_dopen(full_path);

				if (dir2)
                {
					if (FontBrowserDirCount == FontBrowserDirAlloc)
                    {
						if (FontBrowserDirAlloc > ((size_t) -1) / (2 * sizeof(char*)))
                        {
							/*
							 * There's too many
							 * entries to represent
							 * in memory.
							 */
							continue;
						}
						FontBrowserDirAlloc =
							(FontBrowserDirAlloc == 0) ?
							16 : 2 * FontBrowserDirAlloc;
						FontBrowserDirEntries =
							mem_realloc(FontBrowserDirEntries,
							FontBrowserDirAlloc * sizeof(char*));
					}
					FontBrowserDirEntries[FontBrowserDirCount] =
						string_make(file_part);
					++FontBrowserDirCount;
					my_dclose(dir2);
				}
			}
		}
        else if (is_font_file(full_path))
        {
			/* Put it in the file list. */
			if (FontBrowserFileCount == FontBrowserFileAlloc)
            {
				if (FontBrowserFileAlloc > ((size_t) -1) / (2 * sizeof(char*)))
                {
					/*
					 * There's too many entries to represent
					 * in memory.
					 */
					continue;
				}
				FontBrowserFileAlloc =
					(FontBrowserFileAlloc == 0) ?
					16 : 2 * FontBrowserFileAlloc;
				FontBrowserFileEntries = mem_realloc(
					FontBrowserFileEntries,
					FontBrowserFileAlloc * sizeof(char*));
			}
			FontBrowserFileEntries[FontBrowserFileCount] =
				string_make(file_part);
			++FontBrowserFileCount;
		}
	}
	my_dclose(dir);

	/* Sort the entries. */
	if (FontBrowserDirCount > 0)
    {
		sort(FontBrowserDirEntries, FontBrowserDirCount,
			sizeof(FontBrowserDirEntries[0]), cmp_name);
	}
	if (FontBrowserFileCount > 0)
    {
		sort(FontBrowserFileEntries, FontBrowserFileCount,
			sizeof(FontBrowserFileEntries[0]), cmp_name);
	}

	/* Update what's shown. */
	if (oldcur)
    {
		i = 0;
		while (1)
        {
			if (i == FontBrowserFileCount)
            {
				sdl_Button *button;

				/*
				 * Didn't find the old selection.  Clear
				 * new_font and grey out the point size
				 * controls.
				 */
				string_free(new_font.alloc_name);
				new_font.name = NULL;
                new_font.alloc_name = NULL;
				new_font.size = 0;
				new_font.preset = false;
				new_font.bitmapped = false;
				if (FontBrowserPreviewFont) {
					sdl_FontFree(FontBrowserPreviewFont);
					mem_free(FontBrowserPreviewFont);
					FontBrowserPreviewFont = NULL;
				}
				button = sdl_ButtonBankGet(&PopUp.buttons,
					FontBrowserPtSizeBigDec);
				button->unsel_colour = back_colour;
				button->sel_colour = back_colour;
				button->cap_colour = back_colour;
				button = sdl_ButtonBankGet(&PopUp.buttons,
					FontBrowserPtSizeDec);
				button->unsel_colour = back_colour;
				button->sel_colour = back_colour;
				button->cap_colour = back_colour;
				button = sdl_ButtonBankGet(&PopUp.buttons,
					FontBrowserPtSizeInc);
				button->unsel_colour = back_colour;
				button->sel_colour = back_colour;
				button->cap_colour = back_colour;
				button = sdl_ButtonBankGet(&PopUp.buttons,
					FontBrowserPtSizeBigInc);
				button->unsel_colour = back_colour;
				button->sel_colour = back_colour;
				button->cap_colour = back_colour;
				PopUp.need_update = true;
				break;
			}
			if (streq(FontBrowserFileEntries[i], oldcur))
            {
				/*
				 * Found the old selection.  Mark it and
				 * make sure it's in the current page
				 * displayed.  The details in new_font,
				 * FontBrowserPreviewFont, and the status
				 * of the point size controls have not changed
				 * so they're left as they are.
				 */
				FontBrowserFileCur = i;
				FontBrowserFilePage =
					i / (FONT_BROWSER_PAGE_ENTRIES - 1);
				break;
			}
			++i;
		}
		string_free(oldcur);
	}
	ChangeDirPageFontBrowser(sender);
	ChangeFilePageFontBrowser(sender);
}


static void CancelFontBrowser(sdl_Button *sender)
{
	HelpFontBrowserClose();
}


static void GoUpFontBrowser(sdl_Button *sender)
{
	size_t idx = strlen(FontBrowserCurDir);
	sdl_Button *button;

	assert(idx >= 1 && FontBrowserCurDir[idx - 1] == PATH_SEPC);
	if (idx <= FontBrowserRootSz)
    {
		/* Can't go up any further. */
		return;
	}

	/* Forget the font that was being previewed. */
	FontBrowserFileCur = (size_t) -1;
	if (FontBrowserPreviewFont)
    {
		sdl_FontFree(FontBrowserPreviewFont);
		mem_free(FontBrowserPreviewFont);
		FontBrowserPreviewFont = NULL;
	}
	string_free(new_font.alloc_name);
	new_font.name = NULL;
    new_font.alloc_name = NULL;
	new_font.size = 0;
	new_font.preset = false;
	new_font.bitmapped = false;

	/*
	 * Set up the path to the new directory.  First strip off the trailing
	 * path separator.
	 */
	FontBrowserCurDir[idx - 1] = '\0';

	/*
	 * Then drop the last path component, leaving a trailing path
	 * separator.
	 */
	idx = path_filename_index(FontBrowserCurDir);
	assert(idx >= FontBrowserRootSz);
	FontBrowserCurDir[idx] = '\0';

	/* Can a path element be removed and still leave something? */
	button = sdl_ButtonBankGet(&PopUp.buttons, FontBrowserDirUp);
	if (idx > FontBrowserRootSz)
    {
		button->unsel_colour = AltUnselColour;
		button->sel_colour = AltSelColour;
		button->cap_colour = DefaultCapColour;
	}
    else
    {
		button->unsel_colour = back_colour;
		button->sel_colour = back_colour;
		button->cap_colour = back_colour;
	}

	/*
	 * Because there's no longer a selected font, grey out the point size
	 * controls.
	 */
	button = sdl_ButtonBankGet(&PopUp.buttons, FontBrowserPtSizeBigDec);
	button->unsel_colour = back_colour;
	button->sel_colour = back_colour;
	button->cap_colour = back_colour;
	button = sdl_ButtonBankGet(&PopUp.buttons, FontBrowserPtSizeDec);
	button->unsel_colour = back_colour;
	button->sel_colour = back_colour;
	button->cap_colour = back_colour;
	button = sdl_ButtonBankGet(&PopUp.buttons, FontBrowserPtSizeInc);
	button->unsel_colour = back_colour;
	button->sel_colour = back_colour;
	button->cap_colour = back_colour;
	button = sdl_ButtonBankGet(&PopUp.buttons, FontBrowserPtSizeBigInc);
	button->unsel_colour = back_colour;
	button->sel_colour = back_colour;
	button->cap_colour = back_colour;

	/* Refresh the lists of files and directories. */
	button = sdl_ButtonBankGet(&PopUp.buttons, FontBrowserRefresh);
	RefreshFontBrowser(button);

	PopUp.need_update = true;
}


static void SelectDirFontBrowser(sdl_Button *sender)
{
	sdl_Button *button;
	char *full_path;
	size_t sz, page_start = FontBrowserDirPage * (FONT_BROWSER_PAGE_ENTRIES - 1);

	assert(sender->tag >= 0 && sender->tag < FONT_BROWSER_PAGE_ENTRIES);
	if (page_start + sender->tag >= FontBrowserDirCount)
    {
		/* Directory entry is past the end; do nothing. */
		return;
	}

	/* Forget the font that was being previewed. */
	FontBrowserFileCur = (size_t) -1;
	if (FontBrowserPreviewFont)
    {
		sdl_FontFree(FontBrowserPreviewFont);
		mem_free(FontBrowserPreviewFont);
		FontBrowserPreviewFont = NULL;
	}
	string_free(new_font.alloc_name);
	new_font.name = NULL;
    new_font.alloc_name = NULL;
	new_font.size = 0;
	new_font.preset = false;
	new_font.bitmapped = false;

	/* Set up the path to the new directory. */
	sz = strlen(FontBrowserCurDir);
	assert(FontBrowserCurDir && sz >= 1
		&& FontBrowserCurDir[sz - 1] == PATH_SEPC);
	assert(FontBrowserDirEntries
		&& FontBrowserDirEntries[page_start + sender->tag]);
	sz += strlen(FontBrowserDirEntries[page_start + sender->tag]) + 2;
	full_path = mem_alloc(sz);
	strnfmt(full_path, sz, "%s%s%c", FontBrowserCurDir,
		FontBrowserDirEntries[page_start + sender->tag], PATH_SEPC);
	mem_free(FontBrowserCurDir);
	FontBrowserCurDir = full_path;

	/*
	 * Having drilled down into a directory, it'll be possible to go back
	 * up.
	 */
	button = sdl_ButtonBankGet(&PopUp.buttons, FontBrowserDirUp);
	button->unsel_colour = AltUnselColour;
	button->sel_colour = AltSelColour;
	button->cap_colour = DefaultCapColour;

	/*
	 * Because there's no longer a selected font, grey out the point size
	 * controls.
	 */
	button = sdl_ButtonBankGet(&PopUp.buttons, FontBrowserPtSizeBigDec);
	button->unsel_colour = back_colour;
	button->sel_colour = back_colour;
	button->cap_colour = back_colour;
	button = sdl_ButtonBankGet(&PopUp.buttons, FontBrowserPtSizeDec);
	button->unsel_colour = back_colour;
	button->sel_colour = back_colour;
	button->cap_colour = back_colour;
	button = sdl_ButtonBankGet(&PopUp.buttons, FontBrowserPtSizeInc);
	button->unsel_colour = back_colour;
	button->sel_colour = back_colour;
	button->cap_colour = back_colour;
	button = sdl_ButtonBankGet(&PopUp.buttons, FontBrowserPtSizeBigInc);
	button->unsel_colour = back_colour;
	button->sel_colour = back_colour;
	button->cap_colour = back_colour;

	/* Refresh the lists of files and directories. */
	button = sdl_ButtonBankGet(&PopUp.buttons, FontBrowserRefresh);
	RefreshFontBrowser(button);

	PopUp.need_update = true;
}


static void SelectFileFontBrowser(sdl_Button *sender)
{
	sdl_Button *button;
	char *work;
	size_t sz1, sz2, page_start = FontBrowserFilePage * (FONT_BROWSER_PAGE_ENTRIES - 1);
	int nresult;

	assert(sender->tag >= 0 && sender->tag < FONT_BROWSER_PAGE_ENTRIES);
	if (page_start + sender->tag >= FontBrowserFileCount)
    {
		/* File entry is past the end; do nothing. */
		return;
	}

	if (page_start + sender->tag == FontBrowserFileCur)
    {
		/* It's the same selection as before, do nothing. */
		return;
	}

	/* Fill in some details about the new font to preview. */
	assert(FontBrowserFileEntries
		&& FontBrowserFileEntries[page_start + sender->tag]);
	string_free(new_font.alloc_name);

	/* If the font is in ANGBAND_DIR_FONTS, it is a preset font. */
	sz1 = strlen(FontBrowserCurDir)
		+ strlen(FontBrowserFileEntries[page_start + sender->tag]) + 2;
	work = mem_alloc(sz1);
	nresult = path_normalize(work, sz1, ANGBAND_DIR_FONTS, true,
		&sz2, NULL);
	if (nresult == 1)
    {
		/* Try again with a buffer that should hold it. */
		assert(sz2 > sz1);
		work = mem_realloc(work, sz2);
		nresult = path_normalize(work, sz2, ANGBAND_DIR_FONTS, true,
			NULL, NULL);
	}
	if (nresult == 0 && streq(FontBrowserCurDir, work))
    {
		new_font.alloc_name = string_make(
			FontBrowserFileEntries[page_start + sender->tag]);
		new_font.preset = true;
		mem_free(work);
	}
    else
    {
		path_build(work, sz1, FontBrowserCurDir,
			FontBrowserFileEntries[page_start + sender->tag]);
		new_font.alloc_name = work;
		new_font.preset = false;
	}
    new_font.name = new_font.alloc_name;
	if (suffix_i(new_font.name, ".fon"))
    {
		new_font.size = 0;
		new_font.bitmapped = true;
	}
    else
    {
		/*
		 * If were not previewing a scalable font, then either use
		 * the size of the currently selected font for the window or
		 * the default size, as the size to start with the new font.
		 * Otherwise, keep the size that was used for the previous
		 * preview.
		 */
		if (FontBrowserFileCur == (size_t) -1 || new_font.bitmapped)
        {
			term_window *window = &windows[SelectedTerm];

			if (!window->req_font.bitmapped
					&& window->req_font.size >= MIN_POINT_SIZE
					&& window->req_font.size <= MAX_POINT_SIZE)
            {
				new_font.size = window->req_font.size;
			}
            else
				new_font.size = DEFAULT_POINT_SIZE;
		}
		new_font.bitmapped = false;
	}

	/* Change the color on the button for the currently selected font. */
	sender->cap_colour = AltCapColour;

	/*
	 * Change the color on the button for the old file selection, if
	 * visible.
	 */
	if (FontBrowserFileCur >= page_start && FontBrowserFileCur < page_start
			+ FONT_BROWSER_PAGE_ENTRIES)
    {
		button = sdl_ButtonBankGet(&PopUp.buttons,
			FontBrowserFiles[FontBrowserFileCur - page_start]);
		button->cap_colour = DefaultCapColour;
	}

	/* Remember the selection. */
	FontBrowserFileCur = page_start + sender->tag;

	/* Change the colors on the point size controls as appropriate. */
	if (!new_font.bitmapped && new_font.size > MIN_POINT_SIZE)
    {
		button = sdl_ButtonBankGet(&PopUp.buttons,
			FontBrowserPtSizeBigDec);
		button->unsel_colour = AltUnselColour;
		button->sel_colour = AltSelColour;
		button->cap_colour = DefaultCapColour;
		button = sdl_ButtonBankGet(&PopUp.buttons,
			FontBrowserPtSizeDec);
		button->unsel_colour = AltUnselColour;
		button->sel_colour = AltSelColour;
		button->cap_colour = DefaultCapColour;
	}
    else
    {
		button = sdl_ButtonBankGet(&PopUp.buttons,
			FontBrowserPtSizeBigDec);
		button->unsel_colour = back_colour;
		button->sel_colour = back_colour;
		button->cap_colour = back_colour;
		button = sdl_ButtonBankGet(&PopUp.buttons,
			FontBrowserPtSizeDec);
		button->unsel_colour = back_colour;
		button->sel_colour = back_colour;
		button->cap_colour = back_colour;
	}
	if (!new_font.bitmapped && new_font.size < MAX_POINT_SIZE)
    {
		button = sdl_ButtonBankGet(&PopUp.buttons,
			FontBrowserPtSizeInc);
		button->unsel_colour = AltUnselColour;
		button->sel_colour = AltSelColour;
		button->cap_colour = DefaultCapColour;
		button = sdl_ButtonBankGet(&PopUp.buttons,
			FontBrowserPtSizeBigInc);
		button->unsel_colour = AltUnselColour;
		button->sel_colour = AltSelColour;
		button->cap_colour = DefaultCapColour;
	}
    else
    {
		button = sdl_ButtonBankGet(&PopUp.buttons,
			FontBrowserPtSizeInc);
		button->unsel_colour = back_colour;
		button->sel_colour = back_colour;
		button->cap_colour = back_colour;
		button = sdl_ButtonBankGet(&PopUp.buttons,
			FontBrowserPtSizeBigInc);
		button->unsel_colour = back_colour;
		button->sel_colour = back_colour;
		button->cap_colour = back_colour;
	}

	/* Try to load the newly selected font. */
	if (FontBrowserPreviewFont)
		sdl_FontFree(FontBrowserPreviewFont);
	else
		FontBrowserPreviewFont = mem_alloc(sizeof(*FontBrowserPreviewFont));
	if (sdl_FontCreate(FontBrowserPreviewFont, &new_font, PopUp.surface))
    {
		/* Failed. */
		mem_free(FontBrowserPreviewFont);
		FontBrowserPreviewFont = NULL;
	}

	PopUp.need_update = true;
}


static void ChangePtSzFontBrowser(sdl_Button *sender)
{
	sdl_Button *button;

	if (FontBrowserFileCur == (size_t) -1 || new_font.bitmapped)
    {
		/* Size changes aren't relevant. */
		return;
	}
	if ((new_font.size == MIN_POINT_SIZE && sender->tag < 0) ||
        (new_font.size == MAX_POINT_SIZE && sender->tag > 0))
    {
		/* Size doesn't change. */
		return;
	}

	new_font.size = MAX(MIN_POINT_SIZE, MIN(MAX_POINT_SIZE,
		new_font.size + sender->tag));

	/* Change colors on the buttons. */
	if (new_font.size > MIN_POINT_SIZE)
    {
		button = sdl_ButtonBankGet(&PopUp.buttons,
			FontBrowserPtSizeBigDec);
		button->unsel_colour = AltUnselColour;
		button->sel_colour = AltSelColour;
		button->cap_colour = DefaultCapColour;
		button = sdl_ButtonBankGet(&PopUp.buttons,
			FontBrowserPtSizeDec);
		button->unsel_colour = AltUnselColour;
		button->sel_colour = AltSelColour;
		button->cap_colour = DefaultCapColour;
	}
    else
    {
		button = sdl_ButtonBankGet(&PopUp.buttons,
			FontBrowserPtSizeBigDec);
		button->unsel_colour = back_colour;
		button->sel_colour = back_colour;
		button->cap_colour = back_colour;
		button = sdl_ButtonBankGet(&PopUp.buttons,
			FontBrowserPtSizeDec);
		button->unsel_colour = back_colour;
		button->sel_colour = back_colour;
		button->cap_colour = back_colour;
	}
	if (new_font.size < MAX_POINT_SIZE)
    {
		button = sdl_ButtonBankGet(&PopUp.buttons,
			FontBrowserPtSizeInc);
		button->unsel_colour = AltUnselColour;
		button->sel_colour = AltSelColour;
		button->cap_colour = DefaultCapColour;
		button = sdl_ButtonBankGet(&PopUp.buttons,
			FontBrowserPtSizeBigInc);
		button->unsel_colour = AltUnselColour;
		button->sel_colour = AltSelColour;
		button->cap_colour = DefaultCapColour;
	}
    else
    {
		button = sdl_ButtonBankGet(&PopUp.buttons,
			FontBrowserPtSizeInc);
		button->unsel_colour = back_colour;
		button->sel_colour = back_colour;
		button->cap_colour = back_colour;
		button = sdl_ButtonBankGet(&PopUp.buttons,
			FontBrowserPtSizeBigInc);
		button->unsel_colour = back_colour;
		button->sel_colour = back_colour;
		button->cap_colour = back_colour;
	}

	/* Try to create the preview font at the new size. */
	if (FontBrowserPreviewFont)
		sdl_FontFree(FontBrowserPreviewFont);
	else
		FontBrowserPreviewFont = mem_alloc(sizeof(*FontBrowserPreviewFont));
	if (sdl_FontCreate(FontBrowserPreviewFont, &new_font, PopUp.surface))
    {
		/* Failed. */
		mem_free(FontBrowserPreviewFont);
		FontBrowserPreviewFont = NULL;
	}

	PopUp.need_update = true;
}


static void DrawFontBrowser(sdl_Window *win)
{
	int filepanel_left = FONT_BROWSER_BORDER + FONT_BROWSER_MARGIN
		+ 2 * (FONT_BROWSER_SUB_BORDER + FONT_BROWSER_SUB_MARGIN)
		+ (FONT_BROWSER_DIR_LENGTH + 1) * PopUp.font.width
		+ FONT_BROWSER_HOR_SPACE + 3 * PopUp.font.width
		+ 2 * PopUp.font.width;
	int subpanel_bottom = FONT_BROWSER_BORDER + FONT_BROWSER_MARGIN
		+ PopUp.font.height + 2 + 2 * (FONT_BROWSER_SUB_BORDER
		+ FONT_BROWSER_SUB_MARGIN) + FONT_BROWSER_PAGE_ENTRIES
		* (PopUp.font.height + 2);
	sdl_Button *button;
	SDL_Rect rc;

	/* Draw the panel border. */
	sdl_RECT(0, 0, win->width, win->height, &rc);
	sdl_DrawBox(win->surface, &rc, AltUnselColour, FONT_BROWSER_BORDER);

	/* Draw the subpanel labels. */
	sdl_WindowText(win, AltUnselColour, FONT_BROWSER_BORDER +
		FONT_BROWSER_MARGIN, FONT_BROWSER_BORDER + FONT_BROWSER_MARGIN,
		"Directories");
	sdl_WindowText(win, AltUnselColour, filepanel_left, FONT_BROWSER_BORDER
		+ FONT_BROWSER_MARGIN, "Fixed-width Fonts");

	/* Draw the directory subpanel border. */
	sdl_RECT(FONT_BROWSER_BORDER + FONT_BROWSER_MARGIN, FONT_BROWSER_BORDER
		+ FONT_BROWSER_MARGIN + PopUp.font.height + 2,
		filepanel_left - 2 * PopUp.font.width - FONT_BROWSER_BORDER
		- FONT_BROWSER_MARGIN,
		subpanel_bottom - PopUp.font.height - 2 - FONT_BROWSER_BORDER
		- FONT_BROWSER_MARGIN, &rc);
	sdl_DrawBox(win->surface, &rc, AltUnselColour, FONT_BROWSER_SUB_BORDER);

	/* Draw the file subpanel border. */
	sdl_RECT(filepanel_left, FONT_BROWSER_BORDER + FONT_BROWSER_MARGIN
		+ PopUp.font.height + 2, win->width - FONT_BROWSER_BORDER
		- FONT_BROWSER_MARGIN - filepanel_left, subpanel_bottom
		- PopUp.font.height - 2 - FONT_BROWSER_BORDER
		- FONT_BROWSER_MARGIN, &rc);
	sdl_DrawBox(win->surface, &rc, AltUnselColour, FONT_BROWSER_SUB_BORDER);

	/* Draw the point size label as appropriate. */
	button = sdl_ButtonBankGet(&PopUp.buttons, FontBrowserPtSizeBigDec);
	if (FontBrowserFileCur != (size_t) -1 && !new_font.bitmapped)
    {
		assert(new_font.size >= MIN_POINT_SIZE
			&& new_font.size <= MAX_POINT_SIZE);
		sdl_WindowText(win, AltUnselColour, button->pos.x
			+ 12 * PopUp.font.width, button->pos.y + 1,
			format("%d pt", new_font.size));
	}
    else
    {
		int n = (int) strlen(format("%d pt", MAX_POINT_SIZE));

		sdl_RECT(button->pos.x + 12 * PopUp.font.width, button->pos.y + 1,
			n * PopUp.font.width, PopUp.font.height, &rc);
		SDL_FillRect(win->surface, &rc, back_pixel_colour);
	}

	if (FontBrowserPreviewFont)
    {
		/*
		 * Redraw the preview area.  Like sdl_FontDraw() but do not
		 * use the font associated with the window.  Include x07 in
		 * the preview to see what glyph is there for the code page
		 * 437 centered dot.  Include the UTF-8 sequence xC2 xB7 to
		 * see what glyph is there for U+00B7, the Unicode centered
		 * dot.
		 */
		const char *preview_contents[5] =
        {
			"abcdefghijklmnopqrst",
			"uvwxyz1234567890-=,.",
			"ABCDEFGHIJKLMNOPQRST",
			"UVWXYZ!@#$%^&*()_+<>",
			"/?;:'\"[{]}\\|`~\x07\xC2\xB7    "
		};
		int preview_bottom, i;

		if (SDL_MUSTLOCK(win->surface))
        {
			if (SDL_LockSurface(win->surface) < 0)
				return;
		}

		if (20 * FontBrowserPreviewFont->width
				> win->width - 2 * (FONT_BROWSER_BORDER
				+ FONT_BROWSER_MARGIN))
        {
			/* Doesn't fit horizontally.  Clip. */
			rc.x = FONT_BROWSER_BORDER + FONT_BROWSER_MARGIN;
			rc.w = win->width - 2 * (FONT_BROWSER_BORDER
				+ FONT_BROWSER_MARGIN);
		}
        else
        {
			rc.x = (win->width - 20
				* FontBrowserPreviewFont->width) / 2;
			rc.w = 20 * FontBrowserPreviewFont->width;
		}
		rc.y = subpanel_bottom + FONT_BROWSER_VER_SPACE
			+ PopUp.font.height + 2 + FONT_BROWSER_VER_SPACE;
		assert(rc.y < win->height - FONT_BROWSER_PREVIEW_HEIGHT);
		preview_bottom = rc.y + FONT_BROWSER_PREVIEW_HEIGHT;
		if (FontBrowserPreviewFont->height > FONT_BROWSER_PREVIEW_HEIGHT)
        {
			/* One row doesn't fit vertically.  Clip. */
			rc.h = FONT_BROWSER_PREVIEW_HEIGHT;
		}
        else
			rc.h = FontBrowserPreviewFont->height;
		for (i = 0; i < 5; ++i)
        {
			SDL_Surface *text = TTF_RenderUTF8_Solid(
				FontBrowserPreviewFont->sdl_font,
				preview_contents[i], AltUnselColour);

			if (text)
            {
				SDL_BlitSurface(text, NULL, PopUp.surface, &rc);
				SDL_FreeSurface(text);
			}
			rc.y += FontBrowserPreviewFont->height;
			if (rc.y >= preview_bottom)
				break;
			if (rc.y + FontBrowserPreviewFont->height > preview_bottom)
				rc.h = preview_bottom - rc.y;
		}

		if (SDL_MUSTLOCK(win->surface)) {
			SDL_UnlockSurface(win->surface);
		}
	}
}


/*
 * See the comment for ActivatePointSize() for why enabling and disabling
 * buttons is done by coloring them rather than using sdl_ButtonVisible().
 */
static void ActivateFontBrowser(sdl_Button *sender)
{
	term_window *window = &windows[SelectedTerm];
	int ptsz_width = ((int) strlen(format("%d pt", MAX_POINT_SIZE)))
		* PopUp.font.width;
	int height, width;
	int subpanel_top, subpanel_bottom;
	int dirpanel_right, filepanel_left, filepanel_right, ptsize_left;
	sdl_Button *button;
	int i;

	RemovePopUp();

	/*
	 * Clear new_font, since a new font file hasn't been selected by this
	 * use of the browser.
	 */
	FontBrowserFileCur = (size_t) -1;
	if (FontBrowserPreviewFont)
    {
		sdl_FontFree(FontBrowserPreviewFont);
		mem_free(FontBrowserPreviewFont);
		FontBrowserPreviewFont = NULL;
	}
	string_free(new_font.alloc_name);
	new_font.name = NULL;
    new_font.alloc_name = NULL;
	new_font.size = 0;
	new_font.preset = false;
	new_font.bitmapped = false;

	/*
	 * If the current font is not preset, use its directory as the
	 * starting point for the browser; otherwise, use the last directory
	 * browsed by the browser or, if no browsing has been done so far,
	 * ANGBAND_DIR_FONTS.
	 */
	mem_free(FontBrowserCurDir);
	FontBrowserCurDir = NULL;
	if (!window->req_font.preset && window->req_font.name)
    {
		size_t sz1, sz2, file_index;
		int nresult;

		/*
		 * Normalize the path.  It could have come from the preference
		 * file and may not be already normalized.
		 */
		sz1 = strlen(window->req_font.name) + 1;
		FontBrowserCurDir = mem_alloc(sz1);
		nresult = path_normalize(FontBrowserCurDir, sz1,
			window->req_font.name, false, &sz2, &FontBrowserRootSz);
		if (nresult == 1)
        {
			/* Try again with a buffer that should hold it. */
			assert(sz2 > sz1);
			FontBrowserCurDir = mem_realloc(FontBrowserCurDir, sz2);
			nresult = path_normalize(FontBrowserCurDir, sz2,
				window->req_font.name, false, NULL,
				&FontBrowserRootSz);
		}
		if (nresult == 0)
        {
			/*
			 * Terminate just past the last directory separator in
			 * the path name.
			 */
			file_index = path_filename_index(FontBrowserCurDir);
			assert(file_index >= FontBrowserRootSz);
			FontBrowserCurDir[file_index] = '\0';
		}
        else
        {
			mem_free(FontBrowserCurDir);
			FontBrowserCurDir = NULL;
		}
	}
    else if (FontBrowserLastDir)
    {
		FontBrowserCurDir = string_make(FontBrowserLastDir);
        FontBrowserRootSz = FontBrowserLastRootSz;
    }
	if (!FontBrowserCurDir)
    {
		size_t sz1, sz2;
		int nresult;

		sz1 = 1024;
		FontBrowserCurDir = mem_alloc(sz1);
		nresult = path_normalize(FontBrowserCurDir, sz1,
			ANGBAND_DIR_FONTS, true, &sz2, &FontBrowserRootSz);
		if (nresult == 1)
        {
			/* Try again with a buffer that should hold it. */
			assert(sz2 > sz1);
			FontBrowserCurDir = mem_realloc(FontBrowserCurDir, sz2);
			nresult = path_normalize(FontBrowserCurDir, sz2,
				ANGBAND_DIR_FONTS, true, NULL,
				&FontBrowserRootSz);
		}
		if (nresult != 0)
			quit_fmt("could not normalize %s", ANGBAND_DIR_FONTS);
	}

	/* Set the window for the browser panel. */
	/* First account for the outside border. */
	height = 2 * (FONT_BROWSER_BORDER + FONT_BROWSER_MARGIN);
	width = 2 * (FONT_BROWSER_BORDER + FONT_BROWSER_MARGIN);

	/*
	 * Then for the directory and file subpanels with their labels
	 * and borders.
	 */
	height += 2 * (FONT_BROWSER_SUB_BORDER + FONT_BROWSER_SUB_MARGIN)
		+ PopUp.font.height + 2
		+ FONT_BROWSER_PAGE_ENTRIES * (PopUp.font.height + 2);
	width += 4 * (FONT_BROWSER_SUB_BORDER + FONT_BROWSER_SUB_MARGIN)
		+ (FONT_BROWSER_DIR_LENGTH + 1) * PopUp.font.width
		+ (FONT_BROWSER_FILE_LENGTH + 1) * PopUp.font.width;

	/*
	 * Now account for the scroll controls and a two character wide
	 * space between the file and directory subpanels.
	 */
	width += 2 * (FONT_BROWSER_HOR_SPACE + 3 * PopUp.font.width)
		+ 2 * PopUp.font.width;

	/*
	 * Account for the point size control and its separation from the
	 * subpanels.
	 */
	height += FONT_BROWSER_VER_SPACE + PopUp.font.height + 2;

	/*
	 * Account for the preview and its separation from the point size
	 * control.
	 */
	height += FONT_BROWSER_VER_SPACE + FONT_BROWSER_PREVIEW_HEIGHT;

	/*
	 * Account for the action buttons and their one character high
	 * separation from the preview.
	 */
	height += 2 * (PopUp.font.height + 2);
	sdl_WindowInit(&PopUp, width, height, AppWin, StatusBar.font.req);
	PopUp.left = AppWin->w / 2 - width / 2;
	PopUp.top = StatusBar.height + 2;
	PopUp.draw_extra = DrawFontBrowser;

	/* Set buttons for the directory and file subpanels. */
	subpanel_top = FONT_BROWSER_BORDER + FONT_BROWSER_MARGIN
		+ PopUp.font.height + 2;
	dirpanel_right = FONT_BROWSER_BORDER + FONT_BROWSER_MARGIN
		+ 2 * (FONT_BROWSER_SUB_BORDER + FONT_BROWSER_SUB_MARGIN)
		+ (FONT_BROWSER_DIR_LENGTH + 1) * PopUp.font.width
		+ FONT_BROWSER_HOR_SPACE + 3 * PopUp.font.width;
	FontBrowserDirUp = sdl_ButtonBankNew(&PopUp.buttons);
	button = sdl_ButtonBankGet(&PopUp.buttons, FontBrowserDirUp);

	/*
	 * Grey it out if there's not at least one element in the path that
	 * can be stripped off while still leaving something in the path.
	 */
	if (strlen(FontBrowserCurDir) <= FontBrowserRootSz)
    {
		button->unsel_colour = back_colour;
		button->sel_colour = back_colour;
		button->cap_colour = back_colour;
	}
    else
    {
		button->unsel_colour = AltUnselColour;
		button->sel_colour = AltSelColour;
		button->cap_colour = DefaultCapColour;
	}
	sdl_ButtonSize(button, 4 * PopUp.font.width, PopUp.font.height + 2);
	sdl_ButtonVisible(button, true);
	sdl_ButtonCaption(button, "Up");
	sdl_ButtonMove(button, dirpanel_right - 4 * PopUp.font.width,
		subpanel_top - PopUp.font.height - 2);
	button->activate = GoUpFontBrowser;
	for (i = 0; i < FONT_BROWSER_PAGE_ENTRIES; ++i)
    {
		FontBrowserDirectories[i] = sdl_ButtonBankNew(&PopUp.buttons);
		button = sdl_ButtonBankGet(&PopUp.buttons,
			FontBrowserDirectories[i]);

		/* Grey it out until needed. */
		button->unsel_colour = back_colour;
		button->sel_colour = back_colour;
		button->cap_colour = back_colour;
		sdl_ButtonSize(button, (FONT_BROWSER_DIR_LENGTH + 1)
			* PopUp.font.width, PopUp.font.height + 2);
		sdl_ButtonVisible(button, true);
		sdl_ButtonCaption(button, "");
		sdl_ButtonMove(button, FONT_BROWSER_BORDER
			+ FONT_BROWSER_MARGIN + FONT_BROWSER_SUB_BORDER
			+ FONT_BROWSER_SUB_MARGIN, subpanel_top
			+ FONT_BROWSER_SUB_BORDER + FONT_BROWSER_SUB_MARGIN
			+ i * (PopUp.font.height + 2));
		button->tag = i;
		button->activate = SelectDirFontBrowser;
	}
	dirpanel_right = FONT_BROWSER_BORDER + FONT_BROWSER_MARGIN
		+ 2 * (FONT_BROWSER_SUB_BORDER + FONT_BROWSER_SUB_MARGIN)
		+ (FONT_BROWSER_DIR_LENGTH + 1) * PopUp.font.width
		+ FONT_BROWSER_HOR_SPACE + 3 * PopUp.font.width;
	FontBrowserDirPageBefore = sdl_ButtonBankNew(&PopUp.buttons);
	button = sdl_ButtonBankGet(&PopUp.buttons, FontBrowserDirPageBefore);

	/* Grey it out until needed. */
	button->unsel_colour = back_colour;
	button->sel_colour = back_colour;
	button->cap_colour = back_colour;
	sdl_ButtonSize(button, 3 * PopUp.font.width, PopUp.font.height + 2);
	sdl_ButtonVisible(button, true);
	sdl_ButtonCaption(button, "-");
	sdl_ButtonMove(button, dirpanel_right - 3 * PopUp.font.width
		- FONT_BROWSER_SUB_BORDER - FONT_BROWSER_SUB_MARGIN,
		subpanel_top + FONT_BROWSER_SUB_BORDER
		+ FONT_BROWSER_SUB_MARGIN);
	button->tag = -1;
	button->activate = ChangeDirPageFontBrowser;
	FontBrowserDirPageAfter = sdl_ButtonBankNew(&PopUp.buttons);
	button = sdl_ButtonBankGet(&PopUp.buttons, FontBrowserDirPageAfter);

	/* Grey it out until needed. */
	button->unsel_colour = back_colour;
	button->sel_colour = back_colour;
	button->cap_colour = back_colour;
	sdl_ButtonSize(button, 3 * PopUp.font.width, PopUp.font.height + 2);
	sdl_ButtonVisible(button, true);
	sdl_ButtonCaption(button, "+");
	sdl_ButtonMove(button, dirpanel_right - FONT_BROWSER_SUB_BORDER
		- FONT_BROWSER_SUB_MARGIN - 3 * PopUp.font.width,
		subpanel_top + FONT_BROWSER_SUB_BORDER
		+ FONT_BROWSER_SUB_MARGIN + (FONT_BROWSER_PAGE_ENTRIES - 1)
		* (PopUp.font.height + 2));
	button->tag = 1;
	button->activate = ChangeDirPageFontBrowser;

	/*
	 * Have a button that does nothing between the page before and page
	 * after buttons so that stray clicks in that area don't dismiss
	 * the 'Font Browser' panel.  If the button had access to the click
	 * coordinates, it could implement jumping to a page.
	 */
	FontBrowserDirPageDummy = sdl_ButtonBankNew(&PopUp.buttons);
	button = sdl_ButtonBankGet(&PopUp.buttons, FontBrowserDirPageDummy);
	button->unsel_colour = back_colour;
	button->sel_colour = back_colour;
	button->cap_colour = back_colour;
	sdl_ButtonSize(button, 3 * PopUp.font.width, (FONT_BROWSER_PAGE_ENTRIES
		- 2) * (PopUp.font.height + 2));
	sdl_ButtonVisible(button, true);
	sdl_ButtonCaption(button, "");
	sdl_ButtonMove(button, dirpanel_right - FONT_BROWSER_SUB_BORDER
		- FONT_BROWSER_SUB_MARGIN - 3 * PopUp.font.width,
		subpanel_top + FONT_BROWSER_SUB_BORDER
		+ FONT_BROWSER_SUB_MARGIN + PopUp.font.height + 2);

	filepanel_left = dirpanel_right + 2 * PopUp.font.width;
	for (i = 0; i < FONT_BROWSER_PAGE_ENTRIES; ++i)
    {
		FontBrowserFiles[i] = sdl_ButtonBankNew(&PopUp.buttons);
		button = sdl_ButtonBankGet(&PopUp.buttons,
			FontBrowserFiles[i]);

		/* Grey it out until needed. */
		button->unsel_colour = back_colour;
		button->sel_colour = back_colour;
		button->cap_colour = back_colour;
		sdl_ButtonSize(button, (FONT_BROWSER_FILE_LENGTH + 1)
			* PopUp.font.width, PopUp.font.height + 2);
		sdl_ButtonVisible(button, true);
		sdl_ButtonCaption(button, "");
		sdl_ButtonMove(button, filepanel_left + FONT_BROWSER_SUB_BORDER
			+ FONT_BROWSER_SUB_MARGIN, subpanel_top
			+ FONT_BROWSER_SUB_BORDER + FONT_BROWSER_SUB_MARGIN
			+ i * (PopUp.font.height + 2));
		button->tag = i;
		button->activate = SelectFileFontBrowser;
	}
	filepanel_right = width - FONT_BROWSER_BORDER - FONT_BROWSER_MARGIN;
	FontBrowserFilePageBefore = sdl_ButtonBankNew(&PopUp.buttons);
	button = sdl_ButtonBankGet(&PopUp.buttons, FontBrowserFilePageBefore);

	/* Grey it out until needed. */
	button->unsel_colour = back_colour;
	button->sel_colour = back_colour;
	button->cap_colour = back_colour;
	sdl_ButtonSize(button, 3 * PopUp.font.width, PopUp.font.height + 2);
	sdl_ButtonVisible(button, true);
	sdl_ButtonCaption(button, "-");
	sdl_ButtonMove(button, filepanel_right - FONT_BROWSER_SUB_BORDER
		- FONT_BROWSER_SUB_MARGIN - 3 * PopUp.font.width, subpanel_top
		+ FONT_BROWSER_SUB_BORDER + FONT_BROWSER_SUB_MARGIN);
	button->tag = -1;
	button->activate = ChangeFilePageFontBrowser;
	FontBrowserFilePageAfter = sdl_ButtonBankNew(&PopUp.buttons);
	button = sdl_ButtonBankGet(&PopUp.buttons, FontBrowserFilePageAfter);

	/* Grey it out until needed. */
	button->unsel_colour = back_colour;
	button->sel_colour = back_colour;
	button->cap_colour = back_colour;
	sdl_ButtonSize(button, 3 * PopUp.font.width, PopUp.font.height + 2);
	sdl_ButtonVisible(button, true);
	sdl_ButtonCaption(button, "+");
	sdl_ButtonMove(button, filepanel_right - FONT_BROWSER_SUB_BORDER
		- FONT_BROWSER_SUB_MARGIN - 3 * PopUp.font.width,
		subpanel_top + FONT_BROWSER_SUB_BORDER
		+ FONT_BROWSER_SUB_MARGIN + (FONT_BROWSER_PAGE_ENTRIES - 1)
		* (PopUp.font.height + 2));
	button->tag = 1;
	button->activate = ChangeFilePageFontBrowser;

	/* As above, this is a dummy button to absorb frequent stray clicks. */
	FontBrowserFilePageDummy = sdl_ButtonBankNew(&PopUp.buttons);
	button = sdl_ButtonBankGet(&PopUp.buttons, FontBrowserFilePageDummy);
	button->unsel_colour = back_colour;
	button->sel_colour = back_colour;
	button->cap_colour = back_colour;
	sdl_ButtonSize(button, 3 * PopUp.font.width, (FONT_BROWSER_PAGE_ENTRIES
		- 2) * (PopUp.font.height + 2));
	sdl_ButtonVisible(button, true);
	sdl_ButtonCaption(button, "");
	sdl_ButtonMove(button, filepanel_right - FONT_BROWSER_SUB_BORDER
		- FONT_BROWSER_SUB_MARGIN - 3 * PopUp.font.width,
		subpanel_top + FONT_BROWSER_SUB_BORDER
		+ FONT_BROWSER_SUB_MARGIN + PopUp.font.height + 2);

	/*
	 * Set up the point size controls, all are greyed out until needed.
	 * Center them, if possible, below the file panel.
	 */
	if (24 * PopUp.font.width + ptsz_width
			> filepanel_right - filepanel_left)
    {
		ptsize_left = filepanel_right - 24 * PopUp.font.width
			- ptsz_width;
	}
    else
    {
		ptsize_left = filepanel_left + (filepanel_right
			- filepanel_left - 24 * PopUp.font.width
			- ptsz_width) / 2;
	}
	subpanel_bottom = subpanel_top + 2 * (FONT_BROWSER_SUB_BORDER
		+ FONT_BROWSER_SUB_MARGIN) + FONT_BROWSER_PAGE_ENTRIES
		* (PopUp.font.height + 2);
	FontBrowserPtSizeBigDec = sdl_ButtonBankNew(&PopUp.buttons);
	button = sdl_ButtonBankGet(&PopUp.buttons, FontBrowserPtSizeBigDec);
	button->unsel_colour = back_colour;
	button->sel_colour = back_colour;
	button->cap_colour = back_colour;
	sdl_ButtonSize(button, 4 * PopUp.font.width, PopUp.font.height + 2);
	sdl_ButtonVisible(button, true);
	sdl_ButtonCaption(button, "--");
	sdl_ButtonMove(button, ptsize_left, subpanel_bottom
		+ FONT_BROWSER_VER_SPACE);
	button->tag = -10;
	button->activate = ChangePtSzFontBrowser;
	FontBrowserPtSizeDec = sdl_ButtonBankNew(&PopUp.buttons);
	button = sdl_ButtonBankGet(&PopUp.buttons, FontBrowserPtSizeDec);
	button->unsel_colour = back_colour;
	button->sel_colour = back_colour;
	button->cap_colour = back_colour;
	sdl_ButtonSize(button, 4 * PopUp.font.width, PopUp.font.height + 2);
	sdl_ButtonVisible(button, true);
	sdl_ButtonCaption(button, " -");
	sdl_ButtonMove(button, ptsize_left + 6 * PopUp.font.width,
		subpanel_bottom + FONT_BROWSER_VER_SPACE);
	button->tag = -1;
	button->activate = ChangePtSzFontBrowser;
	FontBrowserPtSizeInc = sdl_ButtonBankNew(&PopUp.buttons);
	button = sdl_ButtonBankGet(&PopUp.buttons, FontBrowserPtSizeInc);
	button->unsel_colour = back_colour;
	button->sel_colour = back_colour;
	button->cap_colour = back_colour;
	sdl_ButtonSize(button, 4 * PopUp.font.width, PopUp.font.height + 2);
	sdl_ButtonVisible(button, true);
	sdl_ButtonCaption(button, " +");
	sdl_ButtonMove(button, ptsize_left + 14 * PopUp.font.width
		+ ptsz_width, subpanel_bottom + FONT_BROWSER_VER_SPACE);
	button->tag = 1;
	button->activate = ChangePtSzFontBrowser;
	FontBrowserPtSizeBigInc = sdl_ButtonBankNew(&PopUp.buttons);
	button = sdl_ButtonBankGet(&PopUp.buttons, FontBrowserPtSizeBigInc);
	button->unsel_colour = back_colour;
	button->sel_colour = back_colour;
	button->cap_colour = back_colour;
	sdl_ButtonSize(button, 4 * PopUp.font.width, PopUp.font.height + 2);
	sdl_ButtonVisible(button, true);
	sdl_ButtonCaption(button, "++");
	sdl_ButtonMove(button, ptsize_left + 20 * PopUp.font.width
		+ ptsz_width, subpanel_bottom + FONT_BROWSER_VER_SPACE);
	button->tag = 10;
	button->activate = ChangePtSzFontBrowser;

	FontBrowserOk = sdl_ButtonBankNew(&PopUp.buttons);
	button = sdl_ButtonBankGet(&PopUp.buttons, FontBrowserOk);
	button->unsel_colour = AltUnselColour;
	button->sel_colour = AltSelColour;
	sdl_ButtonSize(button, 10 * PopUp.font.width, PopUp.font.height + 2);
	sdl_ButtonVisible(button, true);
	sdl_ButtonCaption(button, "OK");
	sdl_ButtonMove(button, width / 2 - 19 * PopUp.font.width, height
		- FONT_BROWSER_BORDER - FONT_BROWSER_MARGIN
		- PopUp.font.height - 2);
	button->activate = AcceptFontBrowser;

	/*
	 * Have a refresh button since file system change notifications are
	 * not monitored.  That way the user has a way to resync with what's
	 * in the directory without closing and reopening the browser panel
	 * or, for some changes, going to another directory and then back to
	 * the current one.
	 */
	FontBrowserRefresh = sdl_ButtonBankNew(&PopUp.buttons);
	button = sdl_ButtonBankGet(&PopUp.buttons, FontBrowserRefresh);
	button->unsel_colour = AltUnselColour;
	button->sel_colour = AltSelColour;
	sdl_ButtonSize(button, 10 * PopUp.font.width, PopUp.font.height + 2);
	sdl_ButtonVisible(button, true);
	sdl_ButtonCaption(button, "Refresh");
	sdl_ButtonMove(button, width / 2 - 5 * PopUp.font.width, height
		- FONT_BROWSER_BORDER - FONT_BROWSER_MARGIN
		- PopUp.font.height - 2);

	/*
	 * Set the tag to zero so this button can be passed to
	 * ChangeDirPageFontBrowser() and ChangeFilePageFontBrowser().
	 */
	button->tag = 0;
	button->activate = RefreshFontBrowser;

	FontBrowserCancel = sdl_ButtonBankNew(&PopUp.buttons);
	button = sdl_ButtonBankGet(&PopUp.buttons, FontBrowserCancel);
	button->unsel_colour = AltUnselColour;
	button->sel_colour = AltSelColour;
	sdl_ButtonSize(button, 10 * PopUp.font.width, PopUp.font.height + 2);
	sdl_ButtonVisible(button, true);
	sdl_ButtonCaption(button, "Cancel");
	sdl_ButtonMove(button, width / 2 + 9 * PopUp.font.width, height
		- FONT_BROWSER_BORDER - FONT_BROWSER_MARGIN
		- PopUp.font.height - 2);
	button->activate = CancelFontBrowser;

	/*
	 * Update the browser for the directories and files in the directory
	 * being browsed.
	 */
	button = sdl_ButtonBankGet(&PopUp.buttons, FontBrowserRefresh);
	RefreshFontBrowser(button);

	popped = true;
}


static int get_font_namewidth(void)
{
    const char *browse_label = "Other ...";
    int i, maxl = (int) strlen(browse_label);

    for (i = 0; i < num_fonts; i++)
    {
        size_t sl = strlen(FontList[i]);

        /*
		 * Use 49 because that is one less than the maximum size of
		 * a button's caption.
		 */
		maxl = (sl >= 49) ? 49 : MAX(maxl, (int) sl);
    }

    return (maxl * StatusBar.font.width + 20);
}


static void FontActivate(sdl_Button *sender)
{
    const char *browse_label = "Other ...";
	term_window *window = &windows[SelectedTerm];
	int i;
	int extra, width, height;
	int h, b;
	sdl_Button *button;

	/*
	 * Always add an additional menu item to open a file browser to
	 * select a font file.  If not currently using a preset font, also
	 * add an entry which corresponds to that font.
	 */
	if (window->req_font.preset)
		extra = 1;
	else
		extra = 2;
	width = get_font_namewidth();
	height = (num_fonts + extra) * (StatusBar.font.height + 1);

	sdl_WindowInit(&PopUp, width, height, AppWin, StatusBar.font.req);
	PopUp.left = sender->pos.x;
	PopUp.top = sender->pos.y;

	h = PopUp.font.height;
	for (i = 0; i < num_fonts; i++)
    {
		b = sdl_ButtonBankNew(&PopUp.buttons);
		button = sdl_ButtonBankGet(&PopUp.buttons, b);
		sdl_ButtonSize(button, width - 2 , h);
		sdl_ButtonMove(button, 1, i * (h + 1));
		if (window->req_font.preset && streq(window->req_font.name, FontList[i]))
			button->cap_colour = AltCapColour;
		sdl_ButtonCaption(button, FontList[i]);
		sdl_ButtonVisible(button, true);
		button->activate = (suffix_i(FontList[i], ".fon")? SelectPresetBitmappedFont:
            SelectPresetScalableFont);
	}

	if (extra == 2)
    {
		/*
		 * Add a button corresponding to the current, but not preset
		 * font.
		 */
		char caption[50];

		b = sdl_ButtonBankNew(&PopUp.buttons);
		button = sdl_ButtonBankGet(&PopUp.buttons, b);
		sdl_ButtonSize(button, width - 2 , h);
		sdl_ButtonMove(button, 1, num_fonts * (h + 1));
		button->cap_colour = AltCapColour;
		get_font_short_name(caption, sizeof(caption),
			&window->req_font);
		sdl_ButtonCaption(button, caption);
		sdl_ButtonVisible(button, true);
		button->activate = AlterNonPresetFontSize;
	}

	/* Add the button for launching the font file browser dialog. */
	b = sdl_ButtonBankNew(&PopUp.buttons);
	button = sdl_ButtonBankGet(&PopUp.buttons, b);
	sdl_ButtonSize(button, width - 2 , h);
	sdl_ButtonMove(button, 1, (num_fonts + extra - 1) * (h + 1));
	sdl_ButtonCaption(button, browse_label);
	sdl_ButtonVisible(button, true);
	button->activate = ActivateFontBrowser;

	popped = true;
}


static errr load_gfx(void);
static bool do_update_w = false;
static bool do_update = false;


static void SelectGfx(sdl_Button *sender)
{
    SelectedGfx = sender->tag;
}


/*
 * Compute tile width/height multipliers to display the best possible tiles
 */
static void apply_nice_graphics(term_window* win)
{
    graphics_mode *mode = get_graphics_mode(use_graphics, true);
    int nice_tile_wid = 0, nice_tile_hgt = 0;

    if (!nicegfx) return;

    if (mode && mode->grafID)
    {
        if (mode->file[0])
        {
            char *end;

            nice_tile_wid = strtol(mode->file, &end, 10);
            nice_tile_hgt = strtol(end + 1, NULL, 10);
        }
        if ((nice_tile_wid == 0) || (nice_tile_hgt == 0))
        {
            nice_tile_wid = mode->cell_width;
            nice_tile_hgt = mode->cell_height;
        }
    }
    if ((nice_tile_wid == 0) || (nice_tile_hgt == 0))
    {
        nice_tile_wid = win->tile_wid;
        nice_tile_hgt = win->tile_hgt;
    }

    if (nice_tile_wid >= win->tile_wid * 2)
        tile_width = nice_tile_wid / win->tile_wid;
    if (nice_tile_hgt >= win->tile_hgt * 2)
        tile_height = nice_tile_hgt / win->tile_hgt;
}


static void AcceptChanges(sdl_Button *sender)
{
    sdl_Button *button;
    bool do_video_reset = false;

    if (use_graphics != SelectedGfx)
    {
        do_update = true;

        use_graphics = SelectedGfx;
    }

    if (!use_graphics) reset_tile_params();

    button = sdl_ButtonBankGet(&PopUp.buttons, MoreNiceGfx);
    if (button->tag != nicegfx)
    {
        nicegfx = !nicegfx;
        do_update = true;
    }

    load_gfx();

    /* Reset visuals */
    reset_visuals(true);

    /* Invalidate all the gfx surfaces */
    if (do_update)
    {
        int i;

        for (i = 0; i < ANGBAND_TERM_MAX; i++)
        {
            term_window *win = &windows[i];

            if (win->tiles)
            {
                SDL_FreeSurface(win->tiles);
                win->tiles = NULL;
            }
        }
    }

    button = sdl_ButtonBankGet(&PopUp.buttons, MoreFullscreen);

    if (button->tag != fullscreen)
    {
        fullscreen = !fullscreen;

        do_video_reset = true;
    }

    SetStatusButtons();

    RemovePopUp();

    /* Hacks */
    if (do_update_w)
    {
        ResizeWin(&windows[SelectedTerm], windows[SelectedTerm].width, windows[SelectedTerm].height);

        /* Show on the screen */
        sdl_BlitAll();
    }

    if (do_update)
    {
        /* Redraw */
        if (Setup.initialized)
        {
            ResizeWin(&windows[0], windows[0].width, windows[0].height);
            do_cmd_redraw();
        }

        /* This will set up the window correctly */
        else
            ResizeWin(&windows[0], windows[0].width, windows[0].height);
    }

    if (do_video_reset)
    {
        SDL_Event Event;

        memset(&Event, 0, sizeof(SDL_Event));

        Event.type = SDL_VIDEORESIZE;
        Event.resize.w = screen_w;
        Event.resize.h = screen_h;

        SDL_PushEvent(&Event);
    }

    do_update_w = false;
    do_update = false;
}


static void FlipTag(sdl_Button *sender)
{
    if (sender->tag)
    {
        sender->tag = 0;
        sdl_ButtonCaption(sender, "Off");
    }
    else
    {
        sender->tag = 1;
        sdl_ButtonCaption(sender, "On");
    }
}


static void SnapChange(sdl_Button *sender)
{
    SnapRange += sender->tag;
    if (SnapRange < 0) SnapRange = 0;
    if (SnapRange > 20) SnapRange = 20;
    PopUp.need_update = true;
}


static void WidthChange(sdl_Button *sender)
{
    tile_width += sender->tag;
    if (tile_width < 1) tile_width = 1;
    if (tile_width > 12) tile_width = 12;
    do_update = true;
}


static void HeightChange(sdl_Button *sender)
{
    tile_height += sender->tag;
    if (tile_height < 1) tile_height = 1;
    if (tile_height > 8) tile_height = 8;
    do_update = true;
}


static void WindowBordersChange(sdl_Button *sender)
{
    term_window *window = &windows[SelectedTerm];

    window->windowborders += sender->tag;
    if (window->windowborders < 0) window->windowborders = BASIC_COLORS;
    if (window->windowborders > BASIC_COLORS) window->windowborders = 0;
    do_update_w = true;
    do_update = true;
}


static void SoundVolumeChange(sdl_Button *sender)
{
    sound_volume += sender->tag;
    if (sound_volume < 0) sound_volume = 0;
    if (sound_volume > 100) sound_volume = 100;
}


static void MusicVolumeChange(sdl_Button *sender)
{
    music_volume += sender->tag;
    if (music_volume < 0) music_volume = 0;
    if (music_volume > 100) music_volume = 100;
}


static void MoreDraw(sdl_Window *win)
{
    term_window *window = &windows[SelectedTerm];
    SDL_Rect rc;
    sdl_Button *button;
    int y = 20;
    graphics_mode *mode;
    int tag;

    sdl_RECT(0, 0, win->width, win->height, &rc);

    /* Draw a nice box */
    sdl_DrawBox(win->surface, &rc, AltUnselColour, 5);

    sdl_WindowText(win, AltUnselColour, 20, y, "Selected Graphics:");

    mode = get_graphics_mode(SelectedGfx, false);
    if (mode && mode->grafID)
        sdl_WindowText(win, AltSelColour, 150, y, mode->menuname);
    else
        sdl_WindowText(win, AltSelColour, 150, y, "None");

    y += 20;

    /* Only allow changes to the graphics mode in initial phase */
    if (!Setup.initialized) sdl_WindowText(win, AltUnselColour, 20, y, "Available Graphics:");

    mode = graphics_modes;
    while (mode)
    {
        if (mode->menuname[0])
        {
            button = sdl_ButtonBankGet(&win->buttons, GfxButtons[mode->grafID]);
            if (!Setup.initialized)
            {
                sdl_ButtonMove(button, 150, y);
                sdl_ButtonVisible(button, true);
                y += 20;
            }
            else
                sdl_ButtonVisible(button, false);
        }
        mode = mode->pNext;
    }

    sdl_WindowText(win, AltUnselColour, 20, y, "Nice graphics is:");
    button = sdl_ButtonBankGet(&win->buttons, MoreNiceGfx);
    tag = button->tag;
    if (!Setup.initialized)
    {
        sdl_ButtonMove(button, 150, y);
        sdl_ButtonVisible(button, true);
    }
    else
    {
        sdl_ButtonVisible(button, false);
        sdl_WindowText(win, AltSelColour, 150, y, (tag? "On": "Off"));
    }
    y += 20;

    if (SelectedGfx)
        sdl_WindowText(win, AltUnselColour, 20, y, format("Tile width is %d.", tile_width));
    button = sdl_ButtonBankGet(&win->buttons, MoreWidthMinus);
    if (SelectedGfx && !tag && !Setup.initialized)
    {
        sdl_ButtonMove(button, 150, y);
        sdl_ButtonVisible(button, true);
    }
    else
        sdl_ButtonVisible(button, false);
    button = sdl_ButtonBankGet(&win->buttons, MoreWidthPlus);
    if (SelectedGfx && !tag && !Setup.initialized)
    {
        sdl_ButtonMove(button, 180, y);
        sdl_ButtonVisible(button, true);
    }
    else
        sdl_ButtonVisible(button, false);
    if (SelectedGfx) y += 20;

    if (SelectedGfx)
        sdl_WindowText(win, AltUnselColour, 20, y, format("Tile height is %d.", tile_height));
    button = sdl_ButtonBankGet(&win->buttons, MoreHeightMinus);
    if (SelectedGfx && !tag && !Setup.initialized)
    {
        sdl_ButtonMove(button, 150, y);
        sdl_ButtonVisible(button, true);
    }
    else
        sdl_ButtonVisible(button, false);
    button = sdl_ButtonBankGet(&win->buttons, MoreHeightPlus);
    if (SelectedGfx && !tag && !Setup.initialized)
    {
        sdl_ButtonMove(button, 180, y);
        sdl_ButtonVisible(button, true);
    }
    else
        sdl_ButtonVisible(button, false);
    if (SelectedGfx) y += 20;

    button = sdl_ButtonBankGet(&win->buttons, MoreFullscreen);
    sdl_WindowText(win, AltUnselColour, 20, y, "Fullscreen is:");

    sdl_ButtonMove(button, 150, y);
    y += 20;

    sdl_WindowText(win, AltUnselColour, 20, y, format("Window borders is %d.", window->windowborders));
    button = sdl_ButtonBankGet(&win->buttons, MoreWindowBordersMinus);
    sdl_ButtonMove(button, 150, y);

    button = sdl_ButtonBankGet(&win->buttons, MoreWindowBordersPlus);
    sdl_ButtonMove(button, 180, y);

    y += 20;

    sdl_WindowText(win, AltUnselColour, 20, y, format("Sound Volume is %d.", sound_volume));
    button = sdl_ButtonBankGet(&win->buttons, MoreSoundVolumeMinus);
    sdl_ButtonMove(button, 150, y);

    button = sdl_ButtonBankGet(&win->buttons, MoreSoundVolumePlus);
    sdl_ButtonMove(button, 180, y);

    y += 20;

    sdl_WindowText(win, AltUnselColour, 20, y, format("Music Volume is %d.", music_volume));
    button = sdl_ButtonBankGet(&win->buttons, MoreMusicVolumeMinus);
    sdl_ButtonMove(button, 150, y);

    button = sdl_ButtonBankGet(&win->buttons, MoreMusicVolumePlus);
    sdl_ButtonMove(button, 180, y);

    y += 20;

    sdl_WindowText(win, AltUnselColour, 20, y, format("Snap range is %d.", SnapRange));
    button = sdl_ButtonBankGet(&win->buttons, MoreSnapMinus);
    sdl_ButtonMove(button, 150, y);

    button = sdl_ButtonBankGet(&win->buttons, MoreSnapPlus);
    sdl_ButtonMove(button, 180, y);
}


static int get_gfx_namewidth(void)
{
    int maxl = 0, l;
    graphics_mode *mode = graphics_modes;

    while (mode)
    {
        if (mode->menuname[0])
        {
            l = strlen(mode->menuname);
            if (l > maxl) maxl = l;
        }
        mode = mode->pNext;
    }

    return (maxl * StatusBar.font.width + 20);
}


static void MoreActivate(sdl_Button *sender)
{
    int width = 300;
    int height = 320;
    sdl_Button *button;
    graphics_mode *mode;
    int gfx_namewidth = get_gfx_namewidth();

    sdl_WindowInit(&PopUp, width, height, AppWin, StatusBar.font.req);
    PopUp.left = (AppWin->w / 2) - width / 2;
    PopUp.top = (AppWin->h / 2) - height / 2;
    PopUp.draw_extra = MoreDraw;

    SelectedGfx = use_graphics;

    MoreWidthPlus = sdl_ButtonBankNew(&PopUp.buttons);
    button = sdl_ButtonBankGet(&PopUp.buttons, MoreWidthPlus);

    button->unsel_colour = AltUnselColour;
    button->sel_colour = AltSelColour;
    sdl_ButtonSize(button, 20, PopUp.font.height + 2);
    sdl_ButtonCaption(button, "+");
    button->tag = 1;
    sdl_ButtonVisible(button, SelectedGfx? true: false);
    button->activate = WidthChange;

    MoreWidthMinus = sdl_ButtonBankNew(&PopUp.buttons);
    button = sdl_ButtonBankGet(&PopUp.buttons, MoreWidthMinus);

    button->unsel_colour = AltUnselColour;
    button->sel_colour = AltSelColour;
    sdl_ButtonSize(button, 20, PopUp.font.height + 2);
    sdl_ButtonCaption(button, "-");
    button->tag = -1;
    sdl_ButtonVisible(button, SelectedGfx? true: false);
    button->activate = WidthChange;

    MoreHeightPlus = sdl_ButtonBankNew(&PopUp.buttons);
    button = sdl_ButtonBankGet(&PopUp.buttons, MoreHeightPlus);

    button->unsel_colour = AltUnselColour;
    button->sel_colour = AltSelColour;
    sdl_ButtonSize(button, 20, PopUp.font.height + 2);
    sdl_ButtonCaption(button, "+");
    button->tag = 1;
    sdl_ButtonVisible(button, SelectedGfx? true: false);
    button->activate = HeightChange;

    MoreHeightMinus = sdl_ButtonBankNew(&PopUp.buttons);
    button = sdl_ButtonBankGet(&PopUp.buttons, MoreHeightMinus);

    button->unsel_colour = AltUnselColour;
    button->sel_colour = AltSelColour;
    sdl_ButtonSize(button, 20, PopUp.font.height + 2);
    sdl_ButtonCaption(button, "-");
    button->tag = -1;
    sdl_ButtonVisible(button, SelectedGfx? true: false);
    button->activate = HeightChange;

    MoreNiceGfx = sdl_ButtonBankNew(&PopUp.buttons);
    button = sdl_ButtonBankGet(&PopUp.buttons, MoreNiceGfx);

    button->unsel_colour = AltUnselColour;
    button->sel_colour = AltSelColour;
    sdl_ButtonSize(button, 50, PopUp.font.height + 2);
    sdl_ButtonVisible(button, true);
    sdl_ButtonCaption(button, nicegfx? "On": "Off");
    button->tag = nicegfx;
    button->activate = FlipTag;

    /* Allow only in initial phase */
    if (!Setup.initialized)
    {
        mode = graphics_modes;
        while (mode)
        {
            if (mode->menuname[0])
            {
                GfxButtons[mode->grafID] = sdl_ButtonBankNew(&PopUp.buttons);
                button = sdl_ButtonBankGet(&PopUp.buttons, GfxButtons[mode->grafID]);

                button->unsel_colour = AltUnselColour;
                button->sel_colour = AltSelColour;
                sdl_ButtonSize(button, gfx_namewidth, PopUp.font.height + 2);
                sdl_ButtonVisible(button, true);
                sdl_ButtonCaption(button, mode->menuname);
                button->tag = mode->grafID;
                button->activate = SelectGfx;
            }
            mode = mode->pNext;
        }
    }

    MoreFullscreen = sdl_ButtonBankNew(&PopUp.buttons);
    button = sdl_ButtonBankGet(&PopUp.buttons, MoreFullscreen);

    button->unsel_colour = AltUnselColour;
    button->sel_colour = AltSelColour;
    sdl_ButtonSize(button, 50, PopUp.font.height + 2);
    sdl_ButtonVisible(button, true);
    sdl_ButtonCaption(button, fullscreen? "On": "Off");
    button->tag = fullscreen;
    button->activate = FlipTag;
    
    MoreWindowBordersPlus = sdl_ButtonBankNew(&PopUp.buttons);
    button = sdl_ButtonBankGet(&PopUp.buttons, MoreWindowBordersPlus);

    button->unsel_colour = AltUnselColour;
    button->sel_colour = AltSelColour;
    sdl_ButtonSize(button, 20, PopUp.font.height + 2);
    sdl_ButtonCaption(button, "+");
    button->tag = 1;
    sdl_ButtonVisible(button, true);
    button->activate = WindowBordersChange;

    MoreWindowBordersMinus = sdl_ButtonBankNew(&PopUp.buttons);
    button = sdl_ButtonBankGet(&PopUp.buttons, MoreWindowBordersMinus);

    button->unsel_colour = AltUnselColour;
    button->sel_colour = AltSelColour;
    sdl_ButtonSize(button, 20, PopUp.font.height + 2);
    sdl_ButtonCaption(button, "-");
    button->tag = -1;
    sdl_ButtonVisible(button, true);
    button->activate = WindowBordersChange;

    MoreSoundVolumePlus = sdl_ButtonBankNew(&PopUp.buttons);
    button = sdl_ButtonBankGet(&PopUp.buttons, MoreSoundVolumePlus);

    button->unsel_colour = AltUnselColour;
    button->sel_colour = AltSelColour;
    sdl_ButtonSize(button, 20, PopUp.font.height + 2);
    sdl_ButtonCaption(button, "+");
    button->tag = 5;
    sdl_ButtonVisible(button, true);
    button->activate = SoundVolumeChange;

    MoreSoundVolumeMinus = sdl_ButtonBankNew(&PopUp.buttons);
    button = sdl_ButtonBankGet(&PopUp.buttons, MoreSoundVolumeMinus);

    button->unsel_colour = AltUnselColour;
    button->sel_colour = AltSelColour;
    sdl_ButtonSize(button, 20, PopUp.font.height + 2);
    sdl_ButtonCaption(button, "-");
    button->tag = -5;
    sdl_ButtonVisible(button, true);
    button->activate = SoundVolumeChange;

    MoreMusicVolumePlus = sdl_ButtonBankNew(&PopUp.buttons);
    button = sdl_ButtonBankGet(&PopUp.buttons, MoreMusicVolumePlus);

    button->unsel_colour = AltUnselColour;
    button->sel_colour = AltSelColour;
    sdl_ButtonSize(button, 20, PopUp.font.height + 2);
    sdl_ButtonCaption(button, "+");
    button->tag = 5;
    sdl_ButtonVisible(button, true);
    button->activate = MusicVolumeChange;

    MoreMusicVolumeMinus = sdl_ButtonBankNew(&PopUp.buttons);
    button = sdl_ButtonBankGet(&PopUp.buttons, MoreMusicVolumeMinus);

    button->unsel_colour = AltUnselColour;
    button->sel_colour = AltSelColour;
    sdl_ButtonSize(button, 20, PopUp.font.height + 2);
    sdl_ButtonCaption(button, "-");
    button->tag = -5;
    sdl_ButtonVisible(button, true);
    button->activate = MusicVolumeChange;

    MoreSnapPlus = sdl_ButtonBankNew(&PopUp.buttons);
    button = sdl_ButtonBankGet(&PopUp.buttons, MoreSnapPlus);

    button->unsel_colour = AltUnselColour;
    button->sel_colour = AltSelColour;
    sdl_ButtonSize(button, 20, PopUp.font.height + 2);
    sdl_ButtonCaption(button, "+");
    button->tag = 1;
    sdl_ButtonVisible(button, true);
    button->activate = SnapChange;

    MoreSnapMinus = sdl_ButtonBankNew(&PopUp.buttons);
    button = sdl_ButtonBankGet(&PopUp.buttons, MoreSnapMinus);

    button->unsel_colour = AltUnselColour;
    button->sel_colour = AltSelColour;
    sdl_ButtonSize(button, 20, PopUp.font.height + 2);
    sdl_ButtonCaption(button, "-");
    button->tag = -1;
    sdl_ButtonVisible(button, true);
    button->activate = SnapChange;

    MoreOK = sdl_ButtonBankNew(&PopUp.buttons);
    button = sdl_ButtonBankGet(&PopUp.buttons, MoreOK);

    button->unsel_colour = AltUnselColour;
    button->sel_colour = AltSelColour;
    sdl_ButtonSize(button, 50, PopUp.font.height + 2);
    sdl_ButtonVisible(button, true);
    sdl_ButtonCaption(button, "OK");
    sdl_ButtonMove(button, width / 2 - 25, height - 40);
    button->activate = AcceptChanges;

    popped = true;
}


static errr Term_xtra_sdl_clear(void);


/* Window size bounds checking */
static void check_bounds_resize(term_window *win, int *cols, int *rows, int *max_rows, int *width,
    int *height)
{
    int dummy;

    /* Get the amount of columns & rows */
    *cols = dummy = (*width - (win->border * 2)) / win->tile_wid;
    *rows = *max_rows = (*height - win->border - win->title_height) / win->tile_hgt;

    check_term_resize((win->Term_idx == 0), cols, rows);
    check_term_resize((win->Term_idx == 0), &dummy, max_rows);

    /* Calculate the width & height */
    *width = (*cols * win->tile_wid) + (win->border * 2);
    *height = (*max_rows * win->tile_hgt) + win->border + win->title_height;
}


/*
 * Make a window with size (x,y) pixels
 * Note: The actual size of the window may end up smaller.
 * This may be called when a window wants resizing,
 * is made visible, or the font has changed.
 * This function doesn't go in for heavy optimization, and doesn't need it-
 * it may initialize a few too many redraws or whatnot, but everything gets done!
 */
static void ResizeWin(term_window *win, int w, int h)
{
    /* Don't bother */
    if (!win->visible) return;

    win->border = 2;
    win->title_height = StatusHeight;

    /* No font - a new font is needed -> get dimensions */
    if (!win->font.data)
    {
        /* Get font dimensions */
		if (sdl_CheckFont(&win->req_font, &win->tile_wid, &win->tile_hgt) ||
            win->tile_wid <= 0 || win->tile_hgt <= 0)
        {
			quit_fmt("unable to find font '%s';\n"
				"note that there are new extended font files "
				"ending in 'x' in %s;\n"
				"please check %s and edit if necessary",
				win->req_font.name, ANGBAND_DIR_FONTS,
				sdl_settings_file);
		}

        /* Apply nice graphics */
        if (win->Term_idx == 0)
        {
            apply_nice_graphics(win);
            tile_distorted = is_tile_distorted(use_graphics, tile_width, tile_height);
        }
    }

    /* Initialize the width & height */
    win->width = w;
    win->height = h;

    /* Window size bounds checking */
    check_bounds_resize(win, &win->cols, &win->rows, &win->max_rows, &win->width, &win->height);

    /* Delete the old surface */
    if (win->surface) SDL_FreeSurface(win->surface);

    /* Create a new surface */
    win->surface = SDL_CreateRGBSurface(SDL_SWSURFACE, win->width, win->height,
        AppWin->format->BitsPerPixel, AppWin->format->Rmask, AppWin->format->Gmask,
        AppWin->format->Bmask, AppWin->format->Amask);

    /* Fill it */
    if ((win->windowborders >= 0) && (win->windowborders < BASIC_COLORS))
    {
        SDL_FillRect(win->surface, NULL, SDL_MapRGB(AppWin->format,
            text_colours[win->windowborders].r, text_colours[win->windowborders].g,
            text_colours[win->windowborders].b));
    }
    else
    {
        SDL_FillRect(win->surface, NULL,
            SDL_MapRGB(AppWin->format, AltUnselColour.r, AltUnselColour.g, AltUnselColour.b));
    }

    /* Label it */
    sdl_FontDraw(&SystemFont, win->surface, back_colour, 1, 1,
        strlen(angband_term_name[win->Term_idx]), angband_term_name[win->Term_idx]);

    /* Mark the whole window for redraw */
    sdl_RECT(0, 0, win->width, win->height, &win->uRect);

    /* Create the font if we need to */
    if (!win->font.data)
        sdl_FontCreate(&win->font, &win->req_font, win->surface);

    /* This window was never visible before, or needs resizing */
    if (!angband_term[win->Term_idx])
    {
        term *old = Term;

        /* Initialize the term data */
        term_data_link_sdl(win);

        /* Make it visible to angband */
        angband_term[win->Term_idx] = &win->term_data;

        /* Activate it */
        Term_activate((term*)&win->term_data);

        /* Redraw */
        Term_redraw();

        /* Restore */
        Term_activate(old);
    }
    else
    {
        term *old = Term;

        /* Activate it */
        Term_activate((term*)&win->term_data);

        /* Resize */
        Term_resize(win->cols, win->rows, win->max_rows);

        /* Redraw */
        Term_redraw();

        /* Restore */
        Term_activate(old);
    }

    /* Calculate the hotspot */
    if (win->Term_idx == SelectedTerm) sdl_SizingSpot(win, false, &SizingSpot);

    StatusBar.need_update = true;

    /* Dungeon size */
    if (win->Term_idx == 0)
        net_term_resize(win->cols, win->rows, win->max_rows);

    /* Hack -- redraw all windows */
    if (Setup.initialized) do_cmd_redraw();
}


static errr load_prefs(void)
{
    char buf[MSG_LEN];
    ang_file *fff;

    /* Build the path */
    path_build(buf, sizeof(buf), ANGBAND_DIR_USER, "sdlinit.txt");
    sdl_settings_file = string_make(buf);

    /* Open the file */
    fff = file_open(buf, MODE_READ, FTYPE_TEXT);

    /* Check it */
    if (!fff) return (1);

    /* Process the file */
    while (file_getl(fff, buf, sizeof(buf)))
    {
        char *s;
        if (!buf[0]) continue;

        if (buf[0] == '#') continue;

        s = strchr(buf, '=');
        s++;
        while (!isalnum(*s)) s++;

        if (strstr(buf, "Resolution"))
        {
            screen_w = atoi(s);
            s = strchr(buf, 'x');
            screen_h = atoi(s + 1);
        }
        else if (strstr(buf, "Fullscreen"))
            fullscreen = atoi(s);
        else if (strstr(buf, "SoundVolume"))
            sound_volume = atoi(s);
        else if (strstr(buf, "MusicVolume"))
            music_volume = atoi(s);
        else if (strstr(buf, "DefaultColor"))
        {
            int d_color_r, d_color_g, d_color_b;

            sscanf(s, "%d,%d,%d", &d_color_r, &d_color_g, &d_color_b);
            if ((d_color_r >= 0) && (d_color_r <= 255) && (d_color_g >= 0) && (d_color_g <= 255) &&
                (d_color_b >= 0) && (d_color_b <= 255))
            {
                AltUnselColour.r = d_color_r;
                AltUnselColour.g = d_color_g;
                AltUnselColour.b = d_color_b;
            }
        }
        else if (strstr(buf, "StatusBarColor"))
            statusbar_color = atoi(s);
        else if (strstr(buf, "NiceGraphics"))
            nicegfx = atoi(s);
        else if (strstr(buf, "Graphics"))
            use_graphics = atoi(s);
        else if (strstr(buf, "TileWidth"))
            tile_width = atoi(s);
        else if (strstr(buf, "TileHeight"))
            tile_height = atoi(s);
    }

    if (screen_w < MIN_SCREEN_WIDTH) screen_w = MIN_SCREEN_WIDTH;
    if (screen_h < MIN_SCREEN_HEIGHT) screen_h = MIN_SCREEN_HEIGHT;

    if (sound_volume < 0) sound_volume = 0;
    if (sound_volume > 100) sound_volume = 100;

    if (music_volume < 0) music_volume = 0;
    if (music_volume > 100) music_volume = 100;

    if ((statusbar_color < 0) || (statusbar_color >= BASIC_COLORS)) statusbar_color = 0;

    file_close(fff);

    tile_distorted = is_tile_distorted(use_graphics, tile_width, tile_height);

    return (0);
}


static errr load_window_prefs(void)
{
    char buf[MSG_LEN];
    ang_file *fff;
    term_window *win;
    int i, w, h, b = 2;

    /* Initialize the windows with default values */
    for (i = 0; i < ANGBAND_TERM_MAX; i++)
    {
        win = &windows[i];

        /* Clear the data */
        memset(win, 0, sizeof(term_window));

        /* Who? */
        win->Term_idx = i;

        /* Default font */
        win->req_font.alloc_name = string_make(default_term_font.name);
        win->req_font.name = win->req_font.alloc_name;
		win->req_font.size = default_term_font.size;
		win->req_font.preset = default_term_font.preset;
		win->req_font.bitmapped = default_term_font.bitmapped;

        /* Default window borders */
        win->windowborders = BASIC_COLORS;

        /* Default width & height */
        sdl_CheckFont(&win->req_font, &w, &h);
        win->width = (NORMAL_WID * w) + (b * 2);
        win->height = (NORMAL_HGT * h) + b + StatusHeight;

        /* Default values */
        if (i == 0)
        {
            win->top = StatusHeight;
            win->keys = 1024;
            win->visible = true;
        }
        else
        {
            win->top = windows[0].top + windows[0].height + (i * 10);
            win->left = (i - 1) * 10;
            win->keys = 32;
            win->visible = false;
        }
    }

    /* Open the file */
    fff = file_open(sdl_settings_file, MODE_READ, FTYPE_TEXT);

    /* Check it */
    if (!fff) return (1);

    /* Process the file */
    while (file_getl(fff, buf, sizeof(buf)))
    {
        char *s;
        if (!buf[0]) continue;

        if (buf[0] == '#') continue;

        s = strchr(buf, '=');
        s++;
        while (!isalnum(*s)) s++;

        if (strstr(buf, "Window"))
            win = &windows[atoi(s)];
        else if (strstr(buf, "Visible"))
            win->visible = atoi(s);
        else if (strstr(buf, "Left"))
            win->left = atoi(s);
        else if (strstr(buf, "Top"))
            win->top = atoi(s);
        else if (strstr(buf, "Width"))
            win->width = atoi(s);
        else if (strstr(buf, "Height"))
            win->height = atoi(s);
        else if (strstr(buf, "Keys"))
            win->keys = atoi(s);
        else if (strstr(buf, "WinBorders"))
            win->windowborders = atoi(s);
        else if (strstr(buf, "Font"))
        {
            const char *garbled_msg = "garbled font entry in pref "
				"file; use the default fault instead\n";
			long fsz;
			char *se;
			int w, h;

			string_free(win->req_font.alloc_name);
			if (prefix(s, "NOTPRESET,"))
            {
				win->req_font.preset = false;
				fsz = strtol(s + 10, &se, 10);
				if (*se == ',')
                {
					if (!fsz && (fsz < MIN_POINT_SIZE || fsz > MAX_POINT_SIZE))
                    {
						fprintf(stderr,
							"invalid point size, "
							"%ld, in pref file; "
							"use the default size "
							"instead\n", fsz);
						fsz = DEFAULT_POINT_SIZE;
					}
					win->req_font.alloc_name = string_make(se + 1);
				}
                else
                {
					fprintf(stderr, "%s",
						garbled_msg);
					win->req_font.preset =
						default_term_font.preset;
					win->req_font.alloc_name = string_make(
						default_term_font.name);
					fsz = (default_term_font.bitmapped) ?
						0 : default_term_font.size;
				}
			}
            else
            {
				win->req_font.preset = true;
				if (prefix(s, "NOTBITMAP,"))
                {
					fsz = strtol(s + 10, &se, 10);
					if (*se == ',')
                    {
						if (fsz < MIN_POINT_SIZE || fsz > MAX_POINT_SIZE)
							fsz = DEFAULT_POINT_SIZE;
						win->req_font.alloc_name =
							string_make(se + 1);
					}
                    else
                    {
						fprintf(stderr, "%s",
							garbled_msg);
						win->req_font.preset =
							default_term_font.preset;
						win->req_font.alloc_name = string_make(
							default_term_font.name);
						fsz = (default_term_font.bitmapped) ?
							0 : default_term_font.size;
					}
				}
                else
                {
					win->req_font.name = string_make(s);
					fsz = 0;
				}
			}
            win->req_font.name = win->req_font.alloc_name;
			win->req_font.size = fsz;
			win->req_font.bitmapped = (fsz == 0);
			if (sdl_CheckFont(&win->req_font, &w, &h))
            {
				if (streq(win->req_font.name,
						default_term_font.name))
                {
					quit_fmt("could not load the default "
						"font, %s", win->req_font.name);
				}
				fprintf(stderr, "unusable font "
					"file, %s, from pref file; using the "
					"default font\n", win->req_font.name);
				string_free(win->req_font.alloc_name);
				win->req_font.alloc_name = string_make(
					default_term_font.name);
				win->req_font.name = win->req_font.alloc_name;
                win->req_font.size = default_term_font.size;
				win->req_font.preset = default_term_font.preset;
				win->req_font.bitmapped =
					default_term_font.bitmapped;
			}
        }
    }

    file_close(fff);

    return (0);
}


static errr save_prefs(void)
{
    ang_file *fff;
    int i;

    /* Open the file */
    fff = file_open(sdl_settings_file, MODE_WRITE, FTYPE_TEXT);

    /* Check it */
    if (!fff) return (1);

    file_putf(fff, "Resolution = %dx%d\n", screen_w, screen_h);
    file_putf(fff, "Fullscreen = %d\n", fullscreen);
    file_putf(fff, "SoundVolume = %d\n", sound_volume);
    file_putf(fff, "MusicVolume = %d\n", music_volume);
    file_putf(fff, "DefaultColor = %d,%d,%d\n", AltUnselColour.r, AltUnselColour.g, AltUnselColour.b);
    file_putf(fff, "StatusBarColor = %d\n", statusbar_color);
    file_putf(fff, "NiceGraphics = %d\n", nicegfx);
    file_putf(fff, "Graphics = %d\n", use_graphics);
    file_putf(fff, "TileWidth = %d\n", tile_width);
    file_putf(fff, "TileHeight = %d\n", tile_height);

    for (i = 0; i < ANGBAND_TERM_MAX; i++)
    {
        term_window *win = &windows[i];

        file_putf(fff, "\nWindow = %d\n", i);
        file_putf(fff, "Visible = %d\n", (int)win->visible);
        file_putf(fff, "Left = %d\n", win->left);
        file_putf(fff, "Top = %d\n", win->top);
        file_putf(fff, "Width = %d\n", win->width);
        file_putf(fff, "Height = %d\n", win->height);
        file_putf(fff, "Keys = %d\n", win->keys);
        file_putf(fff, "WinBorders = %d\n", win->windowborders);
        if (win->req_font.bitmapped)
        {
			file_putf(fff, "Font = %s%s\n\n",
				(win->req_font.preset) ? "" : "NOTPRESET,0,",
				win->req_font.name);
		}
        else
        {
			assert(win->req_font.size >= MIN_POINT_SIZE &&
				win->req_font.size <= MAX_POINT_SIZE);
			file_putf(fff, "Font = %s,%d,%s\n\n",
				(win->req_font.preset) ?
				"NOTBITMAP" : "NOTPRESET",
				win->req_font.size, win->req_font.name);
		}
    }   

    file_close(fff);

    /* Done */
    return (0);
}


static void set_update_rect(term_window *win, SDL_Rect *rc);


static void DrawSizeWidget(void)
{
    Uint32 colour = SDL_MapRGB(AppWin->format, 30, 160, 70);

    SDL_FillRect(AppWin, &SizingSpot, colour);
    SDL_UpdateRects(AppWin, 1, &SizingSpot);
}


static int Movingx;
static int Movingy;


/*
 * Is What within Range units of Origin
 */
#define closeto(Origin, What, Range) \
    ((ABS((Origin) - (What))) < (Range))


/*
 * This function keeps the 'mouse' info up to date,
 * and reacts to mouse buttons appropriately.
 */
static void sdl_HandleMouseEvent(SDL_Event *event)
{
    term_window *win;

    switch (event->type)
    {
        /* Mouse moved */
        case SDL_MOUSEMOTION:
        {
            mouse.x = event->motion.x;
            mouse.y = event->motion.y;
            win = &windows[SelectedTerm];

            /* We are moving or resizing a window */
            if (Moving)
            {
                int i;

                /* Move the window */
                win->left = (mouse.x - Movingx);
                win->top = (mouse.y - Movingy);

                /* Left bounds check */
                if (win->left < 0)
                {
                    win->left = 0;
                    Movingx = mouse.x;
                }

                /* Right bounds check */
                if ((win->left + win->width) > AppWin->w)
                {
                    win->left = AppWin->w - win->width;
                    Movingx = mouse.x - win->left;
                }

                /* Top bounds check */
                if (win->top < StatusHeight)
                {
                    win->top = StatusHeight;
                    Movingy = mouse.y - win->top;
                }

                /* Bottom bounds check */
                if ((win->top + win->height) > AppWin->h)
                {
                    win->top = AppWin->h - win->height;
                    Movingy = mouse.y - win->top;
                }

                for (i = 0; i < ANGBAND_TERM_MAX; i++)
                {
                    term_window *snapper = &windows[i];

                    /* Can't snap to self... */
                    if (i == SelectedTerm) continue;

                    /* Can't snap to the invisible */
                    if (!snapper->visible) continue;

                    /* Check the windows are across from each other */
                    if ((snapper->top < win->top + win->height) &&
                        (win->top < snapper->top + snapper->height))
                    {
                        /* Lets try to the left... */
                        if (closeto(win->left, snapper->left + snapper->width, SnapRange))
                        {
                            win->left = snapper->left + snapper->width;
                            Movingx = mouse.x - win->left;
                        }

                        /* Maybe to the right */
                        if (closeto(win->left + win->width, snapper->left, SnapRange))
                        {
                            win->left = snapper->left - win->width;
                            Movingx = mouse.x - win->left;
                        }
                    }
                    
                    /* Check the windows are above/below each other */
                    if ((snapper->left < win->left + win->width) &&
                        (win->left < snapper->left + snapper->width))
                    {
                        /* Lets try to the top... */
                        if (closeto(win->top, snapper->top + snapper->height, SnapRange))
                        {
                            win->top = snapper->top + snapper->height;
                            Movingy = mouse.y - win->top;
                        }

                        /* Maybe to the bottom */
                        if (closeto(win->top + win->height, snapper->top, SnapRange))
                        {
                            win->top = snapper->top - win->height;
                            Movingy = mouse.y - win->top;
                        }
                    }
                }

                /* Show on the screen */
                sdl_BlitAll();
            }
            else if (Sizing)
            {
                int dummy_cols, dummy_rows, dummy_max_rows;
                int rect_width, rect_height;

                /* Calculate the dimensions of the sizing rectangle */
                rect_width = win->width - win->left + (mouse.x - Movingx);
                rect_height = win->height - win->top + (mouse.y - Movingy);

                /* Window size bounds checking */
                check_bounds_resize(win, &dummy_cols, &dummy_rows, &dummy_max_rows, &rect_width,
                    &rect_height);

                /* Adjust the sizing rectangle */
                SizingRect.w = rect_width;
                SizingRect.h = rect_height;

                /* Show on the screen */                
                sdl_BlitAll();
            }

            else if (!popped)
            {
                /* Have a look for the corner stuff */
                if (point_in(&SizingSpot, mouse.x, mouse.y))
                {
                    if (!Sizingshow)
                    {
                        /* Indicate the hotspot */
                        Sizingshow = true;
                        DrawSizeWidget();
                    }
                }

                /* Remove the hotspot */
                else if (Sizingshow)
                {
                    SDL_Rect rc;

                    Sizingshow = false;
                    sdl_SizingSpot(win, true, &rc);
                    set_update_rect(win, &rc);
                    sdl_BlitWin(win);
                }
            }

            break;
        }
            
        /* A button has been pressed */
        case SDL_MOUSEBUTTONDOWN:
        {
            sdl_Window *window;
            bool res;
            int idx = sdl_LocateWin(mouse.x, mouse.y);
            
            if (event->button.button == SDL_BUTTON_LEFT)
            {
                mouse.left = 1;
                mouse.leftx = event->button.x;
                mouse.lefty = event->button.y;

                /* Pop up window gets priority */
                if (popped) window = &PopUp; else window = &StatusBar;

                /* React to a button press */
                res = sdl_ButtonBankMouseDown(&window->buttons,
                    mouse.x - window->left, mouse.y - window->top);

                // Hack -- weather
                if (popped && res && player->weather_type != 0)
                    player->weather_type = 0;

                /* If pop-up window active and no reaction, cancel the popup */
                if (popped && !res)
                {
                    RemovePopUp();
                    break;
                }

                /* Has this mouse press been handled */
                if (res) break;

                /* Is the mouse press in a term_window? */
                if (idx < 0) break;

                /* The 'focused' window has changed */
                if (idx != SelectedTerm) TermFocus(idx);

                /* A button press has happened on the focused term window */
                win = &windows[idx];

                /* Check for mouse press in the title bar... */
                if (mouse.y < win->top + win->title_height)
                {
                    /* Let's get moving */
                    Moving = true;

                    /* Remember where we started */
                    Movingx = mouse.x - win->left;
                    Movingy = mouse.y - win->top;
                }

                /* ...or the little hotspot in the bottom right corner... */
                else if (point_in(&SizingSpot, mouse.x, mouse.y))
                {
                    /* Let's get sizing */
                    Sizing = true;

                    /* Create the sizing rectangle */
                    sdl_RECT(win->left, win->top, win->width, win->height, &SizingRect);

                    /* Remember where we started */
                    Movingx = mouse.x - win->left;
                    Movingy = mouse.y - win->top;
                }
            }
            else if (event->button.button == SDL_BUTTON_RIGHT)
            {
                mouse.right = 1;
                mouse.rightx = event->button.x;
                mouse.righty = event->button.y;

                /* Right-click always cancels the popup */
                if (popped) popped = false;
            }

            break;
        }

        case SDL_MOUSEBUTTONUP:
        {
            /* Handle release of left button */
            if (event->button.button == SDL_BUTTON_LEFT)
            {
                sdl_Window *window;
                bool res;
                mouse.left = 0;

                /* Pop up window gets priority */
                if (popped) window = &PopUp; else window = &StatusBar;

                /* React to a button release */
                res = sdl_ButtonBankMouseUp(&window->buttons, mouse.x - window->left,
                    mouse.y - window->top);

                // Hack -- weather
                if (popped && res && player->weather_type != 0)
                    player->weather_type = 0;

                /* Cancel popup */
                if (popped && !res) RemovePopUp();

                /* Finish moving */
                if (Moving)
                {
                    Moving = false;

                    /* Update */
                    sdl_BlitAll();
                }

                /* Finish sizing */
                if (Sizing)
                {
                    /* Sort out the window */
                    ResizeWin(&windows[SelectedTerm], SizingRect.w, SizingRect.h);
                    Sizing = false;
                    Sizingshow = false;

                    /* Update */
                    sdl_BlitAll();
                }
            }
            else if (event->button.button == SDL_BUTTON_RIGHT)
                mouse.right = 0;

            break;
        }
    }
}


/*
 * Handle keypresses.
 *
 * We treat left and right modifier keys as equivalent.
 * We ignore any key without a valid SDLK index.
 */
static void sdl_keypress(SDL_keysym keysym)
{
    uint16_t key_code = keysym.unicode;
    SDLKey key_sym = keysym.sym;
    int ch = 0;

    /* Store the value of various modifier keys */
    bool mc = ((keysym.mod & KMOD_CTRL) > 0);
	bool ms = ((keysym.mod & KMOD_SHIFT) > 0);
	bool ma = ((keysym.mod & KMOD_ALT) > 0);
	bool mm = ((keysym.mod & KMOD_META) > 0);
    /*bool mg = ((keysym.mod & KMOD_MODE) > 0);*/
    bool kp = false;
    uint8_t mods = ((ma? KC_MOD_ALT: 0) | (mm? KC_MOD_META: 0));

    /* Hack -- for keyboards with Alt-Gr translated into KMOD_RALT | KMOD_LCTRL */
    /*if ((keysym.mod & KMOD_RALT) && (keysym.mod & KMOD_LCTRL)) mg = true;*/

    /* Ignore if main term is not initialized */
    if (!Term) return;

    /* Handle all other valid SDL keys */
    switch (key_sym)
    {
        /* Keypad */
        case SDLK_KP0: ch = '0'; kp = true; break;
        case SDLK_KP1: ch = '1'; kp = true; break;
        case SDLK_KP2: ch = '2'; kp = true; break;
        case SDLK_KP3: ch = '3'; kp = true; break;
        case SDLK_KP4: ch = '4'; kp = true; break;
        case SDLK_KP5: ch = '5'; kp = true; break;
        case SDLK_KP6: ch = '6'; kp = true; break;
        case SDLK_KP7: ch = '7'; kp = true; break;
        case SDLK_KP8: ch = '8'; kp = true; break;
        case SDLK_KP9: ch = '9'; kp = true; break;
        case SDLK_KP_PERIOD: ch = '.'; kp = true; break;
        case SDLK_KP_DIVIDE: ch = '/'; kp = true; break;
        case SDLK_KP_MULTIPLY: ch = '*'; kp = true; break;
        case SDLK_KP_MINUS: ch = '-'; kp = true; break;
        case SDLK_KP_PLUS: ch = '+'; kp = true; break;
        case SDLK_KP_ENTER: ch = KC_ENTER; kp = true; break;
        case SDLK_KP_EQUALS: ch = '='; kp = true; break;

        /* Have these to get consistent ctrl-shift behaviour */
        /*case SDLK_0: if ((!ms || mc || ma) && !mg) ch = '0'; break;
        case SDLK_1: if ((!ms || mc || ma) && !mg) ch = '1'; break;
        case SDLK_2: if ((!ms || mc || ma) && !mg) ch = '2'; break;
        case SDLK_3: if ((!ms || mc || ma) && !mg) ch = '3'; break;
        case SDLK_4: if ((!ms || mc || ma) && !mg) ch = '4'; break;
        case SDLK_5: if ((!ms || mc || ma) && !mg) ch = '5'; break;
        case SDLK_6: if ((!ms || mc || ma) && !mg) ch = '6'; break;
        case SDLK_7: if ((!ms || mc || ma) && !mg) ch = '7'; break;
        case SDLK_8: if ((!ms || mc || ma) && !mg) ch = '8'; break;
        case SDLK_9: if ((!ms || mc || ma) && !mg) ch = '9'; break;*/

        case SDLK_UP: ch = ARROW_UP; break;
        case SDLK_DOWN: ch = ARROW_DOWN; break;
        case SDLK_RIGHT: ch = ARROW_RIGHT; break;
        case SDLK_LEFT: ch = ARROW_LEFT; break;

        case SDLK_INSERT: ch = KC_INSERT; break;
        case SDLK_HOME: ch = KC_HOME; break;
        case SDLK_PAGEUP: ch = KC_PGUP; break;
        case SDLK_DELETE: ch = KC_DELETE; break;
        case SDLK_END: ch = KC_END; break;
        case SDLK_PAGEDOWN: ch = KC_PGDOWN; break;
        case SDLK_ESCAPE: ch = ESCAPE; break;
        case SDLK_BACKSPACE: ch = KC_BACKSPACE; break;
        case SDLK_RETURN: ch = KC_ENTER; break;
        case SDLK_TAB: ch = KC_TAB; break;

        case SDLK_F1: ch = KC_F1; break;
        case SDLK_F2: ch = KC_F2; break;
        case SDLK_F3: ch = KC_F3; break;
        case SDLK_F4: ch = KC_F4; break;
        case SDLK_F5: ch = KC_F5; break;
        case SDLK_F6: ch = KC_F6; break;
        case SDLK_F7: ch = KC_F7; break;
        case SDLK_F8: ch = KC_F8; break;
        case SDLK_F9: ch = KC_F9; break;
        case SDLK_F10: ch = KC_F10; break;
        case SDLK_F11: ch = KC_F11; break;
        case SDLK_F12: ch = KC_F12; break;
        case SDLK_F13: ch = KC_F13; break;
        case SDLK_F14: ch = KC_F14; break;
        case SDLK_F15: ch = KC_F15; break;

        default: break;
    }

    if (ch)
    {
        if (kp) mods |= KC_MOD_KEYPAD;
        if (mc) mods |= KC_MOD_CONTROL;
        if (ms) mods |= KC_MOD_SHIFT;
        Term_keypress(ch, mods);
    }
    else if (key_code)
    {
        /* If the keycode is 7-bit ASCII (except numberpad) send directly to the game */
        if (mc && ((key_sym == SDLK_TAB) || (key_sym == SDLK_RETURN) ||
            (key_sym == SDLK_BACKSPACE) || MODS_INCLUDE_CONTROL(key_code)))
        {
            mods |= KC_MOD_CONTROL;
        }
        if (ms && MODS_INCLUDE_SHIFT(key_code)) mods |= KC_MOD_SHIFT;

        Term_keypress(key_code, mods);
    }
}


static void init_windows(void);
static void init_morewindows(void);


/*
 * Handle a single message sent to the application.
 *
 * Functions that are either called from a separate thread or which need to
 * create a separate thread (such as sounds) need to pass messages to this
 * function in order to execute most operations.  See the useage of
 * "SDL_USEREVENT".
 */
static errr sdl_HandleEvent(SDL_Event *event)
{
    /* Handle message */
    switch (event->type)
    {
        /* Keypresses */
        case SDL_KEYDOWN:
        {
            /* Handle keypress */
            sdl_keypress(event->key.keysym);
            break;
        }
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
        {
            /* Handle mouse stuff */
            sdl_HandleMouseEvent(event);
            break;
        }

        case SDL_MOUSEMOTION:
        {
            int i;
            SDL_Event events[10];

            /*
             * If there are a bundle of mouse movements pending,
             * we'll just take every tenth one - this makes a
             * simple approach to dragging practical, for instance.
             */  
            i = SDL_PeepEvents(events, 10, SDL_GETEVENT,
                SDL_EVENTMASK(SDL_MOUSEMOTION));
            if (i > 0) *event = events[i - 1];

            /* Handle mouse stuff */
            sdl_HandleMouseEvent(event);
            break;
        }
            
        /* Shut down the game */
        /* XXX - check for stuck inside menu etc... */
        case SDL_QUIT:
        {
            save_prefs();

            quit(NULL);
            break;
        }
            
        /* Resize the application */
        case SDL_VIDEORESIZE:
        {
            /* Free the surface */
            SDL_FreeSurface(AppWin);

            if (!fullscreen)
            {
                /* Make sure */
                vflags &= ~(SDL_FULLSCREEN);
                vflags |= SDL_RESIZABLE;

                screen_w = event->resize.w;
                screen_h = event->resize.h;

                if (screen_w < MIN_SCREEN_WIDTH) screen_w = MIN_SCREEN_WIDTH;
                if (screen_h < MIN_SCREEN_HEIGHT) screen_h = MIN_SCREEN_HEIGHT;

                /* Resize the application surface */
                AppWin = SDL_SetVideoMode(screen_w, screen_h, 0, vflags);
            }
            else
            {
                /* Make sure */
                vflags |= SDL_FULLSCREEN;
                vflags &= ~(SDL_RESIZABLE);

                AppWin = SDL_SetVideoMode(full_w, full_h, 0, vflags);
            }
            init_windows();
            init_morewindows();
            sdl_BlitAll();

            break;
        }
            
        case WINDOW_DRAW:
        {
            /* Redraw window that have asked */
            sdl_Window *window = (sdl_Window*)event->user.data1;
            sdl_WindowBlit(window);
            break;
        }

        default:
        {
            /* Do nothing */
            break;
        }
    }   
    sdl_WindowUpdate(&StatusBar);
    sdl_WindowUpdate(&PopUp);
    return (0);
}


/*
 * Update the redraw rect
 * A simple but effective way to keep track of what
 * parts of a window need to updated.
 * Any new areas that are updated before a blit are simply combined
 * into a new larger rectangle to encompass all changes.
 */
static void set_update_rect(term_window *win, SDL_Rect *rc)
{
    /* No outstanding update areas yet? */
    if (win->uRect.x == -1)
    {
        /* Simple copy */
        win->uRect = *rc;
    }
    else
    {
        /* Combine the old update area with the new */
        int x = MIN(win->uRect.x, rc->x);
        int y = MIN(win->uRect.y, rc->y);
        int x2 = MAX(win->uRect.x + win->uRect.w, rc->x + rc->w);
        int y2 = MAX(win->uRect.y + win->uRect.h, rc->y + rc->h);
        sdl_RECT(x, y, x2 - x, y2 - y, &win->uRect);
    }
}


/*
 * Clear a terminal window
 */
static errr Term_xtra_sdl_clear(void)
{
    term_window *win = (term_window*)(Term->data);
    SDL_Rect rc;

    /* Oops */
    if (!win->surface) return (1);

    /* Create the fill area */
    sdl_RECT(win->border, win->title_height, win->width - (2 * win->border),
         win->height - win->border - win->title_height, &rc);

    /* Fill the rectangle */
    SDL_FillRect(win->surface, &rc, back_pixel_colour);

    /* Rectangle to update */
    set_update_rect(win, &rc);

    /* Success */
    return (0);
}


/*
 * Process at least one event
 */
static errr Term_xtra_sdl_event(int v)
{
    SDL_Event event;
    errr error = 0;

    /* Wait or check for an event */
    if (v)
    {
        /* Wait for an event */
        if (SDL_WaitEvent(&event))
        {
            /* Handle it */
            error = sdl_HandleEvent(&event);
        }
        else return (1);
    }
    else
    {
        /* Get a single pending event */
        if (SDL_PollEvent(&event))
        {
            /* Handle it */
            error = sdl_HandleEvent(&event);
        }
    }

    /* Note success or failure */
    return (error);
}


/*
 * Process all pending events
 */
static errr Term_xtra_sdl_flush(void)
{
    SDL_Event event;

    /* Get all pending events */
    while (SDL_PollEvent(&event))
    {
        /* Handle them (ignore errors) */
        sdl_HandleEvent(&event);
    }

    /* Done */
    return (0);
}


/*
 * Delay for "x" milliseconds
 */
static errr Term_xtra_sdl_delay(int v)
{
    /* Sleep */
    if (v > 0)
    {
        Term_xtra_sdl_event(0);
        SDL_Delay(v);
    }

    /* Success */
    return (0);
}


static errr get_sdl_rect(term_window *win, int col, int row, bool translate, SDL_Rect *prc)
{
    /* Make the destination rectangle */
    sdl_RECT(col * win->tile_wid, row * win->tile_hgt, win->tile_wid, win->tile_hgt, prc);

    /* Stretch for bigtile mode */
    if (!Term->minimap_active)
    {
	    prc->w *= tile_width;
	    prc->h *= tile_height;
    }

    /* Translate it */
    prc->x += win->border;
    prc->y += win->title_height;

    /* Success */
    return (0);
}


/*
 * Displays a cursor
 */
static errr Term_curs_sdl_aux(int col, int row, SDL_Color colour)
{
    term_window *win = (term_window*)(Term->data);
    SDL_Rect rc;

    /* Make a rectangle */
    sdl_RECT(col * win->tile_wid, row * win->tile_hgt, win->tile_wid, win->tile_hgt, &rc);

    /* Translate it */
    rc.x += win->border;
    rc.y += win->title_height;

    /* Paranoia */
    if (rc.y > win->height) return (-1);

    /* Draw it */
    sdl_DrawBox(win->surface, &rc, colour, 1);

    /* Update area */
    set_update_rect(win, &rc);

    /* Success */
    return (0);
}


/*
 * Displays the "normal" cursor
 */
static errr Term_curs_sdl(int col, int row)
{
    return Term_curs_sdl_aux(col, row, text_colours[COLOUR_YELLOW]);
}


/*
 * Displays the "big" cursor
 */
static errr Term_bigcurs_sdl(int col, int row)
{
    term_window *win = (term_window*)(Term->data);
    SDL_Color colour = text_colours[COLOUR_YELLOW];
    SDL_Rect rc;
    /*uint16_t a, ta;
    char c, tc;
    int j = 0;*/

    if (Term->minimap_active)
    {
        /* Normal cursor in map window */
        Term_curs_sdl(col, row);
        return 0;
    }

    /* Make a rectangle */
    if (get_sdl_rect(win, col, row, true, &rc)) return (1);

    /* If we are using overdraw, draw a double height cursor (disabled for now) */
    /*if (!Term_info(col, row, &a, &c, &ta, &tc)) j = (a & 0x7F);
    if (overdraw && (j > ROW_MAP + 1) && (j >= overdraw) && (j <= overdraw_max))
    {
        rc.y -= rc.h;
        rc.h = (rc.h << 1);
    }*/

    /* Draw it */
    sdl_DrawBox(win->surface, &rc, colour, 1);

    /* Update area */
    set_update_rect(win, &rc);

    /* Success */
    return (0);
}


static errr Term_xtra_sdl(int n, int v)
{
    switch (n)
    {
        /* Process an event */
        case TERM_XTRA_EVENT:
            return (Term_xtra_sdl_event(v));

        /* Flush all events */
        case TERM_XTRA_FLUSH:
            return (Term_xtra_sdl_flush());

        /* Clear the screen */
        case TERM_XTRA_CLEAR:
            return (Term_xtra_sdl_clear());

        /* Show or hide the cursor */
        case TERM_XTRA_SHAPE:
        {
            int x, y;

            /* Obtain the cursor */
            Term_locate(&x, &y);

            /* Show or hide the cursor */
            Term_curs_sdl(x, y);
            return (0);
        }

        case TERM_XTRA_FRESH:
        {
            /* Get the current window data */
            term_window *win = (term_window*)(Term->data);

            /* Blat it! */
            sdl_BlitWin(win);

            /* Done */
            return (0);
        }

        case TERM_XTRA_DELAY:
            return (Term_xtra_sdl_delay(v));

        case TERM_XTRA_REACT:
        {
            int i;

            /* Re-initialize the colours */
            back_colour.r = angband_color_table[COLOUR_DARK][1];
            back_colour.g = angband_color_table[COLOUR_DARK][2];
            back_colour.b = angband_color_table[COLOUR_DARK][3];
            back_pixel_colour = SDL_MapRGB(AppWin->format, back_colour.r, back_colour.g,
                back_colour.b);
            for (i = 0; i < MAX_COLORS; i++)
            {
                text_colours[i].r = angband_color_table[i][1];
                text_colours[i].g = angband_color_table[i][2];
                text_colours[i].b = angband_color_table[i][3];
            }

            if (use_graphics != v)
            {
                use_graphics = v;
                if (!use_graphics) reset_tile_params();
                load_gfx();
                reset_visuals(true);

                /* Redraw */
                if (Setup.initialized)
                    do_cmd_redraw();

                /* Apply nice graphics */
                else
                {
                    apply_nice_graphics(&windows[0]);
                    tile_distorted = is_tile_distorted(use_graphics, tile_width, tile_height);
                }
            }
        }
    }

    return (1);
}


static errr Term_wipe_sdl(int col, int row, int n)
{
    term_window *win = (term_window*)(Term->data);
    SDL_Rect rc;

    /* Build the area to black out */
    rc.x = col * win->tile_wid;
    rc.y = row * win->tile_hgt;
    rc.w = win->tile_wid * n;
    rc.h = win->tile_hgt;

    /* Translate it */
    rc.x += win->border;
    rc.y += win->title_height;

    /* Paranoia */
    if (rc.y > win->height) return (-1);

    /* Wipe it */
    SDL_FillRect(win->surface, &rc, back_pixel_colour);

    /* Update */
    set_update_rect(win, &rc);

    return (0);
}


/*
 * Do a 'stretched blit'
 * SDL has no support for stretching... What a bastard!
 */
static void sdl_StretchBlit(SDL_Surface *src, SDL_Rect *srcRect, SDL_Surface *dest,
    SDL_Rect *destRect)
{
    int x, y;
    int sx, sy, dx, dy;
    Uint8 *ps, *pd;

    for (y = 0; y < destRect->h; y++)
    {
        for (x = 0; x < destRect->w; x++)
        {
            /* Actual source coords */
            sx = (srcRect->w * x / (destRect->w)) + srcRect->x;
            sy = (srcRect->h * y / (destRect->h)) + srcRect->y;

            /* Find a source pixel */
            ps = (Uint8 *)src->pixels + (sx * src->format->BytesPerPixel) + (sy * src->pitch);

            /* Actual destination pixel coords */
            dx = x + destRect->x;
            dy = y + destRect->y;

            /* Destination pixel */
            pd = (Uint8 *)dest->pixels + (dx * dest->format->BytesPerPixel) + (dy * dest->pitch);

            switch (dest->format->BytesPerPixel)
            {
                case 1:
                {
                    *pd = *ps;
                    break;
                }
                case 2:
                {
                    Uint16 *ps16 = (Uint16*) ps;
                    Uint16 *pd16 = (Uint16*) pd;
                    *pd16 = *ps16;
                    break;
                }
                case 3:
                case 4:
                {
                    Uint32 *ps32 = (Uint32*) ps;
                    Uint32 *pd32 = (Uint32*) pd;
                    *pd32 = *ps32;
                }
            }
        }
    }
}


/*
 * Make the 'pre-stretched' tiles for this window
 * Assumes the tiles surface was freed elsewhere
 */
static errr sdl_BuildTileset(term_window *win)
{
    int x, y;
    int ta, td;
    int xx, yy;
    graphics_mode *info;
    int dwid, dhgt;
    SDL_Surface *surface = GfxSurface;

    info = get_graphics_mode(use_graphics, true);
    if (!(info && info->grafID)) return (1);

    if (!surface) return (1);

    if (Term->minimap_active)
    {
        dwid = win->tile_wid;
        dhgt = win->tile_hgt;
    }
    else
    {
        dwid = win->tile_wid * tile_width;
        dhgt = win->tile_hgt * tile_height;
    }

    /* Calculate the number of tiles across & down */
    ta = surface->w / info->cell_width;
    td = surface->h / info->cell_height;

    /* Calculate the size of the new surface */
    x = ta * dwid;
    y = td * dhgt;

    /* Make it */
    win->tiles = SDL_CreateRGBSurface(SDL_SWSURFACE, x, y, surface->format->BitsPerPixel,
        surface->format->Rmask, surface->format->Gmask, surface->format->Bmask,
        surface->format->Amask);

    /* Bugger */
    if (!win->tiles) return (1);

    /* For every tile... */
    for (xx = 0; xx < ta; xx++)
    {
        for (yy = 0; yy < td; yy++)
        {
            SDL_Rect src, dest;

            /* Source rectangle (on surface) */
            sdl_RECT(xx * info->cell_width, yy * info->cell_height, info->cell_width,
                info->cell_height, &src);

            /* Destination rectangle (win->tiles) */
			sdl_RECT(xx * dwid, yy * dhgt, dwid, dhgt, &dest);

            /* Do the stretch thing */
            sdl_StretchBlit(surface, &src, win->tiles, &dest);
        }
    }

    return (0);
}


/*
 * Draw a tile, given its position and dimensions.
 *
 * If "prc" is not null, only draw the portion of the tile inside that rectangle. We suppose here
 * that the two rectangles "rc" and "prc" are overlapping.
 */
static void sdl_DrawTile(term_window *win, int col, int row, SDL_Rect rc, SDL_Rect *prc, uint16_t a,
    char c, bool background)
{
    int j = (a & 0x7F);
    SDL_Rect src;

    /* Get the dimensions of the graphic surface */
    src.w = rc.w;
    src.h = rc.h;

    /* Default background to darkness */
    src.x = 0;
    src.y = 0;

    //// Slash fx ////
    if (sfx_effect && !background) slashfx_move(&rc.x, &rc.y);

    /* Use the terrain picture only if mapped */
    if ((a & 0x80) || !background)
    {
        src.x = (c & 0x7F) * src.w;
        src.y = j * src.h;
    }

    /* If we are using overdraw, draw the top rectangle */
    if (overdraw && (row > ROW_MAP + 1) && (j >= overdraw) && (j <= overdraw_max))
    {
        /* Double the height */
        src.y -= rc.h;
        rc.y -= rc.h;
        rc.h = (rc.h << 1);
        src.h = rc.h;
        SDL_BlitSurface(win->tiles, &src, win->surface, &rc);
    }

    /* Draw a portion of the tile */
    else if (prc)
    {
        int dx = prc->x - rc.x;
        int dy = prc->y - rc.y;

        if (dx > 0)
        {
            src.x += dx;
            rc.x = prc->x;
        }
        rc.w -= abs(dx);
        src.w = rc.w;
        if (dy > 0)
        {
            src.y += dy;
            rc.y = prc->y;
        }
        rc.h -= abs(dy);
        src.h = rc.h;
        SDL_BlitSurface(win->tiles, &src, win->surface, &rc);
    }

    /* Draw the tile */
    else
        SDL_BlitSurface(win->tiles, &src, win->surface, &rc);
}


/*
 * Draw foreground and background tiles, given their position and dimensions.
 *
 * If "prc" is not null, only draw the portion of the tiles inside that rectangle. We suppose here
 * that the two rectangles "rc" and "prc" are overlapping.
 */
static void sdl_DrawTiles(term_window *win, int col, int row, SDL_Rect rc, SDL_Rect *prc, uint16_t a,
    char c, uint16_t ta, char tc)
{
    /* Draw the terrain tile */
    sdl_DrawTile(win, col, row, rc, prc, ta, tc, true);

    /* If foreground is the same as background, we're done */
    if ((ta == a) && (tc == c)) return;

    /* Draw the foreground tile */
    sdl_DrawTile(win, col, row, rc, prc, a, c, false);
}


/*
 * Draw some text to a window
 */
static errr Term_text_sdl_aux(int col, int row, int n, uint16_t a, const char *s)
{
    term_window *win = (term_window*)(Term->data);
    SDL_Color colour = text_colours[a % MAX_COLORS];
    SDL_Color bg = text_colours[COLOUR_DARK];
    int x = col * win->tile_wid;
    int y = row * win->tile_hgt;
    char buf[256];

    /* Paranoia */
    if (n > win->cols) return (-1);

    /* Translate */
    x += win->border;
    y += win->title_height;

    /* Not much point really... */
    if (!win->visible) return (0);

    /* Clear the way */
    Term_wipe_sdl(col, row, n);

    /* Take a copy of the incoming string, but truncate it at n chars */
    my_strcpy(buf, s, sizeof(buf));
    buf[n] = '\0';

    /* Handle background */
    switch (a / MULT_BG)
    {
        /* Default Background */
        case BG_BLACK: break;

        /* Background same as foreground */
        case BG_SAME: bg = colour; break;

        /* Highlight Background */
        case BG_DARK: bg = text_colours[COLOUR_SHADE]; break;
    }

    /* Draw it */
    return (sdl_mapFontDraw(&win->font, win->surface, colour, bg, x, y, n, buf));
}


/*
 * Draw some text to a window.
 *
 * For double-height tiles, we redraw all double-height tiles below.
 */
static errr Term_text_sdl(int col, int row, int n, uint16_t a, const char *s)
{
    term_window *win = (term_window*)(Term->data);
    SDL_Rect rc;
    int i;
    uint16_t fa, ta;
    char fc, tc;
    int tile_wid = 1, tile_hgt = 1;

    /* Large tile mode */
    if (!Term->minimap_active)
    {
        tile_wid = tile_width;
        tile_hgt = tile_height;
    }

    /* Redraw the current text */
    Term_text_sdl_aux(col, row, n, a, s);

    /* Redraw the bottom tiles (recursively) */
    for (i = 0; i < n; i++)
    {
        int j = 1, tilex, tiley;

        while (j)
        {
            /* Get the position of the jth tile below the ith character */
            tilex = COL_MAP + ((col - COL_MAP + i) / tile_wid) * tile_wid;
            tiley = ROW_MAP + ((row - ROW_MAP) / tile_hgt + j) * tile_hgt;

            if (overdraw && (tiley > ROW_MAP + 1) && !Term_info(tilex, tiley, &fa, &fc, &ta, &tc))
            {
                int row = (fa & 0x7F);
                int trow = (ta & 0x7F);

                if (((trow >= overdraw) && (trow <= overdraw_max)) ||
                    ((row >= overdraw) && (row <= overdraw_max)))
                {
                    get_sdl_rect(win, tilex, tiley, false, &rc);
                    set_update_rect(win, &rc);
                    sdl_DrawTiles(win, tilex, tiley, rc, NULL, fa, fc, ta, tc);
                    j++;
                }
                else j = 0;
            }
            else j = 0;
        }
    }

    /* Highlight the player */
    if (Term->minimap_active && (win->Term_idx == 0) && cursor_x && cursor_y)
        Term_curs_sdl(cursor_x + COL_MAP, cursor_y + ROW_MAP);

    /* Highlight party members */
    for (i = 0; Term->minimap_active && (win->Term_idx == 0) && (i < party_n); i++)
        Term_curs_sdl_aux(party_x[i] + COL_MAP, party_y[i] + ROW_MAP, text_colours[COLOUR_L_BLUE]);

    /* Success */
    return 0;
}


/*
 * Put some gfx on the screen.
 *
 * Called with n > 1 only if always_pict is true, which is never the case.
 *
 * For double-height tiles, we redraw the tile just above and all double-height tiles below.
 */
static errr Term_pict_sdl(int col, int row, int n, const uint16_t *ap, const char *cp,
    const uint16_t *tap, const char *tcp)
{
    /* Get the right window */
    term_window *win = (term_window*)(Term->data);
    SDL_Rect rc, rc2;
	int i;
    uint16_t a, ta;
    char c, tc;
    int tile_wid = 1, tile_hgt = 1;

    /* Large tile mode */
    if (!Term->minimap_active)
    {
        tile_wid = tile_width;
        tile_hgt = tile_height;
    }

    /* Toggle minimap view */
    if (win->minimap_active != Term->minimap_active)
    {
        win->minimap_active = Term->minimap_active;
        if (win->tiles)
        {
            SDL_FreeSurface(win->tiles);
            win->tiles = NULL;
        }
    }

    /* First time a pict is requested we load the tileset in */
    if (!win->tiles)
    {
        sdl_BuildTileset(win);
        if (!win->tiles) return (1);
    }

    /* Make the destination rectangle */
    if (get_sdl_rect(win, col, row, true, &rc)) return (1);

    /* Blit 'em! (it) */
    for (i = 0; i < n; i++)
    {
        int j = 1;

        /* Update area */
        set_update_rect(win, &rc);

        /* Clear the way */
        SDL_FillRect(win->surface, &rc, back_pixel_colour);

        /* Redraw the top tile */
        if (overdraw && !Term_info(col + i * tile_wid, row - tile_hgt, &a, &c, &ta, &tc))
        {
            if (a & 0x80)
            {
                get_sdl_rect(win, col + i * tile_wid, row - tile_hgt, false, &rc2);
                set_update_rect(win, &rc2);
                sdl_DrawTiles(win, col + i * tile_wid, row - tile_hgt, rc2, NULL, a, c, ta, tc);
            }
            else
            {
                int tx, ty;

                for (tx = col + i * tile_wid; tx < col + (i + 1) * tile_wid; tx++)
                {
                    for (ty = row - tile_hgt; ty < row; ty++)
                    {
                        Term_info(tx, ty, &a, &c, &ta, &tc);
                        Term_text_sdl_aux(tx, ty, 1, a, &c);
                    }
                }
            }
        }

        /* Draw the terrain and foreground tiles */
        sdl_DrawTiles(win, col + i * tile_wid, row, rc, NULL, ap[i], cp[i], tap[i], tcp[i]);

        /* Redraw the bottom tile (recursively) */
        while (j)
        {
            if (overdraw && (row + j * tile_hgt > ROW_MAP + 1) &&
                !Term_info(col + i * tile_wid, row + j * tile_hgt, &a, &c, &ta, &tc))
            {
                int frow = (a & 0x7F);
                int trow = (ta & 0x7F);

                if (((trow >= overdraw) && (trow <= overdraw_max)) ||
                    ((frow >= overdraw) && (frow <= overdraw_max)))
                {
                    get_sdl_rect(win, col + i * tile_wid, row + j * tile_hgt, false, &rc2);
                    set_update_rect(win, &rc2);
                    sdl_DrawTiles(win, col + i * tile_wid, row + j * tile_hgt, rc2, NULL, a, c,
                        ta, tc);
                    j++;
                }
                else j = 0;
            }
            else j = 0;
        }

        /* Advance */
        rc.x += rc.w;
    }

    /* Highlight the player */
    if (Term->minimap_active && (win->Term_idx == 0) && cursor_x && cursor_y)
        Term_curs_sdl(cursor_x + COL_MAP, cursor_y + ROW_MAP);

    /* Highlight party members */
    for (i = 0; Term->minimap_active && (win->Term_idx == 0) && (i < party_n); i++)
        Term_curs_sdl_aux(party_x[i] + COL_MAP, party_y[i] + ROW_MAP, text_colours[COLOUR_L_BLUE]);

    return (0);
}


/*
 * Create and initialize the Term contained within this window.
 */
static void term_data_link_sdl(term_window *win)
{
    term *t = &win->term_data;

    /* Initialize the term */
    term_init(t, win->cols, win->rows, win->max_rows, win->keys);

    t->higher_pict = true;

    /* Use a "software" cursor */
    t->soft_cursor = true;

    /* Differentiate between BS/^h, Tab/^i, etc. */
    t->complex_input = true;

    /* Never refresh one row */
    t->never_frosh = true;

    /* Ignore the init/nuke hooks */

    /* Prepare the template hooks */
    t->xtra_hook = Term_xtra_sdl;
    t->curs_hook = Term_curs_sdl;
    t->bigcurs_hook = Term_bigcurs_sdl;
    t->wipe_hook = Term_wipe_sdl;
    t->text_hook = Term_text_sdl;
    t->pict_hook = Term_pict_sdl;

    /* Remember where we came from */
    t->data = win;
}


/*
 * Initialize the status bar:
 *  Populate it with some buttons
 *  Set the custom draw function for the bar
 */
static void init_morewindows(void)
{
    char buf[128];
    sdl_Button *button;
    int x;

    popped = false;

    /* Make sure */
    sdl_WindowFree(&PopUp);

    /* Initialize the status bar */
    sdl_WindowInit(&StatusBar, AppWin->w, StatusHeight, AppWin, &default_term_font);

    /* Custom drawing function */
    StatusBar.draw_extra = draw_statusbar;

    /* Don't overlap the buttons */
    if (AppWin->w >= 720) my_strcpy(buf, version_build(VERSION_NAME, true), sizeof(buf));
    else my_strcpy(buf, "About...", sizeof(buf));

    AboutSelect = sdl_ButtonBankNew(&StatusBar.buttons);
    button = sdl_ButtonBankGet(&StatusBar.buttons, AboutSelect);

    /* Initialize the 'about' button */
    sdl_ButtonSize(button, StatusBar.font.width * strlen(buf) + 5, StatusHeight - 2);
    sdl_ButtonMove(button, 1, 1);
    sdl_ButtonVisible(button, true);
    sdl_ButtonCaption(button, buf);
    button->activate = AboutActivate;

    /* New button */
    TermSelect = sdl_ButtonBankNew(&StatusBar.buttons);
    button = sdl_ButtonBankGet(&StatusBar.buttons, TermSelect);

    /* Initialize the 'term' button */
    sdl_ButtonSize(button, get_term_namewidth(), StatusHeight - 2);
    x = 100 + (StatusBar.font.width * 5);
    sdl_ButtonMove(button, x, 1);
    sdl_ButtonVisible(button, true);
    button->activate = TermActivate;

    /* Another new button */
    VisibleSelect = sdl_ButtonBankNew(&StatusBar.buttons);
    button = sdl_ButtonBankGet(&StatusBar.buttons, VisibleSelect);

    /* Initialize the 'visible' button */
    sdl_ButtonSize(button, 50, StatusHeight - 2);
    x = 200 + (StatusBar.font.width * 8);
    sdl_ButtonMove(button, x, 1);
    sdl_ButtonVisible(button, true);
    button->activate = VisibleActivate;

    /* Another new button */
    FontSelect = sdl_ButtonBankNew(&StatusBar.buttons);
    button = sdl_ButtonBankGet(&StatusBar.buttons, FontSelect);

    /* Initialize the 'font_select' button */
    sdl_ButtonSize(button, get_font_namewidth(), StatusHeight - 2);
    sdl_ButtonMove(button, 400, 1);
    button->activate = FontActivate;

    /* Another new button */
    MoreSelect = sdl_ButtonBankNew(&StatusBar.buttons);
    button = sdl_ButtonBankGet(&StatusBar.buttons, MoreSelect);

    /* Initialize the 'more' button */
    sdl_ButtonSize(button, 50, StatusHeight - 2);
    sdl_ButtonMove(button, 400, 1);
    sdl_ButtonVisible(button, true);
    sdl_ButtonCaption(button, "Options");
    button->activate = MoreActivate;

    /* Another new button */
    QuitSelect = sdl_ButtonBankNew(&StatusBar.buttons);
    button = sdl_ButtonBankGet(&StatusBar.buttons, QuitSelect);

    /* Initialize the 'quit' button */
    sdl_ButtonSize(button, 50, StatusHeight - 2);
    sdl_ButtonMove(button, AppWin->w - 51, 1);
    sdl_ButtonCaption(button, "Quit");
    button->activate = QuitActivate;
    sdl_ButtonVisible(button, true);

    SetStatusButtons();

    TermFocus(0);
}


/*
 * The new streamlined graphics loader.
 * Only uses colour keys.
 * Much more tolerant of different bit-planes
 */
static errr load_gfx(void)
{
    char buf[MSG_LEN];
    const char *filename = NULL;
    SDL_Surface *temp;
    graphics_mode *mode;

    if (GfxSurface && is_current_graphics_mode(use_graphics)) return (0);

    mode = get_graphics_mode(use_graphics, true);
    if (mode && mode->grafID) filename = mode->file;

    /* Free the old surfaces */
    if (GfxSurface)
    {
        SDL_FreeSurface(GfxSurface);
        GfxSurface = NULL;
    }

    /* This may be called when GRAPHICS_NONE is set */
    if (!filename) return (0);

    /* Find and load the file into a temporary surface */
    path_build(buf, sizeof(buf), mode->path, filename);
    temp = IMG_Load(buf);
    if (!temp) return (1);

    /* Change the surface type to the current video surface format */
    GfxSurface = SDL_DisplayFormatAlpha(temp);

    overdraw = mode->overdrawRow;
    overdraw_max = mode->overdrawMax;

    /* All good */
    return (0);
}


/*
 * Initialize the graphics
 */
static void init_gfx(void)
{
    graphics_mode *mode;

    /* Check for existence of required files */
    mode = graphics_modes;
    while (mode)
    {
        char path[MSG_LEN];

        /* Check the graphic file */
        if (mode->file[0])
        {
            path_build(path, sizeof(path), mode->path, mode->file);

            if (!file_exists(path))
            {
                plog_fmt("Can't find file %s - graphics mode '%s' will be disabled.", path,
                    mode->menuname);
                mode->file[0] = 0;
            }
        }
        mode = mode->pNext;
    }

    /* Check availability (default to no graphics) */
    mode = get_graphics_mode(use_graphics, true);
    if (!(mode && mode->grafID && mode->file[0]))
    {
        use_graphics = GRAPHICS_NONE;
        reset_tile_params();
    }

    /* Load the graphics stuff in */
    load_gfx();
}

/*
 * Create the windows
 * Called sometime after load_prefs()
 */
static void init_windows(void)
{
    int i;

    for (i = 0; i < ANGBAND_TERM_MAX; i++)
    {
        term_window *win = &windows[i];

        /* Only bother with visible windows */
        if (win->visible)
        {
            /* Left bounds check */
            if (win->left < 0) win->left = 0;

            /* Right bounds check */
            if ((win->left + win->width) > AppWin->w)
            {
                if (win->width > AppWin->w) win->width = AppWin->w;
                win->left = AppWin->w - win->width;
            }

            /* Top bounds check */
            if (win->top < StatusHeight) win->top = StatusHeight;

            /* Bottom bounds check */
            if ((win->top + win->height) > AppWin->h)
            {
                if (win->height > AppWin->h) win->height = AppWin->h;
                win->top = AppWin->h - win->height;
            }

            /* Invalidate the gfx surface */
            if (win->tiles)
            {
                SDL_FreeSurface(win->tiles);
                win->tiles = NULL;
            }

            /* This will set up the window correctly */
            ResizeWin(win, win->width, win->height);
        }
        else
        {
            /* Doesn't exist */
            angband_term[i] = NULL;
        }

        /* Term 0 is at the top */
        Zorder[i] = ANGBAND_TERM_MAX - i - 1;

        /* Hack -- set ANGBAND_FONTNAME for main window */
        if (i == 0) ANGBAND_FONTNAME = win->req_font.name;
    }

    /* Good to go... */
    Term_activate(term_screen);
}


/*
 * Set up some SDL stuff
 */
static void init_sdl_local(void)
{
    const SDL_VideoInfo *VideoInfo;
    int i;
    int h, w;
    char path[MSG_LEN];

    /* Get information about the video hardware */
    VideoInfo = SDL_GetVideoInfo();

    /* Require at least 256 colors */
    if (VideoInfo->vfmt->BitsPerPixel < 8)
        quit_fmt("this %s port requires lots of colors.", version_build(VERSION_NAME, true));

    full_w = VideoInfo->current_w;
    full_h = VideoInfo->current_h;

    /* Use a software surface - A tad inefficient, but stable... */
    vflags |= SDL_SWSURFACE;

    /* Set fullscreen flag */
    if (fullscreen) vflags |= SDL_FULLSCREEN;

    /* otherwise we make this surface resizable */
    else vflags |= SDL_RESIZABLE;

    /* Create the main window */
    AppWin = SDL_SetVideoMode(fullscreen? full_w: screen_w, fullscreen? full_h: screen_h, 0, vflags);

    /* Handle failure */
    if (!AppWin)
    {
        quit_fmt("failed to create %dx%d window at %d bpp!", screen_w, screen_h,
            VideoInfo->vfmt->BitsPerPixel);
    }

    /* Set the window caption */
    SDL_WM_SetCaption(version_build(VERSION_NAME, true), NULL);

    /* Enable key repeating; use defaults */
    SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);

    /* Enable Unicode (so we can read key codes) */
    SDL_EnableUNICODE(1);

    /* Build a color for "black" that matches the pixel depth of this surface */
    back_colour.r = angband_color_table[COLOUR_DARK][1];
    back_colour.g = angband_color_table[COLOUR_DARK][2];
    back_colour.b = angband_color_table[COLOUR_DARK][3];
    back_pixel_colour = SDL_MapRGB(AppWin->format, back_colour.r, back_colour.g, back_colour.b);

    /* Initialize the colours */
    for (i = 0; i < MAX_COLORS; i++)
    {
        text_colours[i].r = angband_color_table[i][1];
        text_colours[i].g = angband_color_table[i][2];
        text_colours[i].b = angband_color_table[i][3];
    }

    /* Get the height of the status bar */
    if (sdl_CheckFont(&default_term_font, &w, &h))
		quit_fmt("could not load the default font, %s", default_term_font.name);
    StatusHeight = h + 3;

    /* Font used for window titles */
    sdl_FontCreate(&SystemFont, &default_term_font, AppWin);

    /* Get the icon for display in the About box */
    path_build(path, sizeof(path), ANGBAND_DIR_ICONS, "att-128.png");
    if (file_exists(path))
        mratt = IMG_Load(path);
}


/*
 * Font sorting function
 *
 * Orders by width, then height, then face
 */
static int cmp_font(const void *f1, const void *f2)
{
    const char *font1 = *(const char **)f1;
    const char *font2 = *(const char **)f2;
    int height1 = 0, height2 = 0;
    int width1 = 0, width2 = 0;
    char *ew, *face1 = NULL, *ext1 = NULL, *face2 = NULL, *ext2 = NULL;
    long lv;

    lv = strtol(font1, &ew, 10);
    if (ew != font1 && *ew == 'x' && lv > INT_MIN && lv < INT_MAX)
    {
        width1 = (int)lv;
        lv = strtol(ew + 1, &face1, 10);
        if (face1 != ew + 1 && lv > INT_MIN && lv < INT_MAX)
        {
            height1 = (int)lv;
            ext1 = strchr(face1, '.');
            if (ext1 == face1) ext1 = NULL;
        }
    }
    lv = strtol(font2, &ew, 10);
    if (ew != font2 && *ew == 'x' && lv > INT_MIN && lv < INT_MAX)
    {
        width2 = (int)lv;
        lv = strtol(ew + 1, &face2, 10);
        if (face2 != ew + 1 && lv > INT_MIN && lv < INT_MAX)
        {
            height2 = (int)lv;
            ext2 = strchr(face2, '.');
            if (ext2 == face2) ext2 = NULL;
        }
    }

    if (!ext1)
    {
		if (!ext2)
        {
			/* Neither match the expected pattern. Sort alphabetically. */
			return strcmp(font1, font2);
		}

		/* Put f2 first since it matches the expected pattern. */
		return 1;
	}
	if (!ext2)
    {
		/* Put f1 first since it matches the expected pattern. */
		return -1;
	}
	if (width1 < width2)
		return -1;
	if (width1 > width2)
		return 1;
	if (height1 < height2)
		return -1;
	if (height1 > height2)
		return 1;
	return strncmp(face1, face2, MAX(ext1 - face1, ext2 - face2));
}


static void init_paths(void)
{
    int i;
    char buf[MSG_LEN], path[MSG_LEN];
    ang_dir *dir;

    /* Hack -- validate the basic font */
    if (default_term_font.preset)
    {
		/* Build the filename */
		path_build(path, sizeof(path), ANGBAND_DIR_FONTS, default_term_font.name);
		validate_file(path);
	}
    else
		validate_file(default_term_font.name);

    for (i = 0; i < MAX_FONTS; i++)
        FontList[i] = NULL;

    /* Open the fonts directory */
    dir = my_dopen(ANGBAND_DIR_FONTS);
    if (!dir) return;

    /* Read every font to the limit */
	while (my_dread(dir, buf, sizeof(buf)))
    {
		path_build(path, sizeof(path), ANGBAND_DIR_FONTS, buf);
		if (is_font_file(path))
			FontList[num_fonts++] = string_make(buf);

		/* Don't grow to long */
		if (num_fonts == MAX_FONTS) break;
	}

    sort(FontList, num_fonts, sizeof(FontList[0]), cmp_font);

    /* Done */
    my_dclose(dir);
}


/*
 * The SDL port's "main()" function.
 */
errr init_sdl(void)
{
    /* Remove W8080 warnings: SDL_Swap16/64 is declared but never used */
    SDL_Swap16(0);
    SDL_Swap64(0);

    /* Activate hook */
    plog_aux = hook_plog;

    /* Initialize SDL:  Timer, video, and audio functions */
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        plog_fmt("Couldn't initialize SDL: %s\n", SDL_GetError());
        return (2);
    }

    /* Initialize the TTF library */
    if (TTF_Init() < 0)
    {
        plog_fmt("Couldn't initialize TTF: %s\n", SDL_GetError());
        SDL_Quit();
        return (2);
    }

    /* Init some extra paths */
    init_paths();

    /* Load possible graphics modes */
    init_graphics_modes();
    GfxButtons = mem_zalloc(sizeof(int) * (graphics_mode_high_id + 1));

    /* Load prefs */
    load_prefs();

    /* Get sdl going */
    init_sdl_local();

    /* Load window prefs */
    load_window_prefs();

    /* Prepare the windows */
    init_windows();

    /* Prepare the gfx */
    init_gfx();

    /* Prepare some more windows(!) */
    init_morewindows();

    /* Show on the screen */
    sdl_BlitAll();

    /* Activate hook */
    quit_aux = hook_quit;

#ifdef WINDOWS
    /* Register a control handler */
    if (!SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, true))
        quit("Could not set control handler");
#endif

    /* Paranoia */
    return (0);
}

#endif /* USE_SDL */

