#ifndef PNG_IO_H
#define PNG_IO_H

#ifdef __cplusplus
extern "C" {
#endif
    PNG_EXPORT(301, FILE *, png_fopen,(const char * _Filename, const char * _Mode));
    PNG_EXPORT(302, errno_t, png_fopen_s,(FILE ** _File, const char * _Filename, const char * _Mode));
    PNG_EXPORT(303, size_t, png_fread,(void * _DstBuf, size_t _ElementSize, size_t _Count, FILE * _File));
    PNG_EXPORT(304, size_t, png_fread_s,(void * _DstBuf, size_t _DstSize, size_t _ElementSize, size_t _Count, FILE * _File));
    PNG_EXPORT(305, int, png_fseek,(FILE * _File, long _Offset, int _Origin));
    PNG_EXPORT(306, long, png_ftell,(FILE * _File));
    PNG_EXPORT(307, int, png_fseeki64,(FILE * _File,  __int64 _Offset,  int _Origin));
    PNG_EXPORT(308, __int64, png_ftelli64,(FILE * _File));
    PNG_EXPORT(309, int, png_fflush, (FILE * _File));
    PNG_EXPORT(310, int, png_fwrite, (FILE * _File));
    PNG_EXPORT(311, int, png_ungetc, (FILE * _File));
    PNG_EXPORT(312, int, png_fclose,(FILE * _File));
#ifdef __cplusplus
}
#endif
#endif /* PNG_IO_H */
