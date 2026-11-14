#pragma once
#include <d3d11.h>
#include <string>

namespace Splash {
    void SetStage(const char* text, float progress = -1.f);
    void SetDetail(const char* text);
    void SetProgress(float progress);
    std::string Stage();
    std::string Detail();
    float Progress();

    bool Init(ID3D11Device* device, ID3D11DeviceContext* context);
    void Shutdown();
    void Tick();
    void Render();
    bool Covering();
    void MarkReady();
    void MarkBusy();
}
