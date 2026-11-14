#define NOMINMAX
#include "aimbot.h"
#include "core/kmbox_usb.h"
#include "math/vector.h"
#include "imgui.h"

#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdio>
#include <string>
#include <random>

namespace {

char g_status[160] = "Aim idle";
uintptr_t g_trackAddress = 0;
int g_trackBone = -1;
Vector2 g_trackScreen{};
bool g_trackHas = false;

void SetStatus(const char* text) {
    std::snprintf(g_status, sizeof(g_status), "%s", text);
}

std::string Lower(std::string s) {
    for (char& c : s)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

bool SameTeam(const GameEntity& local, const GameEntity& ent) {
    if (ent.isTeammate) return true;
    if (!local.clan.empty() && !ent.clan.empty())
        return Lower(local.clan) == Lower(ent.clan);
    return false;
}

bool LocalAimActive(const AimConfig& config) {
    if (config.aimKey == 0)
        return true;
    static const int kVk[] = { 0, VK_LBUTTON, VK_RBUTTON, VK_MBUTTON, VK_XBUTTON1, VK_XBUTTON2 };
    if (config.aimKey >= 1 && config.aimKey <= 5) {
        if ((GetAsyncKeyState(kVk[config.aimKey]) & 0x8000) != 0)
            return true;
    }
    if (config.aimKey2 >= 1 && config.aimKey2 <= 5) {
        if ((GetAsyncKeyState(kVk[config.aimKey2]) & 0x8000) != 0)
            return true;
    }
    return false;
}

Vector3 BoxPoint(const GameEntity& ent, float along) {
    const float height = ent.markerHeight > 0.35f ? ent.markerHeight : 1.8f;
    Vector3 point = ent.position;
    point.y += height * std::clamp(along, 0.05f, 0.98f);
    return point;
}

Vector3 BoneAt(const GameEntity& ent, int index, const Vector3& named) {
    if (named.IsValid() && !named.IsZero())
        return named;
    if (index >= 0 && index < static_cast<int>(ent.skeletonPositions.size())) {
        const Vector3& p = ent.skeletonPositions[static_cast<size_t>(index)];
        if (p.IsValid() && !p.IsZero())
            return p;
    }
    return {};
}

Vector3 AimWorldPoint(const GameEntity& ent, int bone) {
    Vector3 named;
    float along = 0.64f;
    int index = -1;
    switch (bone) {
        case 0:
            named = BoneAt(ent, ent.boneIdxHead2, ent.skeletonHead);
            if (!named.IsValid())
                named = BoneAt(ent, ent.boneIdxHead1, {});
            along = 0.90f;
            index = ent.boneIdxHead2 >= 0 ? ent.boneIdxHead2 : ent.boneIdxHead1;
            break;
        case 1:
            named = BoneAt(ent, ent.boneIdxNeck, ent.skeletonNeck);
            along = 0.80f;
            index = ent.boneIdxNeck;
            break;
        case 2:
            named = BoneAt(ent, ent.boneIdxSpine3, ent.skeletonChest);
            if (!named.IsValid())
                named = BoneAt(ent, ent.boneIdxSpine2, {});
            along = 0.64f;
            index = ent.boneIdxSpine3 >= 0 ? ent.boneIdxSpine3 : ent.boneIdxSpine2;
            break;
        case 3:
            named = BoneAt(ent, ent.boneIdxSpine2, ent.skeletonChest);
            if (named.IsValid() && ent.skeletonPelvis.IsValid())
                named = (named + ent.skeletonPelvis) * 0.5f;
            along = 0.54f;
            index = ent.boneIdxSpine2;
            break;
        default:
            named = BoneAt(ent, ent.boneIdxRoot, ent.skeletonPelvis);
            along = 0.42f;
            index = ent.boneIdxRoot;
            break;
    }
    if (named.IsValid() && !named.IsZero())
        return named;
    if (index >= 0 && index < static_cast<int>(ent.skeletonPositions.size())) {
        const Vector3& p = ent.skeletonPositions[static_cast<size_t>(index)];
        if (p.IsValid() && !p.IsZero())
            return p;
    }
    return BoxPoint(ent, along);
}

void ClearTrack() {
    g_trackAddress = 0;
    g_trackBone = -1;
    g_trackHas = false;
}

float EffectiveFov(const AimConfig& config, float targetDistM) {
    if (!config.dynamicFov)
        return std::max(8.f, config.fov);
    const float maxD = std::max(10.f, config.maxDistance);
    const float t = std::clamp(targetDistM / maxD, 0.f, 1.f);
    const float minF = std::min(config.fov, config.fovMin);
    const float maxF = std::max(config.fov, config.fovMin);
    return minF + (maxF - minF) * (1.f - t);
}

bool IsHub(const GameEntity& ent) {
    return Lower(ent.role).find("hub") != std::string::npos;
}

bool KeepTarget(const GameEntity& ent, const GameEntity& local, const Vector3& origin,
                const AimConfig& config) {
    if (!ent.IsValid() || ent.isLocalPlayer || ent.type != EntityType::Player)
        return false;
    if (!IsHub(ent) && !ent.isAlive)
        return false;
    if (config.teamCheck && SameTeam(local, ent))
        return false;
    if (config.visibleOnly && (ent.hidingInProp || ent.isHidden))
        return false;
    return ent.DistanceTo(origin) <= config.maxDistance;
}

bool ValidTarget(const GameEntity& ent, const GameEntity& local, const Vector3& origin,
                 const AimConfig& config) {
    if (!KeepTarget(ent, local, origin, config))
        return false;
    if (!IsHub(ent) && ent.isKnocked && !config.targetKnocked)
        return false;
    return true;
}

struct TargetPick {
    bool found = false;
    bool holding = false;
    uintptr_t address = 0;
    int bone = -1;
    Vector2 screen{};
    Vector3 world{};
};

bool ProjectBestBone(const GameEntity& ent,
                     const CameraData& camera,
                     const AimConfig& config,
                     int screenWidth,
                     int screenHeight,
                     float centerX,
                     float centerY,
                     bool holdBone,
                     Vector2& outScreen,
                     Vector3& outWorld,
                     int& outBone) {
    bool enabled[5] = {
        config.targetHead, config.targetNeck, config.targetChest,
        config.targetStomach, config.targetPelvis
    };
    if (!enabled[0] && !enabled[1] && !enabled[2] && !enabled[3] && !enabled[4])
        enabled[0] = true;

    auto project = [&](int bone, Vector2& screen, Vector3& world) {
        world = AimWorldPoint(ent, bone);
        return WorldToScreenUnity(world, camera.viewInfo, screenWidth, screenHeight, screen);
    };

    if (holdBone && g_trackBone >= 0 && g_trackBone <= 4 && enabled[g_trackBone]) {
        if (project(g_trackBone, outScreen, outWorld)) {
            outBone = g_trackBone;
            return true;
        }
    }

    float bestDist = 1.e9f;
    bool found = false;
    for (int bone = 0; bone < 5; ++bone) {
        if (!enabled[bone])
            continue;
        Vector2 projected;
        Vector3 world;
        if (!project(bone, projected, world))
            continue;
        const float dx = projected.x - centerX;
        const float dy = projected.y - centerY;
        const float dist = std::sqrtf(dx * dx + dy * dy);
        if (dist < bestDist) {
            bestDist = dist;
            outScreen = projected;
            outWorld = world;
            outBone = bone;
            found = true;
        }
    }
    return found;
}

TargetPick SelectTarget(const std::vector<GameEntity>& entities,
                        const GameEntity& localPlayer,
                        const CameraData& camera,
                        const AimConfig& config,
                        int screenWidth,
                        int screenHeight) {
    const Vector3 origin = localPlayer.isEnabled ? localPlayer.position : camera.viewInfo.location;
    const float centerX = screenWidth * 0.5f;
    const float centerY = screenHeight * 0.5f;

    TargetPick pick;
    if (config.targetLock && g_trackAddress != 0) {
        for (const auto& ent : entities) {
            if (ent.address != g_trackAddress) continue;
            if (!KeepTarget(ent, localPlayer, origin, config)) break;
            if (!ProjectBestBone(ent, camera, config, screenWidth, screenHeight, centerX, centerY,
                                 true, pick.screen, pick.world, pick.bone))
                break;
            pick.address = ent.address;
            pick.found = true;
            pick.holding = true;
            break;
        }
    }

    int players = 0;
    int ranged = 0;
    if (!pick.found) {
        if (config.targetLock)
            g_trackAddress = 0;
        float closest = 1.e9f;
        for (const auto& ent : entities) {
            if (ent.IsValid() && !ent.isLocalPlayer && ent.type == EntityType::Player)
                ++players;
            if (!ValidTarget(ent, localPlayer, origin, config))
                continue;
            ++ranged;
            Vector2 projected;
            Vector3 world;
            int bone = -1;
            if (!ProjectBestBone(ent, camera, config, screenWidth, screenHeight, centerX, centerY,
                                 false, projected, world, bone))
                continue;
            const float dx = projected.x - centerX;
            const float dy = projected.y - centerY;
            const float dist = std::sqrtf(dx * dx + dy * dy);
            const float worldDist = ent.DistanceTo(origin);
            const float fovLimit = EffectiveFov(config, worldDist);
            if (dist > fovLimit)
                continue;
            float score = dist;
            if (config.priorityMode == 1)
                score = worldDist;
            else if (config.priorityMode == 2)
                score = ent.healthNormalized;
            if (score >= closest)
                continue;
            closest = score;
            pick.screen = projected;
            pick.world = world;
            pick.address = ent.address;
            pick.bone = bone;
            pick.found = true;
        }
        if (!pick.found) {
            if (ranged > 0)
                SetStatus("Player is in range, but outside the circle");
            else if (players > 0)
                SetStatus("Players are listed, but none are inside aim distance");
            else
                SetStatus("No other player in the list");
        }
    }
    return pick;
}

float AimSmoothValue(const AimConfig& config) {
    return std::max(1.f, config.smooth);
}

float SmoothGain(const AimConfig& config) {
    const float smooth = AimSmoothValue(config);
    float t = 1.f / smooth;
    if (config.smoothMode == 2)
        t = t * t * (3.f - 2.f * t);
    return std::clamp(t, 0.02f, 1.f);
}

bool KeyHeld(int key) {
    if (key <= 0)
        return true;
    static const int kVk[] = { 0, VK_LBUTTON, VK_RBUTTON, VK_MBUTTON, VK_XBUTTON1, VK_XBUTTON2 };
    if (key >= 1 && key <= 5)
        return (GetAsyncKeyState(kVk[key]) & 0x8000) != 0;
    return false;
}

void TickTrigger(const std::vector<GameEntity>& entities,
                 const GameEntity& localPlayer,
                 const CameraData& camera,
                 const AimConfig& config,
                 int screenWidth,
                 int screenHeight,
                 RobRunSession& session) {
    if (!config.triggerbot) {
        session.SetShotInput(false);
        return;
    }
    if (!KeyHeld(config.triggerKey)) {
        session.SetShotInput(false);
        return;
    }

    AimConfig visCfg = config;
    visCfg.teamCheck = config.triggerTeamCheck;
    visCfg.visibleOnly = config.triggerVisibleOnly;
    visCfg.targetLock = false;
    TargetPick pick = SelectTarget(entities, localPlayer, camera, visCfg, screenWidth, screenHeight);
    const float cx = screenWidth * 0.5f;
    const float cy = screenHeight * 0.5f;
    const bool onTarget = pick.found &&
        std::fabsf(pick.screen.x - cx) < 8.f &&
        std::fabsf(pick.screen.y - cy) < 8.f;

    static uint32_t fireAt = 0;
    static bool firing = false;
    const uint32_t now = GetTickCount();
    if (!onTarget) {
        fireAt = 0;
        firing = false;
        session.SetShotInput(false);
        return;
    }
    if (fireAt == 0) {
        float lo = std::min(config.triggerDelayMin, config.triggerDelayMax);
        float hi = std::max(config.triggerDelayMin, config.triggerDelayMax);
        static std::mt19937 rng{ std::random_device{}() };
        std::uniform_real_distribution<float> dist(lo, std::max(lo + 1.f, hi));
        fireAt = now + static_cast<uint32_t>(dist(rng));
        firing = false;
    }
    if (now >= fireAt) {
        firing = true;
        session.SetShotInput(true);
    }
}

} // namespace

void Aimbot::RenderFov(const AimConfig& config, int screenWidth, int screenHeight) {
    if (!config.enabled || !config.drawFov)
        return;
    const float cx = screenWidth * 0.5f;
    const float cy = screenHeight * 0.5f;
    const float radius = std::max(8.f, config.fov);
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    const ImU32 ring = config.silentAim ? IM_COL32(80, 200, 255, 80) : IM_COL32(255, 90, 90, 70);
    dl->AddCircle(ImVec2(cx, cy), radius, ring, 72, 2.f);
    dl->AddCircle(ImVec2(cx, cy), radius, IM_COL32(255, 255, 255, 45), 72, 1.f);
    if (config.dynamicFov) {
        const float inner = std::min(config.fov, config.fovMin);
        dl->AddCircle(ImVec2(cx, cy), inner, IM_COL32(255, 180, 80, 40), 72, 1.f);
    }
}

void Aimbot::Tick(const std::vector<GameEntity>& entities,
                  const GameEntity& localPlayer,
                  const CameraData& camera,
                  const AimConfig& config,
                  int screenWidth,
                  int screenHeight,
                  float /*scaleX*/,
                  float /*scaleY*/,
                  RobRunSession& session) {
    TickTrigger(entities, localPlayer, camera, config, screenWidth, screenHeight, session);

    if (!config.enabled) {
        SetStatus("Aim is off");
        session.SetMemoryAimTarget(false, {}, {}, 0.f);
        KmboxUsb::Get().ClearAimMove();
        ClearTrack();
        return;
    }
    if (!camera.valid) {
        SetStatus("Camera is not ready");
        session.SetMemoryAimTarget(false, {}, {}, 0.f);
        KmboxUsb::Get().ClearAimMove();
        ClearTrack();
        return;
    }

    if (!LocalAimActive(config)) {
        SetStatus("Hold the aim key on this PC");
        session.SetMemoryAimTarget(false, {}, {}, 0.f);
        KmboxUsb::Get().ClearAimMove();
        ClearTrack();
        return;
    }

    TargetPick pick = SelectTarget(entities, localPlayer, camera, config, screenWidth, screenHeight);
    if (!pick.found) {
        session.SetMemoryAimTarget(false, {}, {}, 0.f);
        KmboxUsb::Get().ClearAimMove();
        ClearTrack();
        return;
    }

    SetStatus(pick.holding ? "Holding target" : "Locked");
    g_trackAddress = pick.address;
    g_trackBone = pick.bone;
    g_trackScreen = pick.screen;
    g_trackHas = true;

    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    dl->AddCircleFilled(ImVec2(pick.screen.x, pick.screen.y), 3.f,
                        config.silentAim ? IM_COL32(80, 200, 255, 230) : IM_COL32(255, 70, 70, 230));

    Vector3 eye = camera.viewInfo.location;
    if (!eye.IsValid() && localPlayer.isEnabled)
        eye = localPlayer.position;
    Vector3 aimed = pick.world;
    for (const auto& ent : entities) {
        if (ent.address != pick.address) continue;
        const float leadMs = config.prediction ? config.leadMs : 0.f;
        const float leadPx = config.prediction ? config.leadPx : 0.f;
        aimed = pick.world + ent.velocity * leadMs;
        Vector2 leadScreen;
        if (WorldToScreenUnity(aimed, camera.viewInfo, screenWidth, screenHeight, leadScreen)) {
            const float cx = screenWidth * 0.5f;
            const float cy = screenHeight * 0.5f;
            float dx = leadScreen.x - cx;
            float dy = leadScreen.y - cy;
            float len = std::sqrtf(dx * dx + dy * dy);
            if (len > 0.5f && leadPx > 0.f) {
                aimed = pick.world + ent.velocity * leadMs;
                pick.screen.x += (dx / len) * leadPx;
                pick.screen.y += (dy / len) * leadPx;
            }
        }
        break;
    }

    if (config.silentAim) {
        KmboxUsb::Get().ClearAimMove();
        session.SetMemoryAimTarget(true, eye, aimed, 1.f);
        SetStatus(pick.holding ? "Silent aim (holding)" : "Silent aim");
        return;
    }

    if (!config.memoryAim) {
        auto& km = KmboxUsb::Get();
        if (!km.IsConnected()) {
            session.SetMemoryAimTarget(false, {}, {}, 0.f);
            SetStatus("KMBox serial not connected");
            return;
        }
        const float centerX = screenWidth * 0.5f;
        const float centerY = screenHeight * 0.5f;
        const float dx = pick.screen.x - centerX;
        const float dy = pick.screen.y - centerY;
        const float dist = std::sqrtf(dx * dx + dy * dy);
        session.SetMemoryAimTarget(false, {}, {}, 0.f);
        if (dist < config.deadzone) {
            km.ClearAimMove();
            SetStatus(pick.holding ? "KMBox (deadzone)" : "KMBox locked");
            return;
        }
        const float gain = SmoothGain(config);
        static float emaDx = 0.f;
        static float emaDy = 0.f;
        float mx = dx * gain;
        float my = dy * gain;
        if (config.smoothMode == 1) {
            emaDx += gain * (dx - emaDx);
            emaDy += gain * (dy - emaDy);
            mx = emaDx;
            my = emaDy;
        } else {
            emaDx = mx;
            emaDy = my;
        }
        km.QueueAimMove(static_cast<int>(std::lround(mx)),
                        static_cast<int>(std::lround(my)));
        SetStatus(pick.holding ? "KMBox aim (holding)" : "KMBox aim");
        return;
    }

    const float gain = SmoothGain(config);
    session.SetMemoryAimTarget(true, eye, aimed, 1.f / std::max(0.04f, gain));
    SetStatus(pick.holding ? "Memory aim (holding)" : "Memory aim");
}

void Aimbot::Reset() {
    ClearTrack();
}

const char* Aimbot::LastStatus() {
    return g_status;
}
