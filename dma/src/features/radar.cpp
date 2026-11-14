#include "radar.h"
#include "imgui.h"
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
                {
                    std::string role = ent.role;
                    for (char& c : role)
                        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                    const auto& esp = Config::Get().esp;
                    if (role.find("cop") != std::string::npos) colour = esp.copColour;
                    else if (role.find("robber") != std::string::npos) colour = esp.robberColour;
                    else colour = esp.playerColour;
                }
                break;
            case EntityType::Item:
                if (!config.showItems) continue;
                colour = config.itemColour;
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
                if (!config.showContainers) continue;
                colour = config.containerColour;
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

        float dotSize = (ent.type == EntityType::Item) ? 2.5f : 3.5f;
        dl->AddCircleFilled(ImVec2(radarX, radarY), dotSize, ToImCol(colour));

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
}
