#ifndef _DEC_CABAC_H_
#define _DEC_CABAC_H_
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "common/common.h"
#include "macroblock.h"

#include "common/macroblock.h"  
#include "common/predict.h"   


int x264dec_mb_read_cabac( x264_t *h,bs_t *s );
#endif