#include "handmade.h"

#include <windows.h>
#include <Xinput.h>
#include <dsound.h>
#include <math.h>

global_variable bool Running;

#include "handmade.cpp"

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
  int LatencySampleCount;
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
  buffer->Info.bmiHeader.biHeight = buffer->Height;
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
#define WAS_DOWN_MASK (1 << 30)
#define IS_DOWN_MASK (1 << 31)
#define IS_ALT (1 << 29)

    uint32 VKCode = WParam;
    bool WasDown = (LParam & WAS_DOWN_MASK) != 0;
    bool IsDown = (LParam & IS_DOWN_MASK) == 0;
    bool AltIsDown = (LParam & IS_ALT) != 0;

    if ((VKCode == 'R') && IsDown) {
      global_game_state.volume += 1;
      if (global_game_state.volume > 10) {
        global_game_state.volume = 10;
      }
    }
    if ((VKCode == 'T') && IsDown) {
      global_game_state.volume -= 1;
      if (global_game_state.volume < -10) {
        global_game_state.volume = -10;
      }
    }
    if ((VKCode == 'Q') && IsDown && !WasDown) {
      global_game_state.note -= 1;
    }
    if ((VKCode == 'E') && IsDown && !WasDown) {
      global_game_state.note += 1;
    }

    if ((VKCode == 'W' || VKCode == VK_UP) && IsDown) {
      global_game_state.ypos += 10;
    }
    if ((VKCode == 'A' || VKCode == VK_LEFT) && IsDown) {
      global_game_state.xpos -= 10;
    }
    if ((VKCode == 'S' || VKCode == VK_DOWN) && IsDown) {
      global_game_state.ypos -= 10;
    }
    if ((VKCode == 'D' || VKCode == VK_RIGHT) && IsDown) {
      global_game_state.xpos += 10;
    }
    if (VKCode == VK_ESCAPE && (!WasDown) && IsDown) {
      Running = false;
    }
    if (VKCode == VK_F4 && AltIsDown) {
      Running = false;
    }
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

int CALLBACK WinMain(HINSTANCE Instance, HINSTANCE PrevInstance,
                     LPSTR lpCmdLine, int nCmdShow) {
  LARGE_INTEGER LastCounter;
  LARGE_INTEGER PerfCounterFrequency;
  int64 LastCycleCount = __rdtsc();
  QueryPerformanceFrequency(&PerfCounterFrequency);
  QueryPerformanceCounter(&LastCounter);
  GlobalSoundOutput.SamplingRateInHz = 48000;
  GlobalSoundOutput.RunningSampleIndex = 0;
  GlobalSoundOutput.LatencySampleCount =
      GlobalSoundOutput.SamplingRateInHz / 12;
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
      int64 TotalMemorySize = GameMemory.TransientStorageSize + GameMemory.PermanentStorageSize;

      GameMemory.PermanentStorage =
          VirtualAlloc(0, TotalMemorySize,
                       MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
      GameMemory.TransientStorage = (uint8*)GameMemory.PermanentStorage + GameMemory.PermanentStorageSize;

      if (Samples && GameMemory.PermanentStorage &&
          GameMemory.TransientStorage) {

        game_input Inputs[2];
        game_input *NewInput = &Inputs[0];
        game_input *OldInput = &Inputs[1];
        while (Running) {
          MSG message;
          while (PeekMessage(&message, 0, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) {
              Running = false;
            }
            TranslateMessage(&message);
            DispatchMessage(&message);
          }

          int MaxControllerCount = XUSER_MAX_COUNT;
          if (MaxControllerCount > ArrayCount(Inputs->Controllers)) {
            MaxControllerCount = ArrayCount(Inputs->Controllers);
          }
          for (DWORD ControllerIndex = 0; ControllerIndex < MaxControllerCount;
               ++ControllerIndex) {
            XINPUT_STATE ControllerState;

            game_controller_input *OldController =
                &OldInput->Controllers[ControllerIndex];
            game_controller_input *NewController =
                &NewInput->Controllers[ControllerIndex];

            if (XInputGetState(ControllerIndex, &ControllerState) ==
                ERROR_SUCCESS) {
              XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;

              bool DPadUp = Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP;
              bool DPadLeft = Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT;
              bool DPadRight = Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT;
              bool DPadDown = Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN;

              real32 X;
              real32 Y;

              if (Pad->sThumbLX < 0) {
                X = Pad->sThumbLX / 32768.0f;
              } else {
                X = Pad->sThumbLX / 32767.0f;
              }
              if (Pad->sThumbLY < 0) {
                Y = Pad->sThumbLY / 32768.0f;
              } else {
                Y = Pad->sThumbLY / 32767.0f;
              }
              NewController->isAnalog = true;
              NewController->StartX = OldController->EndX;
              NewController->MinX = NewController->MaxX = NewController->EndX =
                  X;
              NewController->StartY = OldController->EndY;
              NewController->MinY = NewController->MaxY = NewController->EndY =
                  Y;
              Win32ProcessXInputDigitalButton(Pad->wButtons, XINPUT_GAMEPAD_A,
                                              &OldController->Down,
                                              &NewController->Down);
            } else {
              // Not available
            }
          }

          game_offscreen_buffer ScreenBuffer = {};
          ScreenBuffer.Memory = GlobalScreenBuffer.Memory;
          ScreenBuffer.Width = GlobalScreenBuffer.Width;
          ScreenBuffer.Height = GlobalScreenBuffer.Height;
          ScreenBuffer.BytesPerPixel = GlobalScreenBuffer.BytesPerPixel;

          DWORD PlayCursor = 0;
          DWORD WriteCursor = 0;
          DWORD BytesToWrite = 0;
          DWORD BytesToLock = 0;
          bool SoundIsValid = false;
          HRESULT Res = GlobalSoundBuffer.Buffer->GetCurrentPosition(
              &PlayCursor, &WriteCursor);
          if (SUCCEEDED(Res)) {
            BytesToLock = GlobalSoundOutput.RunningSampleIndex *
                          GlobalSoundBuffer.BytesPerSample %
                          GlobalSoundBuffer.SoundBufferSize;
            DWORD TargetCursor =
                (PlayCursor + GlobalSoundOutput.LatencySampleCount *
                                  GlobalSoundBuffer.BytesPerSample) %
                GlobalSoundBuffer.SoundBufferSize;
            if (BytesToLock > TargetCursor) {
              BytesToWrite = GlobalSoundBuffer.SoundBufferSize - BytesToLock;
              BytesToWrite += TargetCursor;
            } else {
              BytesToWrite = TargetCursor - BytesToLock;
            }
            SoundIsValid = true;
          }

          game_sound_output_buffer SoundBuffer = {};
          SoundBuffer.SamplesPerSecond = GlobalSoundOutput.SamplingRateInHz;
          SoundBuffer.SampleCount =
              BytesToWrite /
              GlobalSoundBuffer
                  .BytesPerSample; // SoundBuffer.SamplesPerSecond / 30;
          SoundBuffer.Samples = Samples;
          GameUpdateAndRender(&GameMemory, NewInput, &ScreenBuffer,
                              &SoundBuffer);

          if (SoundIsValid) {

            Win32FillSoundBuffer(BytesToLock, BytesToWrite, &SoundBuffer);
          }
          InvalidateRect(Window, 0, FALSE);

          int64 EndCycleCount = __rdtsc();

          LARGE_INTEGER EndCounter;
          QueryPerformanceCounter(&EndCounter);
          int64 DeltaCycles = EndCycleCount - LastCycleCount;
          int64 DeltaTime = EndCounter.QuadPart - LastCounter.QuadPart;
          int32 DeltaTimeMS = DeltaTime * 1000 / PerfCounterFrequency.QuadPart;
          int32 fps = PerfCounterFrequency.QuadPart / DeltaTime;
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
      } else {
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
