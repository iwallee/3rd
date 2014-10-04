/******************************************************
文件名:dec_cabac.c
功能:  解码码流中的cabac，读取出各种参数
author: li hengzhong 
date:   2005-5-6

******************************************************/

#include "dec_cabac.h"
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


static inline int array_non_zero_count( int *v, int i_count )
{
    int i;
    int i_nz;

    for( i = 0, i_nz = 0; i < i_count; i++ )
    {
        if( v[i] )
        {
            i_nz++;
        }
    }
    return i_nz;
}

/*******************************************************
函数功能：解码出macroblock是否可以skip
输入参数：x264结构体h

********************************************************/
int x264dec_cabac_mb_skip( x264_t *h )
{
	int b_skip;
    int ctx = 0;

    if( h->mb.i_mb_x > 0 && !IS_SKIP( h->mb.type[h->mb.i_mb_xy -1]) )
    {
        ctx++;
    }
    if( h->mb.i_mb_y > 0 && !IS_SKIP( h->mb.type[h->mb.i_mb_xy -h->mb.i_mb_stride]) )
    {
        ctx++;
    }

    if( h->sh.i_type == SLICE_TYPE_P )
        b_skip=x264_cabac_decode_decision( &h->cabac, 11 + ctx );
    else // SLICE_TYPE_B 
        b_skip=x264_cabac_decode_decision( &h->cabac, 24 + ctx );
	return b_skip;
}

/*******************************************************
函数功能：解码出macroblock的type
输入参数：x264结构体h

********************************************************/
static void x264_cabac_mb_type( x264_t *h )
{
    int act_sym, mode_sym, sym, sym1, sym2;
	int32_t mb_mode;

    if( h->sh.i_type == SLICE_TYPE_I )
    {
        int ctx = 0;
        if( h->mb.i_mb_x > 0 && h->mb.type[h->mb.i_mb_xy - 1] != I_4x4 )
        {
            ctx++;
        }
        if( h->mb.i_mb_y > 0 && h->mb.type[h->mb.i_mb_xy - h->mb.i_mb_stride] != I_4x4 )
        {
            ctx++;
        }

        act_sym = x264_cabac_decode_decision(&h->cabac, 3+ctx);
		if(act_sym == 0)
		{
			mb_mode = I_4x4;
		}
		else
		{
			mode_sym = x264_cabac_decode_terminal(&h->cabac);
			if(mode_sym == 0)	/*I_16x16*/
			{
				mb_mode = I_16x16;
				sym = x264_cabac_decode_decision(&h->cabac, 3+3);
				h->mb.i_cbp_luma=(sym==0)?0:15; //add by lihengzhong 2005-4-5
			
				sym = x264_cabac_decode_decision(&h->cabac, 3+4);
				if(sym == 0)
				{
					h->mb.i_cbp_chroma = 0;
				}
				else
				{
					sym = x264_cabac_decode_decision(&h->cabac, 3+5);
					h->mb.i_cbp_chroma = (sym==0)?1:2;
				}
				sym1 = x264_cabac_decode_decision(&h->cabac, 3+6);
				sym2 = x264_cabac_decode_decision(&h->cabac, 3+7);
				h->mb.i_intra16x16_pred_mode = (sym1<<1)|sym2;
			}
			else
			{
				//I_PCM

				// added by myw on 2011
				mb_mode = I_PCM;
				///////////

			}
		}
    }
	else if( h->sh.i_type == SLICE_TYPE_P )
	{
		/* prefix: 14, suffix: 17 */
		int ctx = 0;
		act_sym = x264_cabac_decode_decision(&h->cabac, 14);
		if(act_sym == 0)
		{
			//P_MODE
			static int mb_part_map[] = {D_16x16, D_8x8, D_8x16, D_16x8};
			mb_mode = P_L0;                                      //有些问题P8x8
			sym1 = x264_cabac_decode_decision(&h->cabac, 15);
			ctx = (sym1==0)?16:17;
			sym2 = x264_cabac_decode_decision(&h->cabac, ctx);
			sym = (sym1<<1)|sym2;
			h->mb.i_partition = mb_part_map[sym];

			if(h->mb.i_partition==D_8x8)  //add by lihengzhong 2005-4-9
			{
				mb_mode=P_8x8;
			}

		}
		else
		{
			mode_sym = x264_cabac_decode_decision(&h->cabac, 17);
			if(mode_sym == 0)
			{
				mb_mode = I_4x4;
			}
			else
			{
				sym = x264_cabac_decode_terminal(&h->cabac);
				if(sym == 0)
				{
					//I_16x16
					mb_mode = I_16x16;
					sym1 = x264_cabac_decode_decision(&h->cabac, 17+1);
					h->mb.i_cbp_luma =(sym1==0)?0:15; //add by lihengzhong 2005-4-6
					
					sym1 = x264_cabac_decode_decision(&h->cabac, 17+2);
					if(sym1 == 0)
					{
						h->mb.i_cbp_chroma = 0;
					}
					else
					{
						sym2 = x264_cabac_decode_decision(&h->cabac, 17+2);
						h->mb.i_cbp_chroma = (sym2==0)?1:2;
					}
					sym1 = x264_cabac_decode_decision(&h->cabac, 17+3);
					sym2 = x264_cabac_decode_decision(&h->cabac, 17+3);
					h->mb.i_intra16x16_pred_mode = (sym1<<1)|sym2;
				}
				else
				{
					//I_PCM

					// added by myw on 2011
					mb_mode = I_PCM;
					///////////

				}
			}
		}
	}
	else if( h->sh.i_type == SLICE_TYPE_B )
	{   //B frame
	}

	h->mb.i_type=mb_mode;
	h->mb.type[h->mb.i_mb_xy]=mb_mode;
}


static int x264_cabac_mb_intra4x4_pred_mode( x264_t *h, int i_pred)
{
	int i_mode, sym;
	sym = x264_cabac_decode_decision(&h->cabac, 68);
	if(sym == 1)
	{
		i_mode = i_pred;
	}
	else
	{
		i_mode = x264_cabac_decode_decision(&h->cabac, 69);
		sym = x264_cabac_decode_decision(&h->cabac, 69);
		i_mode |= (sym<<1);
		sym = x264_cabac_decode_decision(&h->cabac, 69);
		i_mode |= (sym<<2);
		if(i_mode >= i_pred)
			i_mode ++;
	}
	return i_mode;
}


static void x264_cabac_mb_intra8x8_pred_mode( x264_t *h )
{
    int i_mode, sym;
	int ctx = 0;

    /* No need to test for I4x4 or I_16x16 as cache_save handle that */
    if( h->mb.i_mb_x > 0 && h->mb.chroma_pred_mode[h->mb.i_mb_xy - 1] != 0 )
    {
        ctx++;
    }
    if( h->mb.i_mb_y > 0 && h->mb.chroma_pred_mode[h->mb.i_mb_xy - h->mb.i_mb_stride] != 0 )
    {
        ctx++;
    }
	sym = x264_cabac_decode_decision(&h->cabac, 64+ctx);
	if(sym == 0)
	{
		i_mode = 0;//Intra_8x8_DC;  //modified by lihengzhong 2005-4-9
	}
	else
	{
		sym = x264_cabac_decode_decision(&h->cabac, 64+3);
		if(sym == 0)
		{
			i_mode = 1;
		}
		else
		{
			sym = x264_cabac_decode_decision(&h->cabac, 64+3);
			i_mode = (sym==0)?2:3;
		}
	}

	h->mb.i_chroma_pred_mode= i_mode;
    h->mb.chroma_pred_mode[h->mb.i_mb_xy]=i_mode;
}


static inline  void x264_cabac_mb_ref( x264_t *h, int i_list, int idx,int width, int height, int i_ref_max )
{
	int i8    = x264_scan8[idx];
	
	int i_ref;
	if( i_ref_max <= 1 )
	{
		i_ref = 0;
	}
	else
	{  //more ref frames
	}

	//h->mb.cache.ref[i_list][i8]=i_ref;
	x264_macroblock_cache_ref(h,block_idx_x[idx],block_idx_y[idx],width,height,i_list,i_ref);
}


static inline  int  x264_cabac_mb_mvd_cpn( x264_t *h, int i_list, int idx, int l)
{
	int i_abs, i_prefix, i_suffix;
	const int amvd = abs( h->mb.cache.mvd[i_list][x264_scan8[idx] - 1][l] ) +
                     abs( h->mb.cache.mvd[i_list][x264_scan8[idx] - 8][l] );
	const int ctxbase = (l == 0 ? 40 : 47);
	
	int ctx;

	if( amvd < 3 )
		ctx = 0;
	else if( amvd > 32 )
		ctx = 2;
	else
		ctx = 1;

	i_prefix = 0;
	while(i_prefix<9 && x264_cabac_decode_decision(&h->cabac, ctxbase+ctx)!=0)
	{
		i_prefix ++;
		if(ctx < 3)
			ctx = 3;
		else if(ctx < 6)
			ctx ++;
	}
	if(i_prefix >= 9)
	{
		int k = 3;
		i_suffix = 0;
		while(x264_cabac_decode_bypass(&h->cabac) != 0)
		{
			i_suffix += 1<<k;
			k++;
		}
		while(k--)
		{
			i_suffix += x264_cabac_decode_bypass(&h->cabac)<<k;
		}
		i_abs = 9 + i_suffix; 
	}
	else
		i_abs = i_prefix;
	
	/* sign */
	if(i_abs != 0)
	{
		if(x264_cabac_decode_bypass(&h->cabac) != 0)
			i_abs = -i_abs;
	}
	return i_abs;
}


static inline void  x264_cabac_mb_mvd( x264_t *h, int i_list, int idx, int width, int height )
{
    int mvp[2];
    int mdx, mdy,mvx,mvy;

    /* Calculate mvd */
    x264_mb_predict_mv( h, i_list, idx, width, mvp );

    /* decode */
    mdx=x264_cabac_mb_mvd_cpn( h, i_list, idx, 0 );
    mdy=x264_cabac_mb_mvd_cpn( h, i_list, idx, 1 );

	//h->mb.cache.mv[i_list][x264_scan8[idx]][0]= mdx +mvp[0];
    //h->mb.cache.mv[i_list][x264_scan8[idx]][1]= mdy +mvp[1];
	mvx= mdx +mvp[0];
	mvy= mdy +mvp[1];

    /* save value */
	x264_macroblock_cache_mv(h,block_idx_x[idx],block_idx_y[idx],width,height,i_list,mvx,mvy);

    /* save value */
    x264_macroblock_cache_mvd( h, block_idx_x[idx], block_idx_y[idx], width, height, i_list, mdx, mdy );
}


static inline  int x264_cabac_mb_sub_p_partition( x264_t *h)
{
	int i_sub, sym;
	sym = x264_cabac_decode_decision(&h->cabac, 21);
	if(sym == 1)
	{
		i_sub = D_L0_8x8;
	}
	else
	{
		sym = x264_cabac_decode_decision(&h->cabac, 22);
		if(sym == 0)
		{
			i_sub = D_L0_8x4;
		}
		else
		{
			sym = x264_cabac_decode_decision(&h->cabac, 23);
			i_sub = (sym==0)?D_L0_4x4:D_L0_4x8;
		}
	}
	return i_sub;
}


static inline void x264_cabac_mb8x8_mvd( x264_t *h, int i_list )
{
    int i;
    for( i = 0; i < 4; i++ )
    {
/*        if( !x264_mb_partition_listX_table[i_list][ h->mb.i_sub_partition[i] ] )
        {
            continue;
        }
*/
        switch( h->mb.i_sub_partition[i] )
        {
            case D_L0_8x8:
            case D_L1_8x8:
            case D_BI_8x8:
                x264_cabac_mb_mvd( h, i_list, 4*i, 2, 2 );
                break;
            case D_L0_8x4:
            case D_L1_8x4:
            case D_BI_8x4:
                x264_cabac_mb_mvd( h, i_list, 4*i+0, 2, 1 );
                x264_cabac_mb_mvd( h, i_list, 4*i+2, 2, 1 );
                break;
            case D_L0_4x8:
            case D_L1_4x8:
            case D_BI_4x8:
                x264_cabac_mb_mvd( h, i_list, 4*i+0, 1, 2 );
                x264_cabac_mb_mvd( h, i_list, 4*i+1, 1, 2 );
                break;
            case D_L0_4x4:
            case D_L1_4x4:
            case D_BI_4x4:
                x264_cabac_mb_mvd( h, i_list, 4*i+0, 1, 1 );
                x264_cabac_mb_mvd( h, i_list, 4*i+1, 1, 1 );
                x264_cabac_mb_mvd( h, i_list, 4*i+2, 1, 1 );
                x264_cabac_mb_mvd( h, i_list, 4*i+3, 1, 1 );
                break;
        }
    }
}


static void x264_cabac_mb_cbp_luma( x264_t *h )
{
    /* TODO: clean up and optimize */
	int sym,cbp_y,cbp_ya, cbp_yb;
    int i8x8;
	cbp_y=0;
    for( i8x8 = 0; i8x8 < 4; i8x8++ )
    {
        int i_mba_xy = -1;
        int i_mbb_xy = -1;
        int x = block_idx_x[4*i8x8];
        int y = block_idx_y[4*i8x8];
        int ctx = 0;

        if( x > 0 )
		{
            i_mba_xy = h->mb.i_mb_xy;
			cbp_ya = cbp_y;
		}
        else if( h->mb.i_mb_x > 0 )
		{
            i_mba_xy = h->mb.i_mb_xy - 1;
			cbp_ya = h->mb.cbp[i_mba_xy]%16;
		}

        if( y > 0 )
		{
            i_mbb_xy = h->mb.i_mb_xy;
			cbp_yb = cbp_y;
		}
        else if( h->mb.i_mb_y > 0 )
		{
            i_mbb_xy = h->mb.i_mb_xy - h->mb.i_mb_stride;
			cbp_yb = h->mb.cbp[i_mbb_xy]%16;
		}


        /* No need to test for PCM and SKIP */
        if( i_mba_xy >= 0 )
        {
            const int i8x8a = block_idx_xy[(x-1)&0x03][y]/4;
            if( ((cbp_ya >> i8x8a)&0x01) == 0 )
            {
                ctx++;
            }
        }

        if( i_mbb_xy >= 0 )
        {
            const int i8x8b = block_idx_xy[x][(y-1)&0x03]/4;
            if( ((cbp_yb >> i8x8b)&0x01) == 0 )
            {
                ctx += 2;
            }
        }

        sym = x264_cabac_decode_decision(&h->cabac, 73+ctx);
		cbp_y |= (sym<<i8x8);
    }
	h->mb.i_cbp_luma = cbp_y;
}


static void x264_cabac_mb_cbp_chroma( x264_t *h )
{
    int cbp_a = -1;
    int cbp_b = -1;
    int ctx,sym;

    /* No need to test for SKIP/PCM */
    if( h->mb.i_mb_x > 0 )
    {
        cbp_a = (h->mb.cbp[h->mb.i_mb_xy - 1] >> 4)&0x3;
    }

    if( h->mb.i_mb_y > 0 )
    {
        cbp_b = (h->mb.cbp[h->mb.i_mb_xy - h->mb.i_mb_stride] >> 4)&0x3;
    }

    ctx = 0;
    if( cbp_a > 0 ) ctx++;
    if( cbp_b > 0 ) ctx += 2;
    sym = x264_cabac_decode_decision(&h->cabac, 77+ctx);
	if(sym == 0)
	{
		h->mb.i_cbp_chroma = 0;
	}
	else
	{
		ctx = 4;
		if( cbp_a == 2 ) ctx++;
		if( cbp_b == 2 ) ctx += 2;
		sym = x264_cabac_decode_decision(&h->cabac, 77+ctx);
		h->mb.i_cbp_chroma = (sym==0)?1:2;
	}
}


/* TODO check it with != qp per mb */
static void x264_cabac_mb_qp_delta( x264_t *h )
{
    int i_mbn_xy = h->mb.i_mb_xy - 1;
    int i_dqp = h->mb.i_last_dqp ; //有问题,待查 lihengzhong
    int val = i_dqp <= 0 ? (-2*i_dqp) : (2*i_dqp - 1);
    int ctx;

    /* No need to test for PCM / SKIP */
    if( i_mbn_xy >= 0 && h->mb.i_last_dqp != 0 &&
        ( h->mb.type[i_mbn_xy] == I_16x16 || (h->mb.cbp[i_mbn_xy]&0x3f) ) )
        ctx = 1;
    else
        ctx = 0;

    val = 0;
	while(x264_cabac_decode_decision(&h->cabac, 60+ctx) != 0)
	{
		val ++;
		if(ctx < 2)
			ctx = 2;
		else
			ctx = 3;
	}

	h->mb.i_last_dqp = ((val&0x01)==0)?(-(val>>1)):((val+1)>>1);
	h->mb.qp[h->mb.i_mb_xy] = (h->mb.i_last_dqp + h->mb.i_last_qp+52)%52;
//	h->mb.qp[h->mb.i_mb_xy] = h->mb.i_last_dqp + h->mb.i_last_qp;
//	h->mb.i_last_qp=h->mb.qp[h->mb.i_mb_xy];
}

static int x264_cabac_mb_cbf_ctxidxinc( x264_t *h, int i_cat, int i_idx )
{
    /* TODO: clean up/optimize */
    int i_mba_xy = -1;
    int i_mbb_xy = -1;
    int i_nza = -1;
    int i_nzb = -1;
    int ctx = 0;
	int cbp_ya, cbp_yb;

    if( i_cat == 0 )
    {
        if( h->mb.i_mb_x > 0 )
        {
            i_mba_xy = h->mb.i_mb_xy -1;
            if( h->mb.type[i_mba_xy] == I_16x16 )
            {
                i_nza = h->mb.cbp[i_mba_xy]&0x100;
            }
        }
        if( h->mb.i_mb_y > 0 )
        {
            i_mbb_xy = h->mb.i_mb_xy - h->mb.i_mb_stride;
            if( h->mb.type[i_mbb_xy] == I_16x16 )
            {
                i_nzb = h->mb.cbp[i_mbb_xy]&0x100;
            }
        }
    }
    else if( i_cat == 1 || i_cat == 2 )
    {
        int x = block_idx_x[i_idx];
        int y = block_idx_y[i_idx];

        if( x > 0 )
		{
            i_mba_xy = h->mb.i_mb_xy;
			cbp_ya = h->mb.i_cbp_luma;
		}
        else if( h->mb.i_mb_x > 0 )
		{
            i_mba_xy = h->mb.i_mb_xy -1;
			cbp_ya = h->mb.cbp[i_mba_xy]%16;
		}

        if( y > 0 )
		{
            i_mbb_xy = h->mb.i_mb_xy;
			cbp_yb = h->mb.i_cbp_luma;
		}
        else if( h->mb.i_mb_y > 0 )
		{
            i_mbb_xy = h->mb.i_mb_xy - h->mb.i_mb_stride;
			cbp_yb = h->mb.cbp[i_mbb_xy]%16;
		}

        /* no need to test for skip/pcm */
        if( i_mba_xy >= 0 )
        {
            const int i8x8a = block_idx_xy[(x-1)&0x03][y]/4;
            if( (cbp_ya&0x0f)>> i8x8a )
            {
                i_nza = h->mb.cache.non_zero_count[x264_scan8[i_idx] - 1];
            }
        }
        if( i_mbb_xy >= 0 )
        {
            const int i8x8b = block_idx_xy[x][(y-1)&0x03]/4;
            if( (cbp_yb&0x0f)>> i8x8b )
            {
                i_nzb = h->mb.cache.non_zero_count[x264_scan8[i_idx] - 8];
            }
        }
    }
    else if( i_cat == 3 )
    {
        /* no need to test skip/pcm */
        if( h->mb.i_mb_x > 0 )
        {
            i_mba_xy = h->mb.i_mb_xy -1;
            if( h->mb.cbp[i_mba_xy]&0x30 )
            {
                i_nza = h->mb.cbp[i_mba_xy]&( 0x02 << ( 8 + i_idx) );
            }
        }
        if( h->mb.i_mb_y > 0 )
        {
            i_mbb_xy = h->mb.i_mb_xy - h->mb.i_mb_stride;
            if( h->mb.cbp[i_mbb_xy]&0x30 )
            {
                i_nzb = h->mb.cbp[i_mbb_xy]&( 0x02 << ( 8 + i_idx) );
            }
        }
    }
    else if( i_cat == 4 )
    {
		int cbp_ca, cbp_cb;
        int idxc = i_idx% 4;

        if( idxc == 1 || idxc == 3 )
		{
            i_mba_xy = h->mb.i_mb_xy;
			cbp_ca = h->mb.i_cbp_chroma;
		}
        else if( h->mb.i_mb_x > 0 )
		{
            i_mba_xy = h->mb.i_mb_xy - 1;
			cbp_ca = h->mb.cbp[i_mba_xy]/16;
		}

        if( idxc == 2 || idxc == 3 )
		{
            i_mbb_xy = h->mb.i_mb_xy;
			cbp_cb = h->mb.i_cbp_chroma;
		}
        else if( h->mb.i_mb_y > 0 )
		{
            i_mbb_xy = h->mb.i_mb_xy - h->mb.i_mb_stride;
			cbp_cb = h->mb.cbp[i_mbb_xy]/16;
		}

        /* no need to test skip/pcm */
        if( i_mba_xy >= 0 && (cbp_ca&0x03) == 0x02 )
        {
            i_nza = h->mb.cache.non_zero_count[x264_scan8[16+i_idx] - 1];
        }
        if( i_mbb_xy >= 0 && (cbp_cb&0x03) == 0x02 )
        {
            i_nzb = h->mb.cache.non_zero_count[x264_scan8[16+i_idx] - 8];
        }
    }

    if( ( i_mba_xy < 0  && IS_INTRA( h->mb.i_type ) ) || i_nza > 0 )
    {
        ctx++;
    }
    if( ( i_mbb_xy < 0  && IS_INTRA( h->mb.i_type ) ) || i_nzb > 0 )
    {
        ctx += 2;
    }

    return 4 * i_cat + ctx;
}

/*****************************************************************
函数功能：解码residual data
输入参数：h: x264结构体
          i_ctxBlockCat:0-> DC 16x16  i_idx = 0
                        1-> AC 16x16  i_idx = luma4x4idx
                        2-> Luma4x4   i_idx = luma4x4idx
                        3-> DC Chroma i_idx = iCbCr
                        4-> AC Chroma i_idx = 4 * iCbCr + chroma4x4idx

*****************************************************************/
static void block_residual_read_cabac( x264_t *h, int i_ctxBlockCat, int i_idx, int *l, int i_count )
{
    static const int significant_coeff_flag_offset[5] = { 0, 15, 29, 44, 47 };
    static const int last_significant_coeff_flag_offset[5] = { 0, 15, 29, 44, 47 };
    static const int coeff_abs_level_m1_offset[5] = { 0, 10, 20, 30, 39 };

 //   int i_coeff_abs_m1[16];
    int i_coeff_sig_map[16];
    int i_coeff = 0;
    int i_last  = 0;

    int i_abslevel1 = 0;
    int i_abslevelgt1 = 0;


	int i, i1, sym, i_abs, x, y;

    /* i_ctxBlockCat: 0-> DC 16x16  i_idx = 0
     *                1-> AC 16x16  i_idx = luma4x4idx
     *                2-> Luma4x4   i_idx = luma4x4idx
     *                3-> DC Chroma i_idx = iCbCr
     *                4-> AC Chroma i_idx = 4 * iCbCr + chroma4x4idx
     */

	//memset(l, 0, sizeof(int16_t)*i_count);
	memset(l, 0, sizeof(int)*i_count);  
	sym = x264_cabac_decode_decision(&h->cabac, 85 + x264_cabac_mb_cbf_ctxidxinc( h, i_ctxBlockCat, i_idx ));
	if(sym == 0)
	{
		//the block is not coded
		return;
	}


    for(i=0; i<i_count-1; i++)
	{
		int i_ctxIdxInc;
		i_ctxIdxInc = X264_MIN(i, i_count-2);
		sym = x264_cabac_decode_decision(&h->cabac, 105+significant_coeff_flag_offset[i_ctxBlockCat] + i_ctxIdxInc);
		if(sym != 0)
		{
			i_coeff_sig_map[i] = 1;
			i_coeff ++;
			//--- read last coefficient symbol ---
			if(x264_cabac_decode_decision(&h->cabac, 166 + last_significant_coeff_flag_offset[i_ctxBlockCat] + i_ctxIdxInc))
			{
				for(i++; i<i_count; i++)
					i_coeff_sig_map[i] = 0;
			}
		}
		else
		{
			i_coeff_sig_map[i] = 0;
		}
	}

	//--- last coefficient must be significant if no last symbol was received ---
	if (i<i_count)
	{
		i_coeff_sig_map[i] = 1;
		i_coeff ++;
	}
	i1 = i_count - 1;
	for( i = i_coeff - 1; i >= 0; i-- )
	{
		int i_prefix;
		int i_ctxIdxInc;

		i_ctxIdxInc = (i_abslevelgt1 != 0 ? 0 : X264_MIN( 4, i_abslevel1 + 1 )) + coeff_abs_level_m1_offset[i_ctxBlockCat];
		sym = x264_cabac_decode_decision(&h->cabac, 227+i_ctxIdxInc);
		if(sym == 0)
		{
			i_abs = 0;
		}
		else
		{
			i_prefix = 1;
			i_ctxIdxInc = 5 + X264_MIN( 4, i_abslevelgt1 ) + coeff_abs_level_m1_offset[i_ctxBlockCat];
			while(i_prefix<14 && x264_cabac_decode_decision(&h->cabac, 227+i_ctxIdxInc)!=0)
			{
				i_prefix ++;
			}
			/* suffix */
			if(i_prefix >= 14)
			{
				int k = 0;
				int i_suffix = 0;
				while(x264_cabac_decode_bypass(&h->cabac) != 0)
				{
					i_suffix += 1<<k;
					k++;
				}
				while(k--)
				{
					i_suffix += x264_cabac_decode_bypass(&h->cabac)<<k;
				}
				i_abs = i_prefix + i_suffix;
			}
			else
			{
				i_abs = i_prefix;
			}
		}
		/* read the sign */
		sym = x264_cabac_decode_bypass(&h->cabac);
		while(i_coeff_sig_map[i1]==0)
		{
			i1 --;
		}
		l[i1] = (sym==0)?(i_abs+1):(-(i_abs+1));
		i1 --;
		if(i_abs == 0)
		{
			i_abslevel1 ++;
		}
		else
		{
			i_abslevelgt1 ++;
		}
	}
	
	if (i_ctxBlockCat==1 || i_ctxBlockCat==2)
	{
		x = block_idx_x[i_idx];
		y = block_idx_y[i_idx];
		h->mb.cache.non_zero_count[x264_scan8[i_idx]] = i_coeff;  //可能有问题
//		t->mb.nnz_ref[NNZ_LUMA + y * 8 + x] = i_coeff;
	}
	else if (i_ctxBlockCat==4 && i_idx<4)
	{
		int idx = i_idx + 16;
		h->mb.cache.non_zero_count[x264_scan8[16+i_idx]] = i_coeff;
//		x = (idx - 16) % 2;
//		y = (idx - 16) / 2;
//		t->mb.nnz_ref[NNZ_CHROMA0 + y * 8 + x] = i_coeff;
	}
	else if (i_ctxBlockCat==4 && i_idx<8)
	{
		int idx = i_idx + 16;
		h->mb.cache.non_zero_count[x264_scan8[16+i_idx]] = i_coeff;
//		x = (idx - 20) % 2;
//		y = (idx - 20) / 2;
//		t->mb.nnz_ref[NNZ_CHROMA1 + y * 8 + x] = i_coeff;
	}
}


/**************************************************************
函数功能：解码一个macroblock
输入参数：h：x264结构体
          s：cabac码流结构体

**************************************************************/
void x264_macroblock_read_cabac( x264_t *h, bs_t *s ) 
{
	int i_mb_type;
	const int i_mb_pos_start = bs_pos( s );
	int       i_mb_pos_tex;

	//   int i_list;
	int i,cbp_dc;

	/* Write the MB type */
	x264_cabac_mb_type( h );
	i_mb_type = h->mb.i_type;

	if( IS_INTRA( i_mb_type ) )
	{
		/* Prediction */
		if( i_mb_type == I_4x4 )
		{
			for( i = 0; i < 16; i++ )
			{
				const int i_pred = x264_mb_predict_intra4x4_mode( h, i );
				const int i_mode = x264_cabac_mb_intra4x4_pred_mode( h, i_pred );
				h->mb.cache.intra4x4_pred_mode[x264_scan8[i]] = i_mode;
			}
		}
		x264_cabac_mb_intra8x8_pred_mode( h );
		/* save ref */
		memset(h->mb.i_sub_partition, -1, sizeof(h->mb.i_sub_partition));
		h->mb.i_partition = -1;
	}
	else if( i_mb_type == P_L0 )
	{
		int i_ref_max = h->frames.i_max_ref0;
		if( h->mb.i_partition == D_16x16 )
		{

			x264_cabac_mb_ref( h, 0, 0 ,4, 4,i_ref_max);
			x264_cabac_mb_mvd( h, 0, 0, 4, 4 );
		}
		else if( h->mb.i_partition == D_16x8 )
		{
			x264_cabac_mb_ref( h, 0, 0,4, 2, i_ref_max );
			x264_cabac_mb_ref( h, 0, 8,4, 2, i_ref_max);

			x264_cabac_mb_mvd( h, 0, 0, 4, 2 );
			x264_cabac_mb_mvd( h, 0, 8, 4, 2 );
		}
		else if( h->mb.i_partition == D_8x16 )
		{
			x264_cabac_mb_ref( h, 0, 0,2, 4,i_ref_max );
			x264_cabac_mb_ref( h, 0, 4,2, 4,i_ref_max );

			x264_cabac_mb_mvd( h, 0, 0, 2, 4 );
			x264_cabac_mb_mvd( h, 0, 4, 2, 4 );
		}
	}
	else if( i_mb_type == P_8x8 )
	{
		int i_ref_max = h->frames.i_max_ref0;
		// sub mb type 
		h->mb.i_sub_partition[0]=x264_cabac_mb_sub_p_partition( h );
		h->mb.i_sub_partition[1]=x264_cabac_mb_sub_p_partition( h );
		h->mb.i_sub_partition[2]=x264_cabac_mb_sub_p_partition( h );
		h->mb.i_sub_partition[3]=x264_cabac_mb_sub_p_partition( h );

		// ref 0            
		x264_cabac_mb_ref( h, 0, 0 ,2, 2,i_ref_max);
		x264_cabac_mb_ref( h, 0, 4 ,2, 2,i_ref_max);
		x264_cabac_mb_ref( h, 0, 8 ,2, 2,i_ref_max);
		x264_cabac_mb_ref( h, 0, 12 ,2, 2,i_ref_max);

		x264_cabac_mb8x8_mvd( h, 0 );
	}
	/*

	else if( i_mb_type == B_8x8 )
	{
	/ * sub mb type * /
	x264_cabac_mb_sub_b_partition( h, h->mb.i_sub_partition[0] );
	x264_cabac_mb_sub_b_partition( h, h->mb.i_sub_partition[1] );
	x264_cabac_mb_sub_b_partition( h, h->mb.i_sub_partition[2] );
	x264_cabac_mb_sub_b_partition( h, h->mb.i_sub_partition[3] );

	/ * ref * /
	for( i_list = 0; i_list < 2; i_list++ )
	{
	if( ( i_list ? h->sh.i_num_ref_idx_l1_active : h->sh.i_num_ref_idx_l0_active ) == 1 )
	continue;
	for( i = 0; i < 4; i++ )
	{
	if( x264_mb_partition_listX_table[i_list][ h->mb.i_sub_partition[i] ] )
	{
	x264_cabac_mb_ref( h, i_list, 4*i );
	}
	}
	}

	x264_cabac_mb8x8_mvd( h, 0 );
	x264_cabac_mb8x8_mvd( h, 1 );
	}
	else if( i_mb_type != B_DIRECT )
	{
	/ * All B mode * /
	int b_list[2][2];

	/ * init ref list utilisations * /
	for( i = 0; i < 2; i++ )
	{
	b_list[0][i] = x264_mb_type_list0_table[i_mb_type][i];
	b_list[1][i] = x264_mb_type_list1_table[i_mb_type][i];
	}

	for( i_list = 0; i_list < 2; i_list++ )
	{
	const int i_ref_max = i_list == 0 ? h->sh.i_num_ref_idx_l0_active : h->sh.i_num_ref_idx_l1_active;

	if( i_ref_max > 1 )
	{
	if( h->mb.i_partition == D_16x16 )
	{
	if( b_list[i_list][0] ) x264_cabac_mb_ref( h, i_list, 0 );
	}
	else if( h->mb.i_partition == D_16x8 )
	{
	if( b_list[i_list][0] ) x264_cabac_mb_ref( h, i_list, 0 );
	if( b_list[i_list][1] ) x264_cabac_mb_ref( h, i_list, 8 );
	}
	else if( h->mb.i_partition == D_8x16 )
	{
	if( b_list[i_list][0] ) x264_cabac_mb_ref( h, i_list, 0 );
	if( b_list[i_list][1] ) x264_cabac_mb_ref( h, i_list, 4 );
	}
	}
	}
	for( i_list = 0; i_list < 2; i_list++ )
	{
	if( h->mb.i_partition == D_16x16 )
	{
	if( b_list[i_list][0] ) x264_cabac_mb_mvd( h, i_list, 0, 4, 4 );
	}
	else if( h->mb.i_partition == D_16x8 )
	{
	if( b_list[i_list][0] ) x264_cabac_mb_mvd( h, i_list, 0, 4, 2 );
	if( b_list[i_list][1] ) x264_cabac_mb_mvd( h, i_list, 8, 4, 2 );
	}
	else if( h->mb.i_partition == D_8x16 )
	{
	if( b_list[i_list][0] ) x264_cabac_mb_mvd( h, i_list, 0, 2, 4 );
	if( b_list[i_list][1] ) x264_cabac_mb_mvd( h, i_list, 4, 2, 4 );
	}
	}
	}*/

	i_mb_pos_tex = bs_pos( s );
	h->stat.frame.i_hdr_bits += i_mb_pos_tex - i_mb_pos_start;

	if( i_mb_type != I_16x16 )
	{
		x264_cabac_mb_cbp_luma( h );
		x264_cabac_mb_cbp_chroma( h );
	}

	cbp_dc = 0;
	if( h->mb.i_cbp_luma > 0 || h->mb.i_cbp_chroma > 0 || i_mb_type == I_16x16 )
	{
		x264_cabac_mb_qp_delta( h );

		/* read residual */
		if( i_mb_type == I_16x16 )
		{
			/* DC Luma */
			int dc_nz;
			block_residual_read_cabac( h, 0, 0, h->dct.luma16x16_dc, 16 );
			//for CABAC, record the DC non_zero
			dc_nz = array_non_zero_count(&(h->dct.luma16x16_dc[0]), 16);
			if(dc_nz != 0)
			{
				cbp_dc = 1;
			}

			if( h->mb.i_cbp_luma != 0 )
			{
				/* AC Luma */
				for( i = 0; i < 16; i++ )
				{
					if( h->mb.i_cbp_luma & ( 1 << ( i / 4 ) ) )  //add by hengzhong li 2005-4-5
					{
						block_residual_read_cabac( h, 1, i, h->dct.block[i].residual_ac, 15 );
						//						t->mb.dct_y_z[i][0] = t->mb.dc4x4_z[i];
					}
				}
			}
		}
		else
		{
			for( i = 0; i < 16; i++ )
			{
				if( h->mb.i_cbp_luma & ( 1 << ( i / 4 ) ) )
				{
					block_residual_read_cabac( h, 2, i, h->dct.block[i].luma4x4, 16 );
				}
			}
		}

		if( h->mb.i_cbp_chroma &0x03 )    /* Chroma DC residual present */
		{
			int dc_nz0, dc_nz1;
			block_residual_read_cabac( h, 3, 0, h->dct.chroma_dc[0], 4 );
			block_residual_read_cabac( h, 3, 1, h->dct.chroma_dc[1], 4 );
			//for CABAC, chroma dc pattern
			dc_nz0 = array_non_zero_count(h->dct.chroma_dc[0], 4) > 0;
			dc_nz1 = array_non_zero_count(h->dct.chroma_dc[1], 4) > 0;
			if(dc_nz0)
				cbp_dc |= 0x02;
			if(dc_nz1)
				cbp_dc |= 0x04;
		}

		if( h->mb.i_cbp_chroma&0x02 ) /* Chroma AC residual present */
		{
			for( i = 0; i < 8; i++ )
			{
				block_residual_read_cabac( h, 4, i, h->dct.block[16+i].residual_ac, 15 );
				//				h->dct.dct_uv_z[i>>2][i&0x03][0] = t->mb.dc2x2_z[i>>2][i&0x03];

			}
		}
		else
		{
			/*			for(i = 0 ; i < 4 ; i ++)
			{
			t->mb.dct_uv_z[0][i][0] = t->mb.dc2x2_z[0][i];
			t->mb.dct_uv_z[1][i][0] = t->mb.dc2x2_z[1][i];
			}
			*/ 		
		}
	}
	else
	{
		h->mb.qp[h->mb.i_mb_xy] = h->mb.i_last_qp;  //add 2005-4-14 qp
		h->mb.i_last_dqp=0;
	}

	//for CABAC, cbp
	h->mb.cbp[h->mb.i_mb_xy] = h->mb.i_cbp_luma | (h->mb.i_cbp_chroma <<4) | (cbp_dc << 8);

	if( IS_INTRA( i_mb_type ) )
		h->stat.frame.i_itex_bits += bs_pos(s) - i_mb_pos_tex;
	else
		h->stat.frame.i_ptex_bits += bs_pos(s) - i_mb_pos_tex;
}   

int x264dec_mb_read_cabac(x264_t *h,bs_t *s)
{
	int skip;
	//for dec CABAC, set MVD to zero
//	memset(&(t->mb.mvd[0][0][0]), 0, sizeof(t->mb.mvd));
	h->mb.i_cbp_luma = h->mb.i_cbp_chroma  = 0;
	h->mb.cbp[h->mb.i_mb_xy] =0;
	
	if (h->sh.i_type != SLICE_TYPE_I)
		skip = x264dec_cabac_mb_skip(h);
	else
		skip = 0;
	if(skip)
	{
		x264_macroblock_decode_skip( h );
		/* skip mb block */
/*		if (t->slice_type == SLICE_P)
		{
			x264_predict_mv_skip(t, 0, &t->mb.vec[0][0]);
			copy_nvec(&t->mb.vec[0][0], &t->mb.vec[0][0], 4, 4, 4);
			t->mb.mb_mode = P_MODE;     // decode as MB_16x16 
			t->mb.mb_part = MB_16x16;
		}
		else if(t->slice_type == SLICE_B)
		{
			mb_get_directMB16x16_mv_cabac(t);
            t->mb.is_copy = 1;
			t->mb.mb_mode = B_MODE;     // decode as MB_16x16                 
			t->mb.mb_part = MB_16x16;
		}
		else
		{
			assert(0);
		}				
*/	}
	else
	{
		x264_macroblock_read_cabac(h, s);					
	}
	return skip;
}