#pragma once
#include "config/config.h"
#include "game/session.hpp"

namespace Aimbot {
    void RenderFov(const AimConfig& config, int screenWidth, int screenHeight);
    void Tick(const std::vector<GameEntity>& entities,
              const GameEntity& localPlayer,
              const CameraData& camera,
              const AimConfig& config,
              int screenWidth,
              int screenHeight,
              float scaleX,
              float scaleY,
              RobRunSession& session);
    void Reset();
    const char* LastStatus();
}
