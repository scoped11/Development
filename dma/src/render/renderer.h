#pragma once
#include <d3d11.h>
#include <dxgi.h>
#include <string>
#include <functional>
#include <chrono>

struct GeneralConfig;

struct RendererConfig {
    std::string title = "Rob Run DMA";
    int width = 1280;
    int height = 720;
    bool vsync = true;
    bool streamProof = false;
};

class Renderer {
public:
    static Renderer& Get() {
        static Renderer instance;
        return instance;
    }

    bool Init(const RendererConfig& config);
    void Show();
    void Shutdown();

    // Main render loop — calls renderCallback each frame
    void Run(std::function<void()> renderCallback);
    void RequestExit() { m_running = false; }
    bool IsRunning() const { return m_running; }

    // Frame helpers
    void BeginFrame();
    void EndFrame();

    // Overlay window: fuser capture vs single-PC transparent topmost
    void SyncOverlayWindow(const GeneralConfig& general, bool interactWithMenu);

    // Accessors
    HWND GetHWND() const { return m_hwnd; }
    ID3D11Device* GetDevice() const { return m_device; }
    ID3D11DeviceContext* GetContext() const { return m_context; }
    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }
    struct ImFont* EspFont() const;

    void SetStreamProof(bool enabled);
    void SetVsync(bool enabled);
    void SetFrameCap(float fps);

private:
    Renderer() = default;
    ~Renderer() { Shutdown(); }
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    bool CreateWindowAndDevice(const RendererConfig& config);
    bool InitImGui();
    void CleanupDevice();
    void ApplyFullscreen(bool enable, int width, int height);
    void ResizeClient(int width, int height);
    void ApplyPendingResize();
    void CommitOverlayWindow();
    void WaitFrameCap();
    int MonitorRefreshHz() const;

    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

    HWND m_hwnd = nullptr;
    WNDCLASSEXW m_wc = {};

    ID3D11Device* m_device = nullptr;
    ID3D11DeviceContext* m_context = nullptr;
    IDXGISwapChain* m_swapChain = nullptr;
    ID3D11RenderTargetView* m_rtv = nullptr;

    int m_width = 1280;
    int m_height = 720;
    int m_windowedWidth = 1280;
    int m_windowedHeight = 720;
    RECT m_windowedRect = {};
    bool m_vsync = true;
    bool m_running = false;
    bool m_initialized = false;
    bool m_fullscreenActive = false;
    bool m_singlePcOverlay = false;
    bool m_clearTransparent = false;
    bool m_streamProof = false;
    DWORD m_lastExStyle = 0;
    int m_lastFullscreenW = 0;
    int m_lastFullscreenH = 0;
    bool m_inFrame = false;
    bool m_resizing = false;
    bool m_resizePending = false;
    int m_pendingW = 0;
    int m_pendingH = 0;
    bool m_wantFullscreen = false;
    int m_wantFsW = 0;
    int m_wantFsH = 0;
    int m_wantFsX = 0;
    int m_wantFsY = 0;
    int m_overlayMonitor = 0;
    bool m_wantSplashChrome = true;
    bool m_wantSinglePc = false;
    bool m_wantInteract = true;
    bool m_wantStreamProof = false;
    bool m_wantVsync = true;
    bool m_splashChromeApplied = false;
    int m_holdChromeFrames = 0;
    struct ImFont* m_espFont = nullptr;
    float m_frameCap = 0.f;
    std::chrono::steady_clock::time_point m_nextFrameAt{};
};
