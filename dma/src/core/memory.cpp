#define NOMINMAX
#include <Windows.h>
#include <TlHelp32.h>

#include "memory.h"
#include "config/config.h"
#include "game/offsets.h"
#include "render/splash.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <algorithm>
#include <thread>
#include <chrono>
#include <cstring>
#include <cstdio>
#include <cstdarg>
#include <share.h>

// =============================================================================
// Initialization
// =============================================================================

static bool IsOverlayProcessName(const wchar_t* name) {
    return name && _wcsnicmp(name, L"RobRunDMA", 9) == 0;
}

static DWORD g_watchdogPid = 0;

void Memory::SetWatchdogPid(DWORD pid) {
    g_watchdogPid = pid;
}

void Memory::ReclaimDevice() {
    const DWORD self = GetCurrentProcessId();
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE)
        return;

    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    int killed = 0;
    if (Process32FirstW(snap, &pe)) {
        do {
            if (pe.th32ProcessID == self || pe.th32ProcessID == g_watchdogPid)
                continue;
            if (!IsOverlayProcessName(pe.szExeFile))
                continue;
            HANDLE proc = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, pe.th32ProcessID);
            if (!proc)
                continue;
            std::cout << "[*] Closing leftover FPGA session PID " << pe.th32ProcessID << "\n";
            TerminateProcess(proc, 1);
            WaitForSingleObject(proc, 4000);
            CloseHandle(proc);
            killed++;
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    if (killed)
        Sleep(600);
}

static HANDLE g_fpgaMutex = nullptr;

static void TakeExclusiveFpgaMutex() {
    if (g_fpgaMutex)
        return;
    g_fpgaMutex = CreateMutexW(nullptr, FALSE, L"Local\\RobRunDMA_FPGA");
    if (!g_fpgaMutex)
        return;
    const DWORD wait = WaitForSingleObject(g_fpgaMutex, 0);
    if (wait == WAIT_OBJECT_0 || wait == WAIT_ABANDONED)
        return;
    Memory::Get().ReclaimDevice();
    WaitForSingleObject(g_fpgaMutex, 4000);
}

bool Memory::Init() {
    if (m_vmm) {
        std::cout << "[+] Already initialized\n";
        return true;
    }

    Splash::SetStage("Claiming DMA card", 0.16f);
    TakeExclusiveFpgaMutex();
    ReclaimDevice();

    Splash::SetStage("Preparing memory map", 0.22f);
    DumpMemoryMap();

    // Step 2: Initialize VMM with FPGA device and memory map
    std::string mmapPath = std::filesystem::temp_directory_path().string() + "\\mmap.txt";
    bool useMemMap = std::filesystem::exists(mmapPath);

    std::vector<std::string> argStrings;
    argStrings.push_back("");
    argStrings.push_back("-device");
    argStrings.push_back("fpga://algo=0");
    argStrings.push_back("-v");
    argStrings.push_back("-printf");

    if (useMemMap) {
        argStrings.push_back("-memmap");
        argStrings.push_back(mmapPath);
    }

    std::vector<LPCSTR> args;
    for (auto& s : argStrings) args.push_back(s.c_str());

    Splash::SetStage("Initializing FPGA", 0.30f);
    std::cout << "[*] Initializing VMMDLL...\n";
    m_vmm = VMMDLL_Initialize(static_cast<DWORD>(args.size()), args.data());

    if (!m_vmm) {
        std::cerr << "[-] Failed to initialize VMMDLL\n";
        if (useMemMap) {
            std::cout << "[*] Retrying without memory map...\n";
            std::filesystem::remove(mmapPath);
            argStrings.erase(argStrings.end() - 2, argStrings.end());
            args.clear();
            for (auto& s : argStrings) args.push_back(s.c_str());
            m_vmm = VMMDLL_Initialize(static_cast<DWORD>(args.size()), args.data());
        }
        if (!m_vmm) {
            std::cerr << "[-] VMMDLL initialization failed — retrying after reclaiming FPGA\n";
            ReclaimDevice();
            m_vmm = VMMDLL_Initialize(static_cast<DWORD>(args.size()), args.data());
        }
        if (!m_vmm) {
            std::cerr << "[-] VMMDLL initialization failed. Check DMA card connection.\n";
            return false;
        }
    }

    Splash::SetStage("Configuring DMA card", 0.36f);
    ConfigureFPGA();

    std::cout << "[+] VMMDLL initialized successfully\n";
    return true;
}

bool Memory::ConfigureFPGA() {
    if (!m_vmm) return false;

    ULONG64 qwID = 0, qwVersionMajor = 0, qwVersionMinor = 0;
    VMMDLL_ConfigGet(m_vmm, LC_OPT_FPGA_FPGA_ID, &qwID);
    VMMDLL_ConfigGet(m_vmm, LC_OPT_FPGA_VERSION_MAJOR, &qwVersionMajor);
    VMMDLL_ConfigGet(m_vmm, LC_OPT_FPGA_VERSION_MINOR, &qwVersionMinor);

    std::cout << "[*] FPGA ID: " << qwID
              << " v" << qwVersionMajor << "." << qwVersionMinor << "\n";

    if (qwVersionMajor < 4 || (qwVersionMajor == 4 && qwVersionMinor < 7)) {
        std::cout << "[!] FPGA firmware < 4.7 — PCIe register configuration skipped\n";
        return true;
    }

    // Configure PCIe TLP abort register auto-clear for stealth.
    // This prevents the target system from detecting DMA read errors.
    HANDLE hLC = nullptr;
    LC_CONFIG lcConfig = {};
    lcConfig.dwVersion = LC_CONFIG_VERSION;
    lcConfig.szDevice[0] = '\0';
    strncpy_s(lcConfig.szDevice, "existing", _TRUNCATE);

    hLC = LcCreate(&lcConfig);
    if (!hLC) {
        std::cerr << "[-] Failed to create LeechCore handle for FPGA config\n";
        return false;
    }

    // Clear TLP abort status registers
    BYTE abortClear[4] = { 0x10, 0x00, 0x10, 0x00 };
    LcCommand(hLC, LC_CMD_FPGA_CFGREGPCIE_MARKWR | 0x002, 4, abortClear, nullptr, nullptr);

    BYTE cfgSpace[4] = { 0x00, 0x00, 0x00, 0x00 };
    LcCommand(hLC, LC_CMD_FPGA_CFGREGPCIE | 0x002, 0, nullptr, nullptr, nullptr);

    LcClose(hLC);
    std::cout << "[+] FPGA PCIe registers configured\n";
    return true;
}

bool Memory::DumpMemoryMap(bool forceRefresh) {
    std::string mmapPath = std::filesystem::temp_directory_path().string() + "\\mmap.txt";

    if (!forceRefresh && std::filesystem::exists(mmapPath)) {
        std::cout << "[+] Using existing memory map: " << mmapPath << "\n";
        return true;
    }

    // Temporary VMM handle just for reading the physical memory map
    LPCSTR args[] = { "", "-device", "fpga://algo=0", "-v", "-printf" };
    VMM_HANDLE tempVmm = VMMDLL_Initialize(5, args);
    if (!tempVmm) {
        std::cerr << "[-] Cannot create temp VMM handle for memory map dump\n";
        return false;
    }

    PVMMDLL_MAP_PHYSMEM physMap = nullptr;
    if (!VMMDLL_Map_GetPhysMem(tempVmm, &physMap) || !physMap) {
        std::cerr << "[-] Failed to get physical memory map\n";
        VMMDLL_Close(tempVmm);
        return false;
    }

    std::ofstream ofs(mmapPath);
    if (!ofs.is_open()) {
        std::cerr << "[-] Failed to write memory map file\n";
        VMMDLL_MemFree(physMap);
        VMMDLL_Close(tempVmm);
        return false;
    }

    for (DWORD i = 0; i < physMap->cMap; i++) {
        auto& entry = physMap->pMap[i];
        ofs << std::hex
            << entry.pa << " " << (entry.pa + entry.cb - 1) << "\n";
    }

    ofs.close();
    VMMDLL_MemFree(physMap);
    VMMDLL_Close(tempVmm);

    std::cout << "[+] Memory map dumped to: " << mmapPath << "\n";
    return true;
}

// =============================================================================
// Process attachment
// =============================================================================

bool Memory::AttachToProcess(const std::string& processName) {
    if (!m_vmm) {
        std::cerr << "[-] VMMDLL not initialized\n";
        return false;
    }

    m_processName = processName;
    m_connected = false;
    Splash::SetStage("Searching for ROB RUN.exe", 0.46f);

    if (!FindProcess(processName)) {
        Splash::SetStage("Fixing process page tables", 0.50f);
        std::cout << "[*] Attempting CR3 fix...\n";
        if (!FixCr3()) {
            std::cerr << "[-] CR3 fix failed\n";
            return false;
        }
        if (!FindProcess(processName)) {
            std::cerr << "[-] Process not found even after CR3 fix: " << processName << "\n";
            return false;
        }
    }

    m_connected = true;
    std::cout << "[+] Attached to " << processName
              << " (PID: " << m_pid
              << ", Base: 0x" << std::hex << m_baseAddress << std::dec
              << ", Size: " << (m_baseSize / 1024) << " KB)\n";
    Splash::SetStage("Pinning GameAssembly (TypeInfo)", 0.68f);
    if (!EnsureIl2CppReady()) {
        Splash::SetStage("Pinning GameAssembly (TypeInfo)", 0.78f);
        std::cout << "[!] GameAssembly.dll not in module map yet — CR3/pid retry will continue\n";
    }
    return true;
}

bool Memory::FindProcess(const std::string& processName) {
    DWORD pid = 0;
    if (!VMMDLL_PidGetFromName(m_vmm, const_cast<LPSTR>(processName.c_str()), &pid) || !pid)
        return false;

    m_pid = pid;
    m_baseAddress = GetModuleBase(processName);
    m_baseSize = GetModuleSize(processName);
    // EAC often hides the main EXE from the module map. PID is enough to
    // keep the FPGA session; GameAssembly is discovered from VADs separately.
    return true;
}

void Memory::Close() {
    m_connected = false;
    VMM_HANDLE handle = static_cast<VMM_HANDLE>(
        InterlockedExchangePointer(reinterpret_cast<PVOID*>(&m_vmm), nullptr));
    m_pid = 0;
    m_baseAddress = 0;
    m_baseSize = 0;
    m_gameAssemblyBase = 0;
    m_gameAssemblySize = 0;
    m_unityPlayerBase = 0;
    m_unityPlayerSize = 0;
    if (handle)
        VMMDLL_Close(handle);
    VMMDLL_CloseAll();
}

// =============================================================================
// Module info
// =============================================================================

void Memory::RefreshProcessModuleMap() {
    if (!m_vmm || !m_pid) return;
    const auto now = std::chrono::steady_clock::now();
    if (m_lastModuleMapRefresh != std::chrono::steady_clock::time_point{} &&
        now - m_lastModuleMapRefresh < std::chrono::seconds(2))
        return;
    m_lastModuleMapRefresh = now;
    VMMDLL_ConfigSet(m_vmm, VMMDLL_OPT_REFRESH_SPECIFIC_PROCESS, m_pid);
}

bool Memory::EnsureIl2CppReady() {
    if (!m_vmm || !m_pid)
        return false;
    if (!m_gameAssemblyBase)
        Splash::SetStage("Pinning GameAssembly (TypeInfo)", 0.80f);
    RefreshProcessModuleMap();
    if (GetModuleBase("GameAssembly.dll")) {
        if (!m_unityPlayerBase)
            DiscoverUnityPlayer();
        return true;
    }

    DiscoverGameAssembly();
    if (m_gameAssemblyBase) {
        DiscoverUnityPlayer();
        return true;
    }

    FixCr3();
    m_lastModuleMapRefresh = {};
    RefreshProcessModuleMap();
    DiscoverGameAssembly();
    if (GetModuleBase("GameAssembly.dll") || m_gameAssemblyBase)
        return true;

    if (!m_processName.empty()) {
        DWORD pid = 0;
        if (VMMDLL_PidGetFromName(m_vmm, const_cast<LPSTR>(m_processName.c_str()), &pid) && pid != 0 &&
            pid != m_pid) {
            std::cout << "[*] Process pid changed " << m_pid << " -> " << pid << "\n";
            m_pid = pid;
            m_gameAssemblyBase = 0;
            m_gameAssemblySize = 0;
            m_baseAddress = GetModuleBase(m_processName);
            m_baseSize = GetModuleSize(m_processName);
            m_lastModuleMapRefresh = {};
            RefreshProcessModuleMap();
            FixCr3();
            m_lastModuleMapRefresh = {};
            RefreshProcessModuleMap();
            DiscoverGameAssembly();
        }
    }
    return GetModuleBase("GameAssembly.dll") != 0 || m_gameAssemblyBase != 0;
}

namespace {

bool NameIsGameAssembly(const char* name) {
    if (!name || !*name)
        return false;
    std::string s(name);
    for (char& c : s)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s.find("gameassembly") != std::string::npos;
}

bool NameIsUnityPlayer(const char* name) {
    if (!name || !*name)
        return false;
    std::string s(name);
    for (char& c : s)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s.find("unityplayer") != std::string::npos;
}

} // namespace

void Memory::PublishGameAssembly(uintptr_t base, size_t size) {
    if (!base)
        return;
    if (!size)
        size = ReadPeImageSize(base);
    if (size < 0x100000)
        return;
    const uint16_t mz = Read<uint16_t>(base);
    if (mz != 0x5A4D)
        return;
    if (m_gameAssemblyBase == base && m_gameAssemblySize == size)
        return;
    m_gameAssemblyBase = base;
    m_gameAssemblySize = size;
    std::cout << "[+] GameAssembly autodiscovered @ 0x" << std::hex << base
              << " size 0x" << size << std::dec << "\n";
}

void Memory::PublishUnityPlayer(uintptr_t base, size_t size) {
    if (!base)
        return;
    if (!size)
        size = ReadPeImageSize(base);
    if (size < 0x100000)
        return;
    const uint16_t mz = Read<uint16_t>(base);
    if (mz != 0x5A4D)
        return;
    if (m_unityPlayerBase == base && m_unityPlayerSize == size)
        return;
    m_unityPlayerBase = base;
    m_unityPlayerSize = size;
    std::cout << "[+] UnityPlayer autodiscovered @ 0x" << std::hex << base
              << " size 0x" << size << std::dec << "\n";
}

bool Memory::DiscoverGameAssembly() {
    if (!m_vmm || !m_pid)
        return false;
    if (m_gameAssemblyBase)
        return true;

    auto tryPublish = [&](uintptr_t base, size_t size, const char* via) -> bool {
        if (!base)
            return false;
        PublishGameAssembly(base, size);
        if (m_gameAssemblyBase) {
            std::cout << "[+] GameAssembly source: " << via << "\n";
            return true;
        }
        return false;
    };

    PVMMDLL_MAP_MODULE pMap = nullptr;
    if (VMMDLL_Map_GetModuleU(m_vmm, m_pid, &pMap, 0) && pMap) {
        for (DWORD i = 0; i < pMap->cMap; ++i) {
            const auto& e = pMap->pMap[i];
            if (NameIsGameAssembly(e.uszText) ||
                NameIsGameAssembly(e.uszFullName)) {
                if (tryPublish(static_cast<uintptr_t>(e.vaBase),
                               static_cast<size_t>(e.cbImageSize), "module-map"))
                    break;
            }
        }
        VMMDLL_MemFree(pMap);
        if (m_gameAssemblyBase)
            return true;
    }

    PVMMDLL_MAP_VAD pVad = nullptr;
    if (VMMDLL_Map_GetVadU(m_vmm, m_pid, TRUE, &pVad) && pVad) {
        for (DWORD i = 0; i < pVad->cMap; ++i) {
            const auto& e = pVad->pMap[i];
            if (!e.fImage)
                continue;
            if (!NameIsGameAssembly(e.uszText))
                continue;
            const uintptr_t base = static_cast<uintptr_t>(e.vaStart);
            const size_t span =
                e.vaEnd > e.vaStart ? static_cast<size_t>(e.vaEnd - e.vaStart) : 0;
            if (tryPublish(base, span, "vad"))
                break;
        }
        VMMDLL_MemFree(pVad);
        if (m_gameAssemblyBase)
            return true;
    }

    VMMDLL_PROCESS_INFORMATION pinfo = {};
    pinfo.magic = VMMDLL_PROCESS_INFORMATION_MAGIC;
    pinfo.wVersion = VMMDLL_PROCESS_INFORMATION_VERSION;
    SIZE_T cb = sizeof(pinfo);
    if (VMMDLL_ProcessGetInformation(m_vmm, m_pid, &pinfo, &cb) && pinfo.win.vaPEB) {
        const uintptr_t peb = static_cast<uintptr_t>(pinfo.win.vaPEB);
        const uintptr_t ldr = Read<uintptr_t>(peb + 0x18);
        if (ldr) {
            const uintptr_t head = ldr + 0x20;
            uintptr_t link = Read<uintptr_t>(head);
            int guard = 0;
            while (link && link != head && guard++ < 512) {
                const uintptr_t entry = link - 0x10;
                const uintptr_t dllBase = Read<uintptr_t>(entry + 0x30);
                const uint32_t imageSize = Read<uint32_t>(entry + 0x40);
                struct {
                    uint16_t length;
                    uint16_t maxLength;
                    uint32_t pad;
                    uintptr_t buffer;
                } name = {};
                if (ReadRaw(entry + 0x58, &name, sizeof(name)) && name.buffer && name.length > 0 &&
                    name.length < 512) {
                    std::wstring wname(name.length / 2, L'\0');
                    if (ReadRaw(name.buffer, wname.data(), name.length)) {
                        std::string narrow;
                        narrow.reserve(wname.size());
                        for (wchar_t c : wname) {
                            if (c == 0)
                                break;
                            narrow.push_back(c < 128 ? static_cast<char>(c) : '?');
                        }
                        if (NameIsGameAssembly(narrow.c_str())) {
                            if (tryPublish(dllBase, imageSize, "peb-ldr"))
                                break;
                        }
                    }
                }
                link = Read<uintptr_t>(link);
            }
        }
    }

    // Unnamed VAD images: match known GameAssembly SizeOfImage (see offsets.h).
    pVad = nullptr;
    if (VMMDLL_Map_GetVadU(m_vmm, m_pid, TRUE, &pVad) && pVad) {
        for (DWORD i = 0; i < pVad->cMap; ++i) {
            const auto& e = pVad->pMap[i];
            if (!e.fImage)
                continue;
            const uintptr_t base = static_cast<uintptr_t>(e.vaStart);
            if (Read<uint16_t>(base) != 0x5A4D)
                continue;
            const size_t img = ReadPeImageSize(base);
            if (!offsets::IsKnownGameAssemblySize(img))
                continue;
            const size_t span =
                e.vaEnd > e.vaStart ? static_cast<size_t>(e.vaEnd - e.vaStart) : 0;
            if (span && span < img)
                continue;
            if (tryPublish(base, img, "vad-pe-signature"))
                break;
        }
        VMMDLL_MemFree(pVad);
    }
    return m_gameAssemblyBase != 0;
}

bool Memory::EnsureUnityPlayer() {
    if (!m_vmm || !m_pid)
        return false;
    if (m_unityPlayerBase)
        return true;
  return DiscoverUnityPlayer();
}

bool Memory::DiscoverUnityPlayer() {
    if (!m_vmm || !m_pid)
        return false;
    if (m_unityPlayerBase)
        return true;

    auto tryPublish = [&](uintptr_t base, size_t size, const char* via) -> bool {
        if (!base)
            return false;
        PublishUnityPlayer(base, size);
        if (m_unityPlayerBase) {
            std::cout << "[+] UnityPlayer source: " << via << "\n";
            return true;
        }
        return false;
    };

    PVMMDLL_MAP_MODULE pMap = nullptr;
    if (VMMDLL_Map_GetModuleU(m_vmm, m_pid, &pMap, 0) && pMap) {
        for (DWORD i = 0; i < pMap->cMap; ++i) {
            const auto& e = pMap->pMap[i];
            if (NameIsUnityPlayer(e.uszText) || NameIsUnityPlayer(e.uszFullName)) {
                if (tryPublish(static_cast<uintptr_t>(e.vaBase),
                               static_cast<size_t>(e.cbImageSize), "module-map"))
                    break;
            }
        }
        VMMDLL_MemFree(pMap);
        if (m_unityPlayerBase)
            return true;
    }

    PVMMDLL_MAP_VAD pVad = nullptr;
    if (VMMDLL_Map_GetVadU(m_vmm, m_pid, TRUE, &pVad) && pVad) {
        for (DWORD i = 0; i < pVad->cMap; ++i) {
            const auto& e = pVad->pMap[i];
            if (!e.fImage)
                continue;
            if (!NameIsUnityPlayer(e.uszText))
                continue;
            const uintptr_t base = static_cast<uintptr_t>(e.vaStart);
            const size_t span =
                e.vaEnd > e.vaStart ? static_cast<size_t>(e.vaEnd - e.vaStart) : 0;
            if (tryPublish(base, span, "vad"))
                break;
        }
        VMMDLL_MemFree(pVad);
        if (m_unityPlayerBase)
            return true;
    }

    pVad = nullptr;
    if (VMMDLL_Map_GetVadU(m_vmm, m_pid, TRUE, &pVad) && pVad) {
        for (DWORD i = 0; i < pVad->cMap; ++i) {
            const auto& e = pVad->pMap[i];
            if (!e.fImage)
                continue;
            const uintptr_t base = static_cast<uintptr_t>(e.vaStart);
            if (Read<uint16_t>(base) != 0x5A4D)
                continue;
            const size_t img = ReadPeImageSize(base);
            if (!offsets::IsKnownUnityPlayerSize(img))
                continue;
            const size_t span =
                e.vaEnd > e.vaStart ? static_cast<size_t>(e.vaEnd - e.vaStart) : 0;
            if (span && span < img)
                continue;
            if (tryPublish(base, img, "vad-pe-signature"))
                break;
        }
        VMMDLL_MemFree(pVad);
    }
    return m_unityPlayerBase != 0;
}

static bool SameModuleName(const char* a, const std::string& b) {
    if (!a)
        return false;
    return _stricmp(a, b.c_str()) == 0;
}

uintptr_t Memory::GetModuleBase(const std::string& moduleName) {
    if (!m_vmm || !m_pid) return 0;
    const bool gameAssembly =
        _stricmp(moduleName.c_str(), "GameAssembly.dll") == 0;
    const bool unityPlayer =
        _stricmp(moduleName.c_str(), "UnityPlayer.dll") == 0;
    if (gameAssembly && m_gameAssemblyBase)
        return m_gameAssemblyBase;
    if (unityPlayer && m_unityPlayerBase)
        return m_unityPlayerBase;
    RefreshProcessModuleMap();

    PVMMDLL_MAP_MODULEENTRY pModuleEntry = nullptr;
    if (VMMDLL_Map_GetModuleFromNameU(m_vmm, m_pid,
            const_cast<LPSTR>(moduleName.c_str()), &pModuleEntry, 0)) {
        uintptr_t base = pModuleEntry->vaBase;
        const size_t size = static_cast<size_t>(pModuleEntry->cbImageSize);
        VMMDLL_MemFree(pModuleEntry);
        if (gameAssembly)
            PublishGameAssembly(base, size);
        if (unityPlayer)
            PublishUnityPlayer(base, size);
        return base;
    }

    PVMMDLL_MAP_MODULE pMap = nullptr;
    if (!VMMDLL_Map_GetModuleU(m_vmm, m_pid, &pMap, 0) || !pMap) {
        if (gameAssembly) {
            DiscoverGameAssembly();
            return m_gameAssemblyBase;
        }
        if (unityPlayer) {
            DiscoverUnityPlayer();
            return m_unityPlayerBase;
        }
        return 0;
    }
    uintptr_t base = 0;
    for (DWORD i = 0; i < pMap->cMap; ++i) {
        const auto& e = pMap->pMap[i];
        if (SameModuleName(e.uszText, moduleName) ||
            (gameAssembly && NameIsGameAssembly(e.uszText)) ||
            (unityPlayer && NameIsUnityPlayer(e.uszText))) {
            base = static_cast<uintptr_t>(e.vaBase);
            if (gameAssembly)
                PublishGameAssembly(base, static_cast<size_t>(e.cbImageSize));
            if (unityPlayer)
                PublishUnityPlayer(base, static_cast<size_t>(e.cbImageSize));
            break;
        }
    }
    VMMDLL_MemFree(pMap);
    if (gameAssembly && !base)
        DiscoverGameAssembly();
    if (unityPlayer && !base)
        DiscoverUnityPlayer();
    if (gameAssembly && m_gameAssemblyBase)
        return m_gameAssemblyBase;
    if (unityPlayer && m_unityPlayerBase)
        return m_unityPlayerBase;
    return base;
}

size_t Memory::GetModuleSize(const std::string& moduleName) {
    if (!m_vmm || !m_pid) return 0;
    const bool gameAssembly =
        _stricmp(moduleName.c_str(), "GameAssembly.dll") == 0;
    const bool unityPlayer =
        _stricmp(moduleName.c_str(), "UnityPlayer.dll") == 0;
    if (gameAssembly && m_gameAssemblySize)
        return m_gameAssemblySize;
    if (unityPlayer && m_unityPlayerSize)
        return m_unityPlayerSize;
    RefreshProcessModuleMap();

    PVMMDLL_MAP_MODULEENTRY pModuleEntry = nullptr;
    if (VMMDLL_Map_GetModuleFromNameU(m_vmm, m_pid,
            const_cast<LPSTR>(moduleName.c_str()), &pModuleEntry, 0)) {
        size_t size = pModuleEntry->cbImageSize;
        VMMDLL_MemFree(pModuleEntry);
        return size;
    }

    PVMMDLL_MAP_MODULE pMap = nullptr;
    if (!VMMDLL_Map_GetModuleU(m_vmm, m_pid, &pMap, 0) || !pMap) {
        if (gameAssembly) {
            GetModuleBase("GameAssembly.dll");
            return m_gameAssemblySize;
        }
        if (unityPlayer) {
            GetModuleBase("UnityPlayer.dll");
            return m_unityPlayerSize;
        }
        return 0;
    }
    size_t size = 0;
    for (DWORD i = 0; i < pMap->cMap; ++i) {
        const auto& e = pMap->pMap[i];
        if (SameModuleName(e.uszText, moduleName) ||
            (gameAssembly && NameIsGameAssembly(e.uszText)) ||
            (unityPlayer && NameIsUnityPlayer(e.uszText))) {
            size = static_cast<size_t>(e.cbImageSize);
            if (gameAssembly)
                PublishGameAssembly(static_cast<uintptr_t>(e.vaBase), size);
            if (unityPlayer)
                PublishUnityPlayer(static_cast<uintptr_t>(e.vaBase), size);
            break;
        }
    }
    VMMDLL_MemFree(pMap);
    if (gameAssembly && !size) {
        GetModuleBase("GameAssembly.dll");
        size = m_gameAssemblySize;
    }
    if (unityPlayer && !size) {
        GetModuleBase("UnityPlayer.dll");
        size = m_unityPlayerSize;
    }
    return size;
}

size_t Memory::ReadPeImageSize(uintptr_t moduleBase) {
    if (!m_vmm || !moduleBase)
        return 0;
    const uint32_t e_lfanew = Read<uint32_t>(moduleBase + 0x3C);
    if (e_lfanew == 0 || e_lfanew > 0x1000)
        return 0;
    const uintptr_t nt = moduleBase + e_lfanew;
    const uint32_t peSig = Read<uint32_t>(nt);
    if (peSig != 0x00004550) // "PE\0\0"
        return 0;
    const uint16_t magic = Read<uint16_t>(nt + 4 + sizeof(IMAGE_FILE_HEADER));
    if (magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC)
        return 0;
    const uint32_t sizeOfImage =
        Read<uint32_t>(nt + 4 + sizeof(IMAGE_FILE_HEADER) + 0x38);
    if (sizeOfImage < 0x1000000 || sizeOfImage > 0x20000000)
        return 0;
    return static_cast<size_t>(sizeOfImage);
}

// =============================================================================
// Memory reads
// =============================================================================

namespace {
    thread_local bool t_dmaMotion = false;
    thread_local bool t_dmaDiscovery = false;
    thread_local int t_dmaHeapScan = 0;
    std::mutex g_dmaMutex;
    std::atomic<int> g_motionBurst{ 0 };

    struct HeapScanScope {
        HeapScanScope() { ++t_dmaHeapScan; }
        ~HeapScanScope() { --t_dmaHeapScan; }
        HeapScanScope(const HeapScanScope&) = delete;
        HeapScanScope& operator=(const HeapScanScope&) = delete;
    };

    DWORD ReadFlagsForThread() {
        // Do not CACHE_RECENT_ONLY here. That flag zeropads unread GC pages, so
        // BattleEntities/Game evaluation fails and splash never leaves
        // Resolving Game*. Heap scans also skip ZEROPAD_ON_FAIL — that made
        // unmapped GC windows look like valid zeros, so Game* never matched.
        (void)t_dmaDiscovery;
        if (t_dmaHeapScan > 0)
            return VMMDLL_FLAG_NOCACHE;
        return VMMDLL_FLAG_NOCACHE | VMMDLL_FLAG_ZEROPAD_ON_FAIL;
    }
}

void Memory::SetMotionThread(bool motion) {
    t_dmaMotion = motion;
}

void Memory::BeginMotionBurst() {
    g_motionBurst.fetch_add(1, std::memory_order_release);
}

void Memory::EndMotionBurst() {
    g_motionBurst.fetch_sub(1, std::memory_order_release);
}

void Memory::BeginDiscoveryBurst() {
    t_dmaDiscovery = true;
}

void Memory::EndDiscoveryBurst() {
    t_dmaDiscovery = false;
}

int Memory::MotionBurstDepth() const {
    return g_motionBurst.load(std::memory_order_acquire);
}

void Memory::Log(const char* fmt, ...) {
    if (!fmt)
        return;
    char path[MAX_PATH]{};
    GetModuleFileNameA(nullptr, path, MAX_PATH);
    if (char* slash = strrchr(path, '\\'))
        memcpy(slash + 1, "robrun.log", 11);
    HANDLE file = CreateFileA(path, FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return;
    char line[1024];
    va_list args;
    va_start(args, fmt);
    const int n = _vsnprintf_s(line, _TRUNCATE, fmt, args);
    va_end(args);
    if (n > 0) {
        DWORD written = 0;
        WriteFile(file, line, static_cast<DWORD>(n), &written, nullptr);
    }
    CloseHandle(file);
}

void RobRunDmaEnter() {
    if (t_dmaMotion) {
        g_dmaMutex.lock();
        return;
    }
    for (;;) {
        while (g_motionBurst.load(std::memory_order_acquire) > 0)
            std::this_thread::sleep_for(std::chrono::microseconds(200));
        g_dmaMutex.lock();
        if (g_motionBurst.load(std::memory_order_acquire) == 0)
            return;
        g_dmaMutex.unlock();
    }
}

void RobRunDmaLeave() {
    g_dmaMutex.unlock();
}

std::vector<uintptr_t> Memory::Search(const void* pattern, size_t size, size_t align, size_t maxResults) {
    (void)pattern;
    (void)size;
    (void)align;
    (void)maxResults;
    // Full-VA MemSearch stalls the FPGA for seconds. Attach uses SearchHeap.
    return {};
}

std::vector<uintptr_t> Memory::SearchRange(const void* pattern, size_t size, size_t align, size_t maxResults,
                                           uintptr_t vaMin, uintptr_t vaMax, int timeoutMs) {
    std::vector<uintptr_t> hits;
    if (!m_vmm || !m_pid || !pattern || size == 0 || size > 64)
        return hits;
    if (vaMax <= vaMin)
        return hits;
    if (g_motionBurst.load(std::memory_order_acquire) > 0)
        return hits;
    if (vaMax - vaMin > 0x400000ULL)
        return hits;
    if (maxResults == 0)
        maxResults = 1;
    if (align == 0)
        align = 1;
    if (timeoutMs <= 0)
        timeoutMs = 16;

    constexpr size_t kWindow = 0x10000;
    std::vector<uint8_t> buf(kWindow);
    uint64_t needle = 0;
    if (size <= 8)
        memcpy(&needle, pattern, size);
    uintptr_t va = vaMin;
    if (align > 1)
        va = (va + (align - 1)) & ~(static_cast<uintptr_t>(align) - 1);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (va < vaMax && hits.size() < maxResults) {
        if (g_motionBurst.load(std::memory_order_acquire) > 0)
            break;
        if (std::chrono::steady_clock::now() >= deadline)
            break;
        const size_t chunk = (std::min)(kWindow, static_cast<size_t>(vaMax - va));
        if (ReadRaw(va, buf.data(), chunk)) {
            if (size == 8 && align == 8) {
                for (size_t off = 0; off + 8 <= chunk && hits.size() < maxResults; off += 8) {
                    uint64_t word = 0;
                    memcpy(&word, buf.data() + off, 8);
                    if (word == needle)
                        hits.push_back(va + off);
                }
            } else {
                for (size_t off = 0; off + size <= chunk && hits.size() < maxResults; off += align) {
                    if (memcmp(buf.data() + off, pattern, size) == 0)
                        hits.push_back(va + off);
                }
            }
        }
        va += chunk;
    }
    return hits;
}

std::vector<uintptr_t> Memory::SearchHeap(const void* pattern, size_t size, size_t align, size_t maxResults,
                                          bool gcOnly, int timeoutMs) {
    std::vector<uintptr_t> hits;
    if (!m_vmm || !m_pid || !pattern || size == 0 || size > 64)
        return hits;
    if (g_motionBurst.load(std::memory_order_acquire) > 0)
        return hits;
    HeapScanScope nocache;
    if (maxResults == 0)
        maxResults = 1;
    if (align == 0)
        align = 1;

    struct Region {
        uintptr_t start = 0;
        uintptr_t end = 0;
        size_t bytes = 0;
        DWORD commit = 0;
    };
    struct Cursor {
        uint64_t needle = 0;
        size_t regionIdx = 0;
        uintptr_t va = 0;
        std::vector<Region> regions;
        std::chrono::steady_clock::time_point built{};
    };
    static Cursor cursors[8];

    uint64_t needle = 0;
    memcpy(&needle, pattern, (std::min)(size, sizeof(needle)));
    Cursor* cur = nullptr;
    for (auto& slot : cursors) {
        if (slot.needle == needle) {
            cur = &slot;
            break;
        }
    }
    if (!cur) {
        cur = &cursors[0];
        for (auto& slot : cursors) {
            if (slot.built < cur->built)
                cur = &slot;
        }
        *cur = {};
        cur->needle = needle;
    }

    const auto now = std::chrono::steady_clock::now();
    if (g_motionBurst.load(std::memory_order_acquire) > 0)
        return hits;
    if (cur->regions.empty() || now - cur->built > std::chrono::seconds(90)) {
        std::vector<Region> regions;
        PVMMDLL_MAP_VAD pVad = nullptr;
        DWORD totalVad = 0;
        {
            DmaScope lock;
            if (!VMMDLL_Map_GetVadU(m_vmm, m_pid, FALSE, &pVad) || !pVad)
                return hits;
        }
        totalVad = pVad->cMap;
        const uintptr_t ga0 = m_gameAssemblyBase;
        const uintptr_t ga1 = m_gameAssemblyBase && m_gameAssemblySize
            ? m_gameAssemblyBase + m_gameAssemblySize : 0;
        DWORD sparseDrop = 0;
        regions.reserve(64);
        auto already = [&](uintptr_t start) {
            for (const auto& r : regions) {
                if (r.start == start)
                    return true;
            }
            return false;
        };
        for (DWORD i = 0; i < pVad->cMap; ++i) {
            const auto& e = pVad->pMap[i];
            if (e.fImage || e.fFile || e.fStack || e.fTeb)
                continue;
            if (e.vaEnd <= e.vaStart)
                continue;
            const uintptr_t start = static_cast<uintptr_t>(e.vaStart);
            const uintptr_t end = static_cast<uintptr_t>(e.vaEnd);
            if (ga0 && ga1 && start < ga1 && end > ga0)
                continue;
            const size_t bytes = static_cast<size_t>(end - start);
            if (bytes < 0x1000 || bytes > 0x10000000ULL)
                continue;
            // Klass metadata lives near the hint. Game* instances live in the
            // GC, which is a different VAD. Do not prefer "near the klass".
            const uint64_t committed = static_cast<uint64_t>(e.CommitCharge) * 4096ull;
            if (gcOnly && !(e.fPrivateMemory || e.fHeap))
                continue;
            // Unity 6 GC sections are often 256KB–96MB. 128MB+ private VADs are Windows heaps.
            if (gcOnly && (bytes < 0x40000ULL || bytes > 0x8000000ULL))
                continue;
            if (gcOnly && committed * 4ull < bytes) {
                ++sparseDrop;
                continue;
            }
            if (!already(start))
                regions.push_back({ start, end, bytes, e.CommitCharge });
        }
        VMMDLL_MemFree(pVad);
        auto gcRank = [&](const Region& r) {
            const bool band = r.start >= 0x250000000ULL && r.start < 0x360000000ULL;
            int sizeRank = 4;
            if (r.bytes >= 0x400000ULL && r.bytes <= 0x1800000ULL) sizeRank = 0;
            else if (r.bytes > 0x1800000ULL && r.bytes <= 0x3000000ULL) sizeRank = 1;
            else if (r.bytes > 0x3000000ULL && r.bytes <= 0x6000000ULL) sizeRank = 2;
            else if (r.bytes >= 0x100000ULL && r.bytes < 0x400000ULL) sizeRank = 3;
            return band ? sizeRank : sizeRank + 5;
        };
        std::sort(regions.begin(), regions.end(), [&](const Region& a, const Region& b) {
            const int ra = gcRank(a);
            const int rb = gcRank(b);
            if (ra != rb)
                return ra < rb;
            if (a.commit != b.commit)
                return a.commit > b.commit;
            return a.bytes > b.bytes;
        });
        if (regions.size() > 120)
            regions.resize(120);
        const uintptr_t oldVa = cur->va;
        cur->regions = std::move(regions);
        cur->built = now;
        cur->regionIdx = 0;
        cur->va = cur->regions.empty() ? 0 : cur->regions[0].start;
        for (size_t i = 0; i < cur->regions.size(); ++i) {
            if (oldVa >= cur->regions[i].start && oldVa < cur->regions[i].end) {
                cur->regionIdx = i;
                cur->va = oldVa;
                break;
            }
        }
        static int loggedRegions = 0;
        if (loggedRegions < 8) {
            ++loggedRegions;
            Log("[*] heap VADs %zu of %u sparse-drop %u",
                cur->regions.size(), totalVad, sparseDrop);
            for (size_t i = 0; i < cur->regions.size() && i < 6; ++i)
                Log(" [%zu] 0x%llx %zuKB c=%u", i,
                    static_cast<unsigned long long>(cur->regions[i].start),
                    cur->regions[i].bytes / 1024, cur->regions[i].commit);
            Log("\n");
            std::cout << "[*] heap VADs " << cur->regions.size() << " of " << totalVad
                      << " sparse-drop " << sparseDrop;
            for (size_t i = 0; i < cur->regions.size() && i < 4; ++i)
                std::cout << " [" << i << "] 0x" << std::hex << cur->regions[i].start
                          << std::dec << "/" << (cur->regions[i].bytes / 1024) << "KB";
            std::cout << "\n" << std::flush;
        }
    }
    if (cur->regions.empty())
        return hits;

    hits.reserve(maxResults);
    // 256KB is one FPGA transaction that finishes in a few milliseconds.
    // A 2MB NOCACHE read cannot be cancelled, so the 120ms deadline never
    // ran and the position thread sat on the lock until SNAP hit ~1000ms.
    const size_t kWindow = timeoutMs >= 40 ? 0x80000ull : 0x40000ull;
    std::vector<uint8_t> buf(kWindow);
    if (timeoutMs <= 0)
        timeoutMs = gcOnly ? 12 : 8;
    if (timeoutMs > 250)
        timeoutMs = 250;
    const auto deadline = now + std::chrono::milliseconds(timeoutMs);
    size_t idx = cur->regionIdx;
    uintptr_t va = cur->va ? cur->va : cur->regions[idx].start;
    const size_t nreg = cur->regions.size();
    unsigned ok = 0;
    unsigned fail = 0;
    for (size_t step = 0; step < nreg && hits.size() < maxResults; ++step) {
        if (g_motionBurst.load(std::memory_order_acquire) > 0)
            break;
        if (std::chrono::steady_clock::now() >= deadline)
            break;
        if (idx >= nreg)
            idx = 0;
        const Region& region = cur->regions[idx];
        if (va < region.start || va >= region.end)
            va = region.start;
        if (align > 1)
            va = (va + (align - 1)) & ~(static_cast<uintptr_t>(align) - 1);
        while (va < region.end && hits.size() < maxResults) {
            if (g_motionBurst.load(std::memory_order_acquire) > 0)
                break;
            if (std::chrono::steady_clock::now() >= deadline)
                break;
            size_t chunk = (std::min)(kWindow, static_cast<size_t>(region.end - va));
            bool readOk = ReadRaw(va, buf.data(), chunk);
            if (!readOk && chunk > 0x1000) {
                chunk = 0x1000;
                readOk = ReadRaw(va, buf.data(), chunk);
            }
            if (readOk) {
                ++ok;
                if (size == 8 && align == 8) {
                    for (size_t off = 0; off + 8 <= chunk && hits.size() < maxResults; off += 8) {
                        uint64_t word = 0;
                        memcpy(&word, buf.data() + off, 8);
                        if (word == needle)
                            hits.push_back(va + off);
                    }
                } else {
                    for (size_t off = 0; off + size <= chunk && hits.size() < maxResults; off += align) {
                        if (memcmp(buf.data() + off, pattern, size) == 0)
                            hits.push_back(va + off);
                    }
                }
            } else {
                ++fail;
            }
            va += chunk;
        }
        if (va >= region.end) {
            std::cout << "[*] heap region " << idx << "/" << nreg
                      << " done hits=" << hits.size()
                      << " next=0x" << std::hex
                      << (nreg ? cur->regions[(idx + 1) % nreg].start : 0)
                      << std::dec << "\n" << std::flush;
            idx = (idx + 1) % nreg;
            va = cur->regions[idx].start;
        }
    }
    cur->regionIdx = idx;
    cur->va = va;
    static int scanLogs = 0;
    if (scanLogs < 10) {
        ++scanLogs;
        Log("[*] heap scan ok=%u fail=%u hits=%zu r=%zu va=0x%llx\n",
            ok, fail, hits.size(), idx, static_cast<unsigned long long>(va));
    }
    return hits;
}

bool Memory::ReadRaw(uintptr_t address, void* buffer, size_t size) {
    if (!m_vmm || !m_pid || !buffer || size == 0) return false;

    DmaScope lock;
    DWORD bytesRead = 0;
    return VMMDLL_MemReadEx(m_vmm, m_pid, address,
        static_cast<PBYTE>(buffer), static_cast<DWORD>(size),
        &bytesRead, ReadFlagsForThread());
}

std::string Memory::ReadString(uintptr_t address, size_t maxLen) {
    std::vector<char> buf(maxLen + 1, 0);
    if (!ReadRaw(address, buf.data(), maxLen)) return {};
    buf[maxLen] = '\0';
    return std::string(buf.data());
}

std::wstring Memory::ReadWString(uintptr_t address, size_t maxLen) {
    std::vector<wchar_t> buf(maxLen + 1, 0);
    if (!ReadRaw(address, buf.data(), maxLen * sizeof(wchar_t))) return {};
    buf[maxLen] = L'\0';
    return std::wstring(buf.data());
}

std::string Memory::ReadFString(uintptr_t address) {
    // UE FString: pointer to TCHAR array at +0x0, element count at +0x8
    uintptr_t dataPtr = Read<uintptr_t>(address);
    int32_t length = Read<int32_t>(address + 0x8);

    if (dataPtr == 0 || length <= 0 || length > 1024) return {};

    std::wstring ws = ReadWString(dataPtr, length);
    if (ws.empty()) return {};

    // Convert wstring to string (safe narrow conversion)
    std::string result;
    result.reserve(ws.size());
    for (wchar_t wc : ws) {
        result.push_back(static_cast<char>(wc & 0x7F));
    }
    return result;
}

// =============================================================================
// Memory writes
// =============================================================================

bool Memory::WriteRaw(uintptr_t address, void* buffer, size_t size) {
    if (!m_vmm || !m_pid || !buffer || size == 0) return false;
    DmaScope lock;
    return VMMDLL_MemWrite(m_vmm, m_pid, address,
        static_cast<PBYTE>(buffer), static_cast<DWORD>(size));
}

// =============================================================================
// Scatter operations
// =============================================================================

ScatterHandle Memory::CreateScatterHandle() {
    if (!m_vmm || !m_pid) return {};

    VMMDLL_SCATTER_HANDLE handle = VMMDLL_Scatter_Initialize(m_vmm, m_pid,
        VMMDLL_FLAG_NOCACHE | VMMDLL_FLAG_ZEROPAD_ON_FAIL |
        VMMDLL_FLAG_SCATTER_PREPAREEX_NOMEMZERO);

    if (!handle) return {};
    return ScatterHandle(handle, m_vmm, m_pid);
}

ScatterHandle& Memory::BorrowScatter() {
    thread_local ScatterHandle handle;
    if (!handle.IsValid()) {
        handle = CreateScatterHandle();
    } else {
        handle.Clear();
    }
    // Motion tick stays small and exclusive. Discovery/GOM must flush in
    // 64-read slices or SNAP jumps from 1ms to ~1000ms while the FPGA is busy.
    int chunk = std::clamp(Config::Get().general.scatterMaxPrepares, 32, 512);
    if (!t_dmaMotion)
        chunk = (std::min)(chunk, 64);
    else
        chunk = (std::min)(chunk, 256);
    handle.SetMaxQueued(chunk);
    return handle;
}

// =============================================================================
// Pointer chain
// =============================================================================

uintptr_t Memory::ResolvePointerChain(uintptr_t base, const std::vector<uintptr_t>& offsets) {
    uintptr_t current = base;
    for (size_t i = 0; i < offsets.size(); i++) {
        current = Read<uintptr_t>(current + offsets[i]);
        if (current == 0) return 0;
    }
    return current;
}

// =============================================================================
// Signature scanning
// =============================================================================

std::vector<uint8_t> Memory::PatternToBytes(const char* pattern) {
    std::vector<uint8_t> bytes;
    const char* cur = pattern;

    while (*cur) {
        if (*cur == ' ') { cur++; continue; }
        if (*cur == '?') {
            bytes.push_back(0x00);  // wildcard marker
            cur++;
            if (*cur == '?') cur++;
            continue;
        }
        // Parse hex byte
        uint8_t byte = 0;
        for (int i = 0; i < 2 && *cur; i++, cur++) {
            byte <<= 4;
            if (*cur >= '0' && *cur <= '9') byte |= (*cur - '0');
            else if (*cur >= 'A' && *cur <= 'F') byte |= (*cur - 'A' + 10);
            else if (*cur >= 'a' && *cur <= 'f') byte |= (*cur - 'a' + 10);
        }
        bytes.push_back(byte);
    }
    return bytes;
}

uintptr_t Memory::FindSignature(const char* pattern, uintptr_t regionStart, size_t regionSize) {
    auto patternBytes = PatternToBytes(pattern);
    if (patternBytes.empty()) return 0;

    // Build a wildcard mask from the original pattern string
    std::vector<bool> mask;
    const char* cur = pattern;
    while (*cur) {
        if (*cur == ' ') { cur++; continue; }
        if (*cur == '?') {
            mask.push_back(false);  // wildcard
            cur++;
            if (*cur == '?') cur++;
        } else {
            mask.push_back(true);   // must match
            cur++;
            if (*cur && *cur != ' ') cur++;
        }
    }

    constexpr size_t CHUNK_SIZE = 0x100000; // 1MB read chunks
    std::vector<uint8_t> buffer(CHUNK_SIZE);

    for (size_t offset = 0; offset < regionSize; offset += CHUNK_SIZE - patternBytes.size()) {
        size_t readSize = (std::min)(CHUNK_SIZE, regionSize - offset);
        if (!ReadRaw(regionStart + offset, buffer.data(), readSize))
            continue;

        for (size_t i = 0; i < readSize - patternBytes.size(); i++) {
            bool found = true;
            for (size_t j = 0; j < patternBytes.size(); j++) {
                if (mask[j] && buffer[i + j] != patternBytes[j]) {
                    found = false;
                    break;
                }
            }
            if (found) return regionStart + offset + i;
        }
    }
    return 0;
}

// =============================================================================
// CR3 / DTB fix
// =============================================================================

bool Memory::FixCr3() {
    if (!m_vmm) return false;

    std::cout << "[*] Running CR3/DTB fix...\n";

    // Wait for VMM info plugin to finish loading
    VMMDLL_ConfigSet(m_vmm,
        VMMDLL_OPT_CONFIG_IS_PAGING_ENABLED, 1);

    // Get the info/dtb plugin output which lists all known DTBs
    DWORD cbData = 0;
    BYTE dummy;
    VMMDLL_ProcessGetInformationString(m_vmm, m_pid, VMMDLL_PROCESS_INFORMATION_OPT_STRING_PATH_KERNEL);

    // Strategy: try all PIDs and their DTBs to find one that lets us
    // read the target process module
    PDWORD pPIDs = nullptr;
    SIZE_T cPIDs = 0;
    if (!VMMDLL_PidList(m_vmm, nullptr, &cPIDs) || cPIDs == 0) return false;

    std::vector<DWORD> pids(cPIDs);
    if (!VMMDLL_PidList(m_vmm, pids.data(), &cPIDs)) return false;

    for (DWORD testPid : pids) {
        VMMDLL_PROCESS_INFORMATION procInfo = {};
        procInfo.magic = VMMDLL_PROCESS_INFORMATION_MAGIC;
        procInfo.wVersion = VMMDLL_PROCESS_INFORMATION_VERSION;
        SIZE_T cbInfo = sizeof(procInfo);

        if (!VMMDLL_ProcessGetInformation(m_vmm, testPid, &procInfo, &cbInfo))
            continue;

        // Skip processes with no DTB
        if (procInfo.paDTB == 0) continue;

        // Try setting this DTB for our target PID
        VMMDLL_ConfigSet(m_vmm,
            VMMDLL_OPT_PROCESS_DTB | m_pid, procInfo.paDTB);

        // IL2CPP ESP needs GameAssembly mapped, not just the exe module.
        if (GetModuleBase("GameAssembly.dll") != 0 || DiscoverGameAssembly()) {
            m_baseAddress = GetModuleBase(m_processName);
            m_baseSize = GetModuleSize(m_processName);
            std::cout << "[+] CR3 fix succeeded with DTB: 0x"
                      << std::hex << procInfo.paDTB << std::dec
                      << " (GameAssembly visible)\n";
            return true;
        }
    }

    std::cerr << "[-] CR3 fix exhausted all DTBs without success\n";
    return false;
}

namespace {

bool ResolutionPair(int width, int height) {
    if (width < 800 || width > 7680) return false;
    if (height < 600 || height > 4320) return false;
    const float aspect = static_cast<float>(width) / static_cast<float>(height);
    return aspect > 1.2f && aspect < 3.2f;
}

bool StartsWith(const char* text, const char* prefix) {
    while (*prefix) {
        if (*text != *prefix) return false;
        ++text;
        ++prefix;
    }
    return true;
}

bool ReadResolutionValues(VMM_HANDLE vmm, const char* key, int& width, int& height) {
    int foundW = 0;
    int foundH = 0;
    for (DWORD index = 0; index < 64; ++index) {
        char name[256] = {};
        DWORD nameLen = sizeof(name);
        BYTE data[8] = {};
        DWORD dataLen = sizeof(data);
        DWORD type = 0;
        if (!VMMDLL_WinReg_EnumValueU(vmm, key, index, name, &nameLen, &type, data, &dataLen))
            break;
        if (type != REG_DWORD || dataLen < sizeof(DWORD))
            continue;
        const int value = static_cast<int>(*reinterpret_cast<DWORD*>(data));
        if (StartsWith(name, "Screenmanager Resolution Width"))
            foundW = value;
        else if (StartsWith(name, "Screenmanager Resolution Height"))
            foundH = value;
    }
    if (!ResolutionPair(foundW, foundH))
        return false;
    width = foundW;
    height = foundH;
    return true;
}

bool EnumName(VMM_HANDLE vmm, const char* key, DWORD index, char* name, DWORD nameBytes) {
    DWORD len = nameBytes;
    return VMMDLL_WinReg_EnumKeyExU(vmm, key, index, name, &len, nullptr) && name[0] != '\0';
}

} // namespace

bool Memory::ReadUnityResolution(int& width, int& height) {
    if (!m_vmm)
        return false;

    const char* direct[] = {
        "HKCU\\Software\\Kefir\\ROB RUN",
        "HKCU\\Software\\Kefir\\Rob Run",
        "HKCU\\Software\\Kefir\\ROBRUN",
        "HKCU\\Software\\kefir\\ROB RUN",
        "HKCU\\Software\\DefaultCompany\\ROB RUN",
    };
    for (const char* key : direct) {
        if (ReadResolutionValues(m_vmm, key, width, height))
            return true;
    }

    const char* companies[] = { "Kefir", "kefir", "KEFIR", "DefaultCompany" };
    for (DWORD userIndex = 0; userIndex < 16; ++userIndex) {
        char sid[128] = {};
        if (!EnumName(m_vmm, "HKU", userIndex, sid, sizeof(sid)))
            break;
        if (sid[0] == '.')
            continue;
        for (const char* company : companies) {
            char companyKey[320] = {};
            std::snprintf(companyKey, sizeof(companyKey), "HKU\\%s\\Software\\%s", sid, company);
            if (ReadResolutionValues(m_vmm, companyKey, width, height))
                return true;
            for (DWORD productIndex = 0; productIndex < 16; ++productIndex) {
                char product[128] = {};
                if (!EnumName(m_vmm, companyKey, productIndex, product, sizeof(product)))
                    break;
                char productKey[460] = {};
                std::snprintf(productKey, sizeof(productKey), "%s\\%s", companyKey, product);
                if (ReadResolutionValues(m_vmm, productKey, width, height))
                    return true;
            }
        }
    }
    return false;
}
