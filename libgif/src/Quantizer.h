/*
	颜色量化处理类
*/

#if !defined(QUANTIZER_H_)
#define QUANTIZER_H_

struct NODE
{
	bool		bIsLeaf;
	long		nPixelCount;
	long		nRedSum;
	long		nGreenSum;
	long		nBlueSum;
	unsigned int	nColorIndex;
	NODE		*pChild[8];
	NODE		*pNext;
};

class CQuantizer
{
	public:
		CQuantizer(UINT nMaxColors, UINT nColorBits);
		~CQuantizer();
		
		BOOL ProcessImage (BYTE* lpRgb24,int nWidth,int nHeight);
		
		UINT GetColorCount ();

		void SetColorTable (RGBQUAD* prgb);

		BOOL GetColorIndex(BYTE r,BYTE g,BYTE b,UINT *pColorIndex);

	protected:
		
		void AddColor (NODE** ppNode, BYTE r, BYTE g, BYTE b,
			UINT nColorBits, UINT nLevel, UINT*	pLeafCount,	NODE** pReducibleNodes);
		
		void* CreateNode (UINT nLevel, UINT	nColorBits,	UINT* pLeafCount,
				NODE** pReducibleNodes);
		
		void ReduceTree	(UINT nColorBits, UINT*	pLeafCount,
				NODE** pReducibleNodes);

		void DeleteTree	(NODE**	ppNode);

		void GetPaletteColors (NODE* pTree,	RGBQUAD* prgb, UINT* pIndex);

		BOOL FindColorIndex(NODE *pNode,BYTE r,BYTE g,BYTE b,int nLevel,UINT *pColorIndex);

		BYTE GetPixelIndex(long x, long y, int nbit, long effwdt, BYTE *pimage);

		int m_nColorBits;
		UINT m_nLeafCount;
		int m_nMaxColors;
		NODE *m_pTree;
		NODE *m_pReducibleNodes[9];
};

#endif