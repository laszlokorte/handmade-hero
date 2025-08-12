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

  real32 AverageStickX;
  real32 AverageStickY;

  union {
    game_button_state Buttons[12];
    struct {
      game_button_state MoveUp;
      game_button_state MoveDown;
      game_button_state MoveLeft;
      game_button_state MoveRight;
      game_button_state ActionUp;
      game_button_state ActionDown;
      game_button_state ActionLeft;
      game_button_state ActionRight;
      game_button_state LeftShoulder;
      game_button_state RightShoulder;
      game_button_state Menu;
      game_button_state Back;
    };
  };
};

struct game_input {
  game_controller_input Controllers[5];
};

struct game_memory {
  bool Initialized;
  uint64 PermanentStorageSize;
  void *PermanentStorage;

  uint64 TransientStorageSize;
  void *TransientStorage;
};

struct game_state {
  bool Muted;
  uint64 Time;
  int XPos;
  int YPos;
  int XPlayer;
  int YPlayer;
  int Note;
  int Volume;
  int JumpTime;
};

struct game_sound_synth {
  int ToneBaseVolume;
  real32 ToneBaseFreqInHz;
  real32 ToneStepFactor;
  real32 GeneratorTimeInRadians;
};

#define GAME_UPDATE_AND_RENDER(name)                                           \
  void name(game_memory *Memory, game_input *Input,                            \
            game_offscreen_buffer *ScreenBuffer, bool *ShallExit)

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender);
typedef GAME_UPDATE_AND_RENDER(game_update_and_render);

#define GAME_GET_SOUND_SAMPLES(name)                                           \
  void name(game_memory *Memory, game_sound_output_buffer *SoundBuffer)
typedef GAME_GET_SOUND_SAMPLES(game_get_sound_samples);
extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples);

#define HANDMADE_H
#endif
