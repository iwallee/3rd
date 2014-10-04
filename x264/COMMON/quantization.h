/*****************************************************************************
 * quantization.h: h264 encoder library
 *****************************************************************************
 * Copyright (C) 2005 Xia Wei-ping
 * $Id: dct.h,v 1.1.1.1 2005/04/18 13:27:22 264 Exp $
 *
 * Authors: Xia wei ping <wpxia@ict.ac.cn>
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

#ifndef _QUANTIZATION_H
#define _QUANTIZATION_H 1

typedef void (*x264_quant_4x4_t)( int16_t dct[4][4], int i_qscale, int b_intra );
typedef void (*x264_quant_4x4_dc_t)( int16_t dct[4][4], int i_qscale);
typedef void (*x264_dequant_4x4_t)( int16_t dct[4][4], int i_qscale);
typedef void (*x264_dequant_4x4_dc_t)( int16_t dct[4][4], int i_qscale);

typedef void (*x264_dequant_2x2_dc_t)( int16_t dct[2][2], int i_qscale);
typedef void (*x264_quant_2x2_dc_t)( int16_t dct[2][2], int i_qscale, int b_intra);



void x264_quan_init( int cpu,  x264_t *h );

#endif
