#pragma once
// Shared usermode contract for Betest after mapper_nal maps it.
// Must stay packed identically to driver/BetestDriver.c request structs.

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <winioctl.h>
#include <stdint.h>
#include <stdio.h>

#define IOCTL_SAFE_WRITE           CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_WRITE_DATA)
#define IOCTL_SAFE_READ            CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_READ_DATA)
#define IOCTL_READ_KERNEL_MEM      CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_READ_DATA)
#define IOCTL_WRITE_KERNEL_MEM     CTL_CODE(FILE_DEVICE_UNKNOWN, 0x803, METHOD_BUFFERED, FILE_WRITE_DATA)
#define IOCTL_ALLOC_KERNEL_MEM     CTL_CODE(FILE_DEVICE_UNKNOWN, 0x804, METHOD_BUFFERED, FILE_READ_DATA)
#define IOCTL_READ_PROCESS_MEM     CTL_CODE(FILE_DEVICE_UNKNOWN, 0x805, METHOD_BUFFERED, FILE_READ_DATA)
#define IOCTL_WRITE_PROCESS_MEM    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x806, METHOD_BUFFERED, FILE_WRITE_DATA)
#define IOCTL_SESSION_AUTH         CTL_CODE(FILE_DEVICE_UNKNOWN, 0x870, METHOD_BUFFERED, FILE_ANY_ACCESS)

#pragma pack(push, 1)
typedef struct _KREQ_READ {
    uint64_t Address;
    uint32_t Size;
    uint32_t Pad;
} KREQ_READ;

typedef struct _KREQ_WRITE {
    uint64_t Address;
    uint32_t Size;
    uint32_t Pad;
    uint8_t  Data[1];
} KREQ_WRITE;

typedef struct _PROC_READ_REQ {
    uint64_t ProcessId;
    uint64_t Address;
    uint32_t Size;
    uint32_t Pad;
} PROC_READ_REQ;

typedef struct _PROC_WRITE_REQ {
    uint64_t ProcessId;
    uint64_t Address;
    uint32_t Size;
    uint32_t Pad;
    uint8_t  Data[1];
} PROC_WRITE_REQ;
#pragma pack(pop)

static const char* const kBetestStaticDevices[] = {
    "\\\\.\\MsFlt0", "\\\\.\\WdFilter1", "\\\\.\\FltMgr2", "\\\\.\\NdProxy0",
    "\\\\.\\TdiFilter", "\\\\.\\MsSecFlt", "\\\\.\\WfpDrv0", "\\\\.\\NtFilter1",
};

static const char* const kBetestDynPrefixes[] = {
    "MsFlt", "WdFlt", "FltMgr", "NdPxy", "TdiFlt",
    "MsSecF", "WfpDr", "NtFlt", "ClfsF", "VhdFlt",
    "SrFlt", "FsDep", "PcwDrv", "MsIScsi", "VolSnap", "WmiLib",
};

#ifdef __cplusplus
#include <vector>
#include <cstring>

inline HANDLE BetestOpenDevice()
{
    static const char* const kVehicleDev[] = {
        "\\\\.\\RTCore64", "\\\\.\\Nal", "\\\\.\\CorsairLLAccess",
        "\\\\.\\TrixxSvc", "\\\\.\\ThrottleStop", "\\\\.\\SIVDRIVER",
    };
    for (auto name : kVehicleDev) {
        HANDLE h = CreateFileA(name, GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
        if (h != INVALID_HANDLE_VALUE) return h;
    }
    char buf[64];
    for (int p = 0; p < 16; p++) {
        for (int d = 0; d < 10; d++) {
            sprintf_s(buf, sizeof(buf), "\\\\.\\%s%d", kBetestDynPrefixes[p], d);
            HANDLE h = CreateFileA(buf, GENERIC_READ | GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
            if (h != INVALID_HANDLE_VALUE) return h;
        }
    }
    for (auto name : kBetestStaticDevices) {
        HANDLE h = CreateFileA(name, GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
        if (h != INVALID_HANDLE_VALUE) return h;
    }
    return INVALID_HANDLE_VALUE;
}

inline bool BetestSessionAuth(HANDLE h)
{
    ULONG64 challenge = GetTickCount64() ^ ((ULONG64)GetCurrentProcessId() << 32);
    if (challenge == 0) challenge = 1;
    ULONG64 token = 0;
    DWORD ret = 0;
    if (!DeviceIoControl(h, IOCTL_SESSION_AUTH, &challenge, sizeof(challenge),
            &token, sizeof(token), &ret, nullptr))
        return false;
    return ret == sizeof(ULONG64) && token != 0;
}

inline bool BetestReadProcess(HANDLE h, uint64_t pid, uint64_t addr, void* out, uint32_t size)
{
    PROC_READ_REQ req{};
    req.ProcessId = pid;
    req.Address = addr;
    req.Size = size;
    DWORD ret = 0;
    return DeviceIoControl(h, IOCTL_READ_PROCESS_MEM, &req, sizeof(req),
        out, size, &ret, nullptr) != FALSE && ret == size;
}

inline bool BetestWriteProcess(HANDLE h, uint64_t pid, uint64_t addr, const void* data, uint32_t size)
{
    if (!data || !size) return false;
    const ULONG hdr = (ULONG)FIELD_OFFSET(PROC_WRITE_REQ, Data);
    std::vector<uint8_t> buf(hdr + size);
    auto* req = reinterpret_cast<PROC_WRITE_REQ*>(buf.data());
    req->ProcessId = pid;
    req->Address = addr;
    req->Size = size;
    req->Pad = 0;
    memcpy(req->Data, data, size);
    DWORD ret = 0;
    return DeviceIoControl(h, IOCTL_WRITE_PROCESS_MEM, buf.data(), (DWORD)buf.size(),
        nullptr, 0, &ret, nullptr) != FALSE;
}
#endif
