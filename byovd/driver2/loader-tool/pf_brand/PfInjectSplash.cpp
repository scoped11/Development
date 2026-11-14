#include "PfInjectSplash.hpp"
#include "PfBrand.hpp"
#include <algorithm>
#include <gdiplus.h>

#pragma comment(lib, "gdiplus.lib")

namespace {

using namespace Gdiplus;

struct SplashState {
    HWND hwnd = nullptr;
    float progress = 0.f;
    Image* img = nullptr;
    ULONG_PTR gdip = 0;
    bool workDone = false;
    bool workRan = false;
};

static SplashState* gSplash = nullptr;

static void PaintSplash(HWND hwnd)
{
    if (!gSplash)
        return;
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT rc;
    GetClientRect(hwnd, &rc);
    const int w = rc.right - rc.left;
    const int h = rc.bottom - rc.top;
    Bitmap back(w, h, PixelFormat32bppARGB);
    Graphics g(&back);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.Clear(Color(235, 12, 14, 18));

    FontFamily ff(L"Segoe UI");
    Font title(&ff, 15, FontStyleBold, UnitPixel);
    SolidBrush white(Color(255, 240, 240, 245));
    g.DrawString(L"penis-finger-external", -1, &title, PointF(16, 12), &white);

    const float margin = 20.f;
    const float barTop = 48.f;
    const float barH = static_cast<float>(h) - barTop - 36.f;
    const float barW = static_cast<float>(w) - margin * 2.f;
    float p = gSplash->progress;
    if (p < 0.f)
        p = 0.f;
    if (p > 1.f)
        p = 1.f;

    // Track background
    SolidBrush track(Color(80, 40, 42, 50));
    g.FillRectangle(&track, margin, barTop, barW, barH);

    if (gSplash->img && gSplash->img->GetLastStatus() == Ok) {
        const REAL sw = static_cast<REAL>(gSplash->img->GetWidth());
        const REAL sh = static_cast<REAL>(gSplash->img->GetHeight());
        const REAL minScale = 0.35f;
        const REAL stretch = minScale + p * (1.65f - minScale);
        const REAL drawW = barW * stretch;
        const REAL drawH = barH;
        const REAL x = margin;
        const REAL y = barTop;
        // Stretch from left (wrist) — finger extends right as progress grows
        g.DrawImage(gSplash->img, RectF(x, y, drawW, drawH), 0, 0, sw, sh, UnitPixel);
    } else {
        SolidBrush bar(Color(255, 220, 180, 120));
        g.FillRectangle(&bar, margin, barTop, barW * p, barH);
    }

    Font pctFont(&ff, 13, FontStyleRegular, UnitPixel);
    wchar_t pct[32];
    swprintf_s(pct, L"%.0f%%", p * 100.f);
    g.DrawString(pct, -1, &pctFont, PointF(margin, static_cast<REAL>(h) - 28), &white);

    Graphics screen(hdc);
    screen.DrawImage(&back, 0, 0);
    EndPaint(hwnd, &ps);
}

static LRESULT CALLBACK SplashProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_PAINT:
        PaintSplash(hwnd);
        return 0;
    case WM_DESTROY:
        return 0;
    default:
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
}

} // namespace

void RunPfInjectSplash(float durationSeconds, const std::function<void()>& work)
{
    if (durationSeconds < 0.2f)
        durationSeconds = 0.2f;

    ULONG_PTR gdip = 0;
    GdiplusStartupInput in;
    if (GdiplusStartup(&gdip, &in, nullptr) != Ok) {
        if (work)
            work();
        return;
    }

    SplashState state;
    std::wstring art = PfBrandImagePath();
    if (!art.empty()) {
        state.img = Image::FromFile(art.c_str());
        if (state.img && state.img->GetLastStatus() != Ok) {
            delete state.img;
            state.img = nullptr;
        }
    }
    state.gdip = gdip;
    gSplash = &state;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = SplashProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"PfInjectSplash";
    RegisterClassExW(&wc);

    const int winW = 420;
    const int winH = 200;
    int sx = GetSystemMetrics(SM_CXSCREEN);
    HWND hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, wc.lpszClassName, L"Injecting...",
        WS_POPUP | WS_VISIBLE, (sx - winW) / 2, 80, winW, winH, nullptr, nullptr, wc.hInstance, nullptr);
    state.hwnd = hwnd;

    const DWORD t0 = GetTickCount();
    const DWORD durationMs = static_cast<DWORD>(durationSeconds * 1000.f);
    MSG msg{};
    HWND parent = GetForegroundWindow();
    while (!state.workDone || state.progress < 1.f) {
        DWORD elapsed = GetTickCount() - t0;
        float timeP = static_cast<float>(elapsed) / static_cast<float>(durationMs);
        if (timeP > 1.f)
            timeP = 1.f;
        if (!state.workRan && timeP >= 0.85f) {
            if (work)
                work();
            state.workRan = true;
            state.workDone = true;
        }
        state.progress = state.workDone ? (std::max)(state.progress, timeP) : timeP * 0.85f;
        if (state.workDone && elapsed >= durationMs)
            break;

        InvalidateRect(hwnd, nullptr, FALSE);
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (parent && msg.hwnd == parent)
                DispatchMessageW(&msg);
            else {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
        }
        Sleep(16);
    }
    state.progress = 1.f;
    InvalidateRect(hwnd, nullptr, FALSE);
    UpdateWindow(hwnd);
    Sleep(80);
    if (state.img)
        delete state.img;
    DestroyWindow(hwnd);
    GdiplusShutdown(gdip);
    gSplash = nullptr;
}
