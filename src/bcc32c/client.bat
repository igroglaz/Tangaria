del ..\client\c-cmd.obj
del ..\client\main.obj
del ..\client\set_focus.obj
del ..\client\sound-core.obj
del ..\client\ui-display.obj
make -f makefile.gcu
pause
del ..\client\c-cmd.obj
del ..\client\main.obj
del ..\client\set_focus.obj
del ..\client\sound-core.obj
del ..\client\ui-display.obj
make -f makefile.sdl
pause
del ..\client\c-cmd.obj
del ..\client\main.obj
del ..\client\set_focus.obj
del ..\client\sound-core.obj
del ..\client\ui-display.obj
make -f makefile.sdl2
pause
del ..\client\c-cmd.obj
del ..\client\main.obj
del ..\client\set_focus.obj
del ..\client\sound-core.obj
del ..\client\ui-display.obj
make -f makefile-win-sdl2-mixer.bcc
:: make -f makefile.bcc
:: make -f makefile-win-sdl-mixer.bcc
pause
move mangclient_gcu.exe ..\..\mangclient_gcu.exe
move mangclient_sdl.exe ..\..\mangclient_sdl.exe
move mangclient_sdl2.exe ..\..\mangclient_sdl2.exe
move mangclient.exe ..\..\mangclient.exe
