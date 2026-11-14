#define NOMINMAX
#include <Windows.h>
#include <shellapi.h>

#include "core/memory.h"
#include "game/session.hpp"
#include "render/renderer.h"
#include "render/splash.h"
#include "render/menu.h"
#include "features/esp.h"
#include "features/radar.h"
#include "features/aimbot.h"
#include "features/hud.h"
#include "config/config.h"
#include "core/kmbox_usb.h"
#include "app.h"
#include "imgui.h"

#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <exception>
#include <cstring>
#include <cstdio>
#include <share.h>

std::atomic<bool> g_running = true;
static RobRunSession g_session;
static std::atomic<bool> g_fpgaReleased{false};
static HANDLE g_fpgaCloseDone = nullptr;
static thread_local const char* g_crashStage = "startup";

static void WriteCrashLog(const char* text) {
    if (!text)
        return;
    char path[MAX_PATH]{};
    GetModuleFileNameA(nullptr, path, MAX_PATH);
    if (char* slash = strrchr(path, '\\'))
        memcpy(slash + 1, "robrun-crash.log", 17);
    HANDLE h = CreateFileA(path, FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        return;
    DWORD written = 0;
    WriteFile(h, text, static_cast<DWORD>(strlen(text)), &written, nullptr);
    CloseHandle(h);
}

static DWORD WINAPI CloseFpgaThread(LPVOID) {
    Memory::Get().Close();
    if (g_fpgaCloseDone)
        SetEvent(g_fpgaCloseDone);
    return 0;
}

void ReleaseFpgaNow() {
    if (g_fpgaReleased.exchange(true))
        return;
    g_running = false;
    g_fpgaCloseDone = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    HANDLE thread = CreateThread(nullptr, 0, CloseFpgaThread, nullptr, 0, nullptr);
    if (thread && g_fpgaCloseDone)
        WaitForSingleObject(g_fpgaCloseDone, 1500);
    if (thread)
        CloseHandle(thread);
}

void RequestUninjectAndDisconnect() {
    Config::Save();
    g_running = false;
    KmboxUsb::Get().ClearMoves();
    KmboxUsb::Get().Disconnect();
    g_session.SetMemoryAimTarget(false, {}, {}, 0.f);
    g_session.Shutdown();
    ReleaseFpgaNow();
    Renderer::Get().RequestExit();
    std::cout << "[*] Uninject: overlay exit, FPGA/VMM disconnected\n" << std::flush;
}

static bool IsFatalException(DWORD code) {
    switch (code) {
    case EXCEPTION_ACCESS_VIOLATION:
    case EXCEPTION_STACK_OVERFLOW:
    case EXCEPTION_ILLEGAL_INSTRUCTION:
    case EXCEPTION_PRIV_INSTRUCTION:
    case EXCEPTION_IN_PAGE_ERROR:
    case EXCEPTION_NONCONTINUABLE_EXCEPTION:
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:
    case EXCEPTION_DATATYPE_MISALIGNMENT:
        return true;
    default:
        return false;
    }
}

LONG WINAPI CrashVeh(EXCEPTION_POINTERS* info) {
    const DWORD code = info && info->ExceptionRecord
        ? info->ExceptionRecord->ExceptionCode : 0;
    if (code == EXCEPTION_STACK_OVERFLOW) {
        g_running = false;
        TerminateProcess(GetCurrentProcess(), 1);
        return EXCEPTION_CONTINUE_SEARCH;
    }
    if (IsFatalException(code))
        ReleaseFpgaNow();
    return EXCEPTION_CONTINUE_SEARCH;
}

LONG WINAPI CrashFilter(EXCEPTION_POINTERS* info) {
    ReleaseFpgaNow();
    const DWORD code = info && info->ExceptionRecord
        ? info->ExceptionRecord->ExceptionCode : 0;
    const uintptr_t address = info && info->ExceptionRecord
        ? reinterpret_cast<uintptr_t>(info->ExceptionRecord->ExceptionAddress) : 0;
    const uintptr_t base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    char line[512];
    _snprintf_s(line, _TRUNCATE,
        "\n[-] UNHANDLED EXCEPTION code=0x%lx address=0x%llx exe+rva=0x%llx stage=%s\n",
        code,
        static_cast<unsigned long long>(address),
        static_cast<unsigned long long>(address >= base ? address - base : 0),
        g_crashStage);
    std::cerr << line << std::flush;
    WriteCrashLog(line);
    TerminateProcess(GetCurrentProcess(), 1);
    return EXCEPTION_EXECUTE_HANDLER;
}

[[noreturn]] void TerminateHandler() {
    ReleaseFpgaNow();
    char line[256];
    _snprintf_s(line, _TRUNCATE, "\n[-] C++ TERMINATE stage=%s\n", g_crashStage);
    std::cerr << line << std::flush;
    WriteCrashLog(line);
    TerminateProcess(GetCurrentProcess(), 3);
    std::abort();
}

void SignalHandler(int signal) {
    ReleaseFpgaNow();
    char line[256];
    _snprintf_s(line, _TRUNCATE, "\n[-] FATAL SIGNAL %d stage=%s\n", signal, g_crashStage);
    std::cerr << line << std::flush;
    WriteCrashLog(line);
    TerminateProcess(GetCurrentProcess(), static_cast<UINT>(signal));
}

BOOL WINAPI ConsoleCtrlHandler(DWORD type) {
    ReleaseFpgaNow();
    Renderer::Get().RequestExit();
    if (type == CTRL_CLOSE_EVENT || type == CTRL_LOGOFF_EVENT || type == CTRL_SHUTDOWN_EVENT)
        Sleep(400);
    return TRUE;
}

static int RunFpgaGuard(DWORD parentPid) {
    HANDLE parent = OpenProcess(SYNCHRONIZE | PROCESS_TERMINATE, FALSE, parentPid);
    if (!parent)
        return 0;
    WaitForSingleObject(parent, INFINITE);
    CloseHandle(parent);
    return 0;
}

static void SpawnFpgaGuard() {
    wchar_t exe[MAX_PATH]{};
    if (!GetModuleFileNameW(nullptr, exe, MAX_PATH))
        return;
    wchar_t cmd[MAX_PATH + 64]{};
    _snwprintf_s(cmd, _TRUNCATE, L"\"%s\" --fpga-guard %lu", exe, GetCurrentProcessId());
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(exe, cmd, nullptr, nullptr, FALSE,
                        CREATE_NO_WINDOW | DETACHED_PROCESS, nullptr, nullptr, &si, &pi))
        return;
    Memory::Get().SetWatchdogPid(pi.dwProcessId);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
}

void GlobalsWorker() {
    while (g_running) {
        if (Memory::Get().IsConnected() && Memory::Get().MotionBurstDepth() == 0) {
            g_crashStage = "CacheGlobals";
            g_session.CacheGlobals();
            g_crashStage = "GlobalsWorker sleep";
        }
        int sleepMs = Config::Get().general.updateRateGlobals;
        if (!g_session.IsInGame())
            sleepMs = 16;
        std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
    }
}

void ActorWorker() {
    while (g_running) {
        auto& mem = Memory::Get();
        int rateMs = Config::Get().general.updateRateActors;
        if (rateMs < 8) rateMs = 8;
        if (rateMs > 2000) rateMs = 2000;
        if (mem.IsConnected() && g_session.IsInGame() && mem.MotionBurstDepth() == 0) {
            g_crashStage = "CacheActors";
            g_session.CacheActors();
            g_crashStage = "ActorWorker sleep";
            rateMs = Config::Get().general.updateRateActors;
        } else if (mem.IsConnected() && !g_session.IsInGame()) {
            rateMs = Config::Get().general.updateRateActors;
        }
        if (rateMs < 8) rateMs = 8;
        std::this_thread::sleep_for(std::chrono::milliseconds(rateMs));
    }
}

void PositionWorker() {
    Memory::Get().SetMotionThread(true);
    while (g_running) {
        int& rateMs = Config::Get().general.updateRatePositions;
        if (rateMs < 8) rateMs = 8;
        if (rateMs > 50) rateMs = 50;
        const auto period = std::chrono::milliseconds(rateMs);
        const auto start = std::chrono::steady_clock::now();
        if (Memory::Get().IsConnected() && g_session.IsInGame()) {
            g_crashStage = "CachePositions";
            Memory::Get().BeginMotionBurst();
            g_session.CachePositions();
            Memory::Get().EndMotionBurst();
            g_session.WriteFreecam();
            g_session.ServiceMemoryAim();
            g_session.ApplyGun();
            g_crashStage = "PositionWorker sleep";
        }
        const auto elapsed = std::chrono::steady_clock::now() - start;
        if (elapsed < period)
            std::this_thread::sleep_for(period - elapsed);
    }
}

void ConnectionWorker() {
    while (g_running) {
        auto& mem = Memory::Get();
        auto& cfg = Config::Get().general;

        if (!mem.IsConnected()) {
            g_crashStage = "Memory connection";
            Splash::MarkBusy();
            Splash::SetStage("Connecting to DMA", 0.18f);
            std::cout << "[*] Searching for " << cfg.processName << "...\n";

            if (!mem.Init()) {
                Splash::SetStage("DMA card not found — retrying", 0.18f);
                std::this_thread::sleep_for(std::chrono::seconds(2));
                continue;
            }

            Splash::SetStage("Searching for ROB RUN.exe", 0.42f);
            if (!mem.AttachToProcess(cfg.processName)) {
                Splash::SetStage("Waiting for ROB RUN.exe", 0.44f);
                std::this_thread::sleep_for(std::chrono::seconds(1));
                continue;
            }

            Splash::SetStage("Attached — mapping GameAssembly", 0.70f);
            if (g_session.Initialize(cfg.processName)) {
                std::cout << "[+] Game session ready — starting data workers\n";
            }
        } else if (!mem.EnsureIl2CppReady()) {
            Splash::SetStage("Pinning GameAssembly (TypeInfo)", 0.82f);
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            continue;
        }

        if (!g_session.ReadyForOverlay()) {
            Splash::SetStage("Pinning Game chain", 0.92f);
            g_session.CacheGlobals();
            continue;
        }

        Splash::SetStage("Ready", 1.f);
        Splash::MarkReady();
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow)
{
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv && argc >= 3 && wcscmp(argv[1], L"--fpga-guard") == 0) {
        const DWORD parentPid = wcstoul(argv[2], nullptr, 10);
        LocalFree(argv);
        return RunFpgaGuard(parentPid);
    }
    if (argv)
        LocalFree(argv);

    char dbgConsole[8] = {};
    const DWORD dbgLen = GetEnvironmentVariableA("ROBRUN_DEBUG_CONSOLE", dbgConsole, sizeof(dbgConsole));
    const bool showConsole = dbgLen > 0 && dbgConsole[0] != '0';
    char logPath[MAX_PATH]{};
    GetModuleFileNameA(nullptr, logPath, MAX_PATH);
    if (char* slash = strrchr(logPath, '\\'))
        memcpy(slash + 1, "robrun.log", 11);
    FILE* logFile = _fsopen(logPath, "w", _SH_DENYNO);
    if (logFile) {
        *stdout = *logFile;
        *stderr = *logFile;
        setvbuf(stdout, nullptr, _IONBF, 0);
        setvbuf(stderr, nullptr, _IONBF, 0);
    }
    if (showConsole)
        AllocConsole();
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    SetUnhandledExceptionFilter(CrashFilter);
    AddVectoredExceptionHandler(1, CrashVeh);
    SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);
    std::atexit([] { ReleaseFpgaNow(); });
    std::at_quick_exit([] { ReleaseFpgaNow(); });
    std::set_terminate(TerminateHandler);
    std::signal(SIGABRT, SignalHandler);
    std::signal(SIGSEGV, SignalHandler);

    Memory::Get().Log("========================================\n  ROB RUN DMA (build 2026-11-14)\n========================================\n");
    Memory::Get().ReclaimDevice();
    SpawnFpgaGuard();

    std::cout << "========================================\n";
    std::cout << "  ROB RUN DMA\n";
    std::cout << "========================================\n\n";

    Config::Load();
    Splash::SetStage("Starting overlay", 0.05f);

    RendererConfig renderCfg;
    renderCfg.title = "ROB RUN DMA";
    renderCfg.width = 1280;
    renderCfg.height = 800;
    renderCfg.vsync = Config::Get().general.overlayVsync;
    renderCfg.streamProof = Config::Get().general.streamProof;

    if (!Renderer::Get().Init(renderCfg)) {
        std::cerr << "[-] Renderer init failed\n";
        ReleaseFpgaNow();
        return 1;
    }
    Splash::Init(Renderer::Get().GetDevice(), Renderer::Get().GetContext());
    Renderer::Get().SyncOverlayWindow(Config::Get().general, true);
    Renderer::Get().Show();

    std::thread connThread(ConnectionWorker);
    std::thread globalsThread(GlobalsWorker);
    std::thread actorThread(ActorWorker);
    std::thread posThread(PositionWorker);
    std::cout << "[+] Background workers started\n";
    std::cout << "[*] Waiting for game...\n\n";

    Renderer::Get().Run([]() {
        g_crashStage = "Render snapshot";
        auto& cfg = Config::Get();

        static bool f2Was = false;
        const bool f2Down = (GetAsyncKeyState(VK_F2) & 0x8000) != 0;
        if (f2Down && !f2Was)
            cfg.assist.aim.enabled = !cfg.assist.aim.enabled;
        f2Was = f2Down;
        static bool f6Was = false;
        const bool f6Down = (GetAsyncKeyState(VK_F6) & 0x8000) != 0;
        if (f6Down && !f6Was)
            cfg.general.freeCam = !cfg.general.freeCam;
        f6Was = f6Down;

        Splash::Tick();
        const bool splashUp = Splash::Covering();
        static bool menuOpenedAfterSplash = false;
        if (splashUp) {
            menuOpenedAfterSplash = false;
            g_crashStage = "Splash render";
            Splash::Render();
            Renderer::Get().SyncOverlayWindow(cfg.general, true);
            g_crashStage = "Renderer idle";
            return;
        }

        g_crashStage = "Render snapshot";
        std::vector<GameEntity> entities;
        GameEntity localPlayer;
        CameraData camera;
        g_session.CopyRenderState(entities, localPlayer, camera);
        auto& assist = cfg.assist;
        static uint32_t s_kmboxRetryMs = 0;
        static bool s_kmboxProfileApplied = false;
        auto& kmbox = KmboxUsb::Get();
        if (!kmbox.IsConnected()) {
            s_kmboxProfileApplied = false;
            if (assist.autoConnect) {
                const uint32_t now = GetTickCount();
                if (now - s_kmboxRetryMs >= 3000) {
                    s_kmboxRetryMs = now;
                    if (kmbox.Connect(assist.comPort))
                        kmbox.SetAcProfileName(assist.acProfile);
                }
            }
        } else if (!s_kmboxProfileApplied) {
            kmbox.SetAcProfileName(assist.acProfile);
            s_kmboxProfileApplied = true;
        }
        kmbox.ServiceOnce();

        const int overlayW = Renderer::Get().GetWidth();
        const int overlayH = Renderer::Get().GetHeight();
        int gameW = cfg.general.gameScreenWidth;
        int gameH = cfg.general.gameScreenHeight;
        if (gameW <= 0 || gameH <= 0) {
            gameW = g_session.DetectedWidth();
            gameH = g_session.DetectedHeight();
        }
        float scaleX = 1.f;
        float scaleY = 1.f;
        if (gameW > 0 && overlayW > 0)
            scaleX = static_cast<float>(gameW) / static_cast<float>(overlayW);
        if (gameH > 0 && overlayH > 0)
            scaleY = static_cast<float>(gameH) / static_cast<float>(overlayH);
        camera.gameWidth = gameW;
        camera.gameHeight = gameH;
        if (!menuOpenedAfterSplash) {
            Menu::Open();
            menuOpenedAfterSplash = true;
        }

        g_crashStage = "Menu render";
        Menu::Render(entities, localPlayer, camera);
        g_session.UpdateFreecam(ImGui::GetIO().DeltaTime, Menu::IsOpen());
        const bool menuInteract =
            Menu::IsOpen() || ImGui::GetIO().WantCaptureMouse || ImGui::GetIO().WantCaptureKeyboard;
        Renderer::Get().SyncOverlayWindow(cfg.general, menuInteract);
        g_crashStage = "Radar render";
        Radar::Render(entities, localPlayer, camera, cfg.radar);
        Hud::Render(entities, localPlayer, camera, cfg);

        Aimbot::RenderFov(assist.aim, overlayW, overlayH);
        if (assist.aim.enabled && camera.valid)
            Aimbot::Tick(entities, localPlayer, camera, assist.aim, overlayW, overlayH, scaleX, scaleY, g_session);

        if (cfg.esp.enabled && camera.valid) {
            g_crashStage = "ESP render";
            ESP::Render(entities, localPlayer, camera, cfg.esp, overlayW, overlayH);
        }
        g_crashStage = "Renderer idle";
    });

    g_running = false;
    std::cout << "[*] Renderer stopped; waiting for workers\n" << std::flush;
    connThread.join();
    globalsThread.join();
    actorThread.join();
    posThread.join();

    g_session.SetMemoryAimTarget(false, {}, {}, 0.f);
    Aimbot::Reset();
    g_session.Shutdown();
    ReleaseFpgaNow();
    Splash::Shutdown();
    Renderer::Get().Shutdown();

    Config::Save();
    FreeConsole();
    return 0;
}
