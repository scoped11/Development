#pragma once
#include "engine/engine.h"

namespace Menu {
    bool IsOpen();
    void Open();
    void Close();
    void Render(const std::vector<GameEntity>& entities,
                const GameEntity& localPlayer,
                const CameraData& camera);
}
