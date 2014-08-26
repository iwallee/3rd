#pragma once

#include <windows.h>
#include <math.h>

//four addation function
#define F(x, y, z) (((x) & (y)) | ((~x) & (z)))
#define G(x, y, z) (((x) & (z)) | ((y) & (~z)))
#define H(x, y, z) ((x) ^ (y) ^ (z))
#define I(x, y, z) ((y) ^ ((x) | (~z)))

//using for rotate x left n bit 
#define ROTATE_LEFT(x, n) (((x) << (n)) | ((x) >> (32-(n))))

//four transform for round 1, 2, 3 and 4
#define FF(a, b, c, d, index, RotateBit, SinValue)			\
{															\
	(a) += ( F((b),(c),(d)) + (index) + (SinValue) ) ;		\
	(a) = ROTATE_LEFT ((a), (RotateBit));					\
	(a) += (b) ;											\
}
#define GG(a, b, c, d, index, RotateBit, SinValue)			\
{															\
	(a) += ( G((b),(c),(d)) + (index) + (SinValue) ) ;		\
	(a) = ROTATE_LEFT ((a), (RotateBit));					\
	(a) += (b) ;											\
}
#define HH(a, b, c, d, index, RotateBit, SinValue)			\
{															\
	(a) += ( H((b),(c),(d)) + (index) + (SinValue) ) ;		\
	(a) = ROTATE_LEFT ((a), (RotateBit));					\
	(a) += (b) ;											\
}
#define II(a, b, c, d, index, RotateBit, SinValue)			\
{															\
	(a) += ( I((b),(c),(d)) + (index) + (SinValue) ) ;		\
	(a) = ROTATE_LEFT ((a), (RotateBit));					\
	(a) += (b) ;											\
}

//using for storing MD5_Init_CONTEXT, MD5_EachRound_CONTEXT 
//and also MD5_Final_CONTEXT ( eg: final hash value )
typedef struct 
{
	UINT A, B, C, D ;
} MD5_CONTEXT ;

//Initial the MD5_INIT_CONTEXT
void MD5_InitContext () ;

//Generate sin value and fill uSinValue array
void MD5_GenerateSinValue();

//caculate the number of groups as the text should be devide
__int64 GetGroups ( __int64 uMsgLengthInBit ) ;

//Check and fix each group of text,which contain 512 bits
BOOL CheckAndFixMessage ( UINT* Group32Value, BYTE* SourceMsg, 
						 __int64* uMsgLength, __int64 uTimes ) ;

//Caculate each group throught 4 round with FF,GG,HH,II
void MD5_Transform ( MD5_CONTEXT* MD5_Value, UINT* FixedMsg ) ;

//User interface: can use this interface for caculate MD5 value of target text
bool MD5_Caculate ( MD5_CONTEXT* MD5_Value, BYTE* SourceMsg, __int64 uMsgLength ) ;
/*
//User interface:	Caculate MD5 value of input string 
//					and return in CString style
CString MD5_Caculate_String( CString SourceMsg )
{
	MD5_CONTEXT	MD5_Value ;
	memset ( &MD5_Value, 0, sizeof(MD5_Value) ) ;

	CString MD5_Value_String = "" ;

	if ( MD5_Caculate( &MD5_Value, (byte*)SourceMsg.GetBuffer(SourceMsg.GetLength()), SourceMsg.GetLength()*8 ) )
	{
		CString TempString ;
		MD5_Value_String = "" ;
		for ( int i = 0; i < 16; i++ )
		{
			TempString.Format ( "%02X", ((byte*)&MD5_Value)[i] ) ;
			MD5_Value_String += TempString ;
		}
	}

	return MD5_Value_String ;
}

//User interface:	Caculate MD5 value of target file name 
//					and return in CString style
CString MD5_Caculate_File ( CString szFileName )
{
	CString MD5_Value_String = "" ;

	HANDLE hFile = CreateFile ( szFileName, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, \
		NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL ) ;
	if ( hFile == INVALID_HANDLE_VALUE )
	{
		MD5_Value_String.Format ( "CreateFile Error ( code:%d )!", GetLastError() );
		return MD5_Value_String;
	}

	HANDLE hFileMap = CreateFileMapping ( hFile, NULL, PAGE_READWRITE, 0, 0, NULL ) ;
	if ( hFileMap == NULL )
	{
		MD5_Value_String.Format ( "CreateFileMapping Error ( code:%d )!", GetLastError() );
		CloseHandle ( hFile ) ;
		return MD5_Value_String ;
	}

	PVOID pMapView = MapViewOfFile ( hFileMap, FILE_MAP_READ, 0, 0, 0 ) ;
	if ( pMapView == NULL )
	{
		MD5_Value_String.Format ( "MapViewOfFile Error ( code:%d )!", GetLastError() );
		CloseHandle ( hFileMap ) ;
		CloseHandle ( hFile ) ;
		return MD5_Value_String ;
	}

	DWORD dwLowDWord = 0, dwHighDWord = 0 ;
	dwLowDWord = GetFileSize ( hFile, &dwHighDWord ) ;

	UINT64 uFileSize = ( dwHighDWord << 32 ) + dwLowDWord ;

	MD5_CONTEXT	MD5_Value ;
	memset ( &MD5_Value, 0, sizeof(MD5_Value) ) ;

	if ( MD5_Caculate( &MD5_Value, (BYTE*)pMapView, uFileSize*8) )
	{
		CString TempString ;
		MD5_Value_String = "" ;
		for ( int i = 0; i < 16; i++ )
		{
			TempString.Format ( "%02X", ((byte*)&MD5_Value)[i] ) ;
			MD5_Value_String += TempString ;
		}
	}

	UnmapViewOfFile ( pMapView ) ;
	CloseHandle ( hFileMap ) ;
	CloseHandle ( hFile ) ;

	return MD5_Value_String ;
}
*/