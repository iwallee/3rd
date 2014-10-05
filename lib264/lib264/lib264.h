#ifndef LIB264_H
#define LIB264_H

#ifdef LIB264_EXPORTS
#define LIB264_API __declspec(dllexport)
#else
#define LIB264_API
#endif

extern "C" {
    LIB264_API void* encoder_open();
    LIB264_API int encoder_config(void * ahandle,
                                  unsigned int auWidth, unsigned int auHeight, unsigned int auColorBit,
                                  unsigned int auKbps, unsigned int auFps, unsigned int auKeyInterval,
                                  int aiQualityMode);
    LIB264_API void encoder_close(void * ahandle);
    LIB264_API int encoder_encode(void * ahandle,
                                  const unsigned char * apSrc,
                                  unsigned char * apOut, unsigned int * auOutLen /* in out */,
                                  int * auIsKeyFrame /* in out */,
                                  long long * ai_dts, long long * ai_pts);
    LIB264_API void encoder_get_sps_pps(void * ahandle,
                                        void ** apSps, unsigned int * auSpsLen,
                                        void ** apPps, unsigned int * auPpsLen);

    LIB264_API void * decoder_open();
    LIB264_API int decoder_config(void * ahandle,
                                  const char * apSps, const unsigned int auSpsLen,
                                  const char * apPps, const unsigned int auPpsLen,
                                  const unsigned int auWidth, const unsigned int auHeight,
                                  const unsigned int auColorBit, const int abIsBGROrder);
    LIB264_API void decoder_close(void * ahandle);
    LIB264_API int decoder_decode(void * ahandle,
                                  const unsigned char * apSrc,
                                  const unsigned int auSrcLen,
                                  unsigned char * apOut,
                                  unsigned int * auOutLen /* in out */);
}

#endif