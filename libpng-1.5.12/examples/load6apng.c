/*
 * load6apng.c
 *
 * loads APNG file, saves all frames as PNG (32bpp).
 * including frames composition.
 *
 * needs regular, unpatched libpng.
 *
 * Copyright (c) 2012 Max Stepin
 * maxst at users.sourceforge.net
 *
 * zlib license
 * ------------
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "png.h"
#include "zlib.h"

typedef struct 
{
  unsigned int w, h, d, t, ch, it, w0, h0, x0, y0, frames, plays, cur;
  png_color pl[256]; int ps; png_byte tr[256]; int ts; png_color_16 tc;
  unsigned short delay_num, delay_den; unsigned char dop, bop;
  z_stream zstream;
  unsigned char * buf, * image, * frame, * restore; 
  unsigned int buf_size, size;
  png_bytepp rows_image;
  png_bytepp rows_frame;
} image_info;

void SavePNG(image_info * pimg)
{
  FILE * f1;
  char   szOut[512];
  
  sprintf(szOut, "test_load6_%03d.png", pimg->cur);
  if ((f1 = fopen(szOut, "wb")) != 0)
  {
    png_structp  png_ptr  = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop    info_ptr = png_create_info_struct(png_ptr);
    if (png_ptr != NULL && info_ptr != NULL && setjmp(png_jmpbuf(png_ptr)) == 0)
    {
      png_init_io(png_ptr, f1);
      png_set_compression_level(png_ptr, 9);
      if (pimg->image && pimg->rows_image)
      {
        png_set_IHDR(png_ptr, info_ptr, pimg->w, pimg->h, 8, 6, 0, 0, 0);
        png_write_info(png_ptr, info_ptr);
        png_write_image(png_ptr, pimg->rows_image);
        png_write_end(png_ptr, info_ptr);
      }
    }
    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(f1);
    printf("OUT: %s : %dx%d\n", szOut, pimg->w, pimg->h);
  }
  pimg->cur++;
}

/* handle acTL, fcTL, fdAT chunks */
static int handle_apng_chunks(png_struct *png_ptr, png_unknown_chunkp chunk)
{
  static int    mask4[2]  = {240,15};
  static int    shift4[2] = {4,0};
  static int    mask2[4]  = {192,48,12,3};
  static int    shift2[4] = {6,4,2,0};
  static int    mask1[8]  = {128,64,32,16,8,4,2,1};
  static int    shift1[8] = {7,6,5,4,3,2,1,0};
  /*unsigned int  seq_num;*/
  image_info  * pimg;

  if (memcmp(chunk->name, "acTL", 4) == 0)
  {
    if (chunk->size != 8)
      return (-1);

    pimg = (image_info *)png_get_user_chunk_ptr(png_ptr);
    pimg->frames = png_get_uint_31(png_ptr, chunk->data);
    pimg->plays  = png_get_uint_31(png_ptr, chunk->data + 4);

    return (1);
  }
  else
  if (memcmp(chunk->name, "fcTL", 4) == 0)
  {
    if (chunk->size != 26)
      return (-1);

    /*seq_num  = png_get_uint_31(png_ptr, chunk->data);*/
    pimg = (image_info *)png_get_user_chunk_ptr(png_ptr);
    pimg->w0 = png_get_uint_31(png_ptr, chunk->data + 4);
    pimg->h0 = png_get_uint_31(png_ptr, chunk->data + 8);
    pimg->x0 = png_get_uint_31(png_ptr, chunk->data + 12);
    pimg->y0 = png_get_uint_31(png_ptr, chunk->data + 16);
    pimg->delay_num = png_get_uint_16(chunk->data + 20);
    pimg->delay_den = png_get_uint_16(chunk->data + 22);
    pimg->dop = chunk->data[24];
    pimg->bop = chunk->data[25];
    pimg->zstream.next_out  = pimg->buf;
    pimg->zstream.avail_out = pimg->buf_size;

    return (1);
  }
  else
  if (memcmp(chunk->name, "fdAT", 4) == 0)
  {
    int ret;

    if (chunk->size < 4)
      return (-1);

    /*seq_num = png_get_uint_31(png_ptr, chunk->data);*/
    pimg = (image_info *)png_get_user_chunk_ptr(png_ptr);

    /* interlaced apng - not supported yet */
    if (pimg->it > 0)
      return (0);

    pimg->zstream.next_in   = chunk->data+4;
    pimg->zstream.avail_in  = (uInt)(chunk->size-4);
    ret = inflate(&pimg->zstream, Z_SYNC_FLUSH);

    if (ret == Z_STREAM_END)
    {
      unsigned int    i, j, n;
      unsigned char * sp, * dp;
      unsigned char * prev_row = NULL;
      unsigned char * row = pimg->buf;
      unsigned int    pixeldepth = pimg->d*pimg->ch;
      unsigned int    bpp = (pixeldepth + 7) >> 3;
      unsigned int    rowbytes = (pixeldepth >= 8) ? pimg->w0 * (pixeldepth >> 3) : (pimg->w0 * pixeldepth + 7) >> 3;

      for (j=0; j<pimg->h0; j++)
      {
        /* filters */
        switch (*row++) 
        {
          case 0: break;
          case 1: for (i=bpp; i<rowbytes; i++) row[i] += row[i-bpp]; break;
          case 2: if (prev_row) for (i=0; i<rowbytes; i++) row[i] += prev_row[i]; break;
          case 3: 
            if (prev_row)
            {
              for (i=0; i<bpp; i++) row[i] += prev_row[i]>>1;
              for (i=bpp; i<rowbytes; i++) row[i] += (prev_row[i] + row[i-bpp])>>1;
            } 
            else 
              for (i=bpp; i<rowbytes; i++) row[i] += row[i-bpp]>>1;
            break;
          case 4: 
            if (prev_row) 
            {
              int a, b, c, pa, pb, pc, p;
              for (i=0; i<bpp; i++) row[i] += prev_row[i];
              for (i=bpp; i<rowbytes; i++)
              {
                a = row[i-bpp];
                b = prev_row[i];
                c = prev_row[i-bpp];
                p = b - c;
                pc = a - c;
                pa = abs(p);
                pb = abs(pc);
                pc = abs(p + pc);
                row[i] += ((pa <= pb && pa <= pc) ? a : (pb <= pc) ? b : c);
              }
            } 
            else 
              for (i=bpp; i<rowbytes; i++) row[i] += row[i-bpp];
            break;
        }

        /* transform to RGBA 32bpp */
        sp = row;
        dp = pimg->rows_frame[j];
        switch (pimg->t)
        {
          case 0:
            switch (pimg->d)
            {
              case 16: for (i=0; i<pimg->w0; i++) { *dp++ = *sp; *dp++ = *sp; *dp++ = *sp; *dp++ = (pimg->ts && png_get_uint_16(sp)==pimg->tc.gray) ? 0 : 255; sp+=2; }  break;
              case 8:  for (i=0; i<pimg->w0; i++) { *dp++ = *sp; *dp++ = *sp; *dp++ = *sp; *dp++ = (pimg->ts && *sp==pimg->tc.gray) ? 0 : 255; sp++;  }  break;
              case 4:  pimg->tc.gray *= 0x11; for (i=0; i<pimg->w0; i++) { n = ((sp[i>>1] & mask4[i&1]) >> shift4[i&1])*0x11; *dp++ = n; *dp++ = n; *dp++ = n; *dp++ = (pimg->ts && n==pimg->tc.gray) ? 0 : 255; } break;
              case 2:  pimg->tc.gray *= 0x55; for (i=0; i<pimg->w0; i++) { n = ((sp[i>>2] & mask2[i&3]) >> shift2[i&3])*0x55; *dp++ = n; *dp++ = n; *dp++ = n; *dp++ = (pimg->ts && n==pimg->tc.gray) ? 0 : 255; } break;
              case 1:  pimg->tc.gray *= 0xFF; for (i=0; i<pimg->w0; i++) { n = ((sp[i>>3] & mask1[i&7]) >> shift1[i&7])*0xFF; *dp++ = n; *dp++ = n; *dp++ = n; *dp++ = (pimg->ts && n==pimg->tc.gray) ? 0 : 255; } break;
            }
            break;
          case 2:
            switch (pimg->d)
            {
              case 8:  for (i=0; i<pimg->w0; i++, sp+=3) { *dp++ = sp[0]; *dp++ = sp[1]; *dp++ = sp[2]; *dp++ = (pimg->ts && sp[0]==pimg->tc.red && sp[1]==pimg->tc.green && sp[2]==pimg->tc.blue) ? 0 : 255; } break;
              case 16: for (i=0; i<pimg->w0; i++, sp+=6) { *dp++ = sp[0]; *dp++ = sp[2]; *dp++ = sp[4]; *dp++ = (pimg->ts && png_get_uint_16(sp)==pimg->tc.red && png_get_uint_16(sp+2)==pimg->tc.green && png_get_uint_16(sp+4)==pimg->tc.blue) ? 0 : 255; } break;
            }
            break;
          case 3: 
            switch (pimg->d)
            {
              case 8:  for (i=0; i<pimg->w0; i++) { n = sp[i]; *dp++ = pimg->pl[n].red; *dp++ = pimg->pl[n].green; *dp++ = pimg->pl[n].blue; *dp++ = pimg->tr[n]; } break;
              case 4:  for (i=0; i<pimg->w0; i++) { n = (sp[i>>1] & mask4[i&1]) >> shift4[i&1]; *dp++ = pimg->pl[n].red; *dp++ = pimg->pl[n].green; *dp++ = pimg->pl[n].blue; *dp++ = pimg->tr[n]; } break;
              case 2:  for (i=0; i<pimg->w0; i++) { n = (sp[i>>2] & mask2[i&3]) >> shift2[i&3]; *dp++ = pimg->pl[n].red; *dp++ = pimg->pl[n].green; *dp++ = pimg->pl[n].blue; *dp++ = pimg->tr[n]; } break;
              case 1:  for (i=0; i<pimg->w0; i++) { n = (sp[i>>3] & mask1[i&7]) >> shift1[i&7]; *dp++ = pimg->pl[n].red; *dp++ = pimg->pl[n].green; *dp++ = pimg->pl[n].blue; *dp++ = pimg->tr[n]; } break;
            }
            break;
          case 4: 
            switch (pimg->d)
            {
              case 8:  for (i=0; i<pimg->w0; i++) { *dp++ = *sp; *dp++ = *sp; *dp++ = *sp++; *dp++ = *sp++; } break;
              case 16: for (i=0; i<pimg->w0; i++, sp+=4) { *dp++ = sp[0]; *dp++ = sp[0]; *dp++ = sp[0]; *dp++ = sp[2]; } break;
            }
            break;
          case 6:
            switch (pimg->d)
            {
              case 8:  memcpy(dp, sp, pimg->w0*4); break;
              case 16: for (i=0; i<pimg->w0; i++, sp+=8) { *dp++ = sp[0]; *dp++ = sp[2]; *dp++ = sp[4]; *dp++ = sp[6]; } break;
            }
            break;
        }
        prev_row = row;
        row += rowbytes;
      }

      /* prepare for dispose operation */
      if (pimg->dop == 2)
        memcpy(pimg->restore, pimg->image, pimg->size);

      /* blend operation */
      if (pimg->t < 4 && pimg->ts == 0)
        pimg->bop = 0;

      if (pimg->bop == 1)
      {
        int u, v, al;
        for (j=0; j<pimg->h0; j++)
        {
          sp = pimg->rows_frame[j];
          dp = pimg->rows_image[j+pimg->y0] + pimg->x0*4;

          for (i=0; i<pimg->w0; i++, sp+=4, dp+=4)
          {
            if (sp[3] == 255)
              memcpy(dp, sp, 4);
            else
            if (sp[3] != 0)
            {
              if (dp[3] != 0)
              {
                u = sp[3]*255;
                v = (255-sp[3])*dp[3];
                al = 255*255-(255-sp[3])*(255-dp[3]);
                dp[0] = (sp[0]*u + dp[0]*v)/al;
                dp[1] = (sp[1]*u + dp[1]*v)/al;
                dp[2] = (sp[2]*u + dp[2]*v)/al;
                dp[3] = al/255;
              }
              else
                memcpy(dp, sp, 4);
            }
          }  
        }
      }
      else
        for (j=0; j<pimg->h0; j++)
          memcpy(pimg->rows_image[j+pimg->y0] + pimg->x0*4, pimg->rows_frame[j], pimg->w0*4);

      SavePNG(pimg);

      /* dispose operation */
      if (pimg->dop == 2)
        memcpy(pimg->image, pimg->restore, pimg->size);
      else
      if (pimg->dop == 1)
        for (j=0; j<pimg->h0; j++)
          memset(pimg->rows_image[j+pimg->y0] + pimg->x0*4, 0, pimg->w0*4);

      inflateReset(&pimg->zstream);
    }
    else
    if (ret != Z_OK)
      return (-1);

    return (1);
  }
  else
    return (0);
}

void DecodeAPNG(char * szIn)
{
  FILE * f1;
  png_byte apng_chunks[]= {"acTL\0fcTL\0fdAT\0"};

  if ((f1 = fopen(szIn, "rb")) != 0)
  {
    png_colorp      palette;
    png_color_16p   trans_color;
    png_bytep       trans_alpha;
    unsigned int    rowbytes, j;
    unsigned char   sig[8];
    image_info      img;

    memset(&img, 0, sizeof(img));
    memset(img.tr, 255, 256);
    img.zstream.zalloc  = Z_NULL;
    img.zstream.zfree   = Z_NULL;
    img.zstream.opaque  = Z_NULL;
    inflateInit(&img.zstream);

    if (fread(sig, 1, 8, f1) == 8 && png_sig_cmp(sig, 0, 8) == 0)
    {
      png_structp png_ptr  = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
      png_infop   info_ptr = png_create_info_struct(png_ptr);
      if (png_ptr != NULL && info_ptr != NULL && setjmp(png_jmpbuf(png_ptr)) == 0)
      {
        png_set_keep_unknown_chunks(png_ptr, 2, apng_chunks, 3);
        png_set_read_user_chunk_fn(png_ptr, &img, handle_apng_chunks);
        png_init_io(png_ptr, f1);
        png_set_sig_bytes(png_ptr, 8);
        png_read_info(png_ptr, info_ptr);

        img.w    = png_get_image_width(png_ptr, info_ptr);
        img.h    = png_get_image_height(png_ptr, info_ptr);
        img.d    = png_get_bit_depth(png_ptr, info_ptr);
        img.t    = png_get_color_type(png_ptr, info_ptr);
        img.ch   = png_get_channels(png_ptr, info_ptr);
        img.it   = png_get_interlace_type(png_ptr, info_ptr);
        rowbytes = png_get_rowbytes(png_ptr, info_ptr);
        printf(" IN: %s : %dx%d\n", szIn, img.w, img.h);
        img.buf_size = img.h*(rowbytes+1);
        img.buf  = (unsigned char *)malloc(img.buf_size);

        if (png_get_PLTE(png_ptr, info_ptr, &palette, &img.ps))
          memcpy(img.pl, palette, img.ps * 3);
        else
          img.ps = 0;

        if (png_get_tRNS(png_ptr, info_ptr, &trans_alpha, &img.ts, &trans_color))
        {
          if (img.t == PNG_COLOR_TYPE_PALETTE)
            memcpy(img.tr, trans_alpha, img.ts);
          else
            memcpy(&img.tc, trans_color, sizeof(png_color_16));
        }
        else
          img.ts = 0;

        png_set_expand(png_ptr);
        png_set_gray_to_rgb(png_ptr);
        png_set_strip_16(png_ptr);
        png_set_add_alpha(png_ptr, 0xff, PNG_FILLER_AFTER);
        (void)png_set_interlace_handling(png_ptr);
        png_read_update_info(png_ptr, info_ptr);

        img.size = img.h*img.w*4;
        img.image = (png_bytep)malloc(img.size);
        img.frame = (png_bytep)malloc(img.size);
        img.restore = (png_bytep)malloc(img.size);
        img.rows_image = (png_bytepp)malloc(img.h*sizeof(png_bytep));
        img.rows_frame = (png_bytepp)malloc(img.h*sizeof(png_bytep));

        if (img.buf && img.image && img.frame && img.restore && img.rows_image && img.rows_frame)
        {
          for (j=0; j<img.h; j++)
            img.rows_image[j] = img.image + j*img.w*4;

          for (j=0; j<img.h; j++)
            img.rows_frame[j] = img.frame + j*img.w*4;

          png_read_image(png_ptr, img.rows_image);
          SavePNG(&img);
          if (img.dop != 0) memset(img.image, 0, img.size);
          png_read_end(png_ptr, info_ptr);

          free(img.rows_frame);
          free(img.rows_image);
          free(img.restore);
          free(img.frame);
          free(img.image);
          free(img.buf);
        }
      }
      png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    }
    inflateEnd(&img.zstream);
    fclose(f1);
  }
}

int main(int argc, char** argv)
{
  if (argc <= 1)
    printf("Usage : load5apng input.png\n");
  else
    DecodeAPNG(argv[1]);

  printf("\n");
  return (0);
}
