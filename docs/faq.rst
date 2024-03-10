Frequently Asked Questions
==========================

The best way to get answers to your questions is to post them on the
`Angband forum`_ or `MAngband forum`_.

Issues and problems
-------------------

How do I report a bug?
**********************

Post on the `Angband forum`_ or `MAngband forum`_.

Be sure to include everything you can figure out to reproduce the bug.

Savefiles that show the problem might be requested, because they help tracking
bugs down.

Dark monsters are hard to see
*****************************

Fix (reduce) the alpha on your screen, or use the "Interact with colors" screen
under the options (``=``) menu. Navigate to the ``8`` using ``n`` and increase
the color intensity with r(ed)/g(reen)/b(lue).

Is there a way to disable that thing that pops up when you hit the enter key?
*****************************************************************************

Go into the options menu, choose "Edit keymaps", then "Create a keymap". Press
Enter at the "Key" prompt and a single space as the "Action".

And then you'll probably want to choose to "Append keymaps to a file" so that it
persists for next time you load the game.

This just replaces the default action of Enter with a "do nothing but don't tell
me about help" action. If you want to keep the menu available, say on the 'Tab'
key, you can also remap the Tab keypress to the ``\n`` action.

In PWMAngband, you can also use the disable_enter option from the Options menu,
Advanced options section.

Development
-----------

What are the current plans for the game?
****************************************

See forums.

How do I suggest an idea/feature?
*********************************

Post it on the forums. If people think it's a good idea, it will generally get
some discussion; if they don't, it won't. The developers keep an eye on the
forums, and ideas deemed OK will get filed for future implementation.

Sometimes a suggestion may not be right for the game, though. Some suggestions
would change aspects of PWMAngband that are essential to its nature; PWMAngband
has a long history, and so has developed a certain character over the years.
Some suggestions might make a good game, perhaps even a better game than
PWMAngband, but would make a game that is not PWMAngband. To some extent,
variants exist to address this (for example: Tangaria has a more colorful
graphical environment), but even so they tend to adhere to the core PWMAngband
principles.

How do I get a copy of the source code?
***************************************

Get it from GitHub: https://github.com/draconisPW/PWMAngband.

How do I compile the game?
**************************

Please see the compiling section of the manual.

How do I contribute to the game?
********************************

Post about it on the forum.

.. _Angband forum: https://angband.live/forums/
.. _MAngband forum: https://mangband.org/forum/
