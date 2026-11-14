#pragma once
#include <vmmdll.h>
#include <cstdint>
#include <vector>

void RobRunDmaEnter();
void RobRunDmaLeave();

struct DmaScope {
    DmaScope() { RobRunDmaEnter(); }
    ~DmaScope() { RobRunDmaLeave(); }
    DmaScope(const DmaScope&) = delete;
    DmaScope& operator=(const DmaScope&) = delete;
};

class ScatterHandle {
public:
    ScatterHandle() = default;

    ScatterHandle(VMMDLL_SCATTER_HANDLE handle, VMM_HANDLE vmm, DWORD pid)
        : m_handle(handle), m_vmm(vmm), m_pid(pid) {}

    ~ScatterHandle() { Close(); }

    ScatterHandle(const ScatterHandle&) = delete;
    ScatterHandle& operator=(const ScatterHandle&) = delete;

    ScatterHandle(ScatterHandle&& other) noexcept
        : m_handle(other.m_handle), m_vmm(other.m_vmm), m_pid(other.m_pid),
          m_queued(other.m_queued), m_maxQueued(other.m_maxQueued)
    {
        other.m_handle = nullptr;
        other.m_queued = 0;
    }

    ScatterHandle& operator=(ScatterHandle&& other) noexcept {
        if (this != &other) {
            Close();
            m_handle = other.m_handle;
            m_vmm = other.m_vmm;
            m_pid = other.m_pid;
            m_queued = other.m_queued;
            m_maxQueued = other.m_maxQueued;
            other.m_handle = nullptr;
            other.m_queued = 0;
        }
        return *this;
    }

    bool IsValid() const { return m_handle != nullptr; }

    void SetMaxQueued(int maxQueued) {
        if (maxQueued < 32) maxQueued = 32;
        if (maxQueued > 4096) maxQueued = 4096;
        m_maxQueued = maxQueued;
    }

    bool Prepare(uintptr_t address, void* buffer, size_t size) {
        if (!m_handle || !buffer || size == 0) return false;
        if (!VMMDLL_Scatter_PrepareEx(m_handle, address,
                static_cast<DWORD>(size), static_cast<PBYTE>(buffer), nullptr))
            return false;
        ++m_queued;
        if (m_queued >= m_maxQueued)
            return FlushReads();
        return true;
    }

    template<typename T>
    bool Prepare(uintptr_t address, T* out) {
        return Prepare(address, out, sizeof(T));
    }

    bool PrepareWrite(uintptr_t address, void* buffer, size_t size) {
        if (!m_handle || !buffer || size == 0) return false;
        VMMDLL_Scatter_PrepareWrite(m_handle, address,
            static_cast<PBYTE>(buffer), static_cast<DWORD>(size));
        ++m_queued;
        return true;
    }

    template<typename T>
    bool PrepareWrite(uintptr_t address, T* value) {
        return PrepareWrite(address, value, sizeof(T));
    }

    bool FlushReads() {
        if (!m_handle || m_queued == 0)
            return true;
        DmaScope lock;
        const bool ok = VMMDLL_Scatter_ExecuteRead(m_handle);
        VMMDLL_Scatter_Clear(m_handle, m_pid, VMMDLL_FLAG_NOCACHE);
        m_queued = 0;
        return ok;
    }

    bool Execute() { return FlushReads(); }

    bool ExecuteWrite() {
        if (!m_handle) return false;
        DmaScope lock;
        const bool ok = VMMDLL_Scatter_Execute(m_handle);
        VMMDLL_Scatter_Clear(m_handle, m_pid, VMMDLL_FLAG_NOCACHE);
        m_queued = 0;
        return ok;
    }

    void Clear() {
        if (m_handle) {
            VMMDLL_Scatter_Clear(m_handle, m_pid, VMMDLL_FLAG_NOCACHE);
            m_queued = 0;
        }
    }

    void Close() {
        if (m_handle) {
            VMMDLL_Scatter_CloseHandle(m_handle);
            m_handle = nullptr;
            m_queued = 0;
        }
    }

    VMMDLL_SCATTER_HANDLE Get() const { return m_handle; }
    int Queued() const { return m_queued; }

private:
    VMMDLL_SCATTER_HANDLE m_handle = nullptr;
    VMM_HANDLE m_vmm = nullptr;
    DWORD m_pid = 0;
    int m_queued = 0;
    int m_maxQueued = 512;
};
