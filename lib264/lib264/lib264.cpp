// lib264.cpp : 定义 DLL 应用程序的导出函数。
//

#include "stdafx.h"
#include <windows.h>
#include "lib264.h"

typedef __int64 int64_t;

#pragma comment(lib, "libgcc.a")
#pragma comment(lib, "libmingwex.a")
#pragma comment(lib, "libavcodec.a")
#pragma comment(lib, "libavutil.a")
#pragma comment(lib, "libswscale.a")
#pragma comment(lib, "libx264.a")


extern "C"
{
    void* x264_encoder_open2();
    int x264_set_basic_config2(void * ahandle,
                               unsigned int auWidth, unsigned int auHeight, unsigned int auColorBit,
                               unsigned int auKbps, unsigned int auFps, unsigned int auKeyInterval,
                               int aiQualityMode);
    void x264_encoder_close2(void * ahandle);
    int x264_encoder_encode2(void * ahandle,
                             const unsigned char * apSrc,
                             unsigned char * apOut, unsigned int * auOutLen /* in out */,
                             int * auIsKeyFrame /* in out */,
                             int64_t * ai_dts, int64_t * ai_pts);
    void x264_get_sps_pps(void * ahandle,
                          void ** apSps, unsigned int * auSpsLen,
                          void ** apPps, unsigned int * auPpsLen);

    void * h264_decoder_open();
    int h264_set_decode_config(void * ahandle,
                               const char * apSps, const unsigned int auSpsLen,
                               const char * apPps, const unsigned int auPpsLen,
                               const unsigned int auWidth, const unsigned int auHeight,
                               const unsigned int auColorBit, const int abIsBGROrder);
    void h264_decoder_close(void * ahandle);
    int h264_decoder_decode(void * ahandle,
                            const unsigned char * apSrc,
                            const unsigned int auSrcLen,
                            unsigned char * apOut,
                            unsigned int * auOutLen /* in out */);
}

extern "C" {
    void* c264_encoder_open()
    {
        return x264_encoder_open2();
    }
    int c264_encoder_config(void * ahandle,
                       unsigned int auWidth, unsigned int auHeight, unsigned int auColorBit,
                       unsigned int auKbps, unsigned int auFps, unsigned int auKeyInterval,
                       int aiQualityMode)
    {
        return x264_set_basic_config2(ahandle,
                                      auWidth, auHeight, auColorBit,
                                      auKbps, auFps, auKeyInterval,
                                      aiQualityMode);
    }
    void c264_encoder_close(void * ahandle)
    {
        x264_encoder_close2(ahandle);
    }
    int c264_encoder_encode(void * ahandle,
                       const unsigned char * apSrc,
                       unsigned char * apOut, unsigned int * auOutLen /* in out */,
                       int * auIsKeyFrame /* in out */,
                       long long * ai_dts, long long * ai_pts)
    {
        return x264_encoder_encode2(ahandle,
                                    apSrc,
                                    apOut, auOutLen ,
                                    auIsKeyFrame ,
                                    (int64_t*)ai_dts, (int64_t*)ai_pts);
    }
    void c264_encoder_get_sps_pps(void * ahandle,
                             void ** apSps, unsigned int * auSpsLen,
                             void ** apPps, unsigned int * auPpsLen)
    {
        x264_get_sps_pps(ahandle,
                         apSps, auSpsLen,
                         apPps, auPpsLen);
    }

    void * c264_decoder_open()
    {
        return h264_decoder_open();
    }
    int c264_decoder_config(void * ahandle,
                       const char * apSps, const unsigned int auSpsLen,
                       const char * apPps, const unsigned int auPpsLen,
                       const unsigned int auWidth, const unsigned int auHeight,
                       const unsigned int auColorBit, const int abIsBGROrder)
    {
        return h264_set_decode_config(ahandle,
                                      apSps, auSpsLen,
                                      apPps, auPpsLen,
                                      auWidth,  auHeight,
                                      auColorBit,  abIsBGROrder);
    }
    void c264_decoder_close(void * ahandle)
    {
        h264_decoder_close(ahandle);
    }
    int c264_decoder_decode(void * ahandle,
                       const unsigned char * apSrc,
                       const unsigned int auSrcLen,
                       unsigned char * apOut,
                       unsigned int * auOutLen /* in out */)
    {
        return h264_decoder_decode(ahandle,
                                   apSrc,
                                   auSrcLen,
                                   apOut,
                                   auOutLen);
    }
}