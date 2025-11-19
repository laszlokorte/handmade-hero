#if !defined(HANDMADE_H)

#include "./handmade_types.h"
#include "./tilemap.h"
#include "./renderer.h"
#include "./work_queue.h"
typedef struct thread_context thread_context;
typedef struct debug_read_file_result debug_read_file_result;
typedef struct game_offscreen_buffer game_offscreen_buffer;
typedef struct game_sound_output_buffer game_sound_output_buffer;
typedef struct game_button_state game_button_state;
typedef struct game_controller_input game_controller_input;
typedef struct game_mouse_input game_mouse_input;
typedef struct game_input game_input;
typedef struct game_memory game_memory;
typedef struct game_velocity game_velocity;
typedef struct game_position game_position;
typedef struct game_color_rgb game_color_rgb;
typedef struct game_entity game_entity;
typedef struct game_controller_entity_map game_controller_entity_map;
typedef struct game_camera game_camera;
typedef struct game_sound_synth game_sound_synth;
typedef struct game_sound_state game_sound_state;
typedef struct game_state game_state;
typedef struct game_size game_size;


struct thread_context {
  int Dummy;
};

inline uint32 SafeTruncateUInt64(uint64 Value) {
  Assert(Value <= 0xFFFFFFFF);
  uint32 Result = (uint32)Value;
  return (Result);
}

struct debug_read_file_result {
  uint32 ContentSize;
  void *Contents;
};

#define DEBUG_PLATFORM_FREE_FILE_MEMORY(name)                                  \
  void name(thread_context *Context, void *Memory)
typedef DEBUG_PLATFORM_FREE_FILE_MEMORY(debug_platform_free_file_memory);

#define DEBUG_PLATFORM_READ_ENTIRE_FILE(name)                                  \
  debug_read_file_result name(thread_context *Context, char *Filename)
typedef DEBUG_PLATFORM_READ_ENTIRE_FILE(debug_platform_read_entire_file);

#define DEBUG_PLATFORM_WRITE_ENTIRE_FILE(name)                                 \
  bool name(thread_context *Context, char *Filename, uint32 MemorySize,        \
            void *Memory)
typedef DEBUG_PLATFORM_WRITE_ENTIRE_FILE(debug_platform_write_entire_file);

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
  int DeltaX;
  int DeltaY;
  bool InRange;

  real32 WheelX;
  real32 WheelY;

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
  real32 DeltaTime;
  game_mouse_input Mouse;
  game_controller_input Controllers[5];
};

struct game_memory {
  bool Initialized;
  memory_index PermanentStorageSize;
  uint8 *PermanentStorage;

  memory_index TransientStorageSize;
  uint8 *TransientStorage;

  debug_platform_free_file_memory *DebugPlatformFreeFileMemory;
  debug_platform_read_entire_file *DebugPlatformReadEntireFile;
  debug_platform_write_entire_file *DebugPlatformWriteEntireFile;

  work_queue *TaskQueue;
  platform_push_task_to_queue *PlatformPushTaskToQueue;
  platform_wait_for_queue_to_finish *PlatformWaitForQueueToFinish;
};

struct game_position {
  real32 x;
  real32 y;
};

struct game_velocity {
  real32 x;
  real32 y;
};

struct game_size {
  real32 x;
  real32 y;
};

struct game_color_rgb {
  real32 r;
  real32 g;
  real32 b;
};

typedef enum game_direction4 {
  GameDirectionNorth,
  GameDirectionSouth,
  GameDirectionEast,
  GameDirectionWest,
} game_direction4;

typedef enum game_direction8 {
  GameDirectionJustNorth,
  GameDirectionJustSouth,
  GameDirectionJustEast,
  GameDirectionJustWest,
  GameDirectionNorthWest,
  GameDirectionSouthWest,
  GameDirectionNorthEast,
  GameDirectionSouthEast,
} game_direction8;

struct game_entity {
  bool active;
  tile_position p;
  game_velocity v;
  game_size s;
  game_color_rgb c;
  game_direction8 o;
};

struct game_controller_entity_map {
  game_entity *controllers[5];
};

struct game_camera {
  tile_position pos;
  real32 ZoomLevel;
};

struct game_sound_synth {
  real32 ToneBaseVolume;
  int32 Note;
  int32 Duration;
  int32 Progress;
  real32 GeneratorTimeInRadians;
  game_sound_synth *NextSound;
};

struct game_sound_state {
  memory_arena SoundArena;
  game_sound_synth *PlayingSound;
  game_sound_synth *FreeSound;
};

#define ENTITY_MAX 30
struct game_state {
  bool Muted;
  uint64 Time;
  int VolumeRange;
  int Volume;
  int JumpTime;
  int EntityCount;
  loaded_bitmap Logo;
  game_controller_entity_map ControllerMap;
  game_entity Entities[ENTITY_MAX];
  memory_arena WorldArena;
  game_camera Camera;
  game_entity *CameraTrack;
  tile_map TileMap;
  game_sound_state SoundState;
};

typedef bool (game_update_and_render)(
    thread_context *Context,
    game_memory *Memory,
    game_input *Input,
    render_buffer *RenderBuffer
);

typedef void (game_get_sound_samples)(
    thread_context *Context,
    game_memory *Memory,
    game_sound_output_buffer *SoundBuffer
);


#define GAME_UPDATE_AND_RENDER(name)                                           \
  bool name(thread_context *Context, game_memory *Memory, game_input *Input,   \
            render_buffer *RenderBuffer)

#define GAME_GET_SOUND_SAMPLES(name)                                           \
  void name(thread_context *Context, game_memory *Memory,                      \
            game_sound_output_buffer *SoundBuffer)

#ifdef __cplusplus
extern "C" {
#endif

GAME_UPDATE_AND_RENDER(GameUpdateAndRender);
typedef GAME_UPDATE_AND_RENDER(game_update_and_render);

GAME_GET_SOUND_SAMPLES(GameGetSoundSamples);
typedef GAME_GET_SOUND_SAMPLES(game_get_sound_samples);

#ifdef __cplusplus
} // extern "C"
#endif


#define HANDMADE_H
#endif
