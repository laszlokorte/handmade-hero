#include "win32_handmade.h"
#include "handmade.h"
#include "debug_font.h"
#include "handmade_types.h"
#include "renderer.h"
#include <GL/gl.h>

internal void Win32InitDSound(HWND Window, int32 SamplingRateInHz,
                              int32 BufferSize,
                              win32_sound_buffer *SoundBuffer) {
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
        }
      } else {
        OutputDebugString("// could not set priority");
      }

      DSBUFFERDESC BufferDescription = {sizeof(BufferDescription)};
      BufferDescription.dwSize = sizeof(BufferDescription);
      BufferDescription.dwFlags = 0;
      BufferDescription.dwBufferBytes = BufferSize;
      BufferDescription.lpwfxFormat = &WaveFormat;

      Res = DirectSound->CreateSoundBuffer(&BufferDescription,
                                           &SoundBuffer->Buffer, 0);
      if (SUCCEEDED(Res)) {
        OutputDebugString("Secondary Buffer Created");
        // Res = SecondaryBuffer->SetFormat(&WaveFormat);
        // if(SUCCEEDED(Res)) {
        // } else {
        //     OutputDebugString("// could not set format on secondary buffer");
        // }
      } else {
        OutputDebugString("// could not create secondary buffer");
      }
    } else {
      OutputDebugString("// cound not create sound context");
    }
  } else {
    OutputDebugString("// Dsound not loaded");
  }
}
void Win32ClearSoundBuffer(win32_sound_buffer *SoundBuffer) {
  VOID *Region1;
  DWORD Region1Size;
  VOID *Region2;
  DWORD Region2Size;

  HRESULT Res =
      SoundBuffer->Buffer->Lock(0, SoundBuffer->SoundBufferSize, &Region1,
                                &Region1Size, &Region2, &Region2Size, 0);

  if (SUCCEEDED(Res)) {
    uint8 *DestSample = (uint8 *)Region1;
    for (DWORD ByteIndex = 0; ByteIndex < Region1Size; ++ByteIndex) {
      *DestSample++ = 0;
    }
    DestSample = (uint8 *)Region2;
    for (DWORD ByteIndex = 0; ByteIndex < Region2Size; ++ByteIndex) {
      *DestSample++ = 0;
    }

    SoundBuffer->Buffer->Unlock(Region1, Region1Size, Region2, Region2Size);
  }
}
void Win32FillSoundBuffer(DWORD BytesToLock, DWORD BytesToWrite,
                          win32_sound_output *SoundOutput,
                          game_sound_output_buffer *SourceBuffer,
                          win32_sound_buffer *TargetBuffer) {
  VOID *Region1;
  DWORD Region1Size;
  VOID *Region2;
  DWORD Region2Size;

  HRESULT Res =
      TargetBuffer->Buffer->Lock(BytesToLock, BytesToWrite, &Region1,
                                 &Region1Size, &Region2, &Region2Size, 0);

  if (SUCCEEDED(Res)) {
    int16 *DestSample = (int16 *)Region1;
    int16 *SourceSample = SourceBuffer->Samples;
    DWORD Region1SampleCount = Region1Size / TargetBuffer->BytesPerSample;
    for (DWORD SampleIndex = 0; SampleIndex < Region1SampleCount;
         ++SampleIndex) {
      *DestSample++ = *SourceSample++;
      *DestSample++ = *SourceSample++;
      SoundOutput->RunningSampleIndex++;
    }
    DestSample = (int16 *)Region2;
    DWORD Region2SampleCount = Region2Size / TargetBuffer->BytesPerSample;
    for (DWORD SampleIndex = 0; SampleIndex < Region2SampleCount;
         ++SampleIndex) {
      *DestSample++ = *SourceSample++;
      *DestSample++ = *SourceSample++;
      SoundOutput->RunningSampleIndex++;
    }

    TargetBuffer->Buffer->Unlock(Region1, Region1Size, Region2, Region2Size);
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
    PostQuitMessage(0);
    OutputDebugStringA("WM_DETROY\n");
  } break;

  case WM_CLOSE: {
    PostQuitMessage(0);
    OutputDebugStringA("WM_CLOSE\n");
  } break;

  case WM_ACTIVATEAPP: {
    // if(WParam != 0) {
    //     SetCapture(Window);
    // } else {
    //     ReleaseCapture();
    // }

    if (WParam != 0 || !GlobalTransparent) {
      SetLayeredWindowAttributes(Window, RGB(0, 0, 0), 255, LWA_ALPHA);
    } else {
      SetLayeredWindowAttributes(Window, RGB(0, 0, 0), 100, LWA_ALPHA);
    }
    OutputDebugStringA("WM_ACTIVATEAPP\n");
  } break;

    // case WM_SETCURSOR: {
    //  SetCursor(0);
    // } break;

  case WM_SYSKEYDOWN:
  case WM_SYSKEYUP:
  case WM_KEYUP:
  case WM_KEYDOWN: {
    // not handled here
  } break;
  case WM_ERASEBKGND: {
    return 1;
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
internal void Win32BeginRecordingInput(win32_state *State,
                                       int InputRecordingIndex) {
  State->InputRecordingIndex = InputRecordingIndex;

  char Filename[] = "recording.hmi";
  State->RecordingHandle =
      CreateFileA(Filename, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
  DWORD BytesToWrite = (DWORD)State->TotalMemorySize;
  Assert(State->TotalMemorySize == BytesToWrite);
  DWORD BytesWritten;
  WriteFile(State->RecordingHandle, State->GameMemoryBlock, BytesToWrite,
            &BytesWritten, 0);
}

DEBUG_PLATFORM_FREE_FILE_MEMORY(DEBUGPlatformFreeFileMemory) {
  if (Memory) {
    VirtualFree(Memory, 0, MEM_RELEASE);
  }
}

DEBUG_PLATFORM_READ_ENTIRE_FILE(DEBUGPlatformReadEntireFile) {
  debug_read_file_result Result = {};
  HANDLE FileHandle = CreateFileA(Filename, GENERIC_READ, FILE_SHARE_READ, 0,
                                  OPEN_EXISTING, 0, 0);
  if (FileHandle != INVALID_HANDLE_VALUE) {
    LARGE_INTEGER FileSize;
    if (GetFileSizeEx(FileHandle, &FileSize)) {
      uint32 FileSize32 = SafeTruncateUInt64(FileSize.QuadPart);
      Result.Contents =
          VirtualAlloc(0, FileSize32, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
      if (Result.Contents) {
        DWORD BytesRead;
        if (ReadFile(FileHandle, Result.Contents, FileSize32, &BytesRead, 0)) {
          Result.ContentSize = FileSize32;
        } else {
          DEBUGPlatformFreeFileMemory(Context, Result.Contents);
        }
      }
    }
  }
  return Result;
}

DEBUG_PLATFORM_WRITE_ENTIRE_FILE(DEBUGPlatformWriteEntireFile) {
  bool Result = false;

  HANDLE FileHandle =
      CreateFileA(Filename, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
  if (FileHandle != INVALID_HANDLE_VALUE) {
    DWORD BytesWritten;
    if (WriteFile(FileHandle, Memory, MemorySize, &BytesWritten, 0)) {
      Result = (BytesWritten == MemorySize);
    } else {
    }
    CloseHandle(FileHandle);
  } else {
  }

  return Result;
}

internal void Win32SetupGameMemory(win32_state *Win32State,
                                   game_memory *GameMemory) {
  memory_index RenderBufferSize = 10000 * sizeof(render_command);
  GameMemory->Initialized = false;
  GameMemory->PermanentStorageSize = Megabytes(10);
  GameMemory->TransientStorageSize = Megabytes(100);
  Win32State->TotalMemorySize = GameMemory->TransientStorageSize +
                                GameMemory->PermanentStorageSize +
                                RenderBufferSize;
  Win32State->GameMemoryBlock = VirtualAlloc(
      0, Win32State->TotalMemorySize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
  GameMemory->PermanentStorage = (uint8 *)Win32State->GameMemoryBlock;
  GameMemory->TransientStorage =
      (uint8 *)GameMemory->PermanentStorage + GameMemory->PermanentStorageSize;
  InitializeRenderBuffer(&Win32State->RenderBuffer, RenderBufferSize,
                         (render_command *)(GameMemory->PermanentStorage +
                                            GameMemory->PermanentStorageSize +
                                            GameMemory->TransientStorageSize));
  GameMemory->DebugPlatformReadEntireFile = &DEBUGPlatformReadEntireFile;
  GameMemory->DebugPlatformFreeFileMemory = &DEBUGPlatformFreeFileMemory;
  GameMemory->DebugPlatformWriteEntireFile = &DEBUGPlatformWriteEntireFile;
}

internal void Win32EndRecordingInput(win32_state *State) {
  CloseHandle(State->RecordingHandle);
  State->InputRecordingIndex = 0;
}
internal void Win32RecordInput(win32_state *Win32State, game_input *Input) {
  DWORD BytesWritten;
  WriteFile(Win32State->RecordingHandle, Input, sizeof(*Input), &BytesWritten,
            0);
}
internal void Win32BeginInputPlayback(win32_state *State,
                                      int InputPlaybackIndex) {
  State->InputPlayingIndex = InputPlaybackIndex;

  char Filename[] = "recording.hmi";
  State->PlaybackHandle = CreateFileA(Filename, GENERIC_READ, FILE_SHARE_READ,
                                      0, OPEN_EXISTING, 0, 0);

  DWORD BytesToRead = (DWORD)State->TotalMemorySize;
  Assert(State->TotalMemorySize == BytesToRead);
  DWORD BytesRead;
  ReadFile(State->PlaybackHandle, State->GameMemoryBlock, BytesToRead,
           &BytesRead, 0);
}

internal void Win32EndInputPlayback(win32_state *State) {
  CloseHandle(State->PlaybackHandle);
  State->InputPlayingIndex = 0;
}

internal void Win32PlaybackInput(win32_state *Win32State, game_input *Input) {
  DWORD BytesRead;
  if (ReadFile(Win32State->PlaybackHandle, Input, sizeof(*Input), &BytesRead,
               0)) {
    if (BytesRead == 0) {

      int PlayingIndex = Win32State->InputPlayingIndex;
      Win32EndInputPlayback(Win32State);
      Win32BeginInputPlayback(Win32State, PlayingIndex);
      ReadFile(Win32State->PlaybackHandle, Input, sizeof(*Input), &BytesRead,
               0);
    }
  }
}

WINDOWPLACEMENT GlobalWindowPlacement = {sizeof(GlobalWindowPlacement)};
internal void Win32ToggleFullScreen(HWND Window) {
  DWORD dwStyle = GetWindowLong(Window, GWL_STYLE);
  if (dwStyle & WS_OVERLAPPEDWINDOW) {
    MONITORINFO mi = {sizeof(mi)};
    if (GetWindowPlacement(Window, &GlobalWindowPlacement) &&
        GetMonitorInfo(MonitorFromWindow(Window, MONITOR_DEFAULTTOPRIMARY),
                       &mi)) {
      SetWindowLong(Window, GWL_STYLE, dwStyle & ~WS_OVERLAPPEDWINDOW);
      SetWindowPos(Window, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top,
                   mi.rcMonitor.right - mi.rcMonitor.left,
                   mi.rcMonitor.bottom - mi.rcMonitor.top,
                   SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }
  } else {
    SetWindowLong(Window, GWL_STYLE, dwStyle | WS_OVERLAPPEDWINDOW);
    SetWindowPlacement(Window, &GlobalWindowPlacement);
    SetWindowPos(Window, NULL, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER |
                     SWP_FRAMECHANGED);
  }
}

internal void Win32ProcessMouseButton(game_button_state *ButtonState,
                                      WPARAM ActualButton,
                                      WPARAM ExpectedButton) {
  bool NewDown = ActualButton & ExpectedButton;
  ButtonState->HalfTransitionCount += ButtonState->EndedDown != NewDown ? 0 : 1;
  ButtonState->EndedDown = NewDown;
}

internal void Win32ProcessPendingMessages(HWND Window, win32_state *Win32State,
                                          game_memory *GameMemory,
                                          game_input *OldInput,
                                          game_input *NewInput,
                                          bool *ShallReload) {

  MSG message;

  game_controller_input *KeyBoardController = &NewInput->Controllers[0];
  game_controller_input *OldKeyBoardController = &OldInput->Controllers[0];

  game_mouse_input *Mouse = &NewInput->Mouse;
  game_mouse_input *OldMouse = &OldInput->Mouse;

  game_controller_input reset_controller = {};
  game_mouse_input reset_mouse = {};

  reset_mouse.MouseX = OldMouse->MouseX;
  reset_mouse.MouseY = OldMouse->MouseY;
  for (int b = 0; b < ArrayCount(reset_mouse.Buttons); b++) {
    reset_mouse.Buttons[b].EndedDown = OldMouse->Buttons[b].EndedDown;
  }

  for (int b = 0; b < ArrayCount(reset_controller.Buttons); b++) {
    reset_controller.Buttons[b].EndedDown =
        OldKeyBoardController->Buttons[b].EndedDown;
  }

  *KeyBoardController = reset_controller;
  *Mouse = reset_mouse;

  while (PeekMessage(&message, 0, 0, 0, PM_REMOVE)) {
    if (message.message == WM_QUIT) {
      Win32State->Running = false;
    }
    switch (message.message) {
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_XBUTTONDOWN: {
      Win32ProcessMouseButton(&Mouse->Left, message.wParam, MK_LBUTTON);
      Win32ProcessMouseButton(&Mouse->Middle, message.wParam, MK_MBUTTON);
      Win32ProcessMouseButton(&Mouse->Right, message.wParam, MK_RBUTTON);
      Win32ProcessMouseButton(&Mouse->Extra1, message.wParam, MK_XBUTTON1);
      Win32ProcessMouseButton(&Mouse->Extra2, message.wParam, MK_XBUTTON2);
    } break;
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP:
    case WM_XBUTTONUP: {
      Win32ProcessMouseButton(&Mouse->Left, message.wParam, MK_LBUTTON);
      Win32ProcessMouseButton(&Mouse->Middle, message.wParam, MK_MBUTTON);
      Win32ProcessMouseButton(&Mouse->Right, message.wParam, MK_RBUTTON);
      Win32ProcessMouseButton(&Mouse->Extra1, message.wParam, MK_XBUTTON1);
      Win32ProcessMouseButton(&Mouse->Extra2, message.wParam, MK_XBUTTON2);
    } break;
    case WM_MOUSEMOVE: {
      POINT Point;
      Point.x = GET_X_LPARAM(message.lParam);
      Point.y = GET_Y_LPARAM(message.lParam);
      Mouse->MouseX = Point.x;
      Mouse->MouseY = Point.y;
    } break;
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
        Win32State->Running = false;
      }
      if (VKCode == VK_RETURN && AltIsDown && !WasDown && IsDown) {
        Win32ToggleFullScreen(Window);
      }
#ifdef HANDMADE_INTERNAL
      if (VKCode == VK_F5 && !WasDown && IsDown) {
        GlobalDebuggerState.AudioSync = !GlobalDebuggerState.AudioSync;
      }
      if (VKCode == VK_F6 && !WasDown && IsDown) {
        GlobalDebuggerState.RenderPause = !GlobalDebuggerState.RenderPause;
      }
      if (VKCode == VK_F8 && !WasDown && IsDown) {
        if (AltIsDown) {
          Win32SetupGameMemory(Win32State, GameMemory);
          Win32EndInputPlayback(Win32State);
          Win32EndRecordingInput(Win32State);
        } else if (Win32State->InputPlayingIndex != 0) {

          game_controller_input total_reset_controller = {};
          *KeyBoardController = total_reset_controller;
          Win32EndInputPlayback(Win32State);
        } else if (Win32State->InputRecordingIndex == 0) {
          Win32BeginRecordingInput(Win32State, 1);
        } else {
          Win32EndRecordingInput(Win32State);
          Win32BeginInputPlayback(Win32State, 1);
        }
      }
      if (VKCode == VK_F9 && !WasDown && IsDown) {
        GlobalTransparent = !GlobalTransparent;
      }
      if (VKCode == VK_F11 && !WasDown && IsDown) {
        *ShallReload = true;
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

internal void Win32ProcessControllerInput(win32_state *Win32State,
                                          game_input *OldInput,
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

    NewController->isAnalog = OldController->isAnalog;

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

      if (X != 0.0f || Y != 0.0f) {
        NewController->isAnalog = true;
      }
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
      Win32ProcessXInputDigitalButton(Pad->wButtons, XINPUT_GAMEPAD_DPAD_UP,
                                      &OldController->MoveUp,
                                      &NewController->MoveUp);
      Win32ProcessXInputDigitalButton(Pad->wButtons, XINPUT_GAMEPAD_DPAD_DOWN,
                                      &OldController->MoveDown,
                                      &NewController->MoveDown);
      Win32ProcessXInputDigitalButton(Pad->wButtons, XINPUT_GAMEPAD_DPAD_LEFT,
                                      &OldController->MoveLeft,
                                      &NewController->MoveLeft);
      Win32ProcessXInputDigitalButton(Pad->wButtons, XINPUT_GAMEPAD_DPAD_RIGHT,
                                      &OldController->MoveRight,
                                      &NewController->MoveRight);
      Win32ProcessXInputDigitalButton(
          Pad->wButtons, XINPUT_GAMEPAD_LEFT_SHOULDER,
          &OldController->LeftShoulder, &NewController->LeftShoulder);
      Win32ProcessXInputDigitalButton(
          Pad->wButtons, XINPUT_GAMEPAD_RIGHT_SHOULDER,
          &OldController->RightShoulder, &NewController->RightShoulder);
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

internal void Win32DrawFontDigit(win32_offscreen_buffer *ScreenBuffer,
                                 uint8 Digit, int32 Color, int Left, int Bottom,
                                 int Scale) {
  int Top = Bottom - 8 * Scale;
  int Right = Left + 8 * Scale;
  uint8 *Row = (uint8 *)ScreenBuffer->Memory +
               Left * ScreenBuffer->BytesPerPixel +
               Top * ScreenBuffer->Width * ScreenBuffer->BytesPerPixel;
  int Pitch = ScreenBuffer->BytesPerPixel * ScreenBuffer->Width;
  uint8 *Bits = GlobalDebugFont.Numbers[Digit].Bits;
  for (int Y = Top; Y < Bottom && Y < ScreenBuffer->Height; Y++) {
    uint8 *Pixel = (uint8 *)Row;
    for (int X = Left; X < Right && X < ScreenBuffer->Width; X++) {
      int xx = (X - Left) / Scale;
      int yy = (Y - Top) / Scale;
      if (Bits[xx + 8 * yy] != 0) {
        *(uint32 *)Pixel = Color;
      }
      Pixel += ScreenBuffer->BytesPerPixel;
    }
    Row += Pitch;
  }
}

internal void Win32OutputFramerate(win32_offscreen_buffer *ScreenBuffer,
                                   win32_frame_measures *Measures) {
  int PadX = 32;
  int PadY = 32;
  int Height = 16;
  for (int i = 0; i < Measures->DeltaTimeMS; i += 1) {
    Win32DebugDrawVertical(ScreenBuffer, PadX + i * 2, PadY,
                           PadY + Height / (i % 10 == 0 ? 1 : 2), 0xff00ffff);
  }
  int64 Rem = Measures->DeltaTimeMS;
  int Place = 1;
  int DigitCount = 0;
  int Scale = 2;
  do {
    Rem /= 10;
    DigitCount++;
  } while (Rem != 0);
  Rem = Measures->DeltaTimeMS;
  do {
    uint8 Digit = Rem % 10;
    Win32DrawFontDigit(ScreenBuffer, Digit, 0xff00ffff,
                       PadX + (DigitCount - Place) * 8 * Scale, PadY, Scale);
    Rem /= 10;
    Place++;
  } while (Rem != 0);
}

internal void Win32DrawSoundBufferMarker(win32_offscreen_buffer *ScreenBuffer,
                                         real32 Ratio, int PadX, int X, int Top,
                                         int Bottom, int Color, int thickness) {
  Win32DebugDrawVertical(ScreenBuffer, PadX + (int)(Ratio * (real32)X), Top,
                         Bottom, Color);
  for (int t = 1; t < thickness; t += 1) {
    Win32DebugDrawVertical(ScreenBuffer, PadX + (int)(Ratio * (real32)X) + t,
                           Top, Bottom, Color);
    Win32DebugDrawVertical(ScreenBuffer, PadX + (int)(Ratio * (real32)X) - t,
                           Top, Bottom, Color);
  }
}

internal void Win32DebugDrawCircle(win32_offscreen_buffer *Buffer, int CX,
                                   int CY, int Radius, int32 Color) {
  int X = CX - Radius;
  int Y = CY - Radius;
  int Width = 2 * Radius;
  int Height = 2 * Radius;
  if (X < 0) {
    Width += X;
    X = 0;
  }
  if (Y < 0) {
    Height += Y;
    Y = 0;
  }
  if (X >= Buffer->Width || Y >= Buffer->Height) {
    return;
  }
  if (X + Width >= Buffer->Width) {
    Width = Buffer->Width - X;
  }
  if (Y + Height >= Buffer->Height) {
    Height = Buffer->Height - Y;
  }
  size_t Stride = Buffer->Width * Buffer->BytesPerPixel;
  uint8 *Row = (uint8 *)Buffer->Memory + Buffer->BytesPerPixel * X + Stride * Y;
  for (int y = 0; y < Height; y++) {
    uint8 *Pixel = Row;
    for (int x = 0; x < Width; x++) {
      int dx = (X + x - CX);
      int dy = (Y + y - CY);
      if (dx * dx + dy * dy < Radius * Radius) {
        *(int *)Pixel = Color;
      }
      Pixel += Buffer->BytesPerPixel;
    }
    Row += Stride;
  }
}

internal void Win32DebugDrawTriangle(win32_offscreen_buffer *Buffer, int X,
                                     int Y, int Width, int Height,
                                     bool FillLeft, bool FillTop, int Color) {
  int Left = X;
  int Top = Y;
  int Right = X + Width;
  int Bottom = Y + Height;
  if (X < 0) {
    Width += X;
    X = 0;
  }
  if (Y < 0) {
    Height += Y;
    Y = 0;
  }
  if (X >= Buffer->Width || Y >= Buffer->Height) {
    return;
  }
  if (X + Width >= Buffer->Width) {
    Width = Buffer->Width - X;
  }
  if (Y + Height >= Buffer->Height) {
    Height = Buffer->Height - Y;
  }
  size_t Stride = Buffer->Width * Buffer->BytesPerPixel;
  uint8 *Row = (uint8 *)Buffer->Memory + Buffer->BytesPerPixel * X + Stride * Y;
  for (int y = 0; y < Height; y++) {
    uint8 *Pixel = Row;
    for (int x = 0; x < Width; x++) {
      int dx = FillLeft ? (Right - 1 - (X + x)) : (X + x) - Left;
      int dy = FillTop ? (Bottom - 1 - (Y + y)) : (Y + y) - Top;

      if (dy * (Width - 1) <= dx * (Height - 1)) {
        *(int *)Pixel = Color;
      }
      Pixel += Buffer->BytesPerPixel;
    }
    Row += Stride;
  }
}

internal void Win32DisplayRecordingState(win32_offscreen_buffer *ScreenBuffer,
                                         int RecordingIndex, int PlayingIndex) {
  int Padding = 16;
  int Radius = 16;
  int32 Color = 0xff0000;
  if (RecordingIndex) {
    Win32DebugDrawCircle(ScreenBuffer, Padding + Radius, Padding + Radius,
                         Radius, Color);
  } else if (PlayingIndex) {
    Win32DebugDrawTriangle(ScreenBuffer, Padding, Padding, Radius * 2, Radius,
                           true, true, Color);
    Win32DebugDrawTriangle(ScreenBuffer, Padding, Padding + Radius, Radius * 2,
                           Radius, true, false, Color);
  }
}

internal void Win32DebugSyncDisplay(win32_offscreen_buffer *ScreenBuffer,
                                    win32_sound_buffer *SoundBuffer,
                                    int DebugTimeMarkerCount,
                                    size_t CurrentCursorPos,
                                    win32_debug_time_marker *DebugTimeMarker,
                                    real32 TargerSecondsPerFrame) {
  int PadX = 16;
  int PadY = 16;
  int LineHeight = 64;

  real32 Ratio =
      (ScreenBuffer->Width - 2 * PadX) / ((real32)SoundBuffer->SoundBufferSize);
  for (int CursorIndex = 0; CursorIndex < DebugTimeMarkerCount; CursorIndex++) {
    win32_debug_time_marker *Marker = &DebugTimeMarker[CursorIndex];

    Assert(Marker->OutputPlayCursor < SoundBuffer->SoundBufferSize);
    Assert(Marker->OutputWriteCursor < SoundBuffer->SoundBufferSize);
    Assert(Marker->OutputLocation < SoundBuffer->SoundBufferSize);
    Assert(Marker->OutputByteCount < SoundBuffer->SoundBufferSize);

    int Top = PadY;
    int Bottom = Top + LineHeight;

    if (CurrentCursorPos == CursorIndex) {

      Top += LineHeight;
      Bottom += LineHeight;

      Win32DrawSoundBufferMarker(ScreenBuffer, Ratio, PadX,
                                 Marker->OutputPlayCursor, Top, Bottom,
                                 0x00ff00, 2);
      Win32DrawSoundBufferMarker(ScreenBuffer, Ratio, PadX,
                                 Marker->OutputWriteCursor, Top, Bottom,
                                 0xff0000, 2);
      int XOutputStart = Marker->OutputLocation;
      int XOutputEnd = XOutputStart + Marker->OutputByteCount;

      Top += LineHeight;
      Bottom += LineHeight;

      Win32DrawSoundBufferMarker(ScreenBuffer, Ratio, PadX, XOutputStart, Top,
                                 Bottom, 0x00ff00, 2);
      Win32DrawSoundBufferMarker(ScreenBuffer, Ratio, PadX, XOutputEnd, Top,
                                 Bottom, 0xff0000, 2);

      Top += LineHeight;
      Bottom += LineHeight;

      Win32DrawSoundBufferMarker(ScreenBuffer, Ratio, PadX,
                                 Marker->ExpectedFlipPlayCursor, PadY, Bottom,
                                 0xffff00, 2);
    } else {

      Win32DrawSoundBufferMarker(ScreenBuffer, Ratio, PadX,
                                 Marker->OutputPlayCursor, Top, Bottom,
                                 0xff00ff, 1);
      Win32DrawSoundBufferMarker(ScreenBuffer, Ratio, PadX,
                                 Marker->OutputWriteCursor, Top, Bottom,
                                 0x00ffff, 1);
    }
  }
}

internal void ConcatStrings(size_t SourceACount, char *SourceA,
                            size_t SourceBCount, char *SourceB,
                            size_t DestCount, char *Dest) {
  DestCount--;
  for (int Index = 0; Index < SourceACount && DestCount--; ++Index) {
    *Dest++ = *SourceA++;
  }
  for (int Index = 0; Index < SourceBCount && DestCount--; ++Index) {
    *Dest++ = *SourceB++;
  }
  *Dest++ = 0;
}

internal FILETIME Win32GetLastWriteTime(LPCSTR Filename) {
  FILETIME LastWriteTime = {};

  WIN32_FILE_ATTRIBUTE_DATA Data = {};
  if (GetFileAttributesEx(Filename, GetFileExInfoStandard, &Data)) {
    LastWriteTime = Data.ftLastWriteTime;
  }

  return LastWriteTime;
}

internal void Win32GetPathRelativeToExecutable(size_t *PathLength,
                                               DWORD MaxLength,
                                               char *FileName) {
  DWORD len = GetModuleFileNameA(NULL, FileName, MaxLength);
  size_t Last = 0;
  for (size_t l = 0; l < len; l++) {
    if (FileName[l] == '\0') {
      break;
    }
    if (FileName[l] == '\\') {
      Last = l;
    }
  }
  *PathLength = Last + 1;
}

internal win32_game LoadGame(char *SourceDLL, char *TempDLLPath) {
  win32_game Result = {};

  CopyFile(SourceDLL, TempDLLPath, FALSE);
  Result.Dll = LoadLibraryA(TempDLLPath);
  if (Result.Dll) {
    FILETIME LatestWriteTime = Win32GetLastWriteTime(TempDLLPath);
    Result.GetSoundSamples = (game_get_sound_samples *)GetProcAddress(
        Result.Dll, "GameGetSoundSamples");
    Result.UpdateAndRender = (game_update_and_render *)GetProcAddress(
        Result.Dll, "GameUpdateAndRender");
    Result.LastWriteTime = LatestWriteTime;
    Result.IsValid = Result.GetSoundSamples && Result.UpdateAndRender;
  }

  if (!Result.IsValid) {
    Result.GetSoundSamples = &GameGetSoundSamplesStub;
    Result.UpdateAndRender = &GameUpdateAndRenderStub;
  }

  return Result;
}

internal void UnloadGame(win32_game *Game) {
  Game->GetSoundSamples = &GameGetSoundSamplesStub;
  Game->UpdateAndRender = &GameUpdateAndRenderStub;

  if (Game->Dll) {
    FreeLibrary(Game->Dll);
    Game->Dll = NULL;
    Game->IsValid = false;
  }
}
int CALLBACK WinMain(HINSTANCE Instance, HINSTANCE PrevInstance,
                     LPSTR lpCmdLine, int nCmdShow) {
  char ExePath[MAX_PATH];
  size_t ExePathLength = 0;

  thread_context Context = {};

  win32_state Win32State = {};
  Win32GetPathRelativeToExecutable(&ExePathLength, sizeof(ExePath), ExePath);
  char SourceGameCodeDLLFilename[] = "handmade.dll";
  char SourceGameCodeDLLFullPath[MAX_PATH];
  char TargetGameCodeDLLFilename[] = "handmade_temp.dll";
  char TargetGameCodeDLLFullPath[MAX_PATH];

  char LockFileFilename[] = "hotreload.lock";
  char LockFileFullPath[MAX_PATH];

  ConcatStrings(ExePathLength, ExePath, sizeof(SourceGameCodeDLLFilename),
                SourceGameCodeDLLFilename, sizeof(SourceGameCodeDLLFullPath),
                SourceGameCodeDLLFullPath);
  ConcatStrings(ExePathLength, ExePath, sizeof(TargetGameCodeDLLFilename),
                TargetGameCodeDLLFilename, sizeof(TargetGameCodeDLLFullPath),
                TargetGameCodeDLLFullPath);

  ConcatStrings(ExePathLength, ExePath, sizeof(LockFileFilename),
                LockFileFilename, sizeof(LockFileFullPath), LockFileFullPath);

  OutputDebugString(TargetGameCodeDLLFullPath);

  win32_game Game =
      LoadGame(SourceGameCodeDLLFullPath, TargetGameCodeDLLFullPath);
  LARGE_INTEGER LastCounter;
  LARGE_INTEGER PerfCounterFrequency;
  LARGE_INTEGER FlipWallClock = Win32GetWallClock();
  int64 LastCycleCount = __rdtsc();
  UINT DesiredSchedulerMS = 1;
  bool SleepIsGranular =
      (timeBeginPeriod(DesiredSchedulerMS) == TIMERR_NOERROR);

  QueryPerformanceFrequency(&PerfCounterFrequency);
  GlobalPerfCounterFrequency = PerfCounterFrequency.QuadPart;
  QueryPerformanceCounter(&LastCounter);
  // MessageBox(0, "This is me", "Test", MB_OK|MB_ICONINFORMATION);
  WNDCLASS windowClass = {};
  windowClass.style = CS_HREDRAW | CS_VREDRAW;
  windowClass.lpfnWndProc = *Win32MainWindowCallback;
  windowClass.hInstance = Instance;
  windowClass.hbrBackground = NULL;
  windowClass.hCursor = LoadCursorA(0, IDC_ARROW);
  // windowClass.hIcon = ;
  windowClass.lpszClassName = "HandmadeHeroWindowClass";
  if (RegisterClass(&windowClass)) {
    HWND Window = CreateWindowEx(
        WS_EX_LAYERED | WS_EX_TOPMOST, windowClass.lpszClassName,
        "Handmade Window", WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, Instance, 0);
    if (Window) {

      HDC DeviceContext = GetDC(Window);
      uint32 TargetFrameHz = 60;
      int Win32RefreshRate = GetDeviceCaps(DeviceContext, VREFRESH);
      if (Win32RefreshRate > 1) {
        TargetFrameHz = Win32RefreshRate;
      }
      real32 TargetSecondsPerFrame = 1.0f / TargetFrameHz;
      Win32LoadXInput();
      Win32State.Running = true;
      win32_sound_output SoundOutput;
      SoundOutput.SamplingRateInHz = 48000;
      SoundOutput.RunningSampleIndex = 0;
      SoundOutput.BytesPerSample = sizeof(uint16) * 2;
      SoundOutput.SafetySampleBytes = SoundOutput.SamplingRateInHz *
                                      SoundOutput.BytesPerSample /
                                      TargetFrameHz / 2;

      win32_sound_buffer SoundBuffer = {};
      SoundBuffer.BytesPerSample = SoundOutput.BytesPerSample;
      SoundBuffer.SoundBufferSize =
          SoundOutput.SamplingRateInHz * SoundBuffer.BytesPerSample;
      Win32InitDSound(Window, SoundOutput.SamplingRateInHz,
                      SoundBuffer.SoundBufferSize, &SoundBuffer);
      Win32ClearSoundBuffer(&SoundBuffer);
      HRESULT Res = SoundBuffer.Buffer->Play(0, 0, DSBPLAY_LOOPING);

      int16 *Samples =
          (int16 *)VirtualAlloc(0, SoundBuffer.SoundBufferSize,
                                MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

#if HANDMADE_INTERNAL
      LPVOID BaseAddress = (LPVOID)Terabytes((uint64)2);
#else
      LPVOID BaseAddress = 0;
#endif

      game_memory GameMemory = {};
      Win32SetupGameMemory(&Win32State, &GameMemory);
      if (Samples && GameMemory.PermanentStorage &&
          GameMemory.TransientStorage) {

        win32_debug_time_marker DebugTimeMarkers[30] = {};
        win32_frame_measures FrameMeasures = {};
        size_t TimeMarkerCursor = 0;
        LARGE_INTEGER LastFrame = Win32GetWallClock();

        bool SoundIsValid = false;
        game_input Inputs[2] = {};
        game_input *NewInput = &Inputs[0];
        game_input *OldInput = &Inputs[1];

        // Choose pixel format
        PIXELFORMATDESCRIPTOR pfd = {0};
        pfd.nSize = sizeof(pfd);
        pfd.nVersion = 1;
        pfd.dwFlags =
            PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        pfd.iPixelType = PFD_TYPE_RGBA;
        pfd.cColorBits = 24;
        pfd.cDepthBits = 16;
        pfd.iLayerType = PFD_MAIN_PLANE;

        int pf = ChoosePixelFormat(DeviceContext, &pfd);
        SetPixelFormat(DeviceContext, pf, &pfd);

        // Create rendering context
        HGLRC hglrc = wglCreateContext(DeviceContext);
        wglMakeCurrent(DeviceContext, hglrc);
        GLuint tex;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);

        // Simple texture parameters
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
        while (Win32State.Running) {
          bool ShallReload = false;

          FILETIME LockFileTime = Win32GetLastWriteTime(LockFileFullPath);
          FILETIME LatestWriteTime =
              Win32GetLastWriteTime(SourceGameCodeDLLFullPath);
          if (CompareFileTime(&LatestWriteTime, &Game.LastWriteTime) != 0 &&
              (LockFileTime.dwHighDateTime != 0 ||
               LockFileTime.dwLowDateTime != 0)) {
            ShallReload = true;
          }

          Win32ProcessPendingMessages(Window, &Win32State, &GameMemory,
                                      OldInput, NewInput, &ShallReload);
          Win32ProcessControllerInput(&Win32State, OldInput, NewInput);

          if (ShallReload) {
            UnloadGame(&Game);
            Game =
                LoadGame(SourceGameCodeDLLFullPath, TargetGameCodeDLLFullPath);
          }

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
            Res = SoundBuffer.Buffer->GetCurrentPosition(&PlayCursor,
                                                         &WriteCursor);
            if (SUCCEEDED(Res)) {
              if (!SoundIsValid) {
                SoundOutput.RunningSampleIndex =
                    WriteCursor / SoundBuffer.BytesPerSample;
                SoundIsValid = true;
              }
              BytesToLock = SoundOutput.RunningSampleIndex *
                            SoundBuffer.BytesPerSample %
                            SoundBuffer.SoundBufferSize;
              DWORD ExpectedSoundBytesPerFrame =
                  (DWORD)((real32)SoundOutput.SamplingRateInHz *
                          (real32)SoundBuffer.BytesPerSample /
                          (real32)TargetFrameHz);

              LARGE_INTEGER AudioWallClock = Win32GetWallClock();
              real32 FromBeginToAudioSeconds =
                  Win32GetSecondsElapsed(FlipWallClock, AudioWallClock);
              real32 SecondsLeftUntilFlip =
                  TargetSecondsPerFrame - FromBeginToAudioSeconds;
              DWORD ExpectedBytesUntilFlip =
                  (DWORD)((SecondsLeftUntilFlip / TargetSecondsPerFrame) *
                          (real32)ExpectedSoundBytesPerFrame);
              DWORD ExpectedFrameBoundaryByte =
                  PlayCursor + ExpectedBytesUntilFlip;
              DWORD SafeWriteCursor = WriteCursor;
              if (SafeWriteCursor < PlayCursor) {
                SafeWriteCursor += SoundBuffer.SoundBufferSize;
              }
              Assert(SafeWriteCursor >= PlayCursor);
              SafeWriteCursor += SoundOutput.SafetySampleBytes;
              bool AudioCardIsLowLatency =
                  SafeWriteCursor < ExpectedFrameBoundaryByte;

              DWORD TargetCursor = 0;
              if (AudioCardIsLowLatency) {
                TargetCursor =
                    (ExpectedFrameBoundaryByte + ExpectedSoundBytesPerFrame);
              } else {
                TargetCursor = (WriteCursor + ExpectedSoundBytesPerFrame +
                                SoundOutput.SafetySampleBytes);
              }
              TargetCursor = TargetCursor % SoundBuffer.SoundBufferSize;

              if (BytesToLock > TargetCursor) {
                BytesToWrite = SoundBuffer.SoundBufferSize - BytesToLock;
                BytesToWrite += TargetCursor;
              } else {
                BytesToWrite = TargetCursor - BytesToLock;
              }

              // Assert(BytesToWrite > 0);

#ifdef HANDMADE_INTERNAL
              {
                win32_debug_time_marker *Marker =
                    &DebugTimeMarkers[TimeMarkerCursor];
                Marker->OutputPlayCursor = PlayCursor;
                Marker->OutputWriteCursor = WriteCursor;
                Marker->OutputLocation = BytesToLock;
                Marker->OutputByteCount = BytesToWrite;
                Marker->ExpectedFlipPlayCursor = ExpectedFrameBoundaryByte;
                int DistanceToPlay =
                    (BytesToLock - PlayCursor + SoundBuffer.SoundBufferSize) %
                    SoundBuffer.SoundBufferSize;
                int DistanceToWrite =
                    (TargetCursor - PlayCursor + SoundBuffer.SoundBufferSize) %
                    SoundBuffer.SoundBufferSize;
                char OutputBuffer[256] = {};
                wsprintf(OutputBuffer, "DTP: %d, DTW: %d\n", DistanceToPlay,
                         DistanceToWrite);
                OutputDebugString(OutputBuffer);
              }
#endif
              game_sound_output_buffer GameSoundBuffer = {};
              GameSoundBuffer.SamplesPerSecond = SoundOutput.SamplingRateInHz;
              GameSoundBuffer.SampleCount =
                  BytesToWrite /
                  SoundBuffer
                      .BytesPerSample; // SoundBuffer.SamplesPerSecond / 30;
              GameSoundBuffer.Samples = Samples;
              Game.GetSoundSamples(&Context, &GameMemory, &GameSoundBuffer);
              Win32FillSoundBuffer(BytesToLock, BytesToWrite, &SoundOutput,
                                   &GameSoundBuffer, &SoundBuffer);
            } else {
              SoundIsValid = false;
            }

            if (Win32State.InputRecordingIndex) {
              Win32RecordInput(&Win32State, NewInput);
            }
            if (Win32State.InputPlayingIndex) {
              Win32PlaybackInput(&Win32State, NewInput);
            }

            NewInput->DeltaTime = FrameMeasures.DeltaTimeMS / 1000.0f;
            ClearRenderBuffer(&Win32State.RenderBuffer, ScreenBuffer.Width,
                              ScreenBuffer.Height);
            bool ShallExit =
                !Game.UpdateAndRender(&Context, &GameMemory, NewInput,
                                      &ScreenBuffer, &Win32State.RenderBuffer);

            Win32State.Running = Win32State.Running && !ShallExit;

#ifdef HANDMADE_INTERNAL
            if (GlobalDebuggerState.AudioSync) {

              Win32DebugSyncDisplay(&GlobalScreenBuffer, &SoundBuffer,
                                    ArrayCount(DebugTimeMarkers),
                                    TimeMarkerCursor, DebugTimeMarkers,
                                    TargetSecondsPerFrame);
            }
#endif
            Win32DisplayRecordingState(&GlobalScreenBuffer,
                                       Win32State.InputRecordingIndex,
                                       Win32State.InputPlayingIndex);

            LARGE_INTEGER WorkFrame = Win32GetWallClock();
            real32 WorkSecondsEllapsed =
                Win32GetSecondsElapsed(LastFrame, WorkFrame);
            real32 SecondsElapsedForFrame = WorkSecondsEllapsed;

            if (SecondsElapsedForFrame < TargetSecondsPerFrame) {
              while (SecondsElapsedForFrame < TargetSecondsPerFrame) {
                if (SleepIsGranular) {

                  DWORD SleepMs =
                      (DWORD)((TargetSecondsPerFrame - SecondsElapsedForFrame) /
                              1000.0f);
                  Sleep(SleepMs);
                }
                SecondsElapsedForFrame =
                    Win32GetSecondsElapsed(LastFrame, Win32GetWallClock());
              }
              Assert(SecondsElapsedForFrame <= 2 * TargetSecondsPerFrame);
              FrameMeasures.SkippedFrames = 0;
            } else {
              // Assert(false);
              FrameMeasures.SkippedFrames += 1;
              // OutputDebugString("SkippedFrame");
            }
            Win32OutputFramerate(&GlobalScreenBuffer, &FrameMeasures);
            LARGE_INTEGER EndFrame = Win32GetWallClock();
            LastFrame = EndFrame;
            FlipWallClock = Win32GetWallClock();
            // InvalidateRect(Window, 0, FALSE);

            glViewport(0, 0, ScreenBuffer.Width, ScreenBuffer.Height);
            glClearColor(0.1f, 0.1f, 0.3f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ScreenBuffer.Width,
                         ScreenBuffer.Height, 0, GL_BGRA, GL_UNSIGNED_BYTE,
                         ScreenBuffer.Memory);
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, tex);
            glBegin(GL_TRIANGLES);
            // glColor3f(1, 0, 0);
            glColor3f(1.0f, 1.0f, 1.0f);
            glTexCoord2f(0.0f, 1.0f);
            glVertex2f(-1.0f, -1.0f);
            // glColor3f(0, 1, 0);
            glTexCoord2f(1.0f, 1.0f);
            glVertex2f(1.0f, -1.0f);
            // glColor3f(0, 0, 1);
            glTexCoord2f(1.0f, 0.0f);
            glVertex2f(1.0f, 1.0f);

            // glColor3f(0, 0, 1);
            glTexCoord2f(1.0f, 0.0f);
            glVertex2f(1.0f, 1.0f);
            // glColor3f(0, 1, 1);
            glTexCoord2f(0.0f, 0.0f);
            glVertex2f(-1.0f, 1.0f);
            // glColor3f(1, 0, 0);
            glTexCoord2f(0.0f, 1.0f);
            glVertex2f(-1.0f, -1.0f);
            glEnd();
            glDisable(GL_TEXTURE_2D);
            for (uint8 ri = 0; ri < Win32State.RenderBuffer.Count; ri += 1) {

              render_command *RCmd = &Win32State.RenderBuffer.Base[ri];
              switch (RCmd->Type) {
              case RenderCommandRect: {
                glBegin(GL_TRIANGLES);
                glColor3f(0.0f, 0.0f, 0.0f);
                glVertex2f(RCmd->Rect.MinX, RCmd->Rect.MinY);
                glColor3f(0.0f, 0.0f, 0.0f);
                glVertex2f(RCmd->Rect.MaxX, RCmd->Rect.MinY);
                glColor3f(0.0f, 0.0f, 0.0f);
                glVertex2f(RCmd->Rect.MaxX, RCmd->Rect.MaxY);

                glColor3f(0.0f, 0.0f, 0.0f);
                glVertex2f(RCmd->Rect.MaxX, RCmd->Rect.MaxY);
                glColor3f(0.0f, 0.0f, 0.0f);
                glVertex2f(RCmd->Rect.MinX, RCmd->Rect.MaxY);
                glColor3f(0.0f, 0.0f, 0.0f);
                glVertex2f(RCmd->Rect.MinX, RCmd->Rect.MinY);
                glEnd();
              } break;
              default: {
                  break;
              } break;
              }
            }

            SwapBuffers(DeviceContext);

            int64 EndCycleCount = __rdtsc();

            LARGE_INTEGER EndCounter;
            QueryPerformanceCounter(&EndCounter);
            int64 DeltaCycles = EndCycleCount - LastCycleCount;
            int64 DeltaTime = EndCounter.QuadPart - LastCounter.QuadPart;
            LONGLONG DeltaTimeMS =
                DeltaTime * 1000 / PerfCounterFrequency.QuadPart;
            LONGLONG fps = PerfCounterFrequency.QuadPart / DeltaTime;
            FrameMeasures.DeltaTimeMS = DeltaTimeMS;
            FrameMeasures.fps = fps;
            FrameMeasures.DeltaCycles = DeltaCycles;
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
#ifdef HANDMADE_INTERNAL
            {
              TimeMarkerCursor++;
              if (TimeMarkerCursor >= ArrayCount(DebugTimeMarkers)) {
                TimeMarkerCursor = 0;
              }
            }
#endif
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
