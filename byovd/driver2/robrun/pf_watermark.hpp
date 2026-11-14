#pragma once
#include "../loader-tool/LoaderPaths.hpp"
#include <Windows.h>
#include <filesystem>
#include <gdiplus.h>
#include <string>

#pragma comment(lib, "gdiplus.lib")

// Small top-right thumbnail while ESP is in-game.
class PfWatermark {
public:
    ~PfWatermark() { Destroy(); }

    bool Create(const std::wstring& imagePath, int sizePx = 132)
    {
        if (imagePath.empty())
            return false;
        if (Gdiplus::GdiplusStartup(&gdip_, &Gdiplus::GdiplusStartupInput{}, nullptr) != Gdiplus::Ok)
            return false;
        img_ = Gdiplus::Image::FromFile(imagePath.c_str());
        if (!img_ || img_->GetLastStatus() != Gdiplus::Ok) {
            Destroy();
            return false;
        }
        size_ = sizePx;
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = DefWindowProcW;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"PfEspWatermark";
        RegisterClassExW(&wc);
        int sw = GetSystemMetrics(SM_CXSCREEN);
        int x = sw - size_ - 12;
        hwnd_ = CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
            wc.lpszClassName, L"", WS_POPUP, x, 12, size_, size_, nullptr, nullptr, wc.hInstance, nullptr);
        if (!hwnd_) {
            Destroy();
            return false;
        }
        Redraw();
        ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
        return true;
    }

    void Redraw()
    {
        if (!hwnd_ || !img_)
            return;
        Gdiplus::Bitmap bmp(size_, size_, PixelFormat32bppARGB);
        Gdiplus::Graphics g(&bmp);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.Clear(Gdiplus::Color(200, 10, 12, 16));
        Gdiplus::Pen border(Gdiplus::Color(220, 255, 200, 100), 2.f);
        g.DrawRectangle(&border, 1, 1, size_ - 2, size_ - 2);
        const int pad = 6;
        g.DrawImage(img_, pad, pad, size_ - pad * 2, size_ - pad * 2);

        HDC screen = GetDC(nullptr);
        HDC mem = CreateCompatibleDC(screen);
        HBITMAP hbmp;
        bmp.GetHBITMAP(Gdiplus::Color(0, 0, 0, 0), &hbmp);
        HGDIOBJ old = SelectObject(mem, hbmp);
        POINT pt{ 0, 0 };
        SIZE sz{ size_, size_ };
        BLENDFUNCTION blend{};
        blend.SourceConstantAlpha = 230;
        blend.AlphaFormat = AC_SRC_ALPHA;
        UpdateLayeredWindow(hwnd_, screen, nullptr, &sz, mem, &pt, 0, &blend, ULW_ALPHA);
        SelectObject(mem, old);
        DeleteObject(hbmp);
        DeleteDC(mem);
        ReleaseDC(nullptr, screen);
    }

    void Destroy()
    {
        if (hwnd_) {
            DestroyWindow(hwnd_);
            hwnd_ = nullptr;
        }
        if (img_) {
            delete img_;
            img_ = nullptr;
        }
        if (gdip_) {
            Gdiplus::GdiplusShutdown(gdip_);
            gdip_ = 0;
        }
    }

private:
    HWND hwnd_ = nullptr;
    Gdiplus::Image* img_ = nullptr;
    ULONG_PTR gdip_ = 0;
    int size_ = 132;
};

inline std::wstring PfBrandImagePathRobRun()
{
    const wchar_t* rels[] = {
        L"bin\\brand\\penis-finger-external.png",
        L"..\\..\\bin\\brand\\penis-finger-external.png",
        L"brand\\penis-finger-external.png",
        L"driver2\\bin\\brand\\penis-finger-external.png",
        L"driver2\\loader-tool\\pf_brand\\penis-finger-external.png",
    };
    wchar_t exe[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    fs::path base = fs::path(exe).parent_path();
    for (int up = 0; up < 5; ++up) {
        for (const wchar_t* r : rels) {
            auto p = base / r;
            if (fs::exists(p))
                return p.wstring();
        }
        base = base.parent_path();
    }
    return {};
}
