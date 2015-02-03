rem rm "libpng1616/libpng.lib"
rem rm "zlib-1.2.8/zdll.lib"
rem cp "libpng1616/projects/vstudio/Release Library/libpng16.lib" "libpng1616/libpng.lib" 
rem cp "zlib-1.2.8/build/Release/zlibstatic.lib" "zlib-1.2.8/zdll.lib"
 
set CAIROROOT=C:\Develop\code\iwallee\teedow\3rd\cairo
set "INCLUDE=%INCLUDE%;%CAIROROOT%/src;%CAIROROOT%/pixman/pixman;%CAIROROOT%/zlib-1.2.8;%CAIROROOT%/libpng1616;"

cd pixman
make -f Makefile.win32 "CFG=release" "MMX=on" "SSE2=on" "SSSE3=on" 

cd ..
make -f Makefile.win32 "CFG=release" "LIBPNG_PATH=%CAIROROOT%/libpng1616" "ZLIB_PATH=%CAIROROOT%/zlib-1.2.8" "PIXMAN_PATH=%CAIROROOT%/pixman"
