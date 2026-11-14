// Driver3Play.exe — single PC: map driver + launch cheat (admin). No VM, no FPGA.
// Build: setup-tool\build-play.ps1
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <shellapi.h>
#include <cstdio>
#include <string>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

static std::wstring gDriver3;

static bool FindRoot()
{
    wchar_t exe[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    fs::path p = fs::path(exe).parent_path();
    if (p.filename() == L"bin")
        p = p.parent_path();
    for (int i = 0; i < 6; ++i) {
        if (fs::exists(p / L"single-pc-play.ps1")) {
            gDriver3 = p.wstring();
            return true;
        }
        if (p.has_parent_path())
            p = p.parent_path();
    }
    return false;
}

static int RunPs(const std::wstring& args)
{
    std::wstring cmd = L"powershell.exe -NoProfile -ExecutionPolicy Bypass -File \"" +
        gDriver3 + L"\\single-pc-play.ps1\"" + args;
    std::vector<wchar_t> buf(cmd.begin(), cmd.end());
    buf.push_back(L'\0');
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(nullptr, buf.data(), nullptr, nullptr, FALSE, 0, nullptr,
                        gDriver3.c_str(), &si, &pi))
        return 1;
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD ec = 1;
    GetExitCodeProcess(pi.hProcess, &ec);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return (int)ec;
}

int wmain(int argc, wchar_t** argv)
{
    if (!FindRoot()) {
        printf("[!] Place Driver3Play.exe in driver3\\ or driver3\\bin\\\n");
        return 1;
    }
    std::wstring game = L"rust";
    bool live = false;
    bool exec = false;
    std::wstring cheat;
    for (int i = 1; i < argc; ++i) {
        if (_wcsicmp(argv[i], L"-Game") == 0 && i + 1 < argc) game = argv[++i];
        else if (_wcsicmp(argv[i], L"-Live") == 0) live = true;
        else if (_wcsicmp(argv[i], L"-Execute") == 0) exec = true;
        else if (_wcsicmp(argv[i], L"-CheatExe") == 0 && i + 1 < argc) cheat = argv[++i];
        else if (_wcsicmp(argv[i], L"-?") == 0 || _wcsicmp(argv[i], L"--help") == 0) {
            printf("Driver3Play.exe -Execute -Live [-Game rust|r6] [-CheatExe path]\n");
            printf("  Elevated admin required. One PC kernel map + cheat launch.\n");
            return 0;
        }
    }
    if (!exec) {
        printf("SINGLE PC: driver + EXE + play (no VM, no FPGA)\n");
        printf("  Driver3Play.exe -Execute -Live -Game rust\n");
        printf("  See SCALE_S_SINGLE_PC.md\n");
        return RunPs(L" -Game " + game);
    }
    std::wstring a = L" -Execute -Game " + game;
    if (live) a += L" -Live";
    if (!cheat.empty()) a += L" -CheatExe \"" + cheat + L"\"";
    a += L" -Build";
    return RunPs(a);
}
