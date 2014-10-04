/*****************************************************************************
 * csp.h: h264 encoder library
 *****************************************************************************
 * Copyright (C) 2004 Laurent Aimar
 * $Id: csp.h,v 1.1.1.1 2005/03/16 13:27:22 264 Exp $
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

#ifndef _CSP_H
#define _CSP_H 1

typedef struct
{
    void (*i420)( x264_frame_t *, x264_image_t *, int i_width, int i_height );
    void (*i422)( x264_frame_t *, x264_image_t *, int i_width, int i_height );
    void (*i444)( x264_frame_t *, x264_image_t *, int i_width, int i_height );
    void (*yv12)( x264_frame_t *, x264_image_t *, int i_width, int i_height );
    void (*yuyv)( x264_frame_t *, x264_image_t *, int i_width, int i_height );
    void (*rgb )( x264_frame_t *, x264_image_t *, int i_width, int i_height );
    void (*bgr )( x264_frame_t *, x264_image_t *, int i_width, int i_height );
    void (*bgra)( x264_frame_t *, x264_image_t *, int i_width, int i_height );
} x264_csp_function_t;


void x264_csp_init( int cpu, int i_csp, x264_csp_function_t *pf );

void InitConvtTbl();

void plane_copy( uint8_t *dst, int i_dst,
							  uint8_t *src, int i_src, int w, int h);

void YUV2RGB420(x264_image_t *img, unsigned char *dst_ori,
				int width,int height,int stride);
void RGB2YUV420( x264_image_t *img, unsigned char *src_ori, 
                  int i_width, int i_height,int stride ) ;

void rgb24_to_yv12_mmx(uint8_t * const y_out,
						uint8_t * const u_out,
						uint8_t * const v_out,
						const uint8_t * const src,
						const uint32_t width,
						const uint32_t height,
						const uint32_t stride);

void yv12_to_rgb24_mmx(uint8_t *dst, 
                       int dst_stride,
                       uint8_t *y_src, 
                       uint8_t *u_src, 
                       uint8_t * v_src, 
                       int y_stride, 
                       int uv_stride,
                       int width, 
                       int height);

void rgb555_to_yv12_c(uint8_t * y_out,
                      uint8_t * u_out,
                      uint8_t * v_out,
                      uint8_t * src,
                      int width,
                      int height,
                      int y_stride);

void yv12_to_rgb555_c(uint8_t * dst,
                      int dst_stride,
                      uint8_t * y_src,
                      uint8_t * u_src,
                      uint8_t * v_src,
                      int y_stride,
                      int uv_stride,
                      int width,
                      int height);


#endif

