#include "config.h"
#include "kmbox_identity.h"
#include <Windows.h>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <algorithm>
#include "json.hpp"

using json = nlohmann::json;

static AppConfig s_config;

namespace {
    json ColourToJson(const Colour& c) {
        return { {"r", c.r}, {"g", c.g}, {"b", c.b}, {"a", c.a} };
    }

    Colour ColourFromJson(const json& j, const Colour& fallback) {
        Colour c = fallback;
        if (j.contains("r")) c.r = j["r"].get<float>();
        if (j.contains("g")) c.g = j["g"].get<float>();
        if (j.contains("b")) c.b = j["b"].get<float>();
        if (j.contains("a")) c.a = j["a"].get<float>();
        return c;
    }

    template<typename T>
    T SafeGet(const json& j, const std::string& key, T fallback) {
        if (j.contains(key)) {
            try { return j[key].get<T>(); } catch (...) {}
        }
        return fallback;
    }
}

AppConfig& Config::Get() {
    return s_config;
}

void Config::ApplyRecommendedPipeline() {
    AutoTuneWorkersForEsp(0, false);
}

void Config::AutoTuneWorkersForEsp(int enabledPlayers, bool inMatch) {
    if (enabledPlayers < 0)
        enabledPlayers = 0;
    auto& g = s_config.general;
    g.overlayVsync = false;
    // SNAP is camera.AgeMs(). A 1ms position period with extras/bones on the
    // same FPGA tick overruns to ~1000ms the moment anyone moves.
    if (inMatch) {
        if (g.updateRatePositions < 8 || g.updateRatePositions > 50)
            g.updateRatePositions = 16;
        g.updateRateGlobals = 300;
        if (g.updateRateActors < 32)
            g.updateRateActors = 40;
        if (g.skeletonRefreshMs < 32)
            g.skeletonRefreshMs = 50;
        g.maxBonePlayers = (std::clamp)(enabledPlayers > 0 ? (std::min)(enabledPlayers, 8) : 8, 4, 12);
        g.scatterMaxPrepares = 128;
    } else {
        g.updateRateGlobals = 50;
        if (g.updateRateActors < 16)
            g.updateRateActors = 16;
        g.maxBonePlayers = 8;
        g.scatterMaxPrepares = 64;
    }
}

bool Config::Save(const std::string& path) {
    json j;

    auto& g = s_config.general;
    j["general"] = {
        {"processName", g.processName},
        {"overlayVsync", g.overlayVsync},
        {"streamProof", g.streamProof},
        {"fuserMode", g.fuserMode},
        {"fullscreenOverlay", g.fullscreenOverlay},
        {"overlayMonitor", g.overlayMonitor},
        {"gameScreenWidth", g.gameScreenWidth},
        {"gameScreenHeight", g.gameScreenHeight},
        {"serverDashboard", g.serverDashboard},
        {"widgets", g.widgets},
        {"freeCam", g.freeCam},
        {"freeCamSpeed", g.freeCamSpeed}
    };

    auto& e = s_config.esp;
    j["esp"] = {
        {"enabled", e.enabled},
        {"showPlayers", e.showPlayers},
        {"showCops", e.showCops},
        {"showRobbers", e.showRobbers},
        {"showCivilians", e.showCivilians},
        {"hideTeam", e.hideTeam},
        {"hideDead", e.hideDead},
        {"hideKnocked", e.hideKnocked},
        {"showItems", e.showItems},
        {"showVehicles", e.showVehicles},
        {"showMobs", e.showMobs},
        {"showGrenades", e.showGrenades},
        {"showLethalGrenades", e.showLethalGrenades},
        {"showTacticalGrenades", e.showTacticalGrenades},
        {"showContainers", e.showContainers},
        {"showEmptyContainers", e.showEmptyContainers},
        {"showContainerContents", e.showContainerContents},
        {"showCameras", e.showCameras},
        {"showDeathBags", e.showDeathBags},
        {"showCargo", e.showCargo},
        {"showAtm", e.showAtm},
        {"showSafes", e.showSafes},
        {"showVaults", e.showVaults},
        {"showHideSpots", e.showHideSpots},
        {"showHiddenPlayers", e.showHiddenPlayers},
        {"showBases", e.showBases},
        {"showGunIcons", e.showGunIcons},
        {"showItemValues", e.showItemValues},
        {"drawChams", e.drawChams},
        {"chamStyle", e.chamStyle},
        {"materialChams", e.materialChams},
        {"drawFragRadius", e.drawFragRadius},
        {"drawArmor", e.drawArmor},
        {"showGender", e.showGender},
        {"showOutfit", e.showOutfit},
        {"showHair", e.showHair},
        {"showBeard", e.showBeard},
        {"showPoliceUniform", e.showPoliceUniform},
        {"visibleOnly", e.visibleOnly},
        {"lootWeapons", e.lootWeapons},
        {"lootMeds", e.lootMeds},
        {"lootAmmo", e.lootAmmo},
        {"lootValuables", e.lootValuables},
        {"lootOther", e.lootOther},
        {"drawBoxes", e.drawBoxes},
        {"drawSkeleton", e.drawSkeleton},
        {"drawNames", e.drawNames},
        {"drawDistance", e.drawDistance},
        {"drawHealth", e.drawHealth},
        {"drawSnaplines", e.drawSnaplines},
        {"drawAimLines", e.drawAimLines},
        {"drawOffscreen", e.drawOffscreen},
        {"offscreenTeammates", e.offscreenTeammates},
        {"offscreenDead", e.offscreenDead},
        {"offscreenCops", e.offscreenCops},
        {"offscreenRobbers", e.offscreenRobbers},
        {"offscreenScaleDistance", e.offscreenScaleDistance},
        {"boxStyle", e.boxStyle},
        {"maxDistancePlayers", e.maxDistancePlayers},
        {"maxDistanceItems", e.maxDistanceItems},
        {"maxDistanceVehicles", e.maxDistanceVehicles},
        {"maxDistanceMobs", e.maxDistanceMobs},
        {"maxDistanceGrenades", e.maxDistanceGrenades},
        {"maxDistanceContainers", e.maxDistanceContainers},
        {"maxDistanceCameras", e.maxDistanceCameras},
        {"maxDistanceBases", e.maxDistanceBases},
        {"minItemValue", e.minItemValue},
        {"teamEsp", e.teamEsp},
        {"teamColour", ColourToJson(e.teamColour)},
        {"maxDistanceTeam", e.maxDistanceTeam},
        {"baseMine", e.baseMine},
        {"baseClan", e.baseClan},
        {"baseOthers", e.baseOthers},
        {"baseRaidableOnly", e.baseRaidableOnly},
        {"baseHasLootOnly", e.baseHasLootOnly},
        {"baseHideRaidLocked", e.baseHideRaidLocked},
        {"baseHideUnsettled", e.baseHideUnsettled},
        {"basePanel", e.basePanel},
        {"raidAlerts", e.raidAlerts},
        {"openDoorWarning", e.openDoorWarning},
        {"playerColour", ColourToJson(e.playerColour)},
        {"robberColour", ColourToJson(e.robberColour)},
        {"copColour", ColourToJson(e.copColour)},
        {"settlerColour", ColourToJson(e.settlerColour)},
        {"prisonerColour", ColourToJson(e.prisonerColour)},
        {"civilianColour", ColourToJson(e.civilianColour)},
        {"itemColour", ColourToJson(e.itemColour)},
        {"vehicleColour", ColourToJson(e.vehicleColour)},
        {"mobColour", ColourToJson(e.mobColour)},
        {"grenadeColour", ColourToJson(e.grenadeColour)},
        {"lethalGrenadeColour", ColourToJson(e.lethalGrenadeColour)},
        {"tacticalGrenadeColour", ColourToJson(e.tacticalGrenadeColour)},
        {"containerColour", ColourToJson(e.containerColour)},
        {"cameraColour", ColourToJson(e.cameraColour)},
        {"deathBagColour", ColourToJson(e.deathBagColour)},
        {"cargoColour", ColourToJson(e.cargoColour)},
        {"atmColour", ColourToJson(e.atmColour)},
        {"safeColour", ColourToJson(e.safeColour)},
        {"vaultColour", ColourToJson(e.vaultColour)},
        {"baseColour", ColourToJson(e.baseColour)},
        {"myBaseColour", ColourToJson(e.myBaseColour)},
        {"clanBaseColour", ColourToJson(e.clanBaseColour)},
        {"deadColour", ColourToJson(e.deadColour)}
    };

    auto& r = s_config.radar;
    j["radar"] = {
        {"enabled", r.enabled},
        {"zoom", r.zoom},
        {"size", r.size},
        {"posX", r.posX},
        {"posY", r.posY},
        {"rotateWithCamera", r.rotateWithCamera},
        {"arrowBlips", r.arrowBlips},
        {"roleColours", r.roleColours},
        {"bigRadar", r.bigRadar},
        {"follow", r.follow},
        {"showPlayers", r.showPlayers},
        {"showItems", r.showItems},
        {"showVehicles", r.showVehicles},
        {"showMobs", r.showMobs},
        {"showGrenades", r.showGrenades},
        {"showContainers", r.showContainers},
        {"showBases", r.showBases},
        {"showCameras", r.showCameras},
        {"backgroundColour", ColourToJson(r.backgroundColour)},
        {"playerColour", ColourToJson(r.playerColour)},
        {"itemColour", ColourToJson(r.itemColour)},
        {"vehicleColour", ColourToJson(r.vehicleColour)},
        {"mobColour", ColourToJson(r.mobColour)},
        {"grenadeColour", ColourToJson(r.grenadeColour)},
        {"containerColour", ColourToJson(r.containerColour)},
        {"localColour", ColourToJson(r.localColour)}
    };

    auto& a = s_config.assist;
    j["assist"] = {
        {"comPort", a.comPort},
        {"autoConnect", a.autoConnect},
        {"acProfile", a.acProfile},
        {"hidPreset", a.hidPreset},
        {"hidVid", a.hidVid},
        {"hidPid", a.hidPid},
        {"hidManufacturer", a.hidManufacturer},
        {"hidProduct", a.hidProduct},
        {"hidSerial", a.hidSerial},
        {"hidBcdDevice", a.hidBcdDevice},
        {"aim", {
            {"enabled", a.aim.enabled},
            {"memoryAim", a.aim.memoryAim},
            {"silentAim", a.aim.silentAim},
            {"drawFov", a.aim.drawFov},
            {"fov", a.aim.fov},
            {"fovMin", a.aim.fovMin},
            {"dynamicFov", a.aim.dynamicFov},
            {"smooth", a.aim.smooth},
            {"deadzone", a.aim.deadzone},
            {"maxDistance", a.aim.maxDistance},
            {"aimKey", a.aim.aimKey},
            {"aimKey2", a.aim.aimKey2},
            {"lockAimKey", a.aim.lockAimKey},
            {"targetLock", a.aim.targetLock},
            {"teamCheck", a.aim.teamCheck},
            {"targetHead", a.aim.targetHead},
            {"targetNeck", a.aim.targetNeck},
            {"targetChest", a.aim.targetChest},
            {"targetStomach", a.aim.targetStomach},
            {"targetPelvis", a.aim.targetPelvis},
            {"targetKnocked", a.aim.targetKnocked},
            {"visibleOnly", a.aim.visibleOnly},
            {"prediction", a.aim.prediction},
            {"smoothMode", a.aim.smoothMode},
            {"priorityMode", a.aim.priorityMode},
            {"triggerbot", a.aim.triggerbot},
            {"triggerKey", a.aim.triggerKey},
            {"triggerDelayMin", a.aim.triggerDelayMin},
            {"triggerDelayMax", a.aim.triggerDelayMax},
            {"triggerVisibleOnly", a.aim.triggerVisibleOnly},
            {"triggerTeamCheck", a.aim.triggerTeamCheck},
            {"noRecoil", a.aim.noRecoil},
            {"noSway", a.aim.noSway},
            {"noSpread", a.aim.noSpread},
            {"leadMs", a.aim.leadMs},
            {"leadPx", a.aim.leadPx}
        }}
    };

    std::ofstream ofs(path);
    if (!ofs.is_open()) {
        std::cerr << "[-] Failed to save config to: " << path << "\n";
        return false;
    }

    ofs << j.dump(4);
    std::cout << "[+] Config saved to: " << path << "\n";
    WriteKmboxSidecars();
    return true;
}

namespace {
    std::string ExeDirectory() {
        char path[MAX_PATH]{};
        if (!GetModuleFileNameA(nullptr, path, MAX_PATH))
            return ".";
        std::string dir(path);
        const auto slash = dir.find_last_of("\\/");
        if (slash != std::string::npos)
            dir.resize(slash);
        return dir;
    }

    bool WriteAcProfileLine(const std::filesystem::path& file,
                            const std::string& profile) {
        std::ofstream out(file);
        if (!out)
            return false;
        out << profile << "\n";
        return true;
    }
} // namespace

bool Config::WriteKmboxSidecars() {
    const auto& assist = s_config.assist;
    const std::string profile = assist.acProfile.empty() ? "robrun" : assist.acProfile;
    const std::filesystem::path exeDir = ExeDirectory();

    const auto resolved = KmboxIdentity::Resolve(
        assist.hidPreset, assist.hidVid, assist.hidPid, assist.hidManufacturer,
        assist.hidProduct, assist.hidSerial, assist.hidBcdDevice);

    bool ok = true;
    ok = KmboxIdentity::WriteIdentityTextFile(exeDir / "kmbox_identity.txt", resolved) && ok;
    ok = WriteAcProfileLine(exeDir / "kmbox_ac_profile.txt", profile) && ok;

    const std::filesystem::path kitFirmware =
        exeDir.parent_path() / "kmbox-second-pc" / "firmware";
    if (std::filesystem::is_directory(kitFirmware)) {
        ok = KmboxIdentity::WriteIdentityTextFile(kitFirmware / "kmbox_identity.txt", resolved) && ok;
        ok = WriteAcProfileLine(kitFirmware / "ac_profile.txt", profile) && ok;
        std::ofstream note(kitFirmware / "kmbox_ac_profile.txt");
        if (note)
            note << profile << "\n";
    }

    if (ok)
        std::cout << "[+] KMBox sidecars written (identity + AC profile)\n";
    else
        std::cerr << "[-] Failed to write one or more KMBox sidecar files\n";
    return ok;
}

bool Config::Load(const std::string& path) {
    if (!std::filesystem::exists(path)) {
        std::cout << "[*] No config file found, using defaults\n";
        ApplyRecommendedPipeline();
        Save(path);
        return true;
    }

    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        std::cerr << "[-] Failed to open config: " << path << "\n";
        return false;
    }

    json j;
    try {
        ifs >> j;
    } catch (const json::parse_error& e) {
        std::cerr << "[-] Config parse error: " << e.what() << "\n";
        return false;
    }

    if (j.contains("general")) {
        auto& jg = j["general"];
        auto& g = s_config.general;
        g.processName        = SafeGet<std::string>(jg, "processName", g.processName);
        g.overlayVsync       = SafeGet<bool>(jg, "overlayVsync", g.overlayVsync);
        g.streamProof        = SafeGet<bool>(jg, "streamProof", g.streamProof);
        g.fuserMode          = SafeGet<bool>(jg, "fuserMode", g.fuserMode);
        g.fullscreenOverlay  = SafeGet<bool>(jg, "fullscreenOverlay", g.fullscreenOverlay);
        g.overlayMonitor     = SafeGet<int>(jg, "overlayMonitor", g.overlayMonitor);
        g.gameScreenWidth    = SafeGet<int>(jg, "gameScreenWidth", g.gameScreenWidth);
        g.gameScreenHeight   = SafeGet<int>(jg, "gameScreenHeight", g.gameScreenHeight);
        g.serverDashboard    = SafeGet<bool>(jg, "serverDashboard", g.serverDashboard);
        g.widgets            = SafeGet<bool>(jg, "widgets", g.widgets);
        g.freeCam            = SafeGet<bool>(jg, "freeCam", g.freeCam);
        g.freeCamSpeed       = SafeGet<float>(jg, "freeCamSpeed", g.freeCamSpeed);
    }

    if (j.contains("esp")) {
        auto& je = j["esp"];
        auto& e = s_config.esp;
        e.enabled           = SafeGet<bool>(je, "enabled", e.enabled);
        e.showPlayers       = SafeGet<bool>(je, "showPlayers", e.showPlayers);
        e.showCops          = SafeGet<bool>(je, "showCops", e.showCops);
        e.showRobbers       = SafeGet<bool>(je, "showRobbers", e.showRobbers);
        e.showCivilians     = SafeGet<bool>(je, "showCivilians", e.showCivilians);
        e.hideTeam          = SafeGet<bool>(je, "hideTeam", e.hideTeam);
        e.hideDead          = SafeGet<bool>(je, "hideDead", e.hideDead);
        e.hideKnocked       = SafeGet<bool>(je, "hideKnocked", e.hideKnocked);
        e.showItems         = SafeGet<bool>(je, "showItems", e.showItems);
        e.showVehicles      = SafeGet<bool>(je, "showVehicles", e.showVehicles);
        e.showMobs          = SafeGet<bool>(je, "showMobs", e.showMobs);
        e.showGrenades      = SafeGet<bool>(je, "showGrenades", e.showGrenades);
        e.showLethalGrenades = SafeGet<bool>(je, "showLethalGrenades", e.showLethalGrenades);
        e.showTacticalGrenades = SafeGet<bool>(je, "showTacticalGrenades", e.showTacticalGrenades);
        e.showContainers    = SafeGet<bool>(je, "showContainers", e.showContainers);
        e.showEmptyContainers = SafeGet<bool>(je, "showEmptyContainers", e.showEmptyContainers);
        e.showContainerContents = SafeGet<bool>(je, "showContainerContents", e.showContainerContents);
        e.showCameras       = SafeGet<bool>(je, "showCameras", e.showCameras);
        e.showDeathBags     = SafeGet<bool>(je, "showDeathBags", e.showDeathBags);
        e.showCargo         = SafeGet<bool>(je, "showCargo", e.showCargo);
        e.showAtm           = SafeGet<bool>(je, "showAtm", e.showAtm);
        e.showSafes         = SafeGet<bool>(je, "showSafes", e.showSafes);
        e.showVaults        = SafeGet<bool>(je, "showVaults", e.showVaults);
        e.showHideSpots     = SafeGet<bool>(je, "showHideSpots", e.showHideSpots);
        e.showHiddenPlayers = SafeGet<bool>(je, "showHiddenPlayers", e.showHiddenPlayers);
        e.showBases         = SafeGet<bool>(je, "showBases", e.showBases);
        e.showGunIcons      = SafeGet<bool>(je, "showGunIcons", e.showGunIcons);
        e.showItemValues    = SafeGet<bool>(je, "showItemValues", e.showItemValues);
        e.drawChams         = SafeGet<bool>(je, "drawChams", e.drawChams);
        e.chamStyle         = SafeGet<int>(je, "chamStyle", e.chamStyle);
        e.materialChams     = SafeGet<bool>(je, "materialChams", e.materialChams);
        e.drawFragRadius    = SafeGet<bool>(je, "drawFragRadius", e.drawFragRadius);
        e.drawArmor         = SafeGet<bool>(je, "drawArmor", e.drawArmor);
        e.showGender        = SafeGet<bool>(je, "showGender", e.showGender);
        e.showOutfit        = SafeGet<bool>(je, "showOutfit", e.showOutfit);
        e.showHair          = SafeGet<bool>(je, "showHair", e.showHair);
        e.showBeard         = SafeGet<bool>(je, "showBeard", e.showBeard);
        e.showPoliceUniform = SafeGet<bool>(je, "showPoliceUniform", e.showPoliceUniform);
        e.visibleOnly       = SafeGet<bool>(je, "visibleOnly", e.visibleOnly);
        e.lootWeapons       = SafeGet<bool>(je, "lootWeapons", e.lootWeapons);
        e.lootMeds          = SafeGet<bool>(je, "lootMeds", e.lootMeds);
        e.lootAmmo          = SafeGet<bool>(je, "lootAmmo", e.lootAmmo);
        e.lootValuables     = SafeGet<bool>(je, "lootValuables", e.lootValuables);
        e.lootOther         = SafeGet<bool>(je, "lootOther", e.lootOther);
        e.drawBoxes         = SafeGet<bool>(je, "drawBoxes", e.drawBoxes);
        e.drawSkeleton      = SafeGet<bool>(je, "drawSkeleton", e.drawSkeleton);
        e.drawNames         = SafeGet<bool>(je, "drawNames", e.drawNames);
        e.drawDistance      = SafeGet<bool>(je, "drawDistance", e.drawDistance);
        e.drawHealth        = SafeGet<bool>(je, "drawHealth", e.drawHealth);
        e.drawSnaplines     = SafeGet<bool>(je, "drawSnaplines", e.drawSnaplines);
        e.drawAimLines      = SafeGet<bool>(je, "drawAimLines", e.drawAimLines);
        e.drawOffscreen     = SafeGet<bool>(je, "drawOffscreen", e.drawOffscreen);
        e.offscreenTeammates = SafeGet<bool>(je, "offscreenTeammates", e.offscreenTeammates);
        e.offscreenDead     = SafeGet<bool>(je, "offscreenDead", e.offscreenDead);
        e.offscreenCops     = SafeGet<bool>(je, "offscreenCops", e.offscreenCops);
        e.offscreenRobbers  = SafeGet<bool>(je, "offscreenRobbers", e.offscreenRobbers);
        e.offscreenScaleDistance = SafeGet<bool>(je, "offscreenScaleDistance", e.offscreenScaleDistance);
        e.boxStyle          = SafeGet<int>(je, "boxStyle", e.boxStyle);
        e.maxDistancePlayers  = SafeGet<float>(je, "maxDistancePlayers", e.maxDistancePlayers);
        e.maxDistanceItems    = SafeGet<float>(je, "maxDistanceItems", e.maxDistanceItems);
        e.maxDistanceVehicles = SafeGet<float>(je, "maxDistanceVehicles", e.maxDistanceVehicles);
        e.maxDistanceMobs     = SafeGet<float>(je, "maxDistanceMobs", e.maxDistanceMobs);
        e.maxDistanceGrenades = SafeGet<float>(je, "maxDistanceGrenades", e.maxDistanceGrenades);
        e.maxDistanceContainers = SafeGet<float>(je, "maxDistanceContainers", e.maxDistanceContainers);
        e.maxDistanceCameras  = SafeGet<float>(je, "maxDistanceCameras", e.maxDistanceCameras);
        e.maxDistanceBases    = SafeGet<float>(je, "maxDistanceBases", e.maxDistanceBases);
        e.minItemValue        = SafeGet<float>(je, "minItemValue", e.minItemValue);
        e.teamEsp             = SafeGet<bool>(je, "teamEsp", e.teamEsp);
        e.maxDistanceTeam     = SafeGet<float>(je, "maxDistanceTeam", e.maxDistanceTeam);
        e.baseMine            = SafeGet<bool>(je, "baseMine", e.baseMine);
        e.baseClan            = SafeGet<bool>(je, "baseClan", e.baseClan);
        e.baseOthers          = SafeGet<bool>(je, "baseOthers", e.baseOthers);
        e.baseRaidableOnly    = SafeGet<bool>(je, "baseRaidableOnly", e.baseRaidableOnly);
        e.baseHasLootOnly     = SafeGet<bool>(je, "baseHasLootOnly", e.baseHasLootOnly);
        e.baseHideRaidLocked  = SafeGet<bool>(je, "baseHideRaidLocked", e.baseHideRaidLocked);
        e.baseHideUnsettled   = SafeGet<bool>(je, "baseHideUnsettled", e.baseHideUnsettled);
        e.basePanel           = SafeGet<bool>(je, "basePanel", e.basePanel);
        e.raidAlerts          = SafeGet<bool>(je, "raidAlerts", e.raidAlerts);
        e.openDoorWarning     = SafeGet<bool>(je, "openDoorWarning", e.openDoorWarning);

        if (je.contains("playerColour"))  e.playerColour  = ColourFromJson(je["playerColour"], e.playerColour);
        if (je.contains("robberColour"))  e.robberColour  = ColourFromJson(je["robberColour"], e.robberColour);
        if (je.contains("copColour"))     e.copColour     = ColourFromJson(je["copColour"], e.copColour);
        if (je.contains("settlerColour")) e.settlerColour = ColourFromJson(je["settlerColour"], e.settlerColour);
        if (je.contains("prisonerColour")) e.prisonerColour = ColourFromJson(je["prisonerColour"], e.prisonerColour);
        if (je.contains("civilianColour")) e.civilianColour = ColourFromJson(je["civilianColour"], e.civilianColour);
        if (je.contains("itemColour"))    e.itemColour    = ColourFromJson(je["itemColour"], e.itemColour);
        if (je.contains("vehicleColour")) e.vehicleColour = ColourFromJson(je["vehicleColour"], e.vehicleColour);
        if (je.contains("mobColour"))     e.mobColour     = ColourFromJson(je["mobColour"], e.mobColour);
        if (je.contains("grenadeColour")) e.grenadeColour = ColourFromJson(je["grenadeColour"], e.grenadeColour);
        if (je.contains("lethalGrenadeColour")) e.lethalGrenadeColour = ColourFromJson(je["lethalGrenadeColour"], e.lethalGrenadeColour);
        if (je.contains("tacticalGrenadeColour")) e.tacticalGrenadeColour = ColourFromJson(je["tacticalGrenadeColour"], e.tacticalGrenadeColour);
        if (je.contains("containerColour")) e.containerColour = ColourFromJson(je["containerColour"], e.containerColour);
        if (je.contains("cameraColour"))  e.cameraColour  = ColourFromJson(je["cameraColour"], e.cameraColour);
        if (je.contains("deathBagColour")) e.deathBagColour = ColourFromJson(je["deathBagColour"], e.deathBagColour);
        if (je.contains("cargoColour"))   e.cargoColour   = ColourFromJson(je["cargoColour"], e.cargoColour);
        if (je.contains("atmColour"))     e.atmColour     = ColourFromJson(je["atmColour"], e.atmColour);
        if (je.contains("safeColour"))    e.safeColour    = ColourFromJson(je["safeColour"], e.safeColour);
        if (je.contains("vaultColour"))   e.vaultColour   = ColourFromJson(je["vaultColour"], e.vaultColour);
        if (je.contains("baseColour"))    e.baseColour    = ColourFromJson(je["baseColour"], e.baseColour);
        if (je.contains("myBaseColour"))  e.myBaseColour  = ColourFromJson(je["myBaseColour"], e.myBaseColour);
        if (je.contains("clanBaseColour")) e.clanBaseColour = ColourFromJson(je["clanBaseColour"], e.clanBaseColour);
        if (je.contains("teamColour"))    e.teamColour    = ColourFromJson(je["teamColour"], e.teamColour);
        if (je.contains("deadColour"))    e.deadColour    = ColourFromJson(je["deadColour"], e.deadColour);
    }

    if (j.contains("radar")) {
        auto& jr = j["radar"];
        auto& r = s_config.radar;
        r.enabled          = SafeGet<bool>(jr, "enabled", r.enabled);
        r.zoom             = SafeGet<float>(jr, "zoom", r.zoom);
        r.size             = SafeGet<float>(jr, "size", r.size);
        r.posX             = SafeGet<float>(jr, "posX", r.posX);
        r.posY             = SafeGet<float>(jr, "posY", r.posY);
        r.rotateWithCamera  = SafeGet<bool>(jr, "rotateWithCamera", r.rotateWithCamera);
        r.arrowBlips        = SafeGet<bool>(jr, "arrowBlips", r.arrowBlips);
        r.roleColours       = SafeGet<bool>(jr, "roleColours", r.roleColours);
        r.bigRadar          = SafeGet<bool>(jr, "bigRadar", r.bigRadar);
        r.follow            = SafeGet<bool>(jr, "follow", r.follow);
        r.showPlayers       = SafeGet<bool>(jr, "showPlayers", r.showPlayers);
        r.showItems         = SafeGet<bool>(jr, "showItems", r.showItems);
        r.showVehicles      = SafeGet<bool>(jr, "showVehicles", r.showVehicles);
        r.showMobs          = SafeGet<bool>(jr, "showMobs", r.showMobs);
        r.showGrenades      = SafeGet<bool>(jr, "showGrenades", r.showGrenades);
        r.showContainers    = SafeGet<bool>(jr, "showContainers", r.showContainers);
        r.showBases         = SafeGet<bool>(jr, "showBases", r.showBases);
        r.showCameras       = SafeGet<bool>(jr, "showCameras", r.showCameras);

        if (jr.contains("backgroundColour")) r.backgroundColour = ColourFromJson(jr["backgroundColour"], r.backgroundColour);
        if (jr.contains("playerColour"))     r.playerColour     = ColourFromJson(jr["playerColour"], r.playerColour);
        if (jr.contains("itemColour"))       r.itemColour       = ColourFromJson(jr["itemColour"], r.itemColour);
        if (jr.contains("vehicleColour"))    r.vehicleColour    = ColourFromJson(jr["vehicleColour"], r.vehicleColour);
        if (jr.contains("mobColour"))        r.mobColour        = ColourFromJson(jr["mobColour"], r.mobColour);
        if (jr.contains("grenadeColour"))    r.grenadeColour    = ColourFromJson(jr["grenadeColour"], r.grenadeColour);
        if (jr.contains("containerColour"))  r.containerColour  = ColourFromJson(jr["containerColour"], r.containerColour);
        if (jr.contains("localColour"))      r.localColour      = ColourFromJson(jr["localColour"], r.localColour);
    }

    if (j.contains("assist") && j["assist"].is_object()) {
        const auto& ja = j["assist"];
        auto& assist = s_config.assist;
        assist.comPort = SafeGet<int>(ja, "comPort", assist.comPort);
        assist.autoConnect = SafeGet<bool>(ja, "autoConnect", assist.autoConnect);
        assist.acProfile = SafeGet<std::string>(ja, "acProfile", assist.acProfile);
        assist.hidPreset = SafeGet<std::string>(ja, "hidPreset", assist.hidPreset);
        assist.hidVid = SafeGet<std::string>(ja, "hidVid", assist.hidVid);
        assist.hidPid = SafeGet<std::string>(ja, "hidPid", assist.hidPid);
        assist.hidManufacturer = SafeGet<std::string>(ja, "hidManufacturer", assist.hidManufacturer);
        assist.hidProduct = SafeGet<std::string>(ja, "hidProduct", assist.hidProduct);
        assist.hidSerial = SafeGet<std::string>(ja, "hidSerial", assist.hidSerial);
        assist.hidBcdDevice = SafeGet<std::string>(ja, "hidBcdDevice", assist.hidBcdDevice);
        if (ja.contains("aim") && ja["aim"].is_object()) {
            const auto& jaim = ja["aim"];
            auto& aim = assist.aim;
            aim.enabled = SafeGet<bool>(jaim, "enabled", aim.enabled);
            aim.memoryAim = SafeGet<bool>(jaim, "memoryAim", aim.memoryAim);
            aim.silentAim = SafeGet<bool>(jaim, "silentAim", aim.silentAim);
            aim.drawFov = SafeGet<bool>(jaim, "drawFov", aim.drawFov);
            aim.fov = SafeGet<float>(jaim, "fov", aim.fov);
            aim.fovMin = SafeGet<float>(jaim, "fovMin", aim.fovMin);
            aim.dynamicFov = SafeGet<bool>(jaim, "dynamicFov", aim.dynamicFov);
            aim.smooth = SafeGet<float>(jaim, "smooth", aim.smooth);
            aim.deadzone = SafeGet<float>(jaim, "deadzone", aim.deadzone);
            aim.maxDistance = SafeGet<float>(jaim, "maxDistance", aim.maxDistance);
            aim.aimKey = SafeGet<int>(jaim, "aimKey", aim.aimKey);
            aim.aimKey2 = SafeGet<int>(jaim, "aimKey2", aim.aimKey2);
            aim.lockAimKey = SafeGet<bool>(jaim, "lockAimKey", aim.lockAimKey);
            aim.targetLock = SafeGet<bool>(jaim, "targetLock", aim.targetLock);
            aim.teamCheck = SafeGet<bool>(jaim, "teamCheck", aim.teamCheck);
            aim.targetHead = SafeGet<bool>(jaim, "targetHead", aim.targetHead);
            aim.targetNeck = SafeGet<bool>(jaim, "targetNeck", aim.targetNeck);
            aim.targetChest = SafeGet<bool>(jaim, "targetChest", aim.targetChest);
            aim.targetStomach = SafeGet<bool>(jaim, "targetStomach", aim.targetStomach);
            aim.targetPelvis = SafeGet<bool>(jaim, "targetPelvis", aim.targetPelvis);
            aim.targetKnocked = SafeGet<bool>(jaim, "targetKnocked", aim.targetKnocked);
            aim.visibleOnly = SafeGet<bool>(jaim, "visibleOnly", aim.visibleOnly);
            aim.prediction = SafeGet<bool>(jaim, "prediction", aim.prediction);
            aim.smoothMode = SafeGet<int>(jaim, "smoothMode", aim.smoothMode);
            aim.priorityMode = SafeGet<int>(jaim, "priorityMode", aim.priorityMode);
            aim.triggerbot = SafeGet<bool>(jaim, "triggerbot", aim.triggerbot);
            aim.triggerKey = SafeGet<int>(jaim, "triggerKey", aim.triggerKey);
            aim.triggerDelayMin = SafeGet<float>(jaim, "triggerDelayMin", aim.triggerDelayMin);
            aim.triggerDelayMax = SafeGet<float>(jaim, "triggerDelayMax", aim.triggerDelayMax);
            aim.triggerVisibleOnly = SafeGet<bool>(jaim, "triggerVisibleOnly", aim.triggerVisibleOnly);
            aim.triggerTeamCheck = SafeGet<bool>(jaim, "triggerTeamCheck", aim.triggerTeamCheck);
            aim.noRecoil = SafeGet<bool>(jaim, "noRecoil", aim.noRecoil);
            aim.noSway = SafeGet<bool>(jaim, "noSway", aim.noSway);
            aim.noSpread = SafeGet<bool>(jaim, "noSpread", aim.noSpread);
            aim.leadMs = SafeGet<float>(jaim, "leadMs", aim.leadMs);
            aim.leadPx = SafeGet<float>(jaim, "leadPx", aim.leadPx);
        }
    }

    ApplyRecommendedPipeline();
    std::cout << "[+] Config loaded from: " << path << "\n";
    return true;
}
