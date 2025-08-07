#include <windows.h>
#include <stdint.h>
#include <wingdi.h>
#include <winuser.h>

#define local_persist static
#define global_variable static
#define internal static

global_variable bool Running;

global_variable HCURSOR customCursor;

struct game_state {
    uint32_t time;
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

    RenderGradient(*buffer, global_game_state.time, global_game_state.time*2);
}



internal void Win32DisplayBufferWindow(HDC DeviceContext, RECT ClientRect, win32_offscreen_buffer* Buffer, int x, int y, int width, int height) {
    //StretchDIBits(DeviceContext, x, y, width, height, x, y, width, height, BitmapMemory, &BitmapInfo, DIB_RGB_COLORS, SRCCOPY);
    int WindowWidth = ClientRect.right - ClientRect.left;
    int WindowHeight = ClientRect.bottom - ClientRect.top;
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
            RECT ClientRect;
            GetClientRect(Window, &ClientRect);
            int width = ClientRect.right - ClientRect.left;
            int height = ClientRect.bottom - ClientRect.top;
            ResizeDIBSection(&GlobalBackBuffer, width, height);
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

        case WM_SETCURSOR: {
            SetCursor(customCursor);
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
           Win32DisplayBufferWindow(DeviceContext, ClientRect, &GlobalBackBuffer, x, y, width, height);
           EndPaint(Window, &Paint);
           OutputDebugString("WM_PAINT\n");
        } break;

        default: {
            Result = DefWindowProc(Window, Message, WParam, LParam);
        } break;
    }
    return Result;

}

internal HCURSOR makeCursor(HINSTANCE instance) {
    // Yin-shaped cursor AND mask (32x32x1bpp)
     BYTE ANDmaskCursor[] =
    {
        0xFF, 0xFC, 0x3F, 0xFF,   // ##############----##############
        0xFF, 0xC0, 0x1F, 0xFF,   // ##########---------#############
        0xFF, 0x00, 0x3F, 0xFF,   // ########----------##############
        0xFE, 0x00, 0xFF, 0xFF,   // #######---------################
        0xF8, 0x01, 0xFF, 0xFF,   // #####----------#################
        0xF0, 0x03, 0xFF, 0xFF,   // ####----------##################
        0xF0, 0x03, 0xFF, 0xFF,   // ####----------##################
        0xE0, 0x07, 0xFF, 0xFF,   // ###----------###################
        0xC0, 0x07, 0xFF, 0xFF,   // ##-----------###################
        0xC0, 0x0F, 0xFF, 0xFF,   // ##----------####################
        0x80, 0x0F, 0xFF, 0xFF,   // #-----------####################
        0x80, 0x0F, 0xFF, 0xFF,   // #-----------####################
        0x80, 0x07, 0xFF, 0xFF,   // #------------###################
        0x00, 0x07, 0xFF, 0xFF,   // -------------###################
        0x00, 0x03, 0xFF, 0xFF,   // --------------##################
        0x00, 0x00, 0xFF, 0xFF,   // ----------------################
        0x00, 0x00, 0x7F, 0xFF,   // -----------------###############
        0x00, 0x00, 0x1F, 0xFF,   // -------------------#############
        0x00, 0x00, 0x0F, 0xFF,   // --------------------############
        0x80, 0x00, 0x0F, 0xFF,   // #-------------------############
        0x80, 0x00, 0x07, 0xFF,   // #--------------------###########
        0x80, 0x00, 0x07, 0xFF,   // #--------------------###########
        0xC0, 0x00, 0x07, 0xFF,   // ##-------------------###########
        0xC0, 0x00, 0x0F, 0xFF,   // ##------------------############
        0xE0, 0x00, 0x0F, 0xFF,   // ###-----------------############
        0xF0, 0x00, 0x1F, 0xFF,   // ####---------------#############
        0xF0, 0x00, 0x1F, 0xFF,   // ####---------------#############
        0xF8, 0x00, 0x3F, 0xFF,   // #####-------------##############
        0xFE, 0x00, 0x7F, 0xFF,   // #######----------###############
        0xFF, 0x00, 0xFF, 0xFF,   // ########--------################
        0xFF, 0xC3, 0xFF, 0xFF,   // ##########----##################
        0xFF, 0xFF, 0xFF, 0xFF    // ################################
    };

    // Yin-shaped cursor XOR mask (32x32x1bpp)
    BYTE XORmaskCursor[] =
    {
        0x00, 0x01, 0x00, 0x00,   // --------------------------------
        0x00, 0x03, 0xC0, 0x00,   // --------------####--------------
        0x00, 0x3F, 0x00, 0x00,   // ----------######----------------
        0x00, 0xFE, 0x00, 0x00,   // --------#######-----------------
        0x03, 0xFC, 0x00, 0x00,   // ------########------------------
        0x07, 0xF8, 0x00, 0x00,   // -----########-------------------
        0x07, 0xF8, 0x00, 0x00,   // -----########-------------------
        0x0F, 0xF0, 0x00, 0x00,   // ----########--------------------
        0x1F, 0xF0, 0x00, 0x00,   // ---#########--------------------
        0x1F, 0xE0, 0x00, 0x00,   // ---########---------------------
        0x3F, 0xE0, 0x00, 0x00,   // --#########---------------------
        0x3F, 0xE0, 0x00, 0x00,   // --#########---------------------
        0x3F, 0xF0, 0x00, 0x00,   // --##########--------------------
        0x7F, 0xF0, 0x00, 0x00,   // -###########--------------------
        0x7F, 0xF8, 0x00, 0x00,   // -############-------------------
        0x7F, 0xFC, 0x00, 0x00,   // -#############------------------
        0x7F, 0xFF, 0x00, 0x00,   // -###############----------------
        0x7F, 0xFF, 0x80, 0x00,   // -################---------------
        0x7F, 0xFF, 0xE0, 0x00,   // -##################-------------
        0x3F, 0xFF, 0xE0, 0x00,   // --#################-------------
        0x3F, 0xC7, 0xF0, 0x00,   // --########----######------------
        0x3F, 0x83, 0xF0, 0x00,   // --#######------#####------------
        0x1F, 0x83, 0xF0, 0x00,   // ---######------#####------------
        0x1F, 0x83, 0xE0, 0x00,   // ---######------####-------------
        0x0F, 0xC7, 0xE0, 0x00,   // ----######----#####-------------
        0x07, 0xFF, 0xC0, 0x00,   // -----#############--------------
        0x07, 0xFF, 0xC0, 0x00,   // -----#############--------------
        0x01, 0xFF, 0x80, 0x00,   // -------##########---------------
        0x00, 0xFF, 0x00, 0x00,   // --------########----------------
        0x00, 0x3C, 0x00, 0x00,   // ----------####------------------
        0x00, 0x00, 0x00, 0x00,   // --------------------------------
        0x00, 0x00, 0x00, 0x00    // --------------------------------
    };

    // Create a custom cursor at run time.

    return CreateCursor(instance,   // app. instance
                 16,                // horizontal position of hot spot
                 16,                 // vertical position of hot spot
                 32,                // cursor width
                 32,                // cursor height
                 ANDmaskCursor,     // AND mask
                 XORmaskCursor );
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
    customCursor = makeCursor(Instance);
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
                RenderGradient(GlobalBackBuffer, global_game_state.time, global_game_state.time*2);
                InvalidateRect(windowHandle, 0, FALSE);
            }
        }
    } else {

    }
    return 0;
}
