

#include "Gifenc.h"

static const unsigned long code_mask[] = { 0x0000, 0x0001, 0x0003, 0x0007, 0x000F,
0x001F, 0x003F, 0x007F, 0x00FF,
0x01FF, 0x03FF, 0x07FF, 0x0FFF,
					   0x1FFF, 0x3FFF, 0x7FFF, 0xFFFF };

GifEncObj::GifEncObj()
{

	 /*m_hOutFile		= NULL;
	 m_init_bits		= 0;
	 m_nAccum		= 0;
         m_nCurbits		= 0;
	 m_nMaxcode		= 0;
	 m_nDictionaryPos	= 0;
	 m_nTransInx		= 0;	
	 m_nclear_flg		= 0;
	 m_nClearCode		= 0;
	 m_nEOFCode		= 0;
	 m_nEncCount		= 0;
	 m_nTotalCount		= 0;
	 m_nbits		= 0;
	
	ZeroMemory(m_htab,sizeof(m_htab));
	ZeroMemory(m_codetab,sizeof(m_codetab));
	ZeroMemory(m_szAccum,sizeof(m_szAccum));*/

}
GifEncObj::~GifEncObj()
{

}

void GifEncObj::clear_hash(long hsize)
{
	memset(m_htab,0xff,sizeof(m_htab));
	memset(m_codetab,0,sizeof(m_codetab));
}



void GifEncObj::flush_char()
{
	if (m_nEncCount > 0) 
		{
		_lwrite(m_hOutFile,(LPCSTR)&m_nEncCount,1);
		_lwrite(m_hOutFile,(LPCSTR)m_szAccum,m_nEncCount);
		
		m_nTotalCount += m_nEncCount;
		m_nEncCount=0;
		}
}

//×Ö·û´®ÀÛ¼ÓÊä³ö
void GifEncObj::char_out(int c)
{
	m_szAccum[m_nEncCount++]=(char)c;

	if(m_nEncCount >= OUT_MAX_LEN )
		{
		flush_char();
		}
}

//Êä³öÑ¹ËõÂë
void GifEncObj::output(code_int code)
{


	m_nAccum &= code_mask[m_nCurbits];	
	if( m_nCurbits > 0 )
		{
		m_nAccum |= ((long)code << m_nCurbits);
		}
	else	{
		m_nAccum = code;
	
		}
	

	m_nCurbits += m_nbits;	


	while( m_nCurbits >= 8 ) 
		{
		char_out( (unsigned int)(m_nAccum & 0xff) );
		m_nAccum >>= 8;
		m_nCurbits -= 8;
		}
	
	/*
	* If the next entry is going to be too big for the code size,
	* then increase it, if possible.
	*/
	
	if ( m_nDictionaryPos > m_nMaxcode || m_nclear_flg ) 
		{
		if( m_nclear_flg ) 
			{
			m_nMaxcode = (short)MAXCODE(m_nbits = m_init_bits);
			m_nclear_flg = 0;
			} 
		else	{
			m_nbits ++;
			if ( m_nbits == MAXBITSCODES )
				m_nMaxcode = (code_int)1 << MAXBITSCODES;		/* should NEVER generate this code */
			else
				m_nMaxcode = (short)MAXCODE(m_nbits);
			}	
		}
	
	if( code == m_nEOFCode ) 
		{
		// At EOF, write the rest of the buffer
	
 		//ÁÙÊ±´úÂë,ÊÔÍ¼½â¾öÄ³Ð©GifÍ¼Æ¬ÔÚwindowsÍ¼Æ¬ä¯ÀÀÆ÷²¥·Å»á¿¨µÄÇé¿ö
		/*if(m_nEncCount < 2)
 			{			
 			//PrintTrace("m_nCurbits=%d,m_nAccum=%d,m_nEncCount=%d",m_nCurbits,m_nAccum,m_nEncCount);
 			m_nCurbits	= 0;
 			m_nEncCount	= 0;
 			}*/
		
		while( m_nCurbits > 0 ) 
			{
			int nCodec = m_nAccum & 0xff;

			m_nAccum >>= 8;
			m_nCurbits -= 8;

			//if(nCodec == 0) break;
			char_out(nCodec);
			}
			
		flush_char();			
		}

}
/*Êý¾ÝÑ¹Ëõ*/
int GifEncObj::CompressLZW(int init_bits, LPBYTE lpPixel,INT nPixelNum ,HFILE hOutFile)
{
	code_int	nDictionaryLen;
	long		fcode;
	long		c;
	long		nPrefix;	
	long		hshift;
	long		disp;
	long		i;
	int		nCount = 0;
	
	
	m_init_bits	= init_bits;
	m_hOutFile	= hOutFile;	
	m_nTotalCount	= 0;
	
	// Set up the necessary values
	m_nAccum = m_nCurbits = m_nclear_flg = 0;
	m_nMaxcode	= (short)MAXCODE(m_nbits = m_init_bits);
	nDictionaryLen	= MAX_CODE_LEN;//(code_int)1 << MAXBITSCODES;
	
	m_nClearCode	 = (1 << (init_bits - 1));
	m_nEOFCode		 = m_nClearCode + 1;
	m_nDictionaryPos = (short)(m_nClearCode + 2);
	
	m_nEncCount	= 0;
	nPrefix		= lpPixel[0];	
	hshift		= 0;
	
	for (fcode = (long)HSIZE;  fcode < 65536L; fcode *= 2L )
		++hshift;
	
	hshift = 8 - hshift;       /* set hash code range bound */
	clear_hash(HSIZE);        /* clear hash table */
	output((code_int)m_nClearCode);
	
	for(INT j=1;j<nPixelNum;j++)	
	{    
		c = lpPixel[j];
		
		fcode = (long) (((long) c << MAXBITSCODES) + nPrefix);
		i = (((code_int)c << hshift) ^ nPrefix);    /* xor hashing */
		
		//if(i>=HSIZE) break;			 
		if(m_htab[i] == fcode) 
		{
			nPrefix = m_codetab[i];
			continue;
		} 
		else if((long)m_htab[i] < 0 )      /* empty slot */
		{
			goto nomatch;
		}
		
		disp = HSIZE - i;           /* secondary hash (after G. Knott) */
		if(i == 0 ) disp = 1;
		
probe:
		if((i -= disp) < 0)
		{
			i += HSIZE;
		}
		if(m_htab[i] == fcode) 
		{
			nPrefix = m_codetab[i]; 
			continue; 
		}
		
		if((long)m_htab[i] > 0)	
		{
			goto probe;
		}
nomatch:
		output((code_int)nPrefix);
		nPrefix = c;
		if(m_nDictionaryPos < nDictionaryLen) 
		{  
			m_codetab[i] = m_nDictionaryPos++; /* code -> hasm_htable */
			m_htab[i] = fcode;
		} 
		else	{
			clear_hash(HSIZE);
			m_nDictionaryPos=(short)(m_nClearCode+2);
			m_nclear_flg=1;
			output((code_int)m_nClearCode);
			
			nCount ++;		
		}
	}
	
	//PrintTrace("nPrefix=%d,CurPos=%d,%d,nCount=%d,m_nEncCount=%d",nPrefix,m_nDictionaryPos,nDictionaryLen,nCount,m_nEncCount);
	//if(m_nEncCount < 32)
	//	m_nEncCount = 0;
	
	// Put out the final code.
	output( (code_int)nPrefix);
	output( (code_int)m_nEOFCode);
	
	return nPrefix;
}