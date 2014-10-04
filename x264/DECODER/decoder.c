/*****************************************************************************
* x264: h264 decoder
*****************************************************************************
* Copyright (C) 2005 xia wei ping etc.
* $Id: decoder. 2005/04/4 13:27:28 264 Exp $
*
* Authors: xia wei ping <wpxia@ict.ac.cn>
* Modified by:li hengzhong
* 
*****************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include<ASSERT.H>




#include "common/common.h"
#include "common/cpu.h"
#include "common/vlc.h"
#include "common/csp.h"

#include "macroblock.h"
#include "common/macroblock.h"   
#include "set.h"
#include "vlc.h"

#include "dec_cabac.h"
#include "common/image.h"


#define DATA_MAX 300000
/*

#include <stdlib.h>

FILE *fp;*/

static int g_bIsInitConvtTblNew = 0;

static void x264_slice_idr( x264_t *h )
{
    int i;

    h->i_poc_msb = 0;
    h->i_poc_lsb = 0;
    h->i_frame_offset = 0;
    h->i_frame_num = 0;

    if( h->sps )
    {
        for( i = 0; i < h->sps->i_num_ref_frames + 1; i++ )
        {
            h->frames.reference[i]->i_poc = -1;
        }

        h->fdec = h->frames.reference[0];
        h->i_ref0 = 0;
        h->i_ref1 = 0;
    }
}

/* The slice reading is split in two part:
*      - before ref_pic_list_reordering( )
*      - after  dec_ref_pic_marking( )
*/
static int x264_slice_header_part1_read( bs_t *s,
                                        x264_slice_header_t *sh, x264_sps_t sps_array[32], x264_pps_t pps_array[256], int b_idr )
{
    sh->i_first_mb = bs_read_ue( s );
    sh->i_type = bs_read_ue( s );
    if( sh->i_type >= 5 )
    {
        sh->i_type -= 5;
    }
    sh->i_pps_id = bs_read_ue( s );
    if( bs_eof( s ) || sh->i_pps_id >= 256 || pps_array[sh->i_pps_id].i_id == -1 )
    {
        fprintf( stderr, "invalid pps_id in slice header\n" );
        return -1;
    }

    sh->pps = &pps_array[sh->i_pps_id];
    sh->sps = &sps_array[sh->pps->i_sps_id];    /* valid if pps valid */

    sh->i_frame_num = bs_read( s, sh->sps->i_log2_max_frame_num );
    if( !sh->sps->b_frame_mbs_only )
    {
        sh->b_field_pic = bs_read1( s );
        if( sh->b_field_pic )
        {
            sh->b_bottom_field = bs_read1( s );
        }
    }

    if( b_idr )
    {
        sh->i_idr_pic_id = bs_read_ue( s );
    }
    else
    {
        sh->i_idr_pic_id = 0;
    }

    if( sh->sps->i_poc_type == 0 )
    {
        sh->i_poc_lsb = bs_read( s, sh->sps->i_log2_max_poc_lsb );
        if( sh->pps->b_pic_order && !sh->b_field_pic )
        {
            sh->i_delta_poc_bottom = bs_read_se( s );
        }
    }
    else if( sh->sps->i_poc_type == 1 && !sh->sps->b_delta_pic_order_always_zero )
    {
        sh->i_delta_poc[0] = bs_read_se( s );
        if( sh->pps->b_pic_order && !sh->b_field_pic )
        {
            sh->i_delta_poc[1] = bs_read_se( s );
        }
    }

    if( sh->pps->b_redundant_pic_cnt )
    {
        sh->i_redundant_pic_cnt = bs_read_ue( s );
    }
    else
        sh->i_redundant_pic_cnt = 0;  

    if( sh->i_type == SLICE_TYPE_B )
    {
        sh->b_direct_spatial_mv_pred = bs_read1( s );
    }

    if( sh->i_type == SLICE_TYPE_P || sh->i_type == SLICE_TYPE_SP || sh->i_type == SLICE_TYPE_B )
    {
        sh->b_num_ref_idx_override = bs_read1( s );

        sh->i_num_ref_idx_l0_active = sh->pps->i_num_ref_idx_l0_active; /* default */
        sh->i_num_ref_idx_l1_active = sh->pps->i_num_ref_idx_l1_active; /* default */

        if( sh->b_num_ref_idx_override )
        {
            sh->i_num_ref_idx_l0_active = bs_read_ue( s ) + 1;
            if( sh->i_type == SLICE_TYPE_B )
            {
                sh->i_num_ref_idx_l1_active = bs_read_ue( s ) + 1;
            }
        }
    }

    return bs_eof( s ) ? -1 : 0;
}

static int x264_slice_header_part2_read( bs_t *s, x264_slice_header_t *sh )
{
    if( sh->pps->b_cabac && sh->i_type != SLICE_TYPE_I && sh->i_type != SLICE_TYPE_SI )
    {
        sh->i_cabac_init_idc = bs_read_ue( s );
    }
    sh->i_qp_delta = bs_read_se( s );

    if( sh->i_type == SLICE_TYPE_SI || sh->i_type == SLICE_TYPE_SP )
    {
        if( sh->i_type == SLICE_TYPE_SP )
        {
            sh->b_sp_for_swidth = bs_read1( s );
        }
        sh->i_qs_delta = bs_read_se( s );
    }

    if( sh->pps->b_deblocking_filter_control )
    {
        sh->i_disable_deblocking_filter_idc = bs_read_ue( s );
        if( sh->i_disable_deblocking_filter_idc != 1 )
        {
            sh->i_alpha_c0_offset = bs_read_se( s );
            sh->i_beta_offset = bs_read_se( s );
        }
    }
    else
    {
        sh->i_alpha_c0_offset = 0;
        sh->i_beta_offset = 0;
    }

    if( sh->pps->i_num_slice_groups > 1 && sh->pps->i_slice_group_map_type >= 3 && sh->pps->i_slice_group_map_type <= 5 )
    {
        /* FIXME */
        return -1;
    }
    return 0;
}

static int x264_slice_header_ref_pic_reordering( x264_t *h, bs_t *s )
{
    int b_ok;
    int i;

    /* use the no more use frame */
    h->fdec = h->frames.reference[0];
    h->fdec->i_poc = h->i_poc;

    /* build ref list 0/1 */
    h->i_ref0 = 0;
    h->i_ref1 = 0;
    for( i = 1; i < h->sps->i_num_ref_frames + 1; i++ )
    {
        if( h->frames.reference[i]->i_poc >= 0 )
        {
            if( h->frames.reference[i]->i_poc < h->fdec->i_poc )
            {
                h->fref0[h->i_ref0++] = h->frames.reference[i];
            }
            else if( h->frames.reference[i]->i_poc > h->fdec->i_poc )
            {
                h->fref1[h->i_ref1++] = h->frames.reference[i];
            }
        }
    }

    /* Order ref0 from higher to lower poc */
    do
    {
        b_ok = 1;
        for( i = 0; i < h->i_ref0 - 1; i++ )
        {
            if( h->fref0[i]->i_poc < h->fref0[i+1]->i_poc )
            {
                x264_frame_t *tmp = h->fref0[i+1];

                h->fref0[i+1] = h->fref0[i];
                h->fref0[i] = tmp;
                b_ok = 0;
                break;
            }
        }
    } while( !b_ok );
    /* Order ref1 from lower to higher poc (bubble sort) for B-frame */
    do
    {
        b_ok = 1;
        for( i = 0; i < h->i_ref1 - 1; i++ )
        {
            if( h->fref1[i]->i_poc > h->fref1[i+1]->i_poc )
            {
                x264_frame_t *tmp = h->fref1[i+1];

                h->fref1[i+1] = h->fref1[i];
                h->fref1[i] = tmp;
                b_ok = 0;
                break;
            }
        }
    } while( !b_ok );

    if( h->i_ref0 > h->pps->i_num_ref_idx_l0_active )
    {
        h->i_ref0 = h->pps->i_num_ref_idx_l0_active;
    }
    if( h->i_ref1 > h->pps->i_num_ref_idx_l1_active )
    {
        h->i_ref1 = h->pps->i_num_ref_idx_l1_active;
    }

    //fprintf( stderr,"POC:%d ref0=%d POC0=%d\n", h->fdec->i_poc, h->i_ref0, h->i_ref0 > 0 ? h->fref0[0]->i_poc : -1 );


    /* Now parse the stream and change the default order */
    if( h->sh.i_type != SLICE_TYPE_I && h->sh.i_type != SLICE_TYPE_SI )
    {
        int b_reorder = bs_read1( s );

        if( b_reorder )
        {
            /* FIXME */
            return -1;
        }
    }
    if( h->sh.i_type == SLICE_TYPE_B )
    {
        int b_reorder = bs_read1( s );
        if( b_reorder )
        {
            /* FIXME */
            return -1;
        }
    }
    return 0;
}

static int x264_slice_header_pred_weight_table( x264_t *h, bs_t *s )
{
    return -1;
}

static int  x264_slice_header_dec_ref_pic_marking( x264_t *h, bs_t *s, int i_nal_type  )
{
    if( i_nal_type == NAL_SLICE_IDR )
    {
        int b_no_output_of_prior_pics = bs_read1( s );
        int b_long_term_reference_flag = bs_read1( s );

        /* TODO */
        if( b_no_output_of_prior_pics )
        {

        }

        if( b_long_term_reference_flag )
        {

        }
    }
    else
    {
        int b_adaptive_ref_pic_marking_mode = bs_read1( s );
        if( b_adaptive_ref_pic_marking_mode )
        {
            return -1;
        }
    }
    return 0;
}

/****************************************************************************
* Decode a slice header and setup h for mb decoding.
****************************************************************************/
static int x264_slice_header_decode( x264_t *h, bs_t *s, x264_nal_t *nal )
{
    /* read the first part of the slice */
    if( x264_slice_header_part1_read( s, &h->sh,
        h->sps_array, h->pps_array,
        nal->i_type == NAL_SLICE_IDR ? 1 : 0 ) < 0 )
    {
        fprintf( stderr, "x264_slice_header_part1_read failed\n" );
        return -1;
    }

    /* now reset h if needed for this frame */
    if( h->sps != h->sh.sps || h->pps != h->sh.pps )
    {
        //int i;
        /* TODO */

        h->sps = NULL;
        h->pps = NULL;

    }

    /* and init if needed */
    if( h->sps == NULL || h->pps == NULL )
    {
        int i;

        h->sps = h->sh.sps;
        h->pps = h->sh.pps;
        //以下是设置序列头和图像头的部分。  
        h->param.i_width = 16 * h->sps->i_mb_width;
        h->param.i_height= 16 * h->sps->i_mb_height;    
        h->param.b_cabac = h->sh.pps->b_cabac;

        h->mb.i_mb_count = h->sps->i_mb_width * h->sps->i_mb_height;

        h->mb.mv_min[0] =
            h->mb.mv_min[1] = -100*32;
        h->mb.mv_max[0] =
            h->mb.mv_max[1] = 100*32;
        h->frames.i_max_ref0 =0;
        //其他参数是否需要有待验证 

        // fprintf( stderr, "x264: %dx%d\n", h->param.i_width, h->param.i_height );

        // h->mb = x264_macroblocks_new( h->sps->i_mb_width, h->sps->i_mb_height );
        // this function does not exist!

        for( i = 0; i < h->sps->i_num_ref_frames + 1; i++ )
        {
            h->frames.reference[i] = x264_frame_new( h );
            h->frames.reference[i]->i_poc = -1;
        }
        h->fdec = h->frames.reference[0];
        h->i_ref0 = 0;
        h->i_ref1 = 0;

        h->i_poc_msb = 0;
        h->i_poc_lsb = 0;
        h->i_frame_offset = 0;
        h->i_frame_num = 0;
        x264_macroblock_cache_init( h );

        //CAVLC表的初始化信息
        if(h->param.b_cabac ==0)
        {
            for( i = 0; i < 5; i++ )
            {
                // max 2 step 
                h->x264_coeff_token_lookup[i] = x264_vlc_table_lookup_new( x264_coeff_token[i], 17*4, 4 );
            }
            // max 2 step 
            h->x264_level_prefix_lookup = x264_vlc_table_lookup_new( x264_level_prefix, 16, 8 );

            for( i = 0; i < 15; i++ )
            {
                // max 1 step 
                h->x264_total_zeros_lookup[i] = x264_vlc_table_lookup_new( x264_total_zeros[i], 16, 9 );
            }
            for( i = 0;i < 3; i++ )
            {
                // max 1 step 
                h->x264_total_zeros_dc_lookup[i] = x264_vlc_table_lookup_new( x264_total_zeros_dc[i], 4, 3 );
            }
            for( i = 0;i < 7; i++ )
            {
                // max 2 step 
                h->x264_run_before_lookup[i] = x264_vlc_table_lookup_new( x264_run_before[i], 15, 6 );
            }
        }

    }

    /* calculate poc for current frame */
    if( h->sps->i_poc_type == 0 )
    {
        int i_max_poc_lsb = 1 << h->sps->i_log2_max_poc_lsb;

        if( h->sh.i_poc_lsb < h->i_poc_lsb && h->i_poc_lsb - h->sh.i_poc_lsb >= i_max_poc_lsb/2 )
        {
            h->i_poc_msb += i_max_poc_lsb;
        }
        else if( h->sh.i_poc_lsb > h->i_poc_lsb  && h->sh.i_poc_lsb - h->i_poc_lsb > i_max_poc_lsb/2 )
        {
            h->i_poc_msb -= i_max_poc_lsb;
        }
        h->i_poc_lsb = h->sh.i_poc_lsb;

        h->i_poc = h->i_poc_msb + h->sh.i_poc_lsb;
    }
    else if( h->sps->i_poc_type == 1 )
    {
        /* FIXME */
        return -1;
    }
    else
    {
        if( nal->i_type == NAL_SLICE_IDR )
        {
            h->i_frame_offset = 0;
            h->i_poc = 0;
        }
        else
        {
            if( h->sh.i_frame_num < h->i_frame_num )
            {
                h->i_frame_offset += 1 << h->sps->i_log2_max_frame_num;
            }
            if( nal->i_ref_idc > 0 )
            {
                h->i_poc = 2 * ( h->i_frame_offset + h->sh.i_frame_num );
            }
            else
            {
                h->i_poc = 2 * ( h->i_frame_offset + h->sh.i_frame_num ) - 1;
            }
        }
        h->i_frame_num = h->sh.i_frame_num;
    }

    /*  fprintf( stderr, "x264: pic type=%s poc:%d\n",
    h->sh.i_type == SLICE_TYPE_I ? "I" : (h->sh.i_type == SLICE_TYPE_P ? "P" : "B?" ),
    h->i_poc );

    if( h->sh.i_type != SLICE_TYPE_I && h->sh.i_type != SLICE_TYPE_P )
    {
    fprintf( stderr, "only SLICE I/P supported\n" );
    return -1;
    }
    */
    /* read and do the ref pic reordering */
    if( x264_slice_header_ref_pic_reordering( h, s ) < 0 )
    {
        return -1;
    }

    if( ( (h->sh.i_type == SLICE_TYPE_P || h->sh.i_type == SLICE_TYPE_SP) && h->sh.pps->b_weighted_pred  ) ||
        ( h->sh.i_type == SLICE_TYPE_B && h->sh.pps->b_weighted_bipred ) )
    {
        if( x264_slice_header_pred_weight_table( h, s ) < 0 )
        {
            return -1;
        }
    }

    if( nal->i_ref_idc != 0 )
    {
        x264_slice_header_dec_ref_pic_marking( h, s, nal->i_type );
    }

    if( x264_slice_header_part2_read( s, &h->sh ) < 0 )
    {
        return -1;
    }
    //以下加入slice初始化信息
    x264_macroblock_slice_init( h );

    return 0;
}


/*********************************************************
函数功能：解码一个slice中的data（除去head部分）
输入参数：h：x264结构体
s：cabac码流结构体

*********************************************************/
static int x264_slice_data_decode( x264_t *h, bs_t *s )
{
    // int mb_xy = h->sh.i_first_mb;
    int i_ret = 0;
    x264_macroblock_t *mb;
    int  skip, end_of_slice;

    mb = &h->mb;
    mb->i_mb_xy = h->sh.i_first_mb;
    i_ret= 0;
    h->mb.i_last_qp = h->pps->i_pic_init_qp + h->sh.i_qp_delta; //add by lihengzhong 2005-4-8
    h->mb.i_last_dqp = 0;

    if( h->pps->b_cabac )
    {
        /* TODO: alignement and cabac init */
        /* alignment needed */
        bs_align(s);  //add by lehengzhong 2005-4-8

        /* init cabac */
        x264_cabac_context_init( &h->cabac, h->sh.i_type, h->pps->i_pic_init_qp+h->sh.i_qp_delta, h->sh.i_cabac_init_idc );
        x264_cabac_decode_init ( &h->cabac, s );
    }


    /* FIXME field decoding */
    while(h->mb.i_mb_xy< h->sps->i_mb_width * h->sps->i_mb_height )
    {
        const int i_mb_y = h->mb.i_mb_xy / h->sps->i_mb_width;
        const int i_mb_x = h->mb.i_mb_xy % h->sps->i_mb_width;


        /* load cache */
        x264_macroblock_decode_cache_load( h, i_mb_x, i_mb_y );// initialize the cache data structure 2005-4-9

        if( h->pps->b_cabac )
        {
            /*            if( h->sh.i_type != SLICE_TYPE_I && h->sh.i_type != SLICE_TYPE_SI )
            {
            /* TODO */
            /*           }
            i_ret = x264_macroblock_read_cabac( h, s, mb ); */

            if( h->mb.i_mb_xy > 0 )
            {
                /* not end_of_slice_flag */
                end_of_slice=x264_cabac_decode_terminal( &h->cabac);
                
				//assert(end_of_slice == 0);

            }
            skip = x264dec_mb_read_cabac(h,s);


            /*	while( skip > 0 )
            {
            x264_macroblock_decode_skip( h);

            // next macroblock 
            mb_xy++;
            if( mb_xy >= h->sps->i_mb_width * h->sps->i_mb_height )
            {
            break;
            }

            skip--;
            skip = x264dec_mb_read_cabac(h,s);
            }
            */	

        }
        else
        {
            if( h->sh.i_type != SLICE_TYPE_I && h->sh.i_type != SLICE_TYPE_SI )
            {
                int i_skip = bs_read_ue( s );

                while( i_skip > 0 )
                {
                    x264_macroblock_decode_skip( h );

                    /* next macroblock */
                    h->mb.i_mb_xy++;

                    i_skip--;
                }

            }
            i_ret = x264_macroblock_read_cavlc( h, s, mb );
        }

        if( i_ret < 0 )
        {
            fprintf( stderr, "x264_macroblock_read failed [%d,%d]\n", mb->i_mb_x, mb->i_mb_y );
            break;
        }

        if(skip==0) 
        {
            if( x264_macroblock_decode( h, mb ) < 0 )
            {
                fprintf( stderr, "x264_macroblock_decode failed\n" );			
            }
        }

        x264_macroblock_cache_save(h);
        h->mb.i_mb_xy++;
    }

    if( h->pps->b_cabac)
    {   	
        end_of_slice=x264_cabac_decode_terminal( &h->cabac);
        //assert(end_of_slice == 1);
        bs_align(s);
   	}

    if( i_ret >= 0 )
    {
        int i;


        /* apply deblocking filter to the current decoded picture */
        if( h->pps->b_deblocking_filter_control || h->sh.i_disable_deblocking_filter_idc != 1 )
        {
            x264_frame_deblocking_filter( h, h->sh.i_type );
        }
        /* expand border for frame reference TODO avoid it when using b-frame */
        x264_frame_expand_border( h->fdec );

        /* create filtered images */
        x264_frame_filter( h->param.cpu, h->fdec ); 

        /* expand border of filtered images */
        x264_frame_expand_border_filtered( h->fdec );


        /*
        // for test

        fp = fopen( "test.yuv", "ab+" );

        for( i = 0; i < 3;i++ )
        {
        int i_line;
        int i_div;
        i_div = i==0 ? 1 : 2;
        for( i_line = 0; i_line < h->param.i_height/i_div; i_line++ )
        {
        fwrite( h->fdec->plane[i]+i_line*h->fdec->i_stride[i], 1, h->param.i_width/i_div, fp );
        }
        }
        fclose( fp );*/



        h->picture->img.i_plane = h->fdec->i_plane;
        for( i = 0; i < h->picture->img.i_plane; i++ )
        {
            h->picture->img.i_stride[i] = h->fdec->i_stride[i];
            h->picture->img.plane[i]    = h->fdec->plane[i];
        }

        /* move frame in the buffer FIXME won't work for B-frame */
        h->fdec = h->frames.reference[h->sps->i_num_ref_frames];
        for( i = h->sps->i_num_ref_frames; i > 0; i-- )
        {
            h->frames.reference[i] = h->frames.reference[i-1];
        }
        h->frames.reference[0] = h->fdec;
    }

    return i_ret;
}
/****************************************************************************
*
******************************* x264 libs **********************************
*
****************************************************************************/



/****************************************************************************
* x264_decoder_open:
函数功能：为解码初始化一些结构体，参数
****************************************************************************/
void *x264_decoder_open()
{
	x264_t *h = x264_malloc( sizeof( x264_t ) );
	int i;

	if ( !g_bIsInitConvtTblNew )
	{
		g_bIsInitConvtTblNew = 1;
		InitConvtTblNew();
	}

	//memcpy( &h->param, param, sizeof( x264_param_t ) );
	memset( &h->param, 0, sizeof( x264_param_t ) );

	h->param.i_csp = X264_CSP_I420;
	h->param.vui.i_sar_width = 0;
	h->param.vui.i_sar_height= 0;
	h->param.i_fps_den       = 1;
	h->param.i_maxframes     = 0;
	h->param.i_level_idc     = 40;

	h->param.i_frame_reference = 1;
	h->param.i_keyint_max = 250;
	h->param.i_keyint_min = 4;

	h->param.i_bframe = 0;
	h->param.i_scenecut_threshold = 40;
	h->param.b_bframe_adaptive = 1;
	h->param.i_bframe_bias = 0;
	h->param.b_bframe_pyramid = 0;
	h->param.b_cabac = 1;

	h->param.cpu = x264_cpu_detect(); 

	//x264_param_default(&h->param);


	//InitConvtTbl();

	/* no SPS and PPS active yet */
	h->sps = NULL;
	h->pps = NULL;
	h->out.nal[0].p_payload =x264_malloc( DATA_MAX );

	for( i = 0; i < 32; i++ )
	{
		h->sps_array[i].i_id = -1;  /* invalidate it */
	}
	for( i = 0; i < 256; i++ )
	{
		h->pps_array[i].i_id = -1;  /* invalidate it */
	}

	h->picture = x264_malloc( sizeof( x264_picture_t ) );

	x264_predict_16x16_init( h->param.cpu, h->predict_16x16 );
	x264_predict_8x8_init( h->param.cpu, h->predict_8x8 );
	x264_predict_4x4_init( h->param.cpu, h->predict_4x4 );

	x264_pixel_init( h->param.cpu, &h->pixf );
	x264_dct_init( h->param.cpu, &h->dctf );

	x264_mc_init( h->param.cpu, &h->mc );

	x264_quan_init( h->param.cpu,  h );

	/* create the vlc table (we could remove it from x264_t but it will need
	* to introduce a x264_init() for global librarie) */
	//此表只是暂时的引用
	/*	for( i = 0; i < 5; i++ )
	{
	// max 2 step 
	h->x264_coeff_token_lookup[i] = x264_vlc_table_lookup_new( x264_coeff_token[i], 17*4, 4 );
	}
	// max 2 step 
	h->x264_level_prefix_lookup = x264_vlc_table_lookup_new( x264_level_prefix, 16, 8 );

	for( i = 0; i < 15; i++ )
	{
	// max 1 step 
	h->x264_total_zeros_lookup[i] = x264_vlc_table_lookup_new( x264_total_zeros[i], 16, 9 );
	}
	for( i = 0;i < 3; i++ )
	{
	// max 1 step 
	h->x264_total_zeros_dc_lookup[i] = x264_vlc_table_lookup_new( x264_total_zeros_dc[i], 4, 3 );
	}
	for( i = 0;i < 7; i++ )
	{
	// max 2 step 
	h->x264_run_before_lookup[i] = x264_vlc_table_lookup_new( x264_run_before[i], 15, 6 );
	}
	*/
	return h;
}

/****************************************************************************
* x264_decoder_decode: decode one nal unit
函数功能：解码不同的nal数据
****************************************************************************/
int     x264_decoder_decode( x264_t *h,
                            x264_picture_t **pp_pic, x264_nal_t *nal )
{
    int i_ret = 0;
    bs_t bs;

    /* no picture */
    *pp_pic = NULL;

    /* init bitstream reader */
    bs_init( &bs, nal->p_payload, nal->i_payload );

    switch( nal->i_type )
    {
    case NAL_SPS:
        if( ( i_ret = x264_sps_read( &bs, h->sps_array ) ) < 0 )
        {
            fprintf( stderr, "x264: x264_sps_read failed\n" );
        }
        break;

    case NAL_PPS:
        if( ( i_ret = x264_pps_read( &bs, h->pps_array ) ) < 0 )
        {
            fprintf( stderr, "x264: x264_pps_read failed\n" );
        }
        break;

    case NAL_SLICE_IDR:
        //  fprintf( stderr, "x264: NAL_SLICE_IDR\n" );
        x264_slice_idr( h );

    case NAL_SLICE:
        if( ( i_ret = x264_slice_header_decode( h, &bs, nal ) ) < 0 )
        {
            fprintf( stderr, "x264: x264_slice_header_decode failed\n" );
        }
        if( h->sh.i_redundant_pic_cnt == 0 && i_ret == 0 )
        {
            if( ( i_ret = x264_slice_data_decode( h, &bs ) ) < 0 )
            {
                fprintf( stderr, "x264: x264_slice_data_decode failed\n" );
            }
            else
            {
                *pp_pic = h->picture;
            }
        }

        //更新cabac的模型2005-4-10
        if( h->param.b_cabac )
        {
            x264_cabac_model_update( &h->cabac, h->sh.i_type, h->sh.pps->i_pic_init_qp + h->sh.i_qp_delta );
        }
        break;

    case NAL_SLICE_DPA:
    case NAL_SLICE_DPB:
    case NAL_SLICE_DPC:
        fprintf( stderr, "partitioned stream unsupported\n" );
        i_ret = -1;
        break;

    case NAL_SEI:
    default:
        break;
    }

    /* restore CPU state (before using float again) */
    x264_cpu_restore( h->param.cpu );

    return i_ret;
}

/****************************************************************************
* x264_decoder_decode: decode one nal unit
函数功能：解码不同的nal数据
****************************************************************************/
int     x264_decoder_skip( x264_t *h,
                          x264_picture_t **pp_pic )
{
    int i,i_ret = 0;
    x264_macroblock_t *mb;
    int  skip, end_of_slice;
    int i_mb_count = h->mb.i_mb_count;
    /* no picture */
    *pp_pic = NULL;

    /* use the no more use frame */
    h->fdec = h->frames.reference[0];
    h->fdec->i_poc = h->i_poc+2;
    h->i_poc = h->i_poc+2;

    /* build ref list 0/1 */
    h->i_ref0 = 0;
    h->i_ref1 = 0;
    for( i = 1; i < h->sps->i_num_ref_frames + 1; i++ )
    {
        if( h->frames.reference[i]->i_poc >= 0 )
        {
            if( h->frames.reference[i]->i_poc < h->fdec->i_poc )
            {
                h->fref0[h->i_ref0++] = h->frames.reference[i];
            }
            else if( h->frames.reference[i]->i_poc > h->fdec->i_poc )
            {
                h->fref1[h->i_ref1++] = h->frames.reference[i];
            }
        }
    }
    //此处需要将上一次的运动信息等拷贝到当前帧中做运动估计用
    memcpy( h->fdec->mv[0],h->mb.mv[0],2*16 * i_mb_count * sizeof( int16_t ) );
    // memcpy( h->fdec->mv[1],h->mb.mv[1],2*16 * i_mb_count * sizeof( int16_t ) );
    memcpy( h->fdec->ref[0],h->mb.ref[0],4 * i_mb_count * sizeof( int8_t ));
    // memcpy( h->fdec->ref[1],h->mb.ref[1],4 * i_mb_count * sizeof( int8_t ));
    memcpy( h->fdec->mb_type,h->mb.type,i_mb_count * sizeof( int8_t) );

    //以下加入slice初始化信息
    x264_macroblock_slice_init( h );

    mb = &h->mb;
    mb->i_mb_xy = h->sh.i_first_mb;
    i_ret= 0;

    /*  按照宏块的方式逐个解码 */
    while(h->mb.i_mb_xy< h->sps->i_mb_width * h->sps->i_mb_height )
    {
        const int i_mb_y = h->mb.i_mb_xy / h->sps->i_mb_width;
        const int i_mb_x = h->mb.i_mb_xy % h->sps->i_mb_width;

        /* load cache */
        x264_macroblock_decode_lostcache_load( h, i_mb_x, i_mb_y );// initialize the cache data structure 2005-4-9

        x264_macroblock_decode_lostskip( h );           

        //x264_macroblock_cache_save(h);
        h->mb.i_mb_xy++;
        /*	if(h->mb.i_mb_xy > 390)
        i_ret= 0;
        else
        i_ret= 1;
        */		
    }

    h->sh.i_type  =  SLICE_TYPE_P;
    /* apply deblocking filter to the current decoded picture */
    if( !h->pps->b_deblocking_filter_control || h->sh.i_disable_deblocking_filter_idc != 1 )
    {
        x264_frame_deblocking_filter( h, h->sh.i_type );
    } //以上是决定该帧是否需要做deblocking，可以先不做

    /* expand border for frame reference TODO avoid it when using b-frame */
    x264_frame_expand_border( h->fdec );

    /* create filtered images */
    x264_frame_filter( h->param.cpu, h->fdec ); // added by xia wei ping 2005-4-14

    /* expand border of filtered images */
    x264_frame_expand_border_filtered( h->fdec );

    h->picture->img.i_plane = h->fdec->i_plane;
    for( i = 0; i < h->picture->img.i_plane; i++ )
    {
        h->picture->img.i_stride[i] = h->fdec->i_stride[i];
        h->picture->img.plane[i]    = h->fdec->plane[i];
    }

    /* move frame in the buffer FIXME won't work for B-frame */
    h->fdec = h->frames.reference[h->sps->i_num_ref_frames];
    for( i = h->sps->i_num_ref_frames; i > 0; i-- )
    {
        h->frames.reference[i] = h->frames.reference[i-1];
    }
    h->frames.reference[0] = h->fdec;   

    *pp_pic = h->picture; //返回解码后的指针

    /* restore CPU state (before using float again) */
    x264_cpu_restore( h->param.cpu );
    return i_ret;
}

/****************************************************************************
* x264_decoder_close:
函数功能：解码结束后，释放分配的内存
****************************************************************************/
void    x264_decoder_close(void *handle)
{
    int i;
    x264_t* h = (x264_t*)handle;

	if(NULL == h)
    {
		return;
    }
	if(NULL != h->sps)
	{
		for(i = 0; i < h->sps->i_num_ref_frames + 1; i++)
		{
			x264_frame_delete(h->frames.reference[i]);
            h->frames.reference[i] = NULL;
		}
	}

    /* free vlc table */
    //if(h->param.b_cabac ==0)
    //{
    //    for( i = 0; i < 5; i++ )
    //    {
    //        x264_vlc_table_lookup_delete( h->x264_coeff_token_lookup[i] );
    //    }
    //    x264_vlc_table_lookup_delete( h->x264_level_prefix_lookup );

    //    for( i = 0; i < 15; i++ )
    //    {
    //        x264_vlc_table_lookup_delete( h->x264_total_zeros_lookup[i] );
    //    }
    //    for( i = 0;i < 3; i++ )
    //    {
    //        x264_vlc_table_lookup_delete( h->x264_total_zeros_dc_lookup[i] );
    //    }
    //    for( i = 0;i < 7; i++ )
    //    {
    //        x264_vlc_table_lookup_delete( h->x264_run_before_lookup[i] );
    //    }
    //}   

    //x264_picture_clean( h->picture );
    x264_free(h->picture);
    h->picture = NULL;
    x264_free( h );
}


/******************************************************************
函数说明：对h.264编码数据解码操作，如果解码成功，apDstBuff参数中输出实际的编码数据，
aiDstBuffLen参数输出实际编码数据的长度
参数：  char *apSrcData	解码前的h264数据
int aiSrcDataLen	解码前的数据长度
char *apDstBuff【输出】	解码后的输出缓冲区
int& aiDstBuffLen【输入/输出】	输入时，表示输出缓冲区的最大长度
输出时，表示缓冲区数据的实际长度
返回值：int	1＝成功；
0 = 输出缓冲区大小过小，aiDstBuffLen返回需要的缓存区大小
其他错误返回值0


******************************************************************/
int DeCodec(void *handle,char *apSrcData,int aiSrcDataLen,char *apDstBuff,int *aiDstBuffLen,int *abMark)
{

    int i_data;	
    x264_t *h;

    x264_picture_t *pic;

    uint8_t *p, *p_next, *end;

    uint8_t *p_dst;
    int size_dst;
	//int bIsVga;

    size_dst=0;

    i_data  = aiSrcDataLen;

    p = &apSrcData[0];
    end = &apSrcData[aiSrcDataLen];

    h=(x264_t *)handle;

	////////
	// 如果是640*480使用mmx编码会有崩溃，原因待查
	//bIsVga = (h->param.i_height >= 480);
	////////////

    if(i_data==0)  //处理帧丢失的情况
    {
        // decode the content of the nal 
        if(x264_decoder_skip( h, &pic )<0)
        {
            return 0;
        }

        if( pic != NULL )
        {
            int width,height;
            width=h->param.i_width;
            height=h->param.i_height;
            p_dst=apDstBuff+size_dst;
            size_dst+=width*height*3;
            if(size_dst>*aiDstBuffLen)
            {
                return 0;//-1;
            }

			//// 如果是640*480使用mmx编码会有崩溃，原因待查
   //         if ( /*!bIsVga &&*/ (h->param.cpu & X264_CPU_MMXEXT) )
   //         {
   //             yv12_to_rgb24_mmx(p_dst,
   //                 width,
   //                 pic->img.plane[0],
   //                 pic->img.plane[1],
   //                 pic->img.plane[2],
   //                 pic->img.i_stride[0],
   //                 pic->img.i_stride[0]/2,
   //                 width,
   //                 -1*height
   //                 );
   //         }
   //         else
   //         {
   //             YUV2RGB420(&pic->img, p_dst,width,height,pic->img.i_stride[0]);
   //         }
			
			// 使用周斌提供的函数
			{
				int stride_uv = pic->img.i_stride[0] / 2;
				I420_2_RGB24(p_dst,
							 pic->img.plane[0], pic->img.i_stride[0],
							 pic->img.plane[1], stride_uv,
							 pic->img.plane[2], stride_uv,
							 width, height, width
							);
			}
        }

        *abMark=0;
        size_dst=h->param.i_width*h->param.i_height*3;

        if(size_dst<=*aiDstBuffLen)
        {
            *aiDstBuffLen=size_dst;
        }

        return 1;
    }


    while( 1 )
    {

        int i_size;
        /* fill buffer */        

        if( i_data < 3 )
        {
            break;
        }

        /* extract one nal */

        while( p < end - 3 )
        {
            if( p[0] == 0x00 && p[1] == 0x00 && p[2] == 0x01 )
            {
                break;
            }
            p++;
        }

        if( p >= end - 3 )
        {
            fprintf( stderr, "garbage (i_data = %d)\n", i_data );
            i_data = 0;
            return 0; //continue;
        }

        p_next = p + 3;
        while( p_next < end - 3 )
        {
            if( p_next[0] == 0x00 && p_next[1] == 0x00 && p_next[2] == 0x01 )
            {
                break;
            }
            p_next++;
        }

        if( p_next == end - 3 )//&& i_data < DATA_MAX )
        {
            p_next = end;
        }

        /* decode this nal */
        i_size = p_next - p - 3;
        if( i_size <= 0 )
        {
            /*            if( b_eof )
            {
            break;
            }
            fprintf( stderr, "nal too large (FIXME) ?\n" );
            i_data = 0;
            continue;
            */
            return 0;
        }

        x264_nal_decode( &h->out.nal[0], p +3, i_size );

        /* decode the content of the nal */
        if(x264_decoder_decode( h, &pic, &h->out.nal[0] )<0)
        {
            return 0;
        }

        if( pic != NULL )
        {
            int width,height;
            width=h->param.i_width;
            height=h->param.i_height;
            p_dst=apDstBuff+size_dst;
            size_dst+=width*height*3;
            if(size_dst>*aiDstBuffLen)
            {
                return 0;//-1;
            }
    
            if (h->param.i_colordepth == 24)
            {
//                 if ( /*!bIsVga &&*/ (h->param.cpu & X264_CPU_MMXEXT) )
//                 {
//                     yv12_to_rgb24_mmx(p_dst,
//                                     width,
//                                     pic->img.plane[0],
//                                     pic->img.plane[1],
//                                     pic->img.plane[2],
//                                     pic->img.i_stride[0],
//                                     pic->img.i_stride[0]/2,
//                                     width,
//                                     -1*height);
//                 }
//                 else
//                 {
//                     YUV2RGB420(&pic->img, p_dst,width,height,pic->img.i_stride[0]);
//                 }

				// 使用周斌提供的函数
				{
					int stride_uv = pic->img.i_stride[0] / 2;
					I420_2_RGB24(p_dst,
								 pic->img.plane[0], pic->img.i_stride[0],
								 pic->img.plane[1], stride_uv,
								 pic->img.plane[2], stride_uv,
								 width, height, width
							    );
				}

            }
            else if (h->param.i_colordepth == 16)
            {
                yv12_to_rgb555_c(p_dst,
                                 width,
                                 pic->img.plane[0],
                                 pic->img.plane[1],
                                 pic->img.plane[2],
                                 pic->img.i_stride[0],
                                 pic->img.i_stride[0]/2,
                                 width,
                                 -1*height);
            }
        }

        i_data -= i_size+3;
        p=p_next;
    }

    *abMark=0;
    if(h->out.nal[0].i_type==NAL_SLICE_IDR||h->out.nal[0].i_type==NAL_SLICE)
    {
        *abMark=1;
    }

    if(size_dst<=*aiDstBuffLen)
    {
        *aiDstBuffLen=size_dst;

    }

    if(size_dst==0)
    {
        return 0;
    }
    return 1;

}

void x264_decode_close(void *h)
{
    FILE* fp = fopen("decode.log", "ab");
    if (fp != NULL)
    {
        fprintf(fp, "decoder close begin\n");
    }
	if(NULL == h)
		return;

    x264_free( ((x264_t *)h)->out.nal[0].p_payload ); 
    ((x264_t *)h)->out.nal[0].p_payload = NULL;

    if (fp != NULL)
    {
        fprintf(fp, "x264_decoder_close begin\n");
    }
    x264_decoder_close( (x264_t *)h );
    if (fp != NULL)
    {
        fprintf(fp, "x264_decoder_close end\n");
    }
}

