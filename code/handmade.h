#if !defined(HANDMADE_H)

#include "./handmade_types.h"

struct game_offscreen_buffer {
  void *Memory;
  int Width;
  int Height;
  int BytesPerPixel;
};

internal void GameUpdateAndRender(game_offscreen_buffer *Buffer, int offx, int offy, int offz);

#define HANDMADE_H
#endif
