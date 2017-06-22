//----------------------------------------------------------------------------------------------------------------------
// GA-based image->ZX scr converter
//----------------------------------------------------------------------------------------------------------------------

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

//----------------------------------------------------------------------------------------------------------------------
// Basic typedefs
//----------------------------------------------------------------------------------------------------------------------

typedef INT8    i8;
typedef INT16   i16;
typedef INT32   i32;
typedef INT64   i64;

typedef UINT8   u8;
typedef UINT16  u16;
typedef UINT32  u32;
typedef UINT64  u64;

typedef float   f32;
typedef double  f64;

typedef char    bool;

#define YES 1
#define NO 0
#define BOOL(x) ((x) ? YES : NO)

//----------------------------------------------------------------------------------------------------------------------
// Data loading
//----------------------------------------------------------------------------------------------------------------------

typedef struct Data
{
    const u8*   buffer;
    i64         size;
    HANDLE      file;
    HANDLE      fileMap;
}
Data;

void dataUnload(Data);

Data dataLoad(const char* fileName)
{
    Data d = { 0 };

    d.file = CreateFileA(fileName, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
    if (d.file)
    {
        DWORD fileSizeHigh, fileSizeLow;
        fileSizeLow = GetFileSize(d.file, &fileSizeHigh);
        d.fileMap = CreateFileMappingA(d.file, 0, PAGE_READONLY, fileSizeHigh, fileSizeLow, 0);

        if (d.fileMap)
        {
            d.buffer = MapViewOfFile(d.fileMap, FILE_MAP_READ, 0, 0, 0);
            d.size = ((i64)fileSizeHigh << 32) | fileSizeLow;
        }
        else
        {
            dataUnload(d);
        }
    }

    return d;
}

void dataUnload(Data data)
{
    if (data.buffer)    UnmapViewOfFile(data.buffer);
    if (data.fileMap)   CloseHandle(data.fileMap);
    if (data.file)      CloseHandle(data.file);
}

//----------------------------------------------------------------------------------------------------------------------
// Window handling
//----------------------------------------------------------------------------------------------------------------------

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
//----------------------------------------------------------------------------------------------------------------------

int WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmdLine, int cmdShow)
{
    createWindow(inst);
    return run();
}

//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
