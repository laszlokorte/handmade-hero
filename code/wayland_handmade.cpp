#include "wayland_handmade.h"
#include "handmade.h"
#include <GLES2/gl2.h>
#include <cstdint>
#include <wayland-client-protocol.h>
#include <xkbcommon/xkbcommon-keysyms.h>

struct wl_compositor *comp;
struct wl_surface *surface;
struct wl_buffer *bfr;
struct xdg_wm_base *sh;
struct xdg_toplevel *surface_top_level;
struct wl_seat *seat;
struct wl_keyboard *kb;
uint8_t cls;

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

void *audio_thread(void *arg) {
  linux_state *app = (linux_state *)arg;
  short buffer[2048];
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

  glEnable(GL_BLEND);
  glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE,
                      GL_ONE_MINUS_SRC_ALPHA);
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

  glBindBuffer(GL_ARRAY_BUFFER, app->GLState.vertexBufferIndex);
  glBufferData(GL_ARRAY_BUFFER, sizeof(gl_vertex) * app->GLState.Vertices.Count,
               app->GLState.Vertices.Buffer, GL_STATIC_DRAW);

  glViewport(0, 0, app->WindowWidth, app->WindowHeight);
  glClearColor(0.1f, 0.1f, 0.1f, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  m33 ViewMatrix = {};
  m33 OpMatrix = {};
  m3MakeIdentity(&ViewMatrix);
  m3makeScale(&OpMatrix, 2.0f / app->RenderBuffer.Viewport.Width,
              -2.0f / app->RenderBuffer.Viewport.Height);
  m3Mul(&OpMatrix, &ViewMatrix, &ViewMatrix);
  m3makeTranslate(&OpMatrix, app->RenderBuffer.Viewport.Width / -2.0f,
                  app->RenderBuffer.Viewport.Height / -2.0);

  m3Mul(&OpMatrix, &ViewMatrix, &ViewMatrix);
  glUniformMatrix3fv(app->GLState.Uniforms.ViewMatrix, 1, false,
                     ViewMatrix.entries);
  glDrawArrays(GL_TRIANGLES, 0, app->GLState.Vertices.Count);
  eglSwapBuffers(app->EglDisplay, app->EglSurface);
  wl_surface_commit(app->Surface);
  wl_display_flush(app->Display);
  app->CurrentGameInputIndex = (app->CurrentGameInputIndex + 1) % 2;

  game_input *NewInput = &app->GameInputs[app->CurrentGameInputIndex];
  game_input *OldInput = &app->GameInputs[1 - app->CurrentGameInputIndex];

  game_controller_input *KeyBoardController = &NewInput->Controllers[0];
  game_controller_input *OldKeyBoardController = &OldInput->Controllers[0];

  game_mouse_input *Mouse = &NewInput->Mouse;
  game_mouse_input *OldMouse = &OldInput->Mouse;

  game_controller_input reset_controller = {};
  game_mouse_input reset_mouse = {};

  reset_mouse.MouseX = OldMouse->MouseX;
  reset_mouse.MouseY = OldMouse->MouseY;
  for (size_t b = 0; b < ArrayCount(reset_mouse.Buttons); b++) {
    reset_mouse.Buttons[b].EndedDown = OldMouse->Buttons[b].EndedDown;
  }

  for (size_t b = 0; b < ArrayCount(reset_controller.Buttons); b++) {
    reset_controller.Buttons[b].EndedDown =
        OldKeyBoardController->Buttons[b].EndedDown;
  }

  *KeyBoardController = reset_controller;
  *Mouse = reset_mouse;
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
            uint32_t size) {

  struct linux_state *app = (linux_state *)data;

  // 1. Read the keymap string from fd
  char *keymap_string = (char *)malloc(size + 1);
  if (!keymap_string)
    return;

  ssize_t n = read(fd, keymap_string, size);
  close(fd);
  if (n != size) {
    fprintf(stderr, "Failed to read keymap from fd\n");
    free(keymap_string);
    return;
  }
  keymap_string[size] = '\0'; // null-terminate

  struct xkb_context *ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);

  app->XKbdContext = ctx;
  //
  // 2. Create a keymap (e.g., from string from wl_keyboard.keymap event)
  struct xkb_keymap *keymap =
      xkb_keymap_new_from_string(ctx, keymap_string, XKB_KEYMAP_FORMAT_TEXT_V1,
                                 XKB_KEYMAP_COMPILE_NO_FLAGS);

  if (!keymap) {
    fprintf(stderr, "Failed to create XKB keymap\n");
    xkb_context_unref(ctx);
    free(keymap_string);
    return;
  }
  app->XKbdKeyMap = keymap;
  free(keymap_string);

  // 3. Create the state from the keymap
  app->XKbdState = xkb_state_new(keymap);
}

void kb_enter(void *data, struct wl_keyboard *kb, uint32_t ser,
              struct wl_surface *surface, struct wl_array *keys) {}

void kb_leave(void *data, struct wl_keyboard *kb, uint32_t ser,
              struct wl_surface *surface) {}

void kb_key(void *data, struct wl_keyboard *kb, uint32_t ser, uint32_t t,
            uint32_t key, uint32_t stat) {
  struct linux_state *app = (linux_state *)data;

  game_input *NewInput = &app->GameInputs[app->CurrentGameInputIndex];
  game_input *OldInput = &app->GameInputs[1 - app->CurrentGameInputIndex];

  game_controller_input *KeyBoardController = &NewInput->Controllers[0];
  game_controller_input *OldKeyBoardController = &OldInput->Controllers[0];

  bool IsDown = stat == WL_KEYBOARD_KEY_STATE_PRESSED;
  bool WasDown = stat == !IsDown;
  bool AltIsDown = false;
  xkb_keysym_t sym = xkb_state_key_get_one_sym(app->XKbdState, key + 8);
  if (sym == XKB_KEY_q || sym == XKB_KEY_Q) {
    KeyBoardController->LeftShoulder.EndedDown = IsDown;
    KeyBoardController->LeftShoulder.HalfTransitionCount +=
        IsDown != WasDown ? 1 : 0;
  }
  if (sym == XKB_KEY_e || sym == XKB_KEY_E) {
    KeyBoardController->RightShoulder.EndedDown = IsDown;
    KeyBoardController->RightShoulder.HalfTransitionCount +=
        IsDown != WasDown ? 1 : 0;
  }
  if (sym == XKB_KEY_w || sym == XKB_KEY_W) {
    KeyBoardController->MoveUp.EndedDown = IsDown;
    KeyBoardController->MoveUp.HalfTransitionCount += IsDown != WasDown ? 1 : 0;
  }
  if (sym == XKB_KEY_a || sym == XKB_KEY_A) {
    KeyBoardController->MoveLeft.EndedDown = IsDown;
    KeyBoardController->MoveLeft.HalfTransitionCount +=
        IsDown != WasDown ? 1 : 0;
  }
  if (sym == XKB_KEY_s || sym == XKB_KEY_S) {
    KeyBoardController->MoveDown.EndedDown = IsDown;
    KeyBoardController->MoveDown.HalfTransitionCount +=
        IsDown != WasDown ? 1 : 0;
  }
  if (sym == XKB_KEY_d || sym == XKB_KEY_D) {
    KeyBoardController->MoveRight.EndedDown = IsDown;
    KeyBoardController->MoveRight.HalfTransitionCount +=
        IsDown != WasDown ? 1 : 0;
  }
  if (sym == XKB_KEY_Up) {
    KeyBoardController->ActionUp.EndedDown = IsDown;
    KeyBoardController->ActionUp.HalfTransitionCount +=
        IsDown != WasDown ? 1 : 0;
  }
  if (sym == XKB_KEY_Down) {
    KeyBoardController->ActionDown.EndedDown = IsDown;
    KeyBoardController->ActionDown.HalfTransitionCount +=
        IsDown != WasDown ? 1 : 0;
  }
  if (sym == XKB_KEY_Left) {
    KeyBoardController->ActionLeft.EndedDown = IsDown;
    KeyBoardController->ActionLeft.HalfTransitionCount +=
        IsDown != WasDown ? 1 : 0;
  }
  if (sym == XKB_KEY_Right) {
    KeyBoardController->ActionRight.EndedDown = IsDown;
    KeyBoardController->ActionRight.HalfTransitionCount +=
        IsDown != WasDown ? 1 : 0;
  }
  if (sym == XKB_KEY_Escape) {
    KeyBoardController->Menu.EndedDown = IsDown;
    KeyBoardController->Menu.HalfTransitionCount += IsDown != WasDown ? 1 : 0;
  }
  if (sym == XKB_KEY_BackSpace) {
    KeyBoardController->Back.EndedDown = IsDown;
    KeyBoardController->Back.HalfTransitionCount += IsDown != WasDown ? 1 : 0;
  }
}

void kb_mod(void *data, struct wl_keyboard *keyboard, uint32_t serial,
            uint32_t mods_depressed, uint32_t mods_latched,
            uint32_t mods_locked, uint32_t group) {
  struct linux_state *app = (linux_state *)data;

  xkb_state_update_mask(app->XKbdState, mods_depressed, mods_latched,
                        mods_locked, 0, 0, group);
}

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
    wl_keyboard_add_listener(kb, &kb_list, data);
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
    wl_seat_add_listener(seat, &seat_list, data);
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

  struct wl_display *display = wl_display_connect(0);
  if (!display) {
    fprintf(stderr, "failed to connect to Wayland display\n");
    exit(1);
  }
  LinuxState.Display = display;
  struct wl_registry *reg = wl_display_get_registry(display);
  wl_registry_add_listener(reg, &registry_listener, &LinuxState);
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
  LinuxState.GLState.Uniforms.ViewMatrix =
      glGetUniformLocation(prog, "viewMatrix");
  // printf("%d\n", LinuxState.GLState.Uniforms.ViewMatrix);
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
