#include "menu.h"
#include "config/config.h"
#include "core/memory.h"
#include "features/aimbot.h"
#include "core/kmbox_usb.h"
#include "render/renderer.h"
#include "app.h"
#include "imgui.h"

#include <Windows.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {
    constexpr ImVec4 kAccent(0.42f, 0.82f, 0.74f, 1.f);
    constexpr ImVec4 kMuted(0.62f, 0.68f, 0.72f, 1.f);
    constexpr ImVec4 kDim(0.42f, 0.46f, 0.50f, 1.f);
    enum Page { Overlay, Visuals, World, RadarPage, TeamPage, BasesPage, Roster, AimPage, System, PageCount };

    const char* kPages[] = {
        "Overlay", "Visuals", "World", "Radar", "Team", "Bases", "Roster", "Aim", "System"
    };

    static const char* kKeyNames =
        "Always on\0Left mouse\0Right mouse\0Middle mouse\0Mouse 4\0Mouse 5\0";
    static const char* kSecondKeyNames =
        "Off\0Left mouse\0Right mouse\0Middle mouse\0Mouse 4\0Mouse 5\0";

    bool g_open = false;
    bool g_insertDown = false;
    int g_page = Overlay;

    void PollInsert() {
        const bool down = (GetAsyncKeyState(VK_INSERT) & 0x8000) != 0;
        if (down && !g_insertDown)
            g_open = !g_open;
        g_insertDown = down;
    }

    void HelpMarker(const char* desc) {
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::BeginItemTooltip()) {
            ImGui::PushTextWrapPos(280.f);
            ImGui::TextUnformatted(desc);
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
    }

    void ColourEdit(const char* label, Colour& c) {
        float col[4] = { c.r, c.g, c.b, c.a };
        if (ImGui::ColorEdit4(label, col, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoInputs)) {
            c.r = col[0]; c.g = col[1]; c.b = col[2]; c.a = col[3];
        }
    }

    void Section(const char* title) {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, kAccent);
        ImGui::TextUnformatted(title);
        ImGui::PopStyleColor();
        ImVec2 p = ImGui::GetCursorScreenPos();
        float w = ImGui::GetContentRegionAvail().x;
        ImGui::GetWindowDrawList()->AddLine(
            p, ImVec2(p.x + w, p.y), IM_COL32(108, 210, 188, 70), 1.f);
        ImGui::Dummy(ImVec2(0, 6));
    }

    void Chip(const char* label, ImU32 bg, ImU32 fg) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 p = ImGui::GetCursorScreenPos();
        ImVec2 sz = ImGui::CalcTextSize(label);
        const float padX = 8.f;
        const float padY = 3.f;
        const float w = sz.x + padX * 2.f;
        const float h = sz.y + padY * 2.f;
        dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), bg, 5.f);
        dl->AddText(ImVec2(p.x + padX, p.y + padY), fg, label);
        ImGui::Dummy(ImVec2(w, h));
    }

    void FuserModePicker(GeneralConfig& g, float buttonW = 150.f) {
        const bool fuser = g.fuserMode;
        if (!fuser)
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.10f, 0.30f, 0.26f, 1.f));
        else
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.14f, 0.16f, 0.18f, 1.f));
        if (ImGui::Button("Transparent", ImVec2(buttonW, 32)))
            g.fuserMode = false;
        ImGui::PopStyleColor();
        HelpMarker("See-through DWM overlay. Clicks pass through until INSERT opens this menu.");
        ImGui::SameLine();
        if (fuser)
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.34f, 0.24f, 0.10f, 1.f));
        else
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.14f, 0.16f, 0.18f, 1.f));
        if (ImGui::Button("Fuser", ImVec2(buttonW, 32)))
            g.fuserMode = true;
        ImGui::PopStyleColor();
        HelpMarker("Solid background for a second-PC HDMI / capture fuser.");
        if (g.fuserMode)
            ImGui::TextDisabled("Fuser on — solid capture surface.");
        else
            ImGui::TextDisabled("Fuser off — transparent overlay.");
    }

    void PageOverlay(ESPConfig& e, GeneralConfig& g) {
        ImGui::Checkbox("Enable overlay", &e.enabled);
        HelpMarker("Turns ESP drawing off without changing the other toggles.");

        Section("Display mode");
        FuserModePicker(g);
        ImGui::Spacing();
        ImGui::Checkbox("Fullscreen overlay", &g.fullscreenOverlay);
        HelpMarker("Borderless over the selected monitor. Uses override width/height when set.");
        ImGui::SliderInt("Monitor", &g.overlayMonitor, 0, 7);
        HelpMarker("0 is the primary display. Raise this to put the overlay on another screen.");
        ImGui::Checkbox("Stream proof", &g.streamProof);
        ImGui::Checkbox("Overlay VSync", &g.overlayVsync);
        Section("Debug camera");
        ImGui::Checkbox("Freecam / debug camera", &g.freeCam);
        ImGui::SameLine();
        ImGui::TextDisabled("F6");
        HelpMarker("WASD + mouse look on this PC. Writes FollowPosition, BattleCameraModel, and the follow Transform. ESP W2S follows the debug camera.");
        ImGui::SliderFloat("Freecam speed", &g.freeCamSpeed, 2.f, 40.f, "%.0f");

        Section("Draw");
        ImGui::Checkbox("Boxes", &e.drawBoxes);
        ImGui::SameLine();
        ImGui::Checkbox("Names", &e.drawNames);
        ImGui::SameLine();
        ImGui::Checkbox("Distance", &e.drawDistance);
        ImGui::Checkbox("Health", &e.drawHealth);
        ImGui::SameLine();
        ImGui::Checkbox("Snaplines", &e.drawSnaplines);
        ImGui::Checkbox("Aim lines", &e.drawAimLines);
        ImGui::SameLine();
        ImGui::Checkbox("Edge arrows", &e.drawOffscreen);
        ImGui::Checkbox("Skeleton", &e.drawSkeleton);
        ImGui::SameLine();
        ImGui::Checkbox("3D chams", &e.drawChams);
        ImGui::SameLine();
        ImGui::Checkbox("Armor bar", &e.drawArmor);
        const char* chamStyles[] = { "Flat", "Shaded", "Wireframe", "Outline" };
        ImGui::Combo("Cham style", &e.chamStyle, chamStyles, IM_ARRAYSIZE(chamStyles));
        ImGui::Checkbox("Material chams (DMA)", &e.materialChams);
        HelpMarker("Writes CustomizationColorModel skin/hair/beard colors on the player mesh.");
        ImGui::Checkbox("Frag radius", &e.drawFragRadius);
        ImGui::Checkbox("Visible only", &e.visibleOnly);
        HelpMarker("Hides players inside cupboards/bins. Full physics raycast is not in the dump as a readable flag.");
        ImGui::Checkbox("OOF teammates", &e.offscreenTeammates);
        ImGui::SameLine();
        ImGui::Checkbox("OOF dead", &e.offscreenDead);
        ImGui::Checkbox("OOF cops", &e.offscreenCops);
        ImGui::SameLine();
        ImGui::Checkbox("OOF robbers", &e.offscreenRobbers);
        ImGui::Checkbox("OOF scale by distance", &e.offscreenScaleDistance);
        Section("Box");
        const char* styles[] = { "Corners", "Full" };
        ImGui::Combo("Style", &e.boxStyle, styles, IM_ARRAYSIZE(styles));
    }

    void PageVisuals(ESPConfig& e) {
        ImGui::Checkbox("Players", &e.showPlayers);
        ImGui::Checkbox("Cops", &e.showCops);
        ImGui::SameLine();
        ImGui::Checkbox("Robbers", &e.showRobbers);
        ImGui::SameLine();
        ImGui::Checkbox("Civilians", &e.showCivilians);
        ImGui::Checkbox("Hide teammates", &e.hideTeam);
        HelpMarker("Teammates share your clan tag, not your role. Cops are not teammates of other cops.");
        ImGui::Checkbox("Gender", &e.showGender);
        ImGui::SameLine();
        ImGui::Checkbox("Outfit", &e.showOutfit);
        ImGui::Checkbox("Hair", &e.showHair);
        ImGui::SameLine();
        ImGui::Checkbox("Beard", &e.showBeard);
        ImGui::Checkbox("Police uniform", &e.showPoliceUniform);
        ImGui::Checkbox("Hide dead", &e.hideDead);
        ImGui::SameLine();
        ImGui::Checkbox("Hide downed", &e.hideKnocked);
        Section("Distance");
        ImGui::SliderFloat("Players##dist", &e.maxDistancePlayers, 50.f, 2000.f, "%.0f m");
        Section("Colours");
        ColourEdit("Other", e.playerColour);
        ColourEdit("Robber", e.robberColour);
        ColourEdit("Cop", e.copColour);
        ColourEdit("Settler", e.settlerColour);
        ColourEdit("Prisoner", e.prisonerColour);
        ColourEdit("Civilian", e.civilianColour);
        ColourEdit("Team", e.teamColour);
        ColourEdit("Dead", e.deadColour);
        ImGui::Checkbox("Team ESP", &e.teamEsp);
        ImGui::SliderFloat("Team distance", &e.maxDistanceTeam, 50.f, 2000.f, "%.0f m");
        ImGui::TextDisabled("Downed players draw orange.");
    }

    void PageWorld(ESPConfig& e) {
        Section("Show");
        ImGui::Checkbox("Loot", &e.showItems);
        ImGui::SameLine();
        ImGui::Checkbox("Vehicles", &e.showVehicles);
        ImGui::Checkbox("Grenades", &e.showGrenades);
        ImGui::SameLine();
        ImGui::Checkbox("Containers", &e.showContainers);
        ImGui::Checkbox("Empty containers", &e.showEmptyContainers);
        ImGui::Checkbox("Container contents", &e.showContainerContents);
        ImGui::Checkbox("Cameras", &e.showCameras);
        ImGui::SameLine();
        ImGui::Checkbox("Death bags", &e.showDeathBags);
        ImGui::Checkbox("Cargo", &e.showCargo);
        ImGui::SameLine();
        ImGui::Checkbox("ATM", &e.showAtm);
        ImGui::Checkbox("Safes", &e.showSafes);
        ImGui::SameLine();
        ImGui::Checkbox("Vaults", &e.showVaults);
        ImGui::Checkbox("Hide spots", &e.showHideSpots);
        ImGui::SameLine();
        ImGui::Checkbox("Hidden players", &e.showHiddenPlayers);
        ImGui::Checkbox("Lethal grenades", &e.showLethalGrenades);
        ImGui::SameLine();
        ImGui::Checkbox("Tactical grenades", &e.showTacticalGrenades);
        ImGui::Checkbox("Gun icons", &e.showGunIcons);
        ImGui::SameLine();
        ImGui::Checkbox("Item values", &e.showItemValues);
        Section("Loot filters");
        ImGui::Checkbox("Weapons##loot", &e.lootWeapons);
        ImGui::SameLine();
        ImGui::Checkbox("Meds##loot", &e.lootMeds);
        ImGui::Checkbox("Ammo##loot", &e.lootAmmo);
        ImGui::SameLine();
        ImGui::Checkbox("Valuables##loot", &e.lootValuables);
        ImGui::Checkbox("Other loot", &e.lootOther);
        ImGui::SliderFloat("Min item value", &e.minItemValue, 0.f, 50000.f, "%.0f");
        ImGui::Checkbox("Mobs", &e.showMobs);
        HelpMarker("Vigilant NPCs in this build have no world position, so this list stays empty.");
        Section("Distance");
        ImGui::SliderFloat("Loot##dist", &e.maxDistanceItems, 20.f, 500.f, "%.0f m");
        ImGui::SliderFloat("Vehicles##dist", &e.maxDistanceVehicles, 50.f, 2000.f, "%.0f m");
        ImGui::SliderFloat("Grenades##dist", &e.maxDistanceGrenades, 20.f, 500.f, "%.0f m");
        ImGui::SliderFloat("Containers##dist", &e.maxDistanceContainers, 10.f, 300.f, "%.0f m");
        Section("Colours");
        ColourEdit("Loot", e.itemColour);
        ColourEdit("Vehicle", e.vehicleColour);
        ColourEdit("Lethal nade", e.lethalGrenadeColour);
        ColourEdit("Tactical nade", e.tacticalGrenadeColour);
        ColourEdit("Container", e.containerColour);
        ColourEdit("Camera", e.cameraColour);
        ColourEdit("Death bag", e.deathBagColour);
        ColourEdit("Cargo", e.cargoColour);
        ColourEdit("ATM", e.atmColour);
        ColourEdit("Safe", e.safeColour);
        ColourEdit("Vault", e.vaultColour);
    }

    void PageRadar(RadarConfig& r) {
        ImGui::Checkbox("Enable radar", &r.enabled);
        if (!r.enabled) return;
        Section("View");
        ImGui::SliderFloat("Zoom", &r.zoom, 0.1f, 5.f, "%.1f");
        ImGui::SliderFloat("Size", &r.size, 200.f, 640.f, "%.0f");
        ImGui::SliderFloat("Position X", &r.posX, -1.f, 1800.f, "%.0f");
        ImGui::SliderFloat("Position Y", &r.posY, -1.f, 1000.f, "%.0f");
        HelpMarker("-1 keeps the default top-right corner. Raise X/Y to pin the minimap.");
        ImGui::Checkbox("Rotate with camera", &r.rotateWithCamera);
        ImGui::Checkbox("Arrow blips", &r.arrowBlips);
        ImGui::SameLine();
        ImGui::Checkbox("Role colours", &r.roleColours);
        ImGui::Checkbox("Big radar", &r.bigRadar);
        ImGui::SameLine();
        ImGui::Checkbox("Follow", &r.follow);
        Section("Show");
        ImGui::Checkbox("Players##radar", &r.showPlayers);
        ImGui::SameLine();
        ImGui::Checkbox("Loot##radar", &r.showItems);
        ImGui::Checkbox("Vehicles##radar", &r.showVehicles);
        ImGui::SameLine();
        ImGui::Checkbox("Grenades##radar", &r.showGrenades);
        ImGui::Checkbox("Containers##radar", &r.showContainers);
        Section("Colours");
        ColourEdit("Background", r.backgroundColour);
        ColourEdit("Player", r.playerColour);
        ColourEdit("Loot", r.itemColour);
        ColourEdit("Vehicle", r.vehicleColour);
        ColourEdit("Grenade", r.grenadeColour);
        ColourEdit("Container", r.containerColour);
        ColourEdit("You", r.localColour);
    }

    void PageTeam(ESPConfig& e) {
        ImGui::Checkbox("Team ESP", &e.teamEsp);
        ImGui::SliderFloat("Team max distance", &e.maxDistanceTeam, 50.f, 2000.f, "%.0f m");
        ColourEdit("Teammate", e.teamColour);
        ImGui::TextDisabled("Teammates share your clan tag. Cops and robbers are factions, not teams.");
        ImGui::Checkbox("OOF arrows for teammates", &e.offscreenTeammates);
    }

    void PageBases(ESPConfig& e, const std::vector<GameEntity>& entities, const GameEntity& local) {
        ImGui::Checkbox("Bases ESP", &e.showBases);
        ImGui::Checkbox("Bases panel", &e.basePanel);
        ImGui::Checkbox("Raid alerts", &e.raidAlerts);
        ImGui::SameLine();
        ImGui::Checkbox("Open-door warning", &e.openDoorWarning);
        Section("Filters");
        ImGui::Checkbox("Your base", &e.baseMine);
        ImGui::SameLine();
        ImGui::Checkbox("Your clan", &e.baseClan);
        ImGui::Checkbox("Everyone else", &e.baseOthers);
        ImGui::Checkbox("Raidable only", &e.baseRaidableOnly);
        ImGui::Checkbox("Has loot", &e.baseHasLootOnly);
        ImGui::Checkbox("Hide raid-locked", &e.baseHideRaidLocked);
        ImGui::Checkbox("Hide unsettled", &e.baseHideUnsettled);
        ImGui::SliderFloat("Base distance", &e.maxDistanceBases, 50.f, 2000.f, "%.0f m");
        Section("Colours");
        ColourEdit("Other bases", e.baseColour);
        ColourEdit("Your base", e.myBaseColour);
        ColourEdit("Clan base", e.clanBaseColour);
        Section("Settlement");
        int n = 0, loot = 0, vip = 0;
        for (const auto& ent : entities) {
            if (ent.type != EntityType::Base) continue;
            ++n;
            if (ent.hasLoot) ++loot;
            if (ent.vip) ++vip;
        }
        ImGui::Text("%d bases   loot %d   VIP %d", n, loot, vip);
        ImGui::TextDisabled("You: %s", local.displayName.empty() ? "-" : local.displayName.c_str());
        Section("Inspector");
        const GameEntity* pick = nullptr;
        float best = 1.e9f;
        for (const auto& ent : entities) {
            if (ent.type != EntityType::Base) continue;
            float d = ent.DistanceTo(local.position);
            if (d < best) { best = d; pick = &ent; }
        }
        if (pick) {
            ImGui::Text("%s  %.0fm", pick->displayName.c_str(), best);
            if (!pick->clan.empty()) ImGui::Text("Clan [%s]", pick->clan.c_str());
            ImGui::Text("Owner %lld", static_cast<long long>(pick->ownerPublicId));
            if (pick->maxDefensePoints > 0)
                ImGui::Text("Fuse %d / %d", pick->defensePoints, pick->maxDefensePoints);
            ImGui::TextDisabled("%s%s%s%s",
                pick->vip ? "VIP  " : "",
                pick->hasLoot ? "loot  " : "",
                pick->openDoors ? "doors  " : "",
                pick->beingRaided ? "RAID" : "");
        }
        Section("Maps");
        ImGui::TextWrapped("Gameplay maps and visibility worlds load from the live settlement. Drop PNG files in assets/maps next to the overlay to use them on Big Radar.");
        if (ImGui::Button("Refresh maps from settlement", ImVec2(260, 28)))
            ImGui::TextDisabled("Settlement markers are live on the radar.");
    }

    void PageRoster(const std::vector<GameEntity>& entities, const GameEntity& localPlayer) {
        std::vector<const GameEntity*> players;
        for (const auto& ent : entities) {
            if (ent.type == EntityType::Player && ent.IsValid() && !ent.isLocalPlayer)
                players.push_back(&ent);
        }
        std::sort(players.begin(), players.end(), [&](const GameEntity* a, const GameEntity* b) {
            return a->DistanceTo(localPlayer.position) < b->DistanceTo(localPlayer.position);
        });

        ImGui::TextDisabled("%d nearby", static_cast<int>(players.size()));
        if (ImGui::BeginTable("##roster", 5,
                ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_ScrollY,
                ImVec2(0, 0))) {
            ImGui::TableSetupColumn("Name");
            ImGui::TableSetupColumn("Role");
            ImGui::TableSetupColumn("State");
            ImGui::TableSetupColumn("HP", ImGuiTableColumnFlags_WidthFixed, 48.f);
            ImGui::TableSetupColumn("m", ImGuiTableColumnFlags_WidthFixed, 48.f);
            ImGui::TableHeadersRow();
            for (const GameEntity* ent : players) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(ent->displayName.c_str());
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(ent->role.empty() ? "-" : ent->role.c_str());
                ImGui::TableSetColumnIndex(2);
                const char* state = !ent->isAlive ? "Dead" : (ent->isKnocked ? ent->extra.c_str() : "Up");
                ImGui::TextUnformatted(state);
                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%.0f", ent->health);
                ImGui::TableSetColumnIndex(4);
                ImGui::Text("%.0f", ent->DistanceTo(localPlayer.position));
            }
            ImGui::EndTable();
        }
    }

    void PageAim(AssistConfig& assist, GeneralConfig& general, const CameraData& camera) {
        auto& aim = assist.aim;

        Section("Aim");
        ImGui::Checkbox("Enable aim", &aim.enabled);
        ImGui::SameLine();
        ImGui::TextDisabled("F2");
        ImGui::SameLine();
        ImGui::Checkbox("Draw FOV", &aim.drawFov);
        ImGui::Checkbox("Memory aim (DMA)", &aim.memoryAim);
        HelpMarker(
            "Memory aim writes client yaw/pitch on the position thread. "
            "With memory aim off, deltas go out KMBox serial to the game PC mouse. "
            "Hold the aim key on this overlay PC. Smoothing 1 snaps; higher values ease in.");
        ImGui::Checkbox("Silent aim", &aim.silentAim);
        HelpMarker(
            "Does not move the camera or mouse. Rewrites LookDirectionModel, "
            "LookDirectionInput, ShotInterpolation, and ShotArgs.Direction so the "
            "shot ray aims at the target while the crosshair stays put. Uses the same FOV and prediction.");
        ImGui::Combo("Aim key", &aim.aimKey, kKeyNames);
        ImGui::Combo("Second aim key", &aim.aimKey2, kSecondKeyNames);
        if (!aim.enabled)
            ImGui::TextDisabled("Aim is off. Turn it on, then hold the aim key.");
        else
            ImGui::TextDisabled("%s", Aimbot::LastStatus());

        ImGui::SliderFloat("FOV (px)", &aim.fov, 20.f, 400.f, "%.0f");
        ImGui::Checkbox("Dynamic FOV", &aim.dynamicFov);
        if (aim.dynamicFov)
            ImGui::SliderFloat("FOV far (px)", &aim.fovMin, 10.f, 200.f, "%.0f");
        const char* smoothModes[] = { "Linear", "EMA", "Bezier" };
        ImGui::Combo("Smoothing mode", &aim.smoothMode, smoothModes, IM_ARRAYSIZE(smoothModes));
        ImGui::SliderFloat("Smoothing", &aim.smooth, 1.f, 30.f, "%.1f");
        HelpMarker("1 snaps onto the dot. Higher values ease onto it more slowly. EMA holds a running average; Bezier eases in and out.");
        const char* priorityModes[] = { "Closest to FOV", "Closest distance", "Lowest health" };
        ImGui::Combo("Target priority", &aim.priorityMode, priorityModes, IM_ARRAYSIZE(priorityModes));
        ImGui::Checkbox("Target lock", &aim.targetLock);
        HelpMarker("Keeps the same player while the aim key is held.");
        ImGui::Checkbox("Visible only", &aim.visibleOnly);
        HelpMarker("Skips players inside hide objects. No cheap physics raycast exists on the DMA path.");
        ImGui::Checkbox("Prediction", &aim.prediction);
        ImGui::SliderFloat("Deadzone (px)", &aim.deadzone, 0.f, 20.f, "%.1f");
        ImGui::SliderFloat("Lead (ms)", &aim.leadMs, 0.f, 120.f, "%.0f");
        HelpMarker("Memory aim predicts the target this far ahead. Default 45 ms.");
        ImGui::SliderFloat("Lead (px)", &aim.leadPx, 0.f, 20.f, "%.0f");
        ImGui::SliderFloat("Max distance (m)", &aim.maxDistance, 25.f, 600.f, "%.0f");

        Section("Gun");
        ImGui::Checkbox("No recoil", &aim.noRecoil);
        ImGui::SameLine();
        ImGui::Checkbox("No sway", &aim.noSway);
        ImGui::SameLine();
        ImGui::Checkbox("No spread", &aim.noSpread);

        Section("Hitboxes");
        ImGui::Checkbox("Head", &aim.targetHead);
        ImGui::SameLine();
        ImGui::Checkbox("Neck", &aim.targetNeck);
        ImGui::Checkbox("Chest", &aim.targetChest);
        ImGui::SameLine();
        ImGui::Checkbox("Stomach", &aim.targetStomach);
        ImGui::Checkbox("Pelvis", &aim.targetPelvis);
        ImGui::Checkbox("Team check", &aim.teamCheck);
        HelpMarker("Skips clanmates. Cops are not teammates of other cops.");
        ImGui::Checkbox("Target knocked", &aim.targetKnocked);

        Section("Triggerbot");
        ImGui::Checkbox("Triggerbot", &aim.triggerbot);
        HelpMarker("Writes BattleModel.ShotInputModel._isTriggered and FireModel._isFiring over DMA.");
        ImGui::Combo("Trigger key", &aim.triggerKey, kSecondKeyNames);
        ImGui::SliderFloat("Delay min (ms)", &aim.triggerDelayMin, 0.f, 200.f, "%.0f");
        ImGui::SliderFloat("Delay max (ms)", &aim.triggerDelayMax, 0.f, 250.f, "%.0f");
        ImGui::Checkbox("Trigger visible only", &aim.triggerVisibleOnly);
        ImGui::SameLine();
        ImGui::Checkbox("Trigger team check", &aim.triggerTeamCheck);

        Section("Aim resolution");
        if (general.gameScreenWidth > 0 && general.gameScreenHeight > 0) {
            ImGui::Text("Override %d x %d", general.gameScreenWidth, general.gameScreenHeight);
        } else if (camera.gameWidth > 0 && camera.gameHeight > 0) {
            ImGui::Text("Game %d x %d", camera.gameWidth, camera.gameHeight);
        } else {
            ImGui::TextDisabled("Reading the game resolution...");
        }
        ImGui::InputInt("Override width (0 = automatic)", &general.gameScreenWidth);
        ImGui::InputInt("Override height", &general.gameScreenHeight);

        Section("KMBox USB");
        ImGui::InputInt("COM port (0 = scan)", &assist.comPort);
        ImGui::Checkbox("Auto-connect on start", &assist.autoConnect);
        HelpMarker(
            "Serial cable must reach the PC that runs Connect (usually the second PC). "
            "HID USB goes to the game PC.");
        auto& km = KmboxUsb::Get();
        if (km.IsConnected()) {
            ImGui::TextColored(kAccent, "Serial: COM%d @ %d — %s",
                km.GetComPort(), km.GetBaud(), km.Version().c_str());
        } else {
            const std::string err = km.LastError();
            ImGui::TextColored(ImVec4(1.f, 0.45f, 0.4f, 1.f), "Serial: disconnected");
            if (!err.empty())
                ImGui::TextWrapped("%s", err.c_str());
        }
        if (ImGui::Button("Connect", ImVec2(90, 28))) {
            if (km.Connect(assist.comPort))
                km.SetAcProfileName(assist.acProfile);
        }
        ImGui::SameLine();
        if (ImGui::Button("Disconnect", ImVec2(100, 28)))
            km.Disconnect();
        ImGui::SameLine();
        if (ImGui::Button("Test move", ImVec2(100, 28)))
            km.TestMouseMove();
        if (km.IsConnected())
            ImGui::TextDisabled("Last queued move: %d, %d", km.LastMoveX(), km.LastMoveY());

        Section("On-box flash (second PC)");
        const char* acNames = "Rob Run\0EAC\0BattlEye\0ACE\0";
        int acIdx = 0;
        std::string acLower = assist.acProfile;
        std::transform(acLower.begin(), acLower.end(), acLower.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (acLower == "eac") acIdx = 1;
        else if (acLower == "be" || acLower == "battleye") acIdx = 2;
        else if (acLower == "ace") acIdx = 3;
        if (ImGui::Combo("AC serial profile", &acIdx, acNames)) {
            static const char* kAc[] = {"robrun", "eac", "be", "ace"};
            assist.acProfile = kAc[acIdx];
        }
        const char* hidPresets = "Logitech LIGHTSPEED C547\0Stock KMBox\0Custom VID/PID\0";
        int hidIdx = 0;
        std::string presetLower = assist.hidPreset;
        std::transform(presetLower.begin(), presetLower.end(), presetLower.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (presetLower == "stock")
            hidIdx = 1;
        else if (presetLower == "custom")
            hidIdx = 2;
        if (ImGui::Combo("HID identity preset", &hidIdx, hidPresets)) {
            static const char* kPreset[] = {
                "logitech_lightspeed_c547", "stock", "custom"
            };
            assist.hidPreset = kPreset[hidIdx];
            if (hidIdx == 0) {
                assist.hidVid = "046d";
                assist.hidPid = "c547";
                assist.hidManufacturer = "Logitech";
                assist.hidProduct = "USB Receiver";
                assist.hidBcdDevice = "0110";
            } else if (hidIdx == 1) {
                assist.hidVid.clear();
                assist.hidPid.clear();
                assist.hidManufacturer.clear();
                assist.hidProduct.clear();
                assist.hidBcdDevice.clear();
            }
        }
        const bool customHid = hidIdx == 2;
        if (hidIdx == 0) {
            ImGui::TextDisabled("Default: 046D:C547 (this PC's LIGHTSPEED receiver) + Logitech strings.");
        }
        char vidBuf[16]{};
        char pidBuf[16]{};
        char mfgBuf[64]{};
        char prodBuf[64]{};
        strncpy_s(vidBuf, assist.hidVid.c_str(), sizeof(vidBuf) - 1);
        strncpy_s(pidBuf, assist.hidPid.c_str(), sizeof(pidBuf) - 1);
        strncpy_s(mfgBuf, assist.hidManufacturer.c_str(), sizeof(mfgBuf) - 1);
        strncpy_s(prodBuf, assist.hidProduct.c_str(), sizeof(prodBuf) - 1);
        if (customHid) {
            ImGui::InputText("HID VID (hex)", vidBuf, sizeof(vidBuf));
            ImGui::InputText("HID PID (hex)", pidBuf, sizeof(pidBuf));
            ImGui::InputText("Manufacturer (optional)", mfgBuf, sizeof(mfgBuf));
            ImGui::InputText("Product (optional)", prodBuf, sizeof(prodBuf));
            assist.hidVid = vidBuf;
            assist.hidPid = pidBuf;
            assist.hidManufacturer = mfgBuf;
            assist.hidProduct = prodBuf;
        }
        if (ImGui::Button("Write KMBox sidecar files", ImVec2(200, 28)))
            Config::WriteKmboxSidecars();
        ImGui::SameLine();
        if (ImGui::Button("Save config", ImVec2(100, 28)))
            Config::Save();
        HelpMarker(
            "Writes kmbox_identity.txt (v3) and ac profile. On second PC run "
            "Capture-GameMouseIdentity.ps1 to pull VID/PID from the game mouse, or "
            "Flash-KMBox.bat after writing. Power-cycle both USBs on the box.");
    }

    void PageSystem(AppConfig& cfg, const CameraData& camera) {
        auto& g = cfg.general;
        Section("Process");
        char procBuf[256];
        strncpy_s(procBuf, g.processName.c_str(), sizeof(procBuf) - 1);
        if (ImGui::InputText("Process", procBuf, sizeof(procBuf)))
            g.processName = procBuf;

        Section("Workers");
        ImGui::TextDisabled("Globals %d ms | Actors %d | Positions %d | Skeleton %d | Bones max %d",
            g.updateRateGlobals, g.updateRateActors, g.updateRatePositions,
            g.skeletonRefreshMs, g.maxBonePlayers);
        ImGui::SliderInt("Globals ms", &g.updateRateGlobals, 100, 5000);
        ImGui::SliderInt("Positions ms", &g.updateRatePositions, 1, 8);
        ImGui::SliderInt("Skeleton ms", &g.skeletonRefreshMs, 1, 200);
        ImGui::SliderInt("Actors ms", &g.updateRateActors, 8, 500);
        ImGui::SliderInt("Scatter chunk", &g.scatterMaxPrepares, 128, 2048);
        ImGui::SliderInt("Max bone players", &g.maxBonePlayers, 4, 300);

        Section("Widgets");
        ImGui::Checkbox("On-screen widgets", &g.widgets);
        ImGui::SameLine();
        ImGui::Checkbox("Server dashboard", &g.serverDashboard);

        Section("Config");
        if (ImGui::Button("Save", ImVec2(90, 28))) Config::Save();
        ImGui::SameLine();
        if (ImGui::Button("Load", ImVec2(90, 28))) Config::Load();
        ImGui::SameLine();
        if (ImGui::Button("Reset", ImVec2(90, 28))) cfg = AppConfig{};

        Section("Changelog");
        ImGui::TextWrapped("Added silent aim: shot look writes LookDirection/ShotArgs without moving the camera.");
        ImGui::TextWrapped("Added triggerbot, EMA/Bezier smoothing, stomach bone, cham styles, material color writes, armor bars, loot filters, Segoe UI ESP font, and DMA freecam (F6).");
        ImGui::TextWrapped("Added base ESP, hidden-in-prop ESP, 3D chams, camera/death-bag/ATM ESP, team ESP, OOF arrows, big radar, item values.");
        ImGui::TextWrapped("Fixed player dropping from ESP thanks for feedback testers <3");
        ImGui::TextDisabled("Writes stay memory/DMA based.");

        Section("Link");
        auto& mem = Memory::Get();
        if (mem.IsConnected()) {
            ImGui::TextColored(ImVec4(0.55f, 0.85f, 0.55f, 1.f), "Connected");
            ImGui::SameLine();
            ImGui::TextDisabled("pid %u", mem.GetPID());
        } else {
            ImGui::TextColored(ImVec4(0.9f, 0.45f, 0.35f, 1.f), "Waiting");
        }
        if (!camera.status.empty())
            ImGui::TextWrapped("%s", camera.status.c_str());
        ImGui::TextDisabled("Camera %s  gen %llu  age %.0f ms",
            camera.valid ? "locked" : "no lock",
            static_cast<unsigned long long>(camera.generation),
            camera.AgeMs());
        ImGui::TextDisabled("INSERT toggles this menu. Uninject is on the footer of every page.");
    }

    void DrawUninjectPopup() {
        ImGui::SetNextWindowSize(ImVec2(360, 0), ImGuiCond_Appearing);
        if (ImGui::BeginPopupModal("Uninject", nullptr,
                ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextWrapped("Close the overlay, stop workers, and disconnect the FPGA / VMM.");
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.52f, 0.16f, 0.14f, 1.f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.68f, 0.20f, 0.16f, 1.f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.80f, 0.24f, 0.18f, 1.f));
            if (ImGui::Button("Uninject", ImVec2(140, 32))) {
                ImGui::CloseCurrentPopup();
                RequestUninjectAndDisconnect();
            }
            ImGui::PopStyleColor(3);
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(140, 32)))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
    }

    void DrawFooter(GeneralConfig& g) {
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, 6));

        const bool fuser = g.fuserMode;
        if (!fuser)
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.10f, 0.30f, 0.26f, 1.f));
        else
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.14f, 0.16f, 0.18f, 1.f));
        if (ImGui::Button("Transparent", ImVec2(118, 32)))
            g.fuserMode = false;
        ImGui::PopStyleColor();
        ImGui::SameLine();
        if (fuser)
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.34f, 0.24f, 0.10f, 1.f));
        else
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.14f, 0.16f, 0.18f, 1.f));
        if (ImGui::Button("Fuser", ImVec2(80, 32)))
            g.fuserMode = true;
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled(g.fuserMode ? "solid 2nd-PC" : "see-through");
        ImGui::SameLine();
        {
            const float rest = ImGui::GetContentRegionAvail().x - 198.f;
            if (rest > 8.f) {
                ImGui::Dummy(ImVec2(rest, 1.f));
                ImGui::SameLine();
            }
        }
        if (ImGui::Button("Save", ImVec2(72, 32)))
            Config::Save();
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.52f, 0.16f, 0.14f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.68f, 0.20f, 0.16f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.80f, 0.24f, 0.18f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.92f, 0.90f, 1.f));
        if (ImGui::Button("Uninject", ImVec2(110, 32)))
            ImGui::OpenPopup("Uninject");
        ImGui::PopStyleColor(4);
        DrawUninjectPopup();
    }
}

bool Menu::IsOpen() {
    return g_open;
}

void Menu::Open() {
    g_open = true;
}

void Menu::Close() {
    g_open = false;
}

void Menu::Render(const std::vector<GameEntity>& entities,
                  const GameEntity& localPlayer,
                  const CameraData& camera) {
    PollInsert();
    if (!g_open)
        return;

    auto& cfg = Config::Get();
    int nP = 0, nLoot = 0, nV = 0, nG = 0, nC = 0;
    for (const auto& e : entities) {
        switch (e.type) {
            case EntityType::Player:    ++nP; break;
            case EntityType::Item:      ++nLoot; break;
            case EntityType::Vehicle:   ++nV; break;
            case EntityType::Grenade:   ++nG; break;
            case EntityType::Container: ++nC; break;
            default: break;
        }
    }

    ImGui::SetNextWindowPos(ImVec2(28, 28), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(780, 580), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(660, 440), ImVec2(1100, 920));
    ImGui::SetNextWindowBgAlpha(0.97f);

    ImGui::Begin("##RobRun", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 origin = ImGui::GetCursorScreenPos();
    float width = ImGui::GetContentRegionAvail().x;
    dl->AddRectFilled(origin, ImVec2(origin.x + 4.f, origin.y + 34.f), IM_COL32(108, 210, 188, 220), 2.f);

    ImGui::Dummy(ImVec2(10, 0));
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, kAccent);
    ImGui::TextUnformatted("ROB RUN");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::TextDisabled("DMA");
    ImGui::SameLine(0.f, 12.f);

    auto& mem = Memory::Get();
    if (mem.IsConnected())
        Chip("LIVE", IM_COL32(28, 92, 64, 255), IM_COL32(170, 240, 200, 255));
    else
        Chip("WAIT", IM_COL32(92, 40, 32, 255), IM_COL32(255, 190, 170, 255));
    ImGui::SameLine();
    if (cfg.general.fuserMode)
        Chip("FUSER", IM_COL32(92, 64, 24, 255), IM_COL32(255, 214, 140, 255));
    else
        Chip("TRANSPARENT", IM_COL32(24, 72, 68, 255), IM_COL32(160, 230, 220, 255));
    ImGui::SameLine();
    char ageBuf[48];
    snprintf(ageBuf, sizeof(ageBuf), "SNAP %.0fms", camera.AgeMs());
    Chip(ageBuf,
         camera.AgeMs() > 80.f ? IM_COL32(92, 48, 28, 255) : IM_COL32(32, 40, 48, 255),
         IM_COL32(210, 220, 228, 255));

    ImGui::SameLine();
    ImGui::TextColored(kMuted, "  %.0f fps", ImGui::GetIO().Framerate);
    ImGui::SameLine();
    ImGui::TextColored(kDim, "  P %d  Loot %d  V %d  G %d  C %d", nP, nLoot, nV, nG, nC);

    ImGui::Dummy(ImVec2(0, 10));
    dl->AddLine(
        ImVec2(origin.x, ImGui::GetCursorScreenPos().y),
        ImVec2(origin.x + width, ImGui::GetCursorScreenPos().y),
        IM_COL32(108, 210, 188, 70), 1.f);
    ImGui::Dummy(ImVec2(0, 8));

    const float side = 128.f;
    const float footerH = 52.f;
    ImGui::BeginChild("##nav", ImVec2(side, -footerH), false, ImGuiWindowFlags_NoScrollbar);
    for (int i = 0; i < PageCount; ++i) {
        bool active = g_page == i;
        if (active) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.26f, 0.24f, 1.f));
            ImGui::PushStyleColor(ImGuiCol_Text, kAccent);
        }
        if (ImGui::Button(kPages[i], ImVec2(-1, 28)))
            g_page = i;
        if (active)
            ImGui::PopStyleColor(2);
    }
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("##page", ImVec2(0, -footerH), false);
    switch (g_page) {
        case Overlay:   PageOverlay(cfg.esp, cfg.general); break;
        case Visuals:   PageVisuals(cfg.esp); break;
        case World:     PageWorld(cfg.esp); break;
        case RadarPage: PageRadar(cfg.radar); break;
        case TeamPage:  PageTeam(cfg.esp); break;
        case BasesPage: PageBases(cfg.esp, entities, localPlayer); break;
        case Roster:    PageRoster(entities, localPlayer); break;
        case AimPage:   PageAim(cfg.assist, cfg.general, camera); break;
        case System:    PageSystem(cfg, camera); break;
        default: break;
    }
    ImGui::EndChild();

    DrawFooter(cfg.general);

    ImGui::End();
}
