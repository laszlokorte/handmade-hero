#include "x11_handmade.h"
#include "handmade.h"
#include <GL/gl.h>
#include <GL/glext.h>
#include <GLES2/gl2.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <xkbcommon/xkbcommon-keysyms.h>
#include <linux/input-event-codes.h>

#include "linux_work_queue.cpp"

PFNGLGENVERTEXARRAYSPROC glGenVertexArrays = NULL;
PFNGLBINDVERTEXARRAYPROC glBindVertexArray = NULL;

void m3Mul(m33 *a, m33 *b, m33 *t) {
  m33 res = {0};
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      for (int k = 0; k < 3; ++k) {
        res.rows[i].vals[j] += a->rows[i].vals[k] * b->rows[k].vals[j];
      }
    }
  }
  t->rows[0] = res.rows[0];
  t->rows[1] = res.rows[1];
  t->rows[2] = res.rows[2];
}

void m3MakeIdentity(m33 *mat) {
  mat->rows[0] = {0};
  mat->rows[1] = {0};
  mat->rows[2] = {0};
  mat->rows[0].vals[0] = 1.0;
  mat->rows[1].vals[1] = 1.0;
  mat->rows[2].vals[2] = 1.0;
}
void m3makeScale(m33 *mat, float x, float y) {
  mat->rows[0] = {0};
  mat->rows[1] = {0};
  mat->rows[2] = {0};
  mat->rows[0].vals[0] = x;
  mat->rows[1].vals[1] = y;
  mat->rows[2].vals[2] = 1;
}
void m3makeTranslate(m33 *mat, float x, float y) {
  mat->rows[0] = {0};
  mat->rows[1] = {0};
  mat->rows[2] = {0};
  mat->rows[0].vals[0] = 1;
  mat->rows[1].vals[1] = 1;
  mat->rows[2].vals[0] = x;
  mat->rows[2].vals[1] = y;
  mat->rows[2].vals[2] = 1;
}

static void GlPushQuad(gl_vertices *Vertices, float x0, float y0, float x1,
                       float y1, float r, float g, float b, float a,
                       GLint texture) {
  if (Vertices->Capacity < Vertices->Count + 6) {
    // printf("%lu\n", Vertices->Count);
    // printf("%lu\n", Vertices->Capacity);
    return;
  }
  Vertices->Buffer[(Vertices->Count)++] = {
      .pos = {x0, y0},
      .color = {r, g, b, a},
      .texCoord = {0.0f, 1.0f, texture > -1 ? 1.0f : 0.0f}};
  Vertices->Buffer[(Vertices->Count)++] = {
      .pos = {x1, y0},
      .color = {r, g, b, a},
      .texCoord = {1.0f, 1.0f, texture > -1 ? 1.0f : 0.0f}};
  Vertices->Buffer[(Vertices->Count)++] = {
      .pos = {x0, y1},
      .color = {r, g, b, a},
      .texCoord = {0.0f, 0.0f, texture > -1 ? 1.0f : 0.0f}};
  Vertices->Buffer[(Vertices->Count)++] = {
      .pos = {x1, y0},
      .color = {r, g, b, a},
      .texCoord = {1.0f, 1.0f, texture > -1 ? 1.0f : 0.0f}};
  Vertices->Buffer[(Vertices->Count)++] = {
      .pos = {x1, y1},
      .color = {r, g, b, a},
      .texCoord = {1.0f, 0.0f, texture > -1 ? 1.0f : 0.0f}};
  Vertices->Buffer[(Vertices->Count)++] = {
      .pos = {x0, y1},
      .color = {r, g, b, a},
      .texCoord = {0.0f, 0.0f, texture > -1 ? 1.0f : 0.0f}};
}

DEBUG_PLATFORM_FREE_FILE_MEMORY(PlatformFreeFileNoop) {}
DEBUG_PLATFORM_READ_ENTIRE_FILE(PlatformReadEntireFileNoop) {
  debug_read_file_result Result = {};

  return Result;
}
DEBUG_PLATFORM_WRITE_ENTIRE_FILE(PlatformWriteEntireFileNoop) { return false; }

PUSH_TASK_TO_QUEUE(PushTaskToQueueNoop) {}

WAIT_FOR_QUEUE_TO_FINISH(WaitForQueueToFinishNoop) {}

GAME_UPDATE_AND_RENDER(GameUpdateAndRenderNoop) { return false; }

GAME_GET_SOUND_SAMPLES(GameGetSoundSamplesNoop) {}

__time_t LinuxGetLastWriteTime(const char *filename) {
  __time_t lastWrite = {0};

  struct stat st = {0};
  if (stat(filename, &st) == 0) {
    lastWrite = st.st_mtime;
  }

  return lastWrite;
}
DEBUG_PLATFORM_FREE_FILE_MEMORY(DEBUGPlatformFreeFileMemory) {
  if (Memory) {
    free(Memory); // On Linux, we can just free
  }
}

DEBUG_PLATFORM_READ_ENTIRE_FILE(DEBUGPlatformReadEntireFile) {
  debug_read_file_result Result = {0};
  int fd = open(Filename, O_RDONLY);
  if (fd != -1) {
    struct stat st;
    if (fstat(fd, &st) == 0) {
      size_t FileSize = (size_t)st.st_size;
      void *Memory = malloc(FileSize);
      if (Memory) {
        ssize_t BytesRead = read(fd, Memory, FileSize);
        if (BytesRead == (ssize_t)FileSize) {
          Result.Contents = Memory;
          Result.ContentSize = FileSize;
        } else {
          DEBUGPlatformFreeFileMemory(Context, Memory);
        }
      }
    }
    close(fd);
  }
  return Result;
}

DEBUG_PLATFORM_WRITE_ENTIRE_FILE(DEBUGPlatformWriteEntireFile) {
  bool Result = false;
  int fd = open(Filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd != -1) {
    ssize_t BytesWritten = write(fd, Memory, MemorySize);
    if (BytesWritten == (ssize_t)MemorySize) {
      Result = true;
    }
    close(fd);
  }
  return Result;
}
void LinuxUnloadGame(linux_game *Game) {
  if (Game->GameDLL) {
    dlclose(Game->GameDLL);
    free(Game->FullDllPath);
    Game->GameDLL = NULL;
  }
  Game->GameUpdateAndRender = GameUpdateAndRenderNoop;
  Game->GameGetSoundSamples = GameGetSoundSamplesNoop;
  Game->IsValid = false;
}

void LinuxLoadGame(linux_game *Game) {

  Game->IsValid = false;
  Game->GameUpdateAndRender = GameUpdateAndRenderNoop;
  Game->GameGetSoundSamples = GameGetSoundSamplesNoop;
  char exePath[PATH_MAX];
  memset(exePath, 0, sizeof(exePath)); // readlink does not null terminate!
  if (readlink("/proc/self/exe", exePath, PATH_MAX) != -1) {
    // Extract directory
    char *lastSlash = strrchr(exePath, '/');
    if (lastSlash != NULL) {
      *lastSlash = '\0'; // terminate at last slash
    }

    // Construct DLL path
    char dllPath[2 * PATH_MAX];
    snprintf(dllPath, sizeof(dllPath), "%s/handmade_game", exePath);
    Game->GameDLL = dlopen(dllPath, RTLD_NOW);

    __time_t LatestWriteTime = LinuxGetLastWriteTime(dllPath);
    if (Game->GameDLL) {
      Game->GameUpdateAndRender =
          (game_update_and_render *)dlsym(Game->GameDLL, "GameUpdateAndRender");
      Game->GameGetSoundSamples =
          (game_get_sound_samples *)dlsym(Game->GameDLL, "GameGetSoundSamples");
      Game->IsValid = true;
      Game->FullDllPath = strdup(dllPath);
      Game->LatestWriteTime = LatestWriteTime;
    }
  } else {
    printf("%s\n", exePath);
  }
}

void LinuxSetupGameMemory(linux_state *LinuxState, game_memory *GameMemory) {
  uint32 RenderBufferLength = 240000;
  memory_index RenderBufferSize = RenderBufferLength * sizeof(render_command);
  uint32 WorkQueueLength = 128;
  size_t WorkQueueSize = WorkQueueLength * sizeof(linux_work_queue_task);
  GameMemory->Initialized = false;
  GameMemory->PermanentStorageSize = Megabytes(100);
  GameMemory->TransientStorageSize = Megabytes(100);
  LinuxState->TotalMemorySize = GameMemory->TransientStorageSize +
                                GameMemory->PermanentStorageSize +
                                RenderBufferSize + WorkQueueSize;
  LinuxState->GameMemoryBlock =
      mmap(NULL, LinuxState->TotalMemorySize, PROT_READ | PROT_WRITE,
           MAP_ANON | MAP_PRIVATE, -1, 0);
  GameMemory->PermanentStorage = (uint8 *)LinuxState->GameMemoryBlock;
  GameMemory->TransientStorage =
      (uint8 *)GameMemory->PermanentStorage + GameMemory->PermanentStorageSize;
  InitializeRenderBuffer(&LinuxState->RenderBuffer, RenderBufferLength,
                         (render_command *)(GameMemory->PermanentStorage +
                                            GameMemory->PermanentStorageSize +
                                            GameMemory->TransientStorageSize));
  InitializeWorkQueue(
      &LinuxState->WorkQueue, WorkQueueLength,
      (linux_work_queue_task *)(GameMemory->PermanentStorage +
                                GameMemory->PermanentStorageSize +
                                GameMemory->TransientStorageSize +
                                RenderBufferSize));

  GameMemory->TaskQueue = &LinuxState->WorkQueue;
  GameMemory->PlatformPushTaskToQueue = PushTaskToQueue; //&PushTaskToQueue;
  GameMemory->PlatformWaitForQueueToFinish =
      WaitForQueueToFinish; //&WaitForQueueToFinish;
  GameMemory->DebugPlatformReadEntireFile = &DEBUGPlatformReadEntireFile;
  GameMemory->DebugPlatformFreeFileMemory = &DEBUGPlatformFreeFileMemory;
  GameMemory->DebugPlatformWriteEntireFile = &DEBUGPlatformWriteEntireFile;
}

static GLuint compile_shader(GLenum type, const char *src) {
  GLuint s = glCreateShader(type);
  glShaderSource(s, 1, &src, NULL);
  glCompileShader(s);
  GLint ok;
  glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    char log[512];
    glGetShaderInfoLog(s, sizeof(log), NULL, log);
    fprintf(stderr, "Shader error: %s\n", log);
    exit(1);
  }
  return s;
}

void *audio_thread(void *arg) {
  linux_state *app = (linux_state *)arg;
  short buffer[2048] = {0};
  while (app->Running) {

    game_sound_output_buffer GameSoundBuffer = {};
    GameSoundBuffer.SamplesPerSecond = 44100;
    GameSoundBuffer.SampleCount = 1024; // SoundBuffer.SamplesPerSecond / 30;
    GameSoundBuffer.Samples = buffer;

    thread_context Context = {0};

    app->Game.GameGetSoundSamples(&Context, &app->GameMemory, &GameSoundBuffer);

    int rc = snd_pcm_writei(app->PCM, buffer, 1024);
    if (rc == -EPIPE) {
      snd_pcm_prepare(app->PCM);
      printf("undederrun\n");
    } else if (rc < 0) {
      fprintf(stderr, "Error writing to PCM device: %s\n", snd_strerror(rc));
    }
  }
  return 0;
}

bool quited = false;

void on_delete(Display *display, Window window) {
  XDestroyWindow(display, window);
  quited = true;
}

int main() {
  linux_state LinuxState = {};
  LinuxState.Running = true;
  LinuxState.WindowWidth = 1200;
  LinuxState.WindowHeight = 800;
  LinuxLoadGame(&LinuxState.Game);

  LinuxSetupGameMemory(&LinuxState, &LinuxState.GameMemory);
  snd_pcm_t *pcm;
  snd_pcm_hw_params_t *params;
  int err;

  snd_pcm_open(&pcm, "default", SND_PCM_STREAM_PLAYBACK, 0);
  snd_pcm_hw_params_malloc(&params);
  snd_pcm_hw_params_any(pcm, params);
  snd_pcm_hw_params_set_access(pcm, params, SND_PCM_ACCESS_RW_INTERLEAVED);
  snd_pcm_hw_params_set_format(pcm, params, SND_PCM_FORMAT_S16_LE);
  snd_pcm_hw_params_set_channels(pcm, params, 2);
  snd_pcm_hw_params_set_rate(pcm, params, 44100, 0);
  snd_pcm_hw_params_set_buffer_size(pcm, params, 2048);
  snd_pcm_hw_params_set_period_size(pcm, params, 256, 0);
  snd_pcm_hw_params(pcm, params);
  snd_pcm_hw_params_free(params);
  snd_pcm_prepare(pcm);

  LinuxState.PCM = pcm;
  pthread_t thread;
  pthread_create(&thread, NULL, audio_thread, &LinuxState);

  LinuxState.AudioThread = thread;

  Display *display = XOpenDisplay(NULL);
  if (NULL == display) {
    fprintf(stderr, "Failed to initialize display");
    return EXIT_FAILURE;
  }

  Window root = DefaultRootWindow(display);
  if (None == root) {
    fprintf(stderr, "No root window found");
    XCloseDisplay(display);
    return EXIT_FAILURE;
  }

  Window window =
      XCreateSimpleWindow(display, root, 0, 0, 800, 600, 0, 0, 0xffffffff);
  if (None == window) {
    fprintf(stderr, "Failed to create window");
    XCloseDisplay(display);
    return EXIT_FAILURE;
  }

  XMapWindow(display, window);
  XFlush(display);

  Atom wm_delete_window = XInternAtom(display, "WM_DELETE_WINDOW", False);
  XSetWMProtocols(display, window, &wm_delete_window, 1);

  XSelectInput(display, window, ExposureMask | KeyPressMask);
  GC gc = XCreateGC(display, window, 0, NULL);
  if (gc == NULL) {
    fprintf(stderr, "Faied to create GC");
    XCloseDisplay(display);
    return EXIT_FAILURE;
  }

  int screen = DefaultScreen(display);
  int black = XBlackPixel(display, screen);
  int white = XWhitePixel(display, screen);

  XColor fgColor;
  Colormap colormap;

  // Get the default colormap
  colormap = DefaultColormap(display, screen);
  fgColor.red = 0x3333;
  fgColor.green = 0xcccc;
  fgColor.blue = 0xdddd;

  // Allocate the color by name (e.g., "red")
  if (!XAllocColor(display, colormap, &fgColor)) {
    fprintf(stderr, "Failed to allocate color\n");
    return EXIT_FAILURE;
  }
  XEvent event;
  while (!quited) {
    XNextEvent(display, &event);

    switch (event.type) {
    case ClientMessage: {
      if (event.xclient.data.l[0] == (long int)wm_delete_window) {
        on_delete(event.xclient.display, event.xclient.window);
      }
    } break;
    case KeyPress: {
      /* exit on ESC key press */
      KeySym keysym = XLookupKeysym((XKeyEvent *)&event, 0);
      switch (keysym) {
      case XK_Escape: {
        on_delete(event.xclient.display, event.xclient.window);
      } break;
      case XK_Left: {
      } break;
      case XK_Right: {
      } break;
      case XK_Up: {
      } break;
      case XK_Down: {
      } break;
      }
    }
    case Expose: {
      XClearWindow(display, window);
      XSetForeground(display, gc, fgColor.pixel);
      XFillRectangle(display, window, gc, 100, 100, 100, 100);
      XFlush(display);
    } break;
    }
  }

  XFreeGC(display, gc);
  // XDestroyWindow(display, window);
  XCloseDisplay(display);

  return 0;
}
