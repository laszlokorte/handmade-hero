#include "./handmade.h"
#include "math.h"



global_variable game_state global_game_state = {};

internal void GameOutputSound(game_sound_output_buffer *SoundBuffer) {
  local_persist real32 tSin = 0;
  int16 ToneVolumne = 3000 * pow(2.0, global_game_state.volume / 5.0 );
  int toneHz = 440;
  real32 f = (real32)toneHz / (real32)SoundBuffer->SamplesPerSecond * pow(2.0, NOTE_HALFTONE * global_game_state.note);
  int16 *SampleOut = SoundBuffer->Samples;
  for (int SampleIndex = 0; SampleIndex < SoundBuffer->SampleCount;
       ++SampleIndex) {
    int16 SampleValue = sinf(2.0f * Pi32 * tSin) * ToneVolumne;
    *SampleOut++ = SampleValue;
    *SampleOut++ = SampleValue;

    tSin = fmodf(tSin + f, 1.0);
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

internal void GameSetup() {
}

internal void GameUpdateAndRender(game_input *input, game_offscreen_buffer *Buffer,
                                  game_sound_output_buffer *SoundBuffer) {
  global_game_state.time++;
  GameOutputSound(SoundBuffer);
  RenderGradient(Buffer, global_game_state.xpos, global_game_state.ypos, global_game_state.time);
}
