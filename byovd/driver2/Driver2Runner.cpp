// driver2 — vehicle + mapper + Driver2.sys (classic BYOVD stack)
// Build: driver2\build.ps1

#include "betest_ioctl.h"
#include "sdk/driver2_api.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <Shlwapi.h>

#pragma comment(lib, "shlwapi.lib")

static bool IsAdmin()
{
    BOOL admin = FALSE;
    PSID grp = nullptr;
    SID_IDENTIFIER_AUTHORITY auth = SECURITY_NT_AUTHORITY;
    if (!AllocateAndInitializeSid(&auth, 2, SECURITY_BUILTIN_DOMAIN_RID,
            DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &grp))
        return false;
    CheckTokenMembership(nullptr, grp, &admin);
    FreeSid(grp);
    return admin != FALSE;
}

static std::string DirExist(const char* path)
{
    if (path && *path && PathIsDirectoryA(path))
        return path;
    return {};
}

static bool HasMapper(const std::string& root)
{
    if (PathFileExistsA((root + "\\bin\\mapper_nal.exe").c_str()))
        return true;
    return PathFileExistsA((root + "\\driver2\\bin\\mapper_nal.exe").c_str()) != FALSE;
}

static std::string FindByovdRoot()
{
    if (const char* env = std::getenv("BYOVD_ROOT")) {
        std::string d = DirExist(env);
        if (!d.empty() && HasMapper(d)) return d;
    }
    static const char* kFixed[] = {
        "D:\\hostdev\\byovd",
        "C:\\Development\\byovd",
    };
    for (auto p : kFixed) {
        std::string d = DirExist(p);
        if (!d.empty() && HasMapper(d)) return d;
    }
    char exe[MAX_PATH]{};
    GetModuleFileNameA(nullptr, exe, MAX_PATH);
    PathRemoveFileSpecA(exe);
    char canon[MAX_PATH]{};
    const char* rels[] = { "..\\..", "..", "..\\..", "..\\.." };
    for (auto rel : rels) {
        snprintf(canon, sizeof(canon), "%s\\%s", exe, rel);
        char full[MAX_PATH]{};
        if (!PathCanonicalizeA(full, canon)) continue;
        std::string d(full);
        if (HasMapper(d)) return d;
    }
    return {};
}

static bool FileExist(const std::string& path)
{
    return PathFileExistsA(path.c_str()) != FALSE;
}

struct VehiclePick {
    const char* sys;
    const char* driver;
};

// TRIXX omitted — never auto/ fallback (BSOD on Win11). Use --driver trixx to force.
static const VehiclePick kVehicles[] = {
    { "RTCore64.sys", "rtcore" },
    { "CorsairLLAccess64.sys", "corsair" },
    { "ThrottleStop.sys", "ts" },
    { "SIVX64.sys", "siv" },
    { "NalDrvIntel.sys", "nal" },
    { "NalDrv.sys", "nal" },
    { "iqvw64e.sys", "nal" },
};

static bool AutoVehicleAllowed(const VehiclePick& v)
{
    return _stricmp(v.driver, "nal") != 0;
}

static bool VehiclePresent(const std::string& byovd, const char* name)
{
    return FileExist(byovd + "\\vehicles\\" + name) || FileExist(byovd + "\\bin\\" + name);
}

static void CopyVehicle(const std::string& byovd)
{
    std::string bin = byovd + "\\bin";
    for (const auto& v : kVehicles) {
        std::string src = byovd + "\\vehicles\\" + v.sys;
        if (!FileExist(src)) continue;
        std::string dst = bin + "\\" + v.sys;
        if (!CopyFileA(src.c_str(), dst.c_str(), FALSE)) {
            char tmp[MAX_PATH];
            GetTempFileNameA(bin.c_str(), "veh", 0, tmp);
            if (CopyFileA(src.c_str(), tmp, FALSE))
                printf("[*] bin\\%s locked — staged %s\n", v.sys, tmp);
        }
    }
}

static int RunMapper(const std::string& byovd, const char* profile, const char* driver,
    const char* mode, bool forceMap, bool inspector);

static int RunMapperAuto(const std::string& byovd, const char* profile, const char* mode,
    bool forceMap, bool inspector)
{
    int last = 1;
    printf("[*] auto: RTCore first; TRIXX disabled; Nal skipped\n");
    for (const auto& v : kVehicles) {
        if (!VehiclePresent(byovd, v.sys)) continue;
        if (!AutoVehicleAllowed(v)) {
            printf("[*] Skipping %s in auto (use --driver %s to force)\n", v.sys, v.driver);
            continue;
        }
        printf("[*] Vehicle candidate: %s (--driver %s)\n", v.sys, v.driver);
        last = RunMapper(byovd, profile, v.driver, mode, forceMap, inspector);
        if (last == 0) return 0;
        printf("[!] %s map failed (exit %d) — trying next installed vehicle\n", v.sys, last);
    }
    printf("[!] No vehicle succeeded. Run vehicles\\fetch-vehicle.ps1 (MSI Afterburner, iCUE, etc.)\n");
    return last;
}

static std::string ResolvePayload(const std::string& byovd, const char* mode)
{
    if (!strcmp(mode, "stomp")) {
        std::string st = byovd + "\\driver2\\bin\\Driver2Stomp.sys";
        if (FileExist(st)) return st;
        st = byovd + "\\bin\\Driver2Stomp.sys";
        if (FileExist(st)) return st;
        return byovd + "\\driver2\\bin\\Driver2Stomp.sys";
    }
    if (!strcmp(mode, "prove"))
        return byovd + "\\bin\\RwTest.sys";
    std::string d2 = byovd + "\\driver2\\bin\\Driver2.sys";
    if (FileExist(d2)) return d2;
    d2 = byovd + "\\bin\\Driver2.sys";
    if (FileExist(d2)) return d2;
    return byovd + "\\bin\\Betest.sys";
}

static void AppendInspectorFlags(std::string& cmd, bool inspector)
{
    if (!inspector) return;
    cmd += " --ac-safe --stealth 3f";
}

static const char* NormalizeProfile(const char* profile)
{
    if (!profile || !profile[0]) return "rust";
    if (!_stricmp(profile, "be") || !_stricmp(profile, "be-r6") || !_stricmp(profile, "r6s") ||
        !_stricmp(profile, "siege") || !_stricmp(profile, "battleye"))
        return "r6";
    if (!_stricmp(profile, "be-ark") || !_stricmp(profile, "ark") || !_stricmp(profile, "be-arksa"))
        return "arksa";
    if (!_stricmp(profile, "eac") || !_stricmp(profile, "eac-rust") || !_stricmp(profile, "eos") ||
        !_stricmp(profile, "fortnite"))
        return "rust";
    if (!_stricmp(profile, "robrun") || !_stricmp(profile, "rob-run") || !_stricmp(profile, "rob") ||
        !_stricmp(profile, "robrun-eac"))
        return "robrun";
    if (!_stricmp(profile, "universal") || !_stricmp(profile, "all") || !_stricmp(profile, "any"))
        return "universal";
    if (!_stricmp(profile, "ace") || !_stricmp(profile, "delta") || !_stricmp(profile, "deltaforce") ||
        !_stricmp(profile, "ace-deltaforce") || !_stricmp(profile, "df"))
        return "deltaforce";
    if (!_stricmp(profile, "abi") || !_stricmp(profile, "arena") || !_stricmp(profile, "ace-arena") ||
        !_stricmp(profile, "ace-arenabreakout") || !_stricmp(profile, "arenabreakout-infinite"))
        return "arenabreakout";
    return profile;
}

static bool IsAceProfile(const char* profile)
{
    return !_stricmp(profile, "deltaforce") || !_stricmp(profile, "arenabreakout");
}

static bool IsEacProfile(const char* profile)
{
    return !_stricmp(profile, "rust") || !_stricmp(profile, "robrun");
}

static bool IsBattleyeProfile(const char* profile)
{
    return !_stricmp(profile, "r6") || !_stricmp(profile, "arksa");
}

static void AppendTitleAcFlags(std::string& cmd, const char* profile, const char* mode)
{
    if (!strcmp(mode, "prove")) return;
    if (IsBattleyeProfile(profile))
        cmd += " --battleye 1f";
    else if (IsEacProfile(profile))
        cmd += " --eac 7f --encptr f";
    else if (!_stricmp(profile, "deltaforce"))
        cmd += " --ace 3f --delta 1f --ace-vgk";
    else if (!_stricmp(profile, "arenabreakout"))
        cmd += " --ace 3f --ace-vgk";
    else if (!_stricmp(profile, "universal"))
        cmd += " --eac 7f --encptr f --battleye 1f --ace 3f --ace-vgk";
}

static const char* VictimDosA(const char* victimShort)
{
    static const struct { const char* key; const char* dos; } k[] = {
        { "dam", "\\\\.\\Dam" },
        { "luafv", "\\\\.\\Luafv" },
        { "wcifs", "\\\\.\\WCIFS" },
    };
    if (!victimShort) return nullptr;
    for (const auto& e : k) {
        if (!_stricmp(e.key, victimShort)) return e.dos;
    }
    static char guess[32];
    snprintf(guess, sizeof(guess), "\\\\.\\%s", victimShort);
    if (guess[4]) guess[4] = (char)toupper((unsigned char)guess[4]);
    return guess;
}

static int RunStompMapper(const std::string& byovd, const char* driver, const char* victim,
    bool dryRun, bool forceMap)
{
    std::string mapper = byovd + "\\driver2\\bin\\mapper_nal.exe";
    if (!FileExist(mapper)) mapper = byovd + "\\bin\\mapper_nal.exe";
    std::string payload = ResolvePayload(byovd, "stomp");
    if (!FileExist(mapper) || !FileExist(payload)) {
        printf("[!] Missing mapper or Driver2Stomp.sys — run driver2\\build.ps1\n");
        return 10;
    }
    CopyVehicle(byovd);
    std::string cmd = "\"" + mapper + "\" \"" + payload + "\" --stomp --victim " + victim;
    cmd += " --driver " + std::string(driver);
    cmd += " --no-auth --quiet";
    if (dryRun) cmd += " --stomp-dry-run";
    if (forceMap) cmd += " --force-map";
    cmd += " --no-fallback";
    printf("[*] stomp map: %s\n", cmd.c_str());
    char cmdline[2048];
    snprintf(cmdline, sizeof(cmdline), "%s", cmd.c_str());
    STARTUPINFOA si{};
    PROCESS_INFORMATION pi{};
    si.cb = sizeof(si);
    std::string cwd = byovd + "\\driver2\\bin";
    if (!FileExist(cwd + "\\mapper_nal.exe")) cwd = byovd + "\\bin";
    if (!CreateProcessA(nullptr, cmdline, nullptr, nullptr, FALSE, 0, nullptr, cwd.c_str(), &si, &pi)) {
        printf("[!] CreateProcess failed err=%lu\n", GetLastError());
        return 11;
    }
    WaitForSingleObject(pi.hProcess, 300000);
    DWORD ec = 1;
    GetExitCodeProcess(pi.hProcess, &ec);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return (int)ec;
}

static int RunMapper(const std::string& byovd, const char* profile, const char* driver,
    const char* mode, bool forceMap, bool inspector)
{
    std::string mapper = byovd + "\\driver2\\bin\\mapper_nal.exe";
    if (!FileExist(mapper))
        mapper = byovd + "\\bin\\mapper_nal.exe";
    std::string payload = ResolvePayload(byovd, mode);
    if (!FileExist(mapper) || !FileExist(payload)) {
        printf("[!] Missing mapper or payload (%s)\nRun driver2\\build.ps1\n", payload.c_str());
        return 10;
    }
    CopyVehicle(byovd);

    std::string cmd = "\"" + mapper + "\" \"" + payload + "\" --driver " + driver;
    if (!strcmp(mode, "prove")) {
        cmd += " --no-auth --ac-safe --ac-replica ";
        cmd += profile;
    } else if (!strcmp(mode, "ioctl")) {
        cmd += " --no-auth --driver2 --stealth 3f --ac-replica ";
        cmd += profile;
        cmd += " --ac-replica-ioctl";
    } else if (!strcmp(mode, "session")) {
        /* Host lab (Win11 25H2): map Driver2 only. No AC IOCTL spray (0xEF/0x50). */
        cmd += " --no-auth --driver2 --ac-safe --quiet --ac-replica ";
        cmd += profile;
        if (forceMap) cmd += " --force-map";
    } else {
        cmd += " --no-auth --driver2 --ac-replica ";
        cmd += profile;
        cmd += " --ac-replica-bypass";
        if (forceMap) cmd += " --force-map";
        AppendInspectorFlags(cmd, inspector);
        AppendTitleAcFlags(cmd, profile, mode);
    }
    if (!strcmp(mode, "ioctl")) {
        AppendTitleAcFlags(cmd, profile, mode);
    }
  /* Never hop to TRIXX/Nal mid-map if RTCore IOCTL flakes once */
    if (_stricmp(driver, "auto") != 0)
        cmd += " --no-fallback";

    printf("[*] driver2 map (%s): %s\n", mode, cmd.c_str());
    char cmdline[2048];
    if (cmd.size() >= sizeof(cmdline)) {
        printf("[!] command too long\n");
        return 12;
    }
    snprintf(cmdline, sizeof(cmdline), "%s", cmd.c_str());

    STARTUPINFOA si{};
    PROCESS_INFORMATION pi{};
    si.cb = sizeof(si);
    std::string cwd = byovd + "\\driver2\\bin";
    if (!FileExist(cwd + "\\mapper_nal.exe"))
        cwd = byovd + "\\bin";
    if (!CreateProcessA(nullptr, cmdline, nullptr, nullptr, FALSE, 0, nullptr, cwd.c_str(), &si, &pi)) {
        printf("[!] CreateProcess failed err=%lu\n", GetLastError());
        return 11;
    }
    WaitForSingleObject(pi.hProcess, 300000);
    DWORD ec = 1;
    GetExitCodeProcess(pi.hProcess, &ec);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    if (ec != 0) {
        printf("[!] mapper exit %lu\n", (unsigned long)ec);
        return (int)ec;
    }
    printf("[+] Map completed\n");
    return 0;
}

static int ProveRead()
{
    HANDLE h = INVALID_HANDLE_VALUE;
    for (int i = 0; i < 24; i++) {
        h = BetestOpenDevice();
        if (h != INVALID_HANDLE_VALUE) break;
        Sleep(250);
    }
    if (h == INVALID_HANDLE_VALUE) {
        printf("[!] Device not found err=%lu (after retries)\n", GetLastError());
        return 20;
    }
    if (!BetestSessionAuth(h)) {
        printf("[!] Session auth failed err=%lu\n", GetLastError());
        CloseHandle(h);
        return 21;
    }
    volatile uint64_t marker = 0x1122334455667788ULL;
    uint64_t got = 0;
    if (!BetestReadProcess(h, GetCurrentProcessId(), (uint64_t)(uintptr_t)&marker, &got, (uint32_t)sizeof(got))) {
        printf("[!] Read failed err=%lu\n", GetLastError());
        CloseHandle(h);
        return 22;
    }
    printf("[+] read 0x%llx (expect 0x1122334455667788)\n", (unsigned long long)got);
    CloseHandle(h);
    return (got == marker) ? 0 : 23;
}

static void Usage()
{
    printf("Driver2Runner — classic BYOVD stack (driver2\\Driver2.sys)\n\n");
    printf("  Driver2Runner.exe [--profile rust|r6|deltaforce|arenabreakout|universal|...]\n");
    printf("            ACE: ace, delta, df, abi, arena  |  EAC: eac, eos  |  BE: be-r6, siege\n");
    printf("                    [--mode prove|ioctl|session|bypass|stomp]  (default: bypass)\n");
    printf("  stomp: --victim dam|luafv|wcifs  [--dry-run]  (Driver2Stomp.sys + victim device)\n");
    printf("  session: ranked-safe ioctl map (--ac-safe, no bypass sweep)\n");
    printf("  Full sequence: driver2\\everything.ps1 -Execute\n");
    printf("                    [--driver auto|rtcore|nal|corsair|...]  (default: auto)\n");
    printf("                    [--force-map] [--inspector] [--skip-rw]\n");
    printf("  --inspector: Tier 1 lab map (--ac-safe --stealth 3f) for cheat-inspector gate\n\n");
    printf("Docs: driver2\\README.md\n");
}

int main(int argc, char** argv)
{
    const char* profile = "rust";
    const char* driver = "rtcore";
    const char* mode = "bypass";
    const char* victim = "dam";
    bool forceMap = false;
    bool inspector = false;
    bool skipRw = false;
    bool dryRun = false;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--profile") && i + 1 < argc) {
            profile = NormalizeProfile(argv[++i]);
            continue;
        }
        if (!strcmp(argv[i], "--driver") && i + 1 < argc) { driver = argv[++i]; continue; }
        if (!strcmp(argv[i], "--mode") && i + 1 < argc) { mode = argv[++i]; continue; }
        if (!strcmp(argv[i], "--force-map")) { forceMap = true; continue; }
        if (!strcmp(argv[i], "--inspector")) { inspector = true; continue; }
        if (!strcmp(argv[i], "--skip-rw")) { skipRw = true; continue; }
        if (!strcmp(argv[i], "--victim") && i + 1 < argc) { victim = argv[++i]; continue; }
        if (!strcmp(argv[i], "--dry-run")) { dryRun = true; continue; }
        if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) { Usage(); return 0; }
    }

    if (strcmp(mode, "prove") && strcmp(mode, "ioctl") && strcmp(mode, "session") &&
        strcmp(mode, "bypass") && strcmp(mode, "stomp")) {
        printf("[!] Invalid --mode %s\n", mode);
        return 4;
    }

    if (!IsAdmin()) {
        printf("[!] Run as Administrator\n");
        return 5;
    }

    std::string byovd = FindByovdRoot();
    if (byovd.empty()) {
        printf("[!] byovd root not found (BYOVD_ROOT)\n");
        return 6;
    }
    profile = NormalizeProfile(profile);
    const char* acTag = "";
    if (IsBattleyeProfile(profile)) acTag = " (BattlEye)";
    else if (IsEacProfile(profile)) acTag = " (EAC EOS)";
    else if (IsAceProfile(profile)) acTag = " (ACE)";
    printf("[*] byovd=%s profile=%s mode=%s driver=%s%s\n", byovd.c_str(), profile, mode, driver, acTag);

    if (!strcmp(mode, "stomp")) {
        int mr = RunStompMapper(byovd, driver, victim, dryRun, forceMap);
        if (mr != 0) return mr;
        if (dryRun || skipRw) return 0;
        const char* dos = VictimDosA(victim);
        if (dos) BetestSetDeviceOverride(dos);
        Sleep(500);
        return ProveRead();
    }

    int mr = !_stricmp(driver, "auto")
        ? RunMapperAuto(byovd, profile, mode, forceMap, inspector)
        : RunMapper(byovd, profile, driver, mode, forceMap, inspector);
    if (mr != 0) return mr;
    Sleep(1500);

    if (skipRw) {
        printf("[*] --skip-rw: map only\n");
        return 0;
    }
    return ProveRead();
}
