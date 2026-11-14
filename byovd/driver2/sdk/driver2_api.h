#pragma once
// driver2 usermode API — direct IOCTL access to mapped Driver2.sys (full Betest surface).
#include "../../include/betest_ioctl.h"
#include <cstddef>
#include <vector>
#include <cstring>

// Stomp mode: open the resident victim device (e.g. L"\\\\.\\Dam") instead of Betest pool names.
inline void Driver2SetDeviceName(const wchar_t* victimDosName)
{
    if (!victimDosName || !victimDosName[0]) {
        BetestSetDeviceOverride(nullptr);
        return;
    }
    char narrow[64]{};
    WideCharToMultiByte(CP_UTF8, 0, victimDosName, -1, narrow, sizeof(narrow), nullptr, nullptr);
    BetestSetDeviceOverride(narrow);
}

inline HANDLE Driver2OpenWithRetry(int attempts = 24, DWORD sleepMs = 250)
{
    for (int i = 0; i < attempts; i++) {
        HANDLE h = BetestOpenDevice();
        if (h != INVALID_HANDLE_VALUE) return h;
        if (sleepMs) Sleep(sleepMs);
    }
    return INVALID_HANDLE_VALUE;
}

inline HANDLE Driver2Open()
{
    return Driver2OpenWithRetry();
}

inline bool Driver2Auth(HANDLE h)
{
    return BetestSessionAuth(h);
}

inline bool Driver2ReadProcess(HANDLE h, uint64_t pid, uint64_t va, void* out, uint32_t size)
{
    return BetestReadProcess(h, pid, va, out, size);
}

inline bool Driver2WriteProcess(HANDLE h, uint64_t pid, uint64_t va, const void* data, uint32_t size)
{
    return BetestWriteProcess(h, pid, va, data, size);
}

inline bool Driver2ReadKernel(HANDLE h, uint64_t kva, void* out, uint32_t size)
{
    KREQ_READ req{};
    req.Address = kva;
    req.Size = size;
    req.Pad = 0;
    DWORD ret = 0;
    return DeviceIoControl(h, IOCTL_READ_KERNEL_MEM, &req, sizeof(req),
        out, size, &ret, nullptr) != FALSE && ret >= size;
}

inline bool Driver2AceStrengthen(HANDLE h, uint32_t features = ACE_STR_ALL,
    uint64_t targetPid = 0, uint64_t gamePid = 0)
{
    ACE_STRENGTHEN_REQ req{};
    req.Features = features;
    req.TargetPid = targetPid;
    req.GamePid = gamePid;
    uint8_t respBuf[320]{};
    DWORD ret = 0;
    return DeviceIoControl(h, IOCTL_ACE_STRENGTHEN, &req, sizeof(req), respBuf, sizeof(respBuf), &ret, nullptr) != FALSE
        && ret != 0;
}

inline bool Driver2AceDelta(HANDLE h, uint32_t features = ACE_DELTA_ALL, uint64_t targetPid = 0)
{
    ACE_DELTA_REQ req{};
    req.Features = features;
    req.TargetPid = targetPid;
    uint8_t respBuf[256]{};
    DWORD ret = 0;
    return DeviceIoControl(h, IOCTL_ACE_DELTA, &req, sizeof(req), respBuf, sizeof(respBuf), &ret, nullptr) != FALSE
        && ret != 0;
}

inline bool Driver2EacImprove(HANDLE h, uint32_t features = EAC_IMP_ALL,
    uint64_t targetPid = 0, uint64_t gamePid = 0)
{
    EAC_IMPROVE_REQ req{};
    req.Features = features;
    req.TargetPid = targetPid;
    req.GamePid = gamePid;
    uint8_t respBuf[320]{};
    DWORD ret = 0;
    return DeviceIoControl(h, IOCTL_EAC_IMPROVE, &req, sizeof(req), respBuf, sizeof(respBuf), &ret, nullptr) != FALSE
        && ret != 0;
}

inline bool Driver2EosEncPtr(HANDLE h, uint32_t features = EAC_ENC_PTR_ALL)
{
    ENC_PTR_REQ req{};
    req.Features = features;
    uint8_t respBuf[256]{};
    DWORD ret = 0;
    return DeviceIoControl(h, IOCTL_ENC_PTR, &req, sizeof(req), respBuf, sizeof(respBuf), &ret, nullptr) != FALSE
        && ret != 0;
}

inline bool Driver2BattleyeImprove(HANDLE h, uint32_t features = BE_ALL, uint64_t targetPid = 0)
{
    BATTLEYE_REQ req{};
    req.Features = features;
    req.Flags = 0;
    req.TargetPid = targetPid;
    uint8_t respBuf[320]{};
    DWORD ret = 0;
    if (!DeviceIoControl(h, IOCTL_BATTLEYE, &req, sizeof(req), respBuf, sizeof(respBuf), &ret, nullptr))
        return false;
    return ret != 0;
}

inline bool Driver2WriteKernel(HANDLE h, uint64_t kva, const void* data, uint32_t size)
{
    if (!data || !size || size > 4096) return false;
    const ULONG hdr = (ULONG)offsetof(KREQ_WRITE, Data);
    std::vector<uint8_t> buf(hdr + size);
    auto* req = reinterpret_cast<KREQ_WRITE*>(buf.data());
    req->Address = kva;
    req->Size = size;
    req->Pad = 0;
    memcpy(req->Data, data, size);
    DWORD ret = 0;
    return DeviceIoControl(h, IOCTL_WRITE_KERNEL_MEM, buf.data(), (DWORD)buf.size(),
        nullptr, 0, &ret, nullptr) != FALSE;
}
