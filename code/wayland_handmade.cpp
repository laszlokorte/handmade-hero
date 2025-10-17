#include "wayland_handmade.h"
#include <cstdint>

struct wl_compositor *comp;
struct wl_surface *surface;
struct wl_buffer *bfr;
struct xdg_wm_base *sh;
struct xdg_toplevel *surface_top_level;
struct wl_seat *seat;
struct wl_keyboard *kb;
uint8_t cls;

static void GlPushQuad(gl_vertices *Vertices, float x0, float y0, float x1,
                       float y1, float r, float g, float b, float a) {
  if (Vertices->Capacity < Vertices->Count + 6) {
    // printf("%lu\n", Vertices->Count);
    // printf("%lu\n", Vertices->Capacity);
    return;
  }
  Vertices->Buffer[(Vertices->Count)++] = {{x0, y0}, {r, g, b, a}};
  Vertices->Buffer[(Vertices->Count)++] = {{x1, y0}, {r, g, b, a}};
  Vertices->Buffer[(Vertices->Count)++] = {{x0, y1}, {r, g, b, a}};
  Vertices->Buffer[(Vertices->Count)++] = {{x1, y0}, {r, g, b, a}};
  Vertices->Buffer[(Vertices->Count)++] = {{x1, y1}, {r, g, b, a}};
  Vertices->Buffer[(Vertices->Count)++] = {{x0, y1}, {r, g, b, a}};
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
  // InitializeWorkQueue(
  //     &LinuxState->WorkQueue, WorkQueueLength,
  //     (win32_work_queue_task *)(GameMemory->PermanentStorage +
  //                               GameMemory->PermanentStorageSize +
  //                               GameMemory->TransientStorageSize +
  //                               RenderBufferSize));

  GameMemory->TaskQueue = &LinuxState->WorkQueue;
  GameMemory->PlatformPushTaskToQueue = PushTaskToQueueNoop; //&PushTaskToQueue;
  GameMemory->PlatformWaitForQueueToFinish =
      WaitForQueueToFinishNoop; //&WaitForQueueToFinish;
  GameMemory->DebugPlatformReadEntireFile =
      PlatformReadEntireFileNoop; //&DEBUGPlatformReadEntireFile;
  GameMemory->DebugPlatformFreeFileMemory =
      PlatformFreeFileNoop; //&DEBUGPlatformFreeFileMemory;
  GameMemory->DebugPlatformWriteEntireFile =
      PlatformWriteEntireFileNoop; //&DEBUGPlatformWriteEntireFile;
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

typedef struct player {
  int32_t PosX;
  int32_t PosY;
} player;

struct player Player = {
    .PosX = 0,
    .PosY = 0,
};
static struct wl_callback_listener cb_list = {.done = frame_new};
void frame_new(void *data, struct wl_callback *cb, uint32_t a) {
  struct linux_state *app = (linux_state *)data;

  wl_callback_destroy(cb);
  cb = wl_surface_frame(app->Surface);
  wl_callback_add_listener(cb, &cb_list, app);
  thread_context Context = {};
  ClearRenderBuffer(&app->RenderBuffer, app->WindowWidth, app->WindowHeight);
  app->GameInputs[app->CurrentGameInputIndex].DeltaTime = 0.016;

  app->Game.GameUpdateAndRender(&Context, &app->GameMemory,
                                &app->GameInputs[app->CurrentGameInputIndex],
                                &app->RenderBuffer);
  app->GLState.Vertices.Count = 0;
  for (uint32 ri = 0; ri < app->RenderBuffer.Count; ri += 1) {

    render_command *RCmd = &app->RenderBuffer.Base[ri];
    switch (RCmd->Type) {
    case RenderCommandRect: {
      // glColor4f(0.0f,0.0f,0.0f,0.0f);
      if (RCmd->Rect.Image) {
        GlPushQuad(&app->GLState.Vertices, RCmd->Rect.MinX, RCmd->Rect.MinY,
                   RCmd->Rect.MaxX, RCmd->Rect.MaxY, RCmd->Rect.Color.Red,
                   RCmd->Rect.Color.Green, RCmd->Rect.Color.Blue, 0.3f);
      } else {
        GlPushQuad(&app->GLState.Vertices, RCmd->Rect.MinX, RCmd->Rect.MinY,
                   RCmd->Rect.MaxX, RCmd->Rect.MaxY, RCmd->Rect.Color.Red,
                   RCmd->Rect.Color.Green, RCmd->Rect.Color.Blue,
                   RCmd->Rect.Color.Alpha);
      }

    } break;
    case RenderCommandTriangle: {
      // glColor4f(RCmd->Triangle.Color.Red, RCmd->Triangle.Color.Green,
      //           RCmd->Triangle.Color.Blue, RCmd->Triangle.Color.Alpha);

      // glBegin(GL_TRIANGLES);
      // glVertex2f(RCmd->Triangle.AX, RCmd->Triangle.AY);

      // glVertex2f(RCmd->Triangle.BX, RCmd->Triangle.BY);

      // glVertex2f(RCmd->Triangle.CX, RCmd->Triangle.CY);
      // glEnd();
      // glDisable(GL_TEXTURE_2D);
    } break;
    default: {
      break;
    } break;
    }
  }
  GlPushQuad(&app->GLState.Vertices, -0.5, -0.5, 0.5, 0.5, 1.0, 1.0, 0.0, 0.0);

  glBindBuffer(GL_ARRAY_BUFFER, app->GLState.vertexBufferIndex);
  glBufferData(GL_ARRAY_BUFFER, sizeof(gl_vertex) * app->GLState.Vertices.Count,
               app->GLState.Vertices.Buffer, GL_STATIC_DRAW);

  glViewport(0, 0, app->WindowWidth, app->WindowHeight);
  glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  glUniform2f(app->GLState.uniformOffset, Player.PosX / 100.0,
              -Player.PosY / 100.0);
  glDrawArrays(GL_TRIANGLES, 0, app->GLState.Vertices.Count);
  eglSwapBuffers(app->EglDisplay, app->EglSurface);
  wl_surface_commit(app->Surface);
  wl_display_flush(app->Display);
}

void xrfc_conf(void *data, struct xdg_surface *xrfc, uint32_t ser) {
  struct linux_state *app = (linux_state *)data;
  xdg_surface_ack_configure(xrfc, ser);

  wl_egl_window_resize(app->EglWindow, app->WindowWidth, app->WindowHeight, 0,
                       0);
  glViewport(0, 0, app->WindowWidth, app->WindowHeight);

  if (!app->Configured) {
    app->Configured = true;

    // Draw first frame and start frame loop
    struct wl_callback *cb = wl_surface_frame(surface);
    wl_callback_add_listener(cb, &cb_list, app);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    eglSwapBuffers(app->EglDisplay, app->EglSurface);
    wl_surface_commit(app->Surface);
    wl_display_flush(app->Display);
  }
}

struct xdg_surface_listener xrfc_list = {.configure = xrfc_conf};

void toplevel_configuration(void *data, struct xdg_toplevel *surface_top_level,
                            int32_t nw, int32_t nh, struct wl_array *stat) {
  struct linux_state *app = (linux_state *)data;

  if (nw > 0 && nh > 0) {
    app->WindowWidth = nw;
    app->WindowHeight = nh;
    // If you have an EGL window, resize it
    if (app->EglWindow) {
      wl_egl_window_resize(app->EglWindow, app->WindowWidth, app->WindowHeight,
                           0, 0);
      glViewport(0, 0, app->WindowWidth, app->WindowHeight);
    }
  }
}

void toplevel_close(void *data, struct xdg_toplevel *surface_top_level) {
  cls = 1;
}

struct xdg_toplevel_listener toplevel_listener = {
    .configure = toplevel_configuration, .close = toplevel_close};

void sh_ping(void *data, struct xdg_wm_base *sh, uint32_t ser) {
  xdg_wm_base_pong(sh, ser);
}

struct xdg_wm_base_listener sh_list = {.ping = sh_ping};

void kb_map(void *data, struct wl_keyboard *kb, uint32_t frmt, int32_t fd,
            uint32_t sz) {}

void kb_enter(void *data, struct wl_keyboard *kb, uint32_t ser,
              struct wl_surface *surface, struct wl_array *keys) {}

void kb_leave(void *data, struct wl_keyboard *kb, uint32_t ser,
              struct wl_surface *surface) {}

void kb_key(void *data, struct wl_keyboard *kb, uint32_t ser, uint32_t t,
            uint32_t key, uint32_t stat) {
  if (stat == 1) {

    if (key == 1 || key == 16) {
      cls = 1;
    } else if (key == 30) {
      Player.PosX -= 10;
      // printf("a\n");
    } else if (key == 17) {
      Player.PosY -= 10;
      // printf("w\n");
    } else if (key == 31) {
      Player.PosY += 10;
      // printf("s\n");
    } else if (key == 32) {
      Player.PosX += 10;
      // printf("d\n");
    } else {
      printf("%d\n", key);
    }
  }
}

void kb_mod(void *data, struct wl_keyboard *kb, uint32_t ser, uint32_t dep,
            uint32_t lat, uint32_t lock, uint32_t grp) {}

void kb_rep(void *data, struct wl_keyboard *kb, int32_t rate, int32_t del) {}

struct wl_keyboard_listener kb_list = {.keymap = kb_map,
                                       .enter = kb_enter,
                                       .leave = kb_leave,
                                       .key = kb_key,
                                       .modifiers = kb_mod,
                                       .repeat_info = kb_rep};

void seat_cap(void *data, struct wl_seat *seat, uint32_t cap) {
  if (cap & WL_SEAT_CAPABILITY_KEYBOARD && !kb) {
    kb = wl_seat_get_keyboard(seat);
    wl_keyboard_add_listener(kb, &kb_list, 0);
  }
}

void seat_name(void *data, struct wl_seat *seat, const char *name) {}

struct wl_seat_listener seat_list = {.capabilities = seat_cap,
                                     .name = seat_name};

void registry_global(void *data, struct wl_registry *reg, uint32_t name,
                     const char *intf, uint32_t v) {
  if (!strcmp(intf, wl_compositor_interface.name)) {
    comp = (wl_compositor *)wl_registry_bind(reg, name,
                                             &wl_compositor_interface, 4);
  } else if (!strcmp(intf, xdg_wm_base_interface.name)) {
    sh = (xdg_wm_base *)wl_registry_bind(reg, name, &xdg_wm_base_interface, 1);
    xdg_wm_base_add_listener(sh, &sh_list, 0);
  } else if (!strcmp(intf, wl_seat_interface.name)) {
    seat = (wl_seat *)wl_registry_bind(reg, name, &wl_seat_interface, 1);
    wl_seat_add_listener(seat, &seat_list, 0);
  }
}

void registry_global_remove(void *data, struct wl_registry *reg,
                            uint32_t name) {}

struct wl_registry_listener registry_listener = {
    .global = registry_global, .global_remove = registry_global_remove};

int main() {
  linux_state LinuxState = {};
  LinuxState.Running = true;
  LinuxState.WindowWidth = 1200;
  LinuxState.WindowHeight = 800;
  LinuxLoadGame(&LinuxState.Game);

  LinuxSetupGameMemory(&LinuxState, &LinuxState.GameMemory);

  struct wl_display *display = wl_display_connect(0);
  if (!display) {
    fprintf(stderr, "failed to connect to Wayland display\n");
    exit(1);
  }
  LinuxState.Display = display;
  struct wl_registry *reg = wl_display_get_registry(display);
  wl_registry_add_listener(reg, &registry_listener, 0);
  wl_display_roundtrip(display);

  surface = wl_compositor_create_surface(comp);
  if (!surface) {
    fprintf(stderr, "failed to create surface\n");
    exit(1);
  }
  LinuxState.Surface = surface;
  // struct wl_callback *cb = wl_surface_frame(surface);
  // wl_callback_add_listener(cb, &cb_list, 0);

  struct xdg_surface *xrfc = xdg_wm_base_get_xdg_surface(sh, surface);
  xdg_surface_add_listener(xrfc, &xrfc_list, &LinuxState);
  surface_top_level = xdg_surface_get_toplevel(xrfc);
  xdg_toplevel_add_listener(surface_top_level, &toplevel_listener, &LinuxState);
  xdg_toplevel_set_title(surface_top_level, "wayland client");
  wl_surface_commit(surface);

  /* EGL setup */
  EGLDisplay egl_display = eglGetDisplay((EGLNativeDisplayType)display);
  if (egl_display == EGL_NO_DISPLAY) {
    fprintf(stderr, "No EGL display\n");
    return 1;
  }
  if (!eglInitialize(egl_display, NULL, NULL)) {
    fprintf(stderr, "eglInit failed\n");
    return 1;
  }

  LinuxState.EglDisplay = egl_display;
  EGLint attr[] = {EGL_RED_SIZE,
                   8,
                   EGL_GREEN_SIZE,
                   8,
                   EGL_BLUE_SIZE,
                   8,
                   EGL_ALPHA_SIZE,
                   8,
                   EGL_RENDERABLE_TYPE,
                   EGL_OPENGL_ES2_BIT,
                   EGL_NONE};
  EGLConfig config;
  EGLint ncfg;
  eglChooseConfig(egl_display, attr, &config, 1, &ncfg);

  EGLint ctxattr[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
  EGLContext egl_context =
      eglCreateContext(egl_display, config, EGL_NO_CONTEXT, ctxattr);
  LinuxState.EglContext = egl_context;

  /* create wl_egl_window and egl window surface */
  struct wl_egl_window *egl_window = wl_egl_window_create(
      surface, LinuxState.WindowWidth, LinuxState.WindowHeight);
  if (!egl_window) {
    fprintf(stderr, "wl_egl_window_create failed\n");
    return 1;
  }
  LinuxState.EglWindow = egl_window;
  EGLSurface egl_surface = eglCreateWindowSurface(
      egl_display, config, (EGLNativeWindowType)egl_window, NULL);
  eglMakeCurrent(egl_display, egl_surface, egl_surface, egl_context);

  LinuxState.EglSurface = egl_surface;
  /* GL setup: program + buffers */
  GLuint vs = compile_shader(GL_VERTEX_SHADER, VertexShaderSource);
  GLuint fs = compile_shader(GL_FRAGMENT_SHADER, FragmentShaderSource);
  GLuint prog = glCreateProgram();
  glAttachShader(prog, vs);
  glAttachShader(prog, fs);
  glBindAttribLocation(prog, 0, "pos");
  glBindAttribLocation(prog, 1, "color");
  glLinkProgram(prog);
  glUseProgram(prog);

  gl_vertex Verts[1000];
  LinuxState.GLState.Vertices.Buffer = Verts;
  LinuxState.GLState.Vertices.Capacity = 1000;

  // printf("%lu\n", LinuxState.GLState.Vertices.Count);
  // printf("%lu\n", sizeof(((gl_vertex){0}).pos));
  LinuxState.GLState.uniformOffset = glGetUniformLocation(prog, "offset");
  glGenBuffers(1, &LinuxState.GLState.vertexBufferIndex);

  glBindBuffer(GL_ARRAY_BUFFER, LinuxState.GLState.vertexBufferIndex);
  glBufferData(GL_ARRAY_BUFFER,
               sizeof(gl_vertex) * LinuxState.GLState.Vertices.Count,
               LinuxState.GLState.Vertices.Buffer, GL_STATIC_DRAW);
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(gl_vertex), 0);
  glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(gl_vertex),
                        (const GLvoid *)sizeof(((gl_vertex){0}).pos));

  while (wl_display_dispatch(display)) {
    if (cls)
      break;
  }
  /*
          if (kb) {
                  wl_keyboard_destroy(kb);
          }
          wl_seat_release(seat);
          if (bfr) {
                  wl_buffer_destroy(bfr);
          }
          wl_egl_window_destroy(egl_window);
          eglDestroySurface(egl_display, egl_surface);
          eglDestroyContext(egl_display, egl_context);
          eglTerminate(egl_display);
          xdg_toplevel_destroy(surface_top_level);
          xdg_surface_destroy(xrfc);
          wl_surface_destroy(surface);
          wl_display_disconnect(display);
    */
  return 0;
}
