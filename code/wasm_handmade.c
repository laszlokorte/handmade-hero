#include "wasm_handmade.h"
#include "handmade.h"
#include "renderer.h"
#include "work_queue.h"

float audio_buffer[128 * 2];

unsigned char stable_memory[2000000];

void mock() {}
debug_read_file_result ReadEntireFile(thread_context *Context, char *Filename) {
  debug_read_file_result r = {};
  r.ContentSize = 0;

  return r;
}
int DebugPlatformLog(const char *__restrict __format, ...) { return 0; }
void WaitForQueueToFinish(struct work_queue *Queue) {}
struct wasm_state state = {
    .Running = false,
    .Configured = false,
    .Game =
        {
            .GameGetSoundSamples = &GameGetSoundSamples,
            .GameUpdateAndRender = &GameUpdateAndRender,
        },

    .GameInputs = {},
    .CurrentGameInputIndex = 0,
    .GameMemory =
        {
            .Initialized = false,
            .PermanentStorageSize = 10000,
            .PermanentStorage = &stable_memory[0],

            .TransientStorageSize = 10000,
            .TransientStorage = &stable_memory[10000],

            .DebugPlatformFreeFileMemory = &mock,
            .DebugPlatformReadEntireFile = &ReadEntireFile,
            .DebugPlatformWriteEntireFile =
                (debug_platform_write_entire_file *)&mock,
            .DebugPlatformLog = &DebugPlatformLog,

            .TaskQueue = 0,
            .PlatformPushTaskToQueue = &mock,
            .PlatformWaitForQueueToFinish = &WaitForQueueToFinish,
        },

    .TotalMemorySize = sizeof(stable_memory),
    .GameMemoryBlock = &stable_memory,
    .RenderBuffer = {.Base = (render_command *)&stable_memory[20000],
                     .Count = 0,
                     .Size = 10000,
                     .Viewport =
                         {
                             .Width = 800,
                             .Height = 600,
                         }

    },
};

int setup() {
  state.Game.GameGetSoundSamples = &GameGetSoundSamples;
  state.Game.GameUpdateAndRender = &GameUpdateAndRender;
  return sizeof(render_command) / sizeof(float);
}

int update_and_render() {
  thread_context ctx = {};
  state.GameInputs[state.CurrentGameInputIndex].Mouse.InRange = true;
  state.GameInputs[state.CurrentGameInputIndex].Mouse.MouseX = 100;
  state.GameInputs[state.CurrentGameInputIndex].Mouse.MouseY = 50;
  //  state.GameInputs[state.CurrentGameInputIndex].Mouse.Left.HalfTransitionCount
  //  =
  //   1;
  //  state.GameInputs[state.CurrentGameInputIndex].Mouse.Left.EndedDown =
  //     state.CurrentGameInputIndex;
  state.GameInputs[state.CurrentGameInputIndex].Mouse.WheelX = 0.2;
  state.GameInputs[state.CurrentGameInputIndex].Mouse.WheelY = 0.1;
  state.RenderBuffer.Count = 0;
  state.Game.GameUpdateAndRender(&ctx, &state.GameMemory,
                                 &state.GameInputs[state.CurrentGameInputIndex],
                                 &state.RenderBuffer

  );
  state.CurrentGameInputIndex = 1 - state.CurrentGameInputIndex;
  return state.RenderBuffer.Count;
}

void resize_viewport(int width, int height) {
  state.RenderBuffer.Viewport.Width = width;
  state.RenderBuffer.Viewport.Height = height;
}

render_command *get_render_list() { return state.RenderBuffer.Base; }

float *output_audio(int sample_count) { return audio_buffer; }

void mouse_move(int x, int y) {}
void mouse_button_press(int button) {}
void mouse_button_release(int button) {}

void scroll_wheel(int x, int y) {}

void controller_stick(int controller, int x, int y) {}
void controller_button_press(int controller, int button) {}
void controller_button_release(int controller, int button) {}
