//----------------------------------------------------------------------------------------------------------------------
// GA-based image->ZX scr converter
//----------------------------------------------------------------------------------------------------------------------

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <time.h>
#include <stdint.h>

#define POPULATION_SIZE     100
#define CROSSOVER_CHANCE    0.7
#define MUTATION_CHANCE     0.01

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

Image* imageZxConvert(Image* img, u8* bytes)
{
    static const u32 colours[16] =
    {
        0x000000, 0x0000d7, 0xd70000, 0xd700d7, 0x00d700, 0x00d7d7, 0xd7d700, 0xd7d7d7,
        0x000000, 0x0000ff, 0xff0000, 0xff00ff, 0x00ff00, 0x00ffff, 0xffff00, 0xffffff,
    };

    u8* pixels = bytes;
    u8* attr = bytes + 6144;
    int p = 0;

    for (int section = 0; section < 3; ++section)
    {
        int ppp = p;
        for (int pixRow = 0; pixRow < 8; ++pixRow)
        {
            int pp = p;

            for (int row = 0; row < 8; ++row)
            {
                for (int x = 0; x < 32; ++x)
                {
                    int b = *pixels++;
                    int i;
                    u8 colour = attr[(section * 8 + row) * 32 + x];
                    u32 ink = colours[(colour & 7) + ((colour & 0x40) >> 3)];
                    u32 paper = colours[(colour & 0x7f) >> 3];

                    for (i = 7; i >= 0; --i)
                    {
                        img->pixels[p + i] = (b & 1) ? ink : paper;
                        b >>= 1;
                    }
                    p += 8;
                } // end of pixel row
                p += (7 * 256);
            } // end of section
            p = pp + 256;
        } // all intermediate rows
        p = ppp + (8 * 8 * 256);
    } // whole screen

    return img;
}

//----------------------------------------------------------------------------------------------------------------------
// Genetic Algorithm
// Our lifeforms, scrims, describe a screen
//----------------------------------------------------------------------------------------------------------------------

Image* gTargetImage = 0;

typedef struct
{
    u8      genomes[6912 * POPULATION_SIZE];
    i64     errors[POPULATION_SIZE];
    i64     total;
    i64     bestScore;
    i64     worseScore;
    i64     indexBest;
}
Population;

Population gPopA, gPopB;
Population* gCurrentPop;
Population* gFuturePop;

void generateScrim(u8* bytes)
{
    static bool seeded = NO;
    if (!seeded)
    {
        // Ensure we seed the random number generator only once.
        srand((unsigned int)time(NULL));
        seeded = YES;
    }
    for (int i = 0; i < 6912; ++i)
    {
        *bytes++ = (u8)rand();
    }
}

void generatePopulation(Population *pop)
{
    int offset = 0;
    for (int i = 0; i < POPULATION_SIZE; ++i)
    {
        generateScrim(&pop->genomes[offset]);
        offset += 6912;
        pop->errors[i] = 0;
    }

    pop->total = 0;
    pop->indexBest = -1;
}

i64 checkError(Image* targetImg, u8* genome)
{
    static Image* img = 0;
    i64 total = 0;

    if (!img)
    {
        img = imageCreate(256, 192);
    }

    imageZxConvert(img, genome);

    for (int i = 0; i < (256 * 192); ++i)
    {
        total += abs(img->pixels[i] - targetImg->pixels[i]);
    }

    return total;
}

i64 chooseParent(Population* pop)
{
    i64 r = ((i64)rand() << 32 ^ (i64)rand()) % pop->total;
    i64 min = 0;
    i64 max = POPULATION_SIZE - 1;
    i64 index = -1;

    for(;;)
    {
        index = ((min + max) / 2);

        if (pop->errors[index] < r)
        {
            if ((index == POPULATION_SIZE - 1) || (pop->errors[index + 1] > r))
            {
                return index;
            }
            min = index + 1;
            if (min >= POPULATION_SIZE) min = POPULATION_SIZE;
        }
        else
        {
            max = index - 1;
            if (max < 0) max = 0;
        }
    }
}

u8 mutate(u8 b)
{
    f32 r = (f32)rand() / (f32)RAND_MAX;
    if (r < MUTATION_CHANCE)
    {
        int r = rand() % 8;
        b ^= (1 << r);
    }

    return b;
}

void generate(Population* curPop, Population* futurePop)
{
    // First calculate the errors of the current population
    curPop->total = 0;
    curPop->worseScore = 0;
    for (int i = 0, offset = 0; i < POPULATION_SIZE; ++i, offset += 6912)
    {
        i64 t = checkError(gTargetImage, &curPop->genomes[offset]);
        if (curPop->indexBest == -1 || t < curPop->bestScore)
        {
            curPop->bestScore = t;
            curPop->indexBest = i;
        }
        if (t > curPop->worseScore)
        {
            curPop->worseScore = t;
        }
        curPop->errors[i] = curPop->total;
        curPop->total += t;
    }
    curPop->total = 0;

    // Now convert the errors so that the smallest are the largest, and vice versa
    for (int i = 0; i < POPULATION_SIZE; ++i)
    {
        curPop->errors[i] = curPop->worseScore - curPop->errors[i];
        curPop->total += curPop->errors[i];
    }

    // Now we generate next population
    for (int i = 0, offset = 0; i < POPULATION_SIZE; ++i, offset += 6912)
    {
        i64 parents[2];
        for (int p = 0; p < 2; ++p)
        {
            parents[p] = chooseParent(curPop);
        }

        // Decide whether to do cross-over or not
        {
            f32 chance = (f32)rand() / (f32)RAND_MAX;
            if (chance < CROSSOVER_CHANCE)
            {
                int r = rand() % 6912;
                int i = 0;
                for (; i <= r; ++i)
                {
                    u8 b = mutate(curPop->genomes[parents[0] * 6912 + i]);
                    futurePop->genomes[offset + i] = b;
                }
                for (; i < 6912; ++i)
                {
                    u8 b = mutate(curPop->genomes[parents[1] * 6912 + i]);
                    futurePop->genomes[offset + i] = b;
                }
            }
            else
            {
                int r = rand() % 2;
                for (int i = 0; i < 6912; ++i)
                {
                    futurePop->genomes[offset + i] = mutate(curPop->genomes[parents[r] * 6912 + i]);
                }
            }
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Data loading
//----------------------------------------------------------------------------------------------------------------------

typedef struct Data
{
    u8*     buffer;
    i64     size;
    HANDLE  file;
    HANDLE  fileMap;
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

void resizeDIBSection(Win32OffscreenBuffer* buffer, int width, int height, void* img, int stride)
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

    if (img)
    {
        buffer->memory = img;
        buffer->stride = stride;
    }
    else
    {
        bitmapMemorySize = (width * height) * BPP;
        buffer->memory = VirtualAlloc(0, bitmapMemorySize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

        // Memory is xxRRGGBB
        for (int i = 0; i < (width * height); ++i)
        {
            ((u32 *)buffer->memory)[i] = 0;
        }

        buffer->stride = width * BPP;
    }
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
Image* gImage = 0;
int gWindowWidth, gWindowHeight;

LRESULT CALLBACK winProc(HWND wnd, UINT msg, WPARAM w, LPARAM l)
{
    switch (msg)
    {
    case WM_SIZE:
        gWindowWidth = LOWORD(l);
        gWindowHeight = HIWORD(l);
        resizeDIBSection(&gScreen, 32 * 8, 24 * 8, gImage->pixels, 32 * 8);
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
    const int scale = 3;
    RECT r = { 0, 0, 8 * 32 * scale, 8 * 24 * scale };

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
    int generation = 0;
    bool quit = NO;

    while(!quit)
    {
        // Flush windows queue
        while (PeekMessageA(&msg, 0, 0, 0, PM_NOREMOVE))
        {
            if (!GetMessageA(&msg, 0, 0, 0))
            {
                quit = YES;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }

        // Do one generation
        {
            char buffer[32];
            snprintf(buffer, 32, "Generation: %d", generation);
            SetWindowTextA(gWnd, buffer);
        }
        generate(gCurrentPop, gFuturePop);

        //if (generation % 50 == 0)
        {
            imageZxConvert(gImage, &gCurrentPop->genomes[gCurrentPop->indexBest * 6912]);
            InvalidateRect(gWnd, 0, FALSE);
        }

        {
            Population* t = gCurrentPop;
            gCurrentPop = gFuturePop;
            gFuturePop = t;
        }

        ++generation;

    }

    return (int)msg.wParam;
}

//----------------------------------------------------------------------------------------------------------------------
// WinMain
//----------------------------------------------------------------------------------------------------------------------

int WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmdLine, int cmdShow)
{
    // Load target image
    {
        Data img = dataLoad("img1.jpg");
        int width, height, type;
        u8* imgData = 0;

        gImage = imageCreate(256, 192);
        gTargetImage = imageCreate(256, 192);
        imgData = stbi_load_from_memory(img.buffer, (int)img.size, &width, &height, &type, 4);
        if (width != 256 || height != 192) return 1;
        memcpy(gTargetImage->pixels, imgData, width*height * sizeof(u32));
        for (int i = 0; i < width*height; ++i)
        {
            u32 p = gTargetImage->pixels[i];
            p = (p & 0xff00ff00) | ((p & 0x00ff0000) >> 16) | ((p & 0x000000ff) << 16);
            gTargetImage->pixels[i] = p;
        }
        STBI_FREE(imgData);
        dataUnload(img);
    }

    gCurrentPop = &gPopA;
    gFuturePop = &gPopB;
    generatePopulation(gCurrentPop);

    //imageZxConvert(gImage, &gCurrentPop->genomes[0]);

    createWindow(inst);

    return run();
}

//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
