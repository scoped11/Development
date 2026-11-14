#pragma once
#include "game/types.hpp"
#include "config/config.h"

namespace Hud {
    void Render(const std::vector<GameEntity>& entities,
                const GameEntity& localPlayer,
                const CameraData& camera,
                const AppConfig& config);
}
