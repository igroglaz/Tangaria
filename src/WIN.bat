cd ..\..\zlib-1.2.11
make -f win32\Makefile.bor
pause
copy zlib.lib ..\Tangaria-dev\src\win
pause
cd ..\lpng1637
make -f scripts\makefile.bc32
pause
copy libpng.lib ..\Tangaria-dev\src\win
pause
copy png.h ..\Tangaria-dev\src\win
copy pnglibconf.h ..\Tangaria-dev\src\win
copy pngconf.h ..\Tangaria-dev\src\win
pause
