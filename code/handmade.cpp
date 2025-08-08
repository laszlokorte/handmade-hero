#include "./handmade.h"

internal void RenderGradient(game_offscreen_buffer *Buffer, int xoff, int yoff,
                             int zoff) {
  unsigned int *canvas = (unsigned int *)(Buffer->Memory);
  int cx = Buffer->Width / 2;
  int cy = Buffer->Height / 2;
  for (int x = 0; x < Buffer->Width; x++) {
    for (int y = 0; y < Buffer->Height; y++) {
      int yy = y - cy;
      int xx = x - cx;

      uint8_t green = xx + xoff;
      uint8_t blue = yy + yoff;
      uint8_t red = zoff / 2;
      if ((zoff / 256) % 2 == 0) {
        red = 255 - red;
      }
      canvas[(y)*Buffer->Width + (x)] = (green << 8) | blue | (red << 16);
    }
  }
}

internal void GameUpdateAndRender(game_offscreen_buffer *Buffer, int xoff, int yoff, int zoff) {
    RenderGradient(Buffer, xoff, yoff, zoff);
}
