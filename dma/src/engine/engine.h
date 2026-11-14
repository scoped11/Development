#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <atomic>
#include <cstdint>
#include <chrono>
#include "math/vector.h"
#include "config/config.h"

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
    Container
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
    float walkPhase = 0.f;

    // SkeletonModel float[] + cached parent table (children array or spine chain).
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
    // World height of the box, and its screen width divided by its screen height.
    float markerHeight = 1.8f;
    float markerAspect = 0.5f;
    // Vehicle hull. Size is the full box. Center is its offset from the live pivot.
    bool hasHull = false;
    Vector3 hullSize;
    Vector3 hullCenter;
    float hullQuat[4] = { 0.f, 0.f, 0.f, 1.f };
    int cash = -1;
    // RarityItem: 0 none, 1 common, 2 uncommon, 3 rare, 4 epic, 5 legendary, 6 exotic.
    int lootTier = -1;
    uint32_t generation = 0;

    // Resolved value addresses, filled by the actor pass and read by the
    // position pass. posX/posY/posZ are individual floats. When posVec is
    // set, the position is one Vector3 at that address instead.
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
    // Physics pose. The replicated primitive stays where you entered the
    // vehicle. These two are the pose the game draws.
    uintptr_t livePos = 0;
    uintptr_t liveRot = 0;
    // Native Transform world position of the rendered view. Zero when unresolved.
    uintptr_t renderedPos = 0;
    uintptr_t renderedHeadPos = 0;
    // CharacterEntityView head world position from the last scatter, when valid.
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

class UnityEngine {
public:
    bool Initialize(const std::string& processName);
    void Shutdown();

    void CacheGlobals();
    void CacheActors();
    void CachePositions();
    void ApplyGun();
    void SetMemoryAimTarget(bool active, const Vector3& eye, const Vector3& targetWorld, float smooth);
    void ServiceMemoryAim();

    bool IsInGame() const { return m_inGame; }
    bool ReadyForOverlay() const {
        return m_moduleReady.load();
    }
    int DetectedWidth() const { return m_gameWidth.load(); }
    int DetectedHeight() const { return m_gameHeight.load(); }

    std::vector<GameEntity> GetEntities();
    GameEntity GetLocalPlayer();
    CameraData GetCamera();
    // Entities and the lens from the same position tick. Drawing with a
    // newer camera than the positions makes every box slide when you move.
    void CopyRenderState(std::vector<GameEntity>& entities, GameEntity& local, CameraData& camera);
    void RefreshCamera(CameraData& camera);

private:
    struct BattleCandidate {
        uintptr_t address = 0;
        int characters = -1;
        int waiting = -1;
        int models = -1;
        int slots = -1;
        int score = -1;
        bool hasPlayer = false;
        bool hasView = false;
        bool hasServerPosition = false;
    };

    bool EnsureModule();
    bool FindBattleEntities();
    void FindCameraBrain();
    void FindCameraRig();
    void FindViewUpdater();
    bool MatchesKlassName(uintptr_t klass, const char* name) const;
    bool IsBattleKlass(uintptr_t klass) const;
    bool IsNamedKlass(uintptr_t klass, const char* name, const char* nameSpace) const;
    uintptr_t KlassFromTypeInfoWord(uintptr_t word, const char* name,
                                    const char* nameSpace) const;
    uintptr_t FindKlassByName(const char* name) const;
    uintptr_t ScanTypeInfoRegionForKlass(const char* name, const char* nameSpace);
    uintptr_t ResolveNamedKlass(const char* name, const char* nameSpace,
                                const uintptr_t* slots, size_t slotCount);
    uintptr_t ResolveBattleKlass();
    void SyncLinkStatusFromModules();
    bool EvaluateBattleEntities(uintptr_t object, BattleCandidate& result) const;
    bool ValidateBattleEntities(uintptr_t object, int& outCount, bool& outHasPlayer) const;
    uintptr_t ResolveGameKlass();
    uintptr_t FindGameObject();
    uintptr_t FindGameViaStatics(uintptr_t gameKlass);
    uintptr_t FindGameViaHeap(uintptr_t gameKlass);
    uintptr_t FindGameViaUnityEntry(uintptr_t gameKlass);
    uintptr_t FindBattleViaHeap();
    uintptr_t FindBattleViaBattleModel();
    uintptr_t FindBattleViaGameModel();
    uintptr_t FindNamedKlassNearGame(const char* name, const char* nameSpace);
    bool LooksLikeGame(uintptr_t object, uintptr_t gameKlass) const;
    uintptr_t FindGameViaGom(uintptr_t gameKlass);
    uintptr_t GameFromNativeGameObject(uintptr_t nativeGo, uintptr_t gameKlass) const;
    uintptr_t GameFromNativeGameObjects(const std::vector<uintptr_t>& nativeGos,
                                        uintptr_t gameKlass) const;
    uintptr_t BattleEntitiesFromGameModel(uintptr_t gameModel);
    uintptr_t BattleEntitiesFromGame(uintptr_t game);
    bool CommitBattleWorld(const BattleCandidate& best, const char* source);

    std::string ReadIl2CppString(uintptr_t address) const;
    std::string ReadKlassName(uintptr_t klass) const;
    std::string ReadEnumId(uintptr_t description) const;
    std::string ReadModelEnumId(uintptr_t primitiveModel) const;
    std::string ReadDataEnumId(uintptr_t primitive) const;
    uintptr_t PrimitiveValueAddress(uintptr_t primitive) const;
    uintptr_t ModelValueAddress(uintptr_t primitiveModel) const;

    struct DictItem {
        int key = 0;
        uintptr_t value = 0;
    };
    uintptr_t DictionaryObject(uintptr_t dict, int maxCount) const;
    bool ReadIntDictionary(uintptr_t dict, std::vector<DictItem>& out, int maxCount) const;

    bool ResolveCharacter(uintptr_t character, GameEntity& entity) const;
    void ReadSkeleton(uintptr_t character, GameEntity& entity) const;
    bool ResolveVectorPosition(uintptr_t primitive, GameEntity& entity) const;
    bool ResolveTransform(uintptr_t transform, GameEntity& entity) const;
    void AttachRenderedViews(std::vector<GameEntity>& actors);

    void SetStatus(const std::string& text);

    uintptr_t m_gameAssembly = 0;
    size_t m_gameAssemblySize = 0;
    std::atomic<bool> m_moduleReady{ false };
    std::atomic<bool> m_espReady{ false };
    uintptr_t m_battleKlass = 0;
    uintptr_t m_battleTypeInfoRva = 0;
    bool m_gameAssemblyPriorBuild = false;
    uintptr_t m_modelUpdaterKlass = 0;
    uintptr_t m_viewUpdaterKlass = 0;
    std::atomic<uintptr_t> m_viewUpdater{ 0 };
    bool m_updaterKlassesSearched = false;
    std::atomic<uintptr_t> m_gameObject{ 0 };
    uintptr_t m_gameKlass = 0;
    std::chrono::steady_clock::time_point m_gameKlassAt{};
    std::atomic<bool> m_attachReady{ false };
    std::chrono::steady_clock::time_point m_gameSearchRetry{};
    std::chrono::steady_clock::time_point m_heapSearchRetry{};
    std::chrono::steady_clock::time_point m_gomRetry{};
    int m_battleEvalFails = 0;
    std::atomic<uintptr_t> m_battleEntities{ 0 };
    std::atomic<uintptr_t> m_clientPos{ 0 };
    std::atomic<uintptr_t> m_clientLookQuat{ 0 };
    std::atomic<uintptr_t> m_clientYaw{ 0 };
    std::atomic<uintptr_t> m_clientPitch{ 0 };
    std::atomic<uintptr_t> m_followPos{ 0 };
    std::atomic<uintptr_t> m_recoilObject{ 0 };
    std::atomic<uintptr_t> m_recoilKlass{ 0 };
    std::atomic<uintptr_t> m_fakeSpread{ 0 };
    std::atomic<uintptr_t> m_fakeKlass{ 0 };
    std::atomic<uintptr_t> m_spreadArgsObject{ 0 };
    std::atomic<uintptr_t> m_spreadArgsKlass{ 0 };
    std::atomic<uintptr_t> m_weaponSpreadModel{ 0 };
    std::atomic<uintptr_t> m_brains[8]{};
    std::atomic<int> m_brainCount{ 0 };
    std::atomic<int> m_brainReject{ 0 };
    uintptr_t m_brainKlass = 0;
    uintptr_t m_updaterKlass = 0;
    bool m_brainSearched = false;
    bool m_brainFallbackDone = false;
    std::chrono::steady_clock::time_point m_brainRetry{};
    // 0 none, 1 third-person, 2 aiming subclass.
    std::atomic<uintptr_t> m_rig{ 0 };
    std::atomic<int> m_rigKind{ 0 };
    std::atomic<uintptr_t> m_battleCamera{ 0 };
    std::atomic<uintptr_t> m_battleCameraKlass{ 0 };
    std::atomic<uintptr_t> m_battleModel{ 0 };
    bool m_battleCameraSearched = false;
    std::chrono::steady_clock::time_point m_battleCameraRetry{};
    uintptr_t m_rigKlass = 0;
    bool m_rigSearched = false;
    bool m_rigStringTried = false;
    std::chrono::steady_clock::time_point m_rigRetry{};
    uintptr_t m_lootKlass = 0;

    std::atomic<bool> m_inGame{ false };
    std::atomic<bool> m_mapTransition{ false };
    std::atomic<uint32_t> m_battleEpoch{ 0 };
    std::atomic<int> m_gameWidth{ 0 };
    std::atomic<int> m_gameHeight{ 0 };
    std::chrono::steady_clock::time_point m_resRetry{};
    std::chrono::steady_clock::time_point m_lastScan{};
    bool m_scannedOnce = false;
    bool m_lowScoreRetry = false;
    std::chrono::steady_clock::time_point m_klassRetry{};
    bool m_klassMiss = false;

    std::mutex m_mutex;
    std::unordered_map<uintptr_t, GameEntity> m_known;
    std::vector<GameEntity> m_actors;
    std::vector<GameEntity> m_entities;
    GameEntity m_localPlayer;
    CameraData m_camera;
    std::string m_status;
    std::atomic<uint64_t> m_snapshotGeneration{ 0 };

    std::mutex m_aimMutex;
    bool m_aimActive = false;
    Vector3 m_aimEye{};
    Vector3 m_aimTarget{};
    float m_aimSmooth = 1.f;
    bool WriteMemoryAim(const Vector3& eye, const Vector3& targetWorld, float smooth);
};
