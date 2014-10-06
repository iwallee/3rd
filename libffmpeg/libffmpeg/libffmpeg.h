#ifndef LIBFFMPEG_H
#define LIBFFMPEG_H

#ifdef LIBFFMPEG_EXPORTS
#define LIBFFMPEG_API __declspec(dllexport)
#else
#define LIBFFMPEG_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include "ffmpeg/include/libswscale/swscale.h"

    LIBFFMPEG_API void* c264_encoder_open();
    LIBFFMPEG_API int c264_encoder_config(void * ahandle,
                                  unsigned int auWidth, unsigned int auHeight, unsigned int auColorBit,
                                  unsigned int auKbps, unsigned int auFps, unsigned int auKeyInterval,
                                  int aiQualityMode);
    LIBFFMPEG_API void c264_encoder_close(void * ahandle);
    LIBFFMPEG_API int c264_encoder_encode(void * ahandle,
                                  const unsigned char * apSrc,
                                  unsigned char * apOut, unsigned int * auOutLen /* in out */,
                                  int * auIsKeyFrame /* in out */,
                                  long long * ai_dts, long long * ai_pts);
    LIBFFMPEG_API void c264_encoder_get_sps_pps(void * ahandle,
                                        void ** apSps, unsigned int * auSpsLen,
                                        void ** apPps, unsigned int * auPpsLen);

    LIBFFMPEG_API void* c264_decoder_open();
    LIBFFMPEG_API int c264_decoder_config(void * ahandle,
                                  const char * apSps, const unsigned int auSpsLen,
                                  const char * apPps, const unsigned int auPpsLen,
                                  const unsigned int auWidth, const unsigned int auHeight,
                                  const unsigned int auColorBit, const int abIsBGROrder);
    LIBFFMPEG_API void c264_decoder_close(void * ahandle);
    LIBFFMPEG_API int c264_decoder_decode(void * ahandle,
                                  const unsigned char * apSrc,
                                  const unsigned int auSrcLen,
                                  unsigned char * apOut,
                                  unsigned int * auOutLen /* in out */);

#ifdef __cplusplus
}
#endif


#endif
