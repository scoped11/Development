#pragma once
#include "types.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

class RobRunSession {
public:
    bool Initialize(const std::string& processName);
    void Shutdown();

    void CacheGlobals();
    void CacheActors();
    void CachePositions();
    void ApplyGun();
    void SetMemoryAimTarget(bool active, const Vector3& eye, const Vector3& targetWorld, float smooth);
    void ServiceMemoryAim();
    void UpdateFreecam(float dt, bool menuOpen);
    void WriteFreecam();
    void SetShotInput(bool down);
    bool FreecamActive() const { return m_freecamOn.load(); }

    bool IsInGame() const { return m_inGame; }
    bool ReadyForOverlay() const { return m_moduleReady.load(); }
    int DetectedWidth() const { return m_gameWidth.load(); }
    int DetectedHeight() const { return m_gameHeight.load(); }

    std::vector<GameEntity> GetEntities();
    GameEntity GetLocalPlayer();
    CameraData GetCamera();
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
    bool MatchesKlassName(uintptr_t klass, const char* name) const;
    bool IsBattleKlass(uintptr_t klass) const;
    bool IsNamedKlass(uintptr_t klass, const char* name, const char* nameSpace) const;
    uintptr_t KlassFromTypeInfoWord(uintptr_t word, const char* name, const char* nameSpace) const;
    uintptr_t ResolveNamedKlass(const char* name, const char* nameSpace,
                                const uintptr_t* slots, size_t slotCount);
    uintptr_t ResolveBattleKlass();
    uintptr_t ResolveGameKlass();
    void SyncLinkStatusFromModules();
    bool EvaluateBattleEntities(uintptr_t object, BattleCandidate& result) const;
    bool ValidateBattleEntities(uintptr_t object, int& outCount, bool& outHasPlayer) const;
    uintptr_t FindGameObject();
    uintptr_t FindGameViaStatics(uintptr_t gameKlass);
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
    bool WriteMemoryAim(const Vector3& eye, const Vector3& targetWorld, float smooth);
    bool WriteSilentAim(const Vector3& eye, const Vector3& targetWorld);
    void CacheSilentAim(uintptr_t character);
    void ClearSilentAim();
    void ApplyFreecamCamera(CameraData& camera);
    void WriteMaterialChams(std::vector<GameEntity>& actors);
    void FreecamBasis(float yawDeg, float pitchDeg, Vector3& forward, Vector3& right, Vector3& up, float quat[4]) const;

    uintptr_t m_gameAssembly = 0;
    size_t m_gameAssemblySize = 0;
    std::atomic<bool> m_moduleReady{ false };
    std::atomic<bool> m_espReady{ false };
    uintptr_t m_battleKlass = 0;
    uintptr_t m_battleTypeInfoRva = 0;
    bool m_gameAssemblyPriorBuild = false;
    uintptr_t m_viewUpdaterKlass = 0;
    std::atomic<uintptr_t> m_viewUpdater{ 0 };
    std::atomic<uintptr_t> m_gameObject{ 0 };
    uintptr_t m_gameKlass = 0;
    std::chrono::steady_clock::time_point m_gameKlassAt{};
    std::chrono::steady_clock::time_point m_gameSearchRetry{};
    int m_battleEvalFails = 0;
    std::atomic<uintptr_t> m_battleEntities{ 0 };
    std::atomic<uintptr_t> m_clientPos{ 0 };
    std::atomic<uintptr_t> m_clientLookQuat{ 0 };
    std::atomic<uintptr_t> m_clientYaw{ 0 };
    std::atomic<uintptr_t> m_clientPitch{ 0 };
    std::atomic<uintptr_t> m_followPos{ 0 };
    std::atomic<uintptr_t> m_shotInput{ 0 };
    std::atomic<uintptr_t> m_fireFlag{ 0 };
    std::atomic<uintptr_t> m_followCamNative{ 0 };
    std::atomic<uintptr_t> m_cameraFollow{ 0 };
    std::atomic<uintptr_t> m_recoilObject{ 0 };
    std::atomic<uintptr_t> m_recoilKlass{ 0 };
    std::atomic<uintptr_t> m_fakeSpread{ 0 };
    std::atomic<uintptr_t> m_fakeKlass{ 0 };
    std::atomic<uintptr_t> m_spreadArgsObject{ 0 };
    std::atomic<uintptr_t> m_spreadArgsKlass{ 0 };
    std::atomic<uintptr_t> m_weaponSpreadModel{ 0 };
    std::atomic<uintptr_t> m_lookDir{ 0 };
    std::atomic<uintptr_t> m_lookInput{ 0 };
    std::atomic<uintptr_t> m_lastLook{ 0 };
    std::atomic<uintptr_t> m_replLook{ 0 };
    std::atomic<uintptr_t> m_shotInterp{ 0 };
    std::atomic<uintptr_t> m_shotArgsDir{ 0 };
    std::atomic<uintptr_t> m_battleCamera{ 0 };
    std::atomic<uintptr_t> m_battleModel{ 0 };
    uintptr_t m_lootKlass = 0;

    std::atomic<bool> m_inGame{ false };
    std::atomic<bool> m_mapTransition{ false };
    std::atomic<uint32_t> m_battleEpoch{ 0 };
    std::atomic<int> m_gameWidth{ 0 };
    std::atomic<int> m_gameHeight{ 0 };
    std::chrono::steady_clock::time_point m_resRetry{};
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

    std::mutex m_freecamMutex;
    std::atomic<bool> m_freecamOn{ false };
    Vector3 m_freecamPos{};
    float m_freecamYaw = 0.f;
    float m_freecamPitch = 0.f;
    int m_freecamCursorX = 0;
    int m_freecamCursorY = 0;
    bool m_freecamCursorInit = false;
};
