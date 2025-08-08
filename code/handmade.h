#if !defined(HANDMADE_H)

#include "./handmade_types.h"

struct game_offscreen_buffer {
  void *Memory;
  int Width;
  int Height;
  int BytesPerPixel;
};

struct game_sound_output_buffer {
    int SamplesPerSecond;
    int SampleCount;
    int16 *Samples;
};

internal void GameUpdateAndRender(game_offscreen_buffer *ScreenBuffer, game_sound_output_buffer *SoundBuffer, int offx, int offy, int offz);

#define HANDMADE_H
#endif
