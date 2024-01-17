call cleansetup.bat
copy ..\docs\*.* ..\setup\docs
copy ..\docs\_static\*.* ..\setup\docs\_static
copy ..\docs\_templates\*.* ..\setup\docs\_templates
copy ..\docs\hacking\*.* ..\setup\docs\hacking
copy ..\lib\readme.txt ..\setup\lib
copy ..\lib\customize\*.* ..\setup\lib\customize
copy ..\lib\fonts\*.* ..\setup\lib\fonts
copy ..\lib\gamedata\*.* ..\setup\lib\gamedata
copy ..\lib\help\*.* ..\setup\lib\help
copy ..\lib\icons\*.* ..\setup\lib\icons
copy ..\lib\music\*.* ..\setup\lib\music
copy ..\lib\music\generic-dungeon\*.* ..\setup\lib\music\generic-dungeon
copy ..\lib\music\generic-town\*.* ..\setup\lib\music\generic-town
copy ..\lib\music\intro\*.* ..\setup\lib\music\intro
copy ..\lib\music\town-day\*.* ..\setup\lib\music\town-day
copy ..\lib\music\town-night\*.* ..\setup\lib\music\town-night
copy ..\lib\screens\*.* ..\setup\lib\screens
copy ..\lib\sounds\*.* ..\setup\lib\sounds
xcopy /S ..\lib\sounds\ambient ..\setup\lib\sounds\ambient
xcopy /S ..\lib\sounds\tomenet ..\setup\lib\sounds\tomenet
copy ..\lib\tiles\*.* ..\setup\lib\tiles
copy ..\lib\tiles\tangaria\*.* ..\setup\lib\tiles\tangaria
copy ..\lib\user\save\*.* ..\setup\lib\user\save
copy *.* ..\setup\src
copy _SDL\*.* ..\setup\src\_SDL
copy _SDL2\*.* ..\setup\src\_SDL2
copy client\*.* ..\setup\src\client
copy common\*.* ..\setup\src\common
copy curses\*.* ..\setup\src\curses
copy fix\*.* ..\setup\src\fix
copy sdl2\*.* ..\setup\src\sdl2
copy server\*.* ..\setup\src\server
copy win\*.* ..\setup\src\win
pause
call server.bat
call client.bat
copy ..\changes.txt ..\setup
copy ..\*.dll ..\setup
copy ..\*.exe ..\setup
copy ..\Manual.html ..\setup
pause
