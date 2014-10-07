rem 调用VC的工程制造引出库，MinGW 的 dlltool制造出来的在release版本下无法引入
call "C:\Program Files (x86)\Microsoft Visual Studio 12.0\VC\bin\VCVARS32.BAT"

set dir="sdk"
mkdir  %dir%

set filename="avcodec-56"
pexports %filename%.dll > %dir%/%filename%.def
LIB /def:%dir%/%filename%.def /machine:i386 /out:%dir%/%filename%.lib

set filename="avdevice-56"
pexports %filename%.dll > %dir%/%filename%.def
LIB /def:%dir%/%filename%.def /machine:i386 /out:%dir%/%filename%.lib

set filename="avfilter-5"
pexports %filename%.dll > %dir%/%filename%.def
LIB /def:%dir%/%filename%.def /machine:i386 /out:%dir%/%filename%.lib

set filename="avformat-56"
pexports %filename%.dll > %dir%/%filename%.def
LIB /def:%dir%/%filename%.def /machine:i386 /out:%dir%/%filename%.lib

set filename="avutil-54"
pexports %filename%.dll > %dir%/%filename%.def
LIB /def:%dir%/%filename%.def /machine:i386 /out:%dir%/%filename%.lib

set filename="postproc-53"
pexports %filename%.dll > %dir%/%filename%.def
LIB /def:%dir%/%filename%.def /machine:i386 /out:%dir%/%filename%.lib

set filename="swresample-1"
pexports %filename%.dll > %dir%/%filename%.def
LIB /def:%dir%/%filename%.def /machine:i386 /out:%dir%/%filename%.lib

set filename="swscale-3"
pexports %filename%.dll > %dir%/%filename%.def
LIB /def:%dir%/%filename%.def /machine:i386 /out:%dir%/%filename%.lib

 