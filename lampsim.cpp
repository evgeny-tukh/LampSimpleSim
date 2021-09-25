#include <stdio.h>
#include <time.h>
#include <windows.h>
#include <commctrl.h>
#include <Shlwapi.h>
#include <vector>
#include <thread>
#include <memory.h>
#include "resource.h"
#include "editbox.h"

struct Ctx {
    HINSTANCE instance;
    HWND display;
    RECT client;
    HBRUSH displayBrush, wndBrush;
    HPEN borderPen;
    double bearing;
    double elevation;

    Ctx (HINSTANCE _instance): instance (_instance), bearing (0.0), elevation (100) {
        borderPen = CreatePen (PS_SOLID, 3, 0);
        displayBrush = CreateSolidBrush (RGB (100, 100, 100));
        wndBrush = CreateSolidBrush (RGB (200, 200, 200));
    }

    virtual ~Ctx () {
        DeleteObject (borderPen);
        DeleteObject (wndBrush);
        DeleteObject (displayBrush);
    }
};

const double PI = 3.1415926535897932384626433832795;
const double TWO_PI = PI + PI;
const double TO_RAD = PI / 180.0;
const double TO_DEG = 180.0 / PI;
char const *CLS_NAME = "lampSimWin";
char const *DISPLAY_CLS_NAME = "lampSimDispWin";

inline double toDeg (double val) { return val * TO_DEG; }
inline double toRad (double val) { return val * TO_RAD; }

bool queryExit (HWND wnd) {
    return MessageBox (wnd, "Do you want to quit the application?", "Confirmation", MB_YESNO | MB_ICONQUESTION) == IDYES;
}

void initDisplay (HWND wnd, void *data) {
    Ctx *ctx = (Ctx *) data;
    SetWindowLongPtr (wnd, GWLP_USERDATA, (LONG_PTR) data);
    GetClientRect (wnd, & ctx->client);
}

void initWindow (HWND wnd, void *data) {
    Ctx *ctx = (Ctx *) data;
    RECT client;

    GetClientRect (wnd, & client);

    auto createControl = [&wnd, &ctx] (const char *className, const char *text, uint32_t style, bool visible, int x, int y, int width, int height, uint64_t id) {
        style |= WS_CHILD;

        if (visible) style |= WS_VISIBLE;
        
        return CreateWindow (className, text, style, x, y, width, height, wnd, (HMENU) id, ctx->instance, 0);
    };
    auto createPopup = [&wnd, &ctx] (const char *className, const char *text, uint32_t style, bool visible, int x, int y, int width, int height) {
        style |= WS_POPUP;

        if (visible) style |= WS_VISIBLE;
        
        return CreateWindow (className, text, style, x, y, width, height, wnd, 0, ctx->instance, 0);
    };

    SetWindowLongPtr (wnd, GWLP_USERDATA, (LONG_PTR) data);

    auto minSize = (client.right > client.bottom ? client.bottom : client.right) - 20;
    ctx->display = CreateWindow (DISPLAY_CLS_NAME, "", WS_CHILD | WS_VISIBLE, 10, 10, minSize, minSize, wnd, 0, ctx->instance, ctx);
}

void doCommand (HWND wnd, uint16_t command) {
    Ctx *ctx = (Ctx *) GetWindowLongPtr (wnd, GWLP_USERDATA);

    switch (command) {
        case ID_EXIT: {
            if (queryExit (wnd)) DestroyWindow (wnd);
            break;
        }
    }
}

void paintDisplay (HWND wnd) {
    PAINTSTRUCT data;
    Ctx *ctx = (Ctx *) GetWindowLongPtr (wnd, GWLP_USERDATA);
    HDC paintCtx = BeginPaint (wnd, & data);
    RECT client;
    GetClientRect (wnd, & client);
    auto width = client.right + 1;
    auto height = client.bottom + 1;
    auto centerX = client.right >> 1;
    auto centerY = client.bottom >> 1;
    auto project = [centerX, centerY] (double angle, double radius, long& x, long &y) {
        double angleRad = toRad (angle);
        if (angleRad < 0.0) angleRad += TWO_PI;
        if (angleRad >= TWO_PI) angleRad -= TWO_PI;
        x = centerX + (long) (radius * sin (angleRad));
        y = centerY - (long) (radius * cos (angleRad));
    };
    auto drawTick = [centerX, centerY, project, paintCtx] (double angle) {
        POINT tick [4];
        project (angle, centerX - 20, tick [0].x, tick [0].y);
        project (angle - 1.0, centerX - 10, tick [1].x, tick [1].y);
        project (angle + 1.0, centerX - 10, tick [2].x, tick [2].y);
        tick [3].x = tick [0].x;
        tick [3].y = tick [0].y;
        Polygon (paintCtx, tick, 4);
    };
    FillRect (paintCtx, & client, ctx->wndBrush);
    SelectObject (paintCtx, ctx->displayBrush);
    SelectObject (paintCtx, ctx->borderPen);
    Ellipse (paintCtx, client.left + 2, client.top + 2, client.right - 2, client.bottom - 2);
    SelectObject (paintCtx, GetStockObject (BLACK_PEN));
    SelectObject (paintCtx, GetStockObject (BLACK_BRUSH));
    for (auto i = 0; i < 360; i += 10) {
        drawTick ((double) (i));
    }
    //Ellipse (paintCtx, centerX - 10, centerY - 10, centerX + 10, centerY + 10);


    EndPaint (wnd, & data);
}

void onSize (HWND wnd, int width, int height) {
    Ctx *ctx = (Ctx *) GetWindowLongPtr (wnd, GWLP_USERDATA);
    auto minSize = (width < height ? width : height) - 20;
    MoveWindow (ctx->display, 10, 10, minSize, minSize, true);
}

LRESULT wndProc (HWND wnd, UINT msg, WPARAM param1, LPARAM param2) {
    LRESULT result = 0;

    switch (msg) {
        case WM_COMMAND:
            doCommand (wnd, LOWORD (param1)); break;
        case WM_SIZE:
            onSize (wnd, LOWORD (param2), HIWORD (param2)); break;
        case WM_CREATE:
            initWindow (wnd, ((CREATESTRUCT *) param2)->lpCreateParams); break;
        case WM_DESTROY:
            PostQuitMessage (0); break;
        case WM_SYSCOMMAND:
            if (param1 == SC_CLOSE && queryExit (wnd)) DestroyWindow (wnd);
        default:
            result = DefWindowProc (wnd, msg, param1, param2);
    }
    
    return result;
}

void onMouseMove (HWND wnd, int clientX, int clientY) {
    Ctx *ctx = (Ctx *) GetWindowLongPtr (wnd, GWLP_USERDATA);
}

void onRightButtonDown (HWND wnd, int x, int y) {
    Ctx *ctx = (Ctx *) GetWindowLongPtr (wnd, GWLP_USERDATA);
    //TrackPopupMenu (GetSubMenu (ctx->contextMenu, 0), TPM_LEFTALIGN | TPM_TOPALIGN, ctx->clickX, ctx->clickY, 0, GetParent (wnd), 0);
}

LRESULT displayWndProc (HWND wnd, UINT msg, WPARAM param1, LPARAM param2) {
    LRESULT result = 0;

    switch (msg) {
        case WM_RBUTTONDOWN:
            onRightButtonDown (wnd, LOWORD (param2), HIWORD (param2)); break;
        case WM_MOUSEMOVE:
            onMouseMove (wnd, LOWORD (param2), HIWORD (param2)); break;
        case WM_PAINT:
            paintDisplay (wnd); break;
        case WM_CREATE:
            initDisplay (wnd, ((CREATESTRUCT *) param2)->lpCreateParams); break;
        default:
            result = DefWindowProc (wnd, msg, param1, param2);
    }
    
    return result;
}

void registerClass (HINSTANCE instance, Ctx& ctx) {
    WNDCLASS classInfo;

    memset (& classInfo, 0, sizeof (classInfo));

    classInfo.hbrBackground = ctx.wndBrush;
    classInfo.hCursor = (HCURSOR) LoadCursor (0, IDC_ARROW);
    classInfo.hIcon = (HICON) LoadIcon (0, IDI_APPLICATION);
    classInfo.hInstance = instance;
    classInfo.lpfnWndProc = wndProc;
    classInfo.lpszClassName = CLS_NAME;
    classInfo.style = CS_HREDRAW | CS_VREDRAW;

    RegisterClass (& classInfo);

    classInfo.lpfnWndProc = displayWndProc;
    classInfo.lpszClassName = DISPLAY_CLS_NAME;

    RegisterClass (& classInfo);
}

void initCommonControls () {
    INITCOMMONCONTROLSEX data;
	
    data.dwSize = sizeof (INITCOMMONCONTROLSEX);
	data.dwICC = ICC_WIN95_CLASSES;
	
    InitCommonControlsEx (& data);
}

int APIENTRY WinMain (HINSTANCE instance, HINSTANCE prev, char *cmdLine, int showCmd) {
    Ctx ctx (instance);

    CoInitialize (0);
    initCommonControls ();
    registerClass (instance, ctx);
    
    auto mainWnd = CreateWindow (
        CLS_NAME,
        "Lamp Simulator",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        900,
        600,
        0,
        LoadMenu (instance, MAKEINTRESOURCE (IDR_MAINMENU)),
        instance,
        & ctx
    );

    ShowWindow (mainWnd, SW_SHOW);
    UpdateWindow (mainWnd);

    MSG msg;

    while (GetMessage (&msg, 0, 0, 0)) {
        TranslateMessage (&msg);
        DispatchMessage (&msg);
    }
}
