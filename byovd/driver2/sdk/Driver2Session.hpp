#pragma once
// Cheat-dev session wrapper — ACE/EAC/BE agnostic process I/O.
#include "driver2_api.h"
#include <cstdint>

class Driver2Session {
public:
    bool ConnectQuick()
    {
        if (handle_ != INVALID_HANDLE_VALUE)
            return true;
        handle_ = Driver2OpenWithRetry(2, 0);
        if (handle_ == INVALID_HANDLE_VALUE)
            return false;
        if (!Driver2Auth(handle_)) {
            CloseHandle(handle_);
            handle_ = INVALID_HANDLE_VALUE;
            return false;
        }
        return true;
    }

    bool Connect()
    {
        if (handle_ != INVALID_HANDLE_VALUE) return true;
        handle_ = Driver2OpenWithRetry(32, 250);
        if (handle_ == INVALID_HANDLE_VALUE) return false;
        if (!Driver2Auth(handle_)) {
            CloseHandle(handle_);
            handle_ = INVALID_HANDLE_VALUE;
            return false;
        }
        return true;
    }

    void Disconnect()
    {
        if (handle_ != INVALID_HANDLE_VALUE) {
            CloseHandle(handle_);
            handle_ = INVALID_HANDLE_VALUE;
        }
    }

    bool ReadProcess(uint64_t pid, uint64_t va, void* out, uint32_t size)
    {
        return handle_ != INVALID_HANDLE_VALUE && Driver2ReadProcess(handle_, pid, va, out, size);
    }

    bool WriteProcess(uint64_t pid, uint64_t va, const void* data, uint32_t size)
    {
        return handle_ != INVALID_HANDLE_VALUE && Driver2WriteProcess(handle_, pid, va, data, size);
    }

    bool ReadKernel(uint64_t kva, void* out, uint32_t size)
    {
        return handle_ != INVALID_HANDLE_VALUE && Driver2ReadKernel(handle_, kva, out, size);
    }

    bool ProbeSelfMarker()
    {
        volatile uint64_t m = 0x1122334455667788ULL;
        uint64_t got = 0;
        if (!ReadProcess(GetCurrentProcessId(), (uint64_t)(uintptr_t)&m, &got, 8)) return false;
        return got == m;
    }

    ~Driver2Session() { Disconnect(); }

private:
    HANDLE handle_ = INVALID_HANDLE_VALUE;
};
