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

struct gl_state {
  GLint uniformOffset;
};

struct app_state {
  struct wl_display *display;
  struct wl_surface *surface;
  struct wl_egl_window *egl_window;

  EGLDisplay egl_display;
  EGLSurface egl_surface;
  EGLContext egl_context;
  int width;
  int height;
  int configured;

  struct gl_state GLState;
};
static struct app_state app = {0};

static GLfloat verts[] = {-0.5f, 0.5f, -0.5f, -0.5f, 0.5f, -0.5f,
                          0.5f,  0.5f, -0.5f, 0.5f,  0.5f, -0.5f};
static int VertexCount = 6;

static const char *VertexShaderSource =
    "uniform vec2 offset;\n"
    "attribute vec2 pos;\n"
    "void main(){ gl_Position = vec4(pos + offset, 0.0, 1.0); }\n";

static const char *FragmentShaderSource =
    "precision mediump float;\n"
    "void main(){ gl_FragColor = vec4(0.1, 0.8, 0.7, 1.0); }\n";

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
  float WindowWidth;
  float WindowHeight;

  linux_screen_buffer ScreenBuffer;
  linux_game Game;

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
