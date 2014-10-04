/*****************************************************************************
 * macroblock.c: h264 decoder library
 *****************************************************************************
 * Copyright (C) 2005 Xia wei ping etc.
 *****************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "common/common.h"
#include "common/vlc.h"
#include "vlc.h"
#include "macroblock.h"

#include "common/macroblock.h"  
#include "common/predict.h" 
#include "common/global_tbl.h"  

/*
static const uint8_t block_idx_x[16] =
{
    0, 1, 0, 1, 2, 3, 2, 3, 0, 1, 0, 1, 2, 3, 2, 3
};
static const uint8_t block_idx_y[16] =
{
    0, 0, 1, 1, 0, 0, 1, 1, 2, 2, 3, 3, 2, 2, 3, 3
};
static const uint8_t block_idx_xy[4][4] =
{
    { 0, 2, 8,  10},
    { 1, 3, 9,  11},
    { 4, 6, 12, 14},
    { 5, 7, 13, 15}
};*/


//static const int golomb_to_intra4x4_cbp[48]=
static const DECLARE_ALIGNED( int, golomb_to_intra4x4_cbp[48], 16 )=
{
    47, 31, 15,  0, 23, 27, 29, 30,  7, 11, 13, 14, 39, 43, 45, 46,
    16,  3,  5, 10, 12, 19, 21, 26, 28, 35, 37, 42, 44,  1,  2,  4,
     8, 17, 18, 20, 24,  6,  9, 22, 25, 32, 33, 34, 36, 40, 38, 41
};

//static const int golomb_to_inter_cbp[48]=
static const DECLARE_ALIGNED( int, golomb_to_inter_cbp[48], 16 )=
{
     0, 16,  1,  2,  4,  8, 32,  3,  5, 10, 12, 15, 47,  7, 11, 13,
    14,  6,  9, 31, 35, 37, 42, 44, 33, 34, 36, 40, 39, 43, 45, 46,
    17, 18, 20, 24, 19, 21, 26, 28, 23, 27, 29, 30, 22, 25, 38, 41
};

/*
static const int i_chroma_qp_table[52] =
{
     0,  1,  2,  3,  4,  5,  6,  7,  8,  9,
    10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
    20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
    29, 30, 31, 32, 32, 33, 34, 34, 35, 35,
    36, 36, 37, 37, 37, 38, 38, 38, 39, 39,
    39, 39
};*/



void x264_mb_partition_ref_set( x264_macroblock_t *mb, int i_list, int i_part, int i_ref )
{
/*    int x,  y;
    int w,  h;
    int dx, dy;

    x264_mb_partition_getxy( mb, i_part, 0, &x, &y );
    if( mb->i_partition == D_16x16 )
    {
        w = 4; h = 4;
    }
    else if( mb->i_partition == D_16x8 )
    {
        w = 4; h = 2;
    }
    else if( mb->i_partition == D_8x16 )
    {
        w = 2; h = 4;
    }
    else
    {
        // D_8x8 
        w = 2; h = 2;
    }

    for( dx = 0; dx < w; dx++ )
    {
        for( dy = 0; dy < h; dy++ )
        {
            mb->partition[x+dx][y+dy].i_ref[i_list] = i_ref;
        }
    }
    */ 
}

void x264_mb_partition_mv_set( x264_macroblock_t *mb, int i_list, int i_part, int i_sub, int mv[2] )
{
/*    int x,  y;
    int w,  h;
    int dx, dy;

    x264_mb_partition_getxy( mb, i_part, i_sub, &x, &y );
    x264_mb_partition_size ( mb, i_part, i_sub, &w, &h );

    for( dx = 0; dx < w; dx++ )
    {
        for( dy = 0; dy < h; dy++ )
        {
            mb->partition[x+dx][y+dy].mv[i_list][0] = mv[0];
            mb->partition[x+dx][y+dy].mv[i_list][1] = mv[1];
        }
    }
 */ // delete    
}




static int x264_macroblock_decode_ipcm( x264_t *h, bs_t *s, x264_macroblock_t *mb )
{
    /* TODO */
    return -1;
}


#define BLOCK_INDEX_CHROMA_DC   (-1)
#define BLOCK_INDEX_LUMA_DC     (-2)

static int bs_read_vlc( bs_t *s, x264_vlc_table_t *table )
{
    int i_nb_bits;
    int i_value = 0;
    int i_bits;
    int i_index;
    int i_level = 0;

    i_index = bs_show( s, table->i_lookup_bits );
    if( i_index >= table->i_lookup )
    {
        return( -1 );
    }
    i_value = table->lookup[i_index].i_value;
    i_bits  = table->lookup[i_index].i_size;

    while( i_bits < 0 )
    {
        i_level++;
        if( i_level > 5 )
        {
            return( -1 );        // FIXME what to do ?
        }
        bs_skip( s, table->i_lookup_bits );
        i_nb_bits = -i_bits;

        i_index = bs_show( s, i_nb_bits ) + i_value;
        if( i_index >= table->i_lookup )
        {
            return( -1 );
        }
        i_value = table->lookup[i_index].i_value;
        i_bits  = table->lookup[i_index].i_size;
    }
    bs_skip( s, i_bits );

    return( i_value );
}

static int block_residual_read_cavlc( x264_t *h, bs_t *s, x264_macroblock_t *mb,
                                      int i_idx, int *l, int i_count )
{
    int i;
    int level[16], run[16];
    int i_coeff;

    int i_total, i_trailing;
    int i_suffix_length;
    int i_zero_left;

    for( i = 0; i < i_count; i++ )
    {
        l[i] = 0;
    }

    /* total/trailing */
    if( i_idx == BLOCK_INDEX_CHROMA_DC )
    {
        int i_tt;

        if( ( i_tt = bs_read_vlc( s, h->x264_coeff_token_lookup[4] )) < 0 )
        {
            return -1;
        }

        i_total = i_tt / 4;
        i_trailing = i_tt % 4;
    }
    else
    {
        /* x264_mb_predict_non_zero_code return 0 <-> (16+16+1)>>1 = 16 */
        static const int ct_index[17] = {0,0,1,1,2,2,2,2,3,3,3,3,3,3,3,3,3 };
        int nC;
        int i_tt;

        if( i_idx == BLOCK_INDEX_LUMA_DC )
        {
            nC = x264_mb_predict_non_zero_code( h,  0 ); //modified  2005-4-8
        }
        else
        {
            nC = x264_mb_predict_non_zero_code( h, i_idx ); //modified  2005-4-8
        }

        if( ( i_tt = bs_read_vlc( s, h->x264_coeff_token_lookup[ct_index[nC]] ) ) < 0 )
        {
            return -1;
        }

        i_total = i_tt / 4;
        i_trailing = i_tt % 4;
    }

    if( i_idx >= 0 )
    {
       h->mb.non_zero_count[mb->i_mb_xy][i_idx] = i_total;  //modified   2005-4-8
    }

    if( i_total <= 0 )
    {
        return 0;
    }

    i_suffix_length = i_total > 10 && i_trailing < 3 ? 1 : 0;

    for( i = 0; i < i_trailing; i++ )
    {
        level[i] = 1 - 2 * bs_read1( s );
    }

    for( ; i < i_total; i++ )
    {
        int i_prefix;
        int i_level_code;

        i_prefix = bs_read_vlc( s, h->x264_level_prefix_lookup );

        if( i_prefix == -1 )
        {
            return -1;
        }
        else if( i_prefix < 14 )
        {
            if( i_suffix_length > 0 )
            {
                i_level_code = (i_prefix << i_suffix_length) + bs_read( s, i_suffix_length );
            }
            else
            {
                i_level_code = i_prefix;
            }
        }
        else if( i_prefix == 14 )
        {
            if( i_suffix_length > 0 )
            {
                i_level_code = (i_prefix << i_suffix_length) + bs_read( s, i_suffix_length );
            }
            else
            {
                i_level_code = i_prefix + bs_read( s, 4 );
            }
        }
        else /* if( i_prefix == 15 ) */
        {
            i_level_code = (i_prefix << i_suffix_length) + bs_read( s, 12 );
            if( i_suffix_length == 0 )
            {
                i_level_code += 15;
            }
        }
        if( i == i_trailing && i_trailing < 3 )
        {
            i_level_code += 2;
        }
        /* Optimise */
        level[i] = i_level_code&0x01 ? -((i_level_code+1)/2) : (i_level_code+2)/2;

        if( i_suffix_length == 0 )
        {
            i_suffix_length++;
        }
        if( abs( level[i] ) > ( 3 << ( i_suffix_length - 1 ) ) && i_suffix_length < 6 )
        {
            i_suffix_length++;
        }
    }

    if( i_total < i_count )
    {
        if( i_idx == BLOCK_INDEX_CHROMA_DC )
        {
            i_zero_left = bs_read_vlc( s, h->x264_total_zeros_dc_lookup[i_total-1] );
        }
        else
        {
            i_zero_left = bs_read_vlc( s, h->x264_total_zeros_lookup[i_total-1] );
        }
        if( i_zero_left < 0 )
        {
            return -1;
        }
    }
    else
    {
        i_zero_left = 0;
    }

    for( i = 0; i < i_total - 1; i++ )
    {
        if( i_zero_left <= 0 )
        {
            break;
        }
        run[i] = bs_read_vlc( s, h->x264_run_before_lookup[X264_MIN( i_zero_left - 1, 6 )] );

        if( run[i] < 0 )
        {
            return -1;
        }
        i_zero_left -= run[i];
    }
    if( i_zero_left < 0 )
    {
        return -1;
    }

    for( ; i < i_total - 1; i++ )
    {
        run[i] = 0;
    }
    run[i_total-1] = i_zero_left;

    i_coeff = -1;
    for( i = i_total - 1; i >= 0; i-- )
    {
        i_coeff += run[i] + 1;
        l[i_coeff] = level[i];
    }

    return 0;
}

static inline void array_zero_set( int *l, int i_count )
{
    int i;

    for( i = 0; i < i_count; i++ )
    {
        l[i] = 0;
    }
}

int x264_macroblock_read_cavlc( x264_t *h, bs_t *s, x264_macroblock_t *mb )
{
    int i_mb_i_offset;
    int i_mb_p_offset;
    int b_sub_ref0 = 0;
    int i_type;
    int i;

    /* read the mb type */
    switch( h->sh.i_type )
    {
        case SLICE_TYPE_I:
            i_mb_p_offset = 0;  /* shut up gcc */
            i_mb_i_offset = 0;
            break;
        case SLICE_TYPE_P:
            i_mb_p_offset = 0;
            i_mb_i_offset = 5;
            break;
        case SLICE_TYPE_B:
            i_mb_p_offset = 23;
            i_mb_i_offset = 23 + 5;
            break;
        default:
            fprintf( stderr, "internal error or slice unsupported\n" );
            return -1;
    }

    i_type = bs_read_ue( s );

    if( i_type < i_mb_i_offset )
    {
        if( i_type < i_mb_p_offset )
        {
            fprintf( stderr, "unsupported mb type(B*)\n" );
            /* TODO for B frame */
            return -1;
        }
        else
        {
            i_type -= i_mb_p_offset;

            if( i_type == 0 )
            {
                mb->i_type = P_L0;
                mb->i_partition = D_16x16;
            }
            else if( i_type == 1 )
            {
                mb->i_type = P_L0;
                mb->i_partition = D_16x8;
            }
            else if( i_type == 2 )
            {
                mb->i_type = P_L0;
                mb->i_partition = D_8x16;
            }
            else if( i_type == 3 || i_type == 4 )
            {
                mb->i_type = P_8x8;
                mb->i_partition = D_8x8;
                b_sub_ref0 = i_type == 4 ? 1 : 0;
            }
            else
            {
                fprintf( stderr, "invalid mb type\n" );
                return -1;
            }
        }
    }
    else
    {
        i_type -= i_mb_i_offset;

        if( i_type == 0 )
        {
            mb->i_type = I_4x4;
        }
        else if( i_type < 25 )
        {
            mb->i_type = I_16x16;

            mb->i_intra16x16_pred_mode = (i_type - 1)%4;
            mb->i_cbp_chroma = ( (i_type-1) / 4 )%3;
            mb->i_cbp_luma   = ((i_type-1) / 12) ? 0x0f : 0x00;
        }
        else if( i_type == 25 )
        {
            mb->i_type = I_PCM;
        }
        else
        {
            fprintf( stderr, "invalid mb type (%d)\n", i_type );
            return -1;
        }
    }

    if( mb->i_type == I_PCM )
    {
        return x264_macroblock_decode_ipcm( h, s, mb );
    }

    if( IS_INTRA( mb->i_type ) )
    {
        if( mb->i_type == I_4x4 )
        {
            for( i = 0; i < 16; i++ )
            {
                int b_coded;

                b_coded = bs_read1( s );

                if( b_coded )
                {
                 	mb->cache.intra4x4_pred_mode[x264_scan8[i]]  = x264_mb_predict_intra4x4_mode( h, i ); // modified ,可能存在问题
                }
                else
                {
                    int i_predicted_mode = x264_mb_predict_intra4x4_mode( h, i );
                    int i_mode = bs_read( s, 3 );

                    if( i_mode >= i_predicted_mode )
                    {
                       mb->cache.intra4x4_pred_mode[x264_scan8[i]]   = i_mode + 1;
                    }
                    else
                    {
                        mb->cache.intra4x4_pred_mode[x264_scan8[i]]   = i_mode;
                    }
                }
            }
        }

        mb->i_chroma_pred_mode = bs_read_ue( s );
    }
    else if( mb->i_type == P_8x8 || mb->i_type == B_8x8)
    {
        /* FIXME won't work for B_8x8 */

        for( i = 0; i < 4; i++ )
        {
            int i_sub_partition;

            i_sub_partition = bs_read_ue( s );
            switch( i_sub_partition )
            {
                case 0:
                    mb->i_sub_partition[i] = D_L0_8x8;
                    break;
                case 1:
                    mb->i_sub_partition[i] = D_L0_8x4;
                    break;
                case 2:
                    mb->i_sub_partition[i] = D_L0_4x8;
                    break;
                case 3:
                    mb->i_sub_partition[i] = D_L0_4x4;
                    break;
                default:
                    fprintf( stderr, "invalid i_sub_partition\n" );
                    return -1;
            }
        }
        for( i = 0; i < 4; i++ )
        {
            int i_ref;

            i_ref = b_sub_ref0 ? 0 : bs_read_te( s, h->sh.i_num_ref_idx_l0_active - 1 );
            x264_mb_partition_ref_set( mb, 0, i, i_ref );
        }
        for( i = 0; i < 4; i++ )
        {
            int i_sub;
            int i_ref;

           // x264_mb_partition_get( mb, 0, i, 0, &i_ref, NULL, NULL );

            for( i_sub = 0; i_sub < x264_mb_partition_count_table[mb->i_sub_partition[i]]; i_sub++ )
            {
                int mv[2];

                x264_mb_predict_mv( h, 0, i, i_sub, mv );
                mv[0] += bs_read_se( s );
                mv[1] += bs_read_se( s );

                x264_mb_partition_mv_set( mb, 0, i, i_sub, mv );
            }
        }
    }
    else if( mb->i_type != B_DIRECT )
    {
        /* FIXME will work only for P block */

        /* FIXME using x264_mb_partition_set/x264_mb_partition_get here are too unoptimised
         * I should introduce ref and mv get/set */

        /* Motion Vector */
        int i_part = x264_mb_partition_count_table[mb->i_partition];

        for( i = 0; i < i_part; i++ )
        {
            int i_ref;

            i_ref = bs_read_te( s, h->sh.i_num_ref_idx_l0_active - 1 );

            x264_mb_partition_ref_set( mb, 0, i, i_ref );
        }

        for( i = 0; i < i_part; i++ )
        {
            int mv[2];

            x264_mb_predict_mv( h, 0, i, 0, mv );

            mv[0] += bs_read_se( s );
            mv[1] += bs_read_se( s );

            x264_mb_partition_mv_set( mb, 0, i, 0, mv );
        }
    }

    if( mb->i_type != I_16x16 )
    {
        int i_cbp;

        i_cbp = bs_read_ue( s );
        if( i_cbp >= 48 )
        {
            fprintf( stderr, "invalid cbp\n" );
            return -1;
        }

        if( mb->i_type == I_4x4 )
        {
            i_cbp = golomb_to_intra4x4_cbp[i_cbp];
        }
        else
        {
            i_cbp = golomb_to_inter_cbp[i_cbp];
        }
        mb->i_cbp_luma   = i_cbp&0x0f;
        mb->i_cbp_chroma = i_cbp >> 4;
    }

    if( mb->i_cbp_luma > 0 || mb->i_cbp_chroma > 0 || mb->i_type == I_16x16 )
    {
        mb->qp[mb->i_mb_xy] = bs_read_se( s ) + mb->i_last_qp; //modified 2005-4-9
//会有一些列的错误，因为没有设置初始值
        // write residual 
        if( mb->i_type == I_16x16 )
        {
            // DC Luma 
            if( block_residual_read_cavlc( h, s, mb, BLOCK_INDEX_LUMA_DC , h->dct.luma16x16_dc, 16 ) < 0 )
            {
                return -1;
            }

            if( mb->i_cbp_luma != 0 )
            {
                // AC Luma 
                for( i = 0; i < 16; i++ )
                {
                    if( block_residual_read_cavlc( h, s, mb, i, h->dct.block[i].residual_ac, 15 ) < 0 )
                    {
                        return -1;
                    }
                } 
            }
            else
            {
                for( i = 0; i < 16; i++ )
                {
                    h->mb.non_zero_count[mb->i_mb_xy][i]  = 0;
                    array_zero_set( h->dct.block[i].residual_ac, 15 );
                }
            }
        }
        else
        {
            for( i = 0; i < 16; i++ )
            {
               if( mb->i_cbp_luma & ( 1 << ( i / 4 ) ) )
                {
                    if( block_residual_read_cavlc( h, s, mb, i, h->dct.block[i].luma4x4, 16 ) < 0 )
                    {
                        return -1;
                    }
                }
                else
                {
                    h->mb.non_zero_count[mb->i_mb_xy][i] = 0;
                    array_zero_set( h->dct.block[i].luma4x4, 16 );//modified  可能会存在问题顺序的问题
                } 
            }
        }

        if( mb->i_cbp_chroma &0x03 )    // Chroma DC residual present 
        {
           if( block_residual_read_cavlc( h, s, mb, BLOCK_INDEX_CHROMA_DC, h->dct.chroma_dc[0], 4 ) < 0 ||
                block_residual_read_cavlc( h, s, mb, BLOCK_INDEX_CHROMA_DC, h->dct.chroma_dc[1], 4 ) < 0 )
            {
                return -1;
            }

        }
        else
        {
            array_zero_set( h->dct.chroma_dc[0], 4 );
            array_zero_set( h->dct.chroma_dc[1], 4 );
        }
        if( mb->i_cbp_chroma&0x02 )  
        {
            for( i = 0; i < 8; i++ )
            {
                if( block_residual_read_cavlc( h, s, mb, 16 + i, h->dct.block[16+i].residual_ac, 15 ) < 0 )
                {
                    return -1;
                }
            }
        }
        else
        {
            for( i = 0; i < 8; i++ )
            {
              //  h->dct.block[16+i].i_non_zero_count = 0;
		h->mb.non_zero_count[mb->i_mb_xy][i]=0;
              array_zero_set( h->dct.block[16+i].residual_ac, 15 );  
            }
        }
    }
    else
    {
        mb->qp[mb->i_mb_xy] = h->pps->i_pic_init_qp + h->sh.i_qp_delta;
        for( i = 0; i < 16; i++ )
        {
          //  h->dct.block[i].i_non_zero_count = 0;
	  h->mb.non_zero_count[mb->i_mb_xy][i] =0;
          array_zero_set( h->dct.block[i].luma4x4, 16 ); 
        }
        array_zero_set( h->dct.chroma_dc[0], 4 );
        array_zero_set( h->dct.chroma_dc[1], 4 );
        for( i = 0; i < 8; i++ )
        {
           array_zero_set( h->dct.block[16+i].residual_ac, 15 );
           //  h->dct.block[16+i].i_non_zero_count = 0;
	   h->mb.non_zero_count[mb->i_mb_xy][i]=0;
        }
    }

    fprintf( stderr, "mb read type=%d\n", mb->i_type );

    return 0;
}




static int x264_mb_pred_mode16x16_valid( x264_macroblock_t *mb, int i_mode )
{

   if(i_mode ==I_PRED_16x16_DC)	
   	{
	    if( ( mb->i_neighbour & (MB_LEFT|MB_TOP) ) == (MB_LEFT|MB_TOP) )
	    {
	        return i_mode;
	    }
	    else if( ( mb->i_neighbour & MB_LEFT ) )
	               return I_PRED_16x16_DC_LEFT;
	    else if( ( mb->i_neighbour & MB_TOP ) )
	    		return I_PRED_16x16_DC_TOP;
	    else
	    		return I_PRED_16x16_DC_128;
   	}
   else
   	return  i_mode;
    
      
}

static int x264_mb_pred_mode8x8_valid( x264_macroblock_t *mb, int i_mode )
{

   if(i_mode == I_PRED_CHROMA_DC)
   	{
   	  if( ( mb->i_neighbour & (MB_LEFT|MB_TOP) ) == (MB_LEFT|MB_TOP))
   	  	return  I_PRED_CHROMA_DC;
   	  else if(( mb->i_neighbour & MB_LEFT ) )
   	  	return  I_PRED_CHROMA_DC_LEFT;
   	  else if(( mb->i_neighbour & MB_TOP ) )
   	  	return  I_PRED_CHROMA_DC_TOP;
   	  else
   	  	return I_PRED_CHROMA_DC_128;   	  	
   	}
   else
   	return i_mode;
   	  
}

static int x264_mb_pred_mode4x4_valid( x264_macroblock_t *mb, int idx, int i_mode, int *pb_emu )
{
    int b_a, b_b, b_c;
    static const int needmb[16] =
    {
        MB_LEFT|MB_TOP, MB_TOP,
        MB_LEFT,        MB_PRIVATE,
        MB_TOP,         MB_TOP|MB_TOPRIGHT,
        0,              MB_PRIVATE,
        MB_LEFT,        0,
        MB_LEFT,        MB_PRIVATE,
        0,              MB_PRIVATE,
        0,              MB_PRIVATE
    };
    int b_emu = 0;

    *pb_emu = 0;

    b_a = (needmb[idx]&mb->i_neighbour&MB_LEFT) == (needmb[idx]&MB_LEFT);
    b_b = (needmb[idx]&mb->i_neighbour&MB_TOP) == (needmb[idx]&MB_TOP);
    b_c = (needmb[idx]&mb->i_neighbour&(MB_TOPRIGHT|MB_PRIVATE)) == (needmb[idx]&(MB_TOPRIGHT|MB_PRIVATE));

	 if( i_mode == I_PRED_4x4_DC )
    {
        if( b_a && b_b )
        {
            return I_PRED_4x4_DC;
        }
        else if( b_a )
        {
            return I_PRED_4x4_DC_LEFT;
        }
        else if( b_b )
        {
            return I_PRED_4x4_DC_TOP;
        }
        return I_PRED_4x4_DC_128;
    }
	 else
		 return i_mode;  // modified .

/*
    if( b_c == 0 && b_b )
    {
        b_emu = 1;
        b_c = 1;
    }

    // handle I_PRED_4x4_DC 
    if( i_mode == I_PRED_4x4_DC )
    {
        if( b_a && b_b )
        {
            return I_PRED_4x4_DC;
        }
        else if( b_a )
        {
            return I_PRED_4x4_DC_LEFT;
        }
        else if( b_b )
        {
            return I_PRED_4x4_DC_TOP;
        }
        return I_PRED_4x4_DC_128;
    }

    // handle 1 dir needed only 
    if( ( b_a && i_mode == I_PRED_4x4_H ) ||
        ( b_b && i_mode == I_PRED_4x4_V ) )
    {
        return i_mode;
    }

    // handle b_c case (b_b always true) 
    if( b_c && ( i_mode == I_PRED_4x4_DDL || i_mode == I_PRED_4x4_VL ) )
    {
        *pb_emu = b_emu;
        return i_mode;
    }

    if( b_a && b_b )
    {
        // I_PRED_4x4_DDR, I_PRED_4x4_VR, I_PRED_4x4_HD, I_PRED_4x4_HU 
        return i_mode;
    }

    fprintf( stderr, "invalid 4x4 predict mode(%d, mb:%x-%x idx:%d\n", i_mode, mb->i_mb_x, mb->i_mb_y, idx );
    return I_PRED_CHROMA_DC_128;   */ /* unefficient */
}

/****************************************************************************
 * UnScan functions
 ****************************************************************************/
//static const int scan_zigzag_x[16]={0, 1, 0, 0, 1, 2, 3, 2, 1, 0, 1, 2, 3, 3, 2, 3};
//static const int scan_zigzag_y[16]={0, 0, 1, 2, 1, 0, 0, 1, 2, 3, 3, 2, 1, 2, 3, 3};
static const DECLARE_ALIGNED( int, scan_zigzag_x[16], 16 )={0, 1, 0, 0, 1, 2, 3, 2, 1, 0, 1, 2, 3, 3, 2, 3};
static const DECLARE_ALIGNED( int, scan_zigzag_y[16], 16 )={0, 0, 1, 2, 1, 0, 0, 1, 2, 3, 3, 2, 1, 2, 3, 3};

static inline void unscan_zigzag_4x4full( int16_t dct[4][4], int level[16] )
{
	 dct[0][0]=level[0];
     dct[0][1]=level[1];
     dct[1][0]=level[2];
     dct[2][0]=level[3];
     dct[1][1]=level[4];
     dct[0][2]=level[5];
     dct[0][3]=level[6];
     dct[1][2]=level[7];
     dct[2][1]=level[8];
     dct[3][0]=level[9];
     dct[3][1]=level[10];
     dct[2][2]=level[11];
     dct[1][3]=level[12];
     dct[2][3]=level[13];
     dct[3][2]=level[14];
     dct[3][3]=level[15];
#if 0
    int i;

    for( i = 0; i < 16; i++ )
    {
        dct[scan_zigzag_y[i]][scan_zigzag_x[i]] = level[i];
    }
#endif
}
static inline void unscan_zigzag_4x4( int16_t dct[4][4], int level[15] )
{
	 dct[0][1]=level[0];
     dct[1][0]=level[1];
     dct[2][0]=level[2];
     dct[1][1]=level[3];
     dct[0][2]=level[4];
     dct[0][3]=level[5];
     dct[1][2]=level[6];
     dct[2][1]=level[7];
     dct[3][0]=level[8];
     dct[3][1]=level[9];
     dct[2][2]=level[10];
     dct[1][3]=level[11];
     dct[2][3]=level[12];
     dct[3][2]=level[13];
     dct[3][3]=level[14];
#if 0
    int i;

    for( i = 1; i < 16; i++ )
    {
        dct[scan_zigzag_y[i]][scan_zigzag_x[i]] = level[i - 1];
    }
#endif
}

static inline void unscan_zigzag_2x2_dc( int16_t dct[2][2], int level[4] )
{
    dct[0][0] = level[0];
    dct[0][1] = level[1];
    dct[1][0] = level[2];
    dct[1][1] = level[3];
}

/************************************************************************
函数功能：解码一个macroblock，输出图像的原始数据

************************************************************************/
int x264_macroblock_decode( x264_t *h, x264_macroblock_t *mb )
{
  
    int i_qscale;
    int ch;
    int i;
	int cbp_dc;
	//x264_cpu_restore( h->param.cpu );  // addded  2005-4-12
    if( !IS_INTRA(mb->i_type ) )
    {
        /* Motion compensation */
        //x264_mb_dec_mc( h );  // modified  
		x264_mb_mc( h );
    }

    /* luma */
    i_qscale = mb->qp[mb->i_mb_xy]; 
	cbp_dc=(mb->cbp[mb->i_mb_xy])>>8;
    if( mb->i_type == I_16x16 )
    {
        int     i_mode = x264_mb_pred_mode16x16_valid( mb, mb->i_intra16x16_pred_mode );
        //int16_t dct4x4[16+1][4][4];
		DECLARE_ALIGNED( int16_t, dct4x4[16+1][4][4], 16 );
		memset(dct4x4,0,sizeof(int16_t)*17*4*4);

        // do the right prediction 
        h->predict_16x16[i_mode]( h->mb.pic.p_fdec[0], h->mb.pic.i_stride[0] ); 
		
		if((cbp_dc&0x1)||mb->i_cbp_luma!=0)
		{
			// decode the 16x16 macroblock 
			for( i = 0; i < 16; i++ )
			{
				if(mb->i_cbp_luma& ( 1 << ( i / 4 ) ))
				{
					unscan_zigzag_4x4( dct4x4[1+i], h->dct.block[i].residual_ac );
					h->x264_dequant_4x4( dct4x4[1+i], i_qscale );
				}
				
				//h->dctf.idct4x4( luma[i], dct4x4[i+1] );
			}
			
			// get dc coeffs 
			if(cbp_dc&0x1)
			{
				unscan_zigzag_4x4full( dct4x4[0], h->dct.luma16x16_dc );
				h->dctf.idct4x4dc( dct4x4[0]);
				h->x264_dequant_4x4_dc( dct4x4[0], i_qscale );
				for( i = 0; i < 16; i++ )
				{
					// copy dc coeff 
					dct4x4[1+i][0][0] = dct4x4[0][block_idx_y[i]][block_idx_x[i]];
				}
			}
			
			
			// put pixels to fdec 
			h->dctf.add16x16_idct( h->mb.pic.p_fdec[0], h->mb.pic.i_stride[0], &dct4x4[1] );//此处的函数可能会有问题 2005-4-9
		}

	}
    else if( mb->i_type == I_4x4 )
    {
		//int16_t dct4x4[4][4];
		DECLARE_ALIGNED( int16_t, dct4x4[4][4], 16 );
        for( i = 0; i < 16; i++ )
        {				
			uint8_t *p_dst_by;
			int     i_mode;
			int     b_emu;
			const int i_stride = h->mb.pic.i_stride[0];
			
			//Do the right prediction 
			p_dst_by =&h->mb.pic.p_fdec[0][4 * block_idx_x[i] + 4 * block_idx_y[i] * i_stride];
			//p_dst_by = ctx->p_fdec[0] + 4 * block_idx_x[i] + 4 * block_idx_y[i] * ctx->i_fdec[0];
			/*   i_mode   = x264_mb_pred_mode4x4_valid( mb, i, mb->block[i].i_intra4x4_pred_mode, &b_emu );
			if( b_emu )
			{
			fprintf( stderr, "mmmh b_emu\n" );
			memset( &p_dst_by[4], p_dst_by[3], 4 );
			}
			h->predict_4x4[i_mode]( p_dst_by, ctx->i_fdec[0] );
			*/ //modified  2005-4-9
			
			i_mode =x264_mb_pred_mode4x4_valid( mb, i,h->mb.cache.intra4x4_pred_mode[x264_scan8[i]], &b_emu);
			if( b_emu )
			{
				fprintf( stderr, "mmmh b_emu\n" );
				memset( &p_dst_by[4], p_dst_by[3], 4 );
			}
			// 此处可能存在问题，主要是x264_scan8的顺序问题
			/* Do the right prediction */
			h->predict_4x4[i_mode]( p_dst_by, i_stride );
			
			if( mb->cache.non_zero_count[x264_scan8[i]] > 0 && mb->i_cbp_luma& ( 1 << ( i / 4 ) ))
			{
				// decode one 4x4 block 
				unscan_zigzag_4x4full( dct4x4, h->dct.block[i].luma4x4 );
				
				h->x264_dequant_4x4( dct4x4, i_qscale );
				
				h->dctf.add4x4_idct( p_dst_by, i_stride, dct4x4 );
			}
          
        }
    }
    else /* Inter mb */
    {
		if(mb->i_cbp_luma!=0)
		{
			DECLARE_ALIGNED( int16_t, dct4x4[16][4][4], 16 );
			memset(dct4x4,0,sizeof(int16_t)*16*4*4);
			for( i = 0; i < 16; i++ )
			{
				if(mb->i_cbp_luma& ( 1 << ( i / 4 ) ))
				{
					unscan_zigzag_4x4full( dct4x4[i], h->dct.block[i].luma4x4 );
					h->x264_dequant_4x4( dct4x4[i], i_qscale );
				}
			}
			h->dctf.add16x16_idct( h->mb.pic.p_fdec[0], h->mb.pic.i_stride[0], dct4x4 ); 
		}
    }

    /* chroma */
    i_qscale = i_chroma_qp_table[x264_clip3( i_qscale + h->pps->i_chroma_qp_index_offset, 0, 51 )];
	//i_qscale = i_chroma_qp_table[i_qscale + h->pps->i_chroma_qp_index_offset]; //modified by lihengzhong 2005-5-17
    if( IS_INTRA( mb->i_type ) )
    {
        int i_mode = x264_mb_pred_mode8x8_valid( mb, mb->i_chroma_pred_mode );
        /* do the right prediction */
        h->predict_8x8[i_mode]( h->mb.pic.p_fdec[1], h->mb.pic.i_stride[1] );
        h->predict_8x8[i_mode]( h->mb.pic.p_fdec[2], h->mb.pic.i_stride[2] );

    }

 
	if(mb->i_cbp_chroma!=0)
	{
		//int16_t dct2x2[2][2];
		//int16_t dct4x4[4][4][4];
		DECLARE_ALIGNED( int16_t, dct2x2[2][2], 16 );
		DECLARE_ALIGNED( int16_t, dct4x4[4][4][4], 16 );
		for( ch = 0; ch < 2; ch++ )
		{			
			memset(dct2x2,0,sizeof(int16_t)*2*2);
			memset(dct4x4,0,sizeof(int16_t)*4*4*4);	
			
			if(mb->i_cbp_chroma&0x02)
			{
				for( i = 0; i < 4; i++ )
				{
					unscan_zigzag_4x4( dct4x4[i], h->dct.block[16+i+ch*4].residual_ac ); //错误的地方，一个是缺少判断
					h->x264_dequant_4x4( dct4x4[i], i_qscale );
					
				}
			}

			/* get dc chroma */
			if(mb->i_cbp_chroma &0x03)
			{
				unscan_zigzag_2x2_dc( dct2x2, h->dct.chroma_dc[ch] );
				h->dctf.idct2x2dc( dct2x2 );
				h->x264_dequant_2x2_dc( dct2x2, i_qscale );
				
				for(i=0;i<4;i++)
				{
					/* copy dc coeff */
					dct4x4[i][0][0] = dct2x2[block_idx_y[i]][block_idx_x[i]];  
				}
			}
			
			
			// h->pixf.add8x8( ctx->p_fdec[1+ch], ctx->i_fdec[1+ch], chroma ); //delete 
			h->dctf.add8x8_idct( h->mb.pic.p_fdec[1+ch], h->mb.pic.i_stride[1+ch], dct4x4 ); // modified  2005-4-9
		}
	}
    

    return 0;
}


/********************************************************
函数功能：解码skip类型的macroblock

********************************************************/
void x264_macroblock_decode_skip( x264_t *h )
{
    int i;
//    int x, y;
    int mv[2];
    int i_ref_max = h->frames.i_max_ref0;	
    /* decode it as a 16x16 with no luma/chroma */
    h->mb.i_type = P_L0;
    h->mb.i_partition = D_16x16;
    h->mb.i_cbp_luma = 0;
    h->mb.i_cbp_chroma = 0;
   for( i = 0; i < 16 + 8; i++ )
    {
        h->mb.non_zero_count[h->mb.i_mb_xy][i] = 0;
    }
    /*
    for( i = 0; i < 16; i++ )
    {
        array_zero_set( h->dct.block[i].luma4x4, 16 );
    }
    array_zero_set( h->dct.chroma_dc[0], 4 );
    array_zero_set( h->dct.chroma_dc[1], 4 );
    for( i = 0; i < 8; i++ )
    {
        array_zero_set( h->dct.block[16+i].residual_ac, 15 );
    }
*/
    // set ref0 
   /* for( x = 0; x < 4; x++ )
    {
        for( y = 0; y < 4; y++ )
        {
            mb->partition[x][y].i_ref[0] = 0;
        }
    }
    
    */  // delete   2005-4-9
    /* get mv */
    x264_mb_predict_mv_pskip( h, mv );

  //  x264_mb_partition_mv_set( &h->mb, 0, 0, 0, mv );
    x264_macroblock_cache_mv(h,block_idx_x[0],block_idx_y[0],4,4,0,mv[0],mv[1]);
    // extend the mv tyoe 2005-4-15
          
   x264_macroblock_cache_ref(h,block_idx_x[0],block_idx_y[0],4,4,0,0);  
     // added 2005-4-15
	
    /* Motion compensation */
    x264_mb_mc( h );

    h->mb.i_type = P_SKIP;
    h->mb.type[h->mb.i_mb_xy]=P_SKIP;
    h->mb.qp[h->mb.i_mb_xy]=h->mb.i_last_qp;
    h->mb.i_last_dqp=0;
	   
}

/********************************************************
函数功能：解码丢失帧的类型的macroblock

********************************************************/
void x264_macroblock_decode_lostskip( x264_t *h )
{
    int i;
//    int x, y;
    int mv[2];
    int i_ref_max = h->frames.i_max_ref0;
    
    int i_list=0;
    const int i_mb_y = h->mb.i_mb_xy / h->sps->i_mb_width;
    const int i_mb_x = h->mb.i_mb_xy % h->sps->i_mb_width;
    const int i_mb_4x4 = 4*(i_mb_y * h->mb.i_b4_stride + i_mb_x);
   //const int s4x4 = h->mb.i_b4_stride;
    const int iv = i_mb_4x4;

  // static int maxmv=0,minmv=0;
	
    /* decode it as a 16x16 with no luma/chroma */
    h->mb.i_type = P_L0;
    h->mb.i_partition = D_16x16;
    h->mb.i_cbp_luma = 0;
    h->mb.i_cbp_chroma = 0;
   for( i = 0; i < 16 + 8; i++ )
    {
        h->mb.non_zero_count[h->mb.i_mb_xy][i] = 0;
    }

   
   mv[0] = x264_clip3(h->mb.mv[i_list][iv][0],-48,48);
   mv[1] = x264_clip3(h->mb.mv[i_list][iv][1],-48,48); //modified by xia wei ping
            
   // x264_mb_predict_mv_pskip( h, mv );	

  //  x264_mb_partition_mv_set( &h->mb, 0, 0, 0, mv );
    x264_macroblock_cache_mv(h,block_idx_x[0],block_idx_y[0],4,4,0,mv[0],mv[1]);
    // extend the mv tyoe 2005-4-15
          
   x264_macroblock_cache_ref(h,block_idx_x[0],block_idx_y[0],4,4,0,0);  
     // added 2005-4-15
	
    /* Motion compensation */
    x264_mb_mc( h );

    h->mb.i_type = P_SKIP;
    h->mb.type[h->mb.i_mb_xy]=P_SKIP;
    h->mb.qp[h->mb.i_mb_xy]=h->mb.i_last_qp;
    h->mb.i_last_dqp=0;

 //   if(maxmv<mv[0])  maxmv =mv[0];
//    if(minmv >mv[0])  minmv =mv[0];
	   
}

/**********************************************************************
函数功能：初始化结构体中的一些变量

**********************************************************************/
void x264_macroblock_decode_cache_load( x264_t *h, int i_mb_x, int i_mb_y )
{
    const int i_mb_4x4 = 4*(i_mb_y * h->mb.i_b4_stride + i_mb_x);
    const int i_mb_8x8 = 2*(i_mb_y * h->mb.i_b8_stride + i_mb_x);

    int i_top_xy = -1;
    int i_left_xy = -1;
    int i_top_type = -1;    /* gcc warn */
    int i_left_type= -1;

    int i;

    /* init index */
    h->mb.i_mb_x = i_mb_x;
    h->mb.i_mb_y = i_mb_y;
    h->mb.i_mb_xy = i_mb_y * h->mb.i_mb_stride + i_mb_x;
    h->mb.i_b8_xy = i_mb_8x8;
    h->mb.i_b4_xy = i_mb_4x4;
    h->mb.i_neighbour = 0;

    /* load picture pointers */
    for( i = 0; i < 3; i++ )
    {
        const int w = (i == 0 ? 16 : 8);
        const int i_stride = h->fdec->i_stride[i];
        int   j;

        h->mb.pic.i_stride[i] = i_stride;

        h->mb.pic.p_fdec[i] = &h->fdec->plane[i][ w * ( i_mb_x + i_mb_y * i_stride )];

        for( j = 0; j < h->i_ref0; j++ )
        {
            h->mb.pic.p_fref[0][j][i==0 ? 0:i+3] = &h->fref0[j]->plane[i][ w * ( i_mb_x + i_mb_y * i_stride )];
            h->mb.pic.p_fref[0][j][i+1] = &h->fref0[j]->filtered[i+1][ 16 * ( i_mb_x + i_mb_y * h->fdec->i_stride[0] )];
        }
        for( j = 0; j < h->i_ref1; j++ )
        {
            h->mb.pic.p_fref[1][j][i==0 ? 0:i+3] = &h->fref1[j]->plane[i][ w * ( i_mb_x + i_mb_y * i_stride )];
            h->mb.pic.p_fref[1][j][i+1] = &h->fref1[j]->filtered[i+1][ 16 * ( i_mb_x + i_mb_y * h->fdec->i_stride[0] )];
        }
    }
	//initialize the dct block memery to 0. // added  2005-4-10
	
	memset( &h->dct, 0,  sizeof( DCT_coefficient ) );
	memset( h->mb.cache.intra4x4_pred_mode, I_PRED_4x4_DC,  sizeof( int ) *X264_SCAN8_SIZE);
	memset( h->mb.cache.non_zero_count, 0,  sizeof( int ) *X264_SCAN8_SIZE);
	memset( h->mb.cache.mvd, 0,  sizeof( int16_t ) *4*X264_SCAN8_SIZE);
	memset( h->mb.cache.mv, 0,  sizeof( int16_t ) *4*X264_SCAN8_SIZE);
	
	
	h->mb.i_partition =-1;
	
    /* load cache */
    if( i_mb_y > 0 )
    {
        i_top_xy  = h->mb.i_mb_xy - h->mb.i_mb_stride;
        i_top_type= h->mb.type[i_top_xy];

        h->mb.i_neighbour |= MB_TOP;

        /* load intra4x4 */
        h->mb.cache.intra4x4_pred_mode[x264_scan8[0] - 8] = h->mb.intra4x4_pred_mode[i_top_xy][0];
        h->mb.cache.intra4x4_pred_mode[x264_scan8[1] - 8] = h->mb.intra4x4_pred_mode[i_top_xy][1];
        h->mb.cache.intra4x4_pred_mode[x264_scan8[4] - 8] = h->mb.intra4x4_pred_mode[i_top_xy][2];
        h->mb.cache.intra4x4_pred_mode[x264_scan8[5] - 8] = h->mb.intra4x4_pred_mode[i_top_xy][3];

        /* load non_zero_count */
        h->mb.cache.non_zero_count[x264_scan8[0] - 8] = h->mb.non_zero_count[i_top_xy][10];
        h->mb.cache.non_zero_count[x264_scan8[1] - 8] = h->mb.non_zero_count[i_top_xy][11];
        h->mb.cache.non_zero_count[x264_scan8[4] - 8] = h->mb.non_zero_count[i_top_xy][14];
        h->mb.cache.non_zero_count[x264_scan8[5] - 8] = h->mb.non_zero_count[i_top_xy][15];

        h->mb.cache.non_zero_count[x264_scan8[16+0] - 8] = h->mb.non_zero_count[i_top_xy][16+2];
        h->mb.cache.non_zero_count[x264_scan8[16+1] - 8] = h->mb.non_zero_count[i_top_xy][16+3];

        h->mb.cache.non_zero_count[x264_scan8[16+4+0] - 8] = h->mb.non_zero_count[i_top_xy][16+4+2];
        h->mb.cache.non_zero_count[x264_scan8[16+4+1] - 8] = h->mb.non_zero_count[i_top_xy][16+4+3];
    }
    else
    {
        /* load intra4x4 */
        h->mb.cache.intra4x4_pred_mode[x264_scan8[0] - 8] =
        h->mb.cache.intra4x4_pred_mode[x264_scan8[1] - 8] =
        h->mb.cache.intra4x4_pred_mode[x264_scan8[4] - 8] =
        h->mb.cache.intra4x4_pred_mode[x264_scan8[5] - 8] = -1;

        /* load non_zero_count */
        h->mb.cache.non_zero_count[x264_scan8[0] - 8] =
        h->mb.cache.non_zero_count[x264_scan8[1] - 8] =
        h->mb.cache.non_zero_count[x264_scan8[4] - 8] =
        h->mb.cache.non_zero_count[x264_scan8[5] - 8] =
        h->mb.cache.non_zero_count[x264_scan8[16+0] - 8] =
        h->mb.cache.non_zero_count[x264_scan8[16+1] - 8] =
        h->mb.cache.non_zero_count[x264_scan8[16+4+0] - 8] =
        h->mb.cache.non_zero_count[x264_scan8[16+4+1] - 8] = 0x80;

    }

    if( i_mb_x > 0 )
    {
        i_left_xy  = h->mb.i_mb_xy - 1;
        i_left_type= h->mb.type[i_left_xy];

        h->mb.i_neighbour |= MB_LEFT;

        /* load intra4x4 */
        h->mb.cache.intra4x4_pred_mode[x264_scan8[0 ] - 1] = h->mb.intra4x4_pred_mode[i_left_xy][4];
        h->mb.cache.intra4x4_pred_mode[x264_scan8[2 ] - 1] = h->mb.intra4x4_pred_mode[i_left_xy][5];
        h->mb.cache.intra4x4_pred_mode[x264_scan8[8 ] - 1] = h->mb.intra4x4_pred_mode[i_left_xy][6];
        h->mb.cache.intra4x4_pred_mode[x264_scan8[10] - 1] = h->mb.intra4x4_pred_mode[i_left_xy][3];

        /* load non_zero_count */
        h->mb.cache.non_zero_count[x264_scan8[0 ] - 1] = h->mb.non_zero_count[i_left_xy][5];
        h->mb.cache.non_zero_count[x264_scan8[2 ] - 1] = h->mb.non_zero_count[i_left_xy][7];
        h->mb.cache.non_zero_count[x264_scan8[8 ] - 1] = h->mb.non_zero_count[i_left_xy][13];
        h->mb.cache.non_zero_count[x264_scan8[10] - 1] = h->mb.non_zero_count[i_left_xy][15];
       
        h->mb.cache.non_zero_count[x264_scan8[16+0] - 1] = h->mb.non_zero_count[i_left_xy][16+1];
        h->mb.cache.non_zero_count[x264_scan8[16+2] - 1] = h->mb.non_zero_count[i_left_xy][16+3];

        h->mb.cache.non_zero_count[x264_scan8[16+4+0] - 1] = h->mb.non_zero_count[i_left_xy][16+4+1];
        h->mb.cache.non_zero_count[x264_scan8[16+4+2] - 1] = h->mb.non_zero_count[i_left_xy][16+4+3];
    }
    else
    {
        h->mb.cache.intra4x4_pred_mode[x264_scan8[0 ] - 1] =
        h->mb.cache.intra4x4_pred_mode[x264_scan8[2 ] - 1] =
        h->mb.cache.intra4x4_pred_mode[x264_scan8[8 ] - 1] =
        h->mb.cache.intra4x4_pred_mode[x264_scan8[10] - 1] = -1;

        /* load non_zero_count */
        h->mb.cache.non_zero_count[x264_scan8[0 ] - 1] =
        h->mb.cache.non_zero_count[x264_scan8[2 ] - 1] =
        h->mb.cache.non_zero_count[x264_scan8[8 ] - 1] =
        h->mb.cache.non_zero_count[x264_scan8[10] - 1] =
        h->mb.cache.non_zero_count[x264_scan8[16+0] - 1] =
        h->mb.cache.non_zero_count[x264_scan8[16+2] - 1] =
        h->mb.cache.non_zero_count[x264_scan8[16+4+0] - 1] =
        h->mb.cache.non_zero_count[x264_scan8[16+4+2] - 1] = 0x80;
    }

    if( i_mb_y > 0 && i_mb_x < h->sps->i_mb_width - 1 )
    {
        h->mb.i_neighbour |= MB_TOPRIGHT;
    }

    /* load ref/mv/mvd */
    if( h->sh.i_type != SLICE_TYPE_I )
    {
        const int s8x8 = h->mb.i_b8_stride;
        const int s4x4 = h->mb.i_b4_stride;

        int i_top_left_xy   = -1;
        int i_top_right_xy  = -1;

        int i_list;

        if( h->mb.i_mb_y > 0 && h->mb.i_mb_x > 0 )
        {
            i_top_left_xy   = i_top_xy - 1;
        }
        if( h->mb.i_mb_y > 0 && h->mb.i_mb_x < h->sps->i_mb_width - 1 )
        {
            i_top_right_xy = i_top_xy + 1;
        }

        for( i_list = 0; i_list < (h->sh.i_type == SLICE_TYPE_B ? 2  : 1 ); i_list++ )
        {
            /*
            h->mb.cache.ref[i_list][x264_scan8[5 ]+1] =
            h->mb.cache.ref[i_list][x264_scan8[7 ]+1] =
            h->mb.cache.ref[i_list][x264_scan8[13]+1] = -2;
            */

            if( i_top_left_xy >= 0 )
            {
                const int i8 = x264_scan8[0] - 1 - 1*8;
                const int ir = i_mb_8x8 - s8x8 - 1;
                const int iv = i_mb_4x4 - s4x4 - 1;
                h->mb.cache.ref[i_list][i8]  = h->mb.ref[i_list][ir];
                h->mb.cache.mv[i_list][i8][0] = h->mb.mv[i_list][iv][0];
                h->mb.cache.mv[i_list][i8][1] = h->mb.mv[i_list][iv][1];
            }
            else
            {
                const int i8 = x264_scan8[0] - 1 - 1*8;
                h->mb.cache.ref[i_list][i8] = -2;
                h->mb.cache.mv[i_list][i8][0] = 0;
                h->mb.cache.mv[i_list][i8][1] = 0;
            }

            if( i_top_xy >= 0 )
            {
                const int i8 = x264_scan8[0] - 8;
                const int ir = i_mb_8x8 - s8x8;
                const int iv = i_mb_4x4 - s4x4;

                h->mb.cache.ref[i_list][i8+0] =
                h->mb.cache.ref[i_list][i8+1] = h->mb.ref[i_list][ir + 0];
                h->mb.cache.ref[i_list][i8+2] =
                h->mb.cache.ref[i_list][i8+3] = h->mb.ref[i_list][ir + 1];

                for( i = 0; i < 4; i++ )
                {
                    h->mb.cache.mv[i_list][i8+i][0] = h->mb.mv[i_list][iv + i][0];
                    h->mb.cache.mv[i_list][i8+i][1] = h->mb.mv[i_list][iv + i][1];
                }
            }
            else
            {
                const int i8 = x264_scan8[0] - 8;
                for( i = 0; i < 4; i++ )
                {
                    h->mb.cache.ref[i_list][i8+i] = -2;
                    h->mb.cache.mv[i_list][i8+i][0] =
                    h->mb.cache.mv[i_list][i8+i][1] = 0;
                }
            }

            if( i_top_right_xy >= 0 )
            {
                const int i8 = x264_scan8[0] + 4 - 1*8;
                const int ir = i_mb_8x8 - s8x8 + 2;
                const int iv = i_mb_4x4 - s4x4 + 4;

                h->mb.cache.ref[i_list][i8]  = h->mb.ref[i_list][ir];
                h->mb.cache.mv[i_list][i8][0] = h->mb.mv[i_list][iv][0];
                h->mb.cache.mv[i_list][i8][1] = h->mb.mv[i_list][iv][1];
            }
            else
            {
                const int i8 = x264_scan8[0] + 4 - 1*8;
                h->mb.cache.ref[i_list][i8] = -2;
                h->mb.cache.mv[i_list][i8][0] = 0;
                h->mb.cache.mv[i_list][i8][1] = 0;
            }

            if( i_left_xy >= 0 )
            {
                const int i8 = x264_scan8[0] - 1;
                const int ir = i_mb_8x8 - 1;
                const int iv = i_mb_4x4 - 1;

                h->mb.cache.ref[i_list][i8+0*8] =
                h->mb.cache.ref[i_list][i8+1*8] = h->mb.ref[i_list][ir + 0*s8x8];
                h->mb.cache.ref[i_list][i8+2*8] =
                h->mb.cache.ref[i_list][i8+3*8] = h->mb.ref[i_list][ir + 1*s8x8];

                for( i = 0; i < 4; i++ )
                {
                    h->mb.cache.mv[i_list][i8+i*8][0] = h->mb.mv[i_list][iv + i*s4x4][0];
                    h->mb.cache.mv[i_list][i8+i*8][1] = h->mb.mv[i_list][iv + i*s4x4][1];
                }
            }
            else
            {
                const int i8 = x264_scan8[0] - 1;
                for( i = 0; i < 4; i++ )
                {
                    h->mb.cache.ref[i_list][i8+i*8] = -2;
                    h->mb.cache.mv[i_list][i8+i*8][0] =
                    h->mb.cache.mv[i_list][i8+i*8][1] = 0;
                }
            }

            if( h->param.b_cabac )
            {
                if( i_top_xy >= 0 )
                {
                    const int i8 = x264_scan8[0] - 8;
                    const int iv = i_mb_4x4 - s4x4;
                    for( i = 0; i < 4; i++ )
                    {
                        h->mb.cache.mvd[i_list][i8+i][0] = h->mb.mvd[i_list][iv + i][0];
                        h->mb.cache.mvd[i_list][i8+i][1] = h->mb.mvd[i_list][iv + i][1];
                    }
                }
                else
                {
                    const int i8 = x264_scan8[0] - 8;
                    for( i = 0; i < 4; i++ )
                    {
                        h->mb.cache.mvd[i_list][i8+i][0] =
                        h->mb.cache.mvd[i_list][i8+i][1] = 0;
                    }
                }

                if( i_left_xy >= 0 )
                {
                    const int i8 = x264_scan8[0] - 1;
                    const int iv = i_mb_4x4 - 1;
                    for( i = 0; i < 4; i++ )
                    {
                        h->mb.cache.mvd[i_list][i8+i*8][0] = h->mb.mvd[i_list][iv + i*s4x4][0];
                        h->mb.cache.mvd[i_list][i8+i*8][1] = h->mb.mvd[i_list][iv + i*s4x4][1];
                    }
                }
                else
                {
                    const int i8 = x264_scan8[0] - 1;
                    for( i = 0; i < 4; i++ )
                    {
                        h->mb.cache.mvd[i_list][i8+i*8][0] =
                        h->mb.cache.mvd[i_list][i8+i*8][1] = 0;
                    }
                }
            }
        }

        /* load skip */
        if( h->param.b_cabac )
        {
            if( h->sh.i_type == SLICE_TYPE_B )
            {
                memset( h->mb.cache.skip, 0, X264_SCAN8_SIZE * sizeof( int8_t ) );
                if( i_left_xy >= 0 )
                {
                    h->mb.cache.skip[x264_scan8[0] - 1] = h->mb.skipbp[i_left_xy] & 0x2;
                    h->mb.cache.skip[x264_scan8[8] - 1] = h->mb.skipbp[i_left_xy] & 0x8;
                }
                if( i_top_xy >= 0 )
                {
                    h->mb.cache.skip[x264_scan8[0] - 8] = h->mb.skipbp[i_top_xy] & 0x4;
                    h->mb.cache.skip[x264_scan8[4] - 8] = h->mb.skipbp[i_top_xy] & 0x8;
                }
            }
            else if( h->mb.i_mb_xy == 0 && h->sh.i_type == SLICE_TYPE_P )
            {
                memset( h->mb.cache.skip, 0, X264_SCAN8_SIZE * sizeof( int8_t ) );
            }
        }
    }
}

/**********************************************************************
函数功能：初始化结构体中的一些变量

**********************************************************************/
void x264_macroblock_decode_lostcache_load( x264_t *h, int i_mb_x, int i_mb_y )
{
    const int i_mb_4x4 = 4*(i_mb_y * h->mb.i_b4_stride + i_mb_x);
    const int i_mb_8x8 = 2*(i_mb_y * h->mb.i_b8_stride + i_mb_x);

    int i_top_xy = -1;
    int i_left_xy = -1;
    int i_top_type = -1;    /* gcc warn */
    int i_left_type= -1;

    int i;

    /* init index */
    h->mb.i_mb_x = i_mb_x;
    h->mb.i_mb_y = i_mb_y;
    h->mb.i_mb_xy = i_mb_y * h->mb.i_mb_stride + i_mb_x;
    h->mb.i_b8_xy = i_mb_8x8;
    h->mb.i_b4_xy = i_mb_4x4;
    h->mb.i_neighbour = 0;

    /* load picture pointers */
    for( i = 0; i < 3; i++ )
    {
        const int w = (i == 0 ? 16 : 8);
        const int i_stride = h->fdec->i_stride[i];
        int   j;

        h->mb.pic.i_stride[i] = i_stride;

        h->mb.pic.p_fdec[i] = &h->fdec->plane[i][ w * ( i_mb_x + i_mb_y * i_stride )];

        for( j = 0; j < h->i_ref0; j++ )
        {
            h->mb.pic.p_fref[0][j][i==0 ? 0:i+3] = &h->fref0[j]->plane[i][ w * ( i_mb_x + i_mb_y * i_stride )];
            h->mb.pic.p_fref[0][j][i+1] = &h->fref0[j]->filtered[i+1][ 16 * ( i_mb_x + i_mb_y * h->fdec->i_stride[0] )];
        }
        for( j = 0; j < h->i_ref1; j++ )
        {
            h->mb.pic.p_fref[1][j][i==0 ? 0:i+3] = &h->fref1[j]->plane[i][ w * ( i_mb_x + i_mb_y * i_stride )];
            h->mb.pic.p_fref[1][j][i+1] = &h->fref1[j]->filtered[i+1][ 16 * ( i_mb_x + i_mb_y * h->fdec->i_stride[0] )];
        }
    }
	//initialize the dct block memery to 0. // added  2005-4-10
	
	memset( &h->dct, 0,  sizeof( DCT_coefficient ) );
	memset( h->mb.cache.intra4x4_pred_mode, I_PRED_4x4_DC,  sizeof( int ) *X264_SCAN8_SIZE);
	memset( h->mb.cache.non_zero_count, 0,  sizeof( int ) *X264_SCAN8_SIZE);
	memset( h->mb.cache.mvd, 0,  sizeof( int16_t ) *4*X264_SCAN8_SIZE);
	memset( h->mb.cache.mv, 0,  sizeof( int16_t ) *4*X264_SCAN8_SIZE);
	
	
	h->mb.i_partition =-1;
	
    /* load cache */
    if( i_mb_y > 0 )
    {
        i_top_xy  = h->mb.i_mb_xy - h->mb.i_mb_stride;
        i_top_type= h->mb.type[i_top_xy];

        h->mb.i_neighbour |= MB_TOP;

        /* load intra4x4 */
        h->mb.cache.intra4x4_pred_mode[x264_scan8[0] - 8] = h->mb.intra4x4_pred_mode[i_top_xy][0];
        h->mb.cache.intra4x4_pred_mode[x264_scan8[1] - 8] = h->mb.intra4x4_pred_mode[i_top_xy][1];
        h->mb.cache.intra4x4_pred_mode[x264_scan8[4] - 8] = h->mb.intra4x4_pred_mode[i_top_xy][2];
        h->mb.cache.intra4x4_pred_mode[x264_scan8[5] - 8] = h->mb.intra4x4_pred_mode[i_top_xy][3];

        /* load non_zero_count */
        h->mb.cache.non_zero_count[x264_scan8[0] - 8] = h->mb.non_zero_count[i_top_xy][10];
        h->mb.cache.non_zero_count[x264_scan8[1] - 8] = h->mb.non_zero_count[i_top_xy][11];
        h->mb.cache.non_zero_count[x264_scan8[4] - 8] = h->mb.non_zero_count[i_top_xy][14];
        h->mb.cache.non_zero_count[x264_scan8[5] - 8] = h->mb.non_zero_count[i_top_xy][15];

        h->mb.cache.non_zero_count[x264_scan8[16+0] - 8] = h->mb.non_zero_count[i_top_xy][16+2];
        h->mb.cache.non_zero_count[x264_scan8[16+1] - 8] = h->mb.non_zero_count[i_top_xy][16+3];

        h->mb.cache.non_zero_count[x264_scan8[16+4+0] - 8] = h->mb.non_zero_count[i_top_xy][16+4+2];
        h->mb.cache.non_zero_count[x264_scan8[16+4+1] - 8] = h->mb.non_zero_count[i_top_xy][16+4+3];
    }
    else
    {
        /* load intra4x4 */
        h->mb.cache.intra4x4_pred_mode[x264_scan8[0] - 8] =
        h->mb.cache.intra4x4_pred_mode[x264_scan8[1] - 8] =
        h->mb.cache.intra4x4_pred_mode[x264_scan8[4] - 8] =
        h->mb.cache.intra4x4_pred_mode[x264_scan8[5] - 8] = -1;

        /* load non_zero_count */
        h->mb.cache.non_zero_count[x264_scan8[0] - 8] =
        h->mb.cache.non_zero_count[x264_scan8[1] - 8] =
        h->mb.cache.non_zero_count[x264_scan8[4] - 8] =
        h->mb.cache.non_zero_count[x264_scan8[5] - 8] =
        h->mb.cache.non_zero_count[x264_scan8[16+0] - 8] =
        h->mb.cache.non_zero_count[x264_scan8[16+1] - 8] =
        h->mb.cache.non_zero_count[x264_scan8[16+4+0] - 8] =
        h->mb.cache.non_zero_count[x264_scan8[16+4+1] - 8] = 0x80;

    }

    if( i_mb_x > 0 )
    {
        i_left_xy  = h->mb.i_mb_xy - 1;
        i_left_type= h->mb.type[i_left_xy];

        h->mb.i_neighbour |= MB_LEFT;

        /* load intra4x4 */
        h->mb.cache.intra4x4_pred_mode[x264_scan8[0 ] - 1] = h->mb.intra4x4_pred_mode[i_left_xy][4];
        h->mb.cache.intra4x4_pred_mode[x264_scan8[2 ] - 1] = h->mb.intra4x4_pred_mode[i_left_xy][5];
        h->mb.cache.intra4x4_pred_mode[x264_scan8[8 ] - 1] = h->mb.intra4x4_pred_mode[i_left_xy][6];
        h->mb.cache.intra4x4_pred_mode[x264_scan8[10] - 1] = h->mb.intra4x4_pred_mode[i_left_xy][3];

        /* load non_zero_count */
        h->mb.cache.non_zero_count[x264_scan8[0 ] - 1] = h->mb.non_zero_count[i_left_xy][5];
        h->mb.cache.non_zero_count[x264_scan8[2 ] - 1] = h->mb.non_zero_count[i_left_xy][7];
        h->mb.cache.non_zero_count[x264_scan8[8 ] - 1] = h->mb.non_zero_count[i_left_xy][13];
        h->mb.cache.non_zero_count[x264_scan8[10] - 1] = h->mb.non_zero_count[i_left_xy][15];
       
        h->mb.cache.non_zero_count[x264_scan8[16+0] - 1] = h->mb.non_zero_count[i_left_xy][16+1];
        h->mb.cache.non_zero_count[x264_scan8[16+2] - 1] = h->mb.non_zero_count[i_left_xy][16+3];

        h->mb.cache.non_zero_count[x264_scan8[16+4+0] - 1] = h->mb.non_zero_count[i_left_xy][16+4+1];
        h->mb.cache.non_zero_count[x264_scan8[16+4+2] - 1] = h->mb.non_zero_count[i_left_xy][16+4+3];
    }
    else
    {
        h->mb.cache.intra4x4_pred_mode[x264_scan8[0 ] - 1] =
        h->mb.cache.intra4x4_pred_mode[x264_scan8[2 ] - 1] =
        h->mb.cache.intra4x4_pred_mode[x264_scan8[8 ] - 1] =
        h->mb.cache.intra4x4_pred_mode[x264_scan8[10] - 1] = -1;

        /* load non_zero_count */
        h->mb.cache.non_zero_count[x264_scan8[0 ] - 1] =
        h->mb.cache.non_zero_count[x264_scan8[2 ] - 1] =
        h->mb.cache.non_zero_count[x264_scan8[8 ] - 1] =
        h->mb.cache.non_zero_count[x264_scan8[10] - 1] =
        h->mb.cache.non_zero_count[x264_scan8[16+0] - 1] =
        h->mb.cache.non_zero_count[x264_scan8[16+2] - 1] =
        h->mb.cache.non_zero_count[x264_scan8[16+4+0] - 1] =
        h->mb.cache.non_zero_count[x264_scan8[16+4+2] - 1] = 0x80;
    }

    if( i_mb_y > 0 && i_mb_x < h->sps->i_mb_width - 1 )
    {
        h->mb.i_neighbour |= MB_TOPRIGHT;
    }

    /* load ref/mv/mvd */
    if( h->sh.i_type != SLICE_TYPE_I )
    {
        const int s8x8 = h->mb.i_b8_stride;
        const int s4x4 = h->mb.i_b4_stride;

        int i_top_left_xy   = -1;
        int i_top_right_xy  = -1;

        int i_list;

        if( h->mb.i_mb_y > 0 && h->mb.i_mb_x > 0 )
        {
            i_top_left_xy   = i_top_xy - 1;
        }
        if( h->mb.i_mb_y > 0 && h->mb.i_mb_x < h->sps->i_mb_width - 1 )
        {
            i_top_right_xy = i_top_xy + 1;
        }

        for( i_list = 0; i_list < (h->sh.i_type == SLICE_TYPE_B ? 2  : 1 ); i_list++ )
        {
            /*
            h->mb.cache.ref[i_list][x264_scan8[5 ]+1] =
            h->mb.cache.ref[i_list][x264_scan8[7 ]+1] =
            h->mb.cache.ref[i_list][x264_scan8[13]+1] = -2;
            */

            if( i_top_left_xy >= 0 )
            {
                const int i8 = x264_scan8[0] - 1 - 1*8;
                const int ir = i_mb_8x8 - s8x8 - 1;
                const int iv = i_mb_4x4 - s4x4 - 1;
                h->mb.cache.ref[i_list][i8]  = h->mb.ref[i_list][ir];
                h->mb.cache.mv[i_list][i8][0] = h->mb.mv[i_list][iv][0];
                h->mb.cache.mv[i_list][i8][1] = h->mb.mv[i_list][iv][1];
            }
            else
            {
                const int i8 = x264_scan8[0] - 1 - 1*8;
                h->mb.cache.ref[i_list][i8] = -2;
                h->mb.cache.mv[i_list][i8][0] = 0;
                h->mb.cache.mv[i_list][i8][1] = 0;
            }

            if( i_top_xy >= 0 )
            {
                const int i8 = x264_scan8[0] - 8;
                const int ir = i_mb_8x8 - s8x8;
                const int iv = i_mb_4x4 - s4x4;

                h->mb.cache.ref[i_list][i8+0] =
                h->mb.cache.ref[i_list][i8+1] = h->mb.ref[i_list][ir + 0];
                h->mb.cache.ref[i_list][i8+2] =
                h->mb.cache.ref[i_list][i8+3] = h->mb.ref[i_list][ir + 1];

                for( i = 0; i < 4; i++ )
                {
                    h->mb.cache.mv[i_list][i8+i][0] = h->mb.mv[i_list][iv + i][0];
                    h->mb.cache.mv[i_list][i8+i][1] = h->mb.mv[i_list][iv + i][1];
                }
            }
            else
            {
                const int i8 = x264_scan8[0] - 8;
                for( i = 0; i < 4; i++ )
                {
                    h->mb.cache.ref[i_list][i8+i] = -2;
                    h->mb.cache.mv[i_list][i8+i][0] =
                    h->mb.cache.mv[i_list][i8+i][1] = 0;
                }
            }

            if( i_top_right_xy >= 0 )
            {
                const int i8 = x264_scan8[0] + 4 - 1*8;
                const int ir = i_mb_8x8 - s8x8 + 2;
                const int iv = i_mb_4x4 - s4x4 + 4;

                h->mb.cache.ref[i_list][i8]  = h->mb.ref[i_list][ir];
                h->mb.cache.mv[i_list][i8][0] = h->mb.mv[i_list][iv][0];
                h->mb.cache.mv[i_list][i8][1] = h->mb.mv[i_list][iv][1];
            }
            else
            {
                const int i8 = x264_scan8[0] + 4 - 1*8;
                h->mb.cache.ref[i_list][i8] = -2;
                h->mb.cache.mv[i_list][i8][0] = 0;
                h->mb.cache.mv[i_list][i8][1] = 0;
            }

            if( i_left_xy >= 0 )
            {
                const int i8 = x264_scan8[0] - 1;
                const int ir = i_mb_8x8 - 1;
                const int iv = i_mb_4x4 - 1;

                h->mb.cache.ref[i_list][i8+0*8] =
                h->mb.cache.ref[i_list][i8+1*8] = h->mb.ref[i_list][ir + 0*s8x8];
                h->mb.cache.ref[i_list][i8+2*8] =
                h->mb.cache.ref[i_list][i8+3*8] = h->mb.ref[i_list][ir + 1*s8x8];

                for( i = 0; i < 4; i++ )
                {
                    h->mb.cache.mv[i_list][i8+i*8][0] = h->mb.mv[i_list][iv + i*s4x4][0];
                    h->mb.cache.mv[i_list][i8+i*8][1] = h->mb.mv[i_list][iv + i*s4x4][1];
                }
            }
            else
            {
                const int i8 = x264_scan8[0] - 1;
                for( i = 0; i < 4; i++ )
                {
                    h->mb.cache.ref[i_list][i8+i*8] = -2;
                    h->mb.cache.mv[i_list][i8+i*8][0] =
                    h->mb.cache.mv[i_list][i8+i*8][1] = 0;
                }
            }

            if( h->param.b_cabac )
            {
                if( i_top_xy >= 0 )
                {
                    const int i8 = x264_scan8[0] - 8;
                    const int iv = i_mb_4x4 - s4x4;
                    for( i = 0; i < 4; i++ )
                    {
                        h->mb.cache.mvd[i_list][i8+i][0] = h->mb.mvd[i_list][iv + i][0];
                        h->mb.cache.mvd[i_list][i8+i][1] = h->mb.mvd[i_list][iv + i][1];
                    }
                }
                else
                {
                    const int i8 = x264_scan8[0] - 8;
                    for( i = 0; i < 4; i++ )
                    {
                        h->mb.cache.mvd[i_list][i8+i][0] =
                        h->mb.cache.mvd[i_list][i8+i][1] = 0;
                    }
                }

                if( i_left_xy >= 0 )
                {
                    const int i8 = x264_scan8[0] - 1;
                    const int iv = i_mb_4x4 - 1;
                    for( i = 0; i < 4; i++ )
                    {
                        h->mb.cache.mvd[i_list][i8+i*8][0] = h->mb.mvd[i_list][iv + i*s4x4][0];
                        h->mb.cache.mvd[i_list][i8+i*8][1] = h->mb.mvd[i_list][iv + i*s4x4][1];
                    }
                }
                else
                {
                    const int i8 = x264_scan8[0] - 1;
                    for( i = 0; i < 4; i++ )
                    {
                        h->mb.cache.mvd[i_list][i8+i*8][0] =
                        h->mb.cache.mvd[i_list][i8+i*8][1] = 0;
                    }
                }
            }
        }

        /* load skip */
        if( h->param.b_cabac )
        {
            if( h->sh.i_type == SLICE_TYPE_B )
            {
                memset( h->mb.cache.skip, 0, X264_SCAN8_SIZE * sizeof( int8_t ) );
                if( i_left_xy >= 0 )
                {
                    h->mb.cache.skip[x264_scan8[0] - 1] = h->mb.skipbp[i_left_xy] & 0x2;
                    h->mb.cache.skip[x264_scan8[8] - 1] = h->mb.skipbp[i_left_xy] & 0x8;
                }
                if( i_top_xy >= 0 )
                {
                    h->mb.cache.skip[x264_scan8[0] - 8] = h->mb.skipbp[i_top_xy] & 0x4;
                    h->mb.cache.skip[x264_scan8[4] - 8] = h->mb.skipbp[i_top_xy] & 0x8;
                }
            }
            else if( h->mb.i_mb_xy == 0 && h->sh.i_type == SLICE_TYPE_P )
            {
                memset( h->mb.cache.skip, 0, X264_SCAN8_SIZE * sizeof( int8_t ) );
            }
        }
    }
}

