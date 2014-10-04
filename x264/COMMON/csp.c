/*****************************************************************************
 * csp.c: h264 encoder library
 *****************************************************************************
 * Copyright (C) 2004 Laurent Aimar
 * $Id: csp.c,v 1.1.1.1 2005/03/16 13:27:22 264 Exp $
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "common.h"

#define MIN(A,B)	((A)<(B)?(A):(B))
#define MAX(A,B)	((A)>(B)?(A):(B))

#define Y_R_IN			0.257
#define Y_G_IN			0.504
#define Y_B_IN			0.098
#define Y_ADD_IN		16

#define U_R_IN			0.148
#define U_G_IN			0.291
#define U_B_IN			0.439
#define U_ADD_IN		128

#define V_R_IN			0.439
#define V_G_IN			0.368
#define V_B_IN			0.071
#define V_ADD_IN		128

#define SCALEBITS_IN	8
#define FIX_IN(x)		((uint16_t) ((x) * (1L<<SCALEBITS_IN) + 0.5))


#define RGB_Y_OUT		1.164
#define B_U_OUT			2.018
#define Y_ADD_OUT		16

#define G_U_OUT			0.391
#define G_V_OUT			0.813
#define U_ADD_OUT		128

#define R_V_OUT			1.596
#define V_ADD_OUT		128


#define SCALEBITS_OUT	13
#define FIX_OUT(x)		((uint16_t) ((x) * (1L<<SCALEBITS_OUT) + 0.5))

#define MK_RGB555(R,G,B)	((MAX(0,MIN(255, R)) << 7) & 0x7c00) | \
							((MAX(0,MIN(255, G)) << 2) & 0x03e0) | \
							((MAX(0,MIN(255, B)) >> 3) & 0x001f)


static inline void plane_copy( uint8_t *dst, int i_dst,
                               uint8_t *src, int i_src, int w, int h)
{
    for( ; h > 0; h-- )
    {
        memcpy( dst, src, w );
        dst += i_dst;
        src += i_src;
    }
}
static inline void plane_copy_vflip( uint8_t *dst, int i_dst,
                                     uint8_t *src, int i_src, int w, int h)
{
    plane_copy( dst, i_dst, src + (h -1)*i_src, -i_src, w, h );
}

static inline void plane_subsamplev2( uint8_t *dst, int i_dst,
                                      uint8_t *src, int i_src, int w, int h)
{
    for( ; h > 0; h-- )
    {
        uint8_t *d = dst;
        uint8_t *s = src;
        int     i;
        for( i = 0; i < w; i++ )
        {
            *d++ = ( s[0] + s[i_src] + 1 ) >> 1;
            s++;
        }
        dst += i_dst;
        src += 2 * i_src;
    }
}

static inline void plane_subsamplev2_vlip( uint8_t *dst, int i_dst,
                                           uint8_t *src, int i_src, int w, int h)
{
    plane_subsamplev2( dst, i_dst, src + (2*h-1)*i_src, -i_src, w, h );
}

static inline void plane_subsamplehv2( uint8_t *dst, int i_dst,
                                       uint8_t *src, int i_src, int w, int h)
{
    for( ; h > 0; h-- )
    {
        uint8_t *d = dst;
        uint8_t *s = src;
        int     i;
        for( i = 0; i < w; i++ )
        {
            *d++ = ( s[0] + s[1] + s[i_src] + s[i_src+1] + 1 ) >> 2;
            s += 2;
        }
        dst += i_dst;
        src += 2 * i_src;
    }
}

static inline void plane_subsamplehv2_vlip( uint8_t *dst, int i_dst,
                                            uint8_t *src, int i_src, int w, int h)
{
    plane_subsamplehv2( dst, i_dst, src + (2*h-1)*i_src, -i_src, w, h );
}

static void i420_to_i420( x264_frame_t *frm, x264_image_t *img,
                          int i_width, int i_height )
{
    if( img->i_csp & X264_CSP_VFLIP )
    {
        plane_copy_vflip( frm->plane[0], frm->i_stride[0],
                          img->plane[0], img->i_stride[0],
                          i_width, i_height );
        plane_copy_vflip( frm->plane[1], frm->i_stride[1],
                          img->plane[1], img->i_stride[1],
                          i_width / 2, i_height / 2 );
        plane_copy_vflip( frm->plane[2], frm->i_stride[2],
                          img->plane[2], img->i_stride[2],
                          i_width / 2, i_height / 2 );
    }
    else
    {
        plane_copy( frm->plane[0], frm->i_stride[0],
                    img->plane[0], img->i_stride[0],
                    i_width, i_height );
        plane_copy( frm->plane[1], frm->i_stride[1],
                    img->plane[1], img->i_stride[1],
                    i_width / 2, i_height / 2 );
        plane_copy( frm->plane[2], frm->i_stride[2],
                    img->plane[2], img->i_stride[2],
                    i_width / 2, i_height / 2 );
    }
}

static void yv12_to_i420( x264_frame_t *frm, x264_image_t *img,
                          int i_width, int i_height )
{
    if( img->i_csp & X264_CSP_VFLIP )
    {
        plane_copy_vflip( frm->plane[0], frm->i_stride[0],
                          img->plane[0], img->i_stride[0],
                          i_width, i_height );
        plane_copy_vflip( frm->plane[2], frm->i_stride[2],
                          img->plane[1], img->i_stride[1],
                          i_width / 2, i_height / 2 );
        plane_copy_vflip( frm->plane[1], frm->i_stride[1],
                          img->plane[2], img->i_stride[2],
                          i_width / 2, i_height / 2 );
    }
    else
    {
        plane_copy( frm->plane[0], frm->i_stride[0],
                    img->plane[0], img->i_stride[0],
                    i_width, i_height );
        plane_copy( frm->plane[2], frm->i_stride[2],
                    img->plane[1], img->i_stride[1],
                    i_width / 2, i_height / 2 );
        plane_copy( frm->plane[1], frm->i_stride[1],
                    img->plane[2], img->i_stride[2],
                    i_width / 2, i_height / 2 );
    }
}

static void i422_to_i420( x264_frame_t *frm, x264_image_t *img,
                          int i_width, int i_height )
{
    if( img->i_csp & X264_CSP_VFLIP )
    {
        plane_copy_vflip( frm->plane[0], frm->i_stride[0],
                          img->plane[0], img->i_stride[0],
                          i_width, i_height );

        plane_subsamplev2_vlip( frm->plane[1], frm->i_stride[1],
                                img->plane[1], img->i_stride[1],
                                i_width / 2, i_height / 2 );
        plane_subsamplev2_vlip( frm->plane[2], frm->i_stride[2],
                                img->plane[2], img->i_stride[2],
                                i_width / 2, i_height / 2 );
    }
    else
    {
        plane_copy( frm->plane[0], frm->i_stride[0],
                    img->plane[0], img->i_stride[0],
                    i_width, i_height );

        plane_subsamplev2( frm->plane[1], frm->i_stride[1],
                           img->plane[1], img->i_stride[1],
                           i_width / 2, i_height / 2 );
        plane_subsamplev2( frm->plane[2], frm->i_stride[2],
                           img->plane[2], img->i_stride[2],
                           i_width / 2, i_height / 2 );
    }
}

static void i444_to_i420( x264_frame_t *frm, x264_image_t *img,
                          int i_width, int i_height )
{
    if( img->i_csp & X264_CSP_VFLIP )
    {
        plane_copy_vflip( frm->plane[0], frm->i_stride[0],
                          img->plane[0], img->i_stride[0],
                          i_width, i_height );

        plane_subsamplehv2_vlip( frm->plane[1], frm->i_stride[1],
                                 img->plane[1], img->i_stride[1],
                                 i_width / 2, i_height / 2 );
        plane_subsamplehv2_vlip( frm->plane[2], frm->i_stride[2],
                                 img->plane[2], img->i_stride[2],
                                 i_width / 2, i_height / 2 );
    }
    else
    {
        plane_copy( frm->plane[0], frm->i_stride[0],
                    img->plane[0], img->i_stride[0],
                    i_width, i_height );

        plane_subsamplehv2( frm->plane[1], frm->i_stride[1],
                            img->plane[1], img->i_stride[1],
                            i_width / 2, i_height / 2 );
        plane_subsamplehv2( frm->plane[2], frm->i_stride[2],
                            img->plane[2], img->i_stride[2],
                            i_width / 2, i_height / 2 );
    }
}
static void yuyv_to_i420( x264_frame_t *frm, x264_image_t *img,
                          int i_width, int i_height )
{
    uint8_t *src = img->plane[0];
    int     i_src= img->i_stride[0];

    uint8_t *y   = frm->plane[0];
    uint8_t *u   = frm->plane[1];
    uint8_t *v   = frm->plane[2];

    if( img->i_csp & X264_CSP_VFLIP )
    {
        src += ( i_height - 1 ) * i_src;
        i_src = -i_src;
    }

    for( ; i_height > 0; i_height -= 2 )
    {
        uint8_t *ss = src;
        uint8_t *yy = y;
        uint8_t *uu = u;
        uint8_t *vv = v;
        int w;

        for( w = i_width; w > 0; w -= 2 )
        {
            *yy++ = ss[0];
            *yy++ = ss[2];

            *uu++ = ( ss[1] + ss[1+i_src] + 1 ) >> 1;
            *vv++ = ( ss[3] + ss[3+i_src] + 1 ) >> 1;

            ss += 4;
        }
        src += i_src;
        y += frm->i_stride[0];
        u += frm->i_stride[1];
        v += frm->i_stride[2];

        ss = src;
        yy = y;
        for( w = i_width; w > 0; w -= 2 )
        {
            *yy++ = ss[0];
            *yy++ = ss[2];
            ss += 4;
        }
        src += i_src;
        y += frm->i_stride[0];
    }
}

/* Same value than in XviD */
#define BITS 8
#define FIX(f) ((int)((f) * (1 << BITS) + 0.5))

#define Y_R   FIX(0.257)
#define Y_G   FIX(0.504)
#define Y_B   FIX(0.098)
#define Y_ADD 16

#define U_R   FIX(0.148)
#define U_G   FIX(0.291)
#define U_B   FIX(0.439)
#define U_ADD 128

#define V_R   FIX(0.439)
#define V_G   FIX(0.368)
#define V_B   FIX(0.071)
#define V_ADD 128
#define RGB_TO_I420( name, POS_R, POS_G, POS_B, S_RGB ) \
static void name( x264_frame_t *frm, x264_image_t *img, \
                  int i_width, int i_height )           \
{                                                       \
    uint8_t *src = img->plane[0]+3*i_width*(i_height-1); \
    int     i_src= img->i_stride[0];                    \
    int     i_y  = frm->i_stride[0];                    \
    uint8_t *y   = frm->plane[0];                       \
    uint8_t *u   = frm->plane[1];                       \
    uint8_t *v   = frm->plane[2];                       \
                                                        \
    if( img->i_csp & X264_CSP_VFLIP )                   \
    {                                                   \
        src += ( i_height - 1 ) * i_src;                \
        i_src = -i_src;                                 \
    }                                                   \
                                                        \
    for(  ; i_height > 0; i_height -= 2 )               \
    {                                                   \
        uint8_t *ss = src;                              \
        uint8_t *yy = y;                                \
        uint8_t *uu = u;                                \
        uint8_t *vv = v;                                \
        int w;                                          \
                                                        \
        for( w = i_width; w > 0; w -= 2 )               \
        {                                               \
            int cr = 0,cg = 0,cb = 0;                   \
            int r, g, b;                                \
                                                        \
            /* Luma */                                  \
            cr = r = ss[POS_R];                         \
            cg = g = ss[POS_G];                         \
            cb = b = ss[POS_B];                         \
                                                        \
            yy[0] = Y_ADD + ((Y_R * r + Y_G * g + Y_B * b) >> BITS);    \
                                                        \
            cr+= r = ss[POS_R-i_src];                   \
            cg+= g = ss[POS_G-i_src];                   \
            cb+= b = ss[POS_B-i_src];                   \
            yy[i_y] = Y_ADD + ((Y_R * r + Y_G * g + Y_B * b) >> BITS);  \
            yy++;                                       \
            ss += S_RGB;                                \
                                                        \
            cr+= r = ss[POS_R];                         \
            cg+= g = ss[POS_G];                         \
            cb+= b = ss[POS_B];                         \
                                                        \
            yy[0] = Y_ADD + ((Y_R * r + Y_G * g + Y_B * b) >> BITS);    \
                                                        \
            cr+= r = ss[POS_R-i_src];                   \
            cg+= g = ss[POS_G-i_src];                   \
            cb+= b = ss[POS_B-i_src];                   \
            yy[i_y] = Y_ADD + ((Y_R * r + Y_G * g + Y_B * b) >> BITS);  \
            yy++;                                       \
            ss += S_RGB;                                \
                                                        \
            /* Chroma */                                \
            *uu++ = (uint8_t)(U_ADD + ((-U_R * cr - U_G * cg + U_B * cb) >> (BITS+2)) ); \
            *vv++ = (uint8_t)(V_ADD + (( V_R * cr - V_G * cg - V_B * cb) >> (BITS+2)) ); \
        }                                               \
                                                        \
        src -= 2*i_src;                                   \
        y += 2*frm->i_stride[0];                        \
        u += frm->i_stride[1];                          \
        v += frm->i_stride[2];                          \
    }                                                   \
}

/***********************************************************************/

void RGB2YUV420( x264_image_t *img, unsigned char *src_ori, 
                  int i_width, int i_height,int stride )           
{                                                       
    uint8_t *src = src_ori+3*i_width*(i_height-1);     
    int     i_src= i_width*3;                   
    int     i_y  = stride;                    
    uint8_t *y   = img->plane[0];                       
    uint8_t *u   = img->plane[1];                       
    uint8_t *v   = img->plane[2];                       
                                                                                                          
                                                        
    for(  ; i_height > 0; i_height -= 2 )              
    {                                                   
        uint8_t *ss = src;                              
        uint8_t *yy = y;                                
        uint8_t *uu = u;                                
        uint8_t *vv = v;                                
        int w;                                          
                                                        
        for( w = i_width; w > 0; w -= 2 )               
        {                                               
            int cr = 0,cg = 0,cb = 0;                   
            int r, g, b;                                
                                                        
            /* Luma */                                  
            cr = r = ss[2];                         
            cg = g = ss[1];                         
            cb = b = ss[0];                         
                                                        
            yy[0] = Y_ADD + ((Y_R * r + Y_G * g + Y_B * b) >> BITS);    
                                                        
            cr+= r = ss[2-i_src];                   
            cg+= g = ss[1-i_src];                   
            cb+= b = ss[0-i_src];                   
            yy[i_y] = Y_ADD + ((Y_R * r + Y_G * g + Y_B * b) >> BITS);  
            yy++;                                       
            ss += 3;                                
                                                        
            cr+= r = ss[2];                         
            cg+= g = ss[1];                        
            cb+= b = ss[0];                         
                                                        
            yy[0] = Y_ADD + ((Y_R * r + Y_G * g + Y_B * b) >> BITS);    
                                                        
            cr+= r = ss[2-i_src];                   
            cg+= g = ss[1-i_src];                   
            cb+= b = ss[0-i_src];                   
            yy[i_y] = Y_ADD + ((Y_R * r + Y_G * g + Y_B * b) >> BITS);  
            yy++;                                       
            ss += 3;                                
                                                        
            /* Chroma */                                
            *uu++ = (uint8_t)(U_ADD + ((-U_R * cr - U_G * cg + U_B * cb) >> (BITS+2)) ); 
            *vv++ = (uint8_t)(V_ADD + (( V_R * cr - V_G * cg - V_B * cb) >> (BITS+2)) ); 
        }                                               
                                                        
        src -= 2*i_src;                                   
        y += 2*stride;                        
        u += stride/2;                          
        v += stride/2;                          
    }                                                   
}

/*********************************************************************************/

#define RGB_TO_I420_new( name, POS_R, POS_G, POS_B, S_RGB ) \
static void name( x264_frame_t *frm, x264_image_t *img, \
                  int i_width, int i_height )           \
{                                                       \
    uint8_t *src = img->plane[0];                       \
    int     i_src= img->i_stride[0];                    \
    int     i_y  = frm->i_stride[0];                    \
    uint8_t *y   = frm->plane[0];                       \
    uint8_t *u   = frm->plane[1];                       \
    uint8_t *v   = frm->plane[2];                       \
                                                        \
                                                        \
    for(  ; i_height > 0; i_height -= 2 )               \
    {                                                   \
        uint8_t *ss = src;                              \
        uint8_t *yy = y;                                \
        uint8_t *uu = u;                                \
        uint8_t *vv = v;                                \
        int w;                                          \
                                                        \
        for( w = i_width; w > 0; w -= 2 )               \
        {                                               \
            int cr = 0,cg = 0,cb = 0;                   \
            int r, g, b;                                \
                                                        \
            /* Luma */                                  \
            cr = r = ss[POS_R];                         \
            cg = g = ss[POS_G];                         \
            cb = b = ss[POS_B];                         \
                                                        \
            yy[0] = 0.299*r + 0.587*g + 0.114*b;     \
                                                        \
            cr+= r = ss[POS_R+i_src];                   \
            cg+= g = ss[POS_G+i_src];                   \
            cb+= b = ss[POS_B+i_src];                   \
            yy[i_y] = 0.299*r + 0.587*g + 0.114*b ;  \
            yy++;                                       \
            ss += S_RGB;                                \
                                                        \
            cr+= r = ss[POS_R];                         \
            cg+= g = ss[POS_G];                         \
            cb+= b = ss[POS_B];                         \
                                                        \
            yy[0] = 0.299*r + 0.587*g + 0.114*b ;    \
                                                        \
            cr+= r = ss[POS_R+i_src];                   \
            cg+= g = ss[POS_G+i_src];                   \
            cb+= b = ss[POS_B+i_src];                   \
            yy[i_y] = 0.299*r + 0.587*g + 0.114*b ;  \
            yy++;                                       \
            ss += S_RGB;                                \
                                                        \
            /* Chroma */                                \
            *uu++ = (uint8_t)(128.0+(-0.169*cr - 0.331*cg + 0.500*cb)/4) ; \
            *vv++ = (uint8_t)((0.500*cr - 0.419*cg - 0.081*cb)/4 + 128.0 ); \
        }                                               \
                                                        \
        src += 2*i_src;                                   \
        y += 2*frm->i_stride[0];                        \
        u += frm->i_stride[1];                          \
        v += frm->i_stride[2];                          \
    }                                                   \
}

RGB_TO_I420( rgb_to_i420,  2, 1, 0, 3 );
RGB_TO_I420( bgr_to_i420,  2, 1, 0, 3 );
RGB_TO_I420( bgra_to_i420, 2, 1, 0, 4 );


static long int crv_tab[256];
static long int cbu_tab[256];
static long int cgu_tab[256];
static long int cgv_tab[256];
static long int tab_76309[256]; 
static unsigned char clp[1024]; //for clip in CCIR601


static int32_t RGB_Y_tab[256];
static int32_t B_U_tab[256];
static int32_t G_U_tab[256];
static int32_t G_V_tab[256];
static int32_t R_V_tab[256];



/****************************************************/
/* Sum the input */
/* Input: input, len */
/* Output: input */
/* Algorithm: add */
/****************************************************/

void InitConvtTbl()
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

        RGB_Y_tab[i] = FIX_OUT(RGB_Y_OUT) * (i - Y_ADD_OUT);
		B_U_tab[i]   = FIX_OUT(B_U_OUT) * (i - U_ADD_OUT);
		G_U_tab[i]   = FIX_OUT(G_U_OUT) * (i - U_ADD_OUT);
		G_V_tab[i]   = FIX_OUT(G_V_OUT) * (i - V_ADD_OUT);
		R_V_tab[i]   = FIX_OUT(R_V_OUT) * (i - V_ADD_OUT);
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

void YUV2RGB420(x264_image_t *img, unsigned char *dst_ori,
				int width,int height,int stride)
{
	unsigned char *src0;
	unsigned char *src1;
	unsigned char *src2;
	int y1,y2; 
	unsigned char *py1,*py2,*pyy1,*pyy2,*u,*v,*uu,*vv;
	int i,j, c1, c2, c3, c4;
	unsigned char *d1, *d2,*dd1, *dd2;
	
	//Initialization
	src0=img->plane[0]; 
	src1=img->plane[1];
	src2=img->plane[2];
	
	py1=src0;
	py2=py1+stride;
	d1=dst_ori+3*width*(height-1);
	d2=d1-3*width;
	u=src1;
	v=src2;
	for (j = 0; j < height; j += 2) { 
		uu=u;
		vv=v;
		pyy1=py1;
		pyy2=py2;
		dd1=d1;
		dd2=d2;
		for (i = 0; i < width; i += 2) {
			
//			u = *src1++;
//			v = *src2++;
			
			c1 = crv_tab[*vv];
			c2 = cgu_tab[*uu];
			c3 = cgv_tab[*vv++];
			c4 = cbu_tab[*uu++];
			
			//up-left
			y1 = tab_76309[*pyy1++]; 
			*dd1++ = clp[384+((y1 + c4)>>16)]; 
			*dd1++ = clp[384+((y1 - c2 - c3)>>16)];
			*dd1++ = clp[384+((y1 + c1)>>16)];
			
			//down-left
			y2 = tab_76309[*pyy2++];
			*dd2++ = clp[384+((y2 + c4)>>16)]; 
			*dd2++ = clp[384+((y2 - c2 - c3)>>16)];
			*dd2++ = clp[384+((y2 + c1)>>16)];
			
			//up-right
			y1 = tab_76309[*pyy1++];
			*dd1++ = clp[384+((y1 + c4)>>16)]; 
			*dd1++ = clp[384+((y1 - c2 - c3)>>16)];
			*dd1++ = clp[384+((y1 + c1)>>16)];
			
			//down-right
			y2 = tab_76309[*pyy2++];
			*dd2++ = clp[384+((y2 + c4)>>16)]; 
			*dd2++ = clp[384+((y2 - c2 - c3)>>16)];
			*dd2++ = clp[384+((y2 + c1)>>16)];

/*
			//up-left
			y1=*py1++;
			*d1++ =1 * y1 - 0.0009267*(u-128) + 1.4016868*(v-128) ;
			*d1++ =1 * y1 - 0.3436954*(u-128) - 0.7141690*(v-128) ;
			*d1++ =1 * y1 + 1.7721604*(u-128) + 0.0009902*(v-128) ;

			//down-left
			y2=*py2++;
			*d2++ =1 * y2 - 0.0009267*(u-128) + 1.4016868*(v-128) ;
			*d2++ =1 * y2 - 0.3436954*(u-128) - 0.7141690*(v-128) ;
			*d2++ =1 * y2 + 1.7721604*(u-128) + 0.0009902*(v-128) ;


			//up-right
			y1=*py1++;
			*d1++ = 1 * y1 - 0.0009267*(u-128) + 1.4016868*(v-128) ;
			*d1++ =1 * y1 - 0.3436954*(u-128) - 0.7141690*(v-128) ;
			*d1++ =1 * y1 + 1.7721604*(u-128) + 0.0009902*(v-128) ;

			//down-right
			y2=*py2++;
			*d2++ =1 * y2 - 0.0009267*(u-128) + 1.4016868*(v-128) ;
			*d2++ =1 * y2 - 0.3436954*(u-128) - 0.7141690*(v-128) ;
			*d2++ =1 * y2 + 1.7721604*(u-128) + 0.0009902*(v-128) ;
*/
		}
		d1 -= 6*width;
		d2 -= 6*width;
		py1+= 2*stride;
		py2+= 2*stride;
		u  += stride/2;
		v  += stride/2;
	} 
	
}


void x264_csp_init( int cpu, int i_csp, x264_csp_function_t *pf )
{
    switch( i_csp )
    {
        case X264_CSP_I420:
		case X264_CSP_RGB:
            pf->i420 = i420_to_i420;
            pf->i422 = i422_to_i420;
            pf->i444 = i444_to_i420;
            pf->yv12 = yv12_to_i420;
            pf->yuyv = yuyv_to_i420;
            pf->rgb  = rgb_to_i420;
            pf->bgr  = bgr_to_i420;
            pf->bgra = bgra_to_i420;
            break;

        default:
            /* For now, can't happen */
            fprintf( stderr, "arg in x264_csp_init\n" );
            exit( -1 );
            break;
    }
}


void
rgb555_to_yv12_c(uint8_t * y_out,
                 uint8_t * u_out,
                 uint8_t * v_out,
                 uint8_t * src,
                 int width,
                 int height,
                 int y_stride)
{
    int32_t src_stride = width * 2;
    uint32_t y_dif = y_stride - width;
    uint32_t uv_dif = (y_stride - width) / 2;
    uint32_t x, y;

    if (height < 0) {
        height = -height;
        src += (height - 1) * src_stride;
        src_stride = -src_stride;
    }


    for (y = height / 2; y; y--) {
        /* process one 2x2 block per iteration */
        for (x = 0; x < (uint32_t) width; x += 2) {
            int rgb, r, g, b, r4, g4, b4;

            rgb = *(uint16_t *) (src + x * 2);
            b4 = b = (rgb << 3) & 0xf8;
            g4 = g = (rgb >> 2) & 0xf8;
            r4 = r = (rgb >> 7) & 0xf8;
            y_out[0] =
                (uint8_t) ((FIX_IN(Y_R_IN) * r + FIX_IN(Y_G_IN) * g +
                FIX_IN(Y_B_IN) * b) >> SCALEBITS_IN) + Y_ADD_IN;

            rgb = *(uint16_t *) (src + x * 2 + src_stride);
            b4 += b = (rgb << 3) & 0xf8;
            g4 += g = (rgb >> 2) & 0xf8;
            r4 += r = (rgb >> 7) & 0xf8;
            y_out[y_stride] =
                (uint8_t) ((FIX_IN(Y_R_IN) * r + FIX_IN(Y_G_IN) * g +
                FIX_IN(Y_B_IN) * b) >> SCALEBITS_IN) + Y_ADD_IN;

            rgb = *(uint16_t *) (src + x * 2 + 2);
            b4 += b = (rgb << 3) & 0xf8;
            g4 += g = (rgb >> 2) & 0xf8;
            r4 += r = (rgb >> 7) & 0xf8;
            y_out[1] =
                (uint8_t) ((FIX_IN(Y_R_IN) * r + FIX_IN(Y_G_IN) * g +
                FIX_IN(Y_B_IN) * b) >> SCALEBITS_IN) + Y_ADD_IN;

            rgb = *(uint16_t *) (src + x * 2 + src_stride + 2);
            b4 += b = (rgb << 3) & 0xf8;
            g4 += g = (rgb >> 2) & 0xf8;
            r4 += r = (rgb >> 7) & 0xf8;
            y_out[y_stride + 1] =
                (uint8_t) ((FIX_IN(Y_R_IN) * r + FIX_IN(Y_G_IN) * g +
                FIX_IN(Y_B_IN) * b) >> SCALEBITS_IN) + Y_ADD_IN;

            *u_out++ =
                (uint8_t) ((-FIX_IN(U_R_IN) * r4 - FIX_IN(U_G_IN) * g4 +
                FIX_IN(U_B_IN) * b4) >> (SCALEBITS_IN + 2)) +
                U_ADD_IN;


            *v_out++ =
                (uint8_t) ((FIX_IN(V_R_IN) * r4 - FIX_IN(V_G_IN) * g4 -
                FIX_IN(V_B_IN) * b4) >> (SCALEBITS_IN + 2)) +
                V_ADD_IN;

            y_out += 2;
        }
        src += src_stride * 2;
        y_out += y_dif + y_stride;
        u_out += uv_dif;
        v_out += uv_dif;
    }
}

void yv12_to_rgb555_c(uint8_t * dst,
                      int dst_stride,
                      uint8_t * y_src,
                      uint8_t * u_src,
                      uint8_t * v_src,
                      int y_stride,
                      int uv_stride,
                      int width,
                      int height)
{
    const uint32_t dst_dif = 4 * dst_stride - 2 * width;
    int32_t y_dif = 2 * y_stride - width;

    uint8_t *dst2 = dst + 2 * dst_stride;
    uint8_t *y_src2 = y_src + y_stride;
    uint32_t x, y;

    if (height < 0) {
        height = -height;
        y_src += (height - 1) * y_stride;
        y_src2 = y_src - y_stride;
        u_src += (height / 2 - 1) * uv_stride;
        v_src += (height / 2 - 1) * uv_stride;
        y_dif = -width - 2 * y_stride;
        uv_stride = -uv_stride;
    }

    for (y = height / 2; y; y--) 
    {
        int r, g, b;
        int r2, g2, b2;

        r = g = b = 0;
        r2 = g2 = b2 = 0;

        /* process one 2x2 block per iteration */
        for (x = 0; x < (uint32_t) width / 2; x++) 
        {
            int u, v;
            int b_u, g_uv, r_v, rgb_y;

            u = u_src[x];
            v = v_src[x];

            b_u = B_U_tab[u];
            g_uv = G_U_tab[u] + G_V_tab[v];
            r_v = R_V_tab[v];

            rgb_y = RGB_Y_tab[*y_src];
            b = (b & 0x7) + ((rgb_y + b_u) >> SCALEBITS_OUT);
            g = (g & 0x7) + ((rgb_y - g_uv) >> SCALEBITS_OUT);
            r = (r & 0x7) + ((rgb_y + r_v) >> SCALEBITS_OUT);
            *(uint16_t *) dst = MK_RGB555(r, g, b);

            y_src++;
            rgb_y = RGB_Y_tab[*y_src];
            b = (b & 0x7) + ((rgb_y + b_u) >> SCALEBITS_OUT);
            g = (g & 0x7) + ((rgb_y - g_uv) >> SCALEBITS_OUT);
            r = (r & 0x7) + ((rgb_y + r_v) >> SCALEBITS_OUT);
            *(uint16_t *) (dst + 2) = MK_RGB555(r, g, b);
            y_src++;

            rgb_y = RGB_Y_tab[*y_src2];
            b2 = (b2 & 0x7) + ((rgb_y + b_u) >> SCALEBITS_OUT);
            g2 = (g2 & 0x7) + ((rgb_y - g_uv) >> SCALEBITS_OUT);
            r2 = (r2 & 0x7) + ((rgb_y + r_v) >> SCALEBITS_OUT);
            *(uint16_t *) (dst2) = MK_RGB555(r2, g2, b2);
            y_src2++;

            rgb_y = RGB_Y_tab[*y_src2];
            b2 = (b2 & 0x7) + ((rgb_y + b_u) >> SCALEBITS_OUT);
            g2 = (g2 & 0x7) + ((rgb_y - g_uv) >> SCALEBITS_OUT);
            r2 = (r2 & 0x7) + ((rgb_y + r_v) >> SCALEBITS_OUT);
            *(uint16_t *) (dst2 + 2) = MK_RGB555(r2, g2, b2);
            y_src2++;

            dst += 4;
            dst2 += 4;
        }

        dst += dst_dif;
        dst2 += dst_dif;

        y_src += y_dif;
        y_src2 += y_dif;

        u_src += uv_stride;
        v_src += uv_stride;
    }
}
