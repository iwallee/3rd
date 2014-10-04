#if !defined(__IMAGE_H__)
#define __IMAGE_H__
//----------------------------------------------------------
void InitConvtTblNew();

void I420_2_RGB24(unsigned char *dst,
				  unsigned char* src_y, int stride_y,
				  unsigned char* src_u, int stride_u,
				  unsigned char* src_v, int stride_v,
				  int width, int height, int stride);


//----------------------------------------------------------
#endif