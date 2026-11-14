#pragma once
#include <string>
#include <cstdint>
#include <chrono>
#include <mutex>
#include <atomic>
#include <vmmdll.h>
#include <leechcore.h>
#include "scatter.h"

class Memory {
public:
    static Memory& Get() {
        static Memory instance;
        return instance;
    }

    // Lifecycle
    bool Init();
    bool AttachToProcess(const std::string& processName);
    void Close();
    // Kill leftover overlay processes that still own the FPGA, then wait
    // for the USB/FTDI handle to drop. Safe to call from startup and crash.
    void ReclaimDevice();
    void SetWatchdogPid(DWORD pid);
    bool IsConnected() const { return m_connected; }

    // Process info
    DWORD GetPID() const { return m_pid; }
    uintptr_t GetBaseAddress() const { return m_baseAddress; }
    size_t GetBaseSize() const { return m_baseSize; }
    uintptr_t GetModuleBase(const std::string& moduleName);
    size_t GetModuleSize(const std::string& moduleName);
    // SizeOfImage from the mapped PE header (more reliable than VMM cbImageSize).
    size_t ReadPeImageSize(uintptr_t moduleBase);
    void RefreshProcessModuleMap();
    // CR3 / pid refresh until GameAssembly.dll appears in the module map (ESP gate).
    bool EnsureIl2CppReady();
    uintptr_t GameAssemblyBase() const { return m_gameAssemblyBase; }
    size_t GameAssemblySize() const { return m_gameAssemblySize; }
    uintptr_t UnityPlayerBase() const { return m_unityPlayerBase; }
    size_t UnityPlayerSize() const { return m_unityPlayerSize; }
    bool EnsureUnityPlayer();

    // Single reads. Read() zeros the value when DMA fails — that is not a
    // successful zero. Use ReadOk when a failed page must not look like 0.
    template<typename T>
    T Read(uintptr_t address) {
        T value{};
        ReadRaw(address, &value, sizeof(T));
        return value;
    }

    template<typename T>
    bool ReadOk(uintptr_t address, T& out) {
        T value{};
        if (!ReadRaw(address, &value, sizeof(T)))
            return false;
        out = value;
        return true;
    }

    bool ReadRaw(uintptr_t address, void* buffer, size_t size);
    std::string ReadString(uintptr_t address, size_t maxLen = 256);
    std::wstring ReadWString(uintptr_t address, size_t maxLen = 256);

    // Unreal FString: TArray<TCHAR> (pointer to data at +0x0, count at +0x8)
    std::string ReadFString(uintptr_t address);

    // Single writes
    template<typename T>
    bool Write(uintptr_t address, T value) {
        return WriteRaw(address, &value, sizeof(T));
    }

    bool WriteRaw(uintptr_t address, void* buffer, size_t size);

    // Scatter operations (batch multiple reads into one DMA transaction)
    ScatterHandle CreateScatterHandle();
    // Reuses one handle per thread (Clear between batches) to avoid init churn.
    ScatterHandle& BorrowScatter();

    // Pointer chain resolution: reads each pointer in sequence
    // e.g., ResolvePointerChain(base, {0x30, 0x98, 0x10}) reads:
    //   base+0x30 -> ptr1, ptr1+0x98 -> ptr2, ptr2+0x10 -> result
    uintptr_t ResolvePointerChain(uintptr_t base, const std::vector<uintptr_t>& offsets);

    // Signature scanning within a memory region
    uintptr_t FindSignature(const char* pattern, uintptr_t regionStart, size_t regionSize);

    // Search the target process for an exact byte pattern. align is the
    // byte alignment (8 for a pointer). Hits are virtual addresses.
    // Search() is not used on the attach path (it used to walk all of user VA).
    std::vector<uintptr_t> Search(const void* pattern, size_t size, size_t align, size_t maxResults);
    std::vector<uintptr_t> SearchRange(const void* pattern, size_t size, size_t align, size_t maxResults,
                                       uintptr_t vaMin, uintptr_t vaMax, int timeoutMs = 16);
    // Private/heap VADs only. Unity GC lives here; GameAssembly image is skipped.
    // Chunked FPGA reads with a wall-clock deadline — never VMMDLL_MemSearch
    // (a single 100MB+ region hangs the FPGA and SNAP jumps to 1000ms+).
    std::vector<uintptr_t> SearchHeap(const void* pattern, size_t size, size_t align, size_t maxResults,
                                      bool gcOnly = true);

    // CR3/DTB fix for anti-cheat bypass
    bool FixCr3();

    // Physical memory map dump (speeds up subsequent inits)
    bool DumpMemoryMap(bool forceRefresh = false);

    // Unity writes the game window size into the game PC registry.
    bool ReadUnityResolution(int& width, int& height);

    // The position and render threads call this. Discovery waits while they
    // have the FPGA, so a mouse flick is not stuck behind an actor walk.
    void SetMotionThread(bool motion);
    void BeginMotionBurst();
    void EndMotionBurst();
    void BeginDiscoveryBurst();
    void EndDiscoveryBurst();
    int MotionBurstDepth() const;
    void Log(const char* fmt, ...);

private:
    Memory() = default;
    ~Memory() { Close(); }
    Memory(const Memory&) = delete;
    Memory& operator=(const Memory&) = delete;

    bool ConfigureFPGA();
    bool FindProcess(const std::string& processName);
    bool DiscoverGameAssembly();
    bool DiscoverUnityPlayer();
    void PublishGameAssembly(uintptr_t base, size_t size);
    void PublishUnityPlayer(uintptr_t base, size_t size);
    std::vector<uint8_t> PatternToBytes(const char* pattern);

    VMM_HANDLE m_vmm = nullptr;
    DWORD m_pid = 0;
    uintptr_t m_baseAddress = 0;
    size_t m_baseSize = 0;
    std::string m_processName;
    std::atomic<bool> m_connected = false;
    std::mutex m_mutex;
    std::chrono::steady_clock::time_point m_lastModuleMapRefresh{};
    uintptr_t m_gameAssemblyBase = 0;
    size_t m_gameAssemblySize = 0;
    uintptr_t m_unityPlayerBase = 0;
    size_t m_unityPlayerSize = 0;
};
