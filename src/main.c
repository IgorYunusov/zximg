//----------------------------------------------------------------------------------------------------------------------
// GA-based image->ZX scr converter
//----------------------------------------------------------------------------------------------------------------------

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

//----------------------------------------------------------------------------------------------------------------------
// Window Handline

HWND gWnd;

LRESULT CALLBACK winProc(HWND wnd, UINT msg, WPARAM w, LPARAM l)
{
    switch (msg)
    {
    case WM_CLOSE:
        DestroyWindow(wnd);
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProcA(wnd, msg, w, l);
    }

    return 0;
}

void createWindow(HINSTANCE inst)
{
    WNDCLASSEXA wc = { 0 };
    int style = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
    RECT r = { 0, 0, 8 * 32, 8 * 24 };

    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = &winProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = inst;
    wc.hIcon = wc.hIconSm = LoadIconA(0, IDI_APPLICATION);
    wc.hCursor = LoadCursorA(0, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_APPWORKSPACE + 1);
    wc.lpszClassName = "zximg_window";

    RegisterClassExA(&wc);
    AdjustWindowRect(&r, style, FALSE);

    gWnd = CreateWindowA("zximg_window", "ZXIMG", style,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        r.right - r.left,
        r.bottom - r.top,
        0, 0, inst, 0);
}

int run()
{
    MSG msg;

    while (GetMessageA(&msg, 0, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    return (int)msg.wParam;
}

//----------------------------------------------------------------------------------------------------------------------
// WinMain

int WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmdLine, int cmdShow)
{
    createWindow(inst);
    return run();
}

