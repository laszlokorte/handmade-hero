#include <windows.h>
#include <Xinput.h>
#include <dsound.h>
#include <math.h>
#include <stdint.h>

#define local_persist static
#define global_variable static
#define internal static

typedef float real32;
typedef double real64;

#define Pi32 3.14159265359
global_variable bool Running;

struct game_state {
  uint32_t time;
  int xpos;
  int ypos;
  int note;
  int volume;
};

global_variable game_state global_game_state = {};

struct win32_offscreen_buffer {
  BITMAPINFO Info;
  void *Memory;
  int width;
  int height;
  int BytesPerPixel;
};

struct win32_sound_buffer {
  LPDIRECTSOUNDBUFFER Buffer;
  int SoundBufferSize;
  int BytesPerSample = sizeof(int16_t) * 2;
};

struct win32_sound_output {
  int SamplingRateInHz;
  int ToneBaseVolume;
  real32 ToneBaseFreqInHz;
  real32 ToneStepFactor;
  uint32_t RunningSampleIndex;
  real32 GeneratorTimeInRadians;
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

internal void Win32InitDSound(HWND Window, int32_t SamplingRateInHz,
                              int32_t BufferSize) {
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

void Win32FillSoundBuffer(DWORD BytesToLock, DWORD BytesToWrite) {
  VOID *Region1;
  DWORD Region1Size;
  VOID *Region2;
  DWORD Region2Size;

  HRESULT Res =
      GlobalSoundBuffer.Buffer->Lock(BytesToLock, BytesToWrite, &Region1,
                                     &Region1Size, &Region2, &Region2Size, 0);

  real32 ToneFreqInRadians = 2.0 * Pi32 * GlobalSoundOutput.ToneBaseFreqInHz /
                             GlobalSoundOutput.SamplingRateInHz;
  if (SUCCEEDED(Res)) {
    int16_t *SampleOut = (int16_t *)Region1;
    DWORD Region1SampleCount = Region1Size / GlobalSoundBuffer.BytesPerSample;
    for (DWORD SampleIndex = 0; SampleIndex < Region1SampleCount;
         ++SampleIndex) {
      int16_t SampleValue = sin(GlobalSoundOutput.GeneratorTimeInRadians) *
                            GlobalSoundOutput.ToneBaseVolume *
                            pow(2.0, global_game_state.volume / 10.0);
      *SampleOut++ = SampleValue;
      *SampleOut++ = SampleValue;
      GlobalSoundOutput.RunningSampleIndex++;
      GlobalSoundOutput.GeneratorTimeInRadians +=
          ToneFreqInRadians *
          pow(2.0, GlobalSoundOutput.ToneStepFactor * global_game_state.note);
    }
    SampleOut = (int16_t *)Region2;
    DWORD Region2SampleCount = Region2Size / GlobalSoundBuffer.BytesPerSample;
    for (DWORD SampleIndex = 0; SampleIndex < Region2SampleCount;
         ++SampleIndex) {
      int16_t SampleValue = sin(GlobalSoundOutput.GeneratorTimeInRadians) *
                            GlobalSoundOutput.ToneBaseVolume *
                            pow(2.0, global_game_state.volume / 10.0);
      *SampleOut++ = SampleValue;
      *SampleOut++ = SampleValue;
      GlobalSoundOutput.RunningSampleIndex++;
      GlobalSoundOutput.GeneratorTimeInRadians +=
          ToneFreqInRadians *
          pow(2.0, GlobalSoundOutput.ToneStepFactor * global_game_state.note);
    }

    while(GlobalSoundOutput.GeneratorTimeInRadians > 2*Pi32) {
      GlobalSoundOutput.GeneratorTimeInRadians -= 2 * Pi32;
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
internal void RenderGradient(win32_offscreen_buffer Buffer, int xoff, int yoff,
                             int zoff) {
  unsigned int *canvas = (unsigned int *)(Buffer.Memory);
  int cx = Buffer.width / 2;
  int cy = Buffer.height / 2;
  for (int x = 0; x < Buffer.width; x++) {
    for (int y = 0; y < Buffer.height; y++) {
      int yy = y - cy;
      int xx = x - cx;

      uint8_t green = xx + xoff;
      uint8_t blue = yy + yoff;
      uint8_t red = zoff / 2;
      if ((zoff / 256) % 2 == 0) {
        red = 255 - red;
      }
      canvas[(y)*Buffer.width + (x)] = (green << 8) | blue | (red << 16);
    }
  }
}
internal void ResizeDIBSection(win32_offscreen_buffer *buffer, int width,
                               int height) {
  if (buffer->Memory) {
    VirtualFree(buffer->Memory, 0, MEM_RELEASE);
  }
  buffer->width = width;
  buffer->height = height;
  buffer->BytesPerPixel = 4;

  int allocBytes = buffer->width * buffer->height * buffer->BytesPerPixel;

  buffer->Info.bmiHeader.biSize = sizeof(buffer->Info.bmiHeader);
  buffer->Info.bmiHeader.biWidth = buffer->width;
  buffer->Info.bmiHeader.biHeight = buffer->height;
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

  RenderGradient(*buffer, global_game_state.xpos, global_game_state.ypos,
                 global_game_state.time);
}

internal void Win32DisplayBufferWindow(HDC DeviceContext, int WindowWidth,
                                       int WindowHeight,
                                       win32_offscreen_buffer *Buffer) {
  StretchDIBits(DeviceContext, 0, 0, WindowWidth, WindowHeight, 0, 0,
                Buffer->width, Buffer->height, Buffer->Memory, &(Buffer->Info),
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

    uint32_t VKCode = WParam;
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

int CALLBACK WinMain(HINSTANCE Instance, HINSTANCE PrevInstance,
                     LPSTR lpCmdLine, int nCmdShow) {

  GlobalSoundOutput.SamplingRateInHz = 48000;
  GlobalSoundOutput.ToneBaseFreqInHz = 440;
  GlobalSoundOutput.ToneBaseVolume = 6000;
  GlobalSoundOutput.ToneStepFactor = 1.0 / 12.0;
  GlobalSoundOutput.RunningSampleIndex = 0;
  GlobalSoundOutput.LatencySampleCount = GlobalSoundOutput.SamplingRateInHz / 10;
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
      GlobalSoundBuffer.BytesPerSample = sizeof(uint16_t) * 2;
      GlobalSoundBuffer.SoundBufferSize =
          GlobalSoundOutput.SamplingRateInHz * GlobalSoundBuffer.BytesPerSample;
      Win32InitDSound(Window, GlobalSoundOutput.SamplingRateInHz,
                      GlobalSoundBuffer.SoundBufferSize);
      Win32FillSoundBuffer(0, GlobalSoundBuffer.SoundBufferSize);
      HRESULT Res = GlobalSoundBuffer.Buffer->Play(0, 0, DSBPLAY_LOOPING);

      while (Running) {
        global_game_state.time++;
        MSG message;
        while (PeekMessage(&message, 0, 0, 0, PM_REMOVE)) {
          if (message.message == WM_QUIT) {
            Running = false;
          }
          TranslateMessage(&message);
          DispatchMessage(&message);
        }

        for (DWORD ControllerIndex = 0; ControllerIndex < XUSER_MAX_COUNT;
             ++ControllerIndex) {
          XINPUT_STATE ControllerState;
          if (XInputGetState(ControllerIndex, &ControllerState) ==
              ERROR_SUCCESS) {
            XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;

            bool DPadUp = Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP;
            bool DPadLeft = Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT;
            bool DPadRight = Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT;
            bool DPadDown = Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN;
          } else {
            // Not available
          }
        }

        RenderGradient(GlobalScreenBuffer, global_game_state.xpos,
                       global_game_state.ypos, global_game_state.time);

        DWORD PlayCursor;
        DWORD WriteCursor;
        HRESULT Res = GlobalSoundBuffer.Buffer->GetCurrentPosition(
            &PlayCursor, &WriteCursor);
        if (SUCCEEDED(Res)) {
          DWORD BytesToLock = GlobalSoundOutput.RunningSampleIndex *
                              GlobalSoundBuffer.BytesPerSample %
                              GlobalSoundBuffer.SoundBufferSize;
          DWORD TargetCursor =
              (PlayCursor + GlobalSoundOutput.LatencySampleCount *
                                GlobalSoundBuffer.BytesPerSample) %
              GlobalSoundBuffer.SoundBufferSize;
          DWORD BytesToWrite;
          if (BytesToLock > TargetCursor) {
            BytesToWrite = GlobalSoundBuffer.SoundBufferSize - BytesToLock;
            BytesToWrite += TargetCursor;
          } else {
            BytesToWrite = TargetCursor - BytesToLock;
          }
          Win32FillSoundBuffer(BytesToLock, BytesToWrite);
        }
        InvalidateRect(Window, 0, FALSE);
      }
    }
  } else {
  }
  return 0;
}
