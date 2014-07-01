/*
	Color  
*/

#include <Windows.h>
#include "Quantizer.h"
#include <MATH.H>

/////////////////////////////////////////////////////////////////////////////
CQuantizer::CQuantizer (UINT nMaxColors, UINT nColorBits)
{
	m_nColorBits = nColorBits < 8 ? nColorBits : 8;
	
	m_pTree	= NULL;
	m_nLeafCount = 0;
	for(int i=0; i<=(int) m_nColorBits; i++)
		m_pReducibleNodes[i] = NULL;

	m_nMaxColors = nMaxColors;
}

/////////////////////////////////////////////////////////////////////////////
CQuantizer::~CQuantizer	()
{
	if (m_pTree	!= NULL)
		DeleteTree (&m_pTree);
}

/////////////////////////////////////////////////////////////////////////////
BOOL CQuantizer::ProcessImage (BYTE* lpRgb24,int nWidth,int nHeight)
{
	BYTE	r,g, b;
	int	i, j;
	int	nPad = 0;	
	int	linesize = nWidth*3;
		
	if(linesize%4)
		{
		nPad = 4-(linesize%4);
		}
	
	BYTE* pbBits	= lpRgb24;
	for(i=0; i<nHeight;i++) 
		{
		for (j=0; j<nWidth; j++)	
			{
			b =	*pbBits++;
			g =	*pbBits++;
			r =	*pbBits++;

			AddColor (&m_pTree,r, g, b, m_nColorBits, 0, &m_nLeafCount,m_pReducibleNodes);

			while (m_nLeafCount	> m_nMaxColors)
				ReduceTree (m_nColorBits, &m_nLeafCount, m_pReducibleNodes);
			}

			pbBits += nPad;
		}

	return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
void CQuantizer::AddColor (NODE** ppNode, BYTE r, BYTE g, BYTE b,
			   UINT nColorBits, UINT nLevel, UINT*	pLeafCount,	NODE** pReducibleNodes)
{
	static BYTE	mask[8]	= {0x80, 0x40,	0x20, 0x10,	0x08, 0x04,	0x02, 0x01 };
	
	// If the node doesn't exist, create it.
	if (*ppNode	== NULL)
		{
		*ppNode	= (NODE*)CreateNode (nLevel, nColorBits, pLeafCount, pReducibleNodes);
		}
	
	// Update color	information	if it's	a leaf node.
	if ((*ppNode)->bIsLeaf)	
		{
		(*ppNode)->nPixelCount++;
		(*ppNode)->nRedSum += r;
		(*ppNode)->nGreenSum +=	g;
		(*ppNode)->nBlueSum	+= b;
		} 
	else 
		{
		// Recurse a level deeper if the node is not a leaf.
		int	shift =	7 - nLevel;
		int	nIndex =(((r & mask[nLevel]) >> shift) << 2) |
			(((g & mask[nLevel]) >>	shift) << 1) |
			(( b & mask[nLevel]) >> shift);
		
		AddColor(&((*ppNode)->pChild[nIndex]),	r, g, b, nColorBits,
			nLevel + 1,	pLeafCount,	pReducibleNodes);
		}
}

/////////////////////////////////////////////////////////////////////////////
void* CQuantizer::CreateNode (UINT nLevel, UINT	nColorBits,	UINT* pLeafCount,
			      NODE** pReducibleNodes)
{
	NODE* pNode = (NODE*)LocalAlloc(LPTR,sizeof(NODE));
	
	if (pNode== NULL) return NULL;
	
	pNode->bIsLeaf = (nLevel == nColorBits)	? TRUE : FALSE;
	if (pNode->bIsLeaf) 
		{
		(*pLeafCount)++;
		}
	else	{
		pNode->pNext = pReducibleNodes[nLevel];
		pReducibleNodes[nLevel]	= pNode;
		}

	return pNode;
}

/////////////////////////////////////////////////////////////////////////////
void CQuantizer::ReduceTree	(UINT nColorBits, UINT*	pLeafCount,NODE** pReducibleNodes)
{
	// Find	the	deepest	level containing at	least one reducible	node.
	for (int i=nColorBits - 1; (i>0) &&	(pReducibleNodes[i] == NULL); i--);
	
	// Reduce the node most	recently added to the list at level	i.
	NODE* pNode	= pReducibleNodes[i];
	pReducibleNodes[i] = pNode->pNext;
	
	NODE* pNodeTmp = pNode;
	NODE* pNeedReduceNode = NULL,*pPreNode = NULL,*pPreNodeTmp = NULL;
	long nMinPixelCount = 0;
	
	while(pNodeTmp)
	{
		long nPixelCount = 0;
		for(int m=0; m<8; m++)	
		{
			if (pNodeTmp->pChild[m] !=NULL) 
			{
				nPixelCount += pNodeTmp->pChild[m]->nPixelCount;
			}
		}
		
		if(nMinPixelCount == 0)
		{
			nMinPixelCount = nPixelCount;
			pNeedReduceNode = pNodeTmp;
		}
		else if(nPixelCount < nMinPixelCount)
		{
			nMinPixelCount = nPixelCount;
			pNeedReduceNode = pNodeTmp;
			pPreNode = pPreNodeTmp;
		}
		
		pPreNodeTmp = pNodeTmp;
		pNodeTmp = pNodeTmp->pNext;
	}
	
	if(pPreNode)
	{
		pPreNode->pNext = pNeedReduceNode->pNext;
		pReducibleNodes[i] = pNode;
	}
	
	pNode = pNeedReduceNode;
	
	UINT nRedSum = 0;
	UINT nGreenSum = 0;
	UINT nBlueSum =	0;
	UINT nChildren = 0;
	
	for	(i=0; i<8; i++)	
	{
		if (pNode->pChild[i] !=	NULL) 
		{
			nRedSum	+= pNode->pChild[i]->nRedSum;
			nGreenSum += pNode->pChild[i]->nGreenSum;
			nBlueSum +=	pNode->pChild[i]->nBlueSum;
			pNode->nPixelCount += pNode->pChild[i]->nPixelCount;
			LocalFree(pNode->pChild[i]);
			pNode->pChild[i] = NULL;
			nChildren++;
		}
	}
	
	pNode->bIsLeaf = TRUE;
	pNode->nRedSum = nRedSum;
	pNode->nGreenSum = nGreenSum;
	pNode->nBlueSum	= nBlueSum;
	*pLeafCount	-= (nChildren -	1);
}

/////////////////////////////////////////////////////////////////////////////
void CQuantizer::DeleteTree	(NODE**	ppNode)
{
	for(int i=0; i<8; i++)	
		{
		if ((*ppNode)->pChild[i] != NULL) 
			DeleteTree (&((*ppNode)->pChild[i]));
		}
	
	LocalFree(*ppNode);	
	*ppNode	= NULL;
}


BOOL CQuantizer::FindColorIndex(NODE *pNode,BYTE r,BYTE g,BYTE b,int nLevel,UINT *pColorIndex)
{
	static BYTE	mask[8]	= {	0x80, 0x40,	0x20, 0x10,	0x08, 0x04,	0x02, 0x01 };
	
	// If the node doesn't exist, create it.
	if (pNode == NULL)
	{	
		return FALSE;
	}
	
	// Update color	information	if it's	a leaf node.
	if (pNode->bIsLeaf)	
	{
		*pColorIndex = pNode->nColorIndex;
		return TRUE;
	}
	
	// Recurse a level deeper if the node is not a leaf.
	int	shift =	7 -	nLevel;
	int	nIndex =(((r & mask[nLevel]) >> shift) << 2) |
		(((g & mask[nLevel]) >>	shift) << 1) |
		(( b & mask[nLevel]) >> shift);
	
	return FindColorIndex(pNode->pChild[nIndex], r, g, b, nLevel+1, pColorIndex);
}

BOOL CQuantizer::GetColorIndex(BYTE r,BYTE g,BYTE b,UINT *pColorIndex)
{
	return FindColorIndex(m_pTree,r,g,b,0,pColorIndex);
}

/////////////////////////////////////////////////////////////////////////////
void CQuantizer::GetPaletteColors (NODE* pTree,	RGBQUAD* prgb, UINT* pIndex)
{
	if (pTree){
		if (pTree->bIsLeaf)	{
			prgb[*pIndex].rgbRed = (BYTE)((pTree->nRedSum)/(pTree->nPixelCount));
			prgb[*pIndex].rgbGreen = (BYTE)((pTree->nGreenSum)/(pTree->nPixelCount));
			prgb[*pIndex].rgbBlue = (BYTE)((pTree->nBlueSum)/(pTree->nPixelCount));
			prgb[*pIndex].rgbReserved =	0;
			pTree->nColorIndex = *pIndex;
			(*pIndex)++;
		} else {
			for	(int i=0; i<8; i++)	{
				if (pTree->pChild[i] !=	NULL)
					GetPaletteColors (pTree->pChild[i],	prgb, pIndex);
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////
UINT CQuantizer::GetColorCount ()
{
	return m_nLeafCount;
}

/////////////////////////////////////////////////////////////////////////////
void CQuantizer::SetColorTable (RGBQUAD* prgb)
{
	UINT nIndex	= 0;
	GetPaletteColors (m_pTree, prgb, &nIndex);
}

/////////////////////////////////////////////////////////////////////////////
BYTE CQuantizer::GetPixelIndex(long x, long y, int nbit, long effwdt, BYTE *pimage)
{
	if (nbit==8)
	{
		return pimage[y*effwdt + x];
	} else 
	{
		BYTE pos;
		BYTE iDst= pimage[y*effwdt + (x*nbit >> 3)];
		if (nbit==4)
		{
			pos = 4*(1-x%2);
			iDst &= (0x0F<<pos);
			return iDst >> pos;
		} else if (nbit==1)
		{
			pos = 7-x%8;
			iDst &= (0x01<<pos);
			return iDst >> pos;
		}
	}
	return 0;
}

// CRGBQuantizer::CRGBQuantizer(UINT nMaxColors, UINT nColorBits)
// :CQuantizer(nMaxColors,nColorBits)
// {
// }
// 
// BOOL CRGBQuantizer::ProcessImageRGB(BYTE *pRGBData,UINT nWidth,UINT nHeight)
// {
// 	BYTE r,g,b;
// 	
// 	for	(int i=0; i<nHeight;	i++) 
// 	{
// 		for	(int j=0; j<nWidth; j++)	
// 		{
// 			r =	*pRGBData++;
// 			g =	*pRGBData++;
// 			b =	*pRGBData++;
// 			AddColor (&m_pTree,	r, g, b, m_nColorBits, 0, &m_nLeafCount,
// 				m_pReducibleNodes);
// 			while (m_nLeafCount	> m_nMaxColors)
// 				ReduceTree (m_nColorBits, &m_nLeafCount, m_pReducibleNodes);
// 		}
// 	}
// 	
// 	return TRUE;
// }
// 
// CBitmapQuantizer::CBitmapQuantizer(UINT nMaxColors, UINT nColorBits)
// :CQuantizer(nMaxColors,nColorBits)
// {
// }
// 
// BOOL CBitmapQuantizer::ProcessImageBitmap (HBITMAP hBmp)
// {
// 	BITMAP bm;
// 	PBITMAPINFO bmpInf;
// 	
// 	if(GetObject(hBmp,sizeof(bm),&bm)==0)
// 		return FALSE;
// 	
// 	int nPaletteSize=0;
// 	
// 	if(bm.bmBitsPixel<16)
// 		nPaletteSize=(int)pow(2,bm.bmBitsPixel);
// 	
// 	bmpInf=(PBITMAPINFO)LocalAlloc(LPTR,sizeof(BITMAPINFOHEADER)+
// 		sizeof(RGBQUAD)*nPaletteSize+
// 		(bm.bmWidth+7)/8*bm.bmHeight*bm.bmBitsPixel);
// 	
// 	BYTE* buf = ((BYTE*)bmpInf)+
// 		sizeof(BITMAPINFOHEADER)+
// 		sizeof(RGBQUAD)*nPaletteSize;
// 	
// 	//-----------------------------------------------
// 	bmpInf->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
// 	bmpInf->bmiHeader.biWidth = bm.bmWidth;
// 	bmpInf->bmiHeader.biHeight = bm.bmHeight;
// 	bmpInf->bmiHeader.biPlanes = bm.bmPlanes;
// 	bmpInf->bmiHeader.biBitCount = bm.bmBitsPixel;
// 	bmpInf->bmiHeader.biCompression = BI_RGB;
// 	bmpInf->bmiHeader.biSizeImage = (bm.bmWidth+7)/8*bm.bmHeight*bm.bmBitsPixel;
// 	bmpInf->bmiHeader.biXPelsPerMeter = 0;
// 	bmpInf->bmiHeader.biYPelsPerMeter = 0;
// 	bmpInf->bmiHeader.biClrUsed = 0;
// 	bmpInf->bmiHeader.biClrImportant = 0;
// 	//-----------------------------------------------
// 	
// 	HDC hDC = ::GetWindowDC(NULL);
// 	
// 	if(!::GetDIBits(hDC,hBmp,0,(UINT)bm.bmHeight,buf,bmpInf,DIB_RGB_COLORS))
// 	{
// 		::ReleaseDC(NULL,hDC);
// 		LocalFree(bmpInf);
// 		return FALSE;
// 	}
// 	
// 	::ReleaseDC(NULL,hDC);
// 	
// 	bmpInf->bmiHeader.biSize = sizeof(BITMAPINFOHEADER)+sizeof(RGBQUAD)*nPaletteSize;
// 	BOOL bRet = 0;//ProcessImage(bmpInf);
// 	
// 	LocalFree(bmpInf);
// 	
// 	return bRet;
// }