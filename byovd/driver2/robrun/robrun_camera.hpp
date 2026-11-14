#pragma once
#include "robrun_il2cpp.hpp"
#include <cmath>

struct EspCamera {
    FMinimalViewInfo view{};
    bool valid = false;
    float screenW = 1920.f;
    float screenH = 1080.f;
};

inline void CameraFromBattleModel(RobRunMem& mem, uintptr_t battleCamera, EspCamera& cam)
{
    if (!IsPtr(battleCamera))
        return;
    Vector3 pos = mem.Read<Vector3>(battleCamera + offsets::battle_camera::Position);
    Vector3 fwd = mem.Read<Vector3>(battleCamera + offsets::battle_camera::Forward);
    Vector3 right = mem.Read<Vector3>(battleCamera + offsets::battle_camera::Right);
    Vector3 up = mem.Read<Vector3>(battleCamera + offsets::battle_camera::Up);
    if (!FinitePos(pos))
        return;
    cam.view.location = pos;
    if (fwd.Length() > 0.2f && right.Length() > 0.2f && up.Length() > 0.2f) {
        cam.view.axisForward = fwd.Normalized();
        cam.view.axisRight = right.Normalized();
        cam.view.axisUp = up.Normalized();
        cam.view.hasAxes = true;
    }
    cam.view.fov = 75.f;
    cam.valid = true;
}

// Shoulder rig fallback when BattleCameraModel is stale (Cinemachine3rdPersonFollow layout).
inline bool PlaceShoulderEye(const Vector3& root, const Vector3& forward, Vector3& eye)
{
    if (!FinitePos(root) || forward.Length() < 0.2f)
        return false;
    const float shoulderX = 0.45f;
    const float shoulderY = 0.2f;
    const float arm = 0.7f;
    const float dist = 2.2f;
    const float side = 1.f; // right shoulder
    Vector3 flat{ forward.x, 0.f, forward.z };
    if (flat.Length() < 0.2f)
        flat = { 0.f, 0.f, 1.f };
    else
        flat = flat.Normalized();
    Vector3 look = forward.Normalized();
    Vector3 right = Vector3{ 0.f, 1.f, 0.f }.Cross(flat);
    float ox = side >= 0.5f ? shoulderX : -shoulderX;
    Vector3 hand = root + right * ox + Vector3{ 0.f, 1.f, 0.f } * (shoulderY + arm) + flat * 0.f;
    eye = hand - look * dist;
    return FinitePos(eye);
}

inline void EnrichCameraFromLocalPlayer(RobRunMem& mem, uintptr_t battleEntities, EspCamera& cam)
{
    if (cam.valid)
        return;
    uintptr_t player = mem.Read<uintptr_t>(battleEntities + offsets::battle_entities::Player);
    if (!IsPtr(player))
        return;
    uintptr_t client = mem.Read<uintptr_t>(player + offsets::player_entity::ClientModel);
    if (!IsPtr(client))
        return;
    uintptr_t movement = mem.Read<uintptr_t>(client + offsets::player_client::SetMovement);
    uintptr_t rotation = mem.Read<uintptr_t>(client + offsets::player_client::Rotation);
    Vector3 body{};
  if (IsPtr(movement))
        body = mem.Read<Vector3>(movement + offsets::set_movement::Position);
    float yaw = 0.f, pitch = 0.f;
    if (IsPtr(rotation)) {
        yaw = mem.Read<float>(rotation + offsets::client_rotation::LookYaw);
        pitch = mem.Read<float>(rotation + offsets::client_rotation::Pitch);
    }
    Vector3 forward = FRotator(pitch, yaw, 0.f).ToDirection();
    Vector3 eye{};
    if (!PlaceShoulderEye(body, forward, eye))
        return;
    cam.view.location = eye;
    cam.view.rotation.yaw = yaw;
    cam.view.rotation.pitch = pitch;
    cam.view.fov = 75.f;
    cam.valid = true;
}
