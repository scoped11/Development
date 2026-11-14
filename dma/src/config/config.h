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
    bool showItems = true;
    bool showVehicles = true;
    bool showMobs = false;
    bool showGrenades = true;
    bool showLethalGrenades = true;
    bool showTacticalGrenades = true;
    bool showContainers = true;
    bool showEmptyContainers = false;
    bool showContainerContents = true;
    bool showCameras = true;
    bool showDeathBags = true;
    bool showCargo = true;
    bool showAtm = true;
    bool showSafes = true;
    bool showVaults = true;
    bool showHideSpots = true;
    bool showHiddenPlayers = true;
    bool showBases = true;
    bool showGunIcons = true;
    bool showItemValues = true;
    bool drawChams = true;
    int chamStyle = 1; // 0 flat, 1 shaded, 2 wireframe, 3 outline
    bool materialChams = true;
    bool drawFragRadius = true;
    bool drawArmor = true;
    bool showGender = true;
    bool showOutfit = true;
    bool showHair = true;
    bool showBeard = true;
    bool showPoliceUniform = true;
    bool visibleOnly = false;
    bool lootWeapons = true;
    bool lootMeds = true;
    bool lootAmmo = true;
    bool lootValuables = true;
    bool lootOther = true;

    bool drawBoxes = true;
    bool drawSkeleton = true;
    bool drawNames = true;
    bool drawDistance = true;
    bool drawHealth = true;
    bool drawSnaplines = false;
    bool drawAimLines = false;
    bool drawOffscreen = true;
    bool offscreenTeammates = true;
    bool offscreenDead = false;
    bool offscreenCops = true;
    bool offscreenRobbers = true;
    bool offscreenScaleDistance = true;

    int boxStyle = 0;
    float maxDistancePlayers = 500.f;
    float maxDistanceItems = 200.f;
    float maxDistanceVehicles = 500.f;
    float maxDistanceMobs = 300.f;
    float maxDistanceGrenades = 200.f;
    float maxDistanceContainers = 120.f;
    float maxDistanceCameras = 250.f;
    float maxDistanceBases = 800.f;
    float minItemValue = 0.f;

    bool teamEsp = true;
    Colour teamColour = Colour::Green();
    float maxDistanceTeam = 800.f;

    bool baseMine = true;
    bool baseClan = true;
    bool baseOthers = true;
    bool baseRaidableOnly = false;
    bool baseHasLootOnly = false;
    bool baseHideRaidLocked = false;
    bool baseHideUnsettled = false;
    bool basePanel = true;
    bool raidAlerts = true;
    bool openDoorWarning = true;

    Colour playerColour    = Colour::White();
    Colour robberColour    = Colour::Red();
    Colour copColour       = Colour::Blue();
    Colour settlerColour   = Colour(0.35f, 0.82f, 0.55f, 1.f);
    Colour prisonerColour  = Colour(0.85f, 0.55f, 0.20f, 1.f);
    Colour civilianColour  = Colour(0.82f, 0.82f, 0.78f, 1.f);
    Colour itemColour      = Colour::Yellow();
    Colour vehicleColour   = Colour::Cyan();
    Colour mobColour       = Colour::Orange();
    Colour grenadeColour   = Colour::Pink();
    Colour lethalGrenadeColour = Colour(1.f, 0.25f, 0.18f, 1.f);
    Colour tacticalGrenadeColour = Colour(0.35f, 0.78f, 1.f, 1.f);
    Colour containerColour = Colour(0.55f, 0.78f, 0.95f, 1.f);
    Colour cameraColour    = Colour(0.95f, 0.85f, 0.25f, 1.f);
    Colour deathBagColour  = Colour(0.95f, 0.45f, 0.15f, 1.f);
    Colour cargoColour     = Colour(0.95f, 0.70f, 0.25f, 1.f);
    Colour atmColour       = Colour(0.35f, 0.90f, 0.45f, 1.f);
    Colour safeColour      = Colour(0.90f, 0.75f, 0.20f, 1.f);
    Colour vaultColour     = Colour(0.95f, 0.55f, 0.15f, 1.f);
    Colour baseColour      = Colour::Pink();
    Colour myBaseColour    = Colour::Green();
    Colour clanBaseColour  = Colour::Cyan();
    Colour deadColour      = Colour(0.4f, 0.4f, 0.4f, 0.5f);
};

struct RadarConfig {
    bool enabled = false;
    float zoom = 1.f;
    float size = 300.f;
    float posX = -1.f;
    float posY = -1.f;
    bool rotateWithCamera = true;
    bool arrowBlips = true;
    bool roleColours = true;
    bool bigRadar = false;
    bool follow = true;

    bool showPlayers = true;
    bool showItems = true;
    bool showVehicles = true;
    bool showMobs = true;
    bool showGrenades = true;
    bool showContainers = true;
    bool showBases = true;
    bool showCameras = true;

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
    int updateRateActors    = 40;
    int updateRatePositions = 16;
    int skeletonRefreshMs   = 50;
    int scatterMaxPrepares  = 128;
    int maxBonePlayers      = 8;
    bool overlayVsync       = true;
    bool streamProof        = false;
    // false = transparent DWM overlay (click-through until INSERT).
    // true  = solid background for a 2nd-PC HDMI fuser.
    bool fuserMode          = false;
    bool fullscreenOverlay  = true;
    int overlayMonitor      = 0;
    int gameScreenWidth     = 0;
    int gameScreenHeight    = 0;
    bool serverDashboard    = true;
    bool widgets            = true;
    bool freeCam            = false;
    float freeCamSpeed      = 12.f;
};

struct AimConfig {
    bool enabled = false;
    bool memoryAim = true;
    bool silentAim = false;
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
    bool targetLock = true;
    bool teamCheck = true;
    bool targetHead = true;
    bool targetNeck = false;
    bool targetChest = true;
    bool targetStomach = false;
    bool targetPelvis = false;
    bool targetKnocked = false;
    bool visibleOnly = false;
    bool prediction = true;
    int smoothMode = 0; // 0 linear, 1 EMA, 2 bezier
    int priorityMode = 0; // 0 FOV, 1 distance, 2 health
    bool triggerbot = false;
    int triggerKey = 0;
    float triggerDelayMin = 20.f;
    float triggerDelayMax = 45.f;
    bool triggerVisibleOnly = true;
    bool triggerTeamCheck = true;
    bool noRecoil = false;
    bool noSway = false;
    bool noSpread = false;
    float leadMs = 45.f;
    float leadPx = 4.f;
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
