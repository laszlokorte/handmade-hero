#if !defined(HANDMADE_DRAW_H)

#include "./handmade_math.h"
#include "./handmade.h"
#include "renderer.h"

typedef struct game_offscreen_buffer {
  void *Memory;
  int Width;
  int Height;
  int BytesPerPixel;
} game_offscreen_buffer;

internal int Clamp(int value, int min, int max) {
  if (value < min) {
    return min;
  }
  if (value > max) {
    return max;
  }
  return value;
}

internal void FillRect(game_offscreen_buffer *Buffer, real32 XMinReal,
                       real32 YMinReal, real32 XMaxReal, real32 YMaxReal,
                       game_color_rgb Color) {
  int XMin = Clamp(RoundRealToInt(XMinReal), 0, Buffer->Width);
  int XMax = Clamp(RoundRealToInt(XMaxReal), 0, Buffer->Width);
  int YMin = Clamp(RoundRealToInt(YMinReal), 0, Buffer->Height);
  int YMax = Clamp(RoundRealToInt(YMaxReal), 0, Buffer->Height);

  int Red = RoundRealToInt(255.0f * Color.r);
  int Green = RoundRealToInt(255.0f * Color.g);
  int Blue = RoundRealToInt(255.0f * Color.b);

  int RGB = Red << 16 | Green << 8 | Blue;

  size_t Stride = Buffer->Width * Buffer->BytesPerPixel;
  uint8 *Row =
      (uint8 *)Buffer->Memory + Buffer->BytesPerPixel * XMin + Stride * YMin;
  for (int y = YMin; y < YMax; y++) {
    uint8 *Pixel = Row;
    for (int x = XMin; x < XMax; x++) {
      *(int *)Pixel = RGB;
      Pixel += Buffer->BytesPerPixel;
    }
    Row += Stride;
  }
}

internal int32 SampleBitmap(loaded_bitmap *Bitmap, int x, int y) {
  return Bitmap
      ->Memory[(x % Bitmap->Width) +
               (Bitmap->Height - 1 - ((y % Bitmap->Height))) * Bitmap->Width];
}

internal uint32 lerpColor(real32 t, uint32 c1, uint32 c2) {
  uint32 a1 = (c1 >> 24) & 0xff;
  uint32 r1 = (c1 >> 16) & 0xff;
  uint32 g1 = (c1 >> 8) & 0xff;
  uint32 b1 = (c1 >> 0) & 0xff;
  uint32 a2 = (c2 >> 24) & 0xff;
  uint32 r2 = (c2 >> 16) & 0xff;
  uint32 g2 = (c2 >> 8) & 0xff;
  uint32 b2 = (c2 >> 0) & 0xff;

  real32 rm = r1 * (1.0f - t) + r2 * t;
  real32 gm = g1 * (1.0f - t) + g2 * t;
  real32 bm = b1 * (1.0f - t) + b2 * t;
  real32 am = a1 * (1.0f - t) + a2 * t;
  return ((int)am << 24) | ((int)rm << 16) | ((int)gm << 8) | ((int)bm << 0);
}

internal uint32 AlphaBlendARGB(uint32 Bg, uint32 Fg) {
  real32 Alpha = (((Fg >> 24) & 0xff) / 255.0f);
  return lerpColor(Alpha, Bg, Fg);
}

internal uint32 SampleBitmapBilinear(loaded_bitmap *Bitmap, real32 x,
                                     real32 y) {
  real32 xr = x * Bitmap->Width;
  real32 yr = y * Bitmap->Height;
  int x0 = (int)xr;
  int y0 = (int)yr;
  int x1 = (int)xr + 1;
  int y1 = (int)yr + 1;
  real32 xt = xr - x0;
  real32 yt = yr - y0;

  int c00 = SampleBitmap(Bitmap, x0, y0);
  int c01 = SampleBitmap(Bitmap, x0, y1);
  int c10 = SampleBitmap(Bitmap, x1, y0);
  int c11 = SampleBitmap(Bitmap, x1, y1);

  return lerpColor(xt, lerpColor(yt, c00, c01), lerpColor(yt, c10, c11));
}

internal void FillRectTexture(game_offscreen_buffer *Buffer, real32 XMinReal,
                              real32 YMinReal, real32 XMaxReal, real32 YMaxReal,
                              loaded_bitmap *Bitmap) {
  real32 RealWidth = YMaxReal - YMinReal;
  real32 RealHeight = YMaxReal - YMinReal;

  int XMinRealRound = RoundRealToInt(XMinReal);
  int XMaxRealRound = RoundRealToInt(XMaxReal);
  int YMinRealRound = RoundRealToInt(YMinReal);
  int YMaxRealRound = RoundRealToInt(YMaxReal);
  int XMin = Clamp(XMinRealRound, 0, Buffer->Width);
  int XMax = Clamp(XMaxRealRound, 0, Buffer->Width);
  int YMin = Clamp(YMinRealRound, 0, Buffer->Height);
  int YMax = Clamp(YMaxRealRound, 0, Buffer->Height);

  size_t Stride = Buffer->Width * Buffer->BytesPerPixel;
  uint8 *Row =
      (uint8 *)Buffer->Memory + Buffer->BytesPerPixel * XMin + Stride * YMin;
  for (int y = YMin; y < YMax; y++) {
    uint8 *Pixel = Row;
    for (int x = XMin; x < XMax; x++) {
      real32 u = (x - XMinRealRound) / (RealWidth);
      real32 v = (y - YMinRealRound) / (RealHeight);
      int ARGB = SampleBitmapBilinear(Bitmap, u, v);
      *(int *)Pixel = AlphaBlendARGB(*(int *)Pixel, ARGB);
      Pixel += Buffer->BytesPerPixel;
    }
    Row += Stride;
  }
}

internal void RenderRect(game_offscreen_buffer *Buffer, int X, int Y, int Width,
                         int Height, int Color) {
  if (X < 0) {
    Width += X;
    X = 0;
  }
  if (Y < 0) {
    Height += Y;
    Y = 0;
  }
  if (X >= Buffer->Width || Y >= Buffer->Height) {
    return;
  }
  if (X + Width >= Buffer->Width) {
    Width = Buffer->Width - X;
  }
  if (Y + Height >= Buffer->Height) {
    Height = Buffer->Height - Y;
  }
  size_t Stride = Buffer->Width * Buffer->BytesPerPixel;
  uint8 *Row = (uint8 *)Buffer->Memory + Buffer->BytesPerPixel * X + Stride * Y;
  for (int y = 0; y < Height; y++) {
    uint8 *Pixel = Row;
    for (int x = 0; x < Width; x++) {
      *(int *)Pixel = Color;
      Pixel += Buffer->BytesPerPixel;
    }
    Row += Stride;
  }
}

internal void ClearScreen(game_offscreen_buffer *Buffer, int ClearColor) {
  unsigned int *canvas = (unsigned int *)(Buffer->Memory);
  int cx = Buffer->Width / 2;
  int cy = Buffer->Height / 2;
  for (int x = 0; x < Buffer->Width; x++) {
    for (int y = 0; y < Buffer->Height; y++) {
      canvas[(y)*Buffer->Width + (x)] = ClearColor;
    }
  }
}

internal void RenderGradient(game_offscreen_buffer *Buffer, int xoff, int yoff,
                             int zoff) {
  unsigned int *canvas = (unsigned int *)(Buffer->Memory);
  int cx = Buffer->Width / 2;
  int cy = Buffer->Height / 2;
  for (int x = 0; x < Buffer->Width; x++) {
    for (int y = 0; y < Buffer->Height; y++) {
      int yy = y - cy;
      int xx = x - cx;

      uint8 green = (uint8)(xx + xoff);
      uint8 blue = (uint8)(yy + yoff);
      uint8 red = (uint8)(zoff / 2);
      if ((zoff / 256) % 2 == 0) {
        red = 255 - red;
      }
      canvas[(y)*Buffer->Width + (x)] = (green << 8) | blue | (red << 16);
    }
  }
}
#define HANDMADE_DRAW_H
#endif
