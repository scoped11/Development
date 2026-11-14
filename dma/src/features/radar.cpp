#include "radar.h"
#include "game/names.hpp"
#include "imgui.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>

namespace {
    ImU32 ToImCol(const Colour& c) {
        return ImGui::ColorConvertFloat4ToU32(ImVec4(c.r, c.g, c.b, c.a));
    }
}

void Radar::Render(const std::vector<GameEntity>& entities,
                   const GameEntity& localPlayer,
                   const CameraData& camera,
                   const RadarConfig& config)
{
    if (!config.enabled) return;

    const float size = config.size;
    ImGuiIO& io = ImGui::GetIO();
    if (config.posX >= 0.f && config.posY >= 0.f)
        ImGui::SetNextWindowPos(ImVec2(config.posX, config.posY), ImGuiCond_Always);
    else
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - size - 28.f, 24.f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(size + 12.f, size + 36.f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.88f);
    ImGui::Begin("##RadarFloat", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoResize);
    ImGui::TextDisabled("RADAR");

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 radarPos = ImGui::GetCursorScreenPos();
    dl->AddRectFilled(radarPos,
        ImVec2(radarPos.x + size, radarPos.y + size),
        ToImCol(config.backgroundColour));

    dl->AddRect(radarPos,
        ImVec2(radarPos.x + size, radarPos.y + size),
        IM_COL32(232, 177, 90, 140), 0.f, 0, 1.f);

    float cx = radarPos.x + size / 2.f;
    float cy = radarPos.y + size / 2.f;
    ImU32 gridCol = IM_COL32(255, 255, 255, 20);
    dl->AddLine(ImVec2(cx, radarPos.y), ImVec2(cx, radarPos.y + size), gridCol);
    dl->AddLine(ImVec2(radarPos.x, cy), ImVec2(radarPos.x + size, cy), gridCol);

    dl->AddLine(ImVec2(radarPos.x + size * 0.25f, radarPos.y),
                ImVec2(radarPos.x + size * 0.25f, radarPos.y + size), gridCol);
    dl->AddLine(ImVec2(radarPos.x + size * 0.75f, radarPos.y),
                ImVec2(radarPos.x + size * 0.75f, radarPos.y + size), gridCol);
    dl->AddLine(ImVec2(radarPos.x, radarPos.y + size * 0.25f),
                ImVec2(radarPos.x + size, radarPos.y + size * 0.25f), gridCol);
    dl->AddLine(ImVec2(radarPos.x, radarPos.y + size * 0.75f),
                ImVec2(radarPos.x + size, radarPos.y + size * 0.75f), gridCol);

    // Local player marker
    dl->AddCircleFilled(ImVec2(cx, cy), 5.f, ToImCol(config.localColour));

    constexpr float DEG2RAD = 3.14159265358979323846f / 180.f;
    float yawRad = camera.valid ? camera.viewInfo.rotation.yaw * DEG2RAD : 0.f;
    float cosYaw = std::cosf(yawRad);
    float sinYaw = std::sinf(yawRad);

    float dirLen = 12.f;
    if (config.rotateWithCamera && camera.valid) {
        dl->AddLine(ImVec2(cx, cy), ImVec2(cx, cy - dirLen),
            ToImCol(config.localColour), 2.f);
    } else if (camera.valid) {
        dl->AddLine(ImVec2(cx, cy),
            ImVec2(cx + sinYaw * dirLen, cy - cosYaw * dirLen),
            ToImCol(config.localColour), 2.f);
    }

    if (!localPlayer.IsValid()) {
        dl->AddText(ImVec2(radarPos.x + 4, radarPos.y + size + 4),
            IM_COL32(255, 255, 255, 128), "No local player data");
        ImGui::Dummy(ImVec2(size, size + 20));
        ImGui::End();
        return;
    }

    // ~200m radius at zoom=1.0
    float scale = (200.f / size) / config.zoom;

    int counts[4] = {};

    for (const auto& ent : entities) {
        if (!ent.IsValid() || ent.isLocalPlayer) continue;

        Colour colour;
        switch (ent.type) {
            case EntityType::Player:
                if (!config.showPlayers) continue;
                colour = config.roleColours ? Names::RoleColour(ent.role, Config::Get().esp) : config.playerColour;
                break;
            case EntityType::Item:
            case EntityType::DeathBag:
            case EntityType::Cargo:
                if (!config.showItems) continue;
                colour = ent.type == EntityType::Cargo ? Config::Get().esp.cargoColour : config.itemColour;
                break;
            case EntityType::Vehicle:
                if (!config.showVehicles) continue;
                colour = config.vehicleColour;
                break;
            case EntityType::Mob:
                if (!config.showMobs) continue;
                colour = config.mobColour;
                break;
            case EntityType::Grenade:
                if (!config.showGrenades) continue;
                colour = config.grenadeColour;
                break;
            case EntityType::Container:
            case EntityType::Rack:
            case EntityType::Atm:
            case EntityType::Safe:
            case EntityType::Vault:
                if (!config.showContainers) continue;
                colour = config.containerColour;
                break;
            case EntityType::Base:
                if (!config.showBases) continue;
                colour = Config::Get().esp.baseColour;
                break;
            case EntityType::Camera:
                if (!config.showCameras) continue;
                colour = Config::Get().esp.cameraColour;
                break;
            default:
                continue;
        }

        if (!ent.isAlive && (ent.type == EntityType::Player || ent.type == EntityType::Mob))
            colour = Colour(0.4f, 0.4f, 0.4f, 0.5f);

        // Unity ground plane is X/Z. Yaw 0 looks along +Z.
        float relX = ent.position.x - localPlayer.position.x;
        float relZ = ent.position.z - localPlayer.position.z;

        float radarX, radarY;

        if (config.rotateWithCamera && camera.valid) {
            float forward = relX * sinYaw + relZ * cosYaw;
            float right   = relX * cosYaw - relZ * sinYaw;

            radarX = cx + right / scale;
            radarY = cy - forward / scale;
        } else {
            radarX = cx + relX / scale;
            radarY = cy - relZ / scale;
        }

        float margin = 4.f;
        if (radarX < radarPos.x + margin || radarX > radarPos.x + size - margin ||
            radarY < radarPos.y + margin || radarY > radarPos.y + size - margin)
            continue;

        const ImU32 col = ToImCol(colour);
        if (config.arrowBlips && ent.type == EntityType::Player) {
            float ang = std::atan2f(ent.aimForward.x, ent.aimForward.z);
            ImVec2 tip(radarX + std::sinf(ang) * 6.f, radarY - std::cosf(ang) * 6.f);
            ImVec2 a(radarX - std::cosf(ang) * 3.5f, radarY - std::sinf(ang) * 3.5f);
            ImVec2 b(radarX + std::cosf(ang) * 3.5f, radarY + std::sinf(ang) * 3.5f);
            dl->AddTriangleFilled(tip, a, b, col);
        } else {
            float dotSize = (ent.type == EntityType::Item) ? 2.5f : 3.5f;
            dl->AddCircleFilled(ImVec2(radarX, radarY), dotSize, col);
        }

        int typeIndex = static_cast<int>(ent.type) - 1;
        if (typeIndex >= 0 && typeIndex < 4) counts[typeIndex]++;
    }

    char info[96];
    snprintf(info, sizeof(info), "P:%d I:%d M:%d V:%d | Zoom:%.1fx",
             counts[0], counts[1], counts[3], counts[2], config.zoom);
    dl->AddText(ImVec2(radarPos.x + 4, radarPos.y + size + 4),
        IM_COL32(255, 255, 255, 160), info);

    ImGui::Dummy(ImVec2(size, size + 20));
    ImGui::End();

    if (config.bigRadar && localPlayer.position.IsValid()) {
        ImGui::SetNextWindowSize(ImVec2(560.f, 620.f), ImGuiCond_FirstUseEver);
        ImGui::Begin("Big Radar");
        static float panX = 0.f, panZ = 0.f, zoom = 1.f;
        if (config.follow) {
            panX = localPlayer.position.x;
            panZ = localPlayer.position.z;
        }
        ImGui::SliderFloat("Zoom##big", &zoom, 0.2f, 6.f, "%.1fx");
        ImGui::SameLine();
        if (ImGui::Button("Recenter")) {
            panX = localPlayer.position.x;
            panZ = localPlayer.position.z;
        }
        ImDrawList* big = ImGui::GetWindowDrawList();
        ImVec2 p = ImGui::GetCursorScreenPos();
        float bs = ImGui::GetContentRegionAvail().x;
        if (bs < 120.f) bs = 120.f;
        ImGui::InvisibleButton("##bigmap", ImVec2(bs, bs));
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            ImVec2 d = ImGui::GetIO().MouseDelta;
            float bscale = (350.f / bs) / std::max(0.2f, zoom);
            panX -= d.x * bscale;
            panZ += d.y * bscale;
        }
        if (ImGui::IsItemHovered() && std::fabs(ImGui::GetIO().MouseWheel) > 0.f)
            zoom = std::clamp(zoom + ImGui::GetIO().MouseWheel * 0.15f, 0.2f, 6.f);
        big->AddRectFilled(p, ImVec2(p.x + bs, p.y + bs), ToImCol(config.backgroundColour));
        float bcx = p.x + bs * 0.5f, bcy = p.y + bs * 0.5f;
        float bscale = (350.f / bs) / std::max(0.2f, zoom);
        auto plot = [&](float wx, float wz, float& ox, float& oy) {
            ox = bcx + (wx - panX) / bscale;
            oy = bcy - (wz - panZ) / bscale;
        };
        float lx, ly;
        plot(localPlayer.position.x, localPlayer.position.z, lx, ly);
        big->AddCircleFilled(ImVec2(lx, ly), 5.f, ToImCol(config.localColour));
        for (const auto& ent : entities) {
            if (!ent.IsValid() || ent.isLocalPlayer) continue;
            Colour c;
            float sz = 3.5f;
            switch (ent.type) {
                case EntityType::Player:
                    if (!config.showPlayers) continue;
                    c = config.roleColours ? Names::RoleColour(ent.role, Config::Get().esp) : config.playerColour;
                    sz = 4.5f;
                    break;
                case EntityType::Base:
                    if (!config.showBases) continue;
                    c = Config::Get().esp.baseColour;
                    sz = 5.f;
                    break;
                case EntityType::Vehicle:
                    if (!config.showVehicles) continue;
                    c = config.vehicleColour;
                    break;
                default:
                    continue;
            }
            float rx, ry;
            plot(ent.position.x, ent.position.z, rx, ry);
            if (rx < p.x || rx > p.x + bs || ry < p.y || ry > p.y + bs) continue;
            big->AddCircleFilled(ImVec2(rx, ry), sz, ToImCol(c));
        }
        ImGui::End();
    }
}
