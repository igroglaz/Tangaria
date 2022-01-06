del ..\client\c-cmd.obj
del ..\client\main.obj
del ..\client\set_focus.obj
del ..\client\sound-core.obj
del ..\client\ui-display.obj
make -f makefile.sdl2
pause
move mangclient_sdl2.exe ..\..\mangclient_sdl2.exe
