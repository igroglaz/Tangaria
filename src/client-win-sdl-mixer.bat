del client\c-cmd.obj
del client\main.obj
del client\set_focus.obj
del client\sound-core.obj
del client\ui-display.obj
make -f makefile-win-sdl-mixer.bcc
pause
move mangclient.exe ..\mangclient.exe
