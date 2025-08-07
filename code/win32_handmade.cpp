#include <basetsd.h>
#include <cstdint>
#include <windows.h>
#include <stdint.h>
#include <Xinput.h>

#define local_persist static
#define global_variable static
#define internal static

global_variable bool Running;

struct game_state {
    uint32_t time;
    int xpos;
    int ypos;
};

global_variable game_state global_game_state = {};

struct win32_offscreen_buffer {
    BITMAPINFO Info;
    void * Memory;
    int width;
    int height;
    int BytesPerPixel;
};

global_variable win32_offscreen_buffer GlobalBackBuffer;

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


#define X_INPUT_GET_STATE(name) DWORD (WINAPI name)(DWORD wUserIndex, XINPUT_STATE* pState)
#define X_INPUT_SET_STATE(name) DWORD (WINAPI name)(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration)
typedef X_INPUT_GET_STATE(x_input_get_state);
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_GET_STATE(XInputGetStateStub)
{
    return 0;
}
X_INPUT_SET_STATE(XInputSetStateStub)
{
    return 0;
}
global_variable x_input_get_state *XInputGetState_ = XInputGetStateStub;
global_variable x_input_set_state *XInputSetState_ = XInputSetStateStub;
#define XInputGetState XInputGetState_
#define XInputSetState XInputSetState_

internal void Win32LoadXInput(void) {
    HMODULE XInputLibrary = LoadLibrary("XInput1_4.dll");
    if(XInputLibrary) {
        XInputGetState = (x_input_get_state *) GetProcAddress(XInputLibrary, "XInputGetState");
        XInputSetState = (x_input_set_state *) GetProcAddress(XInputLibrary, "XInputSetState");
    }
}
internal void RenderGradient(win32_offscreen_buffer Buffer, int xoff, int yoff) {
    unsigned int* canvas = (unsigned int *) (Buffer.Memory);
    int cx = Buffer.width/ 2;
    int cy = Buffer.height / 2;
    for(int x=0;x<Buffer.width;x++) {
        for(int y=0;y<Buffer.height;y++) {
            int yy = y - cy ;
            int xx = x - cx;
            //if(y > 0 &&y < Buffer.height && x > 0 && x < Buffer.width) {
                //canvas[(yy) * width + (xx)] = 0x0000aaff;

                uint8_t green = xx+xoff;
                uint8_t blue = yy+yoff;
                canvas[(y) * Buffer.width + (x)] = (green << 8) | blue;
                //}
        }
   }
}
internal void ResizeDIBSection(win32_offscreen_buffer* buffer, int width, int height) {
    if(buffer->Memory) {
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
    buffer->Memory = VirtualAlloc(0, allocBytes, MEM_COMMIT, PAGE_READWRITE);

    RenderGradient(*buffer, global_game_state.xpos, global_game_state.ypos);
}



internal void Win32DisplayBufferWindow(HDC DeviceContext, int WindowWidth, int WindowHeight, win32_offscreen_buffer* Buffer) {
    StretchDIBits(
        DeviceContext,
        0, 0, WindowWidth, WindowHeight,
        0, 0, Buffer->width, Buffer->height,
        Buffer->Memory,
        &(Buffer->Info),
        DIB_RGB_COLORS,
        SRCCOPY);
}

LRESULT CALLBACK Win32MainWindowCallback(
  HWND Window,
  UINT Message,
  WPARAM WParam,
  LPARAM LParam
)
{
    LRESULT Result = 0;
    switch(Message) {
        case WM_SIZE: {
            win32_window_dimensions WinSize = Win32GetWindowSize(Window);
            ResizeDIBSection(&GlobalBackBuffer, WinSize.width, WinSize.height);
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
            SetCursor(0);
        } break;

        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYUP:
        case WM_KEYDOWN: {
            #define WAS_DOWN_MASK (1 << 30)
            #define IS_DOWN_MASK (1 << 31)
            uint32_t VKCode = WParam;
            bool WasDown = (LParam & WAS_DOWN_MASK) != 0;
            bool IsDown = (LParam & IS_DOWN_MASK) == 0;

            if(VKCode == 'W' && IsDown) {
                global_game_state.ypos += 10;
            }
            if(VKCode == 'A' && IsDown) {
                global_game_state.xpos -= 10;
            }
            if(VKCode == 'S' && IsDown) {
                global_game_state.ypos -= 10;
            }
            if(VKCode == 'D' && IsDown) {
                global_game_state.xpos += 10;
            }
            if(VKCode == VK_ESCAPE && (!WasDown) && IsDown) {
                Running = false;
            }
        } break;

        case WM_PAINT: {
           PAINTSTRUCT Paint;
           HDC DeviceContext = BeginPaint(Window, &Paint);
           RECT ClientRect;
           win32_window_dimensions WinSize = Win32GetWindowSize(Window);
           GetClientRect(Window, &ClientRect);
           Win32DisplayBufferWindow(DeviceContext, WinSize.width, WinSize.height, &GlobalBackBuffer);
           EndPaint(Window, &Paint);
           OutputDebugStringA("WM_PAINT\n");
        } break;

        default: {
            Result = DefWindowProc(Window, Message, WParam, LParam);
        } break;
    }
    return Result;

}



int CALLBACK
WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // MessageBox(0, "This is me", "Test", MB_OK|MB_ICONINFORMATION);
    WNDCLASS windowClass = {};
    windowClass.style = CS_OWNDC|CS_HREDRAW|CS_VREDRAW;
    windowClass.lpfnWndProc = *Win32MainWindowCallback;
    windowClass.hInstance = Instance;
    // windowClass.hIcon = ;
    windowClass.lpszClassName = "HandmadeHeroWindowClass";
    if(RegisterClass(&windowClass)) {
        HWND windowHandle = CreateWindowEx(
            0,
            windowClass.lpszClassName,
            "Handmade Window",
            WS_OVERLAPPEDWINDOW|WS_VISIBLE,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            0,
            0,
            Instance,
            0
        );
        if(windowHandle) {

            Running = true;
            while(Running) {
                global_game_state.time++;
                MSG message;
                while(PeekMessage(&message,0,0,0, PM_REMOVE)) {
                    if(message.message == WM_QUIT) {
                        Running = false;
                    }
                    TranslateMessage(&message);
                    DispatchMessage(&message);
                }

                for(DWORD ControllerIndex=0;ControllerIndex < XUSER_MAX_COUNT; ++ControllerIndex) {
                    XINPUT_STATE ControllerState;
                    if(XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS) {
                        XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;

                        bool DPadUp = Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP;
                        bool DPadLeft = Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT;
                        bool DPadRight = Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT;
                        bool DPadDown = Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN;
                    } else {
                        // Not available
                    }
                }

                RenderGradient(GlobalBackBuffer, global_game_state.xpos, global_game_state.ypos);
                InvalidateRect(windowHandle, 0, FALSE);
            }
        }
    } else {

    }
    return 0;
}
