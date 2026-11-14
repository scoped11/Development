// VehiclePhysProbe.cpp — minimal VM proof for phys-map IOCTL vehicles (no mapper_nal).
// Lab only. Usage: VehiclePhysProbe.exe corsair|msio|rtcore [path\to\driver.sys]
#include <Windows.h>
#include <cstdio>
#include <cstdint>

static constexpr DWORD IOCTL_CORSAIR_MAP = 0x225374;
static constexpr DWORD IOCTL_CORSAIR_UNMAP = 0x229378;
static constexpr DWORD IOCTL_MSIO_MAP = 0x80102040;
static constexpr DWORD IOCTL_MSIO_UNMAP = 0x80102044;
static constexpr DWORD IOCTL_MYPORT_READ = 0x9C406680;
static constexpr DWORD IOCTL_MYPORT_WRITE = 0x9C40A684;

#pragma pack(push, 1)
struct MapParams {
    LARGE_INTEGER base;
    ULONG size;
};
#pragma pack(pop)

#pragma pack(push, 8)
struct MsIoMapPkt {
    ULONG64 size;
    ULONG64 phys;
    HANDLE section;
    PVOID linear;
    PVOID object;
};
#pragma pack(pop)

static bool LoadCorsair(const char* sysPath) {
    SC_HANDLE scm = OpenSCManagerA(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
    if (!scm) return false;
    const char* tag = "CorsairLLAccess";
    SC_HANDLE svc = OpenServiceA(scm, tag, SERVICE_ALL_ACCESS);
    if (svc) {
        SERVICE_STATUS ss{};
        ControlService(svc, SERVICE_CONTROL_STOP, &ss);
        DeleteService(svc);
        CloseServiceHandle(svc);
        Sleep(1500);
    }
    char sysFull[MAX_PATH];
    GetFullPathNameA(sysPath, MAX_PATH, sysFull, nullptr);
    svc = CreateServiceA(scm, tag, tag, SERVICE_ALL_ACCESS, SERVICE_KERNEL_DRIVER,
        SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, sysFull, nullptr, nullptr, nullptr, nullptr, nullptr);
    if (!svc) {
        svc = OpenServiceA(scm, tag, SERVICE_ALL_ACCESS);
    }
    if (!svc) {
        CloseServiceHandle(scm);
        return false;
    }
    if (!StartServiceA(svc, 0, nullptr) && GetLastError() != 1056) {
        CloseServiceHandle(svc);
        CloseServiceHandle(scm);
        return false;
    }
    CloseServiceHandle(svc);
    CloseServiceHandle(scm);
  Sleep(1000);
    return true;
}

static void* MapPhys(HANDLE h, uint64_t pa, uint32_t size) {
    MapParams p{};
    p.base.QuadPart = (LONGLONG)pa;
    p.size = size;
    DWORD got = 0;
    if (!DeviceIoControl(h, IOCTL_CORSAIR_MAP, &p, sizeof(p), &p, sizeof(p), &got, nullptr))
        return nullptr;
    return (void*)(uintptr_t)p.base.QuadPart;
}

static void UnmapCorsair(HANDLE h, void* va) {
    DWORD got = 0;
    DeviceIoControl(h, IOCTL_CORSAIR_UNMAP, &va, sizeof(va), nullptr, 0, &got, nullptr);
}

static bool MapMsIo(HANDLE h, uint64_t pa, ULONG64 span, MsIoMapPkt& pkt) {
    pkt = {};
    pkt.size = span;
    pkt.phys = pa;
    DWORD got = 0;
    if (!DeviceIoControl(h, IOCTL_MSIO_MAP, &pkt, sizeof(pkt), &pkt, sizeof(pkt), &got, nullptr))
        return false;
    return pkt.linear != nullptr;
}

static void UnmapMsIo(HANDLE h, const MsIoMapPkt& pkt) {
    MsIoMapPkt u = pkt;
    DWORD got = 0;
    DeviceIoControl(h, IOCTL_MSIO_UNMAP, &u, sizeof(u), &u, sizeof(u), &got, nullptr);
}

static bool LoadMsIo(const char* sysPath) {
    SC_HANDLE scm = OpenSCManagerA(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
    if (!scm) return false;
    const char* tag = "MsIo";
    SC_HANDLE svc = OpenServiceA(scm, tag, SERVICE_ALL_ACCESS);
    if (svc) {
        SERVICE_STATUS ss{};
        ControlService(svc, SERVICE_CONTROL_STOP, &ss);
        DeleteService(svc);
        CloseServiceHandle(svc);
        Sleep(1500);
    }
    char sysFull[MAX_PATH];
    GetFullPathNameA(sysPath, MAX_PATH, sysFull, nullptr);
    svc = CreateServiceA(scm, tag, tag, SERVICE_ALL_ACCESS, SERVICE_KERNEL_DRIVER,
        SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, sysFull, nullptr, nullptr, nullptr, nullptr, nullptr);
    if (!svc) svc = OpenServiceA(scm, tag, SERVICE_ALL_ACCESS);
    if (!svc) { CloseServiceHandle(scm); return false; }
    if (!StartServiceA(svc, 0, nullptr) && GetLastError() != 1056) {
        CloseServiceHandle(svc); CloseServiceHandle(scm); return false;
    }
    CloseServiceHandle(svc);
    CloseServiceHandle(scm);
    Sleep(1000);
    return true;
}

static bool LoadRtCore(const char* sysPath) {
    SC_HANDLE scm = OpenSCManagerA(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
    if (!scm) return false;
    const char* tag = "RtCoreSvc";
    SC_HANDLE svc = OpenServiceA(scm, tag, SERVICE_ALL_ACCESS);
    if (svc) {
        SERVICE_STATUS ss{};
        ControlService(svc, SERVICE_CONTROL_STOP, &ss);
        DeleteService(svc);
        CloseServiceHandle(svc);
        Sleep(1500);
    }
    char sysFull[MAX_PATH];
    GetFullPathNameA(sysPath, MAX_PATH, sysFull, nullptr);
    svc = CreateServiceA(scm, tag, tag, SERVICE_ALL_ACCESS, SERVICE_KERNEL_DRIVER,
        SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, sysFull, nullptr, nullptr, nullptr, nullptr, nullptr);
    if (!svc) svc = OpenServiceA(scm, tag, SERVICE_ALL_ACCESS);
    if (!svc) { CloseServiceHandle(scm); return false; }
    if (!StartServiceA(svc, 0, nullptr) && GetLastError() != 1056) {
        CloseServiceHandle(svc); CloseServiceHandle(scm); return false;
    }
    CloseServiceHandle(svc); CloseServiceHandle(scm);
    Sleep(1000);
    return true;
}

static bool ProbeRtCore(const char* sys) {
    if (!LoadRtCore(sys)) { fprintf(stderr, "[-] rtcore load failed gle=%lu\n", GetLastError()); return false; }
    HANDLE h = CreateFileA("\\\\.\\RTCore64", GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) { fprintf(stderr, "[-] RTCore64 open gle=%lu\n", GetLastError()); return false; }
    uint8_t buf[48]{};
    *reinterpret_cast<uint64_t*>(buf + 8) = 0x1000ull;
    *reinterpret_cast<uint32_t*>(buf + 24) = 4;
    DWORD got = 0;
    if (!DeviceIoControl(h, 0x80002048, buf, sizeof(buf), buf, sizeof(buf), &got, nullptr)) {
        fprintf(stderr, "[-] RTCore READ_MEM gle=%lu\n", GetLastError());
        CloseHandle(h);
        return false;
    }
    uint32_t val = *reinterpret_cast<uint32_t*>(buf + 28);
    printf("[+] rtcore phys 0x1000 -> 0x%08x\n", val);
    CloseHandle(h);
    return true;
}

static bool LoadTagged(const char* tag, const char* sysPath) {
    SC_HANDLE scm = OpenSCManagerA(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
    if (!scm) return false;
    SC_HANDLE svc = OpenServiceA(scm, tag, SERVICE_ALL_ACCESS);
    if (svc) {
        SERVICE_STATUS ss{};
        ControlService(svc, SERVICE_CONTROL_STOP, &ss);
        DeleteService(svc);
        CloseServiceHandle(svc);
        Sleep(800);
    }
    char sysFull[MAX_PATH];
    GetFullPathNameA(sysPath, MAX_PATH, sysFull, nullptr);
    svc = CreateServiceA(scm, tag, tag, SERVICE_ALL_ACCESS, SERVICE_KERNEL_DRIVER,
        SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, sysFull, nullptr, nullptr, nullptr, nullptr, nullptr);
    if (!svc) svc = OpenServiceA(scm, tag, SERVICE_ALL_ACCESS);
    if (!svc) { CloseServiceHandle(scm); return false; }
    if (!StartServiceA(svc, 0, nullptr) && GetLastError() != 1056) {
        CloseServiceHandle(svc); CloseServiceHandle(scm); return false;
    }
    CloseServiceHandle(svc);
    CloseServiceHandle(scm);
    Sleep(500);
    return true;
}

static bool MyPortRead32(HANDLE h, uint64_t pa, uint32_t* out) {
    // Driver requires InputBufferLength == 12 (not sizeof, which pads to 16).
    uint8_t pkt[16]{};
    *reinterpret_cast<uint64_t*>(pkt) = pa;
    *reinterpret_cast<uint32_t*>(pkt + 8) = 4;
    DWORD got = 0;
    if (!DeviceIoControl(h, IOCTL_MYPORT_READ, pkt, 12, pkt, 12, &got, nullptr))
        return false;
    *out = *reinterpret_cast<uint32_t*>(pkt);
    return true;
}

static bool MyPortWrite32(HANDLE h, uint64_t pa, uint32_t value) {
    uint8_t pkt[16]{};
    *reinterpret_cast<uint64_t*>(pkt) = pa;
    *reinterpret_cast<uint32_t*>(pkt + 8) = 4;
    *reinterpret_cast<uint32_t*>(pkt + 12) = value;
    DWORD got = 0;
    return DeviceIoControl(h, IOCTL_MYPORT_WRITE, pkt, 16, pkt, 16, &got, nullptr) != 0;
}

static bool ProbeMyPort(const char* sys) {
    HANDLE h = CreateFileW(L"\\\\.\\MyPortIODev", GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        if (!LoadTagged("MyPortIO", sys)) {
            fprintf(stderr, "[-] myport load failed gle=%lu (elevate for scm)\n", GetLastError());
            return false;
        }
        h = CreateFileW(L"\\\\.\\MyPortIODev", GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
        if (h == INVALID_HANDLE_VALUE) {
            fprintf(stderr, "[-] MyPortIODev open gle=%lu\n", GetLastError());
            return false;
        }
    }
    const uint64_t pa = 0x1100;
    uint32_t orig = 0;
    if (!MyPortRead32(h, pa, &orig)) {
        fprintf(stderr, "[-] myport read gle=%lu\n", GetLastError());
        CloseHandle(h);
        return false;
    }
    uint32_t mark = (GetTickCount() & 0xFFFF) | 0xA5000000u;
    if (!MyPortWrite32(h, pa, mark)) {
        fprintf(stderr, "[-] myport write gle=%lu (read ok val=0x%08x)\n", GetLastError(), orig);
        CloseHandle(h);
        return false;
    }
    uint32_t chk = 0;
    if (!MyPortRead32(h, pa, &chk)) {
        fprintf(stderr, "[-] myport reread gle=%lu\n", GetLastError());
        CloseHandle(h);
        return false;
    }
    MyPortWrite32(h, pa, orig);
    if (chk == mark)
        printf("[+] myport pa=0x%llx read=0x%08x write_rmw_ok mark=0x%08x\n",
            (unsigned long long)pa, orig, mark);
    else
        printf("[!] myport pa=0x%llx read=0x%08x wrote=0x%08x got=0x%08x\n",
            (unsigned long long)pa, orig, mark, chk);
    CloseHandle(h);
    return chk == mark;
}

static bool ProbePawnIO(const char* sys) {
    (void)sys;
    fprintf(stderr, "[-] pawnio: method 3 needs signed ioctl_ bytecode (not raw IOCTL) — use OpenRGB+PawnIO scripts\n");
    return false;
}

static bool ProbeMsIo(const char* sys) {
    if (!LoadMsIo(sys)) {
        fprintf(stderr, "[-] load %s failed gle=%lu\n", sys, GetLastError());
        return false;
    }
    HANDLE h = CreateFileW(L"\\\\.\\MsIo", GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "[-] MsIo open gle=%lu\n", GetLastError());
        return false;
    }
    MsIoMapPkt pkt{};
    if (!MapMsIo(h, 0x1000, 4, pkt)) {
        DWORD gle = GetLastError();
        fprintf(stderr, "[-] MsIo IOCTL map gle=%lu", gle);
        if (gle == 998 || gle == 87)
            fprintf(stderr, " (MemortAccessBase=0 on this chipset/VM — phys IOCTL blocked; try bare metal)");
        fprintf(stderr, "\n");
        CloseHandle(h);
        return false;
    }
    uint16_t sig = 0;
    uint16_t orig = 0;
    __try {
        sig = *(uint16_t*)pkt.linear;
        orig = *(uint16_t*)((uint8_t*)pkt.linear + 2);
        uint16_t mark = (uint16_t)(GetTickCount() & 0xFFFF);
        *(uint16_t*)((uint8_t*)pkt.linear + 2) = mark;
        if (*(uint16_t*)((uint8_t*)pkt.linear + 2) == mark)
            printf("[+] msio pa=0x1000 va=%p sig=0x%04x write_rmw_ok mark=0x%04x\n", pkt.linear, sig, mark);
        else
            printf("[!] msio va=%p sig=0x%04x write_verify_fail\n", pkt.linear, sig);
        *(uint16_t*)((uint8_t*)pkt.linear + 2) = orig;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        printf("[-] msio va=%p access fault\n", pkt.linear);
    }
    UnmapMsIo(h, pkt);
    CloseHandle(h);
    return true;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s corsair|msio|myport|rtcore|pawnio [driver.sys]\n", argv[0]);
        return 2;
    }
    if (_stricmp(argv[1], "myport") == 0) {
        const char* sys = (argc >= 3) ? argv[2] : "MyPortIO_x64.sys";
        return ProbeMyPort(sys) ? 0 : 1;
    }
    if (_stricmp(argv[1], "rtcore") == 0) {
        const char* sys = (argc >= 3) ? argv[2] : "RTCore64.sys";
        return ProbeRtCore(sys) ? 0 : 1;
    }
    if (_stricmp(argv[1], "pawnio") == 0)
        return ProbePawnIO(argc >= 3 ? argv[2] : "PawnIO.sys") ? 0 : 1;
    if (_stricmp(argv[1], "msio") == 0) {
        const char* sys = (argc >= 3) ? argv[2] : "MsIo64.sys";
        return ProbeMsIo(sys) ? 0 : 1;
    }
    if (_stricmp(argv[1], "corsair") != 0) {
        fprintf(stderr, "usage: %s corsair|msio|myport|rtcore|pawnio [driver.sys]\n", argv[0]);
        return 2;
    }
    const char* sys = (argc >= 3) ? argv[2] : "CorsairLLAccess64.sys";
    if (!LoadCorsair(sys)) {
        fprintf(stderr, "[-] load %s failed gle=%lu\n", sys, GetLastError());
        return 1;
    }
    HANDLE h = CreateFileW(L"\\\\.\\CorsairLLAccess", GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "[-] CreateFile gle=%lu\n", GetLastError());
        return 1;
    }
    void* m = MapPhys(h, 0x1000, 0x1000);
    if (!m) {
        fprintf(stderr, "[-] IOCTL map gle=%lu\n", GetLastError());
        CloseHandle(h);
        return 1;
    }
    uint16_t sig = 0;
    uint16_t orig = 0;
    __try {
        sig = *(uint16_t*)m;
        orig = *(uint16_t*)((uint8_t*)m + 0x100);
        uint16_t mark = (uint16_t)(GetTickCount() & 0xFFFF);
        *(uint16_t*)((uint8_t*)m + 0x100) = mark;
        uint16_t chk = *(uint16_t*)((uint8_t*)m + 0x100);
        if (chk == mark)
            printf("[+] mapped pa=0x1000 va=%p sig=0x%04x write_rmw_ok mark=0x%04x\n", m, sig, mark);
        else
            printf("[!] mapped pa=0x1000 va=%p sig=0x%04x write_verify_fail\n", m, sig);
        *(uint16_t*)((uint8_t*)m + 0x100) = orig;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        printf("[-] mapped va=%p access fault\n", m);
    }
    UnmapCorsair(h, m);
    CloseHandle(h);
    return 0;
}
