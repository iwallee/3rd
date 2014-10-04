
#ifdef HAVE_STDINT_H
#include <stdint.h>
#else
#include <inttypes.h>
#endif
#include <stdlib.h>
#include <stdarg.h>

#include "common.h"
#include "x264.h"

#include "quantization.h"

#ifdef HAVE_MMXEXT
#   include "i386/dct.h"
#endif

#define DECLARE_ALIGNED2_MATRIX_H(name,sizex,sizey,type,alignment) \
    __declspec(align(alignment)) type name[(sizex)][(sizey)]

//extern const int dequant_mf[6][4][4] =
extern const DECLARE_ALIGNED2_MATRIX_H(dequant_mf, 6, 4 * 4, int16_t, 16) =
{
    10, 13, 10, 13, 13, 16, 13, 16, 10, 13, 10, 13, 13, 16, 13, 16,
    11, 14, 11, 14, 14, 18, 14, 18, 11, 14, 11, 14, 14, 18, 14, 18,
    13, 16, 13, 16, 16, 20, 16, 20, 13, 16, 13, 16, 16, 20, 16, 20,
    14, 18, 14, 18, 18, 23, 18, 23, 14, 18, 14, 18, 18, 23, 18, 23,
    16, 20, 16, 20, 20, 25, 20, 25, 16, 20, 16, 20, 20, 25, 20, 25,
    18, 23, 18, 23, 23, 29, 23, 29, 18, 23, 18, 23, 23, 29, 23, 29
};

//extern const int quant_mf[6][4][4] =
extern const DECLARE_ALIGNED2_MATRIX_H(quant_mf, 6, 4 * 4, int16_t, 16)=
{
    13107, 8066, 13107, 8066, 8066, 5243, 8066, 5243,
    13107, 8066, 13107, 8066, 8066, 5243, 8066, 5243,
    11916, 7490, 11916, 7490, 7490, 4660, 7490, 4660,
    11916, 7490, 11916, 7490, 7490, 4660, 7490, 4660,
    10082, 6554, 10082, 6554, 6554, 4194, 6554, 4194,
    10082, 6554, 10082, 6554, 6554, 4194, 6554, 4194,
     9362, 5825,  9362, 5825, 5825, 3647, 5825, 3647,
     9362, 5825,  9362, 5825, 5825, 3647, 5825, 3647,
     8192, 5243,  8192, 5243, 5243, 3355, 5243, 3355,
     8192, 5243,  8192, 5243, 5243, 3355, 5243, 3355,
     7282, 4559,  7282, 4559, 4559, 2893, 4559, 2893,
     7282, 4559,  7282, 4559, 4559, 2893, 4559, 2893
};

static void quant_4x4( int16_t dct[4][4], int i_qscale, int b_intra )
{
    const int i_qbits = 15 + i_qscale / 6;
    const int i_mf = i_qscale % 6;
    const int f = ( 1 << i_qbits ) / ( b_intra ? 3 : 6 );

    int x,y;
    for( y = 0; y < 4; y++ )
    {
        for( x = 0; x < 4; x++ )
        {
            if( dct[y][x] > 0 )
            {
                dct[y][x] =( f + dct[y][x]  * quant_mf[i_mf][y*4+x] ) >> i_qbits;
            }
            else
            {
                dct[y][x] = - ( ( f - dct[y][x]  * quant_mf[i_mf][y*4+x] ) >> i_qbits );
            }
        }
    }
}
static void quant_4x4_dc( int16_t dct[4][4], int i_qscale )
{
    const int i_qbits = 15 + i_qscale / 6;
    const int f2 = ( 2 << i_qbits ) / 3;
    const int i_qmf = quant_mf[i_qscale%6][0];
    int x,y;

    for( y = 0; y < 4; y++ )
    {
        for( x = 0; x < 4; x++ )
        {
            if( dct[y][x] > 0 )
            {
                dct[y][x] =( f2 + dct[y][x]  * i_qmf) >> ( 1 + i_qbits );
            }
            else
            {
                dct[y][x] = - ( ( f2 - dct[y][x]  * i_qmf ) >> (1 + i_qbits ) );
            }
        }
    }
}


void x264_mb_dequant_4x4_dc( int16_t dct[4][4], int i_qscale )
{
    const int i_qbits = i_qscale/6 - 2;
    int x,y;

    if( i_qbits >= 0 )
    {
        const int i_dmf = dequant_mf[i_qscale%6][0] << i_qbits;

        for( y = 0; y < 4; y++ )
        {
            for( x = 0; x < 4; x++ )
            {
                dct[y][x] = dct[y][x] * i_dmf;
            }
        }
    }
    else
    {
        const int i_dmf = dequant_mf[i_qscale%6][0];
        const int f = 1 << ( 1 + i_qbits );

        for( y = 0; y < 4; y++ )
        {
            for( x = 0; x < 4; x++ )
            {
                dct[y][x] = ( dct[y][x] * i_dmf + f ) >> (-i_qbits);
            }
        }
    }
}

void x264_mb_dequant_4x4( int16_t dct[4][4], int i_qscale )
{
    const int i_mf = i_qscale%6;
    const int i_qbits = i_qscale/6;
    int y;

    for( y = 0; y < 4; y++ )
    {
        dct[y][0] = ( dct[y][0] * dequant_mf[i_mf][y*4] ) << i_qbits;
        dct[y][1] = ( dct[y][1] * dequant_mf[i_mf][y*4+1] ) << i_qbits;
        dct[y][2] = ( dct[y][2] * dequant_mf[i_mf][y*4+2] ) << i_qbits;
        dct[y][3] = ( dct[y][3] * dequant_mf[i_mf][y*4+3] ) << i_qbits;
    }
}

void x264_mb_dequant_2x2_dc( int16_t dct[2][2], int i_qscale )
{
    const int i_qbits = i_qscale/6 - 1;

    if( i_qbits >= 0 )
    {
        const int i_dmf = dequant_mf[i_qscale%6][0] << i_qbits;

        dct[0][0] = dct[0][0] * i_dmf;
        dct[0][1] = dct[0][1] * i_dmf;
        dct[1][0] = dct[1][0] * i_dmf;
        dct[1][1] = dct[1][1] * i_dmf;
    }
    else
    {
        const int i_dmf = dequant_mf[i_qscale%6][0];

        dct[0][0] = ( dct[0][0] * i_dmf ) >> 1;
        dct[0][1] = ( dct[0][1] * i_dmf ) >> 1;
        dct[1][0] = ( dct[1][0] * i_dmf ) >> 1;
        dct[1][1] = ( dct[1][1] * i_dmf ) >> 1;
    }
}


static void quant_2x2_dc( int16_t dct[2][2], int i_qscale, int b_intra )
{
    int const i_qbits = 15 + i_qscale / 6;
    const int f2 = ( 2 << i_qbits ) / ( b_intra ? 3 : 6 );
    const int i_qmf = quant_mf[i_qscale%6][0];

    int x,y;
    for( y = 0; y < 2; y++ )
    {
        for( x = 0; x < 2; x++ )
        {
            if( dct[y][x] > 0 )
            {
                dct[y][x] =( f2 + dct[y][x]  * i_qmf) >> ( 1 + i_qbits );
            }
            else
            {
                dct[y][x] = - ( ( f2 - dct[y][x]  * i_qmf ) >> (1 + i_qbits ) );
            }
        }
    }
}

/****************************************************************************
 * x264_quan_init:
 ****************************************************************************/
void x264_quan_init( int cpu,  x264_t *h )
{
    
	h->x264_quant_4x4 =quant_4x4;
	h->x264_quant_4x4_dc = quant_4x4_dc;
	h->x264_dequant_4x4  = x264_mb_dequant_4x4;
	h->x264_dequant_4x4_dc = x264_mb_dequant_4x4_dc;
    h->x264_dequant_2x2_dc = x264_mb_dequant_2x2_dc;
	h->x264_quant_2x2_dc = quant_2x2_dc;
    
    
    if( cpu&X264_CPU_SSE2 )
            {
                h->x264_quant_4x4 =quant4x4_sse2;
      			h->x264_dequant_4x4  = iquant4x4_sse2;
                
            }
    
    

}

