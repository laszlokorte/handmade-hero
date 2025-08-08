#if !defined(HANDMADE_H)

#include "./handmade_types.h"

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))

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

struct game_button_state {
  int HalfTransitionCount;
  bool EndedDown;
};

struct game_controller_input {
  bool isAnalog;

  real32 StartX;
  real32 StartY;

  real32 MinX;
  real32 MinY;

  real32 MaxX;
  real32 MaxY;

  real32 EndX;
  real32 EndY;

  union {
    game_button_state Buttons[6];
    struct {
      game_button_state Up;
      game_button_state Down;
      game_button_state Left;
      game_button_state Right;
      game_button_state LeftShould;
      game_button_state RightShoulder;
    };
  };
};

struct game_input {
  game_controller_input Controllers[4];
};

struct game_memory {
    bool Initialized;
    uint64 PermanentStorageSize;
  void *PermanentStorage;
};

struct game_state {
  bool initialized;
  uint64 time;
  int xpos;
  int ypos;
  int note;
  int volume;
};

struct game_sound_synth {
  int ToneBaseVolume;
  real32 ToneBaseFreqInHz;
  real32 ToneStepFactor;
  real32 GeneratorTimeInRadians;
};

internal void GameUpdateAndRender(game_memory *Memory, game_input *input,
                                  game_offscreen_buffer *ScreenBuffer,
                                  game_sound_output_buffer *SoundBuffer);

#define HANDMADE_H
#endif
