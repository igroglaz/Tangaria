cd ..\..\..\zlib-1.2.11
make -f win32\Makefile.zlib
pause
copy zlib.lib ..\PWMAngband\src\win
pause
cd ..\lpng1637
make -f scripts\makefile.libpng
pause
copy libpng.lib ..\PWMAngband\src\win
pause
copy png.h ..\PWMAngband\src\win
copy pnglibconf.h ..\PWMAngband\src\win
copy pngconf.h ..\PWMAngband\src\win
pause
