#pragma once
#include "LoaderPaths.hpp"
#include "../sdk/driver2_api.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

inline bool DriverMappedQuick()
{
    HANDLE h = Driver2OpenWithRetry(1, 0);
    if (h == INVALID_HANDLE_VALUE)
        return false;
    bool ok = Driver2Auth(h);
    CloseHandle(h);
    return ok;
}

inline int MapLabSession(const std::wstring& /*byovd*/, const char* profile)
{
    if (DriverMappedQuick())
        return 0;

    auto d2 = FindDriver2RootW();
    if (d2.empty())
        return 10;
    fs::path script = fs::path(d2) / L"map-host-lab.ps1";
    auto runner = fs::path(d2) / L"bin" / L"Driver2Runner.exe";

    std::wstring prof(profile, profile + strlen(profile));
    std::wstring cmd;
    if (fs::exists(script)) {
        cmd = L"powershell.exe -NoProfile -ExecutionPolicy Bypass -File \"" + script.wstring() +
              L"\" -Profile " + prof;
    } else if (fs::exists(runner)) {
        cmd = L"\"" + runner.wstring() + L"\" --profile " + prof +
              L" --mode session --driver rtcore --skip-rw";
    } else
        return 10;
    std::vector<wchar_t> buf(cmd.begin(), cmd.end());
    buf.push_back(L'\0');
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    fs::path cwd = fs::exists(script) ? fs::path(d2) / L"bin" : runner.parent_path();
    if (!CreateProcessW(nullptr, buf.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr,
                        cwd.wstring().c_str(), &si, &pi))
        return 11;
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD ec = 1;
    GetExitCodeProcess(pi.hProcess, &ec);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return static_cast<int>(ec);
}

inline int ProbeDriverClient(const std::wstring& /*byovd*/)
{
    auto d2 = FindDriver2RootW();
    auto client = d2.empty() ? fs::path() : fs::path(d2) / L"bin" / L"Driver2Client.exe";
    if (!fs::exists(client))
        return 10;
    std::wstring cmd = L"\"" + client.wstring() + L"\" probe";
    std::vector<wchar_t> buf(cmd.begin(), cmd.end());
    buf.push_back(L'\0');
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(nullptr, buf.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr,
                        client.parent_path().wstring().c_str(), &si, &pi))
        return 11;
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD ec = 1;
    GetExitCodeProcess(pi.hProcess, &ec);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return static_cast<int>(ec);
}

inline bool IsProcessElevated()
{
    BOOL elev = FALSE;
    HANDLE token = nullptr;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        TOKEN_ELEVATION te{};
        DWORD sz = sizeof(te);
        if (GetTokenInformation(token, TokenElevation, &te, sizeof(te), &sz))
            elev = te.TokenIsElevated;
        CloseHandle(token);
    }
    return elev != FALSE;
}
