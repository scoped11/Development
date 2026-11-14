// GDI+ game tile picker for CheatLoader — auto-detects mapped Driver2.
#include "CheatLoaderCore.hpp"
#include "pf_brand/PfInjectSplash.hpp"
#include <Windowsx.h>
#include <gdiplus.h>
#include <string>

#pragma comment(lib, "gdiplus.lib")

namespace {

using namespace Gdiplus;

struct GameLaunchDefMutable : GameLaunchDef {
    Gdiplus::Image* imgCache = nullptr;
};

struct PickerState {
    GameLaunchDefMutable* games = nullptr;
    size_t count = 0;
    int hover = -1;
    bool driverReady = false;
    bool gameRunningRobRun = false;
    ULONG_PTR gdip = 0;
    HWND hwnd = nullptr;
};

static PickerState* gState = nullptr;
static constexpr UINT_PTR kTimerDriverPoll = 1;
static constexpr UINT_PTR kTimerLaunch = 2;
static constexpr UINT kPollMs = 2000;
static int gPendingLaunch = -1;

static std::wstring SteamPath()
{
    wchar_t buf[MAX_PATH]{};
    DWORD n = GetEnvironmentVariableW(L"SteamPath", buf, MAX_PATH);
    if (n > 0 && n < MAX_PATH)
        return buf;
    HKEY hk = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", 0, KEY_READ, &hk) != ERROR_SUCCESS)
        return {};
    DWORD sz = sizeof(buf);
    RegQueryValueExW(hk, L"SteamPath", nullptr, nullptr, reinterpret_cast<LPBYTE>(buf), &sz);
    RegCloseKey(hk);
    return buf;
}

static std::wstring SteamLibraryCache(uint32_t appId)
{
    std::wstring steam = SteamPath();
    if (steam.empty())
        return {};
    wchar_t name[64];
    swprintf_s(name, L"%u_library_600x900.jpg", appId);
    auto p = fs::path(steam) / L"appcache" / L"librarycache" / name;
    if (fs::exists(p))
        return p.wstring();
    swprintf_s(name, L"%u_header.jpg", appId);
    p = fs::path(steam) / L"appcache" / L"librarycache" / name;
    if (fs::exists(p))
        return p.wstring();
    return {};
}

static std::wstring SteamGameFolderArt(const wchar_t* folder)
{
    std::wstring steam = SteamPath();
    if (steam.empty())
        return {};
    auto libFile = steam + L"\\steamapps\\libraryfolders.vdf";
    std::ifstream f(libFile);
    if (!f)
        return {};
    std::string blob((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    for (size_t i = 0; i < blob.size();) {
        auto q = blob.find('"', i);
        if (q == std::string::npos)
            break;
        auto q2 = blob.find('"', q + 1);
        if (q2 == std::string::npos)
            break;
        std::string path = blob.substr(q + 1, q2 - q - 1);
        if (path.size() > 2 && path[1] == ':') {
            std::wstring wpath(path.begin(), path.end());
            for (const wchar_t* art : { L"library_600x900.jpg", L"header.jpg", L"logo.png" }) {
                auto p = fs::path(wpath) / L"steamapps" / L"common" / folder / art;
                if (fs::exists(p))
                    return p.wstring();
            }
        }
        i = q2 + 1;
    }
    auto def = fs::path(steam) / L"steamapps" / L"common" / folder;
    for (const wchar_t* art : { L"library_600x900.jpg", L"header.jpg" }) {
        auto p = def / art;
        if (fs::exists(p))
            return p.wstring();
    }
    return {};
}

static std::wstring ResolveGameImage(const GameLaunchDef& g)
{
    const wchar_t* exts[] = { L".jpg", L".png", L".jpeg" };
    const wchar_t* dirs[] = { L"bin\\games\\", L"driver2\\loader-tool\\games\\", L"games\\" };
    for (const wchar_t* dir : dirs) {
        for (const wchar_t* ext : exts) {
            auto p = ResolvePath(std::wstring(dir) + g.imageBaseName + ext);
            if (!p.empty())
                return p;
        }
    }
    if (wcscmp(g.id, L"unturned") == 0) {
        auto c = SteamLibraryCache(304930);
        if (!c.empty())
            return c;
        return SteamGameFolderArt(L"Unturned");
    }
    if (wcscmp(g.id, L"robrun") == 0)
        return SteamGameFolderArt(L"Rob Run");
    return {};
}

static void PollDriverAndGame()
{
    if (!gState)
        return;
    bool was = gState->driverReady;
    if (DriverConnected()) {
        if (!GlobalDriver().ProbeSelfMarker())
            ResetDriverConnection();
    } else if (IsDriverDevicePresent()) {
        TryAutoConnect(false);
    }
    gState->driverReady = DriverConnected();
    if (was != gState->driverReady && gState->hwnd)
        InvalidateRect(gState->hwnd, nullptr, FALSE);

    for (size_t i = 0; i < gState->count; ++i) {
        if (wcscmp(gState->games[i].id, L"robrun") == 0) {
            bool run = IsGameProcessRunning(gState->games[i].processExe);
            if (run != gState->gameRunningRobRun) {
                gState->gameRunningRobRun = run;
                if (gState->hwnd)
                    InvalidateRect(gState->hwnd, nullptr, FALSE);
            }
            break;
        }
    }
}

static void DrawGradientCard(Graphics& gfx, const RectF& r, DWORD c1, DWORD c2, const wchar_t* title)
{
    LinearGradientBrush bg(r, Color(255, (c1 >> 16) & 0xFF, (c1 >> 8) & 0xFF, c1 & 0xFF),
                           Color(255, (c2 >> 16) & 0xFF, (c2 >> 8) & 0xFF, c2 & 0xFF), 45.f);
    gfx.FillRectangle(&bg, r);
    FontFamily ff(L"Segoe UI");
    Font titleFont(&ff, 22, FontStyleBold, UnitPixel);
    SolidBrush white(Color(240, 255, 255, 255));
    RectF tr(r.X + 16, r.Y + r.Height - 44, r.Width - 32, 36);
    StringFormat sf;
    gfx.DrawString(title, -1, &titleFont, tr, &sf, &white);
}

static void Paint(HWND hwnd)
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT rc;
    GetClientRect(hwnd, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;

    Bitmap back(w, h);
    Graphics gfx(&back);
    gfx.SetSmoothingMode(SmoothingModeAntiAlias);
    gfx.Clear(Color(255, 18, 20, 26));

    FontFamily ff(L"Segoe UI");
    Font hdr(&ff, 26, FontStyleBold, UnitPixel);
    SolidBrush dim(Color(200, 180, 190, 210));
    gfx.DrawString(L"Select game", -1, &hdr, PointF(24, 20), &dim);

    wchar_t status[160];
    if (gState->driverReady)
        wcscpy_s(status, L"Driver2: ready (auto-detected)");
    else if (IsDriverDevicePresent())
        wcscpy_s(status, L"Driver2: device open — connecting...");
    else
        wcscpy_s(status, L"Driver2: not loaded — run DriverLoader as admin once per boot");
    Font stFont(&ff, 14, FontStyleRegular, UnitPixel);
    SolidBrush stBr(gState->driverReady ? Color(220, 80, 220, 120) : Color(220, 240, 120, 80));
    gfx.DrawString(status, -1, &stFont, PointF(24, 54), &stBr);

    if (gState->gameRunningRobRun) {
        SolidBrush runBr(Color(200, 100, 180, 255));
        gfx.DrawString(L"Rob Run is running — click tile to start / restart ESP", -1, &stFont, PointF(24, 74),
                       &runBr);
    }

    const int cols = 2;
    const float pad = 24.f;
    const float top = 100.f;
    const float gap = 20.f;
    const float cardW = (w - pad * 2 - gap) / cols;
    const float cardH = h - top - pad - 24.f;

    for (size_t i = 0; i < gState->count; ++i) {
        float x = pad + static_cast<float>(i) * (cardW + gap);
        RectF card(x, top, cardW, cardH);
        bool hot = static_cast<int>(i) == gState->hover;
        if (hot) {
            Pen glow(Color(180, 120, 200, 255), 3.f);
            gfx.DrawRectangle(&glow, card.X - 2, card.Y - 2, card.Width + 4, card.Height + 4);
        }
        GameLaunchDefMutable& g = gState->games[i];
        if (!g.imgCache) {
            std::wstring art = ResolveGameImage(g);
            if (!art.empty()) {
                g.imgCache = Image::FromFile(art.c_str());
                if (g.imgCache && g.imgCache->GetLastStatus() != Ok) {
                    delete g.imgCache;
                    g.imgCache = nullptr;
                }
            }
        }
        if (g.imgCache) {
            gfx.DrawImage(g.imgCache, card);
            RectF shade(card.X, card.Y + card.Height - 70, card.Width, 70);
            LinearGradientBrush shadeBr(shade, Color(0, 0, 0, 0), Color(200, 0, 0, 0), 90.f);
            gfx.FillRectangle(&shadeBr, shade);
        } else {
            DrawGradientCard(gfx, card, g.cardColor1, g.cardColor2, g.displayName);
        }
        Font nameFont(&ff, 20, FontStyleBold, UnitPixel);
        SolidBrush nameBr(Color(255, 255, 255, 255));
        RectF nr(card.X + 14, card.Y + card.Height - 40, card.Width - 28, 32);
        StringFormat nameSf;
        gfx.DrawString(g.displayName, -1, &nameFont, nr, &nameSf, &nameBr);

        auto cheat = ResolvePath(g.cheatRel);
        if (cheat.empty()) {
            Font warn(&ff, 12, FontStyleItalic, UnitPixel);
            SolidBrush warnBr(Color(220, 255, 200, 120));
            gfx.DrawString(L"Build missing", -1, &warn, PointF(card.X + 14, card.Y + 14), &warnBr);
        } else if (wcscmp(g.id, L"robrun") == 0) {
            Font sub(&ff, 11, FontStyleRegular, UnitPixel);
            SolidBrush subBr(Color(220, 200, 255, 200));
            gfx.DrawString(L"ESP overlay — safe to relaunch while in match", -1, &sub, PointF(card.X + 14, card.Y + 14),
                           &subBr);
        }
    }

    Font hint(&ff, 12, FontStyleRegular, UnitPixel);
    SolidBrush hintBr(Color(140, 160, 170, 190));
    gfx.DrawString(
        L"Driver is detected automatically. Crash or close ESP anytime — click again. Game can stay open.",
        -1, &hint, PointF(24, static_cast<REAL>(h - 28)), &hintBr);

    Graphics screen(hdc);
    screen.DrawImage(&back, 0, 0);
    EndPaint(hwnd, &ps);
}

static int HitTest(int x, int y, const RECT& rc)
{
    int w = rc.right - rc.left;
    const int cols = 2;
    const float pad = 24.f;
    const float top = 100.f;
    const float gap = 20.f;
    const float cardW = (w - pad * 2 - gap) / cols;
    const float cardH = (rc.bottom - rc.top) - top - pad - 24.f;
    for (size_t i = 0; i < gState->count; ++i) {
        float cx = pad + static_cast<float>(i) * (cardW + gap);
        if (x >= cx && x <= cx + cardW && y >= top && y <= top + cardH)
            return static_cast<int>(i);
    }
    return -1;
}

static void LaunchGameImpl(int index)
{
    if (!gState || index < 0 || static_cast<size_t>(index) >= gState->count)
        return;
    const GameLaunchDef& g = gState->games[index];
    auto cheat = ResolvePath(g.cheatRel);
    if (cheat.empty()) {
        wchar_t msg[512];
        swprintf_s(msg,
            L"%s is not built.\n\nRun from dev tree:\n"
            L"  .\\driver2\\build.ps1 -BuildRobRunEsp -BuildLoaders",
            g.displayName);
        MessageBoxW(gState->hwnd, msg, L"CheatLoader", MB_ICONWARNING);
        return;
    }
    if (!TryAutoConnect(true)) {
        int r = MessageBoxW(gState->hwnd,
            L"Driver2 is not loaded.\n\nRun DriverLoader.exe as Administrator (once per boot), then click again.",
            L"CheatLoader", MB_ICONWARNING | MB_OKCANCEL);
        if (r == IDOK) {
            auto loader = ResolvePath(L"bin\\DriverLoader.exe");
            if (loader.empty())
                loader = ResolvePath(L"driver2\\bin\\DriverLoader.exe");
            if (!loader.empty())
                ShellExecuteW(gState->hwnd, L"runas", loader.c_str(), L"-lab-universal", nullptr, SW_SHOWDEFAULT);
        }
        return;
    }

    ApplyTitleSessionForGame(g);

    if (wcscmp(g.id, L"robrun") == 0 && !IsGameProcessRunning(g.processExe)) {
        int r = MessageBoxW(gState->hwnd,
            L"Rob Run does not appear to be running.\n\nLaunch anyway? (ESP waits until you are in a match.)",
            L"CheatLoader", MB_YESNO | MB_ICONQUESTION);
        if (r != IDYES)
            return;
    }

    int ec = 0;
    RunPfInjectSplash(1.8f, [&]() {
        ec = LaunchCheat(cheat, g.defaultArgs ? g.defaultArgs : L"");
    });
    if (ec != 0) {
        MessageBoxW(gState->hwnd, L"Could not start cheat EXE.", L"CheatLoader", MB_ICONERROR);
        return;
    }
}

static void RunElevatedDriverMapLab()
{
    auto loader = ResolvePath(L"bin\\DriverLoader.exe");
    if (loader.empty())
        loader = ResolvePath(L"driver2\\bin\\DriverLoader.exe");
    if (loader.empty()) {
        MessageBoxW(gState->hwnd, L"DriverLoader.exe not found in bin\\", L"CheatLoader", MB_ICONERROR);
        return;
    }
    ShellExecuteW(gState->hwnd, L"runas", loader.c_str(), nullptr, nullptr, SW_SHOWDEFAULT);
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CREATE:
        SetTimer(hwnd, kTimerDriverPoll, kPollMs, nullptr);
        PollDriverAndGame();
        return 0;
    case WM_MOUSEMOVE: {
        RECT rc;
        GetClientRect(hwnd, &rc);
        int hit = HitTest(GET_X_LPARAM(lp), GET_Y_LPARAM(lp), rc);
        if (gState && hit != gState->hover) {
            gState->hover = hit;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }
    case WM_LBUTTONUP: {
        RECT rc;
        GetClientRect(hwnd, &rc);
        int hit = HitTest(GET_X_LPARAM(lp), GET_Y_LPARAM(lp), rc);
        if (hit >= 0) {
            gPendingLaunch = hit;
            SetTimer(hwnd, kTimerLaunch, 1, nullptr);
        }
        return 0;
    }
    case WM_TIMER:
        if (wp == kTimerDriverPoll) {
            PollDriverAndGame();
            return 0;
        }
        if (wp == kTimerLaunch && gPendingLaunch >= 0) {
            KillTimer(hwnd, kTimerLaunch);
            int idx = gPendingLaunch;
            gPendingLaunch = -1;
            LaunchGameImpl(idx);
            return 0;
        }
        return 0;
    case WM_RBUTTONUP: {
        if (!gState || !gState->driverReady)
            RunElevatedDriverMapLab();
        else
            MessageBoxW(hwnd, L"Driver already loaded.", L"CheatLoader", MB_ICONINFORMATION);
        return 0;
    }
    case WM_PAINT:
        Paint(hwnd);
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd, kTimerDriverPoll);
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
}

} // namespace

int RunGamePickerGui()
{
    size_t n = 0;
    const GameLaunchDef* src = CatalogGames(&n);
    static GameLaunchDefMutable games[8];
    if (n > 8)
        n = 8;
    for (size_t i = 0; i < n; ++i) {
        games[i].id = src[i].id;
        games[i].displayName = src[i].displayName;
        games[i].imageBaseName = src[i].imageBaseName;
        games[i].cheatRel = src[i].cheatRel;
        games[i].defaultArgs = src[i].defaultArgs;
        games[i].processExe = src[i].processExe;
        games[i].mapProfile = src[i].mapProfile;
        games[i].titleIoctl = src[i].titleIoctl;
        games[i].cardColor1 = src[i].cardColor1;
        games[i].cardColor2 = src[i].cardColor2;
        games[i].imgCache = nullptr;
    }

    PickerState state{};
    state.games = games;
    state.count = n;
    gState = &state;

    ULONG_PTR gdip = 0;
    GdiplusStartupInput gdipIn;
    if (GdiplusStartup(&gdip, &gdipIn, nullptr) != Ok)
        return 1;
    state.gdip = gdip;

    ByovdRoot() = FindByovdRootW();
    if (ByovdRoot().empty() || FindDriver2RootW().empty()) {
        MessageBoxW(nullptr, L"Run from driver2\\bin\\CheatLoader.exe", L"CheatLoader", MB_ICONERROR);
        GdiplusShutdown(gdip);
        return 1;
    }

    PollDriverAndGame();

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_HAND);
    wc.lpszClassName = L"Driver2CheatPicker";
    RegisterClassExW(&wc);

    const int winW = 740;
    const int winH = 440;
    int sx = GetSystemMetrics(SM_CXSCREEN);
    int sy = GetSystemMetrics(SM_CYSCREEN);
    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"CheatLoader",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, (sx - winW) / 2, (sy - winH) / 2, winW, winH,
        nullptr, nullptr, wc.hInstance, nullptr);
    if (!hwnd) {
        GdiplusShutdown(gdip);
        return 1;
    }
    state.hwnd = hwnd;
    gState = &state;
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    for (size_t i = 0; i < n; ++i) {
        if (games[i].imgCache)
            delete games[i].imgCache;
    }
    GdiplusShutdown(gdip);
    return 0;
}
