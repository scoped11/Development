// Driver3Setup.exe — Host A setup + cheat-dev orientation (Lane D split-trust).
// Build: driver3\setup-tool\build.ps1

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <shellapi.h>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static std::wstring gDriver3Root;
static std::wstring gByovdRoot;
static std::wstring gDevRoot;
static bool gAuto = false;
static bool gExplainOnly = false;

static void SetColor(WORD attr)
{
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    if (h != INVALID_HANDLE_VALUE)
        SetConsoleTextAttribute(h, attr);
}

static void ResetColor()
{
    SetColor(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
}

static void PrintBanner()
{
    SetColor(FOREGROUND_GREEN | FOREGROUND_INTENSITY);
    printf(
        "\n"
        "  ========================================================\n"
        "   Driver3 Setup (Lane D — split-trust calibration)\n"
        "   Host A = this PC (live AC session, DMA reads)\n"
        "   Host B = DriverLab VM (Driver2 kernel map)\n"
        "  ========================================================\n");
    ResetColor();
    wprintf(L"  Root: %s\n\n", gDriver3Root.c_str());
}

static bool RunPowerShellFile(const wchar_t* scriptRel, const wchar_t* extraArgs = L"")
{
    std::wstring script = gDriver3Root + L"\\" + scriptRel;
    if (!fs::exists(script)) {
        wprintf(L"[!] Missing script: %s\n", script.c_str());
        return false;
    }
    std::wstring cmd = L"powershell.exe -NoProfile -ExecutionPolicy Bypass -File \"";
    cmd += script;
    cmd += L"\"";
    if (extraArgs && extraArgs[0])
        cmd = cmd + L" " + extraArgs;

    wprintf(L"[*] Running: %s\n", scriptRel);
    std::wstring line = cmd;

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    std::vector<wchar_t> buf(line.begin(), line.end());
    buf.push_back(L'\0');

    if (!CreateProcessW(nullptr, buf.data(), nullptr, nullptr, FALSE, 0, nullptr,
                        gDriver3Root.c_str(), &si, &pi)) {
        printf("[!] CreateProcess failed %lu\n", GetLastError());
        return false;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD ec = 1;
    GetExitCodeProcess(pi.hProcess, &ec);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    printf("[*] Exit code: %lu\n", (unsigned long)ec);
    return ec == 0;
}

static void PrintWhatSetupDoes()
{
    SetColor(FOREGROUND_BLUE | FOREGROUND_INTENSITY);
    printf(
        "\n--- What this setup does on THIS PC ---\n");
    ResetColor();
    printf(
        "  1. Sync build_id from your AC dumps (driver2/dumps/*) into lab_manifest.json\n"
        "  2. Run lane gates for each title (EAC / BE / ACE) — confirms Tier 0 on this host\n"
        "  3. Refresh Scale A dashboard and developer respect score\n"
        "  4. Validate dma offsets vs dumps/<date>/dump.cs when Python is available\n"
        "  5. Does NOT map Driver2 on this session while live AC is on (ranked fail-closed)\n\n"
        "--- What the VM must do separately (Host B) ---\n"
        "  - Map Driver2 (BYOVD) and prove IOCTL read path\n"
        "  - Output: vm_hostb_execute.log, kernel_proof_*.json on Z:\\byovd\\driver3\n"
        "  - See docs\\VM_GUEST_READ_WRITE.md (run Z:\\byovd\\driver3\\setup-vm-driver3.ps1 in guest)\n\n");
}

static void PrintCheatDeveloperGuide()
{
    SetColor(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
    printf(
        "\n"
        "================================================================\n"
        "  CHEAT DEVELOPER — how driver3 fits your project\n"
        "================================================================\n");
    ResetColor();

    printf(
        "\n"
        "ARCHITECTURE (two trust boundaries, one product):\n"
        "\n"
        "  [Host A — game PC, live AC]          [Host B — VM, no live AC on map]\n"
        "   Tier 0: no BYOVD map here            Driver2.sys + IOCTL session\n"
        "   Reads: dma/RobRunDMA (FPGA fpga://)    Proof + lab R/W: Driver2Client\n"
        "   Offsets: dumps + offsets.h           Profiles: rust, r6, ACE titles\n"
        "              \\________ build_id ________/  (docs/lab_manifest.json)\n"
        "\n"
        "IF YOU ARE WRITING A CHEAT:\n"
        "\n"
        "  A) Ranked / live session (Rust, R6, ABI, etc. with AC running):\n"
        "     - Your cheat runs HERE (Host A) as usermode overlay / external.\n"
        "     - Memory: physical DMA read (LeechCore/VMMDLL), NOT Driver2 on this OS.\n"
        "     - You need: FPGA hardware, RobRunDMA build, offsets for THIS game build.\n"
        "     - After patch: refresh dump -> patch-build-ids.ps1 -> rebuild dma.\n"
        "     - Commands:\n"
        "         driver3\\ranked.ps1 -Game <profile> -Execute -SyncBuildId\n"
        "         dma\\scripts\\build-windows.ps1\n"
        "\n"
        "  B) Kernel integration / IOCTL dev (lab, VM, or no-AC test host):\n"
        "     - Map on Host B only:\n"
        "         driver2\\Driver2Runner.exe --profile <title> --mode bypass --force-map\n"
        "     - In your C++ cheat:\n"
        "         #include \"sdk/Driver2Session.hpp\"\n"
        "         Driver2Session drv;\n"
        "         drv.Connect();  // after map\n"
        "         drv.ReadProcess(gamePid, virtualAddress, buffer, size);  // IOCTL 0x805 path\n"
        "     - See: driver2\\examples\\cheat_dev_minimal.cpp, DEVELOPER_SDK.md\n"
        "\n"
        "  C) What YOU must keep in sync (per title in games.json):\n"
        "     - driver2/dumps/<capture>/manifest.json  ->  lane_d.by_profile.<name>.build_id\n"
        "     - driver2/profiles/<ac-profile>.json     ->  map-time EAC/BE/ACE hooks\n"
        "     - dma/src/engine/offsets.h               ->  Il2Cpp/dump RVAs (Host A)\n"
        "     - drivers/<profile>.md                   ->  route card (tier, IOCTL list)\n"
        "\n"
        "  D) What driver3 does NOT claim:\n"
        "     - Undetected live ranked\n"
        "     - Kernel map on the same Windows instance as live EAC (D4)\n"
        "     - DMA without FPGA card (dma/ uses fpga://)\n"
        "\n"
        "  E) Typical dev loop:\n"
        "     1. Capture / sync AC dump -> patch-build-ids.ps1 -AllTitles\n"
        "     2. VM: host-b.ps1 -Profile <game> -Execute -ForceMap (kernel proof)\n"
        "     3. Host: ranked preflight + dma build (live read path)\n"
        "     4. Your feature code uses correct host A or B read API above\n"
        "     5. developer-nod.ps1 -AllTitles / complete-100-percent.ps1 (shop score)\n"
        "\n"
        "  Docs (full detail):\n"
        "     driver3\\docs\\HOST_PC_READ_WRITE.md\n"
        "     driver3\\docs\\VM_GUEST_READ_WRITE.md\n"
        "     driver3\\docs\\REACH_100_PERCENT.md\n"
        "\n");
}

static bool DetectRoots()
{
    wchar_t exePath[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    fs::path p = fs::path(exePath).parent_path();
    if (p.filename() == L"bin")
        p = p.parent_path();

    for (int i = 0; i < 6; ++i) {
        if (fs::exists(p / "setup-host-driver3.ps1")) {
            gDriver3Root = p.wstring();
            gByovdRoot = p.parent_path().wstring();
            gDevRoot = p.parent_path().parent_path().wstring();
            return true;
        }
        if (p.has_parent_path())
            p = p.parent_path();
        else
            break;
    }
    return false;
}

static void RunAutoSetup()
{
    PrintWhatSetupDoes();
    printf("[*] Phase: sync build IDs from dumps...\n");
    RunPowerShellFile(L"patch-build-ids.ps1");
    printf("[*] Phase: Host A setup (gates, dashboard)...\n");
    RunPowerShellFile(L"setup-host-driver3.ps1");
    printf("[*] Phase: complete Scale A (static SDK bar)...\n");
    RunPowerShellFile(L"complete-100-percent.ps1", L"-AllowStaticApproval -SkipVm");
    printf("[*] Phase: optional VM Host B (if running)...\n");
    std::wstring probe = L"-NoProfile -ExecutionPolicy Bypass -Command \"";
    probe += L"$v='C:\\Program Files\\Oracle\\VirtualBox\\VBoxManage.exe';";
    probe += L"if(Test-Path $v){ $s=& $v showvminfo DriverLab-E-26200 --machinereadable 2>$null;";
    probe += L"if($s -match 'VMState=\"running\"'){ exit 0 } }; exit 1\"";
    // lightweight VM check via one powershell
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    std::vector<wchar_t> pbuf(probe.begin(), probe.end());
    pbuf.push_back(L'\0');
    if (CreateProcessW(nullptr, pbuf.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                      nullptr, nullptr, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 5000);
        DWORD ec = 1;
        GetExitCodeProcess(pi.hProcess, &ec);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        if (ec == 0) {
            printf("[*] VM running — queuing Host B rust proof (needs guest admin once)...\n");
            RunPowerShellFile(L"vm-guest-hostb-from-host.ps1",
                              L"-Profile rust -ForceMap -SkipProve");
            RunPowerShellFile(L"sync-guest-artifacts.ps1", L"-AllKernelProofs");
            RunPowerShellFile(L"sync-kernel-proof-from-artifacts.ps1", L"-Primary rust");
        } else {
            printf("[*] VM not running — skip Host B; use open-vm-rdp.ps1 when ready.\n");
        }
    }
    RunPowerShellFile(L"build-s-tier.ps1", L"-AllTitles");
    PrintCheatDeveloperGuide();
}

static void InteractiveMenu()
{
    for (;;) {
        PrintBanner();
        printf(
            "  [1] Run full Host A setup (recommended)\n"
            "  [2] Explain cheat integration (print guide)\n"
            "  [3] Open Host A read/write doc\n"
            "  [4] Open VM guest read/write doc\n"
            "  [5] Launch RDP to DriverLab VM\n"
            "  [6] Run complete-100-percent (Scale A + try VM)\n"
            "  [0] Exit\n\n"
            "Choice: ");
        int c = -1;
        if (scanf_s("%d", &c) != 1) {
            while (getchar() != '\n') {}
            continue;
        }
        while (getchar() != '\n') {}

        switch (c) {
        case 0:
            return;
        case 1:
            PrintWhatSetupDoes();
            RunPowerShellFile(L"patch-build-ids.ps1");
            RunPowerShellFile(L"setup-host-driver3.ps1");
            break;
        case 2:
            PrintCheatDeveloperGuide();
            printf("\nPress Enter...");
            getchar();
            break;
        case 3:
            ShellExecuteW(nullptr, L"open",
                          (gDriver3Root + L"\\docs\\HOST_PC_READ_WRITE.md").c_str(),
                          nullptr, nullptr, SW_SHOW);
            break;
        case 4:
            ShellExecuteW(nullptr, L"open",
                          (gDriver3Root + L"\\docs\\VM_GUEST_READ_WRITE.md").c_str(),
                          nullptr, nullptr, SW_SHOW);
            break;
        case 5:
            RunPowerShellFile(L"open-vm-rdp.ps1");
            break;
        case 6:
            RunPowerShellFile(L"complete-100-percent.ps1", L"-AllowStaticApproval");
            break;
        default:
            printf("Invalid.\n");
        }
    }
}

static void ParseArgs(int argc, wchar_t** argv)
{
    for (int i = 1; i < argc; ++i) {
        if (_wcsicmp(argv[i], L"--auto") == 0 || _wcsicmp(argv[i], L"-auto") == 0)
            gAuto = true;
        if (_wcsicmp(argv[i], L"--explain") == 0 || _wcsicmp(argv[i], L"-explain") == 0)
            gExplainOnly = true;
        if (_wcsicmp(argv[i], L"--help") == 0 || _wcsicmp(argv[i], L"-?") == 0) {
            printf("Driver3Setup.exe [--auto] [--explain]\n");
            printf("  --auto     Run Host A setup pipeline without menu\n");
            printf("  --explain  Print cheat developer guide only\n");
            exit(0);
        }
    }
}

int wmain(int argc, wchar_t** argv)
{
    ParseArgs(argc, argv);
    if (!DetectRoots()) {
        printf("[!] Could not find driver3 folder (place Driver3Setup.exe in driver3\\ or driver3\\bin\\)\n");
        return 1;
    }

    if (gExplainOnly) {
        PrintCheatDeveloperGuide();
        return 0;
    }
    if (gAuto) {
        RunAutoSetup();
        return 0;
    }
    InteractiveMenu();
    return 0;
}
