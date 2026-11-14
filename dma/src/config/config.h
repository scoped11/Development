#pragma once
#include <string>
#include "math/vector.h"

struct Colour {
    float r = 1.f, g = 1.f, b = 1.f, a = 1.f;

    Colour() = default;
    Colour(float r, float g, float b, float a = 1.f) : r(r), g(g), b(b), a(a) {}

    static Colour Red()    { return { 1.f, 0.f, 0.f, 1.f }; }
    static Colour Green()  { return { 0.f, 1.f, 0.f, 1.f }; }
    static Colour Blue()   { return { 0.f, 0.4f, 1.f, 1.f }; }
    static Colour Yellow() { return { 1.f, 1.f, 0.f, 1.f }; }
    static Colour White()  { return { 1.f, 1.f, 1.f, 1.f }; }
    static Colour Cyan()   { return { 0.f, 1.f, 1.f, 1.f }; }
    static Colour Orange() { return { 1.f, 0.5f, 0.f, 1.f }; }
    static Colour Purple() { return { 0.7f, 0.f, 1.f, 1.f }; }
    static Colour Grey()   { return { 0.5f, 0.5f, 0.5f, 1.f }; }
    static Colour Pink()   { return { 1.f, 0.4f, 0.7f, 1.f }; }
};

struct ESPConfig {
    bool enabled = true;

    bool showPlayers = true;
    bool showCops = true;
    bool showRobbers = true;
    bool showCivilians = true;
    bool hideTeam = false;
    bool hideDead = false;
    bool hideKnocked = false;
    bool showItems = false;
    bool showVehicles = false;
    bool showMobs = false;
    bool showGrenades = false;
    bool showContainers = false;
    bool showEmptyContainers = false;

    bool drawBoxes = true;
    bool drawSkeleton = true;
    bool drawNames = true;
    bool drawDistance = true;
    bool drawHealth = true;
    bool drawSnaplines = false;
    bool drawAimLines = false;
    bool drawOffscreen = true;

    int boxStyle = 0;
    float maxDistancePlayers = 500.f;
    float maxDistanceItems = 200.f;
    float maxDistanceVehicles = 500.f;
    float maxDistanceMobs = 300.f;
    float maxDistanceGrenades = 200.f;
    float maxDistanceContainers = 80.f;

    Colour playerColour    = Colour::White();
    Colour robberColour    = Colour::Red();
    Colour copColour       = Colour::Blue();
    Colour itemColour      = Colour::Yellow();
    Colour vehicleColour   = Colour::Cyan();
    Colour mobColour       = Colour::Orange();
    Colour grenadeColour   = Colour::Pink();
    Colour containerColour = Colour::Purple();
    Colour deadColour      = Colour(0.4f, 0.4f, 0.4f, 0.5f);
};

struct RadarConfig {
    bool enabled = false;
    float zoom = 1.f;
    float size = 300.f;
    bool rotateWithCamera = true;

    bool showPlayers = true;
    bool showItems = true;
    bool showVehicles = true;
    bool showMobs = true;
    bool showGrenades = true;
    bool showContainers = true;

    Colour backgroundColour = Colour(0.05f, 0.05f, 0.1f, 0.85f);
    Colour playerColour     = Colour::Red();
    Colour itemColour       = Colour::Yellow();
    Colour vehicleColour    = Colour::Cyan();
    Colour mobColour        = Colour::Orange();
    Colour grenadeColour    = Colour::Pink();
    Colour containerColour  = Colour::Purple();
    Colour localColour      = Colour::White();
};

struct GeneralConfig {
    std::string processName = "ROB RUN.exe";
    // Filled by ApplyRecommendedPipeline / AutoTuneWorkersForEsp. Not user knobs.
    int updateRateGlobals   = 200;
    int updateRateActors    = 8;
    int updateRatePositions = 1;
    int skeletonRefreshMs   = 1;
    int scatterMaxPrepares  = 768;
    int maxBonePlayers      = 96;
    bool overlayVsync       = true;
    bool streamProof        = false;
    // false = transparent DWM overlay (click-through until INSERT).
    // true  = solid background for a 2nd-PC HDMI fuser.
    bool fuserMode          = false;
    bool fullscreenOverlay  = true;
    int gameScreenWidth     = 0;
    int gameScreenHeight    = 0;
};

struct AimConfig {
    bool enabled = false;
    bool memoryAim = true;
    bool drawFov = true;
    float fov = 120.f;
    float fovMin = 40.f;
    bool dynamicFov = true;
    float smooth = 1.f;
    float deadzone = 2.f;
    float maxDistance = 400.f;
    int aimKey = 2;
    int aimKey2 = 0;
    bool lockAimKey = false;
    bool teamCheck = true;
    bool targetHead = true;
    bool targetNeck = false;
    bool targetChest = true;
    bool targetPelvis = false;
    bool targetKnocked = false;
    bool noRecoil = false;
    bool noSway = false;
    bool noSpread = false;
};

struct AssistConfig {
    int comPort = 0;
    bool autoConnect = false;
    // Single line for kmbox_ac_profile.txt / onboard ac_profile.txt (robrun, eac, be, ace).
    std::string acProfile = "robrun";
    // Default: Logitech LIGHTSPEED dongle IDs from this machine (046D:C547).
    std::string hidPreset = "logitech_lightspeed_c547";
    std::string hidVid = "046d";
    std::string hidPid = "c547";
    std::string hidManufacturer = "Logitech";
    std::string hidProduct = "USB Receiver";
    std::string hidSerial;
    std::string hidBcdDevice = "0110";
    AimConfig aim;
};

struct AppConfig {
    GeneralConfig general;
    ESPConfig esp;
    RadarConfig radar;
    AssistConfig assist;
};

namespace Config {
    AppConfig& Get();
    bool Save(const std::string& path = "config.json");
    bool Load(const std::string& path = "config.json");
    // Writes kmbox_identity.txt and kmbox_ac_profile.txt beside RobRunDMA.exe
    // (and kmbox-second-pc/firmware when that folder exists).
    bool WriteKmboxSidecars();
    void ApplyRecommendedPipeline();
    void AutoTuneWorkersForEsp(int enabledPlayers, bool inMatch);
}
