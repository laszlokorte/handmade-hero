#include <windows.h>

#define local_persist static
#define global_variable static
#define internal static

global_variable bool Running;

global_variable BITMAPINFO BitmapInfo;
global_variable void *BitmapMemory;
global_variable HBITMAP BitmapHandle;
global_variable HDC DeviceContext;

internal void ResizeDIBSection(int width, int height) {
    if(BitmapHandle) {
        DeleteObject(BitmapHandle);
    }
    BitmapInfo.bmiHeader .biSize = sizeof(BitmapInfo.bmiHeader);
    BitmapInfo.bmiHeader.biWidth = width;
    BitmapInfo.bmiHeader.biHeight = height;
    BitmapInfo.bmiHeader.biPlanes = 1;
    BitmapInfo.bmiHeader.biBitCount = 32;
    BitmapInfo.bmiHeader.biCompression = BI_RGB;
    BitmapInfo.bmiHeader.biSizeImage = 0;
    BitmapInfo.bmiHeader.biXPelsPerMeter = 0;
    BitmapInfo.bmiHeader.biYPelsPerMeter = 0;
    BitmapInfo.bmiHeader.biClrUsed = 0;
    BitmapInfo.bmiHeader.biClrImportant = 0;
    if(!DeviceContext) {
        DeviceContext = CreateCompatibleDC(0);
    }
    BitmapHandle = CreateDIBSection(DeviceContext, &BitmapInfo, DIB_RGB_COLORS, &BitmapMemory, 0, 0);
    unsigned int* canvas = (unsigned int *) BitmapMemory;
    int cx = width/ 2;
    int cy = height / 2;
    for(int x=-5;x<5;x++) {
        for(int y=-5;y<5;y++) {
            int yy = cy + y;
            int xx = cx + x;
            if(yy > 0 && yy < height && xx > 0 && xx < width) {
                canvas[(yy) * width + (xx)] = 0xffffff;
            }
        }
   }
}

void Win32UpdateWindow(HDC DeviceContext, int x, int y, int width, int height) {
    StretchDIBits(DeviceContext, x, y, width, height, x, y, width, height, BitmapMemory, &BitmapInfo, DIB_RGB_COLORS, SRCCOPY);
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
            RECT ClientRect;
            GetClientRect(Window, &ClientRect);
            int width = ClientRect.right - ClientRect.left;
            int height = ClientRect.bottom - ClientRect.top;
            ResizeDIBSection(width, height);
            OutputDebugString("WM_SIZE\n");
        } break;

        case WM_DESTROY: {
            Running = false;
            OutputDebugString("WM_DETROY\n");
        } break;

        case WM_CLOSE: {
            // PostQuitMessage(0);
            Running = false;
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
           Win32UpdateWindow(DeviceContext, x, y, width, height);

           OutputDebugString("WM_PAINT\n");
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
            MSG message;
            Running = true;
            while(Running) {
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
