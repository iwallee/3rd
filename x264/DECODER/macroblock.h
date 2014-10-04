
#ifndef _DECODER_MACROBLOCK_H
#define _DECODER_MACROBLOCK_H 1

//int  x264_macroblock_read_cabac( x264_t *h, bs_t *s, x264_macroblock_t *mb );
int  x264_macroblock_read_cavlc(x264_t *h, bs_t *s, x264_macroblock_t *mb);

int  x264_macroblock_decode(x264_t *h, x264_macroblock_t *mb);
void x264_macroblock_decode_skip(x264_t *h);

void x264_macroblock_decode_cache_load(x264_t *h, int i_mb_x, int i_mb_y);
void x264_mb_dec_mc(x264_t *h);

void x264_macroblock_decode_lostcache_load(x264_t *h, int i_mb_x, int i_mb_y);
void x264_macroblock_decode_lostskip(x264_t *h);

#endif

