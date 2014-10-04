
#include <assert.h>
#include <stdlib.h>
#include <memory.h>

#include "image.h"

static long int crv_tab[256];
static long int cbu_tab[256];
static long int cgu_tab[256];
static long int cgv_tab[256];
static long int tab_76309[256];
static unsigned char clp[1024]; //for clip in CCIR601

void InitConvtTblNew()
{
	long int crv,cbu,cgu,cgv;
	int i,ind; 

	crv = 104597; cbu = 132201; 
	cgu = 25675; cgv = 53279;

	for (i = 0; i < 256; i++) 
	{
		crv_tab[i] = (i-128) * crv;
		cbu_tab[i] = (i-128) * cbu;
		cgu_tab[i] = (i-128) * cgu;
		cgv_tab[i] = (i-128) * cgv;
		tab_76309[i] = 76309*(i-16);
	}

	for (i=0; i<384; i++)
		clp[i] =0;
	ind=384;
	for (i=0;i<256; i++)
		clp[ind++]=i;
	ind=640;
	for (i=0;i<384;i++)
		clp[ind++]=255;
}

#ifdef LITTLE_ENDIAN

#define I420_RGB24(p, y, c1, c2, c3, c4)	\
	*p++ = clp[384+((y + c1)>>16)];			\
	*p++ = clp[384+((y - c2 - c3)>>16)];	\
	*p++ = clp[384+((y + c4)>>16)];

#define I420_ARGB32(p, y, c1, c2, c3, c4)	\
	I420_RGB24(p, y, c1, c2, c3, c4);		\
	*p++ = 0xFF;

#else

#define I420_RGB24(p, y, c1, c2, c3, c4)	\
	*p++ = clp[384+((y + c4)>>16)];			\
	*p++ = clp[384+((y - c2 - c3)>>16)];	\
	*p++ = clp[384+((y + c1)>>16)];

#define I420_ARGB32(p, y, c1, c2, c3, c4)	\
	*p++ = 0xFF;							\
	I420_RGB24(p, y, c1, c2, c3, c4);

#endif

void I420_2_RGB24(unsigned char *dst,
				  unsigned char* src_y, int stride_y,
				  unsigned char* src_u, int stride_u,
				  unsigned char* src_v, int stride_v,
				  int width, int height, int stride)
{
	int y1,y2;
	unsigned char *py1,*py2,*pyy1,*pyy2,*u,*v,*uu,*vv;
	int i,j, c1, c2, c3, c4;
	unsigned char *d1, *d2,*dd1, *dd2;
	static const int color_depth = 3;
	
	const int dt = color_depth * stride * 2;

	//Initialization
	py1 = src_y;
	py2 = src_y + stride_y;
	u = src_u;
	v = src_v;


	d1 = dst + color_depth * width * (height - 1);
	d2 = d1 - color_depth * width;

	//d1 = dst;
	//d2 = dst + color_depth * width;
	for (j = 0; j < height; j += 2)
	{
		uu = u;
		vv = v;
		pyy1 = py1;
		pyy2 = py2;
		dd1 = d1;
		dd2 = d2;
		for (i = 0; i < width; i += 2)
		{
			c1 = crv_tab[*vv];
			c2 = cgu_tab[*uu];
			c3 = cgv_tab[*vv++];
			c4 = cbu_tab[*uu++];
			//SNDA_LOG("%d, %d, %d, %d", c1, c2, c3, c4);
			//up-left
			y1 = tab_76309[*pyy1++];
			I420_RGB24(dd1, y1, c1, c2, c3, c4);

			//down-left
			y2 = tab_76309[*pyy2++];
			I420_RGB24(dd2, y2, c1, c2, c3, c4);

			//up-right
			y1 = tab_76309[*pyy1++];
			I420_RGB24(dd1, y1, c1, c2, c3, c4);

			//down-right
			y2 = tab_76309[*pyy2++];
			I420_RGB24(dd2, y2, c1, c2, c3, c4);
		}
		d1 -= dt;
		d2 -= dt;

		py1 += 2 * stride_y;
		py2 += 2 * stride_y;
		u   += stride_u;
		v   += stride_v;
	}
}

void I420_2_RGB24_ROTATE90(unsigned char *dst,
						   unsigned char* src_y, int stride_y,
						   unsigned char* src_u, int stride_u,
						   unsigned char* src_v, int stride_v,
						   int width, int height, int stride)
{
	int y1,y2;
	unsigned char *py1,*py2,*pyy1,*pyy2,*u,*v,*uu,*vv;
	int i,j, c1, c2, c3, c4;
	unsigned char *d1, *d2,*dd1, *dd2;

	const int color_depth = 3;


	//Initialization
	py1 = src_y;
	py2 = src_y + stride_y;
	u = src_u;
	v = src_v;


	d1 = dst + color_depth * (height - 1);
	d2 = dst + color_depth * (height - 1 + stride);
	for (j = 0; j < height; j += 2)
	{
		uu = u;
		vv = v;
		pyy1 = py1;
		pyy2 = py2;
		dd1 = d1;
		dd2 = d2;
		for (i = 0; i < width; i += 2)
		{
			c1 = crv_tab[*vv];
			c2 = cgu_tab[*uu];
			c3 = cgv_tab[*vv++];
			c4 = cbu_tab[*uu++];
			//SNDA_LOG("%d, %d, %d, %d", c1, c2, c3, c4);

			//down-left
			y2 = tab_76309[*pyy2++];
			I420_RGB24(dd1, y2, c1, c2, c3, c4);

			//up-left
			y1 = tab_76309[*pyy1++];
			I420_RGB24(dd1, y1, c1, c2, c3, c4);

			//down-right
			y2 = tab_76309[*pyy2++];
			I420_RGB24(dd2, y2, c1, c2, c3, c4);

			//up-right
			y1 = tab_76309[*pyy1++];
			I420_RGB24(dd2, y1, c1, c2, c3, c4);

			dd1 += color_depth * 2 * (stride - 1);
			dd2 += color_depth * 2 * (stride - 1);
		}
		d1 -= color_depth * 2;
		d2 -= color_depth * 2;

		py1 += 2 * stride_y;
		py2 += 2 * stride_y;
		u   += stride_u;
		v   += stride_v;
	}
}

void I420_2_ARGB32_ROTATE90(unsigned char *dst,
							unsigned char* src_y, int stride_y,
							unsigned char* src_u, int stride_u,
							unsigned char* src_v, int stride_v,
							int width, int height, int stride)
{
	int y1,y2;
	unsigned char *py1,*py2,*pyy1,*pyy2,*u,*v,*uu,*vv;
	int i,j, c1, c2, c3, c4;
	unsigned char *d1, *d2,*dd1, *dd2;

	const int color_depth = 4;


	//Initialization
	py1 = src_y;
	py2 = src_y + stride_y;
	u = src_u;
	v = src_v;


	d1 = dst + color_depth * (height - 1);
	d2 = dst + color_depth * (height - 1 + stride);
	for (j = 0; j < height; j += 2)
	{
		uu = u;
		vv = v;
		pyy1 = py1;
		pyy2 = py2;
		dd1 = d1;
		dd2 = d2;
		for (i = 0; i < width; i += 2)
		{
			c1 = crv_tab[*vv];
			c2 = cgu_tab[*uu];
			c3 = cgv_tab[*vv++];
			c4 = cbu_tab[*uu++];
			//SNDA_LOG("%d, %d, %d, %d", c1, c2, c3, c4);

			//down-left
			y2 = tab_76309[*pyy2++];
			I420_ARGB32(dd1, y2, c1, c2, c3, c4);

			//up-left
			y1 = tab_76309[*pyy1++];
			I420_ARGB32(dd1, y1, c1, c2, c3, c4);

			//down-right
			y2 = tab_76309[*pyy2++];
			I420_ARGB32(dd2, y2, c1, c2, c3, c4);

			//up-right
			y1 = tab_76309[*pyy1++];
			I420_ARGB32(dd2, y1, c1, c2, c3, c4);

			dd1 += color_depth * 2 * (stride - 1);
			dd2 += color_depth * 2 * (stride - 1);
		}
		d1 -= color_depth * 2;
		d2 -= color_depth * 2;

		py1 += 2 * stride_y;
		py2 += 2 * stride_y;
		u   += stride_u;
		v   += stride_v;
	}
}


// template<class T>
// void ClipPlane(T* src, int src_width, int src_height,int src_stride,
// 				int x, int y,
// 				T* dst, int dst_width, int dst_height, int dst_stride)
// {
// 	assert(src != NULL);
// 	assert(dst != NULL);
// 
// 	register T* src_ptr = src + x + y * src_stride;
// 	register T* dst_ptr = dst;
// 
// 	register int stride_pad = src_stride - dst_width;
// 	for (int j = 0; j < dst_height; j++)
// 	{
// 		for (int i = 0; i < dst_width; i++)
// 		{
// 			*dst_ptr++ = *src_ptr++;
// 		}
// 		src_ptr += stride_pad;
// 	}
// 	return;
// }

int I420SP_2_I420(unsigned char* src,unsigned char *dst, int width, int height, int stride)
{
	register int frame_size_y = stride * height;
	register int frame_size_uv = (stride * height >> 2);
	register unsigned char* src_u_ptr;
	register unsigned char* src_v_ptr;
	register unsigned char* dst_u_ptr;
	register unsigned char* dst_v_ptr;

	assert(src != NULL);
	assert(dst != NULL);

	memcpy(dst, src, frame_size_y);

	src_u_ptr = src + frame_size_y;
	src_v_ptr = src + frame_size_y + 1;
	dst_u_ptr = dst + frame_size_y;
	dst_v_ptr = dst + frame_size_y + frame_size_uv;

	{
		int i;
		for (i = 0; i < frame_size_uv; i++)
		{
			*dst_u_ptr++ = *src_u_ptr;
			*dst_v_ptr++ = *src_v_ptr;
			src_u_ptr += 2;
			src_v_ptr += 2;
		}
	}

	return 0;
}

int I420_2_I420SP(unsigned char* src,unsigned char *dst, int width, int height, int stride)
{
	register int frame_size_y = stride * height;
	register int frame_size_uv = (stride * height >> 2);
	register unsigned char* dst_u_ptr;
	register unsigned char* dst_v_ptr;
	register unsigned char* src_u_ptr;
	register unsigned char* src_v_ptr;

	assert(src != NULL);
	assert(dst != NULL);
	memcpy(dst, src, frame_size_y);

	dst_u_ptr = dst + frame_size_y;
	dst_v_ptr = dst + frame_size_y + 1;
	src_u_ptr = src + frame_size_y;
	src_v_ptr = src + frame_size_y + frame_size_uv;

	{
		int i;
		for (i = 0; i < frame_size_uv; i++)
		{
			*dst_u_ptr = *src_u_ptr++;
			*dst_v_ptr = *src_v_ptr++;
			dst_u_ptr += 2;
			dst_v_ptr += 2;
		}
	}
	return 0;
}

// int Clip_I420SP(unsigned char* src, int src_width, int src_height,int src_stride,
// 				int x, int y,
// 				unsigned char *dst, int dst_width, int dst_height, int dst_stride)
// {
// 	assert(src != NULL);
// 	assert(dst != NULL);
// 	assert(x % 2 == 0);
// 	assert(y % 2 == 0);
// 
// 	ClipPlane<unsigned char>(src, src_width, src_height, src_stride,
// 							 x, y,
// 							 dst, dst_width, dst_height, dst_stride);
// 
// 	register int src_frame_size_y = src_stride * src_height;
// 	register int dst_frame_size_y = dst_stride * dst_height;
// 
// 	register unsigned short* src_uv_ptr = (unsigned short*)(src + src_frame_size_y);
// 	register unsigned short* dst_uv_ptr = (unsigned short*)(dst + dst_frame_size_y);
// 
// 	ClipPlane<unsigned short>(src_uv_ptr, src_width / 2, src_height / 2, src_stride / 2,
// 							  x / 2, y / 2,
// 							  dst_uv_ptr, dst_width / 2, dst_height / 2, dst_stride / 2);
// 
// 	return 0;
// }


void Zoom_RGB24_X2(unsigned char* dst, unsigned char* src, int width, int height, int stride)
{
	const int color_csp = 3;
	
	unsigned char* d1 = dst;
	unsigned char* d2 = dst + width * 2 * color_csp;

	unsigned char* s1 = src;
	unsigned char* s2 = src + stride * color_csp;

	{
		int j;
		for (j = 0; j < height - 1; j++)
		{
			unsigned char* d00 = dst + 4 * j * width * color_csp;
			unsigned char* d01 = d00 + color_csp;
			unsigned char* d10 = d00 + 2 * width * color_csp;
			unsigned char* d11 = d10 + color_csp;

			unsigned char* s00 = src + 2 * j * width * color_csp;
			unsigned char* s01 = src + (j * 2 * width + 1) * color_csp;
			unsigned char* s10 = src + (j + 1) * 2 * width * color_csp;
			unsigned char* s11 = src + ((j + 1) * 2 * width + 1) * color_csp;

			{
				int i;
				for (i = 0;  i < width - 1; i++)
				{
					d00[0] = s00[0];
					d00[1] = s00[1];
					d00[2] = s00[2];

					d01[0] = s00[0] / 2 + s01[0] / 2;
					d01[1] = s00[1] / 2 + s01[1] / 2;
					d01[2] = s00[2] / 2 + s01[2] / 2;

					d10[0] = s00[0] / 2 + s10[0] / 2;
					d10[1] = s00[1] / 2 + s10[1] / 2;
					d10[2] = s00[2] / 2 + s10[2] / 2;

					d11[0] = s00[0] / 2 + s11[0] / 2;
					d11[1] = s00[1] / 2 + s11[1] / 2;
					d11[2] = s00[2] / 2 + s11[2] / 2;

					d00 += 2 * color_csp;
					d01 += 2 * color_csp;
					d10 += 2 * color_csp;
					d11 += 2 * color_csp;

					s00 += color_csp;
					s01 += color_csp;
					s10 += color_csp;
					s11 += color_csp;
				}
			}
		}
	}
}

static int BMP_HEADER_LEN = 0x36;

static unsigned char bmpheader[0x36] ={
	0x42, 0x4D,
	0x00, 0x00, 0x00, 0x00,		// file size
	0x00, 0x00,
	0x00, 0x00,
	0x36, 0x00, 0x00, 0x00,
	0x28, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,		// width
	0x00, 0x00, 0x00, 0x00,		// height
	0x01, 0x00,
	0x00, 0x00,					// depth
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,		// data size
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00
};

void make_bmpheader(unsigned char* header, int width, int height, int depth)
{
	int data_size;
	int file_size;

	if (width % 4 != 0)
	{
		width += 4 - (width % 4);
	}
	data_size = width * height * (depth / 8);
	file_size = data_size + 0x36;
	memcpy(&header[0x02], &file_size, sizeof(int));
	memcpy(&header[0x12], &width,     sizeof(int));
	memcpy(&header[0x16], &height,    sizeof(int));
	memcpy(&header[0x1C], &depth,     sizeof(int));
	memcpy(&header[0x22], &data_size, sizeof(int));
	return;
}

enum{
	ENU_SRC_WIDTH = 352,
	ENU_SRC_HEIGHT = 288,
	ENU_CLIP_WIDTH = 320,
	ENU_CLIP_HEIGHT = 180
};

// int main(void)
// {
// 	InitConvtTbl();
// 	const char lpszSrcFileName[] = "foreman000.yuv";
// 	unsigned char file_buffer[ENU_SRC_WIDTH * ENU_SRC_HEIGHT * 3 / 2];
// 	FILE* src = fopen(lpszSrcFileName, "rb");
// 	if (src == NULL)
// 	{
// 		return -1;
// 	}
// 	fread(&file_buffer[0], 1, sizeof(file_buffer), src);
// 	fclose(src);
// 	src = NULL;
// 	// convert to rgb24
// 	/*
// 	{
// 		unsigned char* src_y = &file_buffer[0];
// 		unsigned char* src_u = &file_buffer[ENU_SRC_WIDTH * ENU_SRC_HEIGHT];
// 		unsigned char* src_v = &file_buffer[ENU_SRC_WIDTH * ENU_SRC_HEIGHT * 5 / 4];
// 
// 		const char lpszDstFileName[] = "foreman000.bmp";
// 		FILE* dst = fopen(lpszDstFileName, "wb");
// 		if (dst == NULL)
// 		{
// 			return -1;
// 		}
// 		make_bmpheader(&bmpheader[0], ENU_SRC_WIDTH, ENU_SRC_HEIGHT, 24);
// 		fwrite(&bmpheader[0], 1, BMP_HEADER_LEN, dst);
// 		unsigned char dst_buffer[ENU_SRC_WIDTH * ENU_SRC_HEIGHT * 3];
// 
// 		I420_2_RGB24(&dst_buffer[0],
//  					 src_y, ENU_SRC_WIDTH,
// 					 src_u, ENU_SRC_WIDTH / 2,
// 					 src_v, ENU_SRC_WIDTH / 2,
// 					 ENU_SRC_WIDTH, ENU_SRC_HEIGHT, ENU_SRC_WIDTH);
// 		fwrite(&dst_buffer[0], 1, sizeof(dst_buffer), dst);
// 		fclose(dst);
// 		dst = NULL;
// 	}
// 	*/
// 	// clip and rotate test
// 	{
// 		unsigned char i420sp_buffer[ENU_SRC_WIDTH * ENU_SRC_HEIGHT * 3 / 2];
// 		I420_2_I420SP(file_buffer, i420sp_buffer, ENU_SRC_WIDTH, ENU_SRC_HEIGHT, ENU_SRC_WIDTH);
// 
// 		int x = (ENU_SRC_WIDTH - ENU_CLIP_WIDTH) / 4;
// 		x *= 2;
// 		int y = (ENU_SRC_HEIGHT - ENU_CLIP_HEIGHT) / 4;
// 		y *= 2;
// 
// 		unsigned char clip_buffer[ENU_CLIP_WIDTH * ENU_CLIP_WIDTH * 3 / 2];
// 		memset(clip_buffer, 0, sizeof(clip_buffer));
// 		Clip_I420SP(i420sp_buffer, ENU_SRC_WIDTH, ENU_SRC_HEIGHT, ENU_SRC_WIDTH,
// 			        x, y,
// 					clip_buffer, ENU_CLIP_WIDTH, ENU_CLIP_HEIGHT, ENU_CLIP_WIDTH);
// 
// 		unsigned char yuv_buffer[ENU_CLIP_WIDTH * ENU_CLIP_WIDTH * 3 / 2];
// 		I420SP_2_I420(clip_buffer, yuv_buffer, ENU_CLIP_WIDTH, ENU_CLIP_HEIGHT, ENU_CLIP_WIDTH);
// 		unsigned char* src_y = &yuv_buffer[0];
// 		unsigned char* src_u = &yuv_buffer[ENU_CLIP_WIDTH * ENU_CLIP_HEIGHT];
// 		unsigned char* src_v = &yuv_buffer[ENU_CLIP_WIDTH * ENU_CLIP_HEIGHT * 5 / 4];
// 
// 		unsigned char* dst_buffer = new unsigned char[ENU_CLIP_WIDTH * ENU_CLIP_WIDTH * 3 * 4];
// 		I420_2_RGB24_ROTATE90(&dst_buffer[(2 * ENU_CLIP_WIDTH + 1) * ENU_CLIP_HEIGHT * 3],
// 							  src_y, ENU_CLIP_WIDTH,
// 							  src_u, ENU_CLIP_WIDTH / 2,
// 							  src_v, ENU_CLIP_WIDTH / 2,
// 							  ENU_CLIP_WIDTH, ENU_CLIP_HEIGHT, ENU_CLIP_HEIGHT * 2);
// 
// 		Zoom_RGB24_X2(&dst_buffer[0], &dst_buffer[(2 * ENU_CLIP_WIDTH + 1) * ENU_CLIP_HEIGHT * 3], ENU_CLIP_HEIGHT, ENU_CLIP_WIDTH, ENU_CLIP_HEIGHT * 2);
// 		const char lpszDstFileName[] = "foreman000_clip_rotate.bmp";
// 		FILE* dst = fopen(lpszDstFileName, "wb");
// 		if (dst == NULL)
// 		{
// 			return -1;
// 		}
// 		make_bmpheader(&bmpheader[0], ENU_CLIP_HEIGHT * 2, ENU_CLIP_WIDTH * 2, 24);
// 		fwrite(&bmpheader[0], 1, BMP_HEADER_LEN, dst);
// 		fwrite(&dst_buffer[0], 1, ENU_CLIP_WIDTH * ENU_CLIP_WIDTH * 3 * 4, dst);
// 		fclose(dst);
// 		dst = NULL;
// 	}
// 	//rotate test
// 	{
// 
// 	}
// 	return 1;
// }