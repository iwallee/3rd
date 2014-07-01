/*
 * load1png.c
 *
 * loads static PNG file, saves it as TGA (24bpp or 32bpp).
 * uses high-level png_read_png() function.
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
#include "png.h"

void SaveTGA(unsigned char ** rows, unsigned int w, unsigned int h, unsigned int channels)
{
  char szOut[512];
  FILE * f2;
  if (channels == 3 || channels == 4)
  {
    unsigned short tgah[9] = {0,2,0,0,0,0,(unsigned short)w,(unsigned short)h,(channels == 3) ? 0x0018 : 0x0820};
    sprintf(szOut, "test_load1.tga");
    if ((f2 = fopen(szOut, "wb")) != 0)
    {
      unsigned int j;
      if (fwrite(&tgah, 1, 18, f2) != 18) return;
      for (j=0; j<h; j++)
        if (fwrite(rows[h-1-j], channels, w, f2) != w) return;
      fclose(f2);
    }
    printf("OUT: %s : %dx%d\n", szOut, w, h);
  }
}

void LoadPNG(char * szImage)
{
  FILE * f1;

  if ((f1 = fopen(szImage, "rb")) != 0)
  {
    unsigned int    width, height, channels;
    png_bytepp      rows;
    unsigned char   sig[8];

    if (fread(sig, 1, 8, f1) == 8 && png_sig_cmp(sig, 0, 8) == 0)
    {
      png_structp png_ptr  = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
      png_infop   info_ptr = png_create_info_struct(png_ptr);
      if (png_ptr != NULL && info_ptr != NULL && setjmp(png_jmpbuf(png_ptr)) == 0)
      {
        png_init_io(png_ptr, f1);
        png_set_sig_bytes(png_ptr, 8);
        png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_EXPAND | PNG_TRANSFORM_STRIP_16 | PNG_TRANSFORM_GRAY_TO_RGB | PNG_TRANSFORM_BGR, NULL);
        width    = png_get_image_width(png_ptr, info_ptr);
        height   = png_get_image_height(png_ptr, info_ptr);
        channels = png_get_channels(png_ptr, info_ptr);
        printf(" IN: %s : %dx%d\n", szImage, width, height);
        rows = png_get_rows(png_ptr, info_ptr);
        SaveTGA(rows, width, height, channels);
      }
      png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    }
    fclose(f1);
  }
}

int main(int argc, char** argv)
{
  if (argc <= 1)
    printf("Usage : load1png input.png\n");
  else
    LoadPNG(argv[1]);

  printf("\n");
  return (0);
}
