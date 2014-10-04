#ifndef _GLOBAL_TBL_H
#define _GLOBAL_TBL_H 1

/****************************************************************************
 * tables
 ****************************************************************************/
//static const uint8_t block_idx_x[16] =
static const DECLARE_ALIGNED( uint8_t, block_idx_x[16], 16 )=
{
    0, 1, 0, 1, 2, 3, 2, 3, 0, 1, 0, 1, 2, 3, 2, 3
};

//static const uint8_t block_idx_y[16] =
static const DECLARE_ALIGNED( uint8_t, block_idx_y[16], 16 )=
{
    0, 0, 1, 1, 0, 0, 1, 1, 2, 2, 3, 3, 2, 2, 3, 3
};

//static const uint8_t block_idx_xy[4][4] =
static const DECLARE_ALIGNED( uint8_t, block_idx_xy[4][4], 16 )=
{
    { 0, 2, 8,  10},
    { 1, 3, 9,  11},
    { 4, 6, 12, 14},
    { 5, 7, 13, 15}
};

//static const int i_chroma_qp_table[52] =
static const DECLARE_ALIGNED( int, i_chroma_qp_table[52], 16 )=
{
     0,  1,  2,  3,  4,  5,  6,  7,  8,  9,
    10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
    20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
    29, 30, 31, 32, 32, 33, 34, 34, 35, 35,
    36, 36, 37, 37, 37, 38, 38, 38, 39, 39,
    39, 39
};

#endif