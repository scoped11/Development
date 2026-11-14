// DriverLoader.exe — friendly GUI (default) or console menu. Admin required to map.
#include "DriverLoaderGui.hpp"
#include "LoaderMapCore.hpp"
#include <cstdio>
#include <vector>

static int RunPsFile(const std::wstring& root, const std::wstring& scriptRel, const std::wstring& args)
{
    std::wstring script = (fs::path(root) / scriptRel).wstring();
    if (!fs::exists(script)) {
        printf("[!] missing script: %ls\n", script.c_str());
        return 1;
    }
    std::wstring cmd = L"powershell.exe -NoProfile -ExecutionPolicy Bypass -File \"" + script + L"\"" + args;
    std::vector<wchar_t> buf(cmd.begin(), cmd.end());
    buf.push_back(L'\0');
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    wchar_t cwd[MAX_PATH]{};
    wcscpy_s(cwd, root.c_str());
    if (!CreateProcessW(nullptr, buf.data(), nullptr, nullptr, FALSE, 0, nullptr, cwd, &si, &pi))
        return 1;
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD ec = 1;
    GetExitCodeProcess(pi.hProcess, &ec);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return static_cast<int>(ec);
}

static int MapProfile(const std::wstring& byovd, const wchar_t* profile, bool live)
{
    std::wstring a = L" -Profile " + std::wstring(profile);
    if (live)
        a += L" -Live";
    printf("[*] ranked-safe map profile=%ls live=%d\n", profile, live ? 1 : 0);
    return RunPsFile(byovd, L"driver2\\ranked-safe-map.ps1", a);
}

static void PrintBanner()
{
    printf("\n=== DriverLoader (console) ===\n");
    printf("  5) Probe  6) Rob Run  7) Unturned  8) Universal  0) Exit\n\n");
}

int wmain(int argc, wchar_t** argv)
{
    auto byovd = FindByovdRootW();
    if (byovd.empty()) {
        MessageBoxW(nullptr, L"byovd root not found", L"DriverLoader", MB_ICONERROR);
        return 1;
    }

    bool console = false;
    for (int i = 1; i < argc; ++i) {
        if (_wcsicmp(argv[i], L"--console") == 0) {
            console = true;
            continue;
        }
        if (_wcsicmp(argv[i], L"-rust") == 0)
            return MapProfile(byovd, L"rust", false);
        if (_wcsicmp(argv[i], L"-r6") == 0)
            return MapProfile(byovd, L"r6", false);
        if (_wcsicmp(argv[i], L"-probe") == 0)
            return ProbeDriverClient(byovd);
        if (_wcsicmp(argv[i], L"-lab-robrun") == 0 || _wcsicmp(argv[i], L"-lab") == 0)
            return MapLabSession(byovd, "robrun");
        if (_wcsicmp(argv[i], L"-lab-unturned") == 0)
            return MapLabSession(byovd, "r6");
        if (_wcsicmp(argv[i], L"-lab-universal") == 0 || _wcsicmp(argv[i], L"-lab-all") == 0)
            return MapLabSession(byovd, "universal");
    }

    if (!console)
        return RunDriverLoaderGui();

    AttachConsole(ATTACH_PARENT_PROCESS);
    if (!GetConsoleWindow())
        AllocConsole();

    for (;;) {
        PrintBanner();
        printf("Choice: ");
        fflush(stdout);
        int ch = 0;
        if (scanf_s("%d", &ch) != 1)
            break;
        int ec = 0;
        switch (ch) {
        case 5: ec = ProbeDriverClient(byovd); break;
        case 6: ec = MapLabSession(byovd, "robrun"); break;
        case 7: ec = MapLabSession(byovd, "r6"); break;
        case 8: ec = MapLabSession(byovd, "universal"); break;
        case 0: return 0;
        default:
            printf("[!] invalid\n");
            continue;
        }
        printf(ec == 0 ? "[+] OK\n\n" : "[!] failed exit=%d\n\n", ec);
    }
    return 0;
}
