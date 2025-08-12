#if !defined(WIN32_HANDMADE_H)

#include "handmade.h"

#include <windows.h>
#include <Xinput.h>
#include <dsound.h>
#include <math.h>
#include <timeapi.h>

global_variable bool Running;
global_variable int64 GlobalPerfCounterFrequency;
#if defined(_M_ARM64)
__int64 __rdtsc() { return _ReadStatusReg(ARM64_PMCCNTR_EL0); }
#endif

#ifdef HANDMADE_INTERNAL
struct win32_debugger_state {
  bool AudioSync;
  bool RenderPause;
};

win32_debugger_state GlobalDebuggerState = {};
#endif
struct win32_debug_time_marker {
  DWORD OutputPlayCursor;
  DWORD OutputWriteCursor;

  DWORD OutputLocation;
  DWORD OutputByteCount;
  DWORD ExpectedFlipPlayCursor;

  DWORD FlipPlayCursor;
  DWORD FlipWriteCursor;
};

struct win32_offscreen_buffer {
  BITMAPINFO Info;
  void *Memory;
  int Width;
  int Height;
  int BytesPerPixel;
};

struct win32_sound_buffer {
  LPDIRECTSOUNDBUFFER Buffer;
  DWORD SoundBufferSize;
  int BytesPerSample = sizeof(int16) * 2;
};

struct win32_sound_output {
  int SamplingRateInHz;
  uint32 RunningSampleIndex;
  int SafetySampleBytes;
};

global_variable win32_offscreen_buffer GlobalScreenBuffer;
global_variable win32_sound_buffer GlobalSoundBuffer;
global_variable win32_sound_output GlobalSoundOutput;

struct win32_window_dimensions {
  int width;
  int height;
};

internal win32_window_dimensions Win32GetWindowSize(HWND Window) {
  RECT ClientRect;
  win32_window_dimensions dim;

  GetClientRect(Window, &ClientRect);

  dim.width = ClientRect.right - ClientRect.left;
  dim.height = ClientRect.bottom - ClientRect.top;

  return dim;
};

#define DIRECT_SOUND_CREATE(name)                                              \
  HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS,               \
                      LPUNKNOWN pUnkOuter)
typedef DIRECT_SOUND_CREATE(direct_sound_create);

struct win32_game {
  bool IsValid;
  HMODULE Dll;
  FILETIME LastWriteTime;
  game_update_and_render *UpdateAndRender;
  game_get_sound_samples *GetSoundSamples;
};

internal GAME_UPDATE_AND_RENDER(GameUpdateAndRenderStub) { return; }

internal GAME_GET_SOUND_SAMPLES(GameGetSoundSamplesStub) { return; }

internal win32_game LoadGame();

internal void UnloadGame(win32_game);


#define WIN32_HANDMADE_H
#endif
