#include <windows.h>
#include <winuser.h>

#define local_persist static
#define global_variable static
#define internal static

global_variable bool Running;

global_variable BITMAPINFO BitmapInfo;
global_variable void *BitmapMemory;
global_variable int BitMapWidth;
global_variable int BitMapHeight;

void RenderGradient(int xoff, int yoff) {
    unsigned int* canvas = (unsigned int *) BitmapMemory;
    int cx = BitMapWidth / 2;
    int cy = BitMapHeight / 2;
    for(int x=0;x<BitMapWidth;x++) {
        for(int y=0;y<BitMapHeight;y++) {
            int yy = y - cy ;
            int xx = x - cx;
            if(y > 0 &&y < BitMapHeight && x > 0 && x < BitMapWidth) {
                //canvas[(yy) * width + (xx)] = 0x0000aaff;
                canvas[(y) * BitMapWidth+ (x)] = ((xx+xoff) << 8) + (yy+yoff);
            }
        }
   }
}
internal void ResizeDIBSection(int width, int height) {
    if(BitmapMemory) {
        VirtualFree(BitmapMemory, 0, MEM_RELEASE);
    }
    BitMapWidth = width;
    BitMapHeight = height;

    BitmapInfo.bmiHeader .biSize = sizeof(BitmapInfo.bmiHeader);
    BitmapInfo.bmiHeader.biWidth = BitMapWidth;
    BitmapInfo.bmiHeader.biHeight = -BitMapHeight;
    BitmapInfo.bmiHeader.biPlanes = 1;
    BitmapInfo.bmiHeader.biBitCount = 32;
    BitmapInfo.bmiHeader.biCompression = BI_RGB;
    BitmapInfo.bmiHeader.biSizeImage = 0;
    BitmapInfo.bmiHeader.biXPelsPerMeter = 0;
    BitmapInfo.bmiHeader.biYPelsPerMeter = 0;
    BitmapInfo.bmiHeader.biClrUsed = 0;
    BitmapInfo.bmiHeader.biClrImportant = 0;
    int bytesPerPixel = BitMapWidth * BitMapHeight * 4;
    BitmapMemory = VirtualAlloc(0, bytesPerPixel, MEM_COMMIT, PAGE_READWRITE);

    RenderGradient(0, 0);
}



void Win32UpdateWindow(HDC DeviceContext, RECT *ClientRect, int x, int y, int width, int height) {
    //StretchDIBits(DeviceContext, x, y, width, height, x, y, width, height, BitmapMemory, &BitmapInfo, DIB_RGB_COLORS, SRCCOPY);
    int WindowWidth = ClientRect->right - ClientRect->left;
    int WindowHeight = ClientRect->bottom - ClientRect->top;
    StretchDIBits(DeviceContext, 0, 0, BitMapWidth, BitMapHeight, 0, 0, WindowWidth, WindowHeight, BitmapMemory, &BitmapInfo, DIB_RGB_COLORS, SRCCOPY);
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
           RECT ClientRect;
           GetClientRect(Window, &ClientRect);
           Win32UpdateWindow(DeviceContext, &ClientRect, x, y, width, height);

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
            int xoff = 0;
            int yoff = 0;

            Running = true;
            while(Running) {
                MSG message;
                while(PeekMessage(&message,0,0,0, PM_REMOVE)) {
                    if(message.message == WM_QUIT) {
                        Running = false;
                    }
                    TranslateMessage(&message);
                    DispatchMessage(&message);
                }
                HDC DeviceContext = GetDC(windowHandle);
                RECT ClientRect;
                GetClientRect(windowHandle, &ClientRect);
                int winWidth = ClientRect.right - ClientRect.left;
                int winHeight = ClientRect.bottom - ClientRect.top;
                RenderGradient(xoff, yoff);
                xoff++;
                yoff++;
                Win32UpdateWindow(DeviceContext, &ClientRect, 0, 0, winWidth, winHeight);
            }
        }
    } else {

    }
    return 0;
}
