/*****************************************************************************
* x264: h264 encoder
*****************************************************************************
* H.264的编码模块
*
*****************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <math.h>

#include "common/common.h"
#include "common/cpu.h"

#include "set.h"
#include "analyse.h"
#include "ratecontrol.h"
#include "macroblock.h"

//#define __USE__FFMPEG_X264__	(1)

#ifdef _DEBUG
#define DEBUG_LOGFILE
//#define DEBUG_MB_TYPE
//#define DEBUG_DUMP_FRAME
#define DEBUG_BENCHMARK
#endif

#ifdef DEBUG_MB_TYPE
static int64_t i_macroblock_count_Total = 0;
static int64_t i_macroblock_count_I = 0;
static int64_t i_macroblock_count_P = 0;
static int64_t i_macroblock_count_S = 0;
#endif

#ifdef DEBUG_BENCHMARK
static int64_t i_mtime_encode_frame = 0;
static int64_t i_mtime_analyse = 0;
static int64_t i_mtime_encode = 0;
static int64_t i_mtime_write = 0;
static int64_t i_mtime_filter = 0;
#define TIMER_START( d ) \
{ \
    int64_t d##start = x264_mdate();

#define TIMER_STOP( d ) \
    d += x264_mdate() - d##start;\
}
#else
#define TIMER_START( d )
#define TIMER_STOP( d )
#endif

#define NALU_OVERHEAD 5 // startcode + NAL type costs 5 bytes per frame

/****************************************************************************
*
******************************* x264 libs **********************************
*
****************************************************************************/

////////////////////////// from ffmpeg

#if defined(__USE__FFMPEG_X264__) && __USE__FFMPEG_X264__

/// 创建一个编码器
extern void * __stdcall av_create_codec();

/// 打开编码器
extern int __stdcall av_open_encode(void * ahandle, 
									unsigned int auWidth, unsigned int auHeight, unsigned int auColorBit, 
									unsigned int auKbps, unsigned int auFps, unsigned int auKeyInterval);

/// 打开解码器
extern int __stdcall av_open_decode(void * ahandle, 
									unsigned int auWidth, unsigned int auHeight, unsigned int auColorBit, 
									unsigned int auKbps, unsigned int auFps, unsigned int auKeyInterval);

/// 关闭
extern void __stdcall av_close_codec(void * ahandle);

/// 编码
extern int __stdcall av_encode_frame(void * ahandle,
										const unsigned char * apSrc, 
										unsigned char * apOut, unsigned int * auOutLen,
										int * auIsKeyFrame /* in out */);

/// 解码
extern int __stdcall av_decode_frame(void * ahandle,
										const unsigned char * apSrc, unsigned int auSrcLen, 
										unsigned char * apOut, unsigned int * auOutLen, 
										int * auIsKeyFrame /* out */);

/// 是否打开
extern int __stdcall av_codec_is_opened(void * ahandle);

/// 删除自身及相关资源
extern void __stdcall av_release_codec(void * ahandle);

#endif
//////////////////////////////////////////////////////////////////////


static void x264_log_default( void *p_unused, int i_level, const char *psz_fmt, va_list arg )
{
    char *psz_prefix;
#ifdef DEBUG_LOGFILE
	FILE *f;
	char *psz_name = "info.txt";
#endif
    switch( i_level )
    {
    case X264_LOG_ERROR:
        psz_prefix = "error";
        break;
    case X264_LOG_WARNING:
        psz_prefix = "warning";
        break;
    case X264_LOG_INFO:
        psz_prefix = "info";
        break;
    case X264_LOG_DEBUG:
        psz_prefix = "debug";
        break;
    default:
        psz_prefix = "unknown";
        break;
    }
    fprintf( stderr, "x264 [%s]: ", psz_prefix );
    vfprintf( stderr, psz_fmt, arg );
#ifdef DEBUG_LOGFILE
	f = fopen(psz_name, "a+");
	if(f)
	{
		fprintf( f, "x264 [%s]: ", psz_prefix );
		vfprintf( f, psz_fmt, arg );
	}
	fclose(f);
#endif
}

static int64_t x264_sqe( x264_t *h, uint8_t *pix1, int i_pix_stride, uint8_t *pix2, int i_pix2_stride, int i_width, int i_height )
{
    int64_t i_sqe = 0;
    int x, y;

#define SSD(size) i_sqe += h->pixf.ssd[size]( pix1+y*i_pix_stride+x, i_pix_stride, \
    pix2+y*i_pix2_stride+x, i_pix2_stride );
    for( y = 0; y < i_height-15; y += 16 )
    {
        for( x = 0; x < i_width-15; x += 16 )
            SSD(PIXEL_16x16);
        if( x < i_width-7 )
            SSD(PIXEL_8x16);
    }
    if( y < i_height-7 )
        for( x = 0; x < i_width-7; x += 8 )
            SSD(PIXEL_8x8);
#undef SSD
    x264_cpu_restore( h->param.cpu );

    return i_sqe;
}

static float x264_mse( int64_t i_sqe, int64_t i_size )
{
    return (double)i_sqe / ((double)65025.0 * (double)i_size);
}

static float x264_psnr( int64_t i_sqe, int64_t i_size )
{
    double f_mse = (double)i_sqe / ((double)65025.0 * (double)i_size);
    if( f_mse <= 0.0000000001 ) /* Max 100dB */
        return 100;

    return (float)(-10.0 * log( f_mse ) / log( 10.0 ));
}

#ifdef DEBUG_DUMP_FRAME
static void x264_frame_dump( x264_t *h, x264_frame_t *fr, char *name )
{
    FILE * f = fopen( name, "a" );
    int i, y;

    fseek( f, 0, SEEK_END );

    for( i = 0; i < fr->i_plane; i++ )
    {
        for( y = 0; y < h->param.i_height / ( i == 0 ? 1 : 2 ); y++ )
        {
            fwrite( &fr->plane[i][y*fr->i_stride[i]], 1, h->param.i_width / ( i == 0 ? 1 : 2 ), f );
        }
    }
    fclose( f );
}
#endif


/* Fill "default" values */
static void x264_slice_header_init( x264_t *h, x264_slice_header_t *sh,
                                   x264_sps_t *sps, x264_pps_t *pps,
                                   int i_type, int i_idr_pic_id, int i_frame, int i_qp )
{
    x264_param_t *param = &h->param;
    int i;

    /* First we fill all field */
    sh->sps = sps;
    sh->pps = pps;

    sh->i_type      = i_type;
    sh->i_first_mb  = 0;
    sh->i_pps_id    = pps->i_id;

    sh->i_frame_num = i_frame;

    sh->b_field_pic = 0;    /* Not field support for now */
    sh->b_bottom_field = 1; /* not yet used */

    sh->i_idr_pic_id = i_idr_pic_id;

    /* poc stuff, fixed later */
    sh->i_poc_lsb = 0;
    sh->i_delta_poc_bottom = 0;
    sh->i_delta_poc[0] = 0;
    sh->i_delta_poc[1] = 0;

    sh->i_redundant_pic_cnt = 0;

    sh->b_direct_spatial_mv_pred = ( param->analyse.i_direct_mv_pred == X264_DIRECT_PRED_SPATIAL );

    sh->b_num_ref_idx_override = 0;
    sh->i_num_ref_idx_l0_active = 1;
    sh->i_num_ref_idx_l1_active = 1;

    sh->b_ref_pic_list_reordering_l0 = h->b_ref_reorder[0];
    sh->b_ref_pic_list_reordering_l1 = h->b_ref_reorder[1];

    /* If the ref list isn't in the default order, construct reordering header */
    /* List1 reordering isn't needed yet */
    if( sh->b_ref_pic_list_reordering_l0 )
    {
        int pred_frame_num = i_frame;
        for( i = 0; i < h->i_ref0; i++ )
        {
            int diff = h->fref0[i]->i_frame_num - pred_frame_num;
            if( diff == 0 )
                x264_log( h, X264_LOG_ERROR, "diff frame num == 0\n" );
            sh->ref_pic_list_order[0][i].idc = ( diff > 0 );
            sh->ref_pic_list_order[0][i].arg = abs( diff ) - 1;
            pred_frame_num = h->fref0[i]->i_frame_num;
        }
    }

    sh->i_cabac_init_idc = param->i_cabac_init_idc;

    sh->i_qp_delta = i_qp - pps->i_pic_init_qp;
    sh->b_sp_for_swidth = 0;
    sh->i_qs_delta = 0;

    /* If effective qp <= 15, deblocking would have no effect anyway */
    if( param->b_deblocking_filter
        && ( h->mb.b_variable_qp
        || 15 < i_qp + X264_MAX(param->i_deblocking_filter_alphac0, param->i_deblocking_filter_beta) ) )
    {
        sh->i_disable_deblocking_filter_idc = 0;
    }
    else
    {
        sh->i_disable_deblocking_filter_idc = 1;
    }
    sh->i_alpha_c0_offset = param->i_deblocking_filter_alphac0 << 1;
    sh->i_beta_offset = param->i_deblocking_filter_beta << 1;
}

static void x264_slice_header_write( bs_t *s, x264_slice_header_t *sh, int i_nal_ref_idc )
{
    int i;

    bs_write_ue( s, sh->i_first_mb );
    bs_write_ue( s, sh->i_type + 5 );   /* same type things */
    bs_write_ue( s, sh->i_pps_id );
    bs_write( s, sh->sps->i_log2_max_frame_num, sh->i_frame_num );

    if( sh->i_idr_pic_id >= 0 ) /* NAL IDR */
    {
        bs_write_ue( s, sh->i_idr_pic_id );
    }

    if( sh->sps->i_poc_type == 0 )
    {
        bs_write( s, sh->sps->i_log2_max_poc_lsb, sh->i_poc_lsb );
        if( sh->pps->b_pic_order && !sh->b_field_pic )
        {
            bs_write_se( s, sh->i_delta_poc_bottom );
        }
    }
    else if( sh->sps->i_poc_type == 1 && !sh->sps->b_delta_pic_order_always_zero )
    {
        bs_write_se( s, sh->i_delta_poc[0] );
        if( sh->pps->b_pic_order && !sh->b_field_pic )
        {
            bs_write_se( s, sh->i_delta_poc[1] );
        }
    }

    if( sh->pps->b_redundant_pic_cnt )
    {
        bs_write_ue( s, sh->i_redundant_pic_cnt );
    }

    if( sh->i_type == SLICE_TYPE_B )
    {
        bs_write1( s, sh->b_direct_spatial_mv_pred );
    }
    if( sh->i_type == SLICE_TYPE_P || sh->i_type == SLICE_TYPE_SP || sh->i_type == SLICE_TYPE_B )
    {
        bs_write1( s, sh->b_num_ref_idx_override );
        if( sh->b_num_ref_idx_override )
        {
            bs_write_ue( s, sh->i_num_ref_idx_l0_active - 1 );
            if( sh->i_type == SLICE_TYPE_B )
            {
                bs_write_ue( s, sh->i_num_ref_idx_l1_active - 1 );
            }
        }
    }

    /* ref pic list reordering */
    if( sh->i_type != SLICE_TYPE_I )
    {
        bs_write1( s, sh->b_ref_pic_list_reordering_l0 );
        if( sh->b_ref_pic_list_reordering_l0 )
        {
            for( i = 0; i < sh->i_num_ref_idx_l0_active; i++ )
            {
                bs_write_ue( s, sh->ref_pic_list_order[0][i].idc );
                bs_write_ue( s, sh->ref_pic_list_order[0][i].arg );

            }
            bs_write_ue( s, 3 );
        }
    }
    if( sh->i_type == SLICE_TYPE_B )
    {
        bs_write1( s, sh->b_ref_pic_list_reordering_l1 );
        if( sh->b_ref_pic_list_reordering_l1 )
        {
            for( i = 0; i < sh->i_num_ref_idx_l1_active; i++ )
            {
                bs_write_ue( s, sh->ref_pic_list_order[1][i].idc );
                bs_write_ue( s, sh->ref_pic_list_order[1][i].arg );
            }
            bs_write_ue( s, 3 );
        }
    }

    if( ( sh->pps->b_weighted_pred && ( sh->i_type == SLICE_TYPE_P || sh->i_type == SLICE_TYPE_SP ) ) ||
        ( sh->pps->b_weighted_bipred == 1 && sh->i_type == SLICE_TYPE_B ) )
    {
        /* FIXME */
    }

    if( i_nal_ref_idc != 0 )
    {
        if( sh->i_idr_pic_id >= 0 )
        {
            bs_write1( s, 0 );  /* no output of prior pics flag */
            bs_write1( s, 0 );  /* long term reference flag */
        }
        else
        {
            bs_write1( s, 0 );  /* adaptive_ref_pic_marking_mode_flag */
            /* FIXME */
        }
    }

    if( sh->pps->b_cabac && sh->i_type != SLICE_TYPE_I )
    {
        bs_write_ue( s, sh->i_cabac_init_idc );
    }
    bs_write_se( s, sh->i_qp_delta );      /* slice qp delta */
#if 0
    if( sh->i_type == SLICE_TYPE_SP || sh->i_type == SLICE_TYPE_SI )
    {
        if( sh->i_type == SLICE_TYPE_SP )
        {
            bs_write1( s, sh->b_sp_for_swidth );
        }
        bs_write_se( s, sh->i_qs_delta );
    }
#endif

    if( sh->pps->b_deblocking_filter_control )
    {
        bs_write_ue( s, sh->i_disable_deblocking_filter_idc );
        if( sh->i_disable_deblocking_filter_idc != 1 )
        {
            bs_write_se( s, sh->i_alpha_c0_offset >> 1 );
            bs_write_se( s, sh->i_beta_offset >> 1 );
        }
    }
}

/****************************************************************************
*
****************************************************************************
****************************** External API*********************************
****************************************************************************
*
****************************************************************************/

/****************************************************************************
* x264_encoder_open:
****************************************************************************/
void *x264_encoder_open   (int aiFps)
{
#if defined(__USE__FFMPEG_X264__) && __USE__FFMPEG_X264__
	
	return av_create_codec();

#else
    x264_t *h = x264_malloc( sizeof( x264_t ) );
    int i, i_slice;

    /* Create a copy of param */
    // memcpy( &h->param, param, sizeof( x264_param_t ) );
    memset(&h-> param, 0, sizeof( x264_param_t ) );

    /* CPU autodetect */
    h-> param.cpu = x264_cpu_detect();

    /* Video properties */
    h->param.i_csp           = X264_CSP_I420;//X264_CSP_RGB;//X264_CSP_I420;
    h->param.i_width         = 0;
    h->param.i_height        = 0;
    h->param.vui.i_sar_width = 0;
    h->param.vui.i_sar_height= 0;
    h->param.i_fps_num       = aiFps /*9*/;
    h->param.i_fps_den       = 1;
    h->param.i_maxframes     = 0;
    h->param.i_level_idc     = 40; /* level 4.0 is sufficient for 720x576 with 
                                   16 reference frames */
    /* Encoder parameters */
    h->param.i_frame_reference = 5 /*1*/;
    h->param.i_keyint_max = 250;
    h->param.i_keyint_min = 25;
    h->param.i_bframe = 0;
    h->param.i_scenecut_threshold = 40;
    h->param.b_bframe_adaptive = 1;
    h->param.i_bframe_bias = 0;
    h->param.b_bframe_pyramid = 0;

    h->param.b_deblocking_filter = 1;
    h->param.i_deblocking_filter_alphac0 = 0;
    h->param.i_deblocking_filter_beta = 0;

    h->param.b_cabac = 1;
    h->param.i_cabac_init_idc = -1;

    h->param.rc.b_cbr = 1;
    h->param.rc.i_bitrate = 60;//  bitrate/framerate/width/height
    h->param.rc.i_rc_buffer_size = 512 /*120*/; // rc_buffer_size>bitrate/framerate
    h->param.rc.i_rc_init_buffer = h->param.rc.i_rc_buffer_size * 1000 /*60000*/;
    h->param.rc.i_rc_sens = 10;
    h->param.rc.i_qp_constant = 31 /*31*/;
    h->param.rc.i_qp_min = 10 /*22*/;
    h->param.rc.i_qp_max = 50 /*32*/;//45;
    h->param.rc.i_qp_step = 15 /*10*/;  //我认为这个可以调整
    h->param.rc.f_ip_factor = 11;
    h->param.rc.f_pb_factor = 1.3;

    h->param.rc.b_stat_write = 0;
    h->param.rc.psz_stat_out = "x264_2pass.log";
    h->param.rc.b_stat_read = 0;
    h->param.rc.psz_stat_in = "x264_2pass.log";
    h->param.rc.psz_rc_eq = "blurCplx^(1-qComp)";
    h->param.rc.f_qcompress = 0.6 /*0.6*/;
    h->param.rc.f_qblur = 0.5;
    h->param.rc.f_complexity_blur = 20;

    /* Log */
    h->param.pf_log = x264_log_default;// temperly test
    h->param.p_log_private = NULL;
    h->param.i_log_level = X264_LOG_INFO;

    /* */
    h->param.analyse.intra = X264_ANALYSE_I4x4;
    h->param.analyse.inter = X264_ANALYSE_I4x4 | X264_ANALYSE_PSUB16x16 | X264_ANALYSE_BSUB16x16;
    h->param.analyse.i_direct_mv_pred = X264_DIRECT_PRED_TEMPORAL;
    h->param.analyse.i_subpel_refine = 3;
    h->param.analyse.b_chroma_me = 1;
    h->param.analyse.i_mv_range = 512;
    h->param.analyse.i_chroma_qp_offset = 0;
#ifdef DEBUG_LOGFILE
	h->param.analyse.b_psnr = 1;
#else
	h->param.analyse.b_psnr = 0;
#endif


    // Fix parameters values 
    h->param.i_frame_reference = x264_clip3( h->param.i_frame_reference, 1, 16 );
    if( h->param.i_keyint_max <= 0 )
        h->param.i_keyint_max = 1;
    h->param.i_keyint_min = x264_clip3( h->param.i_keyint_min, 1, h->param.i_keyint_max/2+1 );

    h->param.i_bframe = x264_clip3( h->param.i_bframe, 0, X264_BFRAME_MAX );
    h->param.i_bframe_bias = x264_clip3( h->param.i_bframe_bias, -90, 100 );
    h->param.b_bframe_pyramid = h->param.b_bframe_pyramid && h->param.i_bframe > 1;
    h->frames.i_delay = h->param.i_bframe;
    h->frames.i_max_ref0 = h->param.i_frame_reference;
    h->frames.i_max_ref1 = h->param.b_bframe_pyramid ? 2
        : h->param.i_bframe ? 1 : 0;
    h->frames.i_max_dpb = h->frames.i_max_ref0 + h->frames.i_max_ref1 + 1;

    h->param.i_deblocking_filter_alphac0 = x264_clip3( h->param.i_deblocking_filter_alphac0, -6, 6 );
    h->param.i_deblocking_filter_beta    = x264_clip3( h->param.i_deblocking_filter_beta, -6, 6 );

    h->param.i_cabac_init_idc = x264_clip3( h->param.i_cabac_init_idc, -1, 2 );

    h->param.analyse.i_subpel_refine = x264_clip3( h->param.analyse.i_subpel_refine, 1, 5 );
    if( h->param.analyse.inter & X264_ANALYSE_PSUB8x8 )
        h->param.analyse.inter |= X264_ANALYSE_PSUB16x16;
    h->param.analyse.i_chroma_qp_offset = x264_clip3(h->param.analyse.i_chroma_qp_offset, -12, 12);
    h->param.analyse.i_mv_range = x264_clip3(h->param.analyse.i_mv_range, 32, 2048);

    if( h->param.rc.f_qblur < 0 )
        h->param.rc.f_qblur = 0;
    if( h->param.rc.f_complexity_blur < 0 )
        h->param.rc.f_complexity_blur = 0;

    /* Init x264_t */
    h->out.i_nal = 0;
    h->out.i_bitstream = 100000; /* FIXME estimate max size (idth/height) */
    h->out.p_bitstream = x264_malloc( h->out.i_bitstream );

    h->i_frame = 0;
    h->i_frame_num = 0;
    h->i_idr_pic_id = 0;

    /* init cabac adaptive model */
    x264_cabac_model_init( &h->cabac );

    /* init CPU functions */
    x264_predict_16x16_init( h->param.cpu, h->predict_16x16 );
    x264_predict_8x8_init( h->param.cpu, h->predict_8x8 );
    x264_predict_4x4_init( h->param.cpu, h->predict_4x4 );

    x264_pixel_init( h->param.cpu, &h->pixf );
    x264_dct_init( h->param.cpu, &h->dctf );
    x264_mc_init( h->param.cpu, &h->mc );
    x264_csp_init( h->param.cpu, h->param.i_csp, &h->csp );

    x264_quan_init( h->param.cpu,  h );


    h->i_last_intra_size = 0;
    h->i_last_inter_size = 0;

    /* stat */
    for( i_slice = 0; i_slice < 5; i_slice++ )
    {
        h->stat.i_slice_count[i_slice] = 0;
        h->stat.i_slice_size[i_slice] = 0;
        h->stat.i_slice_qp[i_slice] = 0;

        h->stat.i_sqe_global[i_slice] = 0;
        h->stat.f_psnr_average[i_slice] = 0.0;
        h->stat.f_psnr_mean_y[i_slice] = h->stat.f_psnr_mean_u[i_slice] = h->stat.f_psnr_mean_v[i_slice] = 0.0;

        for( i = 0; i < 18; i++ )
            h->stat.i_mb_count[i_slice][i] = 0;
    }

	//jlb- added at 2005-8-23
#ifdef DEBUG_LOGFILE
	x264_write_info("*---------------------LOG BEGIN--------------------*\n");
	x264_log( h, X264_LOG_INFO, "using cpu capabilities %s%s%s%s%s%s\n",
		h->param.cpu&X264_CPU_MMX ? "MMX " : "",
		h->param.cpu&X264_CPU_MMXEXT ? "MMXEXT " : "",
		h->param.cpu&X264_CPU_SSE ? "SSE " : "",
		h->param.cpu&X264_CPU_SSE2 ? "SSE2 " : "",
		h->param.cpu&X264_CPU_3DNOW ? "3DNow! " : "",
		h->param.cpu&X264_CPU_ALTIVEC ? "Altivec " : "" );  
	x264_log( h, X264_LOG_INFO, "ip_factor = %3.1f\n",h->param.rc.f_ip_factor);
#endif

    return h;

#endif
}

int x264_set_encode_info(void * ahandle, unsigned int auWidth, unsigned int auHeight, unsigned int auColorBit, 
						 unsigned int auKbps, unsigned int auFps, unsigned int auKeyInterval)
{
#if defined(__USE__FFMPEG_X264__) && __USE__FFMPEG_X264__

	return av_open_encode(ahandle, 
						  auWidth, auHeight, auColorBit, 
						  auKbps, auFps, auKeyInterval
						 );

#else

	SetFrameInfoInitial(auWidth, auHeight, auColorBit, 1, ahandle);

	return SetFrameQuant(auKbps, ahandle);

#endif
}



/******************************************************************
函数说明：设置需要编码的图像信息
参数：  int aiFrameWidth	需要编码的图像宽度
int aiFrameHeight	需要编码的图像高度
int aiBitCount	需要编码的图像的颜色深度，默认值是24
void *p_handle  编码器的指针   
返回值: －1 ：设置图像信息不正确
1   ：正确设置了图像信息
******************************************************************/

int  SetFrameInfoInitial(int aiFrameWidth,int aiFrameHeight,int aiBitCount,int abIsEncode, void *p_handle)
{
    x264_t *h;
    h = (x264_t *)p_handle;
    if (abIsEncode == 1)
    {
        int i;
        h->param.i_width =aiFrameWidth;
        h->param.i_height = aiFrameHeight;
        h->param.i_colordepth = aiBitCount;

        if(aiFrameWidth < 400)
		{
            h->i_intra_prd =1;
		}
        else
		{
            //h->i_intra_prd =0;   // 决定是否采用帧内预测的方式

			// modified by myw on 2012.1.30 我打算启用下看看压缩效果
			h->i_intra_prd = 1;   // 决定是否采用帧内预测的方式
		}

        if(aiFrameWidth > 400)
        {
            //h->param.analyse.i_subpel_refine = 2;
            //h->param.b_deblocking_filter = 0;
        }


        h->sps = &h->sps_array[0];
        x264_sps_init( h->sps, 0, &h->param );

        h->pps = &h->pps_array[0];
        x264_pps_init( h->pps, 0, &h->param, h->sps);

        h->mb.i_mb_count = h->sps->i_mb_width * h->sps->i_mb_height;

        // Init frames. 
        for( i = 0; i < X264_BFRAME_MAX + 3; i++ )
        {
            h->frames.current[i] = NULL;
            h->frames.next[i]    = NULL;
            h->frames.unused[i]  = NULL;
        }
        for( i = 0; i < 1 + h->frames.i_delay; i++ )
        {
            h->frames.unused[i] =  x264_frame_new( h );
        }
        for( i = 0; i < h->frames.i_max_dpb; i++ )
        {
            h->frames.reference[i] = x264_frame_new( h );
        }
        h->frames.reference[h->frames.i_max_dpb] = NULL;
        h->frames.i_last_idr = - 10;//h->param.i_keyint_max; //为了使得第一个IDR出现时值依然正确
        h->frames.i_input    = 0;
        h->frames.last_nonb  = NULL;

        h->i_ref0 = 0;
        h->i_ref1 = 0;

        h->fdec = h->frames.reference[0];

        /* init mb cache */
        x264_macroblock_cache_init( h );	
        // rate control 
        if( x264_ratecontrol_new( h ) < 0 )
            return -1;
        /* Create a new pic */
        h->picture = (x264_picture_t *)x264_malloc( sizeof( x264_picture_t ) );
        x264_picture_alloc(h->picture,X264_CSP_I420,aiFrameWidth,aiFrameHeight);
    }
    else
    {
        h->param.i_colordepth = aiBitCount;
    }

    return 1;
}


/* internal usage */
static void x264_nal_start( x264_t *h, int i_type, int i_ref_idc )
{
    x264_nal_t *nal = &h->out.nal[h->out.i_nal];

    nal->i_ref_idc = i_ref_idc;
    nal->i_type    = i_type;

    bs_align_0( &h->out.bs );   /* not needed */

    nal->i_payload= 0;
    nal->p_payload= &h->out.p_bitstream[bs_pos( &h->out.bs) / 8];
}

static void x264_nal_end( x264_t *h )
{
    x264_nal_t *nal = &h->out.nal[h->out.i_nal];

    bs_align_0( &h->out.bs );   /* not needed */

    nal->i_payload = &h->out.p_bitstream[bs_pos( &h->out.bs)/8] - nal->p_payload;

    h->out.i_nal++;
}

/****************************************************************************
* x264_encoder_headers:
****************************************************************************/
int x264_encoder_headers( x264_t *h, x264_nal_t **pp_nal, int *pi_nal )
{
    /* init bitstream context */
    h->out.i_nal = 0;
    bs_init( &h->out.bs, h->out.p_bitstream, h->out.i_bitstream );

    /* Put SPS and PPS */
    if( h->i_frame == 0 )
    {
        /*
        / * identify ourself * /
        x264_nal_start( h, NAL_SEI, NAL_PRIORITY_DISPOSABLE );
        x264_sei_version_write( &h->out.bs );
        x264_nal_end( h );
        */

        /* generate sequence parameters */
        x264_nal_start( h, NAL_SPS, NAL_PRIORITY_HIGHEST );
        x264_sps_write( &h->out.bs, h->sps );
        x264_nal_end( h );

        /* generate picture parameters */
        x264_nal_start( h, NAL_PPS, NAL_PRIORITY_HIGHEST );
        x264_pps_write( &h->out.bs, h->pps );
        x264_nal_end( h );
    }
    /* now set output*/
    *pi_nal = h->out.i_nal;
    *pp_nal = &h->out.nal[0];

    return 0;
}


static void x264_frame_put( x264_frame_t *list[X264_BFRAME_MAX], x264_frame_t *frame )
{
    int i = 0;
    while( list[i] ) i++;
    list[i] = frame;
}

static void x264_frame_push( x264_frame_t *list[X264_BFRAME_MAX], x264_frame_t *frame )
{
    int i = 0;
    while( list[i] ) i++;
    while( i-- )
        list[i+1] = list[i];
    list[0] = frame;
}

static x264_frame_t *x264_frame_get( x264_frame_t *list[X264_BFRAME_MAX+1] )
{
    x264_frame_t *frame = list[0];
    int i;
    for( i = 0; list[i]; i++ )
        list[i] = list[i+1];
    return frame;
}

static void x264_frame_sort( x264_frame_t *list[X264_BFRAME_MAX+1], int b_dts )
{
    int i, b_ok;
    do {
        b_ok = 1;
        for( i = 0; list[i+1]; i++ )
        {
            int dtype = list[i]->i_type - list[i+1]->i_type;
            int dtime = list[i]->i_frame - list[i+1]->i_frame;
            int swap = b_dts ? dtype > 0 || ( dtype == 0 && dtime > 0 )
                : dtime > 0;
            if( swap )
            {
                x264_frame_t *tmp = list[i+1];
                list[i+1] = list[i];
                list[i] = tmp;
                b_ok = 0;
            }
        }
    } while( !b_ok );
}
#define x264_frame_sort_dts(list) x264_frame_sort(list, 1)
#define x264_frame_sort_pts(list) x264_frame_sort(list, 0)

static inline void x264_reference_build_list( x264_t *h, int i_poc, int i_slice_type )
{
    int i;
    int b_ok;

    /* build ref list 0/1 */
    h->i_ref0 = 0;
    h->i_ref1 = 0;
    for( i = 1; i < h->frames.i_max_dpb; i++ )
    {
        if( h->frames.reference[i]->i_poc >= 0 )
        {
            if( h->frames.reference[i]->i_poc < i_poc )
            {
                h->fref0[h->i_ref0++] = h->frames.reference[i];
            }
            else if( h->frames.reference[i]->i_poc > i_poc )
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

    /* In the standard, a P-frame's ref list is sorted by frame_num.
    * We use POC, but check whether explicit reordering is needed */
    h->b_ref_reorder[0] =
        h->b_ref_reorder[1] = 0;
    if( i_slice_type == SLICE_TYPE_P )
    {
        for( i = 0; i < h->i_ref0 - 1; i++ )
            if( h->fref0[i]->i_frame_num < h->fref0[i+1]->i_frame_num )
            {
                h->b_ref_reorder[0] = 1;
                break;
            }
    }

    h->i_ref0 = X264_MIN( h->i_ref0, h->frames.i_max_ref0 );
    h->i_ref1 = X264_MIN( h->i_ref1, h->frames.i_max_ref1 );
}

static inline void x264_reference_update( x264_t *h )
{
    int i;

    /* apply deblocking filter to the current decoded picture */
    if( !h->sh.i_disable_deblocking_filter_idc )
    {
        TIMER_START( i_mtime_filter );
        x264_frame_deblocking_filter( h, h->sh.i_type );
        TIMER_STOP( i_mtime_filter );
    }
    /* expand border */
    x264_frame_expand_border( h->fdec );

    /* create filtered images */
    x264_frame_filter( h->param.cpu, h->fdec );

    /* expand border of filtered images */
    x264_frame_expand_border_filtered( h->fdec );

    /* move lowres copy of the image to the ref frame */
    /*
    for( i = 0; i < 4; i++)
    {
    uint8_t *tmp = h->fdec->lowres[i];
    h->fdec->lowres[i] = h->fenc->lowres[i];
    h->fenc->lowres[i] = tmp;
    }
    */
    //这一块是不需要用到的
    /* adaptive B decision needs a pointer, since it can't use the ref lists */
    if( h->sh.i_type != SLICE_TYPE_B )
        h->frames.last_nonb = h->fdec;

    /* move frame in the buffer */
    /* FIXME: override to forget earliest pts, not earliest dts */
    h->fdec = h->frames.reference[h->frames.i_max_dpb-1];
    for( i = h->frames.i_max_dpb-1; i > 0; i-- )
    {
        h->frames.reference[i] = h->frames.reference[i-1];
    }
    h->frames.reference[0] = h->fdec;
}

static inline void x264_reference_reset( x264_t *h )
{
    int i;

    /* reset ref pictures */
    for( i = 1; i < h->frames.i_max_dpb; i++ )
    {
        h->frames.reference[i]->i_poc = -1;
    }
    h->frames.reference[0]->i_poc = 0;
}

/******************************************************************
函数说明：根据NAL类型初始化该Slice的相关数据结构
参数：  x264_t *h	编码器的数据结构
int i_nal_type	编码的NAL类型
int i_slice_type	编码的该帧的类型
int i_global_qp    编码的全局的量化步长
返回值: 无
******************************************************************/
static inline void x264_slice_init( x264_t *h, int i_nal_type, int i_slice_type, int i_global_qp )
{
    /* ------------------------ Create slice header  ----------------------- */
    if( i_nal_type == NAL_SLICE_IDR )
    {
        x264_slice_header_init( h, &h->sh, h->sps, h->pps, i_slice_type, h->i_idr_pic_id, h->i_frame_num - 1, i_global_qp );

        /* increment id */
        h->i_idr_pic_id = ( h->i_idr_pic_id + 1 ) % 65536;
    }
    else
    {
        x264_slice_header_init( h, &h->sh, h->sps, h->pps, i_slice_type, -1, h->i_frame_num - 1, i_global_qp );

        /* always set the real higher num of ref frame used */
        h->sh.b_num_ref_idx_override = 1;
        h->sh.i_num_ref_idx_l0_active = h->i_ref0 <= 0 ? 1 : h->i_ref0;
        h->sh.i_num_ref_idx_l1_active = h->i_ref1 <= 0 ? 1 : h->i_ref1;
    }

    h->fdec->i_frame_num = h->sh.i_frame_num;

    if( h->sps->i_poc_type == 0 )
    {
        h->sh.i_poc_lsb = h->fdec->i_poc & ( (1 << h->sps->i_log2_max_poc_lsb) - 1 );
        h->sh.i_delta_poc_bottom = 0;   /* XXX won't work for field */
    }
    else if( h->sps->i_poc_type == 1 )
    {
        /* FIXME TODO FIXME */
    }
    else
    {
        /* Nothing to do ? */
    }

    /* get adapative cabac model if needed */
    if( h->param.b_cabac )
    {
        if( h->param.i_cabac_init_idc == -1 )
        {
            h->sh.i_cabac_init_idc = x264_cabac_model_get( &h->cabac, i_slice_type );
        }
    }

    x264_macroblock_slice_init( h );
}

/******************************************************************
函数说明：编码一帧图像数据并输出码流
参数：  x264_t *h	编码器的数据结构
int i_nal_type		编码的NAL类型
int i_nal_ref_idc	参考的关键帧编号

返回值: 无
******************************************************************/
static inline void x264_slice_write( x264_t *h, int i_nal_type, int i_nal_ref_idc )
{
    int i_skip;
    int mb_xy;
    int i;

    /* Init stats */
    h->stat.frame.i_hdr_bits  =
        h->stat.frame.i_itex_bits =
        h->stat.frame.i_ptex_bits =
        h->stat.frame.i_misc_bits =
        h->stat.frame.i_intra_cost =
        h->stat.frame.i_inter_cost = 0;
    for( i = 0; i < 18; i++ )
        h->stat.frame.i_mb_count[i] = 0;

    /* Slice */
    x264_nal_start( h, i_nal_type, i_nal_ref_idc );

    /* Slice header */
    x264_slice_header_write( &h->out.bs, &h->sh, i_nal_ref_idc );
    if( h->param.b_cabac )
    {
        /* alignment needed */
        bs_align_1( &h->out.bs );

        /* init cabac */
        x264_cabac_context_init( &h->cabac, h->sh.i_type, h->sh.pps->i_pic_init_qp + h->sh.i_qp_delta, h->sh.i_cabac_init_idc );
        x264_cabac_encode_init ( &h->cabac, &h->out.bs );
    }
    h->mb.i_last_qp = h->pps->i_pic_init_qp + h->sh.i_qp_delta;
    h->mb.i_last_dqp = 0;

    for( mb_xy = 0, i_skip = 0; mb_xy < h->sps->i_mb_width * h->sps->i_mb_height; mb_xy++ )
    {
        const int i_mb_y = mb_xy / h->sps->i_mb_width;
        const int i_mb_x = mb_xy % h->sps->i_mb_width;

        int mb_spos = bs_pos(&h->out.bs);

        /* load cache */
        x264_macroblock_cache_load( h, i_mb_x, i_mb_y );

        /* analyse parameters
        * Slice I: choose I_4x4 or I_16x16 mode
        * Slice P: choose between using P mode or intra (4x4 or 16x16)
        * */
        TIMER_START( i_mtime_analyse );
        x264_macroblock_analyse( h );
        TIMER_STOP( i_mtime_analyse );

        /* encode this macrobock -> be carefull it can change the mb type to P_SKIP if needed */
        TIMER_START( i_mtime_encode );
        x264_macroblock_encode( h );
        TIMER_STOP( i_mtime_encode );

        TIMER_START( i_mtime_write );
        if( IS_SKIP( h->mb.i_type ) )
        {
            if( h->param.b_cabac )
            {
                if( mb_xy > 0 )
                {
                    /* not end_of_slice_flag */
                    x264_cabac_encode_terminal( &h->cabac, 0 );
                }

                x264_cabac_mb_skip( h, 1 );
            }
            else
            {
                i_skip++;
            }
        }
        else
        {
            if( h->param.b_cabac )
            {
                if( mb_xy > 0 )
                {
                    /* not end_of_slice_flag */
                    x264_cabac_encode_terminal( &h->cabac, 0 );
                }
                if( h->sh.i_type != SLICE_TYPE_I )
                {
                    x264_cabac_mb_skip( h, 0 );
                }
                x264_macroblock_write_cabac( h, &h->out.bs );
            }
            else
            {
                if( h->sh.i_type != SLICE_TYPE_I )
                {
                    bs_write_ue( &h->out.bs, i_skip );  /* skip run */
                    i_skip = 0;
                }
                x264_macroblock_write_cavlc( h, &h->out.bs );
            }
        }
        TIMER_STOP( i_mtime_write );

        /* save cache */
        x264_macroblock_cache_save( h );

        h->stat.frame.i_mb_count[h->mb.i_type]++;

        if( h->mb.b_variable_qp )
            x264_ratecontrol_mb(h, bs_pos(&h->out.bs) - mb_spos);
    }

    if( h->param.b_cabac )
    {
        /* end of slice */
        x264_cabac_encode_terminal( &h->cabac, 1 );
    }
    else if( i_skip > 0 )
    {
        bs_write_ue( &h->out.bs, i_skip );  /* last skip run */
    }

    if( h->param.b_cabac )
    {
        int i_cabac_word;
        x264_cabac_encode_flush( &h->cabac );
        /* TODO cabac stuffing things (p209) */
        i_cabac_word = (((3 * h->cabac.i_sym_cnt - 3 * 96 * h->sps->i_mb_width * h->sps->i_mb_height)/32) - bs_pos( &h->out.bs)/8)/3;

        while( i_cabac_word > 0 )
        {
            bs_write( &h->out.bs, 16, 0x0000 );
            i_cabac_word--;
        }
    }
    else
    {
        /* rbsp_slice_trailing_bits */
        bs_rbsp_trailing( &h->out.bs );
    }

    x264_nal_end( h );

    /* Compute misc bits */
    h->stat.frame.i_misc_bits = bs_pos( &h->out.bs )
        + NALU_OVERHEAD * 8
        - h->stat.frame.i_itex_bits
        - h->stat.frame.i_ptex_bits
        - h->stat.frame.i_hdr_bits;
}

/******************************************************************
函数说明：编码一帧图像数据
参数：  x264_t *h		 编码器的数据结构
x264_nal_t **pp_nal		 编码后输出的NAL指针的地址
int *pi_nal				 编码后输出该帧图像的NAL数目
x264_picture_t *pic_in   输入的需要编码的图像帧
x264_picture_t *pic_out  输出的编码以后解码后的参考图像帧
返回值: 0 编码成功
		1 编码失败 
******************************************************************/
int x264_encoder_encode( x264_t *h,
                        x264_nal_t **pp_nal, int *pi_nal,
                        x264_picture_t *pic_in,
                        x264_picture_t *pic_out )
{
	x264_frame_t   *frame_psnr = h->fdec; /* just to keep the current decoded frame for psnr calculation */
	int     i_nal_type;
	int     i_nal_ref_idc;
	int     i_slice_type;

	int i;

	int   i_global_qp;

	char psz_message[80];

	/* no data out */
	*pi_nal = 0;
	*pp_nal = NULL;
	/* ------------------- Setup new frame from picture -------------------- */

	TIMER_START( i_mtime_encode_frame );
	if( pic_in != NULL )
	{
		/* 1: Copy the picture to a frame and move it to a buffer */
		x264_frame_t *fenc = x264_frame_get( h->frames.unused );

		x264_frame_copy_picture( h, fenc, pic_in );

		fenc->i_frame = h->frames.i_input++;

		x264_frame_put( h->frames.next, fenc );

		x264_frame_init_lowres( h->param.cpu, fenc );

		if( h->frames.i_input <= h->frames.i_delay )
		{
			/* Nothing yet to encode */
			/* waiting for filling bframe buffer */
			pic_out->i_type = X264_TYPE_AUTO;
			return 0;
		}
	}

	if( h->frames.current[0] == NULL )
	{
		int bframes = 0;
		/* 2: Select frame types */
		if( h->frames.next[0] == NULL )
			return 0;

		x264_slicetype_decide( h );

		/* 3: move some B-frames and 1 non-B to encode queue */
		while( IS_X264_TYPE_B( h->frames.next[bframes]->i_type ) )
			bframes++;
		x264_frame_put( h->frames.current, x264_frame_get( &h->frames.next[bframes] ) );
		/* FIXME: when max B-frames > 3, BREF may no longer be centered after GOP closing */
		if( h->param.b_bframe_pyramid && bframes > 1 )
		{
			x264_frame_t *mid = x264_frame_get( &h->frames.next[bframes/2] );
			mid->i_type = X264_TYPE_BREF;
			x264_frame_put( h->frames.current, mid );
			bframes--;
		}

		while( bframes-- )
			x264_frame_put( h->frames.current, x264_frame_get( h->frames.next ) );
	}
	TIMER_STOP( i_mtime_encode_frame );

	/* ------------------- Get frame to be encoded ------------------------- */
	/* 4: get picture to encode */
	h->fenc = x264_frame_get( h->frames.current );
	if( h->fenc == NULL )
	{
		/* Nothing yet to encode (ex: waiting for I/P with B frames) */
		/* waiting for filling bframe buffer */
		pic_out->i_type = X264_TYPE_AUTO;
		return 0;
	}

do_encode:

	if( h->fenc->i_type == X264_TYPE_IDR )
	{
		if(1 != h->fenc->i_frame - h->frames.i_last_idr)
			h->rc->gop_size = h->fenc->i_frame - h->frames.i_last_idr;

		h->frames.i_last_idr = h->fenc->i_frame;
	}

	/* ------------------- Setup frame context ----------------------------- */
	/* 5: Init data dependant of frame type */
	TIMER_START( i_mtime_encode_frame );
	if( h->fenc->i_type == X264_TYPE_IDR )
	{
		/* reset ref pictures */
		x264_reference_reset( h );

		i_nal_type    = NAL_SLICE_IDR;
		i_nal_ref_idc = NAL_PRIORITY_HIGHEST;
		i_slice_type = SLICE_TYPE_I;
	}
	else if( h->fenc->i_type == X264_TYPE_I )
	{
		i_nal_type    = NAL_SLICE;
		i_nal_ref_idc = NAL_PRIORITY_HIGH; /* Not completely true but for now it is (as all I/P are kept as ref)*/
		i_slice_type = SLICE_TYPE_I;
	}
	else if( h->fenc->i_type == X264_TYPE_P )
	{
		i_nal_type    = NAL_SLICE;
		i_nal_ref_idc = NAL_PRIORITY_HIGH; //NAL_PRIORITY_DISPOSABLE;/* Not completely true but for now it is (as all I/P are kept as ref)*/
		i_slice_type = SLICE_TYPE_P;
	}
	else if( h->fenc->i_type == X264_TYPE_BREF )
	{
		i_nal_type    = NAL_SLICE;
		i_nal_ref_idc = NAL_PRIORITY_HIGH; /* maybe add MMCO to forget it? -> low */
		i_slice_type = SLICE_TYPE_B;
	}
	else    /* B frame */
	{
		i_nal_type    = NAL_SLICE;
		i_nal_ref_idc = NAL_PRIORITY_DISPOSABLE;
		i_slice_type = SLICE_TYPE_B;
	}

	h->fdec->i_poc =
		h->fenc->i_poc = 2 * (h->fenc->i_frame - h->frames.i_last_idr);
	h->fdec->i_type = h->fenc->i_type;
	h->fdec->i_frame = h->fenc->i_frame;
	h->fenc->b_kept_as_ref =
		h->fdec->b_kept_as_ref = i_nal_ref_idc != NAL_PRIORITY_DISPOSABLE;

	/* ------------------- Init                ----------------------------- */
	/* Init the rate control */
	x264_ratecontrol_start( h, i_slice_type, h->fenc->i_qpplus1 );
	i_global_qp = x264_ratecontrol_qp( h );

	pic_out->i_qpplus1 =
		h->fdec->i_qpplus1 = i_global_qp + 1;

	/* build ref list 0/1 */
	x264_reference_build_list( h, h->fdec->i_poc, i_slice_type );

	if( i_slice_type == SLICE_TYPE_B )
		x264_macroblock_bipred_init( h );

	/* increase frame num but only once for B frame */
	if( i_slice_type != SLICE_TYPE_B || h->sh.i_type != SLICE_TYPE_B )
	{
		h->i_frame_num++;
	}

	/* ------------------------ Create slice header  ----------------------- */
	x264_slice_init( h, i_nal_type, i_slice_type, i_global_qp );

	/* ---------------------- Write the bitstream -------------------------- */
	/* Init bitstream context */
	h->out.i_nal = 0;
	bs_init( &h->out.bs, h->out.p_bitstream, h->out.i_bitstream );

	/* Write SPS and PPS */
	if( i_nal_type == NAL_SLICE_IDR )
	{
		/*
		if( h->fenc->i_frame == 0 )
		{
		/ * identify ourself * /
		x264_nal_start( h, NAL_SEI, NAL_PRIORITY_DISPOSABLE );
		x264_sei_version_write( &h->out.bs );
		x264_nal_end( h );
		}*/  //网络传输中不需要NAL_SEI层


		/* generate sequence parameters */
		x264_nal_start( h, NAL_SPS, NAL_PRIORITY_HIGHEST );
		x264_sps_write( &h->out.bs, h->sps );
		x264_nal_end( h );

		/* generate picture parameters */
		x264_nal_start( h, NAL_PPS, NAL_PRIORITY_HIGHEST );
		x264_pps_write( &h->out.bs, h->pps );
		x264_nal_end( h );
	}

	/* Write the slice */
	x264_slice_write( h, i_nal_type, i_nal_ref_idc );

	/* restore CPU state (before using float again) */
	x264_cpu_restore( h->param.cpu );

	if( i_slice_type == SLICE_TYPE_P && !h->param.rc.b_stat_read 
		&& h->param.i_scenecut_threshold >= 0 )
	{
		if(h->i_intra_prd)
		{
			int i_mb_i = h->stat.frame.i_mb_count[I_4x4] + h->stat.frame.i_mb_count[I_16x16];
			int i_mb_p = h->stat.frame.i_mb_count[P_L0] + h->stat.frame.i_mb_count[P_8x8];
			int i_mb_s = h->stat.frame.i_mb_count[P_SKIP];
			int i_mb   = h->sps->i_mb_width * h->sps->i_mb_height;
			int64_t i_inter_cost = h->stat.frame.i_inter_cost;
			int64_t i_intra_cost = h->stat.frame.i_intra_cost;

			float f_bias;
			int i_gop_size = h->fenc->i_frame - h->frames.i_last_idr;
			float f_thresh_max = h->param.i_scenecut_threshold / 100.0;
			/* ratio of 10 pulled out of thin air */
			float f_thresh_min = f_thresh_max * h->param.i_keyint_min
				/ ( h->param.i_keyint_max * 4 );
			if( h->param.i_keyint_min == h->param.i_keyint_max )
				f_thresh_min= f_thresh_max;

			/* macroblock_analyse() doesn't further analyse skipped mbs,
			* so we have to guess their cost */
			if( i_mb_s < i_mb )
				i_intra_cost = i_intra_cost * i_mb / (i_mb - i_mb_s);

			if( i_gop_size < h->param.i_keyint_min / 4 )
				f_bias = f_thresh_min / 4;
			else if( i_gop_size <= h->param.i_keyint_min )
				f_bias = f_thresh_min * i_gop_size / h->param.i_keyint_min;
			else
			{
				f_bias = f_thresh_min
					+ ( f_thresh_max - f_thresh_min )
					* ( i_gop_size - h->param.i_keyint_min )
					/ ( h->param.i_keyint_max - h->param.i_keyint_min );
			}
			f_bias = X264_MIN( f_bias, 1.0 );

			// Bad P will be reencoded as I 
			if( i_mb_s < i_mb &&
				i_inter_cost >= (1.0 - f_bias) * i_intra_cost )
			{
				int b;

#ifdef DEBUG_LOGFILE
				x264_log( h, X264_LOG_INFO, "scene cut at %d size=%d Icost:%.0f Pcost:%.0f ratio:%.3f bias=%.3f lastIDR:%d (I:%d P:%d Skip:%d)\n",
					h->fenc->i_frame,
					h->out.nal[h->out.i_nal-1].i_payload,
					(double)i_intra_cost, (double)i_inter_cost,
					(double)i_inter_cost / i_intra_cost,
					f_bias, i_gop_size,
					i_mb_i, i_mb_p, i_mb_s );
#endif

				h->i_frame_num--;

				for( b = 0; h->frames.current[b] && IS_X264_TYPE_B( h->frames.current[b]->i_type ); b++ );

				if( b > 0 )
				{

					if( h->param.b_bframe_adaptive || b > 1 )
						h->fenc->i_type = X264_TYPE_AUTO;
					x264_frame_sort_pts( h->frames.current );
					x264_frame_push( h->frames.next, h->fenc );
					h->fenc = h->frames.current[b-1];
					h->frames.current[b-1] = NULL;
					h->fenc->i_type = X264_TYPE_P;
					x264_frame_sort_dts( h->frames.current );
				}
				else if( i_gop_size >= h->param.i_keyint_min )
				{
					x264_frame_t *tmp;

					h->i_frame_num = 0;

					h->fenc->i_type = X264_TYPE_IDR;
					h->fenc->i_poc = 0;

					while( (tmp = x264_frame_get( h->frames.current ) ) != NULL )
						x264_frame_put( h->frames.next, tmp );

					x264_frame_sort_pts( h->frames.next );
				}
				else
				{
					h->fenc->i_type = X264_TYPE_I;
					//h->rc->gop_size = i_gop_size;
				}
				goto do_encode;
			}

		}
		h->i_last_inter_size = h->out.nal[h->out.i_nal-1].i_payload;
	}
	else
	{
		h->i_last_intra_size = h->out.nal[h->out.i_nal-1].i_payload;
		h->i_last_intra_qp = i_global_qp;
	}

	/* End bitstream, set output  */
	*pi_nal = h->out.i_nal;
	*pp_nal = &h->out.nal[0];

	/* Set output picture properties */
	if( i_slice_type == SLICE_TYPE_I )
		pic_out->i_type = i_nal_type == NAL_SLICE_IDR ? X264_TYPE_IDR : X264_TYPE_I;
	else if( i_slice_type == SLICE_TYPE_P )
		pic_out->i_type = X264_TYPE_P;
	else
		pic_out->i_type = X264_TYPE_B;
	pic_out->i_pts = h->fenc->i_pts;

	/* ---------------------- Update encoder state ------------------------- */
	/* update cabac */
	if( h->param.b_cabac )
	{
		x264_cabac_model_update( &h->cabac, i_slice_type, h->sh.pps->i_pic_init_qp + h->sh.i_qp_delta );
	}

	/* handle references */
	if( i_nal_ref_idc != NAL_PRIORITY_DISPOSABLE )
	{
		x264_reference_update( h );
	}

	/* increase frame count */
	h->i_frame++;

	/* restore CPU state (before using float again) */
	/* XXX: not needed? (done above) */
	x264_cpu_restore( h->param.cpu );

	/* update rc */
	x264_ratecontrol_end( h, h->out.nal[h->out.i_nal-1].i_payload * 8 );

	x264_frame_put( h->frames.unused, h->fenc );

	TIMER_STOP( i_mtime_encode_frame );

	/* ---------------------- Compute/Print statistics --------------------- */
	/* Slice stat */
	h->stat.i_slice_count[i_slice_type]++;
	h->stat.i_slice_size[i_slice_type] += bs_pos( &h->out.bs ) / 8 + NALU_OVERHEAD;
	h->stat.i_slice_qp[i_slice_type] += i_global_qp;

	for( i = 0; i < 18; i++ )
	{
		h->stat.i_mb_count[h->sh.i_type][i] += h->stat.frame.i_mb_count[i];
	}

	if( h->param.analyse.b_psnr )
	{
		int64_t i_sqe_y, i_sqe_u, i_sqe_v;

		/* PSNR */
		i_sqe_y = x264_sqe( h, frame_psnr->plane[0], frame_psnr->i_stride[0], h->fenc->plane[0], h->fenc->i_stride[0], h->param.i_width, h->param.i_height );
		i_sqe_u = x264_sqe( h, frame_psnr->plane[1], frame_psnr->i_stride[1], h->fenc->plane[1], h->fenc->i_stride[1], h->param.i_width/2, h->param.i_height/2);
		i_sqe_v = x264_sqe( h, frame_psnr->plane[2], frame_psnr->i_stride[2], h->fenc->plane[2], h->fenc->i_stride[2], h->param.i_width/2, h->param.i_height/2);

		h->stat.i_sqe_global[i_slice_type] += i_sqe_y + i_sqe_u + i_sqe_v;
		h->stat.f_psnr_average[i_slice_type] += x264_psnr( i_sqe_y + i_sqe_u + i_sqe_v, 3 * h->param.i_width * h->param.i_height / 2 );
		h->stat.f_psnr_mean_y[i_slice_type] += x264_psnr( i_sqe_y, h->param.i_width * h->param.i_height );
		h->stat.f_psnr_mean_u[i_slice_type] += x264_psnr( i_sqe_u, h->param.i_width * h->param.i_height / 4 );
		h->stat.f_psnr_mean_v[i_slice_type] += x264_psnr( i_sqe_v, h->param.i_width * h->param.i_height / 4 );

		snprintf( psz_message, 80, " PSNR Y:%2.2f U:%2.2f V:%2.2f",
			x264_psnr( i_sqe_y, h->param.i_width * h->param.i_height ),
			x264_psnr( i_sqe_u, h->param.i_width * h->param.i_height / 4),
			x264_psnr( i_sqe_v, h->param.i_width * h->param.i_height / 4) );
		psz_message[79] = '\0';
	}
	else
	{
		psz_message[0] = '\0';
	}

#ifdef _DEBUG
	x264_log( h, X264_LOG_INFO,
		"frame=%4d QP=%i NAL=%d Slice:%c Poc:%-3d I4x4:%-4d I16x16:%-4d P:%-4d SKIP:%-4d size=%d bytes%s\n",
		h->i_frame - 1,
		i_global_qp,
		i_nal_ref_idc,
		i_slice_type == SLICE_TYPE_I ? 'I' : (i_slice_type == SLICE_TYPE_P ? 'P' : 'B' ),
		frame_psnr->i_poc,
		h->stat.frame.i_mb_count[I_4x4],
		h->stat.frame.i_mb_count[I_16x16],
		h->stat.frame.i_mb_count_p,
		h->stat.frame.i_mb_count_skip,
		h->out.nal[h->out.i_nal-1].i_payload,
		psz_message );
#endif

#ifdef DEBUG_MB_TYPE
	{
		static const char mb_chars[] = { 'i', 'I', 'C', 'P', '8', 'S',
			'D', '<', 'X', 'B', 'X', '>', 'B', 'B', 'B', 'B', '8', 'S' };
		int mb_xy;
		for( mb_xy = 0; mb_xy < h->sps->i_mb_width * h->sps->i_mb_height; mb_xy++ )
		{
			if( h->mb.type[mb_xy] < 18 && h->mb.type[mb_xy] >= 0 )
				fprintf( stderr, "%c ", mb_chars[ h->mb.type[mb_xy] ] );
			else
				fprintf( stderr, "? " );

			if( (mb_xy+1) % h->sps->i_mb_width == 0 )
				fprintf( stderr, "\n" );
		}
	}
#endif

#ifdef DEBUG_DUMP_FRAME
	/* Dump reconstructed frame */
	x264_frame_dump( h, frame_psnr, "fdec.yuv" );
#endif
#if 0
	if( h->i_ref0 > 0 )
	{
		x264_frame_dump( h, h->fref0[0], "ref0.yuv" );
	}
	if( h->i_ref1 > 0 )
	{
		x264_frame_dump( h, h->fref1[0], "ref1.yuv" );
	}
#endif
	return 0;
}

/******************************************************************
函数说明：编码缓冲区中的RGB数据
参数：  void *p_handle	编码器的数据结构
char *apSrcData	编码前的缓冲区指针
int aiSrcDataLen	需要编码的数据长度
char *apDstBuff, 编码后的数据指针，存放编码后的结果
int *aiDstBuffLen  输出的编码以后数据大小
int *abMark   输出编码的该帧是否是关键帧
返回值: 1 编码成功
0 编码失败 

******************************************************************/

int  x264_encode_frame(void *p_handle,char *apSrcData,int aiSrcDataLen,char *apDstBuff,int *aiDstBuffLen,int *abMark)
{

#if defined(__USE__FFMPEG_X264__) && __USE__FFMPEG_X264__

	return av_encode_frame(p_handle,
						   (const unsigned char *)apSrcData, 
						   (unsigned char *)apDstBuff, (unsigned int*)aiDstBuffLen,
						   abMark
						  );
#else

    int     framenum,framelength,i_temp,i;
    int         i_nal;
    x264_nal_t  *nal;
    char *pdst;
    int outbitnum;
    x264_t *h;
	
	////////////
	//int bIsVga;	// 标志是否为vga
	////////////

    h=(x264_t *)p_handle;

	//*abMark = 1;

    if(h->param.i_csp==X264_CSP_RGB)
    {
        framelength=h->param.i_height * h->param.i_width * 3;
    }
    else if(h->param.i_csp==X264_CSP_I420)
    {
        if (h->param.i_colordepth == 24)
        {
            framelength = h->param.i_height * h->param.i_width * 3;
        }
        else if(h->param.i_colordepth == 16)
        {
            framelength = h->param.i_height * h->param.i_width * 2;
        }
    }
    framenum = aiSrcDataLen/ framelength; 
    pdst = apDstBuff;	
    outbitnum =0;

	////////
	// 如果是640*480使用mmx编码会有崩溃，原因待查
	//bIsVga = (h->param.i_height >= 480);
	////////////

    for(i_temp=0;i_temp<framenum;i_temp++)
   	{
        if(h->param.i_csp ==X264_CSP_RGB)
        {
            h->picture->img.plane[0]=apSrcData+ i_temp * framelength;
            h->picture->img.i_plane = 1;
            h->picture->img.i_stride[0] = 3 * h->param.i_width ;
        }
        else if(h->param.i_csp==X264_CSP_I420)
        {
            
            h->picture->img.i_plane = 3;
            if (h->param.i_colordepth == 24)
            {
				// 如果是640*480使用mmx编码会有崩溃，原因待查
#if 0
                if ( !bIsVga && (h->param.cpu & X264_CPU_MMXEXT) )
                {
                    rgb24_to_yv12_mmx(h->picture->img.plane[0],
                                    h->picture->img.plane[1],
                                    h->picture->img.plane[2],
                                    apSrcData+ i_temp * framelength,
                                    h->param.i_width,
                                    h->param.i_height,
                                    h->param.i_width );
                }
                else
#endif
                {
                    RGB2YUV420(&h->picture->img,apSrcData+ i_temp * framelength,h->param.i_width,h->param.i_height,h->param.i_width);
                }
            }
            else if (h->param.i_colordepth == 16)
            {
                rgb555_to_yv12_c(h->picture->img.plane[0],
                                 h->picture->img.plane[1],
                                 h->picture->img.plane[2],
                                 apSrcData+ i_temp * framelength,
                                 h->param.i_width,
                                 h->param.i_height,
                                 h->param.i_width);
            }

            h->picture->img.i_stride[0] = h->param.i_width ;
            h->picture->img.i_stride[1] = h->param.i_width  / 2;
            h->picture->img.i_stride[2] = h->param.i_width  / 2;	  	  

        }

        //强制编码关键帧，在*abMark为一的情况下
        if(*abMark==1)
        {
            h->picture->i_type = X264_TYPE_IDR;
        }
        else
        {
            h->picture->i_type = X264_TYPE_AUTO;
        }

        h->picture->i_qpplus1 = 0;

        if( x264_encoder_encode( h, &nal, &i_nal, h->picture,h->picture ) < 0 )
        {
            fprintf( stderr, "x264_encoder_encode failed\n" );
            return 0;
        }

        for( i = 0; i < i_nal; i++ )
        {
            int i_size;
            int i_data;

            i_data = 1000000;
            if( ( i_size = x264_nal_encode( pdst, &i_data, 1, &nal[i] ) ) > 0 )
            {
                outbitnum+=i_size;
                if(outbitnum > *aiDstBuffLen) 
                {
                    return 0;
                }
                pdst +=i_data;
            }
            else if( i_size < 0 )
            {
                fprintf( stderr,
                    "need to increase buffer size (size=%d)\n", -i_size );
            }
        }

   	}

    if(h->picture->i_type == X264_TYPE_IDR || h->picture->i_type ==X264_TYPE_I)
        *abMark =1;
    else
        *abMark =0;

    *aiDstBuffLen=outbitnum;
    return 1;

#endif
}

/******************************************************************
函数说明：关闭编码的数据结构并释放内存空间
参数：  void *p_handle	编码器的数据结构指针

返回值: 无

******************************************************************/
void x264_encoder_close(void *p_handle)
{
#if defined(__USE__FFMPEG_X264__) && __USE__FFMPEG_X264__
	av_close_codec(p_handle);
	av_release_codec(p_handle);

#else
    x264_t* h;
    int i;
    int64_t i_mtime_total;
    int64_t i_yuv_size;

    h = (x264_t *)p_handle;

    if(NULL == h)
    {
        return;
    }


#ifdef DEBUG_BENCHMARK
    i_mtime_total = i_mtime_analyse + i_mtime_encode + i_mtime_write + i_mtime_filter + 1;
#endif
    i_yuv_size = 3 * h->param.i_width * h->param.i_height / 2;

#ifdef DEBUG_BENCHMARK
	//jlb- deleted 2005-7-29 10:59 begin
	//x264_log( h, X264_LOG_INFO,
	//          "analyse=%d(%lldms) encode=%d(%lldms) write=%d(%lldms) filter=%d(%lldms)\n",
	//          (int)(100*i_mtime_analyse/i_mtime_total), i_mtime_analyse/1000,
	//          (int)(100*i_mtime_encode/i_mtime_total), i_mtime_encode/1000,
	//          (int)(100*i_mtime_write/i_mtime_total), i_mtime_write/1000,
	//          (int)(100*i_mtime_filter/i_mtime_total), i_mtime_filter/1000 );
	//jlb- deleted 2005-7-29 10:59 end

	//jlb- added 2005-7-29 10:59 begin
	x264_log( h, X264_LOG_INFO, "internal fps = %d, Keyint_max = %d\n",
		h->param.i_fps_num, h->param.i_keyint_max);

	x264_log( h, X264_LOG_INFO, "analyse = %d%%(%lldms)\n",
		(int)(100*i_mtime_analyse/i_mtime_total), i_mtime_analyse/1000);

	x264_log( h, X264_LOG_INFO, "encode  = %d%%(%lldms)\n",
		(int)(100*i_mtime_encode/i_mtime_total), i_mtime_encode/1000);

	x264_log( h, X264_LOG_INFO, "write   = %d%%(%lldms)\n",
		(int)(100*i_mtime_write/i_mtime_total), i_mtime_write/1000);

	x264_log( h, X264_LOG_INFO, "filter  = %d%%(%lldms)\n",
		(int)(100*i_mtime_filter/i_mtime_total), i_mtime_filter/1000);

	x264_log( h, X264_LOG_INFO, "Total Time = %lldms\n", i_mtime_total/1000);
	//jlb- added 2005-7-29 10:59 end

    /* Slices used and PSNR */
    for( i=0; i<5; i++ )
    {
        static const int slice_order[] = { SLICE_TYPE_I, SLICE_TYPE_SI, SLICE_TYPE_P, SLICE_TYPE_SP, SLICE_TYPE_B };
        static const char *slice_name[] = { "P", "B", "I", "SP", "SI" };
        int i_slice = slice_order[i];

        if( h->stat.i_slice_count[i_slice] > 0 )
        {
            const int i_count = h->stat.i_slice_count[i_slice];
            if( h->param.analyse.b_psnr )
            {
                x264_log( h, X264_LOG_INFO,
                    "slice %s:%-4d Avg QP:%5.2f Avg size:%6.0f PSNR Mean Y:%5.2f U:%5.2f V:%5.2f Avg:%5.2f Global:%5.2f MSE*Size:%5.3f\n",
                    slice_name[i_slice],
                    i_count,
                    (double)h->stat.i_slice_qp[i_slice] / i_count,
                    (double)h->stat.i_slice_size[i_slice] / i_count,
                    h->stat.f_psnr_mean_y[i_slice] / i_count, h->stat.f_psnr_mean_u[i_slice] / i_count, h->stat.f_psnr_mean_v[i_slice] / i_count,
                    h->stat.f_psnr_average[i_slice] / i_count,
                    x264_psnr( h->stat.i_sqe_global[i_slice], i_count * i_yuv_size ),
                    x264_mse( h->stat.i_sqe_global[i_slice], i_count * i_yuv_size ) * h->stat.i_slice_size[i_slice] / i_count );
            }
            else
            {
                x264_log( h, X264_LOG_INFO,
                    "slice %s:%-4d Avg QP:%5.2f Avg size:%6.0f\n",
                    slice_name[i_slice],
                    i_count,
                    (double)h->stat.i_slice_qp[i_slice] / i_count,
                    (double)h->stat.i_slice_size[i_slice] / i_count );
            }
        }
    }

    /* MB types used */
    if( h->stat.i_slice_count[SLICE_TYPE_I] > 0 )
    {
        const int64_t *i_mb_count = h->stat.i_mb_count[SLICE_TYPE_I];
        const double i_count = h->stat.i_slice_count[SLICE_TYPE_I] * h->mb.i_mb_count / 100.0;
        x264_log( h, X264_LOG_INFO,
            "slice I   Avg I4x4:%.1f%%  I16x16:%.1f%%\n",
            i_mb_count[I_4x4]  / i_count,
            i_mb_count[I_16x16]/ i_count );
    }
    if( h->stat.i_slice_count[SLICE_TYPE_P] > 0 )
    {
        const int64_t *i_mb_count = h->stat.i_mb_count[SLICE_TYPE_P];
        const double i_count = h->stat.i_slice_count[SLICE_TYPE_P] * h->mb.i_mb_count / 100.0;
        x264_log( h, X264_LOG_INFO,
            "slice P   Avg I4x4:%.1f%%  I16x16:%.1f%%  P:%.1f%%  P8x8:%.1f%%  PSKIP:%.1f%%\n",
            i_mb_count[I_4x4]  / i_count,
            i_mb_count[I_16x16]/ i_count,
            i_mb_count[P_L0]   / i_count,
            i_mb_count[P_8x8]  / i_count,
            i_mb_count[P_SKIP] / i_count );
    }
    if( h->stat.i_slice_count[SLICE_TYPE_B] > 0 )
    {
        const int64_t *i_mb_count = h->stat.i_mb_count[SLICE_TYPE_B];
        const double i_count = h->stat.i_slice_count[SLICE_TYPE_B] * h->mb.i_mb_count / 100.0;
        x264_log( h, X264_LOG_INFO,
            "slice B   Avg I4x4:%.1f%%  I16x16:%.1f%%  P:%.1f%%  B:%.1f%%  B8x8:%.1f%%  DIRECT:%.1f%%  BSKIP:%.1f%%\n",
            i_mb_count[I_4x4]    / i_count,
            i_mb_count[I_16x16]  / i_count,
            (i_mb_count[B_L0_L0] + i_mb_count[B_L1_L1] + i_mb_count[B_L1_L0] + i_mb_count[B_L0_L1]) / i_count,
            (i_mb_count[B_BI_BI] + i_mb_count[B_L0_BI] + i_mb_count[B_L1_BI] + i_mb_count[B_BI_L0] + i_mb_count[B_BI_L1]) / i_count,
            i_mb_count[B_8x8]    / i_count,
            i_mb_count[B_DIRECT] / i_count,
            i_mb_count[B_SKIP]   / i_count );
    }

    if( h->stat.i_slice_count[SLICE_TYPE_I] + h->stat.i_slice_count[SLICE_TYPE_P] + h->stat.i_slice_count[SLICE_TYPE_B] > 0 )
    {
        const int i_count = h->stat.i_slice_count[SLICE_TYPE_I] +
            h->stat.i_slice_count[SLICE_TYPE_P] +
            h->stat.i_slice_count[SLICE_TYPE_B];
        float fps = (float) h->param.i_fps_num / h->param.i_fps_den;

        if( h->param.analyse.b_psnr )
            x264_log( h, X264_LOG_INFO,
            "PSNR Mean Y:%5.2f U:%5.2f V:%5.2f Avg:%5.2f Global:%5.2f kb/s:%.1f\n",
            (h->stat.f_psnr_mean_y[SLICE_TYPE_I] + h->stat.f_psnr_mean_y[SLICE_TYPE_P] + h->stat.f_psnr_mean_y[SLICE_TYPE_B]) / i_count,
            (h->stat.f_psnr_mean_u[SLICE_TYPE_I] + h->stat.f_psnr_mean_u[SLICE_TYPE_P] + h->stat.f_psnr_mean_u[SLICE_TYPE_B]) / i_count,
            (h->stat.f_psnr_mean_v[SLICE_TYPE_I] + h->stat.f_psnr_mean_v[SLICE_TYPE_P] + h->stat.f_psnr_mean_v[SLICE_TYPE_B]) / i_count,

            (h->stat.f_psnr_average[SLICE_TYPE_I] + h->stat.f_psnr_average[SLICE_TYPE_P] + h->stat.f_psnr_average[SLICE_TYPE_B]) / i_count,

            x264_psnr( h->stat.i_sqe_global[SLICE_TYPE_I] + h->stat.i_sqe_global[SLICE_TYPE_P]+ h->stat.i_sqe_global[SLICE_TYPE_B],
            i_count * i_yuv_size ),
            fps * 8*(h->stat.i_slice_size[SLICE_TYPE_I]+h->stat.i_slice_size[SLICE_TYPE_P]+h->stat.i_slice_size[SLICE_TYPE_B]) / i_count / 1000 );
        else
            x264_log( h, X264_LOG_INFO,
            "kb/s:%.1f\n",
            fps * 8*(h->stat.i_slice_size[SLICE_TYPE_I]+h->stat.i_slice_size[SLICE_TYPE_P]+h->stat.i_slice_size[SLICE_TYPE_B]) / i_count / 1000 );
    }
#endif

    /* frames */
    for( i = 0; i < X264_BFRAME_MAX + 3; i++ )
    {
        if( h->frames.current[i] ) 
        {
            x264_frame_delete( h->frames.current[i] );
            h->frames.current[i] = NULL;
        }
        if( h->frames.next[i] ) 
        {
            x264_frame_delete( h->frames.next[i] );
            h->frames.next[i] = NULL;
        }
        if( h->frames.unused[i] )
        {
            x264_frame_delete( h->frames.unused[i] );
            h->frames.unused[i] = NULL;
        }
    }
    /* ref frames */
    for( i = 0; i < h->frames.i_max_dpb; i++ )
    {
        x264_frame_delete( h->frames.reference[i] );
        h->frames.reference[i] = NULL;
    }

    /* rc */
    x264_ratecontrol_delete( h );
    h->rc = NULL;

    x264_macroblock_cache_end( h );

    x264_free( h->out.p_bitstream );  //jlb- added
    h->out.p_bitstream = NULL;

    x264_picture_clean(h->picture);
    x264_free(h->picture);
    h->picture = NULL;

    x264_free( h );
#endif

}

