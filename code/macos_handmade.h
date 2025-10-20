#if !defined(MACOS_HANDMADE_H)
#include <AppKit/AppKit.h>
#include <Carbon/Carbon.h>
#include <time.h>
#include <semaphore.h>
#include <mach/mach_time.h>

#include "handmade.h"

global_variable mach_timebase_info_data_t GlobalMachTimebase;

struct macos_screen_buffer {
  uint32_t Width;
  uint32_t Height;
  uint32_t Pitch;
  uint32_t BytesPerPixel;
  void *Memory;
};

struct macos_game {
  bool IsValid;
  void *GameDLL;
  timespec LatestWriteTime;
  char *FullDllPath;
  game_update_and_render *GameUpdateAndRender;
  game_get_sound_samples *GameGetSoundSamples;
};

struct macos_work_queue_task {
  work_queue_callback *Callback;
  void *Data;
};

struct work_queue {
  size_t Size;
  macos_work_queue_task *Base;

  _Atomic unsigned int NextWrite;
  _Atomic unsigned int NextRead;
  _Atomic long long CompletionGoal;
  _Atomic long long CompletionCount;

  dispatch_semaphore_t Semaphore;
};

struct macos_thread_info {
  int32 LogicalThreadIndex;
  uint32 ThreadId;
  work_queue *Queue;
};

struct macos_thread_pool {
  size_t Count;
  macos_thread_info *Threads;
};

struct macos_state {
  bool Running;
  float WindowWidth;
  float WindowHeight;

  NSWindow *Window;
  macos_screen_buffer ScreenBuffer;
  macos_game Game;

  size_t TotalMemorySize;
  void *GameMemoryBlock;
  render_buffer RenderBuffer;
  work_queue WorkQueue;
  macos_thread_pool ThreadPool;
};

struct mac_audio_buffer {
  size_t Size;
  int16_t *Memory;

  uint32 ReadHead;
  uint32 WriteHead;
};
#define MACOS_HANDMADE_H
#endif
