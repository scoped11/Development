#pragma once
#include "config/config.h"
#include "engine/engine.h"

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
              UnityEngine& engine);
    void Reset();
    const char* LastStatus();
}
