#if !defined(WAYLAND_HANDMADE_H)

#include <alsa/asoundlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <atomic>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-client.h>
#include <wayland-client-protocol.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-keysyms.h>

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

union v4 {
  struct {
    float x;
    float y;
    float z;
    float w;
  };
  struct {
    float r;
    float g;
    float b;
    float a;
  };
  float vals[4];
};

union v3 {
  struct {
    float x;
    float y;
    float z;
  };
  struct {
    float r;
    float g;
    float b;
  };
  float vals[3];
};
union v2 {
  struct {
    float x;
    float y;
  };
  float vals[2];
};

union m33 {
  v3 rows[3];
  float entries[3 * 3];
};

union m44 {
  v4 rows[4];
  float entries[4 * 4];
};

struct gl_vertex {
  v2 pos;
  v4 color;
  v3 texCoord;
};

struct gl_vertices {
  size_t Capacity;
  size_t Count;
  gl_vertex *Buffer;
};
struct gl_uniforms {
  GLint ViewMatrix;
  GLint Texture;
};

struct gl_bitmap_slot {
  size_t Texture;
  void *Bitmap;
};

struct gl_state {
  gl_uniforms Uniforms;
  GLuint Textures[10];
  gl_bitmap_slot Bitmaps[10];
  GLuint vertexBufferIndex;

  gl_vertices Vertices;
};

struct app_state {};

static const char *VertexShaderSource =
    "#version 330 core\n"
    "uniform mat3 viewMatrix;\n"
    "in vec2 pos;\n"
    "in vec3 texCoord;\n"
    "in vec4 color;\n"
    "out vec4 varColor;\n"
    "out vec3 texUVW;\n"
    "void main(){ \n"
    "  vec2 newPos = (viewMatrix * vec3(pos,1.0)).xy;"
    "  gl_Position = vec4(newPos, 0.0, 1.0);\n"
    "  varColor = color; \n"
    "  texUVW = texCoord; \n"
    "}\n";

static const char *FragmentShaderSource =
    "#version 330 core\n"
    "uniform sampler2D texture;\n"
    "uniform float useTexture;\n"
    "in vec4 varColor;\n"
    "in vec3 texUVW;\n"
    "out vec4 FragColor;\n"
    "void main(){ FragColor = texture2D(texture, texUVW.xy) * texUVW.z + "
    "varColor * (1.0- texUVW.z); }\n";

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

  std::atomic<unsigned int> NextWrite;
  std::atomic<unsigned int> NextRead;
  std::atomic<long long> CompletionGoal;
  std::atomic<long long> CompletionCount;

  sem_t Semaphore;
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

  xkb_context *XKbdContext;
  xkb_keymap *XKbdKeyMap;
  xkb_state *XKbdState;

  snd_pcm_t *PCM;
  pthread_t AudioThread;

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
