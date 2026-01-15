#include "wasm_handmade.h"
#include "handmade.h"
#include "renderer.h"
#include "work_queue.h"
#include "wasm_work_queue.c"

float audio_output_buffer[128 * 2];

unsigned char stable_memory[2000000];
work_queue global_work_queue = {
    .Size = 100,
    .Base = (wasm_work_queue_task *)&stable_memory[80000 + 80000],

    .NextWrite = 0,
    .NextRead = 0,
    .CompletionGoal = 0,
    .CompletionCount = 0,
};

void mock() {}
DEBUG_PLATFORM_READ_ENTIRE_FILE(ReadEntireFile) {
  debug_read_file_result r = {};
  r.ContentSize = 0;

  return r;
}
DEBUG_PLATFORM_LOG(DebugPlatformLog) { return 0; }

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
            .PermanentStorageSize = 80000,
            .PermanentStorage = &stable_memory[0],

            .TransientStorageSize = 80000,
            .TransientStorage = &stable_memory[0 + 80000],

            .DebugPlatformFreeFileMemory = &mock,
            .DebugPlatformReadEntireFile = &ReadEntireFile,
            .DebugPlatformWriteEntireFile =
                (debug_platform_write_entire_file *)&mock,
            .DebugPlatformLog = &DebugPlatformLog,

            .TaskQueue = &global_work_queue,

            .PlatformPushTaskToQueue = &PushTaskToQueue,
            .PlatformWaitForQueueToFinish = &WaitForQueueToFinish,
        },

    .TotalMemorySize = sizeof(stable_memory),
    .GameMemoryBlock = &stable_memory,
    .RenderBuffer =
        {.Base = (render_command *)&stable_memory[2 * 80000 +
                                                  sizeof(wasm_work_queue_task)],
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

int doWork(int ThreadIndex) {
  wasm_thread_info thread_info = {.LogicalThreadIndex = ThreadIndex,
                                  .Queue = &global_work_queue};
  return WorkQueueThreadProc(&thread_info);
}

int update_and_render(float DeltaTime) {
  thread_context ctx = {};
  state.CurrentGameInputIndex = 1 - state.CurrentGameInputIndex;
  game_input *OldInput = &state.GameInputs[1 - state.CurrentGameInputIndex];
  game_input *CurrentInput = &state.GameInputs[state.CurrentGameInputIndex];
  CurrentInput->Mouse.DeltaX =
      OldInput->Mouse.MouseX - CurrentInput->Mouse.MouseX;
  CurrentInput->Mouse.DeltaY =
      OldInput->Mouse.MouseY - CurrentInput->Mouse.MouseY;
  CurrentInput->Mouse.InRange = OldInput->Mouse.InRange;
  CurrentInput->Mouse.MouseX = OldInput->Mouse.MouseX;
  CurrentInput->Mouse.MouseY = OldInput->Mouse.MouseY;
  for (size_t b = 0; b < ArrayCount(CurrentInput->Mouse.Buttons); b++) {
    CurrentInput->Mouse.Buttons[b].EndedDown =
        OldInput->Mouse.Buttons[b].EndedDown;
    CurrentInput->Mouse.Buttons[b].HalfTransitionCount =
        OldInput->Mouse.Buttons[b].HalfTransitionCount;
  }
  for (size_t c = 0; c < ArrayCount(CurrentInput->Controllers); c++) {
    for (size_t b = 0; b < ArrayCount(CurrentInput->Controllers[c].Buttons);
         b++) {
      CurrentInput->Controllers[c].Buttons[b].HalfTransitionCount =
          OldInput->Controllers[c].Buttons[b].HalfTransitionCount;
      CurrentInput->Controllers[c].Buttons[b].EndedDown =
          OldInput->Controllers[c].Buttons[b].EndedDown;
    }
    CurrentInput->Controllers[c].isAnalog = OldInput->Controllers[c].isAnalog;
    CurrentInput->Controllers[c].AverageStickX =
        OldInput->Controllers[c].AverageStickX;
    CurrentInput->Controllers[c].AverageStickY =
        OldInput->Controllers[c].AverageStickY;
  }
  for (size_t h = 0; h < ArrayCount(CurrentInput->Hands); h++) {
    for (size_t f = 0; f < ArrayCount(CurrentInput->Hands[h].Fingers); f++) {
      CurrentInput->Hands[h].Fingers[f].TipX =
          OldInput->Hands[h].Fingers[f].TipX;
      CurrentInput->Hands[h].Fingers[f].TipY =
          OldInput->Hands[h].Fingers[f].TipY;
      CurrentInput->Hands[h].Fingers[f].Touches =
          OldInput->Hands[h].Fingers[f].Touches;
      CurrentInput->Hands[h].Fingers[f].Radius =
          OldInput->Hands[h].Fingers[f].Radius;
      CurrentInput->Hands[h].Fingers[f].Pressure =
          OldInput->Hands[h].Fingers[f].Pressure;
    }
  }
  state.RenderBuffer.Count = 0;
  CurrentInput->DeltaTime = DeltaTime;
  state.Game.GameUpdateAndRender(&ctx, &state.GameMemory, CurrentInput,
                                 &state.RenderBuffer

  );
  CurrentInput->Mouse.WheelX = 0;
  CurrentInput->Mouse.WheelY = 0;
  CurrentInput->Mouse.DeltaX = 0;
  CurrentInput->Mouse.DeltaY = 0;
  for (size_t b = 0; b < ArrayCount(CurrentInput->Mouse.Buttons); b++) {
    CurrentInput->Mouse.Buttons[b].HalfTransitionCount = 0;
  }
  for (size_t c = 0; c < ArrayCount(CurrentInput->Controllers); c++) {
    for (size_t b = 0; b < ArrayCount(CurrentInput->Controllers[c].Buttons);
         b++) {
      CurrentInput->Controllers[c].Buttons[b].HalfTransitionCount = 0;
    }
  }
  return state.RenderBuffer.Count;
}

void resize_viewport(int width, int height) {
  state.RenderBuffer.Viewport.Width = width;
  state.RenderBuffer.Viewport.Height = height;
}
void resize_viewport_safe(int top, int right, int bottom, int left) {
  state.RenderBuffer.Viewport.Inset.Top = top;
  state.RenderBuffer.Viewport.Inset.Right = right;
  state.RenderBuffer.Viewport.Inset.Bottom = bottom;
  state.RenderBuffer.Viewport.Inset.Left = left;
}

render_command *get_render_list() { return state.RenderBuffer.Base; }

float *output_audio(int sample_count) {

  thread_context ctx = {};
  int16 Memory[sample_count * 2];
  game_sound_output_buffer Buffer = {};
  Buffer.SampleCount = sample_count;
  Buffer.SamplesPerSecond = 44100;
  Buffer.Samples = Memory;
  GameGetSoundSamples(&ctx, &state.GameMemory, &Buffer);

  for (size_t s = 0; s < sample_count * 2; s++) {
    audio_output_buffer[s] = Memory[s] / (float)((1 << 15) - 1);
  }
  return audio_output_buffer;
}

void mouse_move(int x, int y) {
  game_input *CurrentInput = &state.GameInputs[state.CurrentGameInputIndex];
  CurrentInput->Mouse.MouseX = x;
  CurrentInput->Mouse.MouseY = y;
}
void toggle_mouse(bool inRange) {
  game_input *CurrentInput = &state.GameInputs[state.CurrentGameInputIndex];
  CurrentInput->Mouse.InRange = inRange;
}
void mouse_button_press(int button) {
  game_input *CurrentInput = &state.GameInputs[state.CurrentGameInputIndex];
  CurrentInput->Mouse.Buttons[button].HalfTransitionCount += 1;
  CurrentInput->Mouse.Buttons[button].EndedDown = true;
}
void mouse_button_release(int button) {
  game_input *CurrentInput = &state.GameInputs[state.CurrentGameInputIndex];
  CurrentInput->Mouse.Buttons[button].HalfTransitionCount += 1;
  CurrentInput->Mouse.Buttons[button].EndedDown = false;
}

void scroll_wheel(float x, float y) {
  game_input *CurrentInput = &state.GameInputs[state.CurrentGameInputIndex];
  CurrentInput->Mouse.WheelX += x;
  CurrentInput->Mouse.WheelY += y;
}

void touch_finger_begin(int finger, int x, int y) {
  game_finger_input *Finger = &state.GameInputs[state.CurrentGameInputIndex]
                                   .Hands[finger / 2]
                                   .Fingers[finger % 5];
  Finger->TipX = x;
  Finger->TipY = y;
  Finger->Touches = true;
}
void touch_finger_move(int finger, int x, int y) {
  game_finger_input *Finger = &state.GameInputs[state.CurrentGameInputIndex]
                                   .Hands[finger / 2]
                                   .Fingers[finger % 5];
  Finger->TipX = x;
  Finger->TipY = y;
}
void touch_finger_end(int finger) {
  game_finger_input *Finger = &state.GameInputs[state.CurrentGameInputIndex]
                                   .Hands[finger / 2]
                                   .Fingers[finger % 5];
  Finger->Touches = false;
}

void controller_stick(int controller, float x, float y) {
  game_input *CurrentInput = &state.GameInputs[state.CurrentGameInputIndex];
  CurrentInput->Controllers[controller].isAnalog = true;
  CurrentInput->Controllers[controller].AverageStickX = x;
  CurrentInput->Controllers[controller].AverageStickY = y;
}
void controller_button_press(int controller, int button) {
  game_input *CurrentInput = &state.GameInputs[state.CurrentGameInputIndex];
  CurrentInput->Controllers[controller].Buttons[button].HalfTransitionCount +=
      !CurrentInput->Controllers[controller].Buttons[button].EndedDown;
  CurrentInput->Controllers[controller].Buttons[button].EndedDown = true;
  if (button >= 0 && button < 4) {
    CurrentInput->Controllers[controller].isAnalog = false;
  }
}
void controller_button_release(int controller, int button) {
  game_input *CurrentInput = &state.GameInputs[state.CurrentGameInputIndex];
  CurrentInput->Controllers[controller].Buttons[button].HalfTransitionCount +=
      !!CurrentInput->Controllers[controller].Buttons[button].EndedDown;
  CurrentInput->Controllers[controller].Buttons[button].EndedDown = false;
}

void blur() {

  game_input *CurrentInput = &state.GameInputs[state.CurrentGameInputIndex];
  for (size_t b = 0; b < ArrayCount(CurrentInput->Mouse.Buttons); b++) {

    CurrentInput->Mouse.Buttons[b].HalfTransitionCount = 0;
    CurrentInput->Mouse.Buttons[b].EndedDown = 0;
  }
  for (size_t c = 0; c < ArrayCount(CurrentInput->Controllers); c++) {
    for (size_t b = 0; b < ArrayCount(CurrentInput->Controllers[c].Buttons);
         b++) {
      CurrentInput->Controllers[c].Buttons[b].HalfTransitionCount = 0;
      CurrentInput->Controllers[c].Buttons[b].EndedDown = 0;
    }
    CurrentInput->Controllers[c].AverageStickX = 0;
    CurrentInput->Controllers[c].AverageStickY = 0;
  }
}
