#include <Windows.h>
#include <TlHelp32.h>
#include <stdio.h>
#include "resource.h"

// ---- Layout ------------------------------------------------------------
static constexpr int W   = 340;
static constexpr int H   = 180;
static constexpr int HDR = 54;

// ---- Palette -----------------------------------------------------------
#define C_BG     RGB( 14,  14,  14)
#define C_PANEL  RGB( 20,  20,  20)
#define C_BORDER RGB( 38,  38,  38)
#define C_ACCENT RGB(  0, 230, 120)
#define C_TEXT   RGB(240, 240, 240)
#define C_DIM    RGB(100, 100, 100)

// ---- Globals -----------------------------------------------------------
static HWND   g_hwnd   = nullptr;
static HWND   g_status = nullptr;
static HWND   g_btn    = nullptr;
static HFONT  g_fBig   = nullptr;
static HFONT  g_fMed   = nullptr;
static HFONT  g_fMono  = nullptr;
static HBRUSH g_hBgBrush = nullptr;  // used by WM_CTLCOLORSTATIC to erase text bg
static bool   g_done   = false;
static bool   g_drag   = false;
static POINT  g_dorg   = {};

#define ID_BTN 1001
#define ID_TMR 1002

static void Status(const wchar_t* s) { SetWindowTextW(g_status, s); }

// ---- Core logic --------------------------------------------------------
static DWORD FindPID(const wchar_t* exe) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W pe{ sizeof(pe) };
    DWORD pid = 0;
    if (Process32FirstW(snap, &pe))
        do { if (!_wcsicmp(pe.szExeFile, exe)) { pid = pe.th32ProcessID; break; } }
        while (Process32NextW(snap, &pe));
    CloseHandle(snap);
    return pid;
}

static bool ExtractDLL(const wchar_t* path, wchar_t* errBuf, int errLen) {
    // RT_RCDATA = MAKEINTRESOURCE(10). Do NOT use the string L"RCDATA" here —
    // that is a named type, not the same as the numeric RCDATA keyword in .rc files.
    HRSRC hr = FindResourceW(nullptr, MAKEINTRESOURCEW(IDR_STASIS_DLL), RT_RCDATA);
    if (!hr) {
        swprintf_s(errBuf, errLen, L"FindResource failed (err %lu)", GetLastError());
        return false;
    }
    HGLOBAL hg  = LoadResource(nullptr, hr);
    void*   dat = LockResource(hg);
    DWORD   sz  = SizeofResource(nullptr, hr);
    if (!dat || sz == 0) {
        swprintf_s(errBuf, errLen, L"Resource is empty (sz=%lu)", sz);
        return false;
    }
    HANDLE hf = CreateFileW(path, GENERIC_WRITE, 0, nullptr,
                            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hf == INVALID_HANDLE_VALUE) {
        swprintf_s(errBuf, errLen, L"CreateFile failed (err %lu)", GetLastError());
        return false;
    }
    DWORD written = 0;
    WriteFile(hf, dat, sz, &written, nullptr);
    CloseHandle(hf);
    if (written != sz) {
        swprintf_s(errBuf, errLen, L"Write incomplete (%lu/%lu bytes)", written, sz);
        return false;
    }
    return true;
}

static bool InjectDLL(DWORD pid, const wchar_t* path, wchar_t* errBuf, int errLen) {
    HANDLE hp = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hp) {
        swprintf_s(errBuf, errLen, L"OpenProcess failed (err %lu)", GetLastError());
        return false;
    }
    size_t len = (wcslen(path) + 1) * sizeof(wchar_t);
    void*  rm  = VirtualAllocEx(hp, nullptr, len, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!rm) {
        swprintf_s(errBuf, errLen, L"VirtualAllocEx failed (err %lu)", GetLastError());
        CloseHandle(hp); return false;
    }
    WriteProcessMemory(hp, rm, path, len, nullptr);
    auto ll = (LPTHREAD_START_ROUTINE)GetProcAddress(
                  GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW");
    HANDLE ht = CreateRemoteThread(hp, nullptr, 0, ll, rm, 0, nullptr);
    if (!ht) {
        swprintf_s(errBuf, errLen, L"CreateRemoteThread failed (err %lu)", GetLastError());
        VirtualFreeEx(hp, rm, 0, MEM_RELEASE); CloseHandle(hp); return false;
    }
    WaitForSingleObject(ht, 6000);
    VirtualFreeEx(hp, rm, 0, MEM_RELEASE);
    CloseHandle(ht); CloseHandle(hp);
    return true;
}

static void DoInject() {
    EnableWindow(g_btn, FALSE);
    InvalidateRect(g_btn, nullptr, TRUE);
    Status(L"Searching for javaw.exe...");

    DWORD pid = FindPID(L"javaw.exe");
    if (!pid) {
        Status(L"javaw.exe not found - launch Minecraft first.");
        EnableWindow(g_btn, TRUE);
        InvalidateRect(g_btn, nullptr, TRUE);
        return;
    }

    wchar_t tmp[MAX_PATH], dll[MAX_PATH], msg[256];
    GetTempPathW(MAX_PATH, tmp);
    // Include a timestamp so we never collide with a previously loaded copy.
    SYSTEMTIME st; GetSystemTime(&st);
    swprintf_s(dll, L"%sstasis_%lu_%u%u%u.dll", tmp, pid,
               st.wHour, st.wMinute, st.wSecond);
    swprintf_s(msg, L"Found PID %lu - extracting payload...", pid);
    Status(msg);

    if (!ExtractDLL(dll, msg, 256)) {
        Status(msg);
        EnableWindow(g_btn, TRUE);
        InvalidateRect(g_btn, nullptr, TRUE);
        return;
    }

    Status(L"Injecting...");
    if (!InjectDLL(pid, dll, msg, 256)) {
        Status(msg);
        EnableWindow(g_btn, TRUE);
        InvalidateRect(g_btn, nullptr, TRUE);
        return;
    }

    g_done = true;
    Status(L"Done! Press M in-game to open the GUI.");
    InvalidateRect(g_btn, nullptr, TRUE);
}

// ---- Button draw -------------------------------------------------------
static void DrawBtn(LPDRAWITEMSTRUCT d) {
    HDC  dc = d->hDC;
    RECT rc = d->rcItem;
    bool en = !(d->itemState & ODS_DISABLED);
    bool pr =   d->itemState & ODS_SELECTED;

    POINT cur; GetCursorPos(&cur); ScreenToClient(g_btn, &cur);
    RECT  br;  GetClientRect(g_btn, &br);
    bool  ht = en && PtInRect(&br, cur);

    COLORREF bg  = !en ? RGB(18,18,18) : pr ? RGB(0,200,100) : ht ? RGB(0,170,82) : RGB(18,18,18);
    COLORREF brd = en  ? C_ACCENT      : RGB(45,45,45);
    COLORREF fg  = en  ? C_TEXT        : C_DIM;

    HBRUSH hbr = CreateSolidBrush(bg);
    HPEN   hpn = CreatePen(PS_SOLID, 1, brd);
    SelectObject(dc, hpn); SelectObject(dc, hbr);
    RoundRect(dc, rc.left, rc.top, rc.right, rc.bottom, 6, 6);
    DeleteObject(hbr); DeleteObject(hpn);

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, fg);
    SelectObject(dc, g_fMed);
    DrawTextW(dc, g_done ? L"INJECTED" : L"INJECT", -1, &rc,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

// ---- Window procedure --------------------------------------------------
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {

    case WM_CREATE:
        g_fBig    = CreateFontW(24,0,0,0,FW_BOLD,    0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
        g_fMed    = CreateFontW(13,0,0,0,FW_SEMIBOLD,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
        g_fMono   = CreateFontW(12,0,0,0,FW_NORMAL,  0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Consolas");
        g_hBgBrush = CreateSolidBrush(C_BG);

        g_status = CreateWindowExW(0, L"STATIC", L"Ready - click INJECT to begin.",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            16, HDR + 22, W - 24, 34, hwnd, nullptr, nullptr, nullptr);
        SendMessageW(g_status, WM_SETFONT, (WPARAM)g_fMono, TRUE);

        g_btn = CreateWindowExW(0, L"BUTTON", L"INJECT",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            W - 114, H - 46, 98, 30, hwnd, (HMENU)ID_BTN, nullptr, nullptr);

        SetTimer(hwnd, ID_TMR, 30, nullptr);
        return 0;

    case WM_TIMER:
        InvalidateRect(g_btn, nullptr, FALSE);
        return 0;

    case WM_COMMAND:
        if (LOWORD(wp) == ID_BTN && !g_done)
            DoInject();
        return 0;

    case WM_DRAWITEM:
        if (((LPDRAWITEMSTRUCT)lp)->CtlID == ID_BTN) {
            DrawBtn((LPDRAWITEMSTRUCT)lp);
            return TRUE;
        }
        break;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);

        HBRUSH hbg = CreateSolidBrush(C_BG);
        FillRect(dc, &rc, hbg); DeleteObject(hbg);

        RECT hdr = {0, 0, W, HDR};
        HBRUSH hhdr = CreateSolidBrush(C_PANEL);
        FillRect(dc, &hdr, hhdr); DeleteObject(hhdr);

        SelectObject(dc, GetStockObject(NULL_BRUSH));
        HPEN hp = CreatePen(PS_SOLID, 1, C_BORDER);
        SelectObject(dc, hp);
        Rectangle(dc, 0, 0, W, H);
        MoveToEx(dc, 0, HDR, nullptr); LineTo(dc, W, HDR);
        DeleteObject(hp);

        SetBkMode(dc, TRANSPARENT);

        SelectObject(dc, g_fBig);
        SetTextColor(dc, C_ACCENT);
        RECT lr = {14, 8, 180, HDR - 6};
        DrawTextW(dc, L"STASIS", -1, &lr, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        SelectObject(dc, g_fMed);
        SetTextColor(dc, C_DIM);
        RECT sr = {102, 18, 260, HDR - 12};
        DrawTextW(dc, L"Injector", -1, &sr, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        RECT xr = {W - 28, 0, W, 26};
        DrawTextW(dc, L"x", -1, &xr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        RECT sl = {14, HDR + 4, 80, HDR + 20};
        DrawTextW(dc, L"STATUS", -1, &sl, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        RECT kl = {14, H - 22, W - 120, H - 4};
        DrawTextW(dc, L"M = GUI  |  END = unload", -1, &kl,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_CTLCOLORSTATIC: {
        HDC dc = (HDC)wp;
        SetBkMode(dc, OPAQUE);       // erase old text before drawing new text
        SetBkColor(dc, C_BG);        // erase color matches window background
        SetTextColor(dc, C_TEXT);
        return (LRESULT)g_hBgBrush;  // solid brush so DefWindowProc fills the rect
    }

    case WM_LBUTTONDOWN: {
        int x = LOWORD(lp), y = HIWORD(lp);
        if (x >= W - 28 && y <= 26) { DestroyWindow(hwnd); return 0; }
        if (y < HDR) {
            g_drag = true;
            POINT cur; GetCursorPos(&cur);
            RECT wr; GetWindowRect(hwnd, &wr);
            g_dorg = {cur.x - wr.left, cur.y - wr.top};
            SetCapture(hwnd);
        }
        return 0;
    }

    case WM_MOUSEMOVE:
        if (g_drag) {
            POINT cur; GetCursorPos(&cur);
            SetWindowPos(hwnd, nullptr, cur.x - g_dorg.x, cur.y - g_dorg.y,
                         0, 0, SWP_NOSIZE | SWP_NOZORDER);
        }
        return 0;

    case WM_LBUTTONUP:
        g_drag = false; ReleaseCapture(); return 0;

    case WM_DESTROY:
        KillTimer(hwnd, ID_TMR);
        DeleteObject(g_fBig); DeleteObject(g_fMed); DeleteObject(g_fMono);
        DeleteObject(g_hBgBrush);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ---- Entry point -------------------------------------------------------
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int) {
    WNDCLASSEXW wc{ sizeof(wc) };
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.lpszClassName = L"StasisInjector";
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassExW(&wc);

    int sx = GetSystemMetrics(SM_CXSCREEN);
    int sy = GetSystemMetrics(SM_CYSCREEN);

    g_hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_APPWINDOW,
        L"StasisInjector", L"Stasis Injector",
        WS_POPUP | WS_VISIBLE,
        (sx - W) / 2, (sy - H) / 2, W, H,
        nullptr, nullptr, hInst, nullptr
    );

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}
