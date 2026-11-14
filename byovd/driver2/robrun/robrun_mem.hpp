#pragma once
#include "../sdk/Driver2Session.hpp"
#include <cstdint>
#include <cstring>

inline bool IsPtr(uintptr_t p)
{
    return p >= 0x10000ULL && p < 0x00007FFFFFFFFFFFULL;
}

class RobRunMem {
public:
    explicit RobRunMem(uint64_t pid) : pid_(pid) {}

    bool Connect()
    {
        return drv_.Connect() && drv_.ProbeSelfMarker();
    }

    template<typename T>
    bool ReadOk(uintptr_t va, T& out)
    {
        return drv_.ReadProcess(pid_, va, &out, sizeof(T));
    }

    template<typename T>
    T Read(uintptr_t va)
    {
        T v{};
        drv_.ReadProcess(pid_, va, &v, sizeof(T));
        return v;
    }

    bool ReadRaw(uintptr_t va, void* out, size_t size)
    {
        return drv_.ReadProcess(pid_, va, out, static_cast<uint32_t>(size));
    }

    uint64_t pid() const { return pid_; }

private:
    Driver2Session drv_;
    uint64_t pid_ = 0;
};
