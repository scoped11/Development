// HijackDriver — map RwTest (prove) then session auth + process read marker.
// Build: driver-lab\vm\hijack-driver\build.ps1
// Docs:  driver-lab\vm\hijack-driver\README.md
// Guest: D:\hostdev\driver-lab\vm\bin\HijackDriver.exe [--profile rust|r6|...] [--map] [--skip-map]

#include "betest_ioctl.h"
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

static std::string FindByovdRoot()
{
    if (const char* env = std::getenv("BYOVD_ROOT")) {
        std::string d = DirExist(env);
        if (!d.empty()) return d;
    }
    static const char* kFixed[] = {
        "D:\\hostdev\\byovd",
        "C:\\Users\\baile\\OneDrive\\Desktop\\Development\\byovd",
    };
    for (auto p : kFixed) {
        std::string d = DirExist(p);
        if (!d.empty()) return d;
    }
    char exe[MAX_PATH]{};
    GetModuleFileNameA(nullptr, exe, MAX_PATH);
    PathRemoveFileSpecA(exe);
    char up[MAX_PATH]{};
    snprintf(up, sizeof(up), "%s\\..\\..\\byovd", exe);
    char canon[MAX_PATH]{};
    if (PathCanonicalizeA(canon, up) && PathIsDirectoryA(canon))
        return canon;
    snprintf(up, sizeof(up), "%s\\..\\..\\..\\byovd", exe);
    if (PathCanonicalizeA(canon, up) && PathIsDirectoryA(canon))
        return canon;
    return {};
}

static bool FileExist(const std::string& path)
{
    return PathFileExistsA(path.c_str()) != FALSE;
}

static void CopyVehicle(const std::string& byovd)
{
    static const char* names[] = { "RTCore64.sys", "CorsairLLAccess64.sys", "NalDrvIntel.sys" };
    std::string bin = byovd + "\\bin";
    for (auto n : names) {
        std::string src = byovd + "\\vehicles\\" + n;
        if (!FileExist(src)) continue;
        std::string dst = bin + "\\" + n;
        if (!FileExist(dst))
            CopyFileA(src.c_str(), dst.c_str(), FALSE);
        break;
    }
}

static int RunMapper(const std::string& byovd, const char* profile, const char* driver)
{
    std::string mapper = byovd + "\\bin\\mapper_nal.exe";
    std::string rwtest = byovd + "\\bin\\RwTest.sys";
    if (!FileExist(mapper) || !FileExist(rwtest)) {
        printf("[!] Missing mapper or RwTest under %s\\bin\n", byovd.c_str());
        return 10;
    }
    CopyVehicle(byovd);

    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
        "\"%s\" \"%s\" --driver %s --no-auth --ac-safe --ac-replica %s",
        mapper.c_str(), rwtest.c_str(), driver, profile);

    printf("[*] Map: %s\n", cmd);
    STARTUPINFOA si{};
    PROCESS_INFORMATION pi{};
    si.cb = sizeof(si);
    std::string cwd = byovd + "\\bin";
    if (!CreateProcessA(nullptr, cmd, nullptr, nullptr, FALSE, 0, nullptr, cwd.c_str(), &si, &pi)) {
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

static bool DeviceAlreadyOpen()
{
    HANDLE h = BetestOpenDevice();
    if (h == INVALID_HANDLE_VALUE) return false;
    CloseHandle(h);
    return true;
}

static int ProveRead()
{
    HANDLE h = BetestOpenDevice();
    if (h == INVALID_HANDLE_VALUE) {
        printf("[!] Driver device not found err=%lu\n", GetLastError());
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
    printf("HijackDriver — map payload + hijack read proof (dispatch/IOCTL path)\n\n");
    printf("  HijackDriver.exe [--profile rust|r6|deltaforce|arenabreakout|arksa]\n");
    printf("                   [--driver rtcore|nal|corsair|...] [--map] [--skip-map]\n\n");
    printf("Default: map if device closed, then auth + IOCTL 0x805 marker read.\n");
    printf("Set BYOVD_ROOT to override mapper/payload tree.\n");
    printf("Full guide: driver-lab\\vm\\hijack-driver\\README.md\n");
}

int main(int argc, char** argv)
{
    const char* profile = "rust";
    const char* driver = "rtcore";
    bool doMap = true;
    bool skipMap = false;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--profile") && i + 1 < argc) { profile = argv[++i]; continue; }
        if (!strcmp(argv[i], "--driver") && i + 1 < argc) { driver = argv[++i]; continue; }
        if (!strcmp(argv[i], "--map")) { doMap = true; continue; }
        if (!strcmp(argv[i], "--skip-map")) { skipMap = true; continue; }
        if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) { Usage(); return 0; }
    }

    if (!IsAdmin()) {
        printf("[!] Run as Administrator (required for mapper)\n");
        return 5;
    }

    std::string byovd = FindByovdRoot();
    if (byovd.empty()) {
        printf("[!] byovd root not found (expected D:\\hostdev\\byovd or BYOVD_ROOT)\n");
        return 6;
    }
    printf("[*] byovd: %s profile=%s driver=%s\n", byovd.c_str(), profile, driver);

    if (!skipMap && (doMap || !DeviceAlreadyOpen())) {
        int mr = RunMapper(byovd, profile, driver);
        if (mr != 0) return mr;
        Sleep(1500);
    } else if (skipMap) {
        printf("[*] --skip-map: expecting driver already mapped\n");
    }

    return ProveRead();
}
