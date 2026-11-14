#pragma once
#include "../LoaderPaths.hpp"
#include <Windows.h>
#include <string>

inline std::wstring PfBrandImagePath()
{
    const wchar_t* rels[] = {
        L"bin\\brand\\penis-finger-external.png",
        L"driver2\\bin\\brand\\penis-finger-external.png",
        L"driver2\\loader-tool\\pf_brand\\penis-finger-external.png",
        L"brand\\penis-finger-external.png",
    };
    auto root = FindByovdRootW();
    if (root.empty())
        return {};
    for (const wchar_t* r : rels) {
        auto p = fs::path(root) / r;
        if (fs::exists(p))
            return p.wstring();
    }
    return {};
}
