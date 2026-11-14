#include "LoaderMapCore.hpp"
#include <CommCtrl.h>
#include <gdiplus.h>
#include <fstream>
#include <shellapi.h>
#include <string>
#include <thread>
#include <vector>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "comctl32.lib")

namespace {

using namespace Gdiplus;

enum {
    ID_BTN_UNIVERSAL = 101,
    ID_BTN_ROBRUN,
    ID_BTN_UNTURNED,
    ID_BTN_PROBE,
    ID_BTN_CHEATLOADER,
    ID_BTN_ELEVATE,
    WM_MAP_DONE = WM_APP + 40,
};

struct UiState {
    HWND hwnd = nullptr;
    HWND status = nullptr;
    HWND log = nullptr;
    std::wstring byovd;
    bool mapping = false;
};

static UiState* gUi = nullptr;

static void SetStatus(const wchar_t* text, COLORREF color)
{
    if (!gUi || !gUi->status)
        return;
    SetWindowTextW(gUi->status, text);
    HDC dc = GetDC(gUi->status);
    SetTextColor(dc, color);
    ReleaseDC(gUi->status, dc);
    InvalidateRect(gUi->status, nullptr, TRUE);
}

static void AppendLog(const wchar_t* line)
{
    if (!gUi || !gUi->log)
        return;
    int len = GetWindowTextLengthW(gUi->log);
    SendMessageW(gUi->log, EM_SETSEL, len, len);
    std::wstring msg = line;
    msg += L"\r\n";
    SendMessageW(gUi->log, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(msg.c_str()));
}

static void EnableMapButtons(HWND hwnd, bool on)
{
    for (int id = ID_BTN_UNIVERSAL; id <= ID_BTN_UNTURNED; ++id)
        EnableWindow(GetDlgItem(hwnd, id), on ? TRUE : FALSE);
}

static void StartMap(const char* profile, const wchar_t* label)
{
    if (!gUi || gUi->mapping)
        return;
    if (!IsProcessElevated()) {
        MessageBoxW(gUi->hwnd,
            L"Run DriverLoader as Administrator.\n\nClick \"Restart as Admin\" first.",
            L"Driver2 Setup", MB_ICONWARNING);
        return;
    }
    gUi->mapping = true;
    EnableMapButtons(gUi->hwnd, false);
    SetStatus(L"Mapping driver — please wait…", RGB(255, 220, 120));
    AppendLog((std::wstring(L"[*] ") + label).c_str());

    std::wstring byovd = gUi->byovd;
    std::thread([profile, byovd]() {
        int ec = MapLabSession(byovd, profile);
        if (ec == 0)
            ec = ProbeDriverClient(byovd);
        PostMessageW(gUi->hwnd, WM_MAP_DONE, static_cast<WPARAM>(ec), 0);
    }).detach();
}

static void OpenCheatLoader()
{
    if (!gUi)
        return;
    wchar_t self[MAX_PATH]{};
    GetModuleFileNameW(nullptr, self, MAX_PATH);
    auto cheat = fs::path(self).parent_path() / L"CheatLoader.exe";
    if (!fs::exists(cheat)) {
        auto d2 = Driver2BinW();
        if (!d2.empty())
            cheat = fs::path(d2) / L"CheatLoader.exe";
    }
    if (fs::exists(cheat))
        ShellExecuteW(gUi->hwnd, L"open", cheat.wstring().c_str(), nullptr,
                      cheat.parent_path().wstring().c_str(), SW_SHOWNORMAL);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CREATE: {
        gUi->hwnd = hwnd;
        CreateWindowExW(0, L"STATIC", L"Driver2 Setup — map once per boot", WS_CHILD | WS_VISIBLE, 20, 14, 500,
                          24, hwnd, nullptr, nullptr, nullptr);
        gUi->status = CreateWindowExW(0, L"STATIC",
            L"Checking driver…", WS_CHILD | WS_VISIBLE, 20, 42, 500, 20, hwnd, nullptr, nullptr, nullptr);
        CreateWindowExW(0, L"BUTTON", L"Universal lab map (EAC + BE + ACE) — recommended",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 20, 72, 500, 36, hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_BTN_UNIVERSAL)), nullptr, nullptr);
        CreateWindowExW(0, L"BUTTON", L"Rob Run only (EAC)",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 20, 114, 245, 32, hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_BTN_ROBRUN)), nullptr, nullptr);
        CreateWindowExW(0, L"BUTTON", L"Unturned only (BattlEye)",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 275, 114, 245, 32, hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_BTN_UNTURNED)), nullptr, nullptr);
        CreateWindowExW(0, L"BUTTON", L"Test connection (probe)",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 20, 154, 160, 28, hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_BTN_PROBE)), nullptr, nullptr);
        CreateWindowExW(0, L"BUTTON", L"Open CheatLoader",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 190, 154, 160, 28, hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_BTN_CHEATLOADER)), nullptr, nullptr);
        CreateWindowExW(0, L"BUTTON", L"Restart as Admin",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 360, 154, 160, 28, hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_BTN_ELEVATE)), nullptr, nullptr);
        gUi->log = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
            20, 192, 500, 160, hwnd, nullptr, nullptr, nullptr);
        SendMessageW(gUi->log, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        if (DriverMappedQuick()) {
            SetStatus(L"Driver already loaded — open CheatLoader or play.", RGB(100, 220, 140));
            AppendLog(L"[+] Driver device is open.");
        } else if (!IsProcessElevated()) {
            SetStatus(L"Not elevated — click Restart as Admin, then map.", RGB(255, 160, 100));
            AppendLog(L"[!] Run as Administrator to map.");
        } else {
            SetStatus(L"Ready — pick a map profile below.", RGB(200, 210, 230));
            AppendLog(L"[*] map-host-lab: RTCore only, AC preflight, then Driver2.");
            AppendLog(L"[*] Close Rob Run first. TRIXX never used.");
        }
        SetTimer(hwnd, 1, 2500, nullptr);
        return 0;
    }
    case WM_TIMER:
        if (wp == 1 && !gUi->mapping) {
            if (DriverMappedQuick())
                SetStatus(L"Driver loaded — open CheatLoader.", RGB(100, 220, 140));
            return 0;
        }
        break;
    case WM_MAP_DONE: {
        gUi->mapping = false;
        EnableMapButtons(hwnd, true);
        int ec = static_cast<int>(wp);
        if (ec == 0) {
            SetStatus(L"Success! Driver is ready — open CheatLoader.", RGB(120, 255, 150));
            AppendLog(L"[+] Map + probe OK.");
            AppendLog(L"[+] Driver2 mapped. RTCore stays loaded (stop would bugcheck).");
        } else {
            wchar_t buf[96];
            swprintf_s(buf, L"[!] Map failed (exit %d).", ec);
            SetStatus(L"Map failed — installed RTCore cannot map Driver2.", RGB(255, 120, 100));
            AppendLog(buf);
            wchar_t tmp[MAX_PATH]{};
            if (GetTempPathW(MAX_PATH, tmp)) {
                std::ifstream in(std::wstring(tmp) + L"pf_diag.tmp");
                std::vector<std::string> lines;
                std::string line;
                while (std::getline(in, line)) {
                    if (!line.empty() && line.back() == '\r')
                        line.pop_back();
                    if (!line.empty())
                        lines.push_back(std::move(line));
                }
                size_t start = lines.size() > 6 ? lines.size() - 6 : 0;
                for (size_t i = start; i < lines.size(); ++i) {
                    std::wstring w(lines[i].begin(), lines[i].end());
                    AppendLog(w.c_str());
                }
            }
        }
        return 0;
    }
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case ID_BTN_UNIVERSAL:
            StartMap("universal", L"Universal lab map");
            break;
        case ID_BTN_ROBRUN:
            StartMap("robrun", L"Rob Run (EAC) map");
            break;
        case ID_BTN_UNTURNED:
            StartMap("r6", L"Unturned (BE) map");
            break;
        case ID_BTN_PROBE: {
            int ec = ProbeDriverClient(gUi->byovd);
            if (ec == 0) {
                SetStatus(L"Probe OK — driver reads work.", RGB(120, 255, 150));
                AppendLog(L"[+] Probe succeeded.");
            } else {
                SetStatus(L"Probe failed — map first.", RGB(255, 120, 100));
                AppendLog(L"[!] Probe failed — driver not mapped.");
            }
            break;
        }
        case ID_BTN_CHEATLOADER:
            OpenCheatLoader();
            break;
        case ID_BTN_ELEVATE: {
            wchar_t path[MAX_PATH]{};
            GetModuleFileNameW(nullptr, path, MAX_PATH);
            ShellExecuteW(hwnd, L"runas", path, nullptr, nullptr, SW_SHOW);
            break;
        }
        default:
            break;
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

} // namespace

int RunDriverLoaderGui()
{
    gUi = new UiState();
    gUi->byovd = FindByovdRootW();
    if (gUi->byovd.empty() || FindDriver2RootW().empty()) {
        MessageBoxW(nullptr, L"Run from driver2\\bin\\DriverLoader.exe (build loader-tool first)", L"Driver2 Setup",
                    MB_ICONERROR);
        delete gUi;
        return 1;
    }

    INITCOMMONCONTROLSEX icc{ sizeof(icc), ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = L"Driver2SetupGui";
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"Driver2 Setup",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT, 560, 400, nullptr,
        nullptr, wc.hInstance, nullptr);
    if (!hwnd) {
        delete gUi;
        return 1;
    }
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    delete gUi;
    gUi = nullptr;
    return 0;
}
