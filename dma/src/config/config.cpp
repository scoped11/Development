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
    g.updateRatePositions = 1;
    g.skeletonRefreshMs = 8;
    g.overlayVsync = false;
    if (inMatch) {
        g.updateRateGlobals = 300;
        g.updateRateActors = 8;
        g.maxBonePlayers = (std::clamp)(enabledPlayers, 8, 16);
        g.scatterMaxPrepares = 768;
    } else {
        g.updateRateGlobals = 16;
        g.updateRateActors = 8;
        g.maxBonePlayers = 8;
        g.scatterMaxPrepares = 512;
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
        {"gameScreenWidth", g.gameScreenWidth},
        {"gameScreenHeight", g.gameScreenHeight}
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
        {"showContainers", e.showContainers},
        {"showEmptyContainers", e.showEmptyContainers},
        {"drawBoxes", e.drawBoxes},
        {"drawSkeleton", e.drawSkeleton},
        {"drawNames", e.drawNames},
        {"drawDistance", e.drawDistance},
        {"drawHealth", e.drawHealth},
        {"drawSnaplines", e.drawSnaplines},
        {"drawAimLines", e.drawAimLines},
        {"drawOffscreen", e.drawOffscreen},
        {"boxStyle", e.boxStyle},
        {"maxDistancePlayers", e.maxDistancePlayers},
        {"maxDistanceItems", e.maxDistanceItems},
        {"maxDistanceVehicles", e.maxDistanceVehicles},
        {"maxDistanceMobs", e.maxDistanceMobs},
        {"maxDistanceGrenades", e.maxDistanceGrenades},
        {"maxDistanceContainers", e.maxDistanceContainers},
        {"playerColour", ColourToJson(e.playerColour)},
        {"robberColour", ColourToJson(e.robberColour)},
        {"copColour", ColourToJson(e.copColour)},
        {"itemColour", ColourToJson(e.itemColour)},
        {"vehicleColour", ColourToJson(e.vehicleColour)},
        {"mobColour", ColourToJson(e.mobColour)},
        {"grenadeColour", ColourToJson(e.grenadeColour)},
        {"containerColour", ColourToJson(e.containerColour)},
        {"deadColour", ColourToJson(e.deadColour)}
    };

    auto& r = s_config.radar;
    j["radar"] = {
        {"enabled", r.enabled},
        {"zoom", r.zoom},
        {"size", r.size},
        {"rotateWithCamera", r.rotateWithCamera},
        {"showPlayers", r.showPlayers},
        {"showItems", r.showItems},
        {"showVehicles", r.showVehicles},
        {"showMobs", r.showMobs},
        {"showGrenades", r.showGrenades},
        {"showContainers", r.showContainers},
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
            {"teamCheck", a.aim.teamCheck},
            {"targetHead", a.aim.targetHead},
            {"targetNeck", a.aim.targetNeck},
            {"targetChest", a.aim.targetChest},
            {"targetPelvis", a.aim.targetPelvis},
            {"targetKnocked", a.aim.targetKnocked},
            {"noRecoil", a.aim.noRecoil},
            {"noSway", a.aim.noSway},
            {"noSpread", a.aim.noSpread}
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
        g.gameScreenWidth    = SafeGet<int>(jg, "gameScreenWidth", g.gameScreenWidth);
        g.gameScreenHeight   = SafeGet<int>(jg, "gameScreenHeight", g.gameScreenHeight);
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
        e.showContainers    = SafeGet<bool>(je, "showContainers", e.showContainers);
        e.showEmptyContainers = SafeGet<bool>(je, "showEmptyContainers", e.showEmptyContainers);
        e.drawBoxes         = SafeGet<bool>(je, "drawBoxes", e.drawBoxes);
        e.drawSkeleton      = SafeGet<bool>(je, "drawSkeleton", e.drawSkeleton);
        e.drawNames         = SafeGet<bool>(je, "drawNames", e.drawNames);
        e.drawDistance      = SafeGet<bool>(je, "drawDistance", e.drawDistance);
        e.drawHealth        = SafeGet<bool>(je, "drawHealth", e.drawHealth);
        e.drawSnaplines     = SafeGet<bool>(je, "drawSnaplines", e.drawSnaplines);
        e.drawAimLines      = SafeGet<bool>(je, "drawAimLines", e.drawAimLines);
        e.drawOffscreen     = SafeGet<bool>(je, "drawOffscreen", e.drawOffscreen);
        e.boxStyle          = SafeGet<int>(je, "boxStyle", e.boxStyle);
        e.maxDistancePlayers  = SafeGet<float>(je, "maxDistancePlayers", e.maxDistancePlayers);
        e.maxDistanceItems    = SafeGet<float>(je, "maxDistanceItems", e.maxDistanceItems);
        e.maxDistanceVehicles = SafeGet<float>(je, "maxDistanceVehicles", e.maxDistanceVehicles);
        e.maxDistanceMobs     = SafeGet<float>(je, "maxDistanceMobs", e.maxDistanceMobs);
        e.maxDistanceGrenades = SafeGet<float>(je, "maxDistanceGrenades", e.maxDistanceGrenades);
        e.maxDistanceContainers = SafeGet<float>(je, "maxDistanceContainers", e.maxDistanceContainers);

        if (je.contains("playerColour"))  e.playerColour  = ColourFromJson(je["playerColour"], e.playerColour);
        if (je.contains("robberColour"))  e.robberColour  = ColourFromJson(je["robberColour"], e.robberColour);
        if (je.contains("copColour"))     e.copColour     = ColourFromJson(je["copColour"], e.copColour);
        if (je.contains("itemColour"))    e.itemColour    = ColourFromJson(je["itemColour"], e.itemColour);
        if (je.contains("vehicleColour")) e.vehicleColour = ColourFromJson(je["vehicleColour"], e.vehicleColour);
        if (je.contains("mobColour"))     e.mobColour     = ColourFromJson(je["mobColour"], e.mobColour);
        if (je.contains("grenadeColour")) e.grenadeColour = ColourFromJson(je["grenadeColour"], e.grenadeColour);
        if (je.contains("containerColour")) e.containerColour = ColourFromJson(je["containerColour"], e.containerColour);
        if (je.contains("deadColour"))    e.deadColour    = ColourFromJson(je["deadColour"], e.deadColour);
    }

    if (j.contains("radar")) {
        auto& jr = j["radar"];
        auto& r = s_config.radar;
        r.enabled          = SafeGet<bool>(jr, "enabled", r.enabled);
        r.zoom             = SafeGet<float>(jr, "zoom", r.zoom);
        r.size             = SafeGet<float>(jr, "size", r.size);
        r.rotateWithCamera  = SafeGet<bool>(jr, "rotateWithCamera", r.rotateWithCamera);
        r.showPlayers       = SafeGet<bool>(jr, "showPlayers", r.showPlayers);
        r.showItems         = SafeGet<bool>(jr, "showItems", r.showItems);
        r.showVehicles      = SafeGet<bool>(jr, "showVehicles", r.showVehicles);
        r.showMobs          = SafeGet<bool>(jr, "showMobs", r.showMobs);
        r.showGrenades      = SafeGet<bool>(jr, "showGrenades", r.showGrenades);
        r.showContainers    = SafeGet<bool>(jr, "showContainers", r.showContainers);

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
            aim.teamCheck = SafeGet<bool>(jaim, "teamCheck", aim.teamCheck);
            aim.targetHead = SafeGet<bool>(jaim, "targetHead", aim.targetHead);
            aim.targetNeck = SafeGet<bool>(jaim, "targetNeck", aim.targetNeck);
            aim.targetChest = SafeGet<bool>(jaim, "targetChest", aim.targetChest);
            aim.targetPelvis = SafeGet<bool>(jaim, "targetPelvis", aim.targetPelvis);
            aim.targetKnocked = SafeGet<bool>(jaim, "targetKnocked", aim.targetKnocked);
            aim.noRecoil = SafeGet<bool>(jaim, "noRecoil", aim.noRecoil);
            aim.noSway = SafeGet<bool>(jaim, "noSway", aim.noSway);
            aim.noSpread = SafeGet<bool>(jaim, "noSpread", aim.noSpread);
        }
    }

    ApplyRecommendedPipeline();
    std::cout << "[+] Config loaded from: " << path << "\n";
    return true;
}
