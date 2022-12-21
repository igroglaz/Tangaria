Customising the game
====================

PWMAngband allows you to change various aspects of the game to suit your tastes.
These include:

* Options - which let you change interface or gameplay behaviour
* `Ignoring items`_ and `inscribing items`_ to change how the game treats them
* `Showing extra info in subwindows`_
* `Keymaps`_ - a way to assign commonly-used actions to specific keys
* `Colours`_ - allowing you to make a given color brighter, darker, or even completely different
* `Interface details`_ - depending on which interface to the game you use, these give you control over the font, window placement, and graphical tile set

Except for the options, which are linked to the save file, and interface
details, that are handled by the front end rather than the core of the game,
you can save your preferences for these into files, which are called
`user pref files`. For the options, customize those using the ``=`` command
while playing.

User Pref Files
---------------

User pref files are PWMAngband's way of saving and loading certain settings.
They can store:

* Altered visual appearances for game entities
* Inscriptions to automatically apply to items
* Keymaps
* Altered colours
* Subwindow settings
* Colours for different types of messages
* What audio files to play for different types of messages

They are simple text files with an easy to modify format, and the game has a set
of pre-existing pref files in the lib/customize/ folder. It's recommended you
don't modify these.

Several options menu (``=``) items allow you to load existing user pref files,
create new user pref files, or save to a user pref file.

Where to find them
******************

On Windows you can find them in ``lib/user/``.

How do they get loaded?
***********************

When the game starts up, after you have loaded or created a character, some user
pref files are loaded automatically. These are the ones mentioned above in the
``lib/customize/`` folder, namely ``pref.prf`` followed by ``font.prf``. If you
have graphics turned on, then the game will also load some settings from
``lib/tiles/``.

After these are complete, the game will try to load (in order):

* *race*.prf - where *race* is your character's race, so something like ``Dwarf.prf``
* *class*.prf - where *class* is your character's class, so something like ``Paladin.prf``
* *name*.prf - where *name* is your character's name, so something like ``Balin.prf``

So, you can save some settings - for example, keymaps - to the ``Mage.prf`` file
if you only want them to be loaded for mages.

You may also enter single user pref commands directly, using the special "Enter
a user pref command" command, activated by pressing ``"``.

You may have to use the redraw command (``^R``) after changing certain of the
aspects of the game to allow PWMAngband to adapt to your changes.

Ignoring items
--------------

PWMAngband allows you to ignore specific items that you don't want to see
anymore. These items are marked 'ignored' and any similar items are hidden from
view. The easiest way to ignore an item is with the ``k`` (or ``^D``) command;
the object is dropped and then hidden from view. When ignoring an object, you
will be given a choice of ignoring just that object, or all objects like it in
some way.

The entire ignoring system can also be accessed from the options menu (``=``) by
choosing ``i`` for ``Item ignoring setup``. This allows ignore settings for
non-wearable items, and quality and ego ignore settings (described below) for
wearable items, to be viewed or changed.
      
There is a quality setting for each wearable item type. Ignoring a wearable item
will prompt you with a question about whether you wish to ignore all of that
type of item with a certain quality setting, or of an ego type, or both.

The quality settings are:

..

worthless
  Weapon/armor with negative AC, to-hit or to-dam, or item with zero base cost.

..

average
  Weapon/armor with no pluses no minuses, or any other non-magical item.

..

good
  Weapon/armor with positive AC, to-hit or to-dam, but without any special
  abilities, brands, slays, stat-boosts, resistances, or magical items.

..
 
non-artifact
  This setting only leaves artifacts unignored.

Inscribing items
----------------

Inscriptions are notes you can mark on objects using the ``{`` command. You can
use this to give the game commands about the object, which are listed below. You
can also set up the game to automatically inscribe certain items whenever you
find them, then save these autoinscriptions to pref file by using ``=`` then
``t``.

..

Inscribing an item with '=g':
    This marks an item as 'always pick up'. This is sometimes useful for
    picking up ammunition after a shootout. If there is a number
    immediately after the 'g', then the amount picked up automatically
    will be limited. If you have inscribed a spellbook with '=g4' and have
    four or more copies in your pack, you will not automatially pick up
    any more copies when you have the 'pickup if in inventory' option
    enabled. If you have three copies in your pack with that inscription
    and happen to find a pile of two copies, you'll automatically pick up
    one so there is four in the pack.

..

Inscribing an item with ``!`` followed by a command letter or ``*``:
    This means "prevent me from using this item". '!w' means 'prevent me from
    wielding', '!d' means 'prevent me from dropping', and so on. If you
    inscribe an item with '!*' then the game will prevent any use of the item.
    Say you inscribed your potion of Speed with '!q'. This would prevent you
    from drinking it to be sure you really meant to.
    Similarly, using !v!k!d makes it very hard for you to accidentally throw,
    ignore or put down the item it is inscribed on.
    Some adventurers use this for Scrolls of Word of Recall so they don't
    accidentally return to the dungeon too soon.

..

Inscribing an item with ``@``, followed by a command letter, followed by 0-9:
    Normally when you select an item from your inventory you must enter the
    letter that corresponds to the item. Since the order of your inventory
    changes as items get added and removed, this can get annoying. You
    can instead assign certain items numbers when using a command so that
    wherever they are in your backpack, you can use the same keypresses.
    If you have multiple items inscribed with the same thing, the game will
    use the first one.
    For example, if you inscribe a staff of Cure Light Wounds with '@u1',
    you can refer to it by pressing 1 when (``u``)sing it. You could also
    inscribe a wand of Wonder with '@a1', and when using ``a``, 1 would select
    that wand.
    Spellcasters should inscribe their books, so that if they lose them they
    do not cast the wrong spell. If you are mage and the beginner's
    spellbook is the first in your inventory, casting 'maa' will cast magic
    missile. But if you lose your spellbook, casting 'maa' will cast the
    first spell in whatever new book is in the top of your inventory. This
    can be a waste in the best case scenario and exceedingly dangerous in
    the worst! By inscribing your spellbooks with '@m1', '@m2', etc., if
    you lose your first spellbook and attempt to cast magic missile by
    using 'm1a', you cannot accidentally select the wrong spellbook.

..

Inscribing an item with ``^``, followed by a command letter:
    When you inscribe an item with ``^``, the game prevents you from doing that
    action. You might inscribe '^>' on an item if you want to be reminded to
    take it off before going down stairs.
    Like with ``!``, you can use ``*`` for the command letter if you want to
    game to prevent you from doing any action. This can get very annoying!

Showing extra info in subwindows
--------------------------------

In addition to the main window, you can create additional windows that have
secondary information on them. You can access the subwindow menu by using ``=``
then ``w``, where you can choose what to display in which window.

You may then need to make the window visible using the "window" menu from the
menu bar (if you have one in your version of the game).

There are a variety of subwindow choices and you should experiment to see which
ones are the most useful for you.

Keymaps
-------

You can set up keymaps in PWAngband, which allow you to map a single keypress,
the trigger, to a series of keypresses, the action. For example you might map
the key F1 to "maa" (the keypresses to cast "Magic Missile" as a spellcaster).
This can speed up access to commonly-used features. To bypass a keymap that's
been assigned to a key, press ``\`` before pressing the key.

To set up keymaps, go to the options menu (``=``) and select "Edit keymaps"
(``e``). There, you can check if a key triggers a keymap: select "Query a
keymap" (``c``) and then press the key to check. You can also remove an existing
keymap: select "Remove a keymap" (``e``) and then press the key that trigger the
keymap to be removed. To add a new keymap (or overwrite an existing one), select
"Create a keymap" (``d``), it will then prompt you for the key that triggers the
keymap. After pressing the trigger key, you'll be prompted for the keymap's
action, the series of keypresses that'll be generated when the trigger key is
pressed. If you make a mistake while entering the keypresses for the action,
press ``Control-u`` to erase the keypresses already entered for the action. Once
you've finished entering the keypresses for the action, press ``=`` to end the
sequence; you'll then be prompted for whether to keep the newly entered keymap.

Within the action for a keymap, it is frequently useful to temporarily suppress
-more- prompts since they can swallow keypresses from the keymap. To disable
those prompts from within the action, include ``(``.  To reenable the prompts,
include ``)``. So, a typical action where -more- prompts could happen would look
like this: ``(`` your keypresses here ``)``.

The keypresses in the action will be interpreted relative to the keyset you are
currently using (original or roguelike). The game will remember what keyset was
in effect when the keymap was created. So if you change keysets, the keymaps
which were only defined for the other keyset won't be visible. You can have two
keymaps, one for the original keyset and another for the roguelike keyset, bound
to the same trigger.

Keymaps are not recursive. If you have F1 as the trigger for a keymap, including
F1 as a keypress in the action for that or another keymap won't invoke that
keymap.

Any changes you make to keymaps from the options menu only last as long as the
game is running. To have them affect future sessions, save the keymaps to a
file. There's an option to do that from the menu for editing keymaps. See
`User Pref Files`_ for how the name of the file affects whether the file is
loaded when the game reloads your character.

Note that the game accounts for the modifier keys (Shift, Control, Alt, Meta)
that are pressed along with a key. On most platforms, the game also
distinguishes between the keys on the numeric keypad that have equivalents on
the main keyboard. When a keypress is displayed or saved to the preference file,
the modifiers, if any, for the keypress are displayed by code letters (S for
Shift, ^ for Control, A for Alt, M for Meta, and K for the numeric keypad)
within curly braces prior to the keypress. There are two exceptions to that: if
Control is the only modifier it will displayed as ^ before the keypress without
any curly braces and if Shift is the only modifier it will often be folded into
the keypress itself. For example::

	{^S}& = Control-Shift-&
	{AK}0 = Alt-0 from the numeric keypad
	^d    = Control-d
	A     = Shift-a

Special keys, like F1, F2, or Tab, are all written within square brackets [].
For example::

	^[F1]     = Control-F1
	{^S}[Tab] = Control-Shift-Tab

Special keys include [Escape].

You may find it easier to edit the preference files directly to change a keymap.
Keymaps are written in pref files as::

	keymap-act:<action>
	keymap-input:<type>:<trigger>

The action must always come first, ```<type>``` means 'keyset type', which is
either 0 for the original keyset or 1 for the roguelike keyset. For example::

	keymap-act:maa
	keymap-input:0:[F1]

An action can have more than one trigger bound to it by having more than
one keymap-input line after it and before the next keymap-act line. One
reason to do that would be to have the keymap work with either keyset. For
example::

	keymap-act:maa
	keymap-input:0:[F1]
	keymap-input:1:[F1]

Angband uses a few built-in keymaps. These are for the movement keys (they are
mapped to ``;`` plus the number, e.g. ``5`` -> ``;5``), amongst others. You can
see the full list in pref.prf, but they shouldn't impact you in any way.

Colours
-------

The "Interact with colors" options submenu (``=``, then ``v``) allows you to
change how different colours are displayed. Depending on what kind of computer
you have, this may or may not have any effect.

The interface is quite clunky. You can move through the colours using ``n`` for
'next colour' and ``N`` for 'previous colour'. Then upper and lower case ``r``,
``g`` and ``b`` will let you tweak the color. You can then save the results to
user pref file.

Interface details
-----------------

Some aspects of how the game is presented, notably the font, window placement
and graphical tile set, are controlled by the front end, rather than the core
of the game itself. Each front end has its own mechanism for setting those
details and recording them between game sessions. Below are brief descriptions
for what you can configure with the standard `Windows`_ and `SDL`_ front ends.

Windows
*******

With the Windows front end, the game, by default, displays several of
the subwindows and uses David Gervais's graphical tiles to display the map.
You can close a subwindow with the standard close control on the window's
upper right corner. Closing the main window with the standard control causes
the game to save its current state and then exit. You can reopen or also
close a subwindow via the "Visibility" menu, the first entry in the "Window"
menu for the main window. To move a window, use the standard procedure:
position the mouse pointer on the window's title bar and then click and drag
the mouse to change the window's position. Click and drag on the edges or
corners of a window to change its size. To select the font for a window, use
the "Font" menu, the second entry in the "Window" menu for the main window.

The "Term Options" entry in the "Window" menu for the main window is a shortcut
to access the core game's method for selecting the contents of the subwindows.
You can read more about that in `Showing extra info in subwindows`_. The
"Reset Layout" will rearrange the windows to conform with the current size and
will have a similar result to what you would get from restarting the Windows
interface without a preset configuration.

The "Bizarre Display" entry in the "Window" menu allows to toggle on or off
an alternate text display algorithm for each window. That was added for
compatibility with Windows Vista and later. The default setting, on, should
likely be used, unless text display is garbled on your system and the off
setting allows text to be displayed properly.

The "Increase Tile Width" and "Decrease Tile Width" options in the "Window",
let you increment or decrement, by one pixel, the width of the columns in a
window. The "Increase Tile Height" and "Decrease Tile Height" options are
similar but work with the height of the rows. For the primary window, you
could use the "Term 0 Font Tile Size" entry as an alternative to those to set
the width of the columns and height of the rows to certain combinations or to
match the width and height of the font, which is the default. When the
"Enable Nice Graphics" option is on (it's in the "Options" menu for the main
window), the "Increase Tile Width", "Decrease Tile Width",
"Increase Tile Height", "Decrease Tile Height", and "Term 0 Font Tile Size"
entries will have no effect since the column width and row height are set
automatically when that option is on.

To change whether graphical tiles are used, use the "Graphics" menu, the first
entry in the "Options" menu for the main window. The "None" option in the
"Graphics" menu will disable graphical tiles and use text for the map. The
next section section in that menu allows you to select one of the graphical
tile sets. Turning on the "Enable Nice Graphics" option in the "Graphics"
menu is a shortcut for automatically setting sizes to get a reasonable-looking
result. When that is turned on or is already on and the tile set is changed,
the width of the columns ("tile width"), height of the rows ("tile height")
and the number of rows and columns used to display a tile (the
"Tile Multiplier") will be adjusted to work well with the current font size and
the native size of the graphical tiles. You can manually adjust the number of
rows and columns used for displaying a tile with the "Tile Multiplier" entry
in the "Graphics" menu. Since typical fonts are often twice as tall as wide,
multipliers where the first value, for the width, is twice the second, often
x work better with the tiles that are natively square.

When you leave the game, the current settings for the Windows interface are
saved as ``mangclient.ini`` in the directory that holds the executable. Those
settings will be automatically reloaded the next time you start the Windows
interface.

SDL
***

With the SDL front end, the main window and any subwindows are displayed within
the application's rectangular window. At the top of the application's window
is a status line. Within that status line, items highlighted in yellow are
buttons that can be pressed to initiate an action. From left to right they are:

* The application's version number - pressing it displays an information dialog about the application
* The currently selected terminal - pressing it brings up a menu for selecting the current terminal; you can also make a terminal the current one by clicking on the terminal's title bar if it is visible
* Whether or not the current terminal is visible - pressing it for any terminal that is not the main window will allow you to show or hide that terminal
* The font for the current terminal - pressing it brings up a menu to choose the font for the terminal
* Options - brings up a dialog for selecting global options including those for the graphical tile set used and whether fullscreen mode is enabled
* Quit - to save the game and exit

To move a terminal window, click on its title bar and then drag the mouse.
To resize a terminal window, position the mouse pointer over the lower right
corner. That should cause a blue square to appear, then click and drag to
resize the terminal.

To change the graphical tile set used when displaying the game's map, press
the Options button in the status bar. Then, in the dialog that appears, press
one of the red buttons that appear to the right of the label,
"Available Graphics:". The last of those buttons, labeled "None", selects
text as the method for displaying the map. Your choice for the graphical tile
set does not take effect until you press the red button labeled "OK" at the
bottom of the dialog.

When you leave the game, the current settings for the SDL interface are saved
as ``sdlinit.txt`` in the same directory as is used for preference files, see
`User Pref Files`_ for details. Those settings will be automatically reloaded
the next time you start the SDL interface.
