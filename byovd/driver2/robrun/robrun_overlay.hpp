#pragma once
#include "../../../dma/src/math/vector.h"
#include <Windows.h>
#include <vector>

struct EspDot {
    Vector2 screen;
    bool onScreen = false;
};

class RobRunOverlay {
public:
    bool Create()
    {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = DefWindowProcW;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"RobRunDriver2Esp";
        RegisterClassExW(&wc);
        hwnd_ = CreateWindowExW(
            WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
            wc.lpszClassName, L"", WS_POPUP, 0, 0,
            GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
            nullptr, nullptr, wc.hInstance, nullptr);
        if (!hwnd_)
            return false;
        SetLayeredWindowAttributes(hwnd_, RGB(0, 0, 0), 0, LWA_COLORKEY);
        ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
        return true;
    }

    void Draw(const std::vector<EspDot>& dots)
    {
        if (!hwnd_)
            return;
        HDC screen = GetDC(nullptr);
        HDC mem = CreateCompatibleDC(screen);
        int w = GetSystemMetrics(SM_CXSCREEN);
        int h = GetSystemMetrics(SM_CYSCREEN);
        BITMAPINFO bmi{};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = w;
        bmi.bmiHeader.biHeight = -h;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        void* bits = nullptr;
        HBITMAP bmp = CreateDIBSection(mem, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
        HGDIOBJ old = SelectObject(mem, bmp);
        memset(bits, 0, static_cast<size_t>(w) * static_cast<size_t>(h) * 4);
        HPEN pen = CreatePen(PS_SOLID, 2, RGB(0, 220, 80));
        HBRUSH brush = CreateSolidBrush(RGB(0, 220, 80));
        HGDIOBJ oldPen = SelectObject(mem, pen);
        HGDIOBJ oldBrush = SelectObject(mem, brush);
        for (const auto& d : dots) {
            if (!d.onScreen)
                continue;
            int x = static_cast<int>(d.screen.x);
            int y = static_cast<int>(d.screen.y);
            Ellipse(mem, x - 6, y - 6, x + 6, y + 6);
        }
        SelectObject(mem, oldPen);
        SelectObject(mem, oldBrush);
        DeleteObject(pen);
        DeleteObject(brush);
        POINT pt{ 0, 0 };
        SIZE sz{ w, h };
        UpdateLayeredWindow(hwnd_, screen, &pt, &sz, mem, &pt, RGB(0, 0, 0), nullptr, ULW_COLORKEY);
        SelectObject(mem, old);
        DeleteObject(bmp);
        DeleteDC(mem);
        ReleaseDC(nullptr, screen);
    }

    void Destroy()
    {
        if (hwnd_) {
            DestroyWindow(hwnd_);
            hwnd_ = nullptr;
        }
    }

private:
    HWND hwnd_ = nullptr;
};
