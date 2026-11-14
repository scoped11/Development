#pragma once
#include "game/types.hpp"
#include "config/config.h"

namespace ESP {
    // Call from the render loop to draw ESP for all entities
    void Render(const std::vector<GameEntity>& entities,
                const GameEntity& localPlayer,
                const CameraData& camera,
                const ESPConfig& config,
                int screenWidth, int screenHeight);
}
