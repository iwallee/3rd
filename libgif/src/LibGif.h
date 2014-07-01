
#pragma once 

#include <windows.h>
#include <math.h>
#include <STDIO.h>
#include "Quantizer.h"




typedef struct _GIFENC_INFO
{
	INT	nWidth;
	INT	nHeight;
	HFILE	hFileObj;
	LPBYTE  pLastBuf;
	LPBYTE  pColorIndex;
	BYTE	szLocalclut[1024];	
	
}GIFENC_INFO,*LPGIFENC_INFO;

//13 byte
typedef struct _GIFHEADER
{
	BYTE Signature[6];
	WORD ScreenWidth;
	WORD ScreenHeight;
	BYTE GlobalFlagByte;
	BYTE BackGroundColor;
	BYTE AspectRatio;	
}GIFHEARD;
//#define ENC_BUF_LEN sizeof(WORD)*10000000

//10 byte
typedef struct _GIFDATAHEARD
{
	BYTE imageLabel;
	WORD imageLeft;
	WORD imageTop;
	WORD imageWidth;
	WORD imageHeight;
	BYTE localFlagByte;
}GIFDATAHEARD;

typedef struct _GraphicController
{
	BYTE extensionIntroducer;
	BYTE graphicControlLabel;
	BYTE blockSize;
	BYTE packedField;
	WORD nDelayTime;
	BYTE transparentColorIndex;
	BYTE blockTerminator;
}GraphicController;

typedef struct _ApplicationExtension
{
	BYTE extensionIntroducer;
	BYTE applicationLabel;
	BYTE blockSize;
	char applicationId[8];
	char appAuthCode[3];
	char cAppData[4];
	BYTE blockTerminator;
	
}ApplicationExtension;

int PrintTrace(char *format, ...)
{ 
	va_list ap; 
	char buf[4096];
	
	va_start(ap, format); 
	
    int n = vsprintf_s(buf, _countof(buf), format, ap);
	
	OutputDebugStringA(buf);
	OutputDebugStringA("\n");
	
	va_end(ap); 
	
	return n;
}