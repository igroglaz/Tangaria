cd ..\..\..\zlib-1.2.11
make -f win32\Makefile.zlib
pause
copy zlib.lib ..\Tangaria-dev\src\win
pause
cd ..\lpng1637
make -f scripts\makefile.libpng
pause
copy libpng.lib ..\Tangaria-dev\src\win
pause
copy png.h ..\Tangaria-dev\src\win
copy pnglibconf.h ..\Tangaria-dev\src\win
copy pngconf.h ..\Tangaria-dev\src\win
pause
