#if !defined(HANDMADE_H)

#include "./handmade_types.h"

struct thread_context {
    int Dummy;
};

inline uint32
SafeTruncateUInt64(uint64 Value)
{
    Assert(Value <= 0xFFFFFFFF);
    uint32 Result = (uint32)Value;
    return (Result);
}


#if HANDMADE_INTERNAL
struct debug_read_file_result {
  uint32 ContentSize;
  void *Contents;
};

#define DEBUG_PLATFORM_FREE_FILE_MEMORY(name) void name(thread_context *Context, void* Memory)
typedef DEBUG_PLATFORM_FREE_FILE_MEMORY(debug_platform_free_file_memory);

#define DEBUG_PLATFORM_READ_ENTIRE_FILE(name) debug_read_file_result name(thread_context *Context, char* Filename)
typedef DEBUG_PLATFORM_READ_ENTIRE_FILE(debug_platform_read_entire_file);

#define DEBUG_PLATFORM_WRITE_ENTIRE_FILE(name) bool name(thread_context *Context, char* Filename, uint32 MemorySize, void* Memory)
typedef DEBUG_PLATFORM_WRITE_ENTIRE_FILE(debug_platform_write_entire_file);

#endif



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

struct game_mouse_input {
    int MouseX;
    int MouseY;
    union {
        game_button_state Buttons[5];
        struct {
          game_button_state Left;
          game_button_state Middle;
          game_button_state Right;
          game_button_state Extra1;
          game_button_state Extra2;
        };
      };
};

struct game_input {
    game_mouse_input Mouse;
    game_controller_input Controllers[5];
};

struct game_memory {
  bool Initialized;
  uint64 PermanentStorageSize;
  void *PermanentStorage;

  uint64 TransientStorageSize;
  void *TransientStorage;

  #if HANDMADE_INTERNAL
  DEBUG_PLATFORM_FREE_FILE_MEMORY(*DebugPlatformFreeFileMemory);
  DEBUG_PLATFORM_READ_ENTIRE_FILE(*DebugPlatformReadEntireFile);
  DEBUG_PLATFORM_WRITE_ENTIRE_FILE(*DebugPlatformWriteEntireFile);
  #endif
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
  void name(thread_context *Context, game_memory *Memory, game_input *Input,                            \
            game_offscreen_buffer *ScreenBuffer, bool *ShallExit)

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender);
typedef GAME_UPDATE_AND_RENDER(game_update_and_render);

#define GAME_GET_SOUND_SAMPLES(name)                                           \
  void name(thread_context *Context, game_memory *Memory, game_sound_output_buffer *SoundBuffer)
typedef GAME_GET_SOUND_SAMPLES(game_get_sound_samples);
extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples);

#define HANDMADE_H
#endif
