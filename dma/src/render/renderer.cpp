#include "renderer.h"
#include "config/config.h"
#include "render/splash.h"
#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#include <dwmapi.h>
#include <timeapi.h>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <thread>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "winmm.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
    HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

// =============================================================================
// Window + Device creation
// =============================================================================

bool Renderer::Init(const RendererConfig& config) {
    m_width = config.width;
    m_height = config.height;
    m_windowedWidth = config.width;
    m_windowedHeight = config.height;
    m_vsync = config.vsync;

    if (!CreateWindowAndDevice(config)) return false;
    if (!InitImGui()) return false;

    if (config.streamProof)
        SetStreamProof(true);

    timeBeginPeriod(1);
    m_initialized = true;
    m_running = true;
    std::cout << "[+] Renderer initialized (" << m_width << "x" << m_height << ")\n";
    return true;
}

void Renderer::Show() {
    if (!m_hwnd)
        return;
    ShowWindow(m_hwnd, SW_SHOWDEFAULT);
    UpdateWindow(m_hwnd);
}

bool Renderer::CreateWindowAndDevice(const RendererConfig& config) {
    std::wstring wtitle(config.title.begin(), config.title.end());

    m_wc = {};
    m_wc.cbSize = sizeof(WNDCLASSEXW);
    m_wc.style = CS_CLASSDC;
    m_wc.lpfnWndProc = WndProc;
    m_wc.hInstance = GetModuleHandle(nullptr);
    m_wc.lpszClassName = L"RobRunDMA";
    RegisterClassExW(&m_wc);

    m_hwnd = CreateWindowW(
        m_wc.lpszClassName, wtitle.c_str(),
        WS_OVERLAPPEDWINDOW,
        100, 100, config.width, config.height,
        nullptr, nullptr, m_wc.hInstance, nullptr);

    if (!m_hwnd) {
        std::cerr << "[-] Window creation failed\n";
        return false;
    }

    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = m_hwnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0
    };

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        featureLevels, 2, D3D11_SDK_VERSION,
        &sd, &m_swapChain, &m_device, &featureLevel, &m_context);

    if (FAILED(hr)) {
        std::cerr << "[-] D3D11 device creation failed\n";
        return false;
    }

    ID3D11Texture2D* backBuffer = nullptr;
    m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
    m_device->CreateRenderTargetView(backBuffer, nullptr, &m_rtv);
    backBuffer->Release();

    return true;
}

bool Renderer::InitImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr; // Don't save layout to disk

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.FramePadding = ImVec2(8, 4);
    style.ItemSpacing = ImVec2(8, 6);

    // Customise colours for a clean dark theme
    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg]        = ImVec4(0.06f, 0.07f, 0.09f, 0.97f);
    colors[ImGuiCol_ChildBg]         = ImVec4(0.08f, 0.09f, 0.11f, 0.45f);
    colors[ImGuiCol_TitleBg]         = ImVec4(0.06f, 0.07f, 0.09f, 1.00f);
    colors[ImGuiCol_TitleBgActive]   = ImVec4(0.06f, 0.07f, 0.09f, 1.00f);
    colors[ImGuiCol_FrameBg]         = ImVec4(0.12f, 0.14f, 0.16f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]  = ImVec4(0.16f, 0.20f, 0.22f, 1.00f);
    colors[ImGuiCol_FrameBgActive]   = ImVec4(0.14f, 0.24f, 0.22f, 1.00f);
    colors[ImGuiCol_Button]          = ImVec4(0.13f, 0.15f, 0.17f, 1.00f);
    colors[ImGuiCol_ButtonHovered]   = ImVec4(0.16f, 0.26f, 0.24f, 1.00f);
    colors[ImGuiCol_ButtonActive]    = ImVec4(0.22f, 0.48f, 0.42f, 1.00f);
    colors[ImGuiCol_CheckMark]       = ImVec4(0.42f, 0.82f, 0.74f, 1.00f);
    colors[ImGuiCol_SliderGrab]      = ImVec4(0.36f, 0.72f, 0.66f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]= ImVec4(0.50f, 0.88f, 0.80f, 1.00f);
    colors[ImGuiCol_Header]          = ImVec4(0.12f, 0.20f, 0.19f, 0.70f);
    colors[ImGuiCol_HeaderHovered]   = ImVec4(0.16f, 0.28f, 0.26f, 0.85f);
    colors[ImGuiCol_HeaderActive]    = ImVec4(0.22f, 0.48f, 0.42f, 0.90f);
    colors[ImGuiCol_Separator]       = ImVec4(0.42f, 0.82f, 0.74f, 0.28f);
    colors[ImGuiCol_TableHeaderBg]   = ImVec4(0.12f, 0.12f, 0.13f, 1.00f);
    colors[ImGuiCol_TableRowBg]      = ImVec4(0.08f, 0.08f, 0.09f, 0.40f);
    colors[ImGuiCol_TableRowBgAlt]   = ImVec4(0.12f, 0.12f, 0.13f, 0.45f);

    ImGui_ImplWin32_Init(m_hwnd);
    ImGui_ImplDX11_Init(m_device, m_context);

    return true;
}

// =============================================================================
// Frame management
// =============================================================================

void Renderer::BeginFrame() {
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void Renderer::EndFrame() {
    ImGui::Render();
    if (!m_context || !m_swapChain || !m_rtv)
        return;
    float clearColor[4];
    if (m_clearTransparent) {
        clearColor[0] = clearColor[1] = clearColor[2] = clearColor[3] = 0.f;
    } else {
        clearColor[0] = 0.02f;
        clearColor[1] = 0.02f;
        clearColor[2] = 0.03f;
        clearColor[3] = 1.0f;
    }
    m_context->OMSetRenderTargets(1, &m_rtv, nullptr);
    m_context->ClearRenderTargetView(m_rtv, clearColor);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    const float monitor = static_cast<float>(MonitorRefreshHz());
    float cap = m_frameCap > 15.f ? m_frameCap : monitor;
    if (monitor >= 30.f)
        cap = (std::min)(cap, monitor);
    cap = (std::clamp)(cap, 20.f, 240.f);
    const bool vblank = m_vsync && cap >= monitor * 0.92f && monitor >= 30.f;
    m_swapChain->Present(vblank ? 1 : 0, 0);
    if (!vblank)
        WaitFrameCap();
}

void Renderer::ResizeClient(int width, int height) {
    if (width < 64 || height < 64 || !m_device || !m_swapChain)
        return;
    if (m_rtv && width == m_width && height == m_height)
        return;
    if (m_context)
        m_context->OMSetRenderTargets(0, nullptr, nullptr);
    if (m_rtv) {
        m_rtv->Release();
        m_rtv = nullptr;
    }
    const HRESULT resized = m_swapChain->ResizeBuffers(
        0, static_cast<UINT>(width), static_cast<UINT>(height), DXGI_FORMAT_UNKNOWN, 0);
    ID3D11Texture2D* backBuf = nullptr;
    if (FAILED(m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuf))) || !backBuf)
        return;
    m_device->CreateRenderTargetView(backBuf, nullptr, &m_rtv);
    backBuf->Release();
    if (m_rtv && SUCCEEDED(resized)) {
        m_width = width;
        m_height = height;
    }
}

void Renderer::ApplyFullscreen(bool enable, int width, int height) {
    if (!m_hwnd)
        return;
    m_resizing = true;
    if (enable) {
        if (!m_fullscreenActive)
            GetWindowRect(m_hwnd, &m_windowedRect);
        if (width <= 0)
            width = GetSystemMetrics(SM_CXSCREEN);
        if (height <= 0)
            height = GetSystemMetrics(SM_CYSCREEN);
        SetWindowLongPtr(m_hwnd, GWL_STYLE, WS_POPUP);
        SetWindowPos(m_hwnd, HWND_TOPMOST, 0, 0, width, height,
            SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        m_lastFullscreenW = width;
        m_lastFullscreenH = height;
        m_resizePending = false;
        m_resizing = false;
        ResizeClient(width, height);
    } else {
        SetWindowLongPtr(m_hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);
        const int w = m_windowedWidth > 0 ? m_windowedWidth : 1280;
        const int h = m_windowedHeight > 0 ? m_windowedHeight : 720;
        int x = m_windowedRect.left;
        int y = m_windowedRect.top;
        if (x == 0 && y == 0) {
            x = 100;
            y = 100;
        }
        SetWindowPos(m_hwnd, HWND_NOTOPMOST, x, y, w, h,
            SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        m_resizePending = false;
        m_resizing = false;
        ResizeClient(w, h);
    }
    m_resizing = false;
    m_fullscreenActive = enable;
}

void Renderer::ApplyPendingResize() {
    if (!m_resizePending)
        return;
    m_resizePending = false;
    ResizeClient(m_pendingW, m_pendingH);
}

void Renderer::CommitOverlayWindow() {
    if (!m_hwnd)
        return;

    if (!m_wantSplashChrome &&
        (m_wantFullscreen != m_fullscreenActive ||
         (m_wantFullscreen &&
          (m_wantFsW != m_lastFullscreenW || m_wantFsH != m_lastFullscreenH)))) {
        ApplyFullscreen(m_wantFullscreen, m_wantFsW, m_wantFsH);
    } else if (m_wantSplashChrome && m_fullscreenActive) {
        ApplyFullscreen(false, 0, 0);
    }

    LONG currentEx = GetWindowLongPtr(m_hwnd, GWL_EXSTYLE);
    LONG wantEx = currentEx & ~(WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_TOOLWINDOW);
    if (m_wantSplashChrome) {
        wantEx |= WS_EX_APPWINDOW | WS_EX_TOPMOST;
        m_clearTransparent = false;
    } else if (m_wantSinglePc) {
        wantEx |= WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW;
        if (!m_wantInteract)
            wantEx |= WS_EX_TRANSPARENT;
        m_clearTransparent = true;
    } else if (m_wantFullscreen) {
        wantEx |= WS_EX_TOPMOST;
        m_clearTransparent = false;
    } else {
        m_clearTransparent = false;
    }

    const bool splashLatch = m_wantSplashChrome && m_splashChromeApplied;
    if (!splashLatch && wantEx != currentEx && wantEx != m_lastExStyle) {
        SetWindowLongPtr(m_hwnd, GWL_EXSTYLE, wantEx);
        SetWindowPos(m_hwnd, m_wantSplashChrome ? HWND_TOPMOST : nullptr, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | (m_wantSplashChrome ? 0 : SWP_NOZORDER) | SWP_NOACTIVATE | SWP_FRAMECHANGED);
        m_lastExStyle = wantEx;
        if (m_wantSplashChrome)
            m_splashChromeApplied = true;
    }
    if (!m_wantSplashChrome)
        m_splashChromeApplied = false;

    if (m_wantSplashChrome) {
        if (m_singlePcOverlay) {
            MARGINS margins = { 0, 0, 0, 0 };
            DwmExtendFrameIntoClientArea(m_hwnd, &margins);
            m_singlePcOverlay = false;
        }
    } else if (m_wantSinglePc && !m_singlePcOverlay) {
        MARGINS margins = { -1, -1, -1, -1 };
        DwmExtendFrameIntoClientArea(m_hwnd, &margins);
        m_singlePcOverlay = true;
    } else if (!m_wantSinglePc && m_singlePcOverlay) {
        MARGINS margins = { 0, 0, 0, 0 };
        DwmExtendFrameIntoClientArea(m_hwnd, &margins);
        m_singlePcOverlay = false;
    }

    if (m_wantStreamProof != m_streamProof) {
        m_streamProof = m_wantStreamProof;
        SetStreamProof(m_streamProof);
    }
    if (m_wantVsync != m_vsync)
        SetVsync(m_wantVsync);
}

void Renderer::SyncOverlayWindow(const GeneralConfig& general, bool interactWithMenu) {
    if (!m_hwnd)
        return;

    m_wantStreamProof = general.streamProof;
    m_wantVsync = general.overlayVsync;
    m_wantInteract = interactWithMenu;

    if (Splash::Covering()) {
        m_wantSplashChrome = true;
        m_wantFullscreen = false;
        m_wantSinglePc = false;
        m_holdChromeFrames = 8;
        return;
    }
    if (m_holdChromeFrames > 0) {
        --m_holdChromeFrames;
        m_wantSplashChrome = true;
        m_wantFullscreen = false;
        m_wantSinglePc = false;
        return;
    }

    m_wantSplashChrome = false;
    int fsW = general.gameScreenWidth;
    int fsH = general.gameScreenHeight;
    if (fsW <= 0)
        fsW = GetSystemMetrics(SM_CXSCREEN);
    if (fsH <= 0)
        fsH = GetSystemMetrics(SM_CYSCREEN);
    m_wantFullscreen = general.fullscreenOverlay;
    m_wantFsW = fsW;
    m_wantFsH = fsH;
    m_wantSinglePc = !general.fuserMode;

    if (!general.fullscreenOverlay) {
        RECT cr = {};
        if (GetClientRect(m_hwnd, &cr)) {
            const int cw = cr.right - cr.left;
            const int ch = cr.bottom - cr.top;
            if (cw >= 64 && ch >= 64 && (cw != m_windowedWidth || ch != m_windowedHeight)) {
                m_windowedWidth = cw;
                m_windowedHeight = ch;
            }
        }
    }
}

// =============================================================================
// Main loop
// =============================================================================

void Renderer::Run(std::function<void()> renderCallback) {
    MSG msg = {};
    while (m_running) {
        while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT) {
                m_running = false;
                return;
            }
        }

        ApplyPendingResize();
        CommitOverlayWindow();
        m_inFrame = true;
        BeginFrame();
        if (renderCallback) renderCallback();
        EndFrame();
        m_inFrame = false;
        ApplyPendingResize();
    }
}

// =============================================================================
// Cleanup
// =============================================================================

void Renderer::Shutdown() {
    if (!m_initialized) return;
    m_initialized = false;
    m_running = false;
    timeEndPeriod(1);

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDevice();

    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
    UnregisterClassW(m_wc.lpszClassName, m_wc.hInstance);
}

void Renderer::CleanupDevice() {
    if (m_rtv) { m_rtv->Release(); m_rtv = nullptr; }
    if (m_swapChain) { m_swapChain->Release(); m_swapChain = nullptr; }
    if (m_context) { m_context->Release(); m_context = nullptr; }
    if (m_device) { m_device->Release(); m_device = nullptr; }
}

void Renderer::SetStreamProof(bool enabled) {
    if (!m_hwnd) return;
    DWORD affinity = enabled ? WDA_EXCLUDEFROMCAPTURE : WDA_NONE;
    SetWindowDisplayAffinity(m_hwnd, affinity);
}

void Renderer::SetVsync(bool enabled) {
    m_vsync = enabled;
}

void Renderer::SetFrameCap(float fps) {
    m_frameCap = fps;
}

int Renderer::MonitorRefreshHz() const {
    DEVMODEW dm{};
    dm.dmSize = sizeof(dm);
    if (EnumDisplaySettingsW(nullptr, ENUM_CURRENT_SETTINGS, &dm) &&
        dm.dmDisplayFrequency >= 20 && dm.dmDisplayFrequency < 1000)
        return static_cast<int>(dm.dmDisplayFrequency);
    return 60;
}

void Renderer::WaitFrameCap() {
    const float monitor = static_cast<float>(MonitorRefreshHz());
    float cap = m_frameCap > 15.f ? m_frameCap : monitor;
    if (monitor >= 30.f)
        cap = (std::min)(cap, monitor);
    cap = (std::clamp)(cap, 20.f, 240.f);
    const auto period = std::chrono::duration_cast<std::chrono::steady_clock::duration>(
        std::chrono::duration<double>(1.0 / static_cast<double>(cap)));
    const auto now = std::chrono::steady_clock::now();
    if (m_nextFrameAt.time_since_epoch().count() == 0 || now > m_nextFrameAt + period * 2) {
        m_nextFrameAt = now + period;
        return;
    }
    if (now < m_nextFrameAt)
        std::this_thread::sleep_until(m_nextFrameAt);
    m_nextFrameAt += period;
}

// =============================================================================
// Window procedure
// =============================================================================

LRESULT CALLBACK Renderer::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp))
        return true;

    switch (msg) {
    case WM_SIZE:
        if (Renderer::Get().m_device && wp != SIZE_MINIMIZED) {
            auto& r = Renderer::Get();
            r.m_pendingW = LOWORD(lp);
            r.m_pendingH = HIWORD(lp);
            r.m_resizePending = true;
            if (!r.m_inFrame && !r.m_resizing)
                r.ApplyPendingResize();
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
