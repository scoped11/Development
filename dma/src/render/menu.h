#pragma once
#include "game/types.hpp"

namespace Menu {
    bool IsOpen();
    void Open();
    void Close();
    void Render(const std::vector<GameEntity>& entities,
                const GameEntity& localPlayer,
                const CameraData& camera);
}
