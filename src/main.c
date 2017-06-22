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
#define MAKE_BOOL(x) ((x) ? YES : NO)

//----------------------------------------------------------------------------------------------------------------------
// Image handling
//----------------------------------------------------------------------------------------------------------------------

typedef struct
{
    int     width;
    int     height;
    u32     pixels[0];
}
Image;

Image* imageCreate(int width, int height)
{
    size_t sz = sizeof(Image) + (width * height * sizeof(u32));
    Image* img = (Image *)malloc(sz);
    img->width = width;
    img->height = height;
    for (int i = 0; i < (width * height); ++i)
    {
        img->pixels[i] = 0xff0000ff;
    }

    return img;
}

void imageDestroy(Image* img)
{
    free(img);
}

Image* createZXImage(u8* bytes)
{
    static const u32 colours[16] =
    {
        0x000000, 0x0000d7, 0xd70000, 0xd700d7, 0x00d700, 0x00d7d7, 0xd7d700, 0xd7d7d7,
        0x000000, 0x0000ff, 0xff0000, 0xff00ff, 0x00ff00, 0x00ffff, 0xffff00, 0xffffff,
    };

    Image* img = imageCreate(256, 192);
    u8* pixels = bytes;
    u8* attr = bytes + 6912;
    int p = 0;

    for (int section = 0; section < 3; ++section)
    {
        for (int pixRow = 0; pixRow < 7; ++pixRow)
        {
            int pp = p;

            for (int row = 0; row < 7; ++row)
            {
                for (int x = 0; x < 32; ++x)
                {
                    int i;
                    int b = *pixels++;
                    u8 colour = attr[(section * 8 + row) * 32 + x];
                    u32 ink = (colour & 7) + ((colour & 0x40) >> 3);
                    u32 paper = (colour & 0x7f) >> 3;

                    for (i = 7; i >= 0; --i)
                    {
                        img->pixels[p + i] = (b & 1) ? ink : paper;
                    }
                    p += 8;
                } // end of pixel row
                p += (7 * 256);
            } // end of section

            p = pp + 256;
        } // all intermediate rows
    } // whole screen

    return img;
}

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
// Windows rendering
//----------------------------------------------------------------------------------------------------------------------

#define BPP 4

typedef struct
{
    BITMAPINFO info;
    void* memory;
    int width;
    int height;
    int stride;
}
Win32OffscreenBuffer;

void resizeDIBSection(Win32OffscreenBuffer* buffer, int width, int height)
{
    int bitmapMemorySize;

    if (buffer->memory)
    {
        VirtualFree(buffer->memory, 0, MEM_RELEASE);
    }

    buffer->width = width;
    buffer->height = height;
    buffer->info.bmiHeader.biSize = sizeof(buffer->info.bmiHeader);
    buffer->info.bmiHeader.biWidth = width;
    buffer->info.bmiHeader.biHeight = -height;
    buffer->info.bmiHeader.biPlanes = 1;
    buffer->info.bmiHeader.biBitCount = 32;
    buffer->info.bmiHeader.biClrImportant = BI_RGB;

    bitmapMemorySize = (width * height) * BPP;
    buffer->memory = VirtualAlloc(0, bitmapMemorySize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

    // Memory is xxRRGGBB
    for (int i = 0; i < (width * height); ++i)
    {
        ((u32 *)buffer->memory)[i] = 0;
    }

    buffer->stride = width * BPP;
}

void displayBuffer(Win32OffscreenBuffer* buffer, HDC dc, int windowWidth, int windowHeight)
{
    StretchDIBits(
        dc,
        0, 0, windowWidth, windowHeight,
        0, 0, buffer->width, buffer->height,
        buffer->memory,
        &buffer->info,
        DIB_RGB_COLORS,
        SRCCOPY);
}

//----------------------------------------------------------------------------------------------------------------------
// Window handling
//----------------------------------------------------------------------------------------------------------------------

HWND gWnd;
Win32OffscreenBuffer gScreen;
int gWindowWidth, gWindowHeight;

LRESULT CALLBACK winProc(HWND wnd, UINT msg, WPARAM w, LPARAM l)
{
    switch (msg)
    {
    case WM_SIZE:
        gWindowWidth = LOWORD(l);
        gWindowHeight = HIWORD(l);
        resizeDIBSection(&gScreen, 32 * 8, 24 * 8);
        break;

    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(wnd, &ps);
            displayBuffer(&gScreen, dc, gWindowWidth, gWindowHeight);
            EndPaint(wnd, &ps);
        }
        break;

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
    int style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE;
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
