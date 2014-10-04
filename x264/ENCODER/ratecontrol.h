/*****************************************************************************
 * ratecontrol.h: h264 encoder library (Rate Control)
 *****************************************************************************
 * Copyright (C) 2003 Laurent Aimar
 * $Id: ratecontrol.h,v 1.1.1.1 2005/03/16 13:27:22 264 Exp $
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

#ifndef _RATECONTROL_H
#define _RATECONTROL_H 1

typedef struct
{
	int pict_type;
	int kept_as_ref;
	float qscale;
	int mv_bits;
	int i_tex_bits;
	int p_tex_bits;
	int misc_bits;
	uint64_t expected_bits;
	float new_qscale;
	int new_qp;
	int i_count;
	int p_count;
	int s_count;
	float blurred_complexity;
} ratecontrol_entry_t;

//码率控制的数据结构
struct x264_ratecontrol_t
{
	/* constants */
	double fps;  //帧率
	int gop_size;//关键帧间隔
	int bitrate;
	int nmb;                    /* number of macroblocks in a frame */
	int buffer_size;
	int rcbufrate;
	int init_qp;
	int qp_constant[5];

	/* 1st pass stuff */
	int gop_qp;
	int buffer_fullness;
	int frames;                 /* 当前图像组内的图像帧数 */
	int pframes;
	int slice_type;
	int mb;                     /* 已经处理完的图像宏块数 */
	int bits_gop;               /* 当前图像组需要的比特数 */
	int bits_last_gop;          /* bits consumed in gop */
	int qp;                     /* qp for current frame */
	int qpm;                    /* qp for next MB */
	float qpa;                  /* average qp for last frame */
	int qps;
	float qp_avg_p;             /* average QP for P frames */
	float qp_last_p;
	int fbits;                  /* bits allocated for current frame */
	int ufbits;                 /* bits used for current frame */
	int nzcoeffs;               /* # of 0-quantized coefficients */
	int ncoeffs;                /* total # of coefficients */
	int overhead;
	int qp_force;

	/* 2pass stuff */
	FILE *p_stat_file_out;
	char *psz_stat_file_tmpname;

	int num_entries;            /* number of ratecontrol_entry_ts */
	ratecontrol_entry_t *entry; /* FIXME: copy needed data and free this once init is done */
	double last_qscale;
	double last_qscale_for[5];  /* last qscale for a specific pict type, used for max_diff & ipb factor stuff  */
	int last_non_b_pict_type;
	double accum_p_qp;          /* for determining I-frame quant */
	double accum_p_norm;
	double last_accum_p_norm;
	double lmin[5];             /* min qscale by frame type */
	double lmax[5];
	double lstep;               /* max change (multiply) in qscale per frame */
	double i_cplx_sum[5];       /* estimated total texture bits in intra MBs at qscale=1 */
	double p_cplx_sum[5];
	double mv_bits_sum[5];
	int frame_count[5];         /* number of frames of each type */
};

int  x264_ratecontrol_new   ( x264_t * );
void x264_ratecontrol_delete( x264_t * );

void x264_ratecontrol_start( x264_t *, int i_slice_type, int i_force_qp );
int  x264_ratecontrol_slice_type( x264_t *, int i_frame );
void x264_ratecontrol_mb( x264_t *, int bits );
int  x264_ratecontrol_qp( x264_t * );
void x264_ratecontrol_end( x264_t *, int bits );

#endif

