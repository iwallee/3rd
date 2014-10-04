
// The following ifdef block is the standard way of creating macros which make exporting 
// from a DLL simpler. All files within this DLL are compiled with the X264DLL_EXPORTS
// symbol defined on the command line. this symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see 
// X264DLL_API functions as being imported from a DLL, wheras this DLL sees symbols
// defined with this macro as being exported.
#ifdef X264DLL_EXPORTS
#define X264DLL_API __declspec(dllexport)
#else
#define X264DLL_API __declspec(dllimport)
#endif

X264DLL_API void* __cdecl encoder_open(int aiFps);
X264DLL_API void __cdecl encoder_close  ( void * h );
X264DLL_API int  __cdecl EnCode(void *h,char *apSrcData,int aiSrcDataLen,char *apDstBuff,int *aiDstBuffLen,int *abMark);

X264DLL_API void* __cdecl decoder_open();
X264DLL_API int  __cdecl DeCode(void *h,char *apSrcData,int aiSrcDataLen,char *apDstBuff,int *aiDstBuffLen,int *abMark);
X264DLL_API void __cdecl decode_close  ( void *h );

X264DLL_API int  __cdecl SetFrameInfo(int aiFrameWidth,int aiFrameHeight,int aiBitCount,int abIsEncode, void *h);
X264DLL_API int  __cdecl SetFrmQuant(int aiQuantVal,void *h);

