// CheatLoader.exe — GUI game picker; auto-detects mapped Driver2.
//   CheatLoader.exe           default GUI
//   CheatLoader.exe --console legacy text menu
#include "CheatLoaderCore.hpp"
#include "GamePickerWin.hpp"
#include <cstdio>
#include <cstdlib>
#include <string>

static void RunConsoleMenu()
{
    auto cheats = LoadManifestEntries();
    CheatEntry robrun;
    robrun.name = L"Rob Run ESP (overlay)";
    robrun.path = ResolvePath(L"bin\\robrun_esp_external.exe");
    if (robrun.path.empty())
        robrun.path = ResolvePath(L"driver2\\bin\\robrun_esp_external.exe");
    robrun.args = L"--overlay";
    if (!robrun.path.empty())
        cheats.insert(cheats.begin(), robrun);

    printf("[*] Polling driver...\n");
    if (TryAutoConnect(false))
        printf("[+] driver auto-detected\n");
    else
        printf("[!] driver not loaded — map with DriverLoader first\n");

    for (;;) {
        printf("\n=== CheatLoader (console) ===\n");
        printf("  driver: %s\n", DriverConnected() ? "connected" : "offline");
        printf("  1) Re-check driver\n");
        printf("  2) Launch Rob Run ESP\n");
        int base = 3;
        for (size_t i = 0; i < cheats.size(); ++i)
            printf("  %zu) %ls\n", base + i, cheats[i].name.c_str());
        printf("  0) Exit\nChoice: ");
        size_t choice = 0;
        if (scanf_s("%zu", &choice) != 1)
            break;
        if (choice == 0)
            return;
        if (choice == 1) {
            TryAutoConnect(true);
            printf(DriverConnected() ? "[+] connected\n" : "[!] not loaded\n");
            continue;
        }
        if (choice == 2) {
            if (!robrun.path.empty())
                LaunchCheat(robrun.path, robrun.args);
            continue;
        }
        if (choice >= static_cast<size_t>(base) && choice < base + cheats.size()) {
            auto& c = cheats[choice - base];
            LaunchCheat(c.path, c.args);
        }
    }
}

int wmain(int argc, wchar_t** argv)
{
    bool console = false;
    for (int i = 1; i < argc; ++i) {
        if (_wcsicmp(argv[i], L"--console") == 0)
            console = true;
    }

    ByovdRoot() = FindByovdRootW();
    if (ByovdRoot().empty()) {
        MessageBoxW(nullptr, L"Run from driver2\\bin\\CheatLoader.exe", L"CheatLoader",
                    MB_ICONERROR);
        return 1;
    }

    if (console) {
        AttachConsole(ATTACH_PARENT_PROCESS);
        if (!GetConsoleWindow())
            AllocConsole();
        RunConsoleMenu();
        return 0;
    }
    return RunGamePickerGui();
}
