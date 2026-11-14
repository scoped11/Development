#pragma once
#include "robrun_mem.hpp"
#include <algorithm>
#include <cstring>
#include <vector>

// Scan committed private read/write regions for an 8-byte klass header (driver-backed reads).
inline std::vector<uintptr_t> HeapScanKlass(RobRunMem& mem, uint32_t pid, uintptr_t klass,
                                              uintptr_t imageLo, uintptr_t imageHi, int maxHits = 24)
{
    std::vector<uintptr_t> hits;
    if (!klass)
        return hits;

    HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!h)
        return hits;

    uint8_t needle[8]{};
    std::memcpy(needle, &klass, 8);
    MEMORY_BASIC_INFORMATION mbi{};
    uintptr_t cursor = 0x10000;
    const size_t chunk = 1 << 20;

    while (cursor < 0x00007FF000000000ULL && hits.size() < static_cast<size_t>(maxHits)) {
        if (!VirtualQueryEx(h, reinterpret_cast<LPCVOID>(cursor), &mbi, sizeof(mbi)))
            break;
        uintptr_t base = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
        size_t size = mbi.RegionSize;
        cursor = base + size;

        if (mbi.State != MEM_COMMIT)
            continue;
        if (mbi.Type != MEM_PRIVATE)
            continue;
        if (mbi.Protect == PAGE_NOACCESS || (mbi.Protect & PAGE_GUARD))
            continue;
        const DWORD prot = mbi.Protect & 0xFF;
        if (prot != PAGE_READWRITE && prot != PAGE_EXECUTE_READWRITE)
            continue;

        std::vector<uint8_t> buf;
        for (size_t off = 0; off < size && hits.size() < static_cast<size_t>(maxHits); off += chunk) {
            size_t n = (std::min)(chunk, size - off);
            buf.resize(n);
            if (!mem.ReadRaw(base + off, buf.data(), n))
                continue;
            for (size_t i = 0; i + 8 <= n; i += 8) {
                uintptr_t addr = base + off + i;
                if (imageLo && addr >= imageLo && addr < imageHi)
                    continue;
                if (std::memcmp(buf.data() + i, needle, 8) != 0)
                    continue;
                hits.push_back(addr);
                if (hits.size() >= static_cast<size_t>(maxHits))
                    break;
            }
        }
    }
    CloseHandle(h);
    return hits;
}
