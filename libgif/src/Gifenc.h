/*
	Gif 数据压缩处理类

*/

#include <windows.h>

#pragma once 

#define FOUR_ALIGN(x)		((x+3)>>2<<2) //数据4字节对齐(向上)
#define HASPalette		0x80
#define COLORS_BITS		7
#define TRANS_COLOR		(1<<COLORS_BITS)

#define MAXBITSCODES		12//12
#define MAXCODE(g_nbits)	(((code_int) 1 << (m_nbits)) - 1)
#define HSIZE			5003//5003 
#define Hasm_htabOf(i)		m_htab[i]
#define m_codetabOf(i)		m_codetab[i]
#define MAX_CODE_LEN		(code_int)1<< MAXBITSCODES;		
#define OUT_MAX_LEN		254

typedef short int code_int;

class GifEncObj
{
	public:

		GifEncObj();
		~GifEncObj();

		int CompressLZW(int init_bits, LPBYTE lpPixel,INT nPixelNum ,HFILE hOutFile);
	private:
		void flush_char();
		void char_out(int c);
		void output(code_int code);
		void clear_hash(long hsize);

	private:
		HFILE		 m_hOutFile;
		int		 m_init_bits;
		unsigned long    m_nAccum;
		int              m_nCurbits;
		code_int	 m_nMaxcode;
		code_int	 m_nDictionaryPos;
		int		 m_nTransInx;	
		int		 m_nclear_flg;
		int		 m_nClearCode;
		int		 m_nEOFCode;
		int		 m_nEncCount;
		int		 m_nTotalCount;
		int		 m_nbits;		/* number of bits/code */
		long		 m_htab[HSIZE];
		unsigned short   m_codetab[HSIZE];	//
		char		 m_szAccum[256];	//256
};