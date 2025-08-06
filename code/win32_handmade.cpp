#include <windows.h>

int CALLBACK
WinMain(HINSTANCE hIntance, HINSTANCE nPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    MessageBox(0, "This is me", "Test", MB_OK|MB_ICONINFORMATION);
    return 0;
}
