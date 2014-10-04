/*****************************************************************************
 * x264: h264 encoder/decoder  program.
 *****************************************************************************
 * Copyright (C) 2005 ICT of GSCAS
 * $Id: x264.c,v 1.1.1.1 2005/03/12 
 *
 * Authors: Xia Weiping etc.
 *
 * This program is designed by the team of ict, distributed to sina corporation.
 *
 *
  *****************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <fcntl.h>  

#include <math.h>

#include <signal.h>
#define _GNU_SOURCE
#include <getopt.h>

#ifdef _MSC_VER
#include <io.h>     /* _setmode() */
#include <fcntl.h>  /* _O_BINARY */
#endif

#ifdef _CYGWIN
#include <windows.h>
#include <vfw.h>
#define AVIS_INPUT
#endif

#include "common/common.h"
#include "common/csp.h"
#include "x264.h"

#define DATA_MAX 400000
uint8_t data[DATA_MAX];

typedef void *hnd_t;

/* Ctrl-C handler */
static int     i_ctrl_c = 0;
static void    SigIntHandler( int a )
{
    i_ctrl_c = 1;
}

static char outputfilename[20]; //just for test to recoder the encode or decode file

#define TIMER_START( d ) \
    { \
        int64_t d##start = x264_mdate();

#define TIMER_STOP( d ) \
        d += x264_mdate() - d##start;\
    }
	//to Stat. the encode or decode time

/* input file operation function pointers */
static int (*p_open_file)( char *psz_filename, hnd_t *p_handle, x264_param_t *p_param );
static int (*p_get_frame_total)( hnd_t handle, int i_width, int i_height );
static int (*p_read_frame)( x264_picture_t *p_pic, hnd_t handle, int i_width, int i_height );
static int (*p_close_file)( hnd_t handle );

static int open_file_yuv( char *psz_filename, hnd_t *p_handle, x264_param_t *p_param );
static int get_frame_total_yuv( hnd_t handle, int i_width, int i_height );
static int read_frame_yuv( x264_picture_t *p_pic, hnd_t handle, int i_width, int i_height );
static int close_file_yuv( hnd_t handle );

#ifdef AVIS_INPUT
static int open_file_avis( char *psz_filename, hnd_t *p_handle, x264_param_t *p_param );
static int get_frame_total_avis( hnd_t handle, int i_width, int i_height );
static int read_frame_avis( x264_picture_t *p_pic, hnd_t handle, int i_width, int i_height );
static int close_file_avis( hnd_t handle );
#endif

static void Help( x264_param_t *defaults );
static int  Parse( int argc, char **argv, x264_param_t  *param, hnd_t *p_hin, FILE **p_fout, int *pb_decompress );
static int  Encode(hnd_t hin,  FILE *fout );
static int  Decode(/*x264_param_t  *param,*/FILE *fh26l, FILE *fout );


/****************************************************************************
 * main:
 ****************************************************************************/
#include <wINDOWS.H>

int main( int argc, char **argv )
{
  
	x264_param_t param;
    FILE    *fout;
    hnd_t   hin;

    int     b_decompress;
    int     i_ret;

#ifdef _MSC_VER
    _setmode(_fileno(stdin), _O_BINARY);    /* thanks to Marcos Morais <morais at dee.ufcg.edu.br> */
    _setmode(_fileno(stdout), _O_BINARY);
#endif

    x264_param_default( &param );

    /* Parse command line */
    if( Parse( argc, argv, &param, &hin, &fout, &b_decompress ) < 0 )
    {
        return -1;
    }

    /* Control-C handler */
    signal( SIGINT, SigIntHandler );

    if( b_decompress )
        i_ret = Decode( hin, fout );
    else
        i_ret = Encode(  hin, fout );

    return i_ret;
}


/*****************************************************************************
 * Help:
 *****************************************************************************/
static void Help( x264_param_t *defaults )
{
    fprintf( stderr,
             "x264 build:%d\n"
             "Syntax: x264 [options] [-o out.h26l] in.yuv widthxheigh\n"
             "\n"
             "  -h, --help                  Print this help\n"
             "\n"
             "  -I, --keyint <integer  >    Maximum GOP size [%d]\n"
             "  -i, --min-keyint <integer>  Minimum GOP size [%d]\n"
             "      --scenecut <integer>    How aggresively to insert extra I frames [%d]\n"
             "  -b, --bframe <integer>      Number of B-frames between I and P [%d]\n"
             "      --no-b-adapt            Disable adaptive B-frame decision\n"
             "      --b-bias <integer>      Influences how often B-frames are used [%d]\n"
             "      --b-pyramid             Keep some B-frames as references\n"
             "\n"
             "      --no-cabac              Disable CABAC\n"
             "  -r, --ref <integer>         Number of reference frames [%d]\n"
             "  -n, --nf                    Disable loop filter\n"
             "  -f, --filter <alpha:beta>   Loop filter AplhaCO and Beta parameters [%d]\n"
             "\n"
             "  -q, --qp <integer>          Set QP [%d]\n"
             "  -B, --bitrate <integer>     Set bitrate\n"
             "      --qpmin <integer>       Set min QP [%d]\n"
             "      --qpmax <integer>       Set max QP [%d]\n"
             "      --qpstep <integer>      Set max QP step [%d]\n"
             "      --rcsens <integer>      CBR ratecontrol sensitivity [%d]\n"
             "      --rcbuf <integer>       Size of VBV buffer [%d]\n"
             "      --rcinitbuf <integer>   Initial VBV buffer occupancy [%d]\n"
             "      --ipratio <float>       QP factor between I and P [%.2f]\n"
             "      --pbratio <float>       QP factor between P and B [%.2f]\n"
             "      --chroma-qp-offset <integer>  QP difference between chroma and luma [%d]\n"
             "\n"
             "  -p, --pass <1|2>            Enable 2 pass ratecontrol\n"
             "      --stats <string>        Filename for 2 pass stats [\"%s\"]\n"
             "      --rceq <string>         Ratecontrol equation [\"%s\"]\n"
             "      --qcomp <float>         0.0 => CBR, 1.0 => CQP [%.2f]\n"
             "      --cplxblur <float>      reduce fluctuations in QP (before curve compression) [%.1f]\n"
             "      --qblur <float>         reduce fluctuations in QP (after curve compression) [%.1f]\n"
             "\n"

             "  -A, --analyse <string>      Analyse options: [\"i4x4|psub16x16|bsub16x16\"]\n"
             "                                  - i4x4\n"
             "                                  - psub16x16, psub8x8, bsub16x16\n"
             "                                  - none, all\n"
             "      --direct <string>       Direct MV prediction mode [\"temporal\"]\n"
             "                                  - none, spatial, temporal\n"
             "  -w, --weightb               Weighted prediction for B-frames\n"
             "  -m, --subme <integer>       Subpixel motion estimation quality: 1=fast, 5=best. [%d]\n"
             "\n"
             "      --level <integer>       Specify IDC level\n"
             "  -s, --sar width:height      Specify Sample Aspect Ratio\n"
             "      --fps <float|rational>  Specify framerate\n"
             "      --frames <integer>      Maximum number of frames to encode\n"
             "  -o, --output                Specify output file\n"
             "\n"
             "      --no-asm                Disable any CPU optims\n"
             "      --no-psnr               Disable PSNR computaion\n"
             "      --quiet                 Quiet Mode\n"
             "  -v, --verbose               Print stats for each frame\n"
             "\n",
            X264_BUILD,
            defaults->i_keyint_max,
            defaults->i_keyint_min,
            defaults->i_scenecut_threshold,
            defaults->i_bframe,
            defaults->i_bframe_bias,
            defaults->i_frame_reference,
            defaults->i_deblocking_filter_alphac0,
            defaults->rc.i_qp_constant,
            defaults->rc.i_qp_min,
            defaults->rc.i_qp_max,
            defaults->rc.i_qp_step,
            defaults->rc.i_rc_sens,
            defaults->rc.i_rc_buffer_size,
            defaults->rc.i_rc_init_buffer,
            defaults->rc.f_ip_factor,
            defaults->rc.f_pb_factor,
            defaults->analyse.i_chroma_qp_offset,
            defaults->rc.psz_stat_out,
            defaults->rc.psz_rc_eq,
            defaults->rc.f_qcompress,
            defaults->rc.f_complexity_blur,
            defaults->rc.f_qblur,
            defaults->analyse.i_subpel_refine
           );
}

/*****************************************************************************
 * 解析参数:
 *****************************************************************************/
static int  Parse( int argc, char **argv,
                   x264_param_t  *param,
                   hnd_t *p_hin, FILE **p_fout, int *pb_decompress )
{
    char *psz_filename = NULL;
    x264_param_t defaults = *param;
    char *psz;
    char b_avis = 0;
	 /* Default output */
    *p_fout = stdout;
    *p_hin  = stdin;
    *pb_decompress = 0;

    /* Default input file driver */
    p_open_file = open_file_yuv;
    p_get_frame_total = get_frame_total_yuv;
    p_read_frame = read_frame_yuv;
    p_close_file = close_file_yuv;

    /* Parse command line options */
    opterr = 0; // no error message
    for( ;; )
    {
        int long_options_index;
#define OPT_QPMIN 256
#define OPT_QPMAX 257
#define OPT_QPSTEP 258
#define OPT_RCSENS 259
#define OPT_IPRATIO 260
#define OPT_PBRATIO 261
#define OPT_RCBUF 262
#define OPT_RCIBUF 263
#define OPT_RCSTATS 264
#define OPT_RCEQ 265
#define OPT_QCOMP 266
#define OPT_NOPSNR 267
#define OPT_QUIET 268
#define OPT_SCENECUT 270
#define OPT_QBLUR 271
#define OPT_CPLXBLUR 272
#define OPT_FRAMES 273
#define OPT_FPS 274
#define OPT_DIRECT 275
#define OPT_LEVEL 276
#define OPT_NOBADAPT 277
#define OPT_BBIAS 278
#define OPT_BPYRAMID 279
#define OPT_CHROMA_QP 280
#define OPT_NO_CHROMA_ME 281
#define OPT_NO_CABAC 282

        static struct option long_options[] =
        {
            { "help",    no_argument,       NULL, 'h' },
            { "bitrate", required_argument, NULL, 'B' },
            { "bframe",  required_argument, NULL, 'b' },
            { "no-b-adapt", no_argument,    NULL, OPT_NOBADAPT },
            { "b-bias",  required_argument, NULL, OPT_BBIAS },
            { "b-pyramid", no_argument,     NULL, OPT_BPYRAMID },
            { "min-keyint",required_argument,NULL,'i' },
            { "keyint",  required_argument, NULL, 'I' },
            { "scenecut",required_argument, NULL, OPT_SCENECUT },
            { "nf",      no_argument,       NULL, 'n' },
            { "filter",  required_argument, NULL, 'f' },
            { "no-cabac",no_argument,       NULL, OPT_NO_CABAC },
            { "qp",      required_argument, NULL, 'q' },
            { "qpmin",   required_argument, NULL, OPT_QPMIN },
            { "qpmax",   required_argument, NULL, OPT_QPMAX },
            { "qpstep",  required_argument, NULL, OPT_QPSTEP },
            { "ref",     required_argument, NULL, 'r' },
            { "no-asm",  no_argument,       NULL, 'C' },
            { "sar",     required_argument, NULL, 's' },
            { "fps",     required_argument, NULL, OPT_FPS },
            { "frames",  required_argument, NULL, OPT_FRAMES },
            { "output",  required_argument, NULL, 'o' },
            { "analyse", required_argument, NULL, 'A' },
            { "direct",  required_argument, NULL, OPT_DIRECT },
            { "weightb", no_argument,       NULL, 'w' },
            { "subme",   required_argument, NULL, 'm' },
            { "no-chroma-me", no_argument,  NULL, OPT_NO_CHROMA_ME },
            { "level",   required_argument, NULL, OPT_LEVEL },
            { "rcsens",  required_argument, NULL, OPT_RCSENS },
            { "rcbuf",   required_argument, NULL, OPT_RCBUF },
            { "rcinitbuf",required_argument,NULL, OPT_RCIBUF },
            { "ipratio", required_argument, NULL, OPT_IPRATIO },
            { "pbratio", required_argument, NULL, OPT_PBRATIO },
            { "chroma-qp-offset", required_argument, NULL, OPT_CHROMA_QP },
            { "pass",    required_argument, NULL, 'p' },
            { "stats",   required_argument, NULL, OPT_RCSTATS },
            { "rceq",    required_argument, NULL, OPT_RCEQ },
            { "qcomp",   required_argument, NULL, OPT_QCOMP },
            { "qblur",   required_argument, NULL, OPT_QBLUR },
            { "cplxblur",required_argument, NULL, OPT_CPLXBLUR },
            { "no-psnr", no_argument,       NULL, OPT_NOPSNR },
            { "quiet",   no_argument,       NULL, OPT_QUIET },
            { "verbose", no_argument,       NULL, 'v' },
            {0, 0, 0, 0}
        };

        int c;

        c = getopt_long( argc, argv, "hi:I:b:r:cxB:q:nf:o:s:A:m:p:vw",
                         long_options, &long_options_index);

        if( c == -1 )
        {
            break;
        }

        switch( c )
        {
            case 'h':
                Help( &defaults );
                return -1;

            case 0:
                break;
            case 'B':
                param->rc.i_bitrate = atol( optarg );
                param->rc.b_cbr = 1;
                break;
            case 'b':
                param->i_bframe = atol( optarg );
                break;
            case OPT_NOBADAPT:
                param->b_bframe_adaptive = 0;
                break;
            case OPT_BBIAS:
                param->i_bframe_bias = atol( optarg );
                break;
            case OPT_BPYRAMID:
                param->b_bframe_pyramid = 1;
                break;
            case 'i':
                param->i_keyint_min = atol( optarg );
                break;
            case 'I':
                param->i_keyint_max = atol( optarg );
                break;
            case OPT_SCENECUT:
                param->i_scenecut_threshold = atol( optarg );
                break;
            case 'n':
                param->b_deblocking_filter = 0;
                break;
            case 'f':
            {
                char *p = strchr( optarg, ':' );
                if( p )
                {
                    param->i_deblocking_filter_alphac0 = atoi( optarg );
                    param->i_deblocking_filter_beta = atoi( p );
                }
                break;
            }
            case 'q':
                param->rc.i_qp_constant = atoi( optarg );
                break;
            case OPT_QPMIN:
                param->rc.i_qp_min = atoi( optarg );
                break;
            case OPT_QPMAX:
                param->rc.i_qp_max = atoi( optarg );
                break;
            case OPT_QPSTEP:
                param->rc.i_qp_step = atoi( optarg );
                break;
            case 'r':
                param->i_frame_reference = atoi( optarg );
                break;
            case OPT_NO_CABAC:
                param->b_cabac = 0;
                break;
            case 'x':
                *pb_decompress = 1;
                break;
            case 'C':
                param->cpu = 0;
                break;
            case OPT_FRAMES:
                param->i_maxframes = atoi( optarg );
                break;
            case'o':
            	
            	strcpy(outputfilename,optarg);
                if( ( *p_fout = fopen( optarg, "wb" ) ) == NULL )
                {
                    fprintf( stderr, "cannot open output file `%s'\n", optarg );
                    return -1;
                }
                break;
            case 's':
            {
                char *p = strchr( optarg, ':' );
                if( p )
                {
                    param->vui.i_sar_width = atoi( optarg );
                    param->vui.i_sar_height = atoi( p + 1 );
                }
                break;
            }
            case OPT_FPS:
            {
                float fps;
                if( sscanf( optarg, "%d/%d", &param->i_fps_num, &param->i_fps_den ) == 2 )
                    ;
                else if( sscanf( optarg, "%f", &fps ) )
                {
                    param->i_fps_num = (int)(fps * 1000 + .5);
                    param->i_fps_den = 1000;
                }
                else
                {
                    fprintf( stderr, "bad fps `%s'\n", optarg );
                    return -1;
                }
            }
            case 'A':
                param->analyse.inter = 0;
                if( strstr( optarg, "none" ) )  param->analyse.inter = 0x000000;
                if( strstr( optarg, "all" ) )   param->analyse.inter = X264_ANALYSE_I4x4|X264_ANALYSE_PSUB16x16|X264_ANALYSE_PSUB8x8|X264_ANALYSE_BSUB16x16;

                if( strstr( optarg, "i4x4" ) )      param->analyse.inter |= X264_ANALYSE_I4x4;
                if( strstr( optarg, "psub16x16" ) ) param->analyse.inter |= X264_ANALYSE_PSUB16x16;
                if( strstr( optarg, "psub8x8" ) )   param->analyse.inter |= X264_ANALYSE_PSUB8x8;
                if( strstr( optarg, "bsub16x16" ) ) param->analyse.inter |= X264_ANALYSE_BSUB16x16;
                break;
            case OPT_DIRECT:
                if( strstr( optarg, "temporal" ) )
                    param->analyse.i_direct_mv_pred = X264_DIRECT_PRED_TEMPORAL;
                else if( strstr( optarg, "spatial" ) )
                    param->analyse.i_direct_mv_pred = X264_DIRECT_PRED_SPATIAL;
                else if( strstr( optarg, "none" ) )
                    param->analyse.i_direct_mv_pred = X264_DIRECT_PRED_NONE;
                else
                    param->analyse.i_direct_mv_pred = atoi( optarg );
                break;
            case 'w':
                param->analyse.b_weighted_bipred = 1;
                break;
            case 'm':
                param->analyse.i_subpel_refine = atoi(optarg);
                break;
            case OPT_NO_CHROMA_ME:
                param->analyse.b_chroma_me = 0;
                break;
            case OPT_LEVEL:
                param->i_level_idc = atoi(optarg);
                break;
            case OPT_RCBUF:
                param->rc.i_rc_buffer_size = atoi(optarg);
                break;
            case OPT_RCIBUF:
                param->rc.i_rc_init_buffer = atoi(optarg);
                break;
            case OPT_RCSENS:
                param->rc.i_rc_sens = atoi(optarg);
                break;
            case OPT_IPRATIO:
                param->rc.f_ip_factor = atof(optarg);
                break;
            case OPT_PBRATIO:
                param->rc.f_pb_factor = atof(optarg);
                break;
            case OPT_CHROMA_QP:
                param->analyse.i_chroma_qp_offset = atoi(optarg);
                break;
            case 'p':
            {
                int i_pass = atoi(optarg);
                if( i_pass == 1 )
                    param->rc.b_stat_write = 1;
                else if( i_pass == 2 )
                    param->rc.b_stat_read = 1;
                else if( i_pass > 2 )
                    param->rc.b_stat_read =
                    param->rc.b_stat_write = 1;
                break;
            }
            case OPT_RCSTATS:
                param->rc.psz_stat_in = optarg;
                param->rc.psz_stat_out = optarg;
                break;
            case OPT_RCEQ:
                param->rc.psz_rc_eq = optarg;
               break;
            case OPT_QCOMP:
                param->rc.f_qcompress = atof(optarg);
                break;
            case OPT_QBLUR:
                param->rc.f_qblur = atof(optarg);
                break;
            case OPT_CPLXBLUR:
                param->rc.f_complexity_blur = atof(optarg);
                break;
            case OPT_NOPSNR:
                param->analyse.b_psnr = 0;
                break;
            case OPT_QUIET:
                param->i_log_level = X264_LOG_NONE;
                break;
            case 'v':
                param->i_log_level = X264_LOG_DEBUG;
                break;
            default:
                fprintf( stderr, "unknown option (%c)\n", optopt );
                return -1;
        }
    }

    /* Get the file name */
    if( optind > argc - 1 )
    {
        Help( &defaults );
        return -1;
    }
    psz_filename = argv[optind++];

    if( !(*pb_decompress) )
    {
        if( optind > argc - 1 )
        {
            /* try to parse the file name */
            for( psz = psz_filename; *psz; psz++ )
            {
                if( *psz >= '0' && *psz <= '9'
                    && sscanf( psz, "%ux%u", &param->i_width, &param->i_height ) == 2 )
                {
                    fprintf( stderr, "x264: file name gives %dx%d\n", param->i_width, param->i_height );
                    break;
                }
            }
        }
        else
        {
            sscanf( argv[optind++], "%ux%u", &param->i_width, &param->i_height );
        }
        
        /* check avis input */
        psz = psz_filename + strlen(psz_filename) - 1;
        while( psz > psz_filename && *psz != '.' )
            psz--;

        if( !strncmp( psz, ".avi", 4 ) || !strncmp( psz, ".avs", 4 ) )
            b_avis = 1;

        if( !b_avis && ( !param->i_width || !param->i_height ) )
        {
            Help( &defaults );
            return -1;
        }
    }

    /* open the input */
    if( !strcmp( psz_filename, "-" ) )
    {
        *p_hin = stdin;
        optind++;
    }
    else
    {
#ifdef AVIS_INPUT
        if( b_avis )
        {
            p_open_file = open_file_avis;
            p_get_frame_total = get_frame_total_avis;
            p_read_frame = read_frame_avis;
            p_close_file = close_file_avis;
        }
#endif
        if( p_open_file( psz_filename, p_hin, param ) )
        {
            fprintf( stderr, "could not open input file '%s'\n", psz_filename );
            return -1;
        }
    }

    return 0;
}

/*****************************************************************************
 * Decode:
 *****************************************************************************/

static int  Decode( FILE *fh26l, FILE *fout )
 /*****************************************************************************
 * test the function of DeCodec
 * input :  fh26l,  output : fout
 *
 * 
 *****************************************************************************/
{
	void *h;
	int abMark;
	
	/*Test*************/
	char *apSrcData;
	FILE* fp;
	int i_read;
	int aiDstBuffLen;
	char *apDstBuff;
	/********************/


	void* (__cdecl* x264_decoder_open)(void);
	void  (__cdecl* DeCodec)(void *h,char *apSrcData,int aiSrcDataLen,char *apDstBuff,int *aiDstBuffLen,int *abMark);
	void  (__cdecl* x264_decode_close)(void* h);
	
	HANDLE hDllHandle = NULL;
	hDllHandle = LoadLibrary("X264DLL.dll");

	x264_decoder_open = (void* (__cdecl*)(void))GetProcAddress(hDllHandle, "x264_decoder_open");
	DeCodec = (void* (__cdecl*)(void))GetProcAddress(hDllHandle, "DeCodec");
	x264_decode_close = (void* (__cdecl*)(void))GetProcAddress(hDllHandle, "x264_decode_close");


	if( ( h = x264_decoder_open() ) == NULL )
    {
        fprintf( stderr, "x264_decoder_open failed\n" );
        return -1;
    }

	/******************Test**************************************/	
	//fp=fopen("E:\\Study\\LabProject\\MpegProcess\\H.264\\sina\\x264_sina\\work\\claire.264","rb");
	apSrcData=x264_malloc( DATA_MAX );
	i_read= fread( apSrcData, 1, DATA_MAX , fh26l );
 	
	aiDstBuffLen=10000000;
	apDstBuff=x264_malloc(aiDstBuffLen);

	
	/************************************************************/

	DeCodec(h,apSrcData,i_read,apDstBuff,&aiDstBuffLen,&abMark);
	
	fwrite( apDstBuff, 1, aiDstBuffLen, fout ); 
	
	x264_decode_close(h);

	CloseHandle(hDllHandle);
	
}

/*****************************************************************************
 * Encode:
 *****************************************************************************/
static int  Encode( hnd_t hin, FILE *fout )
/*****************************************************************************
 * 测试DLL中的  EnCodec,DeCodec 等函数
 * 输入 :  fh26l（原始文件的指针）,  输出 : fout *（编码后的文件或者解码后的文件指针）
 * 
 *****************************************************************************/
{
	FILE* fp;
    x264_t *h;
    x264_picture_t pic;
	void *p_handle;

    int     i_frame, i_frame_total;
    int64_t i_start, i_end;
	int64_t i_encode_start, i_decode_start;
    int64_t i_file;

	/**decode*******************************************/
	void* (__cdecl* x264_decoder_open)(void);
	void  (__cdecl* DeCodec)(void *h,char *apSrcData,int aiSrcDataLen,char *apDstBuff,int *aiDstBuffLen,int *abMark);
	void  (__cdecl* x264_decode_close)(void* h);
	x264_t *h_dec;
//	HANDLE hDllHandle1 = NULL;
	int aiDstBuffLen;
	char *apDstBuff;

	/***************************************************/



	void* (__cdecl* x264_encoder_open)(void);
	int  (__cdecl* EnCodec)(void *h,char *apSrcData,int aiSrcDataLen,char *apDstBuff,int *aiDstBuffLen,int *abMark);
	void  (__cdecl* x264_encoder_close)(void* h);
	int  (__cdecl* SetFrameInfoInitial)(int aiFrameWidth,int aiFrameHeight,int aiBitCount,void *h);
	int  (__cdecl* SetFrmQuant)(int aiQuantVal,void *h);
	HANDLE hDllHandle = NULL;
	hDllHandle = LoadLibrary("X264DLL.dll");

	x264_encoder_open = (void* (__cdecl*)(void))GetProcAddress(hDllHandle, "encoder_open");
	EnCodec = (void* (__cdecl*)(int))GetProcAddress(hDllHandle, "EnCode");
	x264_encoder_close = (void* (__cdecl*)(void))GetProcAddress(hDllHandle, "encoder_close");
    SetFrameInfoInitial=(void* (__cdecl*)(int))GetProcAddress(hDllHandle, "SetFrameInfo");
	SetFrmQuant=(void* (__cdecl*)(int))GetProcAddress(hDllHandle, "SetFrmQuant");

	/*decode*******************************************/
	

//	hDllHandle1 = LoadLibrary("X264DLL.dll");

	x264_decoder_open = (void* (__cdecl*)(void))GetProcAddress(hDllHandle, "decoder_open");
	DeCodec = (void* (__cdecl*)(void))GetProcAddress(hDllHandle, "DeCode");
	x264_decode_close = (void* (__cdecl*)(void))GetProcAddress(hDllHandle, "decode_close");


	if( ( h_dec = x264_decoder_open() ) == NULL )
    {
        fprintf( stderr, "x264_decoder_open failed\n" );
        return -1;
    }

	aiDstBuffLen=10000000;
	apDstBuff=x264_malloc(aiDstBuffLen);

	/**************************************************/


	i_encode_start=i_decode_start= 0;
	
    i_file =0;
    if( ( p_handle = x264_encoder_open() ) == NULL )
    {
        fprintf( stderr, "x264_encoder_open failed\n" );
        return -1;
    }
	h=(x264_t *)p_handle;

	  if(  SetFrameInfoInitial(352,288,24,p_handle) == -1 )
    {
        fprintf( stderr, "x264 set parameter failed \n" );
        return -1;
    }
	  SetFrmQuant(85*142/100,p_handle);
	 
	 i_frame_total = p_get_frame_total( hin, h->param.i_width, h->param.i_height )/2;
	//if(i_frame_total>100)  i_frame_total=100; // added for testing part of the sequence
    
    i_start = x264_mdate();
    for( i_frame = 0, i_file = 0; i_ctrl_c == 0 && i_frame < i_frame_total; i_frame++ )
    {
        int         orglength,dstlength;
		char  orgfrm[DATA_MAX]	,dstbuff[DATA_MAX];
		int framelength,frameonceattime;
		int isKeyframe;

        if (h->param.i_maxframes!=0 && i_frame>=h->param.i_maxframes)
            break;

        /* read a frame 
        if ( p_read_frame( h->picture, hin, h->param.i_width, h->param.i_height ) )
            break;
*/
        /* Do not force any parameters */
        
        if(h->param.i_csp==X264_CSP_RGB)
    	 framelength=h->param.i_height * h->param.i_width * 3;
    else if(h->param.i_csp==X264_CSP_I420)
	 framelength=h->param.i_height * h->param.i_width * 3;
    frameonceattime =1;
    dstlength  =DATA_MAX;

    	orglength= fread( orgfrm, 1, framelength *frameonceattime, hin );
		
		TIMER_START( i_encode_start );
        if( EnCodec(p_handle,orgfrm,orglength,dstbuff,&dstlength,&isKeyframe) < 0 )
                                                            {
                                                             fprintf( stderr, "x264_encoder_encode failed\n" );
                                                            }
		TIMER_STOP( i_encode_start );

		/**decode*********************************/
		TIMER_START( i_decode_start );
		DeCodec(h_dec,dstbuff,dstlength,apDstBuff,&aiDstBuffLen,&isKeyframe);
		TIMER_STOP( i_decode_start );
		/****************************************/
             
       i_file += fwrite( apDstBuff, 1, aiDstBuffLen, fout );
    }
    i_end = x264_mdate();
  
    fprintf( stderr, "\n" );

    p_close_file( hin );
    if( fout != stdout )
    {
        fclose( fout );
    }
//	fp=fopen("E:\\Study\\LabProject\\MpegProcess\\H.264\\sina\\x264_sina\\work\\mine1.rgb","wb");
//	fout=fopen("E:\\Study\\LabProject\\MpegProcess\\H.264\\sina\\x264_sina\\work\\mine.264","rb");
//	Decode( fout,fp  );
//	fclose(fp);
//	fclose(fout);

    if( i_frame > 0 )
    {
        double fps = (double)i_frame * (double)1000000 /
                     (double)( i_end - i_start );

        fprintf( stderr, "encoded %d frames, %.2f fps, %.2f kb/s\n", i_frame, fps, (double) i_file * 8 * h->param.i_fps_num / ( h->param.i_fps_den * i_frame * 1000 ) );

		fprintf( stderr, "average encode time %.2f ms, average decode time %.2f ms\n", (double)i_encode_start/(1000*i_frame) ,(double)i_decode_start/(1000*i_frame) );
    }
    x264_encoder_close( h );
	CloseHandle(hDllHandle);
	/*decode********************************/
	x264_decode_close(h_dec);
	CloseHandle(hDllHandle);

	/**************************************/
    return 0;
}

/* raw 420 yuv file operation */
static int open_file_yuv( char *psz_filename, hnd_t *p_handle , x264_param_t *p_param )
{
	if ((*p_handle = fopen(psz_filename, "rb")) == NULL)
		return -1;
	
	return 0;
}

static int get_frame_total_yuv( hnd_t handle, int i_width, int i_height )
{
    FILE *f = (FILE *)handle;
    int i_frame_total = 0;

    if( !fseek( f, 0, SEEK_END ) )
    {
        int64_t i_size = ftell( f );
        fseek( f, 0, SEEK_SET );
        i_frame_total = (int)(i_size / ( i_width * i_height * 3 / 2 ));
    }

    return i_frame_total;
}

static int read_frame_yuv( x264_picture_t *p_pic, hnd_t handle, int i_width, int i_height )
{
	FILE *f = (FILE *)handle;
	
	if( fread( p_pic->img.plane[0], 1, i_width * i_height, f ) <= 0
			|| fread( p_pic->img.plane[1], 1, i_width * i_height / 4, f ) <= 0
			|| fread( p_pic->img.plane[2], 1, i_width * i_height / 4, f ) <= 0 )
		return -1;
	
	return 0;
}

static int close_file_yuv(hnd_t handle)
{
	return fclose((FILE *)handle);
}


/* avs/avi input file support under cygwin */

#ifdef AVIS_INPUT

static int gcd(int a, int b)
{
    int c;

    while (1)
    {
        c = a % b;
        if (!c)
            return b;
        a = b;
        b = c;
    }
}

static int open_file_avis( char *psz_filename, hnd_t *p_handle, x264_param_t *p_param )
{
    AVISTREAMINFO info;
    PAVISTREAM p_avi = NULL;
    int i;

    *p_handle = NULL;

    AVIFileInit();
    if( AVIStreamOpenFromFile( &p_avi, psz_filename, streamtypeVIDEO, 0, OF_READ, NULL ) )
    {
        AVIFileExit();
        return -1;
    }

    if( AVIStreamInfo(p_avi, &info, sizeof(AVISTREAMINFO)) )
    {
        AVIStreamRelease(p_avi);
        AVIFileExit();
        return -1;
    }

    // check input format
    if (info.fccHandler != MAKEFOURCC('Y', 'V', '1', '2'))
    {
        fprintf( stderr, "avis [error]: unsupported input format (%c%c%c%c)\n",
            (char)(info.fccHandler & 0xff), (char)((info.fccHandler >> 8) & 0xff),
            (char)((info.fccHandler >> 16) & 0xff), (char)((info.fccHandler >> 24)) );
		
        AVIStreamRelease(p_avi);
        AVIFileExit();

        return -1;
    }

    p_param->i_width = info.rcFrame.right - info.rcFrame.left;
    p_param->i_height = info.rcFrame.bottom - info.rcFrame.top;
    i = gcd(info.dwRate, info.dwScale);
    p_param->i_fps_den = info.dwScale / i;
    p_param->i_fps_num = info.dwRate / i;

    fprintf( stderr, "avis [info]: %dx%d @ %.2f fps (%d frames)\n",  
        p_param->i_width, p_param->i_height,
        (double)p_param->i_fps_num / (double)p_param->i_fps_den,
        (int)info.dwLength );

    *p_handle = (hnd_t)p_avi;

    return 0;
}

static int get_frame_total_avis( hnd_t handle, int i_width, int i_height )
{
    PAVISTREAM p_avi = (PAVISTREAM)handle;
    AVISTREAMINFO info;

    if( AVIStreamInfo(p_avi, &info, sizeof(AVISTREAMINFO)) )
        return -1;

    return info.dwLength;
}

static int read_frame_avis( x264_picture_t *p_pic, hnd_t handle, int i_width, int i_height )
{
    static int i_index = 0;
    PAVISTREAM p_avi = (PAVISTREAM)handle;
    
    p_pic->img.i_csp = X264_CSP_YV12;
    
    if(  AVIStreamRead(p_avi, i_index, 1, p_pic->img.plane[0], i_width * i_height * 3 / 2, NULL, NULL ) )
        return -1;

    i_index++;

    return 0;
}

static int close_file_avis( hnd_t handle )
{
    PAVISTREAM p_avi = (PAVISTREAM)handle;
	
    AVIStreamRelease(p_avi);
    AVIFileExit();

    return 0;
}

#endif
