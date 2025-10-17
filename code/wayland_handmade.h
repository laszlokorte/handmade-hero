#if !defined(WAYLAND_HANDMADE_H)

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-client.h>
#include <wayland-client-protocol.h>

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <wayland-egl.h>

#include <unistd.h>
#include <time.h>
#include <limits.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dlfcn.h>

#include "handmade.h"

struct gl_vertex {
  float pos[2];
  float col[4];
};

struct gl_vertices {
  size_t Capacity;
  size_t Count;
  gl_vertex *Buffer;
};
struct gl_state {
  GLint uniformOffset;
  GLuint vertexBufferIndex;

  gl_vertices Vertices;
};

struct app_state {};

static const char *VertexShaderSource =
    "uniform vec2 offset;\n"
    "attribute vec2 pos;\n"
    "attribute vec4 color;\n"
    "varying vec4 varColor;\n"
    "void main(){ \n"
    "  gl_Position = vec4(pos + offset, 0.0, 1.0);\n"
    "  varColor = vec4(color.rgb,1.0); \n"
    "}\n";

static const char *FragmentShaderSource =
    "precision mediump float;\n"
    "varying vec4 varColor;\n"
    "void main(){ gl_FragColor = varColor; }\n";

void frame_new(void *data, struct wl_callback *cb, uint32_t a);

struct linux_screen_buffer {
  uint32_t Width;
  uint32_t Height;
  uint32_t Pitch;
  uint32_t BytesPerPixel;
  void *Memory;
};

struct linux_game {
  bool IsValid;
  void *GameDLL;
  __time_t LatestWriteTime;
  char *FullDllPath;
  game_update_and_render *GameUpdateAndRender;
  game_get_sound_samples *GameGetSoundSamples;
};

struct linux_work_queue_task {
  work_queue_callback *Callback;
  void *Data;
};

struct work_queue {
  size_t Size;
  linux_work_queue_task *Base;

  uint32 volatile NextWrite;
  uint32 volatile NextRead;

  size_t volatile CompletionGoal;
  size_t volatile CompletionCount;

  void *SemaphoreHandle;
};

struct linux_thread_info {
  int32 LogicalThreadIndex;
  uint32 ThreadId;
  work_queue *Queue;
};

struct linux_thread_pool {
  size_t Count;
  linux_thread_info *Threads;
};

struct linux_state {
  bool Running;
  bool Configured;
  float WindowWidth;
  float WindowHeight;

  struct wl_display *Display;
  struct wl_surface *Surface;
  struct wl_egl_window *EglWindow;

  EGLDisplay EglDisplay;
  EGLSurface EglSurface;
  EGLContext EglContext;

  struct gl_state GLState;

  linux_screen_buffer ScreenBuffer;
  linux_game Game;

  game_input GameInputs[2];
  size_t CurrentGameInputIndex;
  game_memory GameMemory;

  size_t TotalMemorySize;
  void *GameMemoryBlock;
  render_buffer RenderBuffer;
  work_queue WorkQueue;
  linux_thread_pool ThreadPool;
};

struct linux_audio_buffer {
  size_t Size;
  int16_t *Memory;

  uint32 ReadHead;
  uint32 WriteHead;
};

#define WAYLAND_HANDMADE_H
#endif
