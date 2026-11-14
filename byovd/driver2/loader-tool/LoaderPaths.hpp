#pragma once
#include <Windows.h>
#include <Shlwapi.h>
#include <filesystem>
#include <string>

#pragma comment(lib, "shlwapi.lib")

namespace fs = std::filesystem;

inline bool PathHasMapper(const fs::path& root)
{
    return fs::exists(root / L"bin" / L"mapper_nal.exe") ||
           fs::exists(root / L"driver2" / L"bin" / L"mapper_nal.exe");
}

inline std::wstring FindDriver2RootW()
{
    if (const wchar_t* env = _wgetenv(L"DRIVER2_ROOT")) {
        std::wstring d(env);
        if (fs::exists(fs::path(d) / L"bin" / L"Driver2Runner.exe"))
            return d;
    }
    wchar_t exe[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    fs::path p = fs::path(exe).parent_path();
    if (fs::exists(p / L"Driver2Runner.exe") && fs::exists(p.parent_path() / L"map-host-lab.ps1"))
        return p.parent_path().wstring();
    for (int i = 0; i < 8; ++i) {
        if (fs::exists(p / L"bin" / L"Driver2Runner.exe") &&
            fs::exists(p / L"map-host-lab.ps1"))
            return p.wstring();
        if (p.has_parent_path())
            p = p.parent_path();
    }
    static const wchar_t* kFixed[] = {
        L"C:\\Users\\baile\\OneDrive\\Desktop\\Development\\byovd\\driver2",
        L"D:\\hostdev\\byovd\\driver2",
    };
    for (auto f : kFixed) {
        if (fs::exists(std::wstring(f) + L"\\bin\\Driver2Runner.exe"))
            return f;
    }
    return {};
}

inline std::wstring FindByovdRootW()
{
    if (const wchar_t* env = _wgetenv(L"BYOVD_ROOT")) {
        std::wstring d(env);
        if (PathHasMapper(fs::path(d)))
            return d;
    }
    auto d2 = FindDriver2RootW();
    if (!d2.empty()) {
        fs::path byovd = fs::path(d2).parent_path();
        if (PathHasMapper(byovd))
            return byovd.wstring();
    }
    wchar_t exe[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    fs::path p = fs::path(exe).parent_path();
    for (int i = 0; i < 8; ++i) {
        if (PathHasMapper(p))
            return p.wstring();
        if (p.has_parent_path())
            p = p.parent_path();
    }
    static const wchar_t* kFixed[] = {
        L"C:\\Users\\baile\\OneDrive\\Desktop\\Development\\byovd",
        L"D:\\hostdev\\byovd",
    };
    for (auto f : kFixed) {
        if (PathHasMapper(f))
            return f;
    }
    return {};
}

inline std::wstring Driver2BinW()
{
    auto d2 = FindDriver2RootW();
    if (d2.empty())
        return {};
    return (fs::path(d2) / L"bin").wstring();
}
