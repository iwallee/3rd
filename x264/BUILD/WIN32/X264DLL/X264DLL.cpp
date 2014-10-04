// X264DLL.cpp : Defines the entry point for the DLL application.
//

#include "stdafx.h"
#include "X264DLL.h"

#include "../../../extras/stdint.h"
#include "../../../x264.h"
#include <STDIO.H>

//#pragma comment(lib, "libx264.lib")

BOOL APIENTRY DllMain( HANDLE hModule, 
                       DWORD  ul_reason_for_call, 
                       LPVOID lpReserved
					 )
{
    switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
		case DLL_THREAD_ATTACH:
		case DLL_THREAD_DETACH:
		case DLL_PROCESS_DETACH:
			break;
    }
    return TRUE;
}


X264DLL_API void* __cdecl encoder_open(int aiFps)
{
	return x264_encoder_open(aiFps);
}
X264DLL_API void  __cdecl encoder_close  ( void * h )
{
	 x264_encoder_close(h);
	 return;
}
X264DLL_API  int __cdecl EnCode(void *h,char *apSrcData,int aiSrcDataLen,char *apDstBuff,int *aiDstBuffLen,int *abMark)
{
	return x264_encode_frame(h, apSrcData, aiSrcDataLen, apDstBuff, aiDstBuffLen, abMark);
}

X264DLL_API  void* __cdecl decoder_open()
{
	return x264_decoder_open();
}
X264DLL_API  int __cdecl DeCode(void *h,char *apSrcData,int aiSrcDataLen,char *apDstBuff,int *aiDstBuffLen,int *abMark)
{
	return DeCodec(h, apSrcData, aiSrcDataLen, apDstBuff, aiDstBuffLen, abMark);	
}
X264DLL_API  void __cdecl decode_close(void *h)
{
	x264_decoder_close(h);
	return;
}

X264DLL_API  int __cdecl SetFrameInfo(int aiFrameWidth,int aiFrameHeight,int aiBitCount,int abIsEncode, void *h)
{
	return SetFrameInfoInitial(aiFrameWidth, aiFrameHeight, aiBitCount,abIsEncode, h);
}
X264DLL_API int __cdecl SetFrmQuant(int aiQuantVal,void *h)
{
	return SetFrameQuant(aiQuantVal,h);
}
