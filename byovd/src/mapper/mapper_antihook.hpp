#pragma once
// Usermode preflight: detect inline hooks on APIs used for BYOVD map + overlay I/O.

#include <Windows.h>
#include <winternl.h>
#include <cstdint>
#include <cstring>

static bool MapperExportLooksHooked(const void* fn)
{
    if (!fn) return true;
    const auto* p = reinterpret_cast<const uint8_t*>(fn);
    if (p[0] == 0xE9) return true;                         // jmp rel32
    if (p[0] == 0xEB) return true;                         // jmp short
    if (p[0] == 0xFF && (p[1] == 0x25 || p[1] == 0x15)) return true; // jmp [rip+...]
    if (p[0] == 0x48 && p[1] == 0xB8) return true;         // mov rax, imm64 (detour)
    if (p[0] == 0xCC) return true;                         // int3 stub
    return false;
}

static void* MapperResolveExport(const char* mod, const char* name)
{
    HMODULE h = GetModuleHandleA(mod);
    if (!h) h = LoadLibraryA(mod);
    if (!h) return nullptr;
    return reinterpret_cast<void*>(GetProcAddress(h, name));
}

struct MapperAntiHookReport {
    int hooks;
    int checked;
};

static MapperAntiHookReport MapperAntiHookScan(void)
{
    static const char* kTargets[] = {
        "ntdll.dll!NtDeviceIoControlFile",
        "ntdll.dll!NtQueryIntervalProfile",
        "ntdll.dll!NtQuerySystemInformation",
        "kernel32.dll!DeviceIoControl",
        "kernel32.dll!CreateFileW",
    };

    MapperAntiHookReport rep{0, 0};
    for (auto* spec : kTargets) {
        char mod[64]{}, sym[64]{};
        const char* bang = strchr(spec, '!');
        if (!bang) continue;
        size_t mlen = (size_t)(bang - spec);
        if (mlen >= sizeof(mod)) continue;
        memcpy(mod, spec, mlen);
        strncpy_s(sym, bang + 1, _TRUNCATE);

        void* fn = MapperResolveExport(mod, sym);
        rep.checked++;
        if (MapperExportLooksHooked(fn))
            rep.hooks++;
    }
    return rep;
}
