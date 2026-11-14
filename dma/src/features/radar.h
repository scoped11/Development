#pragma once
#include "engine/engine.h"
#include "config/config.h"

namespace Radar {
    // Draw the 2D radar minimap within the current ImGui window
    void Render(const std::vector<GameEntity>& entities,
                const GameEntity& localPlayer,
                const CameraData& camera,
                const RadarConfig& config);
}
