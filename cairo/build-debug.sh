# rm "libpng1616/libpng.lib"
# rm "zlib-1.2.8/zdll.lib"
# cp "libpng1616/projects/vstudio/Debug Library/libpng16.lib" "libpng1616/libpng.lib" 
# cp "zlib-1.2.8/build/Debug/zlibstaticd.lib" "zlib-1.2.8/zdll.lib"
 
set CAIROROOT=C:/Develop/code/iwallee/teedow/3rd/cairo
set "INCLUDE=%INCLUDE%;%CAIROROOT%/src;%CAIROROOT%/pixman/pixman;%CAIROROOT%/zlib-1.2.8;%CAIROROOT%/libpng1616;"

cd pixman
make -f Makefile.win32 "CFG=debug" "MMX=on" "SSE2=on" "SSSE3=on" 

cd ..
make -f Makefile.win32 "CFG=debug" "LIBPNG_PATH=C:/Develop/code/iwallee/teedow/3rd/cairo/libpng1616" "ZLIB_PATH=C:/Develop/code/iwallee/teedow/3rd/cairo/zlib-1.2.8" "PIXMAN_PATH=C:/Develop/code/iwallee/teedow/3rd/cairo/pixman" 
