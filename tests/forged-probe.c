/* Post Wine's internal bitmap-forwarding message with the address of an
 * unrelated mapping and check that the receiver leaves that mapping alone.
 * Exit 0: both mappings survive; 1: the writable one was unmapped; 2: the
 * read-only one was unmapped; 3: the marker message never arrived. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define WM_WINE_UPDATE_GDI_CONTENTS 0x80000012
#define WM_MARKER (WM_APP + 1)

void *memset(void *p, int c, size_t n)
{
    volatile unsigned char *b = p;
    while (n--) *b++ = c;
    return p;
}

static LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    return DefWindowProcA(hwnd, msg, wparam, lparam);
}

static int still_mapped(void *view)
{
    MEMORY_BASIC_INFORMATION info;

    return VirtualQuery(view, &info, sizeof(info)) && info.State == MEM_COMMIT &&
           info.Type == MEM_MAPPED && info.AllocationBase == view;
}

static int deliver(HWND hwnd, void *view)
{
    MSG msg;
    int i;

    PostMessageA(hwnd, WM_WINE_UPDATE_GDI_CONTENTS, (WPARAM)hwnd, (LPARAM)view);
    PostMessageA(hwnd, WM_MARKER, 0, 0);
    for (i = 0; i < 500; i++)
    {
        while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_MARKER) return 1;
            DispatchMessageA(&msg);
        }
        Sleep(10);
    }
    return 0;
}

void mainCRTStartup(void)
{
    WNDCLASSA wc = {0};
    HANDLE section;
    HWND hwnd;
    void *writable, *readonly;

    wc.lpfnWndProc = proc;
    wc.hInstance = GetModuleHandleA(0);
    wc.lpszClassName = "ForgedProbe";
    RegisterClassA(&wc);
    hwnd = CreateWindowExA(0, "ForgedProbe", "forged", WS_OVERLAPPEDWINDOW, 0, 0, 200, 200,
                           0, 0, wc.hInstance, 0);

    section = CreateFileMappingA(INVALID_HANDLE_VALUE, 0, PAGE_READWRITE, 0, 65536, 0);
    writable = MapViewOfFile(section, FILE_MAP_WRITE, 0, 0, 0);
    readonly = MapViewOfFile(section, FILE_MAP_READ, 0, 0, 0);

    if (!deliver(hwnd, writable)) ExitProcess(3);
    if (!still_mapped(writable)) ExitProcess(1);
    if (!deliver(hwnd, readonly)) ExitProcess(3);
    if (!still_mapped(readonly)) ExitProcess(2);
    ExitProcess(0);
}
