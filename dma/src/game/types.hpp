#pragma once
#include <chrono>
#include <cstdint>
#include <string>
#include <vector>
#include "math/vector.h"

struct CameraData {
    FMinimalViewInfo viewInfo;
    float viewMatrix[16] = {};
    bool hasViewMatrix = false;
    bool valid = false;
    std::string status;
    int gameWidth = 0;
    int gameHeight = 0;
    uint64_t generation = 0;
    std::chrono::steady_clock::time_point captured{};

    float AgeMs() const {
        if (captured.time_since_epoch().count() == 0)
            return 0.f;
        return std::chrono::duration<float, std::milli>(
            std::chrono::steady_clock::now() - captured).count();
    }
};

enum class EntityType {
    Unknown,
    Player,
    Item,
    Vehicle,
    Mob,
    Grenade,
    Container,
    Camera,
    Base,
    Atm,
    Safe,
    Vault,
    Cargo,
    HideSpot,
    DeathBag,
    Rack
};

struct GameEntity {
    uintptr_t address = 0;
    int creationIndex = -1;
    bool isEnabled = false;
    bool isLocalPlayer = false;
    EntityType type = EntityType::Unknown;

    Vector3 position;
    float healthNormalized = 1.f;
    bool isAlive = true;
    bool isKnocked = false;

    std::string displayName;
    std::string role;
    std::string clan;
    std::string extra;
    Vector3 aimForward;
    bool inVehicle = false;
    bool isCrouching = false;
    bool isSprinting = false;
    bool hasArmor = false;
    float armorAmount = 0.f;
    float maxArmor = 3.f;
    float walkPhase = 0.f;

    int boneCount = 0;
    int boneStride = 0;
    int boneLocal = 0;
    int boneModel = 0;
    int boneLength = 0;
    uintptr_t boneData = 0;
    int boneBytes = 0;
    std::vector<int> boneParents;
    std::vector<float> boneFloats;
    int boneIdxRoot = -1;
    int boneIdxLegs = -1;
    int boneIdxSpine2 = -1;
    int boneIdxSpine3 = -1;
    int boneIdxNeck = -1;
    int boneIdxHead1 = -1;
    int boneIdxHead2 = -1;
    bool skeletonAimReady = false;
    bool skeletonPoseCached = false;
    Vector3 skeletonPelvis{};
    Vector3 skeletonChest{};
    Vector3 skeletonNeck{};
    Vector3 skeletonHead{};
    std::vector<Vector3> skeletonPositions;
    std::vector<std::pair<int, int>> skeletonSegments;

    float health = 100.f;
    float maxHealth = 100.f;
    float markerHeight = 1.8f;
    float markerAspect = 0.5f;
    bool hasHull = false;
    Vector3 hullSize;
    Vector3 hullCenter;
    float hullQuat[4] = { 0.f, 0.f, 0.f, 1.f };
    int cash = -1;
    int lootTier = -1;
    int itemType = -1;
    int grenadeKind = 0;
    float blastRadius = 0.f;
    bool isWeapon = false;
    bool isHidden = false;
    bool hidingInProp = false;
    bool cameraBroken = false;
    bool vip = false;
    bool raidLocked = false;
    bool beingRaided = false;
    bool openDoors = false;
    bool isTeammate = false;
    bool hasLoot = false;
    int decoderCount = 0;
    int defensePoints = 0;
    int maxDefensePoints = 0;
    int64_t drillEndTs = 0;
    int64_t raidLockEndTs = 0;
    int64_t ownerPublicId = 0;
    bool isClanBase = false;
    std::string contents;
    std::string gender;
    std::string outfit;
    std::string hair;
    std::string beard;
    bool policeUniform = false;
    Vector3 velocity{};
    uintptr_t colorModel = 0;
    uintptr_t followCamNative = 0;
    uintptr_t armorAmountAddr = 0;
    uint32_t generation = 0;

    uintptr_t posX = 0;
    uintptr_t posY = 0;
    uintptr_t posZ = 0;
    uintptr_t posVec = 0;
    uintptr_t repPosAddr = 0;
    uintptr_t healthAddr = 0;
    uintptr_t maxHealthAddr = 0;
    uintptr_t deadAddr = 0;
    uintptr_t lookAddr = 0;
    uintptr_t speedAddr = 0;
    uintptr_t crouchAddr = 0;
    uintptr_t sprintAddr = 0;
    uintptr_t armorAddr = 0;
    uintptr_t vehicleIdAddr = 0;
    uintptr_t cashAddr = 0;
    uintptr_t leaveAddr = 0;
    uintptr_t hiddenAddr = 0;
    uintptr_t livePos = 0;
    uintptr_t liveRot = 0;
    uintptr_t renderedPos = 0;
    uintptr_t renderedHeadPos = 0;
    Vector3 viewHead{};
    int rideId = -1;

    bool IsValid() const { return address != 0 && isEnabled; }
    float HealthPercent() const { return healthNormalized * 100.f; }
    float DistanceTo(const Vector3& other) const {
        float dx = position.x - other.x;
        float dy = position.y - other.y;
        float dz = position.z - other.z;
        return std::sqrtf(dx * dx + dy * dy + dz * dz);
    }
};