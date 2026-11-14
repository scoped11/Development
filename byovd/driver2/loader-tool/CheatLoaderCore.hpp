#pragma once
#define WIN32_LEAN_AND_MEAN
#include "LoaderPaths.hpp"
#include "../sdk/Driver2Session.hpp"
#include "../sdk/driver2_api.h"
#include <TlHelp32.h>
#include <cstdio>
#include <fstream>
#include <shellapi.h>
#include <sstream>
#include <string>
#include <vector>

#pragma comment(lib, "shell32.lib")

struct CheatEntry {
    std::wstring name;
    std::wstring path;
    std::wstring args;
};

struct GameLaunchDef {
    const wchar_t* id;
    const wchar_t* displayName;
    const wchar_t* imageBaseName; // robrun / unturned under games/
    const wchar_t* cheatRel;      // relative to byovd root
    const wchar_t* defaultArgs;
    const wchar_t* processExe;
    const wchar_t* mapProfile;      // Driver2Runner --profile
    const wchar_t* titleIoctl;      // L"none" | L"rust" | L"r6"
    DWORD cardColor1;
    DWORD cardColor2;
};

inline std::wstring& ByovdRoot()
{
    static std::wstring root;
    return root;
}

inline std::wstring ResolvePath(const std::wstring& rel)
{
    if (rel.empty())
        return {};
    fs::path p(rel);
    if (p.is_absolute() && fs::exists(p))
        return p.wstring();
    auto stripBin = [&](const std::wstring& r) {
        if (r.size() > 4 && (r.rfind(L"bin\\", 0) == 0 || r.rfind(L"bin/", 0) == 0))
            return r.substr(4);
        return r;
    };
    auto d2bin = Driver2BinW();
    if (!d2bin.empty()) {
        auto c = fs::path(d2bin) / stripBin(rel);
        if (fs::exists(c))
            return c.wstring();
        c = fs::path(d2bin) / rel;
        if (fs::exists(c))
            return c.wstring();
    }
    auto c = fs::path(ByovdRoot()) / rel;
    if (fs::exists(c))
        return c.wstring();
    auto c2 = fs::path(ByovdRoot()) / L"driver2" / rel;
    if (fs::exists(c2))
        return c2.wstring();
    return {};
}

inline Driver2Session& GlobalDriver()
{
    static Driver2Session drv;
    return drv;
}

inline bool& DriverConnected()
{
    static bool connected = false;
    return connected;
}

// Quick device open (no cached session) — for UI polling.
inline bool IsDriverDevicePresent()
{
    HANDLE h = Driver2OpenWithRetry(2, 50);
    if (h == INVALID_HANDLE_VALUE)
        return false;
    bool ok = Driver2Auth(h);
    CloseHandle(h);
    return ok;
}

inline void ResetDriverConnection()
{
    DriverConnected() = false;
    GlobalDriver().Disconnect();
}

// Re-probe when mapped externally; retries after cheat/game restart.
inline bool TryAutoConnect(bool forceReconnect)
{
    if (forceReconnect)
        ResetDriverConnection();

    if (DriverConnected()) {
        if (GlobalDriver().ProbeSelfMarker())
            return true;
        ResetDriverConnection();
    }

    if (!forceReconnect) {
        if (!GlobalDriver().ConnectQuick())
            return false;
    } else if (!GlobalDriver().Connect()) {
        return false;
    }
    if (!GlobalDriver().ProbeSelfMarker()) {
        ResetDriverConnection();
        return false;
    }
    DriverConnected() = true;
    return true;
}

inline bool EnsureConnected()
{
    return TryAutoConnect(false);
}

inline uint32_t FindProcessPid(const wchar_t* exeName)
{
    if (!exeName || !*exeName)
        return 0;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE)
        return 0;
    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    uint32_t pid = 0;
    if (Process32FirstW(snap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, exeName) == 0) {
                pid = pe.th32ProcessID;
                break;
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return pid;
}

// Rob Run ships EasyAntiCheat (install folder). Session map uses EAC replica; refresh IOCTLs when game is up.
inline bool ApplyRobRunEacSession(uint32_t gamePid)
{
    if (!TryAutoConnect(false))
        return false;
    HANDLE h = Driver2OpenWithRetry(4, 100);
    if (h == INVALID_HANDLE_VALUE || !Driver2Auth(h))
        return false;
    bool ok = Driver2EacImprove(h, EAC_IMP_ALL, 0, gamePid) && Driver2EosEncPtr(h, EAC_ENC_PTR_ALL);
    CloseHandle(h);
    return ok;
}

inline bool ApplyBattleyeSession(uint32_t gamePid)
{
    if (!TryAutoConnect(false))
        return false;
    HANDLE h = Driver2OpenWithRetry(4, 100);
    if (h == INVALID_HANDLE_VALUE || !Driver2Auth(h))
        return false;
    bool ok = Driver2BattleyeImprove(h, BE_ALL, gamePid);
    CloseHandle(h);
    return ok;
}

inline void ApplyTitleSessionForGame(const GameLaunchDef& g)
{
    uint32_t pid = FindProcessPid(g.processExe);
    if (!pid)
        return;
    if (g.titleIoctl && _wcsicmp(g.titleIoctl, L"rust") == 0)
        ApplyRobRunEacSession(pid);
    else if (g.titleIoctl && _wcsicmp(g.titleIoctl, L"r6") == 0)
        ApplyBattleyeSession(pid);
}

inline bool IsGameProcessRunning(const wchar_t* exeName)
{
    if (!exeName || !*exeName)
        return false;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE)
        return false;
    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    bool found = false;
    if (Process32FirstW(snap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, exeName) == 0) {
                found = true;
                break;
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return found;
}

inline int LaunchCheat(const std::wstring& exe, const std::wstring& args)
{
    if (!fs::exists(exe))
        return 1;
    if (!EnsureConnected())
        return 2;

    std::wstring cmd = L"\"" + exe + L"\"";
    if (!args.empty())
        cmd += L" " + args;
    std::vector<wchar_t> buf(cmd.begin(), cmd.end());
    buf.push_back(L'\0');

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    wchar_t cwd[MAX_PATH]{};
    wcscpy_s(cwd, fs::path(exe).parent_path().wstring().c_str());
    if (!CreateProcessW(nullptr, buf.data(), nullptr, nullptr, FALSE, CREATE_NEW_CONSOLE, nullptr, cwd, &si,
                        &pi))
        return 3;
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return 0;
}

inline std::vector<CheatEntry> LoadManifestEntries()
{
    std::vector<CheatEntry> out;
    CheatEntry builtin;
    builtin.name = L"cheat_dev_minimal (lab read)";
    builtin.path = ResolvePath(L"bin\\cheat_dev_minimal.exe");
    if (builtin.path.empty())
        builtin.path = ResolvePath(L"driver2\\bin\\cheat_dev_minimal.exe");
    if (!builtin.path.empty())
        out.push_back(builtin);

    auto man = fs::path(Driver2BinW()) / L"games.manifest.json";
    if (!fs::exists(man))
        man = fs::path(FindDriver2RootW()) / L"loader-tool" / L"games.manifest.json";
    if (fs::exists(man)) {
        std::ifstream f(man);
        std::string blob((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        size_t pos = 0;
        while ((pos = blob.find("\"cheat\"", pos)) != std::string::npos) {
            auto cheatKey = pos;
            auto nameP = blob.rfind("\"name\"", cheatKey);
            std::wstring gname;
            if (nameP != std::string::npos && cheatKey - nameP < 200) {
                auto q1 = blob.find('"', blob.find(':', nameP) + 1);
                auto q2 = blob.find('"', q1 + 1);
                if (q1 != std::string::npos && q2 != std::string::npos) {
                    std::string ns = blob.substr(q1 + 1, q2 - q1 - 1);
                    gname.assign(ns.begin(), ns.end());
                }
            }
            auto pq1 = blob.find('"', blob.find(':', pos) + 1);
            auto pq2 = blob.find('"', pq1 + 1);
            if (pq1 == std::string::npos || pq2 == std::string::npos)
                break;
            std::string ps = blob.substr(pq1 + 1, pq2 - pq1 - 1);
            std::wstring path(ps.begin(), ps.end());
            auto full = ResolvePath(path);
            if (!full.empty()) {
                std::wstring args;
                auto argsP = blob.find("\"args\"", pq2);
                if (argsP != std::string::npos && argsP - pq2 < 120) {
                    auto aq1 = blob.find('"', blob.find(':', argsP) + 1);
                    auto aq2 = blob.find('"', aq1 + 1);
                    if (aq1 != std::string::npos && aq2 != std::string::npos) {
                        std::string as = blob.substr(aq1 + 1, aq2 - aq1 - 1);
                        args.assign(as.begin(), as.end());
                    }
                }
                out.push_back({ gname.empty() ? fs::path(full).filename().wstring() : gname, full, args });
            }
            pos = pq2 + 1;
        }
    }
    return out;
}

inline const GameLaunchDef* CatalogGames(size_t* count)
{
    static const GameLaunchDef kGames[] = {
        { L"robrun", L"Rob Run", L"robrun", L"bin\\robrun_esp_external.exe", L"--overlay",
          L"ROB RUN.exe", L"robrun", L"rust", 0xFFE85D04, 0xFF9A3412 },
        { L"unturned", L"Unturned", L"unturned", L"bin\\externaltest.exe", L"", L"Unturned.exe",
          L"deltaforce", L"r6", 0xFF43A047, 0xFF1B5E20 },
    };
    *count = sizeof(kGames) / sizeof(kGames[0]);
    return kGames;
}
