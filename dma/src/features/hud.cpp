#include "hud.h"
#include "game/names.hpp"
#include "imgui.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

namespace {
    ImU32 Col(const Colour& c) {
        return ImGui::ColorConvertFloat4ToU32(ImVec4(c.r, c.g, c.b, c.a));
    }

    void Chip(const char* label, ImVec4 fg) {
        ImGui::SameLine();
        ImGui::TextColored(fg, "%s", label);
    }
}

void Hud::Render(const std::vector<GameEntity>& entities,
                 const GameEntity& localPlayer,
                 const CameraData& camera,
                 const AppConfig& config)
{
    if (config.general.freeCam) {
        ImDrawList* dl = ImGui::GetBackgroundDrawList();
        dl->AddText(ImVec2(18.f, ImGui::GetIO().DisplaySize.y - 36.f),
                    IM_COL32(255, 210, 80, 230),
                    "FREECAM  WASD + mouse   Space/Ctrl up/down   Shift fast   F6 off");
    }
    if (!config.general.widgets)
        return;

    int players = 0, nearby = 0, cops = 0, robbers = 0, settlers = 0, prisoners = 0, vehicles = 0, bases = 0;
    std::vector<std::pair<int, std::string>> clanCount;
    std::vector<const GameEntity*> baseList;
    bool raidAlert = false;
    bool doorWarn = false;
    const GameEntity* lookBase = nullptr;
    float lookDot = 0.55f;
    Vector3 lookDir = camera.valid ? camera.viewInfo.axisForward : Vector3{};
    if (!lookDir.IsValid() && localPlayer.aimForward.Length() > 0.2f)
        lookDir = localPlayer.aimForward;
    for (const auto& e : entities) {
        if (e.type == EntityType::Player && !e.isLocalPlayer && e.isEnabled) {
            ++players;
            if (localPlayer.position.IsValid() && e.DistanceTo(localPlayer.position) < 80.f)
                ++nearby;
            const std::string r = Names::Lower(e.role);
            if (r.find("cop") != std::string::npos) ++cops;
            if (r.find("robber") != std::string::npos) ++robbers;
            if (r.find("settler") != std::string::npos) ++settlers;
            if (r.find("prisoner") != std::string::npos) ++prisoners;
            if (!e.clan.empty()) {
                bool found = false;
                for (auto& c : clanCount) {
                    if (c.second == e.clan) { ++c.first; found = true; break; }
                }
                if (!found) clanCount.push_back({ 1, e.clan });
            }
        } else if (e.type == EntityType::Vehicle) {
            ++vehicles;
        } else if (e.type == EntityType::Base) {
            ++bases;
            baseList.push_back(&e);
            if (e.beingRaided && (e.isTeammate || e.isClanBase ||
                    Names::SameRole(e.clan, localPlayer.clan)))
                raidAlert = true;
            if (e.openDoors && e.isTeammate)
                doorWarn = true;
            if (camera.valid && localPlayer.position.IsValid() && lookDir.IsValid()) {
                Vector3 d = e.position - camera.viewInfo.location;
                float len = d.Length();
                if (len > 1.f) {
                    d = d * (1.f / len);
                    float nd = d.Dot(lookDir.Normalized());
                    if (nd > lookDot) {
                        lookDot = nd;
                        lookBase = &e;
                    }
                }
            }
        }
    }
    std::sort(clanCount.begin(), clanCount.end(), [](auto& a, auto& b) { return a.first > b.first; });
    std::sort(baseList.begin(), baseList.end(), [&](const GameEntity* a, const GameEntity* b) {
        if (a->beingRaided != b->beingRaided) return a->beingRaided;
        return a->DistanceTo(localPlayer.position) < b->DistanceTo(localPlayer.position);
    });

    if (config.general.serverDashboard) {
        ImGui::SetNextWindowPos(ImVec2(18.f, 18.f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(300.f, 250.f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowBgAlpha(0.92f);
        ImGui::Begin("##ServerDash", nullptr,
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);
        ImGui::TextColored(ImVec4(0.42f, 0.82f, 0.74f, 1.f), "SERVER");
        ImGui::Separator();
        ImGui::Text("Online %d", players);
        ImGui::SameLine();
        ImGui::TextDisabled("nearby %d", nearby);
        ImGui::TextColored(ImVec4(0.3f, 0.5f, 1.f, 1.f), "Cops %d", cops);
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.f, 0.3f, 0.3f, 1.f), "Robbers %d", robbers);
        ImGui::TextColored(ImVec4(0.35f, 0.82f, 0.55f, 1.f), "Settlers %d", settlers);
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.85f, 0.55f, 0.20f, 1.f), "Prisoners %d", prisoners);
        ImGui::Text("Vehicles %d   bases %d   clans %d", vehicles, bases,
                    static_cast<int>(clanCount.size()));
        ImGui::TextDisabled("Threat <80m  %d", nearby);
        if (!clanCount.empty()) {
            ImGui::Separator();
            ImGui::TextDisabled("Largest clans");
            for (int i = 0; i < 3 && i < static_cast<int>(clanCount.size()); ++i)
                ImGui::Text("  [%s] %d", clanCount[i].second.c_str(), clanCount[i].first);
        }
        if (camera.valid)
            ImGui::TextDisabled("SNAP %.0f ms", camera.AgeMs());
        ImGui::End();
    }

    if (config.esp.raidAlerts && raidAlert) {
        ImGui::GetForegroundDrawList()->AddText(ImVec2(40.f, 8.f),
            IM_COL32(255, 70, 70, 255), "RAID ALERT — your/clan base");
    }
    if (config.esp.openDoorWarning && doorWarn) {
        ImGui::GetForegroundDrawList()->AddText(ImVec2(40.f, 26.f),
            IM_COL32(255, 180, 70, 255), "Doors open on your base");
    }

    if (lookBase && config.esp.showBases) {
        ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f - 150.f, 70.f),
            ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(300.f, 0.f), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.94f);
        ImGui::Begin("##BaseFocus", nullptr,
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::TextColored(ImVec4(1.f, 0.45f, 0.75f, 1.f), "%s", lookBase->displayName.c_str());
        if (!lookBase->clan.empty())
            ImGui::Text("[%s]", lookBase->clan.c_str());
        ImGui::TextDisabled("%.0fm", lookBase->DistanceTo(localPlayer.position));
        if (lookBase->vip) Chip("VIP", ImVec4(1.f, 0.85f, 0.2f, 1.f));
        if (lookBase->hasLoot) Chip("LOOT", ImVec4(0.45f, 0.9f, 0.5f, 1.f));
        if (lookBase->openDoors) Chip("DOORS", ImVec4(1.f, 0.7f, 0.25f, 1.f));
        if (lookBase->beingRaided) Chip("RAID", ImVec4(1.f, 0.3f, 0.3f, 1.f));
        if (lookBase->raidLocked) Chip("LOCK", ImVec4(1.f, 0.55f, 0.3f, 1.f));
        if (lookBase->decoderCount > 0) Chip("DECODER", ImVec4(0.75f, 0.55f, 1.f, 1.f));
        ImGui::NewLine();
        if (lookBase->maxDefensePoints > 0)
            ImGui::Text("Fuse %d / %d", lookBase->defensePoints, lookBase->maxDefensePoints);
        if (lookBase->isTeammate) ImGui::TextDisabled("Your base");
        else if (lookBase->isClanBase) ImGui::TextDisabled("Clan base");
        ImGui::End();
    }

    if (config.esp.basePanel && config.esp.showBases) {
        ImGui::SetNextWindowPos(ImVec2(18.f, 280.f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(320.f, 280.f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowBgAlpha(0.92f);
        ImGui::Begin("Bases", nullptr, ImGuiWindowFlags_NoCollapse);
        ImGui::TextDisabled("%d bases  drag this window", static_cast<int>(baseList.size()));
        for (const GameEntity* b : baseList) {
            const float d = b->DistanceTo(localPlayer.position);
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(Col(
                b->isTeammate ? config.esp.myBaseColour :
                (b->isClanBase ? config.esp.clanBaseColour : config.esp.baseColour))));
            ImGui::Text("%s  %.0fm", b->displayName.c_str(), d);
            ImGui::PopStyleColor();
            if (!b->clan.empty())
                ImGui::TextDisabled("  [%s]", b->clan.c_str());
            if (b->vip) Chip("VIP", ImVec4(1.f, 0.85f, 0.2f, 1.f));
            if (b->hasLoot) Chip("loot", ImVec4(0.5f, 1.f, 0.55f, 1.f));
            if (b->openDoors) Chip("doors", ImVec4(1.f, 0.7f, 0.3f, 1.f));
            if (b->beingRaided) Chip("RAID", ImVec4(1.f, 0.3f, 0.3f, 1.f));
            if (b->raidLocked) Chip("lock", ImVec4(1.f, 0.55f, 0.3f, 1.f));
            if (b->decoderCount > 0)
                ImGui::TextDisabled("  decoder needed");
            if (b->maxDefensePoints > 0)
                ImGui::TextDisabled("  fuse %d/%d", b->defensePoints, b->maxDefensePoints);
        }
        ImGui::End();
    }
}
