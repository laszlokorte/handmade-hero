#include <windows.h>
#include <wingdi.h>
#include <winuser.h>

LRESULT CALLBACK MainWindowCallback(
  HWND Window,
  UINT Message,
  WPARAM WParam,
  LPARAM LParam
)
{
    LRESULT Result = 0;
    switch(Message) {
        case WM_SIZE: {
            OutputDebugString("WM_SIZE\n");
        } break;

        case WM_DESTROY: {
            OutputDebugString("WM_DETROY\n");
        } break;

        case WM_CLOSE: {
            PostQuitMessage(0);
            OutputDebugString("WM_CLOSE\n");
        } break;

        case WM_ACTIVATEAPP: {
            OutputDebugString("WM_ACTIVATEAPP\n");
        } break;

        case WM_PAINT: {
           PAINTSTRUCT Paint;
           HDC DeviceContext = BeginPaint(Window, &Paint);
           int x = Paint.rcPaint.left;
           int y = Paint.rcPaint.top;
           int width = Paint.rcPaint.right - Paint.rcPaint.left;
           int height = Paint.rcPaint.bottom - Paint.rcPaint.top;
           static DWORD Color = WHITENESS;
           PatBlt(DeviceContext, x, y, width, height, Color);
           EndPaint(Window, &Paint);
           OutputDebugString("WM_PAINT\n");
           if(Color == WHITENESS) {
               Color = BLACKNESS;
           } else {
               Color = WHITENESS;
           }
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
    windowClass.lpfnWndProc = *MainWindowCallback;
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
            MSG message;
            for(;;) {
               BOOL MessageResult = GetMessage(&message, 0, 0, 0);
               if(MessageResult > 0) {
                   TranslateMessage(&message);
                   DispatchMessage(&message);
               } else {
                   break;
               }
            }
        }
    } else {

    }
    return 0;
}
