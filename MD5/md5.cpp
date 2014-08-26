#include <windows.h>
#include "MD5.h"


MD5_CONTEXT MD5_Context ;

//addation array, show the bits to left rotate
unsigned short LeftRotateBits[4][4] = {
	7, 12, 17, 22, 
	5,  9, 14, 20, 
	4,	11, 16, 23,
	6, 10, 15, 21	
} ;

//store abs(sin(n)*4294967296) with n between 1 and 64 
UINT uSinValue[64] = {0} ;

//Initial the MD5_INIT_CONTEXT
void MD5_InitContext ()
{
	MD5_Context.A = 0x67452301;
	MD5_Context.B = 0xefcdab89;
	MD5_Context.C = 0x98badcfe;
	MD5_Context.D = 0x10325476;
}

//Generate sin value and fill uSinValue array
void MD5_GenerateSinValue()
{
	__int64 gene = 4294967296 ;

	int iSinIndex ;
	double temp ;
	for ( iSinIndex = 1; iSinIndex <= 64; iSinIndex++ )
	{
		temp = sin( iSinIndex*1.0 ) ;
		temp = ( temp < 0 ) ? ( temp * (-1) ) : temp ;
		uSinValue[iSinIndex-1] = (UINT)( temp * gene ) ;
	}
}

//caculate the number of groups as the text should be devide
__int64 GetGroups ( __int64 uMsgLengthInBit )
{
	__int64 uGroups = uMsgLengthInBit / 512 ;
	__int64 rem	 = uMsgLengthInBit % 512 ;

	if ( rem >= 448 )
		uGroups += 2 ;
	else
		uGroups ++ ;

	return uGroups ;
}

//Check and fix each group of text,which contain 512 bits
BOOL CheckAndFixMessage ( UINT* Group32Value, BYTE* SourceMsg, 
						 __int64* uMsgLength, __int64 uTimes )
{
	BYTE FixedMsg[64] = {0} ;
	__int64 uMsgLen = *uMsgLength / 8 - uTimes * 64 ;

	if ( uMsgLen >= 64 )
	{
		memcpy ( FixedMsg, &SourceMsg[uTimes*64], 64 ) ;
	}
	else if ( uMsgLen >= 56 )
	{
		memcpy ( FixedMsg, &SourceMsg[uTimes*64], (UINT)uMsgLen ) ;
		FixedMsg[uMsgLen] = 0x80 ;
	}
	else 
	{
		if ( uMsgLen >= 0 )
		{
			memcpy ( FixedMsg, &SourceMsg[uTimes*64], (UINT)uMsgLen ) ;

			if ( uMsgLen < 56 )
				FixedMsg[uMsgLen] = 0x80 ;
		}

		for ( int i = 56; i < 64; i++, ( (*uMsgLength) /= 256 ) )
			FixedMsg[i] = (UINT)( *uMsgLength & 0xFF ) ;
	}

	int i, j = 0 ;
	for ( i = 0; i < 16; i++, j += 4 )
	{
		Group32Value[i] = FixedMsg[j] + ( FixedMsg[j+1] << 8 ) + \
			( FixedMsg[j+2] << 16 ) + ( FixedMsg[j+3] << 24 ) ;
	}

	return true ;
}

//Caculate each group throught 4 round with FF,GG,HH,II
void MD5_Transform ( MD5_CONTEXT* MD5_Value, UINT* FixedMsg )
{
	UINT TempValue[4] = { 
		MD5_Context.A, 
		MD5_Context.B, 
		MD5_Context.C, 
		MD5_Context.D 
	} ;

	short int a = 0, b = 1, c = 2, d = 3, index = 0, pace = 1 ;
	UINT iRoundIndex = 1, iOperateIndex ;
	for ( iOperateIndex = 0; iOperateIndex < 16; iOperateIndex++ )
	{
		FF(TempValue[a], TempValue[b], TempValue[c], TempValue[d],	\
			FixedMsg[index],LeftRotateBits[0][iOperateIndex%4], \
			uSinValue[iOperateIndex] ) ;

		a--; b--; c--; d-- ;

		a = ( a < 0 ) ? (a += 4) : a ;
		b = ( b < 0 ) ? (b += 4) : b ;
		c = ( c < 0 ) ? (c += 4) : c ;
		d = ( d < 0 ) ? (d += 4) : d ;

		index += pace ;
		index = ( index > 15 ) ? ( index - 16 ) : index ;
	}

	iRoundIndex = 2 ;
	a = 0, b = 1, c = 2, d = 3, index = 1, pace = 5 ;
	for ( iOperateIndex = 0; iOperateIndex < 16; iOperateIndex++ )
	{
		GG(TempValue[a], TempValue[b], TempValue[c], TempValue[d],	\
			FixedMsg[index],LeftRotateBits[1][iOperateIndex%4], \
			uSinValue[16+iOperateIndex] ) ;

		a--; b--; c--; d-- ;

		a = ( a < 0 ) ? (a += 4) : a ;
		b = ( b < 0 ) ? (b += 4) : b ;
		c = ( c < 0 ) ? (c += 4) : c ;
		d = ( d < 0 ) ? (d += 4) : d ;

		index += pace ;
		index = ( index > 15 ) ? ( index - 16 ) : index ;
	}

	iRoundIndex = 3 ;
	a = 0, b = 1, c = 2, d = 3, index = 5, pace = 3 ;
	for ( iOperateIndex = 0; iOperateIndex < 16; iOperateIndex++ )
	{
		HH(TempValue[a], TempValue[b], TempValue[c], TempValue[d],	\
			FixedMsg[index],LeftRotateBits[2][iOperateIndex%4], \
			uSinValue[32+iOperateIndex] ) ;

		a--; b--; c--; d-- ;

		a = ( a < 0 ) ? (a += 4) : a ;
		b = ( b < 0 ) ? (b += 4) : b ;
		c = ( c < 0 ) ? (c += 4) : c ;
		d = ( d < 0 ) ? (d += 4) : d ;

		index += pace ;
		index = ( index > 15 ) ? ( index - 16 ) : index ;
	}

	iRoundIndex = 4 ;
	a = 0, b = 1, c = 2, d = 3, index = 0, pace = 7 ;
	for ( iOperateIndex = 0; iOperateIndex < 16; iOperateIndex++ )
	{
		II(TempValue[a], TempValue[b], TempValue[c], TempValue[d],	\
			FixedMsg[index],LeftRotateBits[3][iOperateIndex%4], \
			uSinValue[48+iOperateIndex] ) ;

		a--; b--; c--; d-- ;

		a = ( a < 0 ) ? (a += 4) : a ;
		b = ( b < 0 ) ? (b += 4) : b ;
		c = ( c < 0 ) ? (c += 4) : c ;
		d = ( d < 0 ) ? (d += 4) : d ;

		index += pace ;
		index = ( index > 15 ) ? ( index - 16 ) : index ;
	}

	MD5_Context.A += TempValue[0] ;
	MD5_Context.B += TempValue[1] ;
	MD5_Context.C += TempValue[2] ;
	MD5_Context.D += TempValue[3] ;
}

//User interface: can use this interface for caculate MD5 value of target text
bool MD5_Caculate ( MD5_CONTEXT* MD5_Value, BYTE* SourceMsg, __int64 uMsgLength )
{
	if ( uMsgLength == 0 )
		return false ;
	else
		memset ( MD5_Value, 0, sizeof(MD5_CONTEXT) ) ;

	MD5_InitContext() ;
	MD5_GenerateSinValue() ;
	__int64 uGroups = GetGroups ( uMsgLength ) ;

	UINT	uGroupsIndex , Group32Value[16] = {0} ;
	for ( uGroupsIndex = 0; uGroupsIndex < uGroups; uGroupsIndex++ )
	{	
		CheckAndFixMessage ( Group32Value, SourceMsg, &uMsgLength, uGroupsIndex ) ;
		MD5_Transform ( MD5_Value, Group32Value ) ;
	}

	memcpy ( MD5_Value, &MD5_Context, sizeof(MD5_CONTEXT) ) ;
	return true ;
}