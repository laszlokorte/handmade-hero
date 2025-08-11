#include "handmade.h"

global_variable bool Running;

#include "handmade.cpp"

#include <windows.h>
#include <Xinput.h>
#include <dsound.h>
#include <math.h>
#include <timeapi.h>

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
struct win32_debug_audio_cursors {
  DWORD PlayCursor;
  DWORD WriteCursor;
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
  int SoundBufferSize;
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

internal void Win32InitDSound(HWND Window, int32 SamplingRateInHz,
                              int32 BufferSize) {
  HRESULT Res;
  HMODULE DSoundLibrary = LoadLibrary("dsound.dll");
  if (DSoundLibrary) {
    LPDIRECTSOUND DirectSound;
    direct_sound_create *DirectSoundCreate =
        (direct_sound_create *)GetProcAddress(DSoundLibrary,
                                              "DirectSoundCreate");
    Res = DirectSoundCreate(0, &DirectSound, 0);
    if (DirectSoundCreate && SUCCEEDED(Res)) {
      WAVEFORMATEX WaveFormat;
      WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
      WaveFormat.nChannels = 2;
      WaveFormat.wBitsPerSample = 16;
      WaveFormat.nSamplesPerSec = SamplingRateInHz;
      WaveFormat.nBlockAlign =
          WaveFormat.nChannels * WaveFormat.wBitsPerSample / 8;
      WaveFormat.nAvgBytesPerSec =
          WaveFormat.nSamplesPerSec * WaveFormat.nBlockAlign;
      WaveFormat.cbSize = 0;
      Res = DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY);
      if (SUCCEEDED(Res)) {
        DSBUFFERDESC BufferDescription = {sizeof(BufferDescription)};
        BufferDescription.dwSize = sizeof(BufferDescription);
        BufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;
        LPDIRECTSOUNDBUFFER PrimaryBuffer = {};

        Res = DirectSound->CreateSoundBuffer(&BufferDescription, &PrimaryBuffer,
                                             0);
        if (SUCCEEDED(Res)) {
          Res = PrimaryBuffer->SetFormat(&WaveFormat);
          if (SUCCEEDED(Res)) {
            OutputDebugString("Primary Buffer Created and Format Set");
          } else {
            OutputDebugString("// could not set format on primary buffer");
          }
        } else {
          OutputDebugString("// could not create primary buffer");
          Running = false;
        }
      } else {
        Running = false;
        OutputDebugString("// could not set priority");
      }

      DSBUFFERDESC BufferDescription = {sizeof(BufferDescription)};
      BufferDescription.dwSize = sizeof(BufferDescription);
      BufferDescription.dwFlags = 0;
      BufferDescription.dwBufferBytes = BufferSize;
      BufferDescription.lpwfxFormat = &WaveFormat;

      Res = DirectSound->CreateSoundBuffer(&BufferDescription,
                                           &GlobalSoundBuffer.Buffer, 0);
      if (SUCCEEDED(Res)) {
        OutputDebugString("Secondary Buffer Created");
        // Res = SecondaryBuffer->SetFormat(&WaveFormat);
        // if(SUCCEEDED(Res)) {
        // } else {
        //     OutputDebugString("// could not set format on secondary buffer");
        // }
      } else {
        OutputDebugString("// could not create secondary buffer");
        Running = false;
      }
    } else {
      Running = false;
      OutputDebugString("// cound not create sound context");
    }
  } else {
    Running = false;
    OutputDebugString("// Dsound not loaded");
  }
}
void Win32ClearSoundBuffer() {
  VOID *Region1;
  DWORD Region1Size;
  VOID *Region2;
  DWORD Region2Size;

  HRESULT Res = GlobalSoundBuffer.Buffer->Lock(
      0, GlobalSoundBuffer.SoundBufferSize, &Region1, &Region1Size, &Region2,
      &Region2Size, 0);

  if (SUCCEEDED(Res)) {
    uint8 *DestSample = (uint8 *)Region1;
    for (DWORD ByteIndex = 0; ByteIndex < Region1Size; ++ByteIndex) {
      *DestSample++ = 0;
    }
    DestSample = (uint8 *)Region2;
    for (DWORD ByteIndex = 0; ByteIndex < Region2Size; ++ByteIndex) {
      *DestSample++ = 0;
    }

    GlobalSoundBuffer.Buffer->Unlock(Region1, Region1Size, Region2,
                                     Region2Size);
  }
}
void Win32FillSoundBuffer(DWORD BytesToLock, DWORD BytesToWrite,
                          game_sound_output_buffer *SourceBuffer) {
  VOID *Region1;
  DWORD Region1Size;
  VOID *Region2;
  DWORD Region2Size;

  HRESULT Res =
      GlobalSoundBuffer.Buffer->Lock(BytesToLock, BytesToWrite, &Region1,
                                     &Region1Size, &Region2, &Region2Size, 0);

  if (SUCCEEDED(Res)) {
    int16 *DestSample = (int16 *)Region1;
    int16 *SourceSample = SourceBuffer->Samples;
    DWORD Region1SampleCount = Region1Size / GlobalSoundBuffer.BytesPerSample;
    for (DWORD SampleIndex = 0; SampleIndex < Region1SampleCount;
         ++SampleIndex) {
      *DestSample++ = *SourceSample++;
      *DestSample++ = *SourceSample++;
      GlobalSoundOutput.RunningSampleIndex++;
    }
    DestSample = (int16 *)Region2;
    DWORD Region2SampleCount = Region2Size / GlobalSoundBuffer.BytesPerSample;
    for (DWORD SampleIndex = 0; SampleIndex < Region2SampleCount;
         ++SampleIndex) {
      *DestSample++ = *SourceSample++;
      *DestSample++ = *SourceSample++;
      GlobalSoundOutput.RunningSampleIndex++;
    }

    GlobalSoundBuffer.Buffer->Unlock(Region1, Region1Size, Region2,
                                     Region2Size);
  }
}

#define X_INPUT_GET_STATE(name)                                                \
  DWORD(WINAPI name)(DWORD wUserIndex, XINPUT_STATE * pState)
#define X_INPUT_SET_STATE(name)                                                \
  DWORD(WINAPI name)(DWORD dwUserIndex, XINPUT_VIBRATION * pVibration)
typedef X_INPUT_GET_STATE(x_input_get_state);
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_GET_STATE(XInputGetStateStub) { return ERROR_DEVICE_NOT_CONNECTED; }
X_INPUT_SET_STATE(XInputSetStateStub) { return ERROR_DEVICE_NOT_CONNECTED; }
global_variable x_input_get_state *XInputGetState_ = XInputGetStateStub;
global_variable x_input_set_state *XInputSetState_ = XInputSetStateStub;
#define XInputGetState XInputGetState_
#define XInputSetState XInputSetState_

internal void Win32LoadXInput(void) {
  HMODULE XInputLibrary = LoadLibraryA("XInput1_4.dll");
  if (!XInputLibrary) {
    XInputLibrary = LoadLibraryA("XInput1_3.dll");
  }
  if (XInputLibrary) {
    XInputGetState =
        (x_input_get_state *)GetProcAddress(XInputLibrary, "XInputGetState");
    XInputSetState =
        (x_input_set_state *)GetProcAddress(XInputLibrary, "XInputSetState");
  }
}

internal void ResizeDIBSection(win32_offscreen_buffer *buffer, int width,
                               int height) {
  if (buffer->Memory) {
    VirtualFree(buffer->Memory, 0, MEM_RELEASE);
  }
  buffer->Width = width;
  buffer->Height = height;
  buffer->BytesPerPixel = 4;

  int allocBytes = buffer->Width * buffer->Height * buffer->BytesPerPixel;

  buffer->Info.bmiHeader.biSize = sizeof(buffer->Info.bmiHeader);
  buffer->Info.bmiHeader.biWidth = buffer->Width;
  buffer->Info.bmiHeader.biHeight = -buffer->Height;
  buffer->Info.bmiHeader.biPlanes = 1;
  buffer->Info.bmiHeader.biBitCount = 32;
  buffer->Info.bmiHeader.biCompression = BI_RGB;
  buffer->Info.bmiHeader.biSizeImage = 0;
  buffer->Info.bmiHeader.biXPelsPerMeter = 0;
  buffer->Info.bmiHeader.biYPelsPerMeter = 0;
  buffer->Info.bmiHeader.biClrUsed = 0;
  buffer->Info.bmiHeader.biClrImportant = 0;
  buffer->Memory =
      VirtualAlloc(0, allocBytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
}

internal void Win32DisplayBufferWindow(HDC DeviceContext, int WindowWidth,
                                       int WindowHeight,
                                       win32_offscreen_buffer *Buffer) {
  StretchDIBits(DeviceContext, 0, 0, WindowWidth, WindowHeight, 0, 0,
                Buffer->Width, Buffer->Height, Buffer->Memory, &(Buffer->Info),
                DIB_RGB_COLORS, SRCCOPY);
}

LRESULT CALLBACK Win32MainWindowCallback(HWND Window, UINT Message,
                                         WPARAM WParam, LPARAM LParam) {
  LRESULT Result = 0;
  switch (Message) {
  case WM_SIZE: {
    win32_window_dimensions WinSize = Win32GetWindowSize(Window);
    ResizeDIBSection(&GlobalScreenBuffer, WinSize.width, WinSize.height);
    OutputDebugStringA("WM_SIZE\n");
  } break;

  case WM_DESTROY: {
    Running = false;
    OutputDebugStringA("WM_DETROY\n");
  } break;

  case WM_CLOSE: {
    // PostQuitMessage(0);
    Running = false;
    OutputDebugStringA("WM_CLOSE\n");
  } break;

  case WM_ACTIVATEAPP: {
    OutputDebugStringA("WM_ACTIVATEAPP\n");
  } break;

  case WM_SETCURSOR: {
    // SetCursor(0);
  } break;

  case WM_SYSKEYDOWN:
  case WM_SYSKEYUP:
  case WM_KEYUP:
  case WM_KEYDOWN: {
    // not handled here
  } break;

  case WM_PAINT: {
    PAINTSTRUCT Paint;
    HDC DeviceContext = BeginPaint(Window, &Paint);
    RECT ClientRect;
    win32_window_dimensions WinSize = Win32GetWindowSize(Window);
    GetClientRect(Window, &ClientRect);
    Win32DisplayBufferWindow(DeviceContext, WinSize.width, WinSize.height,
                             &GlobalScreenBuffer);
    EndPaint(Window, &Paint);
    OutputDebugStringA("WM_PAINT\n");
  } break;

  default: {
    Result = DefWindowProc(Window, Message, WParam, LParam);
  } break;
  }
  return Result;
}

internal void Win32ProcessXInputDigitalButton(DWORD XInputButtonState,
                                              DWORD ButtonBit,
                                              game_button_state *OldState,
                                              game_button_state *NewState) {
  NewState->EndedDown = (XInputButtonState & ButtonBit) == ButtonBit;
  NewState->HalfTransitionCount =
      (OldState->EndedDown != NewState->EndedDown) ? 1 : 0;
}

internal void Win32ProcessMessages(game_input *OldInput, game_input *NewInput) {

  MSG message;

  game_controller_input *KeyBoardController = &NewInput->Controllers[0];
  game_controller_input *OldKeyBoardController = &OldInput->Controllers[0];

  game_controller_input reset_controller = {};

  for (int b = 0; b < ArrayCount(reset_controller.Buttons); b++) {
    reset_controller.Buttons[b].EndedDown =
        OldKeyBoardController->Buttons[b].EndedDown;
  }

  *KeyBoardController = reset_controller;

  while (PeekMessage(&message, 0, 0, 0, PM_REMOVE)) {
    if (message.message == WM_QUIT) {
      Running = false;
    }
    switch (message.message) {
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    case WM_KEYUP:
    case WM_KEYDOWN: {
#define WAS_DOWN_MASK (1 << 30)
#define IS_DOWN_MASK (1 << 31)
#define IS_ALT (1 << 29)
      uint64 WParam = message.wParam;
      uint64 LParam = message.lParam;
      uint32 VKCode = (uint32)WParam;
      bool WasDown = (LParam & WAS_DOWN_MASK) != 0;
      bool IsDown = (LParam & IS_DOWN_MASK) == 0;
      bool AltIsDown = (LParam & IS_ALT) != 0;

      if (VKCode == 'Q') {
        KeyBoardController->LeftShoulder.EndedDown = IsDown;
        KeyBoardController->LeftShoulder.HalfTransitionCount +=
            IsDown != WasDown ? 1 : 0;
      }
      if (VKCode == 'E') {
        KeyBoardController->RightShoulder.EndedDown = IsDown;
        KeyBoardController->RightShoulder.HalfTransitionCount +=
            IsDown != WasDown ? 1 : 0;
      }
      if (VKCode == 'W') {
        KeyBoardController->MoveUp.EndedDown = IsDown;
        KeyBoardController->MoveUp.HalfTransitionCount +=
            IsDown != WasDown ? 1 : 0;
      }
      if (VKCode == 'A') {
        KeyBoardController->MoveLeft.EndedDown = IsDown;
        KeyBoardController->MoveLeft.HalfTransitionCount +=
            IsDown != WasDown ? 1 : 0;
      }
      if (VKCode == 'S') {
        KeyBoardController->MoveDown.EndedDown = IsDown;
        KeyBoardController->MoveDown.HalfTransitionCount +=
            IsDown != WasDown ? 1 : 0;
      }
      if (VKCode == 'D') {
        KeyBoardController->MoveRight.EndedDown = IsDown;
        KeyBoardController->MoveRight.HalfTransitionCount +=
            IsDown != WasDown ? 1 : 0;
      }
      if (VKCode == VK_UP) {
        KeyBoardController->ActionUp.EndedDown = IsDown;
        KeyBoardController->ActionUp.HalfTransitionCount +=
            IsDown != WasDown ? 1 : 0;
      }
      if (VKCode == VK_DOWN) {
        KeyBoardController->ActionDown.EndedDown = IsDown;
        KeyBoardController->ActionDown.HalfTransitionCount +=
            IsDown != WasDown ? 1 : 0;
      }
      if (VKCode == VK_LEFT) {
        KeyBoardController->ActionLeft.EndedDown = IsDown;
        KeyBoardController->ActionLeft.HalfTransitionCount +=
            IsDown != WasDown ? 1 : 0;
      }
      if (VKCode == VK_RIGHT) {
        KeyBoardController->ActionRight.EndedDown = IsDown;
        KeyBoardController->ActionRight.HalfTransitionCount +=
            IsDown != WasDown ? 1 : 0;
      }
      if (VKCode == VK_ESCAPE) {
        KeyBoardController->Menu.EndedDown = IsDown;
        KeyBoardController->Menu.HalfTransitionCount +=
            IsDown != WasDown ? 1 : 0;
      }
      if (VKCode == VK_BACK) {
        KeyBoardController->Back.EndedDown = IsDown;
        KeyBoardController->Back.HalfTransitionCount +=
            IsDown != WasDown ? 1 : 0;
      }
      if (VKCode == VK_F4 && AltIsDown) {
        Running = false;
      }
#ifdef HANDMADE_INTERNAL
      if (VKCode == VK_F5 && !WasDown && IsDown) {
        GlobalDebuggerState.AudioSync = !GlobalDebuggerState.AudioSync;
      }
      if (VKCode == VK_F6 && !WasDown && IsDown) {
        GlobalDebuggerState.RenderPause = !GlobalDebuggerState.RenderPause;
      }
#endif
    } break;
    default: {
      TranslateMessage(&message);
      DispatchMessage(&message);
    } break;
    }
  }
}

internal void Win32ProcessControllerInput(game_input *OldInput,
                                          game_input *NewInput) {

  DWORD MaxControllerCount = XUSER_MAX_COUNT;
  if (MaxControllerCount > ArrayCount(NewInput->Controllers) - 1) {
    MaxControllerCount = ArrayCount(NewInput->Controllers) - 1;
  }
  for (DWORD ControllerIndex = 0; ControllerIndex < MaxControllerCount;
       ++ControllerIndex) {
    XINPUT_STATE ControllerState;

    game_controller_input *OldController =
        &OldInput->Controllers[ControllerIndex + 1];
    game_controller_input *NewController =
        &NewInput->Controllers[ControllerIndex + 1];

    game_controller_input ResetController = {};
    *NewController = ResetController;

    if (XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS) {
      XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;

      real32 X = 0.0f;
      real32 Y = 0.0f;

      if (Pad->sThumbLX < -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) {
        X = (Pad->sThumbLX + XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) /
            (32768.0f - XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
      } else if (Pad->sThumbLX > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) {
        X = (Pad->sThumbLX - XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) /
            (32767.0f - XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
      }
      if (Pad->sThumbLY < -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) {
        Y = (Pad->sThumbLY + XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) /
            (32768.0f - XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
      } else if (Pad->sThumbLY > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) {
        Y = (Pad->sThumbLY - XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) /
            (32767.0f - XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
      }
      NewController->isAnalog = (X != 0.0f || Y != 0.0f);
      NewController->AverageStickX = X;
      NewController->AverageStickY = Y;

      if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP) {
        NewController->AverageStickY = 1.0f;
        NewController->isAnalog = false;
      }
      if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN) {
        NewController->AverageStickY = -1.0f;
        NewController->isAnalog = false;
      }

      if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) {
        NewController->AverageStickX = 1.0f;
        NewController->isAnalog = false;
      }

      if (Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT) {
        NewController->AverageStickX = -1.0f;
        NewController->isAnalog = false;
      }

      Win32ProcessXInputDigitalButton(Pad->wButtons, XINPUT_GAMEPAD_B,
                                      &OldController->ActionRight,
                                      &NewController->ActionRight);
      Win32ProcessXInputDigitalButton(Pad->wButtons, XINPUT_GAMEPAD_A,
                                      &OldController->ActionDown,
                                      &NewController->ActionDown);
      Win32ProcessXInputDigitalButton(Pad->wButtons, XINPUT_GAMEPAD_X,
                                      &OldController->ActionLeft,
                                      &NewController->ActionLeft);
      Win32ProcessXInputDigitalButton(Pad->wButtons, XINPUT_GAMEPAD_Y,
                                      &OldController->ActionUp,
                                      &NewController->ActionUp);
    } else {
      // Not available
    }
  }
}

inline LARGE_INTEGER Win32GetWallClock() {
  LARGE_INTEGER Result;
  QueryPerformanceCounter(&Result);

  return Result;
}

inline real32 Win32GetSecondsElapsed(LARGE_INTEGER Start, LARGE_INTEGER End) {
  real32 Result = ((real32)(End.QuadPart - Start.QuadPart)) /
                  (real32)GlobalPerfCounterFrequency;
  return Result;
}

internal void Win32DebugDrawHorizontal(win32_offscreen_buffer *ScreenBuffer,
                                       int Y, int Left, int Right,
                                       int32 Color) {
  uint8 *Pixel = (uint8 *)ScreenBuffer->Memory +
                 Left * ScreenBuffer->BytesPerPixel +
                 Y * ScreenBuffer->Width * ScreenBuffer->BytesPerPixel;

  int Pitch = ScreenBuffer->BytesPerPixel;
  for (int X = Left; X < Right && X < ScreenBuffer->Width; X++) {
    *(uint32 *)Pixel = Color;
    Pixel += Pitch;
  }
}

internal void Win32DebugDrawVertical(win32_offscreen_buffer *ScreenBuffer,
                                     int X, int Top, int Bottom, int32 Color) {
  uint8 *Pixel = (uint8 *)ScreenBuffer->Memory +
                 X * ScreenBuffer->BytesPerPixel +
                 Top * ScreenBuffer->Width * ScreenBuffer->BytesPerPixel;

  int Pitch = ScreenBuffer->BytesPerPixel * ScreenBuffer->Width;
  for (int Y = Top; Y < Bottom && Y < ScreenBuffer->Height; Y++) {
    *(uint32 *)Pixel = Color;
    Pixel += Pitch;
  }
}

internal void Win32DebugSyncDisplay(win32_offscreen_buffer *ScreenBuffer,
                                    int DebugAudioCursorCount,
                                    size_t CurrentCursorPos,
                                    win32_debug_audio_cursors *DebugAudioCursor,
                                    real32 TargerSecondsPerFrame) {
  int PadX = 16;
  int PadY = 16;

  int Top = PadY;
  int Bottom = ScreenBuffer->Height - PadY;

  real32 ratio = (ScreenBuffer->Width - 2 * PadX) /
                 ((real32)GlobalSoundBuffer.SoundBufferSize);
  for (int CursorIndex = 0; CursorIndex < DebugAudioCursorCount;
       CursorIndex++) {
    int XPlay =
        PadX +
        (int)(ratio * (real32)(DebugAudioCursor[CursorIndex].PlayCursor));
    int XWrite =
        PadX +
        (int)(ratio * (real32)(DebugAudioCursor[CursorIndex].WriteCursor));
    if (CurrentCursorPos == CursorIndex) {

      Win32DebugDrawVertical(ScreenBuffer, XPlay, Top + 30, Top + 60, 0x00ff00);
      Win32DebugDrawVertical(ScreenBuffer, XPlay + 1, Top + 30, Top + 60,
                             0x00ff00);
      Win32DebugDrawVertical(ScreenBuffer, XWrite, Top + 30, Top + 60,
                             0xff0000);
      Win32DebugDrawVertical(ScreenBuffer, XWrite + 1, Top + 30, Top + 60,
                             0xff0000);
    } else {

      Win32DebugDrawVertical(ScreenBuffer, XPlay, Top, Top + 30, 0xff00ff);
      Win32DebugDrawVertical(ScreenBuffer, XWrite, Top, Top + 30, 0x00ffff);
    }
  }
}

int CALLBACK WinMain(HINSTANCE Instance, HINSTANCE PrevInstance,
                     LPSTR lpCmdLine, int nCmdShow) {
#define TargetFrameHz 30
  real32 TargetSecondsPerFrame = 1.0f / TargetFrameHz;
  LARGE_INTEGER LastCounter;
  LARGE_INTEGER PerfCounterFrequency;
  int64 LastCycleCount = __rdtsc();
  UINT DesiredSchedulerMS = 1;
  bool SleepIsGranular =
      (timeBeginPeriod(DesiredSchedulerMS) == TIMERR_NOERROR);

  QueryPerformanceFrequency(&PerfCounterFrequency);
  GlobalPerfCounterFrequency = PerfCounterFrequency.QuadPart;
  QueryPerformanceCounter(&LastCounter);
  GlobalSoundOutput.SamplingRateInHz = 48000;
  GlobalSoundOutput.RunningSampleIndex = 0;
  GlobalSoundOutput.SafetySampleBytes = GlobalSoundOutput.SamplingRateInHz *
                                        GlobalSoundBuffer.BytesPerSample /
                                        TargetFrameHz;
  // MessageBox(0, "This is me", "Test", MB_OK|MB_ICONINFORMATION);
  WNDCLASS windowClass = {};
  windowClass.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
  windowClass.lpfnWndProc = *Win32MainWindowCallback;
  windowClass.hInstance = Instance;
  // windowClass.hIcon = ;
  windowClass.lpszClassName = "HandmadeHeroWindowClass";
  if (RegisterClass(&windowClass)) {
    HWND Window = CreateWindowEx(
        0, windowClass.lpszClassName, "Handmade Window",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT,
        CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, Instance, 0);
    if (Window) {
      Win32LoadXInput();
      Running = true;
      GlobalSoundBuffer = {};
      GlobalSoundBuffer.BytesPerSample = sizeof(uint16) * 2;
      GlobalSoundBuffer.SoundBufferSize =
          GlobalSoundOutput.SamplingRateInHz * GlobalSoundBuffer.BytesPerSample;
      Win32InitDSound(Window, GlobalSoundOutput.SamplingRateInHz,
                      GlobalSoundBuffer.SoundBufferSize);
      Win32ClearSoundBuffer();
      HRESULT Res = GlobalSoundBuffer.Buffer->Play(0, 0, DSBPLAY_LOOPING);

      int16 *Samples =
          (int16 *)VirtualAlloc(0, GlobalSoundBuffer.SoundBufferSize,
                                MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

#if HANDMADE_INTERNAL
      LPVOID BaseAddress = (LPVOID)Terabytes((uint64)2);
#else
      LPVOID BaseAddress = 0;
#endif

      game_memory GameMemory = {};
      GameMemory.Initialized = false;
      GameMemory.PermanentStorageSize = Megabytes(10);
      GameMemory.TransientStorageSize = Megabytes(500);
      int64 TotalMemorySize =
          GameMemory.TransientStorageSize + GameMemory.PermanentStorageSize;

      GameMemory.PermanentStorage = VirtualAlloc(
          0, TotalMemorySize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
      GameMemory.TransientStorage = (uint8 *)GameMemory.PermanentStorage +
                                    GameMemory.PermanentStorageSize;

      if (Samples && GameMemory.PermanentStorage &&
          GameMemory.TransientStorage) {

        win32_debug_audio_cursors DebugAudioCursors[TargetFrameHz] = {};
        size_t AudioCursorPos = 0;
        LARGE_INTEGER LastFrame = Win32GetWallClock();

        bool SoundIsValid = false;
        game_input Inputs[2] = {};
        game_input *NewInput = &Inputs[0];
        game_input *OldInput = &Inputs[1];
        while (Running) {
          Win32ProcessMessages(OldInput, NewInput);
          Win32ProcessControllerInput(OldInput, NewInput);

#ifdef HANDMADE_INTERNAL
          if (!GlobalDebuggerState.RenderPause) {
#endif
            game_offscreen_buffer ScreenBuffer = {};
            ScreenBuffer.Memory = GlobalScreenBuffer.Memory;
            ScreenBuffer.Width = GlobalScreenBuffer.Width;
            ScreenBuffer.Height = GlobalScreenBuffer.Height;
            ScreenBuffer.BytesPerPixel = GlobalScreenBuffer.BytesPerPixel;

            DWORD PlayCursor = 0;
            DWORD WriteCursor = 0;
            DWORD BytesToWrite = 0;
            DWORD BytesToLock = 0;
            Res = GlobalSoundBuffer.Buffer->GetCurrentPosition(&PlayCursor,
                                                               &WriteCursor);
            if (SUCCEEDED(Res)) {
#ifdef HANDMADE_INTERNAL
              {
                DebugAudioCursors[AudioCursorPos].PlayCursor = PlayCursor;
                DebugAudioCursors[AudioCursorPos].WriteCursor = WriteCursor;
                AudioCursorPos++;
                if (AudioCursorPos >= ArrayCount(DebugAudioCursors)) {
                  AudioCursorPos = 0;
                }
              }
#endif
              if (!SoundIsValid) {
                GlobalSoundOutput.RunningSampleIndex =
                    WriteCursor / GlobalSoundBuffer.BytesPerSample;
                SoundIsValid = true;
              }
              BytesToLock = GlobalSoundOutput.RunningSampleIndex *
                            GlobalSoundBuffer.BytesPerSample %
                            GlobalSoundBuffer.SoundBufferSize;
              DWORD ExpectedSoundBytesPerFrame =
                  GlobalSoundOutput.SamplingRateInHz *
                  GlobalSoundBuffer.BytesPerSample / TargetFrameHz;
              DWORD ExpectedFrameBoundaryByte =
                  PlayCursor + ExpectedSoundBytesPerFrame;
              DWORD SafeWriteCursor = WriteCursor;
              if (SafeWriteCursor < PlayCursor) {
                SafeWriteCursor += GlobalSoundBuffer.SoundBufferSize;
              }
              Assert(SafeWriteCursor >= PlayCursor);
              SafeWriteCursor += GlobalSoundOutput.SafetySampleBytes;
              bool AudioCardIsLowLatency =
                  SafeWriteCursor < ExpectedFrameBoundaryByte;

              DWORD TargetCursor = 0;
              if (AudioCardIsLowLatency) {
                TargetCursor =
                    (ExpectedFrameBoundaryByte + ExpectedSoundBytesPerFrame);
              } else {
                TargetCursor = (WriteCursor + ExpectedSoundBytesPerFrame +
                                GlobalSoundOutput.SafetySampleBytes);
              }
              TargetCursor = TargetCursor % GlobalSoundBuffer.SoundBufferSize;

              if (BytesToLock > TargetCursor) {
                BytesToWrite = GlobalSoundBuffer.SoundBufferSize - BytesToLock;
                BytesToWrite += TargetCursor;
              } else {
                BytesToWrite = TargetCursor - BytesToLock;
              }

              Assert(BytesToWrite > 0);

              int DistanceToPlay = (BytesToLock - PlayCursor +
                                    GlobalSoundBuffer.SoundBufferSize) %
                                   GlobalSoundBuffer.SoundBufferSize;
              int DistanceToWrite = (TargetCursor - PlayCursor +
                                     GlobalSoundBuffer.SoundBufferSize) %
                                    GlobalSoundBuffer.SoundBufferSize;
              char OutputBuffer[256] = {};
              wsprintf(OutputBuffer, "DTP: %d, DTW: %d\n", DistanceToPlay,
                       DistanceToWrite);
              OutputDebugString(OutputBuffer);

              game_sound_output_buffer SoundBuffer = {};
              SoundBuffer.SamplesPerSecond = GlobalSoundOutput.SamplingRateInHz;
              SoundBuffer.SampleCount =
                  BytesToWrite /
                  GlobalSoundBuffer
                      .BytesPerSample; // SoundBuffer.SamplesPerSecond / 30;
              SoundBuffer.Samples = Samples;
              GameGetSoundSamples(&GameMemory, &SoundBuffer);
              Win32FillSoundBuffer(BytesToLock, BytesToWrite, &SoundBuffer);
            } else {
              SoundIsValid = false;
            }

            bool ShallExit = false;
            GameUpdateAndRender(&GameMemory, NewInput, &ScreenBuffer,
                                &ShallExit);

            Running = Running && !ShallExit;

#ifdef HANDMADE_INTERNAL
            if (GlobalDebuggerState.AudioSync) {

              Win32DebugSyncDisplay(
                  &GlobalScreenBuffer, ArrayCount(DebugAudioCursors),
                  AudioCursorPos, DebugAudioCursors, TargetSecondsPerFrame);
            }
#endif
            LARGE_INTEGER WorkFrame = Win32GetWallClock();
            real32 WorkSecondsEllapsed =
                Win32GetSecondsElapsed(LastFrame, WorkFrame);
            real32 SecondsElapsedForFrame = WorkSecondsEllapsed;

            if (SecondsElapsedForFrame < TargetSecondsPerFrame) {
              while (SecondsElapsedForFrame < TargetSecondsPerFrame) {
                if (SleepIsGranular) {

                  DWORD SleepMs =
                      (DWORD)((TargetSecondsPerFrame - SecondsElapsedForFrame) /
                              1000.0f / 2.0f);
                  Sleep(SleepMs);
                }
                SecondsElapsedForFrame =
                    Win32GetSecondsElapsed(LastFrame, Win32GetWallClock());
              }
            } else {
              // Assert(false);
            }
            LARGE_INTEGER EndFrame = Win32GetWallClock();
            LastFrame = EndFrame;

            InvalidateRect(Window, 0, FALSE);

            int64 EndCycleCount = __rdtsc();

            LARGE_INTEGER EndCounter;
            QueryPerformanceCounter(&EndCounter);
            int64 DeltaCycles = EndCycleCount - LastCycleCount;
            int64 DeltaTime = EndCounter.QuadPart - LastCounter.QuadPart;
            LONGLONG DeltaTimeMS =
                DeltaTime * 1000 / PerfCounterFrequency.QuadPart;
            LONGLONG fps = PerfCounterFrequency.QuadPart / DeltaTime;
            char printBuffer[256];
            wsprintfA(printBuffer,
                      "Frame Duration %d ms/frame; %dfps; %d MC/frame\n",
                      DeltaTimeMS, fps, DeltaCycles / 1000000);
            OutputDebugStringA(printBuffer);

            LastCounter = EndCounter;
            LastCycleCount = EndCycleCount;

            game_input *tmp = OldInput;
            OldInput = NewInput;
            NewInput = tmp;
          } // while Running
        }
#ifdef HANDMADE_INTERNAL
      }
#endif
      else {
        // error: no memory
      }

    } else {
      // error: no window
    }
  } else {
    // error: no window class
  }
  return 0;
}
