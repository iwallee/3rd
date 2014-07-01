#include "pngpriv.h"
#include "png_io.h"

FILE * png_fopen(const char * _Filename, const char * _Mode)
{
    return fopen(_Filename, _Mode);
}
errno_t png_fopen_s(FILE ** _File, const char * _Filename, const char * _Mode)
{
    return fopen_s(_File, _Filename, _Mode);
}
size_t png_fread(void * _DstBuf, size_t _ElementSize, size_t _Count, FILE * _File)
{
    return fread(_DstBuf, _ElementSize, _Count, _File);
}
size_t png_fread_s(void * _DstBuf, size_t _DstSize, size_t _ElementSize, size_t _Count, FILE * _File)
{
    return fread_s(_DstBuf, _DstSize, _ElementSize, _Count, _File);
}
int __cdecl png_fseek(FILE * _File, long _Offset, int _Origin)
{
    return fseek(_File, _Offset, _Origin);
}
long __cdecl png_ftell(FILE * _File)
{
    return ftell(_File);
}
int  png_fseeki64( FILE * _File,  __int64 _Offset,  int _Origin)
{
    return _fseeki64(_File, _Offset, _Origin);
}
__int64  png_ftelli64( FILE * _File)
{
    return _ftelli64(_File);
}
int png_fflush(FILE * _File)
{
    return fflush(_File);
}
int png_fclose(FILE * _File)
{
    return fclose(_File);
}

