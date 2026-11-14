#include "session.hpp"
#include "offsets.h"
#include "names.hpp"
#include "config/config.h"
#include "core/memory.h"
#include "core/scatter.h"
#include "features/skeleton_pose.h"
#include "render/splash.h"

#include <iostream>
#include <cmath>
#include <chrono>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <unordered_map>
#include <unordered_set>
#include <fstream>
#include <vector>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

namespace {

bool IsPtr(uintptr_t p) {
    return p >= 0x10000 && p < 0x0000800000000000ULL;
}

bool FinitePosition(const Vector3& p) {
    if (!p.IsValid() || p.IsZero()) return false;
    return std::fabsf(p.x) < 200000.f &&
           std::fabsf(p.y) < 200000.f &&
           std::fabsf(p.z) < 200000.f;
}

float LookYawDegrees(const Vector3& forward, float storedYaw) {
    float len = forward.Length();
    if (std::isfinite(len) && len > 0.5f && len < 1.5f) {
        Vector3 n = forward.Normalized();
        return std::atan2f(n.x, n.z) * (180.f / 3.14159265f);
    }
    if (!std::isfinite(storedYaw))
        return 0.f;
    if (std::fabsf(storedYaw) <= 6.5f)
        return storedYaw * (180.f / 3.14159265f);
    return storedYaw;
}

float LookPitchDegrees(const Vector3& forward, float storedPitch) {
    float len = forward.Length();
    if (std::isfinite(len) && len > 0.5f && len < 1.5f) {
        float y = std::clamp(forward.Normalized().y, -1.f, 1.f);
        return std::asinf(y) * (180.f / 3.14159265f);
    }
    if (!std::isfinite(storedPitch))
        return 0.f;
    if (std::fabsf(storedPitch) <= 3.3f)
        return storedPitch * (180.f / 3.14159265f);
    return storedPitch;
}

Vector3 QuatForward(float x, float y, float z, float w) {
    return {
        2.f * (x * z + w * y),
        2.f * (y * z - w * x),
        1.f - 2.f * (x * x + y * y)
    };
}

Vector3 QuatRotate(float x, float y, float z, float w, const Vector3& v) {
    Vector3 u{ x, y, z };
    return u * (2.f * u.Dot(v)) + v * (w * w - u.Dot(u)) + u.Cross(v) * (2.f * w);
}

bool UnitQuat(float x, float y, float z, float w) {
    float len = x * x + y * y + z * z + w * w;
    return len > 0.5f && len < 1.5f;
}

void QuatMul(const float a[4], const float b[4], float out[4]) {
    out[0] = a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1];
    out[1] = a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0];
    out[2] = a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3];
    out[3] = a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2];
}

void SetCameraBasis(FMinimalViewInfo& view, float x, float y, float z, float w) {
    view.hasAxes = false;
    if (!UnitQuat(x, y, z, w)) return;
    view.axisForward = QuatForward(x, y, z, w);
    view.axisRight = {
        1.f - 2.f * (y * y + z * z),
        2.f * (x * y + w * z),
        2.f * (x * z - w * y)
    };
    view.axisUp = {
        2.f * (x * y - w * z),
        1.f - 2.f * (x * x + z * z),
        2.f * (y * z + w * x)
    };
    view.hasAxes = view.axisForward.IsValid() && view.axisRight.IsValid() && view.axisUp.IsValid();
    view.rotation.yaw = LookYawDegrees(view.axisForward, 0.f);
    view.rotation.pitch = LookPitchDegrees(view.axisForward, 0.f);
    view.rotation.roll = 0.f;
}

bool ApplyCameraAxes(FMinimalViewInfo& view, const Vector3& forward,
                     const Vector3& right, const Vector3& up) {
    if (forward.Length() < 0.5f || right.Length() < 0.5f || up.Length() < 0.5f)
        return false;
    view.axisForward = forward.Normalized();
    view.axisRight = right.Normalized();
    view.axisUp = up.Normalized();
    view.hasAxes = true;
    view.rotation.yaw = LookYawDegrees(view.axisForward, 0.f);
    view.rotation.pitch = LookPitchDegrees(view.axisForward, 0.f);
    view.rotation.roll = 0.f;
    return true;
}

} // namespace

bool RobRunSession::Initialize(const std::string&) {
    m_inGame = false;
    m_battleEntities = 0;
    m_gameKlass = 0;
    m_gameObject.store(0);
    m_gameSearchRetry = {};
    m_battleEvalFails = 0;
    m_battleKlass = 0;
    m_viewUpdater.store(0);
    m_viewUpdaterKlass = 0;
    m_battleCamera.store(0);
    m_battleModel.store(0);
    m_followPos.store(0);
    m_shotInput.store(0);
    m_fireFlag.store(0);
    m_followCamNative.store(0);
    m_cameraFollow.store(0);
    m_freecamOn.store(false);
    ClearSilentAim();
    m_gameAssembly = 0;
    m_moduleReady.store(false);
    m_espReady.store(false);
    SetStatus("Waiting for GameAssembly.dll");
    std::cout << "[*] Rob Run DMA session ready (shoulder camera)\n";
    return Memory::Get().IsConnected();
}

void RobRunSession::Shutdown() {
    m_inGame = false;
    m_battleEntities.store(0);
    m_gameObject.store(0);
    m_gameKlass = 0;
    m_viewUpdater.store(0);
    m_viewUpdaterKlass = 0;
    m_gameAssembly = 0;
    m_moduleReady.store(false);
    m_espReady.store(false);
    m_battleKlass = 0;
    m_battleCamera.store(0);
    m_battleModel.store(0);
    m_followPos.store(0);
    m_shotInput.store(0);
    m_fireFlag.store(0);
    m_followCamNative.store(0);
    m_cameraFollow.store(0);
    m_freecamOn.store(false);
    ClearSilentAim();
    m_recoilObject.store(0);
    m_recoilKlass.store(0);
    m_fakeSpread.store(0);
    m_fakeKlass.store(0);
    m_spreadArgsObject.store(0);
    m_spreadArgsKlass.store(0);
    m_weaponSpreadModel.store(0);
    std::lock_guard<std::mutex> lock(m_mutex);
    m_actors.clear();
    m_entities.clear();
    m_known.clear();
    m_localPlayer = {};
    m_camera = {};
}

void RobRunSession::SetStatus(const std::string& text) {
    const bool splash = Splash::Covering();
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        const bool locating =
            m_status == "Waiting for match" ||
            m_status == "In lobby — waiting for match" ||
            m_status == "Locating match world";
        if (locating &&
            (text == "Resolving Game*" ||
             text.find("BattleEntities TypeInfo") != std::string::npos ||
             text == "Waiting for Game TypeInfo"))
            return;
        if (m_status != text)
            Memory::Get().Log("[status] %s\n", text.c_str());
        m_status = text;
        m_camera.status = text;
    }
    if (splash)
        Splash::SetStage(text.c_str());
}

bool RobRunSession::EnsureModule() {
    if (m_gameAssembly) return true;
    auto& mem = Memory::Get();
    m_gameAssembly = mem.GetModuleBase("GameAssembly.dll");
    m_gameAssemblySize = mem.GetModuleSize("GameAssembly.dll");
    if (!m_gameAssembly) return false;
    if (!m_gameAssemblySize)
        m_gameAssemblySize = mem.ReadPeImageSize(m_gameAssembly);
    m_gameAssemblyPriorBuild = offsets::IsPriorGameAssemblySize(m_gameAssemblySize);
  // Hardcoded TypeInfo RVAs from offsets.h — no module scan before first DMA read.
    ResolveGameKlass();
    ResolveBattleKlass();
    mem.EnsureUnityPlayer();
    if (m_gameKlass)
        FindGameObject();
    m_moduleReady.store(true);
    std::cout << "[+] GameAssembly.dll @ 0x" << std::hex << m_gameAssembly
              << " size 0x" << m_gameAssemblySize << std::dec
              << " build=" << (m_gameAssemblyPriorBuild ? "prior" : "current")
              << " gameKlass=0x" << std::hex << m_gameKlass
              << " battleKlass=0x" << m_battleKlass << std::dec << "\n";
    return true;
}

std::string RobRunSession::ReadKlassName(uintptr_t klass) const {
    if (!IsPtr(klass)) return {};
    uintptr_t namePtr = Memory::Get().Read<uintptr_t>(klass + offsets::il2cpp::Class_Name);
    if (!IsPtr(namePtr)) return {};
    return Memory::Get().ReadString(namePtr, 80);
}

std::string RobRunSession::ReadEnumId(uintptr_t description) const {
    if (!IsPtr(description)) return {};
    uintptr_t str = Memory::Get().Read<uintptr_t>(description + offsets::identified::Id);
    return ReadIl2CppString(str);
}

std::string RobRunSession::ReadModelEnumId(uintptr_t primitiveModel) const {
    uintptr_t slot = ModelValueAddress(primitiveModel);
    if (!slot) return {};
    return ReadEnumId(Memory::Get().Read<uintptr_t>(slot));
}

std::string RobRunSession::ReadDataEnumId(uintptr_t primitive) const {
    uintptr_t slot = PrimitiveValueAddress(primitive);
    if (!slot) return {};
    return ReadEnumId(Memory::Get().Read<uintptr_t>(slot));
}

std::string RobRunSession::ReadIl2CppString(uintptr_t address) const {
    if (!IsPtr(address)) return {};
    int length = Memory::Get().Read<int>(address + offsets::il2cpp::String_Length);
    if (length <= 0 || length > 64) return {};

    std::vector<wchar_t> chars(static_cast<size_t>(length));
    if (!Memory::Get().ReadRaw(address + offsets::il2cpp::String_Chars,
                               chars.data(), chars.size() * sizeof(wchar_t)))
        return {};

    std::string out;
    out.reserve(chars.size());
    for (wchar_t c : chars) {
        if (c == 0) break;
        out.push_back(c < 128 ? static_cast<char>(c) : '?');
    }
    return out;
}

uintptr_t RobRunSession::PrimitiveValueAddress(uintptr_t primitive) const {
    if (!IsPtr(primitive)) return 0;
    return primitive + offsets::il2cpp::Primitive_Value;
}

uintptr_t RobRunSession::ModelValueAddress(uintptr_t primitiveModel) const {
    if (!IsPtr(primitiveModel)) return 0;
    uintptr_t data = Memory::Get().Read<uintptr_t>(
        primitiveModel + offsets::il2cpp::PrimitiveModel_Data);
    return PrimitiveValueAddress(data);
}

uintptr_t RobRunSession::DictionaryObject(uintptr_t dict, int maxCount) const {
    if (!IsPtr(dict) || maxCount < 0) return 0;
    auto& mem = Memory::Get();

    auto usable = [&](uintptr_t object, int& count) {
        count = -1;
        if (!IsPtr(object)) return false;
        int live = mem.Read<int>(object + offsets::il2cpp::Dict_Count);
        int freeCount = mem.Read<int>(object + offsets::il2cpp::Dict_FreeCount);
        if (live < 0 || live > maxCount) return false;
        if (freeCount < 0 || freeCount > live) return false;
        if (live > 0) {
            uintptr_t entries = mem.Read<uintptr_t>(object + offsets::il2cpp::Dict_Entries);
            if (!IsPtr(entries)) return false;
            int length = mem.Read<int>(entries + offsets::il2cpp::Array_Length);
            if (length < live || length > 100000) return false;
        }
        count = live;
        return true;
    };

    int outerCount = -1;
    bool outerOk = usable(dict, outerCount);
    if (outerOk)
        return dict;

    // ReadOnly.Dictionary keeps the System dictionary at +0x10. Only try
    // this path when the supplied object is not already a valid dictionary;
    // a raw Dictionary also has a buckets pointer at +0x10.
    uintptr_t inner = mem.Read<uintptr_t>(dict + offsets::il2cpp::Dict_Buckets);
    int innerCount = -1;
    if (usable(inner, innerCount))
        return inner;
    return 0;
}

bool RobRunSession::ReadIntDictionary(uintptr_t dict, std::vector<DictItem>& out, int maxCount) const {
    out.clear();
    dict = DictionaryObject(dict, maxCount);
    if (!dict) return false;

    auto& mem = Memory::Get();
    int count = mem.Read<int>(dict + offsets::il2cpp::Dict_Count);
    int freeCount = mem.Read<int>(dict + offsets::il2cpp::Dict_FreeCount);
    if (count < 0 || count > maxCount) return false;
    if (freeCount < 0) return false;
    if (count == 0) return true;

    uintptr_t entries = mem.Read<uintptr_t>(dict + offsets::il2cpp::Dict_Entries);
    if (!IsPtr(entries)) return false;
    int length = mem.Read<int>(entries + offsets::il2cpp::Array_Length);
    if (length < count || length > 100000) return false;

    const size_t bytes = static_cast<size_t>(length) * offsets::il2cpp::Entry_Stride;
    std::vector<uint8_t> raw(bytes);
    if (!mem.ReadRaw(entries + offsets::il2cpp::Array_First, raw.data(), bytes))
        return false;

    out.reserve(static_cast<size_t>(count));
    for (int i = 0; i < length; ++i) {
        const uint8_t* entry = raw.data() + static_cast<size_t>(i) * offsets::il2cpp::Entry_Stride;
        int hash = *reinterpret_cast<const int*>(entry + offsets::il2cpp::Entry_Hash);
        if (hash < 0) continue;
        int key = *reinterpret_cast<const int*>(entry + offsets::il2cpp::Entry_Key);
        uintptr_t value = *reinterpret_cast<const uintptr_t*>(entry + offsets::il2cpp::Entry_Value);
        if (!IsPtr(value)) continue;
        out.push_back({ key, value });
    }
    return true;
}

bool RobRunSession::EvaluateBattleEntities(uintptr_t object, BattleCandidate& result) const {
    result = {};
    result.address = object;
    if (!IsPtr(object)) return false;

    auto& mem = Memory::Get();
    const uintptr_t header = mem.Read<uintptr_t>(object);
    if (m_battleKlass) {
        if (header != m_battleKlass &&
            !MatchesKlassName(header, "BattleEntitiesModel"))
            return false;
    } else if (!MatchesKlassName(header, "BattleEntitiesModel")) {
        return false;
    }

    uintptr_t world = mem.Read<uintptr_t>(object + offsets::battle_entities::WorldEntities);
    if (!IsPtr(world)) return false;

    auto countOf = [&](uintptr_t dict, int maxCount) {
        uintptr_t live = DictionaryObject(dict, maxCount);
        return live ? mem.Read<int>(live + offsets::il2cpp::Dict_Count) : -1;
    };

    result.characters = countOf(
        mem.Read<uintptr_t>(world + offsets::world::BattleCharacters), 300);
    if (result.characters < 0)
        result.characters = 0;
    result.waiting = countOf(
        mem.Read<uintptr_t>(world + offsets::world::NotSpawnedCharacters), 300);
    result.models = countOf(
        mem.Read<uintptr_t>(object + offsets::battle_entities::Models), 100000);

    uintptr_t player = mem.Read<uintptr_t>(object + offsets::battle_entities::Player);
    result.hasPlayer = IsPtr(player);
    if (result.hasPlayer) {
        uintptr_t client = mem.Read<uintptr_t>(player + offsets::player_entity::ClientModel);
        if (IsPtr(client)) {
            uintptr_t movement = mem.Read<uintptr_t>(client + offsets::player_client::SetMovement);
            if (IsPtr(movement)) {
                Vector3 position = mem.Read<Vector3>(movement + offsets::set_movement::Position);
                result.hasView = FinitePosition(position);
            }
        }

        uintptr_t server = mem.Read<uintptr_t>(player + offsets::player_entity::ServerModel);
        if (IsPtr(server)) {
            GameEntity probe;
            uintptr_t transform = mem.Read<uintptr_t>(server + offsets::character::Transform);
            result.hasServerPosition = ResolveTransform(transform, probe);
            if (!result.hasServerPosition) {
                uintptr_t replicated = mem.Read<uintptr_t>(
                    server + offsets::character::ReplicatedTransform);
                if (IsPtr(replicated)) {
                    result.hasServerPosition = ResolveVectorPosition(
                        mem.Read<uintptr_t>(replicated + offsets::replicated::Position), probe);
                }
            }
        }
    }

    uintptr_t slots = mem.Read<uintptr_t>(object + offsets::battle_entities::CharacterSlots);
    if (IsPtr(slots)) {
        result.slots = countOf(
            mem.Read<uintptr_t>(slots + offsets::slots::ByEntityId), 300);
    }

    result.score = 1;
    result.score += result.characters * 100;
    if (result.waiting > 0) result.score += result.waiting * 50;
    if (result.models > 0) result.score += (std::min)(result.models, 2000) * 2;
    if (result.slots > 0) result.score += result.slots * 20;
    if (result.hasPlayer) result.score += 25;
    if (result.hasServerPosition) result.score += 500;
    if (result.hasView) result.score += 1000;
    return true;
}

bool RobRunSession::ValidateBattleEntities(uintptr_t object, int& outCount, bool& outHasPlayer) const {
    BattleCandidate candidate;
    if (!EvaluateBattleEntities(object, candidate)) {
        outCount = -1;
        outHasPlayer = false;
        return false;
    }
    outCount = candidate.characters;
    outHasPlayer = candidate.hasPlayer;
    return true;
}

bool RobRunSession::MatchesKlassName(uintptr_t klass, const char* name) const {
    if (!IsPtr(klass) || !name)
        return false;
    return ReadKlassName(klass) == name;
}

bool RobRunSession::IsBattleKlass(uintptr_t klass) const {
    return IsNamedKlass(klass, "BattleEntitiesModel", "Game.Battle") ||
           MatchesKlassName(klass, "BattleEntitiesModel");
}

uintptr_t RobRunSession::KlassFromTypeInfoWord(uintptr_t word, const char* name,
                                             const char* nameSpace) const {
    auto& mem = Memory::Get();
    uintptr_t cur = word;
    for (int depth = 0; depth < 4; ++depth) {
        if (!cur)
            return 0;
        if (IsNamedKlass(cur, name, nameSpace) || MatchesKlassName(cur, name))
            return cur;
        if (!IsPtr(cur))
            return 0;
        cur = mem.Read<uintptr_t>(cur);
    }
    return 0;
}


bool RobRunSession::IsNamedKlass(
    uintptr_t klass, const char* name, const char* nameSpace) const {
    if (!IsPtr(klass) || !name || !nameSpace) return false;
    if (ReadKlassName(klass) != name) return false;
    uintptr_t ns = Memory::Get().Read<uintptr_t>(klass + offsets::il2cpp::Class_Namespace);
    if (!IsPtr(ns))
        return true;
    return Memory::Get().ReadString(ns, 80) == nameSpace;
}


uintptr_t RobRunSession::ResolveNamedKlass(
    const char* name, const char* nameSpace,
    const uintptr_t* slots, size_t slotCount) {
    auto& mem = Memory::Get();
    auto klassFromSlot = [&](uintptr_t raw) -> uintptr_t {
        if (IsNamedKlass(raw, name, nameSpace))
            return raw;
        if (!IsPtr(raw))
            return 0;
        uintptr_t ind = 0;
        if (!mem.ReadOk(raw, ind))
            return 0;
        return IsNamedKlass(ind, name, nameSpace) ? ind : 0;
    };
    for (size_t i = 0; i < slotCount; ++i) {
        uintptr_t raw = 0;
        if (!mem.ReadOk(m_gameAssembly + slots[i], raw))
            continue;
        uintptr_t klass = klassFromSlot(raw);
        if (klass)
            return klass;
    }
    return 0;
}

uintptr_t RobRunSession::ResolveBattleKlass() {
    if (m_battleKlass && IsBattleKlass(m_battleKlass))
        return m_battleKlass;

    auto& mem = Memory::Get();
    auto accept = [&](uintptr_t rva, uintptr_t klass) -> uintptr_t {
        m_battleTypeInfoRva = rva;
        m_battleKlass = klass;
        m_klassMiss = false;
        if (rva == offsets::typeinfo::BattleEntitiesModel)
            m_gameAssemblyPriorBuild = false;
        else if (rva == offsets::typeinfo::BattleEntitiesModelDump)
            m_gameAssemblyPriorBuild = true;
        mem.Log("[+] BattleEntitiesModel klass @ 0x%llx rva 0x%llx\n",
                static_cast<unsigned long long>(klass),
                static_cast<unsigned long long>(rva));
        return klass;
    };

    uintptr_t slotCandidates[8] = {};
    size_t slotCount = 0;
    auto pushSlot = [&](uintptr_t rva) {
        if (!rva) return;
        for (size_t i = 0; i < slotCount; ++i) {
            if (slotCandidates[i] == rva) return;
        }
        if (slotCount < std::size(slotCandidates))
            slotCandidates[slotCount++] = rva;
    };
    pushSlot(m_battleTypeInfoRva);
    pushSlot(offsets::typeinfo::BattleEntitiesModel);
    pushSlot(offsets::typeinfo::BattleEntitiesModelDump);

    for (size_t i = 0; i < slotCount; ++i) {
        const uintptr_t rva = slotCandidates[i];
        uintptr_t slotted = 0;
        if (!mem.ReadOk(m_gameAssembly + rva, slotted))
            continue;
        const uintptr_t klass =
            KlassFromTypeInfoWord(slotted, "BattleEntitiesModel", "Game.Battle");
        if (klass)
            return accept(rva, klass);
    }

    static bool loggedSlots = false;
    if (!loggedSlots) {
        loggedSlots = true;
        for (uintptr_t rva : { offsets::typeinfo::BattleEntitiesModel,
                               offsets::typeinfo::BattleEntitiesModelDump }) {
            uintptr_t word = 0;
            mem.ReadOk(m_gameAssembly + rva, word);
            uintptr_t ind = 0;
            if (IsPtr(word))
                mem.ReadOk(word, ind);
            mem.Log("[*] Battle TypeInfo rva 0x%llx word 0x%llx name=%s ind 0x%llx name=%s\n",
                    static_cast<unsigned long long>(rva),
                    static_cast<unsigned long long>(word),
                    ReadKlassName(word).c_str(),
                    static_cast<unsigned long long>(ind),
                    ReadKlassName(ind).c_str());
        }
    }

    auto now = std::chrono::steady_clock::now();
    if (m_klassMiss && now - m_klassRetry < std::chrono::milliseconds(400))
        return 0;
    m_klassRetry = now;
    m_klassMiss = true;
    return 0;
}

uintptr_t RobRunSession::ResolveGameKlass() {
    if (m_gameKlass)
        return m_gameKlass;

    auto& mem = Memory::Get();
    const uintptr_t slots[] = {
        offsets::typeinfo::Game,
        offsets::typeinfo::GameDump,
    };
    for (uintptr_t rva : slots) {
        uintptr_t slotted = 0;
        if (!mem.ReadOk(m_gameAssembly + rva, slotted))
            continue;
        const uintptr_t klass = KlassFromTypeInfoWord(slotted, "Game", "Game");
        if (klass) {
            m_gameKlass = klass;
            if (m_gameKlassAt.time_since_epoch().count() == 0)
                m_gameKlassAt = std::chrono::steady_clock::now();
            std::cout << "[+] Game klass @ 0x" << std::hex << klass << std::dec
                      << " via TypeInfo 0x" << std::hex << rva << std::dec
                      << " name=" << ReadKlassName(klass) << "\n" << std::flush;
            Memory::Get().Log("[+] Game klass @ 0x%llx rva 0x%llx name=%s\n",
                              static_cast<unsigned long long>(klass),
                              static_cast<unsigned long long>(rva),
                              ReadKlassName(klass).c_str());
            return klass;
        }
    }
    return 0;
}

uintptr_t RobRunSession::BattleEntitiesFromGameModel(uintptr_t gameModel) {
    if (!IsPtr(gameModel))
        return 0;
    auto& mem = Memory::Get();
    uintptr_t battleModel = 0;
    if (!mem.ReadOk(gameModel + offsets::game_model::BattleModel, battleModel) || !IsPtr(battleModel))
        return 0;
    m_battleModel.store(battleModel);
    uintptr_t camera = 0;
    if (mem.ReadOk(battleModel + offsets::battle_model::BattleCamera, camera) && IsPtr(camera)) {
        if (m_battleCamera.load() != camera) {
            m_battleCamera.store(camera);
            std::cout << "[+] BattleCamera @ 0x" << std::hex << camera << std::dec
                      << " (BattleModel+0x358)\n" << std::flush;
        }
    }
    uintptr_t battleEntities = 0;
    if (!mem.ReadOk(battleModel + offsets::battle_model::BattleEntities, battleEntities) ||
        !IsPtr(battleEntities))
        return 0;
    return battleEntities;
}

uintptr_t RobRunSession::BattleEntitiesFromGame(uintptr_t game) {
    if (!IsPtr(game))
        return 0;
    auto& mem = Memory::Get();
    uintptr_t gameModel = 0;
    if (!mem.ReadOk(game + offsets::game::GameModel, gameModel) || !IsPtr(gameModel))
        return 0;
    return BattleEntitiesFromGameModel(gameModel);
}

bool RobRunSession::LooksLikeGame(uintptr_t object, uintptr_t gameKlass) const {
    if (!IsPtr(object) || !gameKlass)
        return false;
    auto& mem = Memory::Get();
    uintptr_t header = 0;
    if (!mem.ReadOk(object, header) || header != gameKlass)
        return false;
    uintptr_t gameModel = 0;
    return mem.ReadOk(object + offsets::game::GameModel, gameModel) && IsPtr(gameModel);
}

uintptr_t RobRunSession::FindGameViaStatics(uintptr_t gameKlass) {
    if (!gameKlass)
        return 0;
    auto& mem = Memory::Get();
    uintptr_t statics = 0;
    if (!mem.ReadOk(gameKlass + offsets::il2cpp::Class_StaticFields, statics) || !IsPtr(statics))
        return 0;
    uintptr_t words[8]{};
    if (!mem.ReadRaw(statics, words, sizeof(words)))
        return 0;
    for (uintptr_t word : words) {
        if (LooksLikeGame(word, gameKlass))
            return word;
    }
    return 0;
}


uintptr_t RobRunSession::GameFromNativeGameObject(uintptr_t nativeGo, uintptr_t gameKlass) const {
    if (!IsPtr(nativeGo) || !gameKlass)
        return 0;
    auto& mem = Memory::Get();
    int count = 0;
    uintptr_t comps = 0;
    mem.ReadOk(nativeGo + offsets::game_object::ComponentCount, count);
    mem.ReadOk(nativeGo + offsets::game_object::Components, comps);
    if (!IsPtr(comps))
        return 0;
    if (count < 1 || count > 64)
        count = 8;
    static constexpr uintptr_t kManagedOffs[] = {
        offsets::game_object::ManagedFallback,
        offsets::game_object::ManagedHandle,
        0x10, 0x30, 0x38, 0x40,
    };
    for (int i = 0; i < count; ++i) {
        uintptr_t nativeComp = 0;
        if (!mem.ReadOk(comps + static_cast<uintptr_t>(i) * offsets::game_object::ComponentStride
                            + offsets::game_object::ComponentPointer,
                        nativeComp) ||
            !IsPtr(nativeComp))
            continue;
        for (uintptr_t managedOff : kManagedOffs) {
            uintptr_t managed = 0;
            if (!mem.ReadOk(nativeComp + managedOff, managed) || !IsPtr(managed))
                continue;
            uintptr_t header = 0;
            if (mem.ReadOk(managed, header) && header == gameKlass)
                return managed;
            uintptr_t game = 0;
            if (!mem.ReadOk(managed + offsets::unity_entry::Game, game) || !IsPtr(game))
                continue;
            if (mem.ReadOk(game, header) && header == gameKlass)
                return game;
        }
    }
    return 0;
}

uintptr_t RobRunSession::GameFromNativeGameObjects(const std::vector<uintptr_t>& nativeGos,
                                                 uintptr_t gameKlass) const {
    if (nativeGos.empty() || !gameKlass)
        return 0;
    auto& mem = Memory::Get();
    auto& sc = mem.BorrowScatter();

    const size_t n = (std::min)(nativeGos.size(), size_t{128});
    std::vector<int> count(n, 0);
    std::vector<uintptr_t> comps(n, 0);
    for (size_t i = 0; i < n; ++i) {
        if (!IsPtr(nativeGos[i]))
            continue;
        sc.Prepare(nativeGos[i] + offsets::game_object::ComponentCount, &count[i]);
        sc.Prepare(nativeGos[i] + offsets::game_object::Components, &comps[i]);
    }
    sc.Execute();

    std::vector<uintptr_t> nativeComp(n * 8, 0);
    for (size_t i = 0; i < n; ++i) {
        if (!IsPtr(comps[i]))
            continue;
        int c = count[i];
        if (c < 1 || c > 8) {
            count[i] = 0;
            continue;
        }
        count[i] = c;
        for (int k = 0; k < c; ++k) {
            sc.Prepare(comps[i] + static_cast<uintptr_t>(k) * offsets::game_object::ComponentStride
                           + offsets::game_object::ComponentPointer,
                       &nativeComp[i * 8 + static_cast<size_t>(k)]);
        }
    }
    sc.Execute();

    static constexpr uintptr_t kManagedOffs[] = {
        offsets::game_object::ManagedFallback,
        offsets::game_object::ManagedHandle,
        0x10, 0x20, 0x30, 0x38, 0x40,
    };
    constexpr size_t kOffN = sizeof(kManagedOffs) / sizeof(kManagedOffs[0]);
    const size_t managedN = n * 8 * kOffN;
    std::vector<uintptr_t> managed(managedN, 0);
    for (size_t i = 0; i < n; ++i) {
        const int c = count[i];
        for (int k = 0; k < c && k < 8; ++k) {
            const uintptr_t nc = nativeComp[i * 8 + static_cast<size_t>(k)];
            if (!IsPtr(nc))
                continue;
            const size_t base = (i * 8 + static_cast<size_t>(k)) * kOffN;
            for (size_t o = 0; o < kOffN; ++o)
                sc.Prepare(nc + kManagedOffs[o], &managed[base + o]);
        }
    }
    sc.Execute();

    std::vector<uintptr_t> header(managedN, 0);
    std::vector<uintptr_t> gameField(managedN, 0);
    for (size_t i = 0; i < managedN; ++i) {
        if (!IsPtr(managed[i]))
            continue;
        sc.Prepare(managed[i], &header[i]);
        sc.Prepare(managed[i] + offsets::unity_entry::Game, &gameField[i]);
    }
    sc.Execute();

    std::vector<uintptr_t> candidates;
    candidates.reserve(32);
    std::unordered_set<uintptr_t> seen;
    auto push = [&](uintptr_t p) {
        if (!IsPtr(p) || p == gameKlass)
            return;
        if (seen.insert(p).second)
            candidates.push_back(p);
    };
    for (size_t i = 0; i < managedN; ++i) {
        if (header[i] == gameKlass)
            push(managed[i]);
        push(gameField[i]);
    }
    if (candidates.empty())
        return 0;

    std::vector<uintptr_t> candHeader(candidates.size(), 0);
    std::vector<uintptr_t> candModel(candidates.size(), 0);
    for (size_t i = 0; i < candidates.size(); ++i) {
        sc.Prepare(candidates[i], &candHeader[i]);
        sc.Prepare(candidates[i] + offsets::game::GameModel, &candModel[i]);
    }
    sc.Execute();
    for (size_t i = 0; i < candidates.size(); ++i) {
        if (candHeader[i] == gameKlass)
            return candidates[i];
    }
    return 0;
}

uintptr_t RobRunSession::FindGameViaGom(uintptr_t gameKlass) {
    if (!gameKlass)
        return 0;
    auto& mem = Memory::Get();
    if (mem.MotionBurstDepth() > 0)
        return 0;
    if (!mem.UnityPlayerBase())
        mem.EnsureUnityPlayer();
    const uintptr_t unityPlayer = mem.UnityPlayerBase();
    if (!unityPlayer)
        return 0;

    uintptr_t gomPtr = 0;
    if (!mem.ReadOk(unityPlayer + offsets::unity_player::GameObjectManager, gomPtr) || !IsPtr(gomPtr))
        return 0;

    uintptr_t node = 0;
    if (!mem.ReadOk(gomPtr + offsets::gom::EmbeddedList + offsets::gom::ListFirst, node) || !IsPtr(node))
        return 0;

    auto& sc = mem.BorrowScatter();
    std::vector<uintptr_t> gos;
    gos.reserve(32);
    uintptr_t cur = node;
    for (int i = 0; i < 32 && IsPtr(cur); ++i) {
        if (mem.MotionBurstDepth() > 0)
            return 0;
        uintptr_t next = 0;
        uintptr_t go = 0;
        sc.Prepare(cur + offsets::gom::NodeNext, &next);
        sc.Prepare(cur + offsets::gom::NodeObject, &go);
        sc.Execute();
        if (IsPtr(go))
            gos.push_back(go);
        if (!IsPtr(next) || next == node || next == cur)
            break;
        cur = next;
    }
    if (gos.empty())
        return 0;
    return GameFromNativeGameObjects(gos, gameKlass);
}

uintptr_t RobRunSession::FindGameObject() {
    auto& mem = Memory::Get();
    const uintptr_t klass = ResolveGameKlass();
    if (!klass)
        return 0;

    uintptr_t cached = m_gameObject.load();
    if (cached) {
        uintptr_t header = 0;
        if (mem.ReadOk(cached, header) && header == klass) {
            BattleEntitiesFromGame(cached);
            return cached;
        }
        m_gameObject.store(0);
        cached = 0;
    }

    // Game+0xD0 → GameModel+0x10 → BattleModel+0x90 / +0x358. No heap scan.
    // Game lives on UnityEntry+0x28; UnityPlayer GOM slot is a fixed RVA.
    if (const uintptr_t viaStatics = FindGameViaStatics(klass)) {
        m_gameObject.store(viaStatics);
        BattleEntitiesFromGame(viaStatics);
        return viaStatics;
    }
    const uintptr_t viaGom = FindGameViaGom(klass);
    if (!viaGom)
        return 0;
    m_gameObject.store(viaGom);
    BattleEntitiesFromGame(viaGom);
    std::cout << "[+] Game @ 0x" << std::hex << viaGom << std::dec
              << " via UnityPlayer+0x" << std::hex
              << offsets::unity_player::GameObjectManager << std::dec
              << " UnityEntry+0x28\n" << std::flush;
    Memory::Get().Log("[+] Game @ 0x%llx via UnityPlayer GOM\n",
                      static_cast<unsigned long long>(viaGom));
    return viaGom;
}

bool RobRunSession::CommitBattleWorld(const BattleCandidate& best, const char* source) {
    const uintptr_t prevBattle = m_battleEntities.exchange(best.address);
    if (prevBattle != best.address) {
        ++m_battleEpoch;
        m_mapTransition.store(true);
        m_lowScoreRetry = false;
        if (prevBattle != 0)
            std::cout << "[*] Map/world change — refreshing ESP anchors\n" << std::flush;
        std::cout << "[+] BattleEntities @ 0x" << std::hex << best.address << std::dec
                  << " source=" << source
                  << " score=" << best.score
                  << " characters=" << best.characters
                  << " waiting=" << best.waiting
                  << " models=" << best.models
                  << " slots=" << best.slots
                  << " local=" << (best.hasPlayer ? "yes" : "no")
                  << " serverPos=" << (best.hasServerPosition ? "yes" : "no")
                  << " view=" << (best.hasView ? "yes" : "no")
                  << "\n" << std::flush;
        Memory::Get().Log("[+] BattleEntities @ 0x%llx source=%s characters=%d\n",
                          static_cast<unsigned long long>(best.address), source,
                          best.characters);
    }
    m_inGame = true;
    SetStatus("In match — waiting for shoulder camera");
    Splash::SetDetail("BattleModel+0x358 lens (shoulder). ESP waits until that pose is valid.");
    return true;
}

void RobRunSession::SyncLinkStatusFromModules() {
    if (!Memory::Get().IsConnected())
        return;
    auto& mem = Memory::Get();
    if (mem.GetModuleBase("GameAssembly.dll"))
        return;
    if (mem.GetModuleBase("UnityPlayer.dll"))
        SetStatus("Rob Run is running — GameAssembly.dll not visible to DMA yet");
    else
        SetStatus("Attached — check process name or launch Rob Run");
}

bool RobRunSession::FindBattleEntities() {
    if (!EnsureModule()) {
        SetStatus("GameAssembly.dll not loaded yet");
        return false;
    }

    uintptr_t current = m_battleEntities.load();
    if (current) {
        BattleCandidate selected;
        if (EvaluateBattleEntities(current, selected)) {
            m_battleEvalFails = 0;
            return true;
        }
        // One bad DMA read is not a map change. Heap-scanning here stalls
        // the FPGA and SNAP jumps to 1000ms+.
        if (++m_battleEvalFails < 12)
            return true;
        std::cout << "[*] BattleEntities moved, chasing Game chain again\n";
        m_battleEntities.store(0);
        m_mapTransition.store(true);
        m_battleEvalFails = 0;
    }

    auto tryCommit = [&](uintptr_t object, const char* source) -> bool {
        if (!object)
            return false;
        if (!m_battleKlass)
            ResolveBattleKlass();
        BattleCandidate candidate;
        if (!EvaluateBattleEntities(object, candidate))
            return false;
        return CommitBattleWorld(candidate, source);
    };

    ResolveGameKlass();
    const uintptr_t game = FindGameObject();
    const uintptr_t chained = game ? BattleEntitiesFromGame(game) : 0;
    if (tryCommit(chained, "game-chain"))
        return true;

    m_inGame = false;
    m_espReady.store(false);
    if (game) {
        char detail[180];
        snprintf(detail, sizeof(detail),
                 "Game 0x%llx   camera BattleModel+0x358   players BattleEntities+0x90 when the match starts",
                 static_cast<unsigned long long>(game));
        SetStatus("In menu");
        Splash::SetDetail(detail);
    } else if (!Memory::Get().GetModuleBase("UnityPlayer.dll")) {
        SetStatus("Waiting for UnityPlayer.dll");
        Splash::SetDetail("UnityPlayer.dll is not in the module map yet");
    } else if (ResolveGameKlass()) {
        SetStatus("In menu");
        Splash::SetDetail("Game class is known. Match camera is BattleModel+0x358. Players are BattleEntities+0x90.");
    } else {
        SetStatus("Waiting for Game TypeInfo");
        Splash::SetDetail("Game TypeInfo slot has not resolved");
    }
    return false;
}

bool RobRunSession::ResolveVectorPosition(uintptr_t primitive, GameEntity& entity) const {
    uintptr_t value = PrimitiveValueAddress(primitive);
    if (!value) return false;
    entity.posVec = value;
    entity.posX = entity.posY = entity.posZ = 0;
    return true;
}

bool RobRunSession::ResolveTransform(uintptr_t transform, GameEntity& entity) const {
    if (!IsPtr(transform)) return false;
    auto& mem = Memory::Get();
    uintptr_t x = mem.Read<uintptr_t>(transform + offsets::transform::X);
    uintptr_t y = mem.Read<uintptr_t>(transform + offsets::transform::Y);
    uintptr_t z = mem.Read<uintptr_t>(transform + offsets::transform::Z);
    uintptr_t xAddr = PrimitiveValueAddress(x);
    uintptr_t yAddr = PrimitiveValueAddress(y);
    uintptr_t zAddr = PrimitiveValueAddress(z);
    if (!xAddr) xAddr = ModelValueAddress(x);
    if (!yAddr) yAddr = ModelValueAddress(y);
    if (!zAddr) zAddr = ModelValueAddress(z);
    if (!xAddr || !yAddr || !zAddr) return false;
    entity.posX = xAddr;
    entity.posY = yAddr;
    entity.posZ = zAddr;
    entity.posVec = 0;
    return true;
}

static void CacheSkeletonPose(GameEntity& entity) {
    entity.skeletonAimReady = false;
    entity.skeletonPoseCached = false;
    entity.skeletonPositions.clear();
    entity.skeletonSegments.clear();
    entity.skeletonPelvis = {};
    entity.skeletonChest = {};
    entity.skeletonNeck = {};
    entity.skeletonHead = {};
    if (entity.boneCount < 2 || entity.boneFloats.empty())
        return;
    const auto pose = SkeletonPose::Build(entity);
    if (!pose.ok)
        return;
    entity.skeletonPoseCached = true;
    entity.skeletonPositions = std::move(pose.positions);
    entity.skeletonSegments = std::move(pose.segments);
    entity.skeletonPelvis = pose.pelvis;
    entity.skeletonChest = pose.chest;
    entity.skeletonNeck = pose.neck;
    entity.skeletonHead = pose.head;
    entity.skeletonAimReady = entity.skeletonHead.IsValid();
}

void RobRunSession::ReadSkeleton(uintptr_t character, GameEntity& entity) const {
    entity.boneCount = 0;
    entity.boneStride = 0;
    entity.boneLocal = 0;
    entity.boneModel = 0;
    entity.boneLength = 0;
    entity.boneData = 0;
    entity.boneBytes = 0;
    entity.boneParents.clear();
    entity.boneIdxRoot = entity.boneIdxLegs = entity.boneIdxSpine2 = entity.boneIdxSpine3 = -1;
    entity.boneIdxNeck = entity.boneIdxHead1 = entity.boneIdxHead2 = -1;

    auto& mem = Memory::Get();
    uintptr_t model = mem.Read<uintptr_t>(character + offsets::character::Skeleton);
    uintptr_t config = IsPtr(model) ? mem.Read<uintptr_t>(model + offsets::skeleton::Config) : 0;
    uintptr_t bones = IsPtr(model) ? mem.Read<uintptr_t>(model + offsets::skeleton::Bones) : 0;
    if (!IsPtr(model) || !IsPtr(config) || !IsPtr(bones)) {
        static bool loggedMissing = false;
        if (!loggedMissing) {
            loggedMissing = true;
            std::cout << "[+] Skeleton missing model=0x" << std::hex << model
                      << " config=0x" << config << " bones=0x" << bones
                      << std::dec << "\n" << std::flush;
        }
        return;
    }

    int count = mem.Read<int>(config + offsets::skeleton_config::BonesCount);
    int stride = mem.Read<int>(config + offsets::skeleton_config::BoneDataSize);
    int local = mem.Read<int>(config + offsets::skeleton_config::LocalOrientation);
    int modelOff = mem.Read<int>(config + offsets::skeleton_config::ModelOrientation);
    int lengthOff = mem.Read<int>(config + offsets::skeleton_config::Length);
    int maxChild = mem.Read<int>(config + offsets::skeleton_config::MaxChildren);
    int arrLen = IsPtr(bones) ? mem.Read<int>(bones + offsets::il2cpp::Array_Length) : -1;
    static bool loggedSkeleton = false;
    if (!loggedSkeleton) {
        loggedSkeleton = true;
        std::cout << "[+] Skeleton model=0x" << std::hex << model
                  << " config=0x" << config << " bones=0x" << bones << std::dec
                  << " count=" << count << " stride=" << stride
                  << " local=" << local << " modelOff=" << modelOff
                  << " length=" << lengthOff << " children=" << maxChild
                  << " arr=" << arrLen << "\n" << std::flush;
    }
    if (count < 2 || count > 128) return;
    if (stride < 5 || stride > 64) return;
    if (local < 0 || local + 4 > stride || lengthOff < 0 || lengthOff >= stride) return;

    const int need = count * stride;
    if (arrLen < need || arrLen > 8192) return;
    const int haveFloats = need;
    if (modelOff < 0) modelOff = local;
    if (maxChild < 1 || maxChild > 32) maxChild = 0;

    entity.boneParents.assign(static_cast<size_t>(count), -1);
    int chain[7] = {};
    mem.ReadRaw(config + offsets::skeleton_config::Chain, chain, sizeof(chain));
    entity.boneIdxLegs = chain[1];
    entity.boneIdxSpine2 = chain[2];
    entity.boneIdxSpine3 = chain[3];
    entity.boneIdxNeck = chain[4];
    entity.boneIdxHead1 = chain[5];
    entity.boneIdxHead2 = chain[6];
    const int rootBone = (chain[0] >= 0 && chain[0] < count) ? chain[0] : 0;
    entity.boneIdxRoot = rootBone;

    bool anyChildFromTable = false;
    uintptr_t hier = mem.Read<uintptr_t>(config + offsets::skeleton_config::Hierarchy);
    const int needH = count * maxChild;
    if (maxChild > 0 && IsPtr(hier)) {
        int hlen = mem.Read<int>(hier + offsets::il2cpp::Array_Length);
        if (hlen > 0 && hlen <= 8192) {
            const int haveH = (std::min)(hlen, needH);
            std::vector<int> kids(static_cast<size_t>(haveH), -1);
            if (mem.ReadRaw(hier + offsets::il2cpp::Array_First, kids.data(),
                             static_cast<size_t>(haveH) * sizeof(int))) {
                for (int bone = 0; bone < count; ++bone) {
                    for (int c = 0; c < maxChild; ++c) {
                        const int at = bone * maxChild + c;
                        if (at >= haveH) break;
                        const int child = kids[static_cast<size_t>(at)];
                        if (child < 0) break;
                        if (child >= count || child == bone || child == rootBone) continue;
                        if (entity.boneParents[static_cast<size_t>(child)] != -1) continue;
                        entity.boneParents[static_cast<size_t>(child)] = bone;
                        anyChildFromTable = true;
                    }
                }
            }
        }
    }

    auto applySpineChainParents = [&]() {
        // Legs and spine both hang off the root. A linear chain through the
        // legs index yanks the torso off the body.
        entity.boneParents.assign(static_cast<size_t>(count), -1);
        auto link = [&](int child, int parent) {
            if (child < 0 || child >= count || parent < 0 || parent >= count || child == parent)
                return;
            entity.boneParents[static_cast<size_t>(child)] = parent;
        };
        const int root = chain[0];
        const int legs = chain[1];
        const int spine2 = chain[2];
        const int spine3 = chain[3];
        const int neck = chain[4];
        const int head1 = chain[5];
        const int head2 = chain[6];
        link(legs, root);
        link(spine2, root);
        link(spine3, spine2);
        link(neck, spine3);
        link(head1, neck);
        link(head2, head1);
    };

    auto hierarchySane = [&]() -> bool {
        if (!anyChildFromTable) return false;
        int linked = 0;
        int maxDepth = 0;
        std::vector<int> depth(static_cast<size_t>(count), -1);
        std::vector<int> childCount(static_cast<size_t>(count), 0);
        for (int bone = 0; bone < count; ++bone) {
            const int parent = entity.boneParents[static_cast<size_t>(bone)];
            if (parent < 0) continue;
            if (parent >= count || parent == bone) return false;
            ++linked;
            ++childCount[static_cast<size_t>(parent)];
        }
        for (int c : childCount)
            if (c > 16) return false;
        if (linked < 2 || linked >= count) return false;
        int root = rootBone;
        if (root < 0 || root >= count) return false;
        if (entity.boneParents[static_cast<size_t>(root)] >= 0) return false;
        depth[static_cast<size_t>(root)] = 0;
        for (int pass = 0; pass < count; ++pass) {
            bool changed = false;
            for (int bone = 0; bone < count; ++bone) {
                const int parent = entity.boneParents[static_cast<size_t>(bone)];
                if (parent < 0) continue;
                const int parentDepth = depth[static_cast<size_t>(parent)];
                if (parentDepth < 0) continue;
                const int next = parentDepth + 1;
                if (depth[static_cast<size_t>(bone)] < next) {
                    depth[static_cast<size_t>(bone)] = next;
                    if (next > maxDepth) maxDepth = next;
                    changed = true;
                }
            }
            if (!changed) break;
        }
        int reached = 0;
        for (int d : depth)
            if (d >= 0) ++reached;
        return maxDepth >= 2 && maxDepth <= 32 && reached >= linked;
    };

    const bool hierarchyFromTable = hierarchySane();
    if (!hierarchyFromTable)
        applySpineChainParents();

    static int archLogCount = 0;
    if (archLogCount < 3) {
        ++archLogCount;
        std::ofstream arch(R"(C:\Development\.cursor\skeleton-reference.log)",
                          archLogCount == 1 ? std::ios::trunc : std::ios::app);
        if (arch) {
            arch << "=== skeleton snapshot #" << archLogCount
                 << " character=0x" << std::hex << character << std::dec << " ===\n";
            arch << "[offsets] dump/2026-11-10 BattleCharacterModel.SkeletonModel +0x"
                 << std::hex << offsets::character::Skeleton << std::dec << "\n";
            arch << "[offsets] SkeletonModel: Config +0x" << std::hex << offsets::skeleton::Config
                 << std::dec << " Bones(float[]) +0x" << std::hex << offsets::skeleton::Bones
                 << std::dec << "\n";
            arch << "[offsets] SkeletonConfig: count +0x10 stride +0x14 localQuatIdx +0x18"
                 << " modelQuatIdx +0x1C lengthIdx +0x20 hierarchy +0x28 maxChildren +0x30\n";
            arch << "[offsets] named indices from +0x34: root, legs, spine2, spine3, neck, head1, head2\n";
            arch << "[runtime] count=" << count << " stride=" << stride
                 << " localQuatIdx=" << local << " modelQuatIdx=" << modelOff
                 << " lengthIdx=" << lengthOff << " maxChildren=" << maxChild
                 << " boneFloats=" << haveFloats << "\n";
            arch << "[runtime] chain root=" << chain[0] << " legs=" << chain[1]
                 << " spine2=" << chain[2] << " spine3=" << chain[3] << " neck=" << chain[4]
                 << " head1=" << chain[5] << " head2=" << chain[6]
                 << " rootUsed=" << rootBone << "\n";
            arch << "[runtime] parents=" << (hierarchyFromTable ? "hierarchy[] table" : "spine fallback")
                 << " (legs->root, spine2->root, spine3->spine2, neck->spine3, head1->neck, head2->head1)\n";
            arch << "[parents] ";
            for (int b = 0; b < count; ++b) {
                const int p = entity.boneParents[static_cast<size_t>(b)];
                if (p >= 0)
                    arch << b << "<-" << p << " ";
            }
            arch << "\n";
            arch << "[fk] SkeletonPose::Build — root at entity.position (view/sim feet);\n";
            arch << "  facing=yaw from aimForward; segment=parentRot * (0,length,0);\n";
            arch << "  childRot=parentRot*localQuat; localQuat at boneRow[localQuatIdx..+3];\n";
            arch << "  max bone reach 3.5m; fallback stick DrawPosedSkeleton if DMA bones missing;\n";
            arch << "  named aim: pelvis=root chest=spine3 neck neck head=head2|head1\n\n";
            arch.flush();
            std::cout << "[+] Skeleton architecture logged (.cursor/skeleton-reference.log)\n"
                      << std::flush;
        }
    }

    entity.boneCount = count;
    entity.boneStride = stride;
    entity.boneLocal = local;
    entity.boneModel = modelOff;
    entity.boneLength = lengthOff;
    entity.boneData = bones + offsets::il2cpp::Array_First;
    entity.boneBytes = haveFloats * static_cast<int>(sizeof(float));
    entity.boneFloats.clear();
    if (haveFloats > 0 && haveFloats <= 1024) {
        entity.boneFloats.resize(static_cast<size_t>(haveFloats));
        if (!mem.ReadRaw(entity.boneData, entity.boneFloats.data(),
                         static_cast<size_t>(entity.boneBytes)))
            entity.boneFloats.clear();
    }
}

bool RobRunSession::ResolveCharacter(uintptr_t character, GameEntity& entity) const {
    auto& mem = Memory::Get();
    entity.address = character;
    entity.creationIndex = mem.Read<int>(character + offsets::character::Id);
    entity.type = EntityType::Player;
    entity.isEnabled = true;
    entity.displayName = "Player";

    uintptr_t transform = mem.Read<uintptr_t>(character + offsets::character::Transform);
    if (!ResolveTransform(transform, entity)) {
        uintptr_t replicated = mem.Read<uintptr_t>(character + offsets::character::ReplicatedTransform);
        if (IsPtr(replicated)) {
            uintptr_t pos = mem.Read<uintptr_t>(replicated + offsets::replicated::Position);
            ResolveVectorPosition(pos, entity);
        }
    }
    {
        uintptr_t replicated = mem.Read<uintptr_t>(character + offsets::character::ReplicatedTransform);
        if (IsPtr(replicated)) {
            uintptr_t posPrim = mem.Read<uintptr_t>(replicated + offsets::replicated::Position);
            entity.repPosAddr = PrimitiveValueAddress(posPrim);
        }
    }

    uintptr_t look = mem.Read<uintptr_t>(character + offsets::character::LookDirection);
    if (IsPtr(look))
        entity.lookAddr = look + offsets::look::Forward;

    entity.role = ReadModelEnumId(mem.Read<uintptr_t>(character + offsets::character::Role));

    uintptr_t knockout = mem.Read<uintptr_t>(character + offsets::character::Knockout);
    if (IsPtr(knockout)) {
        entity.extra = ReadDataEnumId(mem.Read<uintptr_t>(knockout + offsets::knockout::Phase));
        std::string phase = entity.extra;
        for (char& c : phase)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        entity.isKnocked = phase.find("knock") != std::string::npos ||
                           phase.find("revive") != std::string::npos;
    }

    uintptr_t priv = mem.Read<uintptr_t>(character + offsets::character::PrivateModel);
    if (IsPtr(priv)) {
        uintptr_t health = mem.Read<uintptr_t>(priv + offsets::private_model::Health);
        if (IsPtr(health)) {
            uintptr_t amount = mem.Read<uintptr_t>(health + offsets::health::Amount);
            entity.healthAddr = PrimitiveValueAddress(amount);
            entity.maxHealthAddr = health + offsets::health::MaxAmount;
            uintptr_t deadModel = mem.Read<uintptr_t>(health + offsets::health::IsDead);
            entity.deadAddr = ModelValueAddress(deadModel);
        }
        uintptr_t sprint = mem.Read<uintptr_t>(priv + offsets::private_model::CheatSprint);
        entity.speedAddr = ModelValueAddress(sprint);
        uintptr_t armor = mem.Read<uintptr_t>(priv + offsets::private_model::Armor);
        if (IsPtr(armor)) {
            uintptr_t ammo = mem.Read<uintptr_t>(armor + offsets::armor::Amount);
            if (IsPtr(ammo)) {
                uintptr_t data = mem.Read<uintptr_t>(ammo + offsets::ammo_model::Data);
                entity.armorAmountAddr = PrimitiveValueAddress(data);
            }
        }
    }

    uintptr_t crouch = mem.Read<uintptr_t>(character + offsets::character::Crouch);
    if (IsPtr(crouch)) {
        uintptr_t flag = mem.Read<uintptr_t>(crouch + offsets::crouch::IsCrouching);
        entity.crouchAddr = PrimitiveValueAddress(flag);
    }
    entity.sprintAddr = ModelValueAddress(mem.Read<uintptr_t>(character + offsets::character::IsSprinting));
    entity.armorAddr = ModelValueAddress(mem.Read<uintptr_t>(character + offsets::character::HasArmor));
    {
        uintptr_t custom = mem.Read<uintptr_t>(character + offsets::character::Customization);
        uintptr_t cdata = IsPtr(custom) ? mem.Read<uintptr_t>(custom + offsets::customization::Data) : 0;
        if (IsPtr(cdata)) {
            entity.gender = ReadDataEnumId(mem.Read<uintptr_t>(cdata + offsets::customization_data::Sex));
            const bool female = Names::Lower(entity.gender).find("female") != std::string::npos;
            entity.outfit = ReadDataEnumId(mem.Read<uintptr_t>(
                cdata + (female ? offsets::customization_data::FBody : offsets::customization_data::Body)));
            if (entity.outfit.empty())
                entity.outfit = ReadDataEnumId(mem.Read<uintptr_t>(
                    cdata + (female ? offsets::customization_data::FBag : offsets::customization_data::Bag)));
            entity.hair = ReadDataEnumId(mem.Read<uintptr_t>(
                cdata + (female ? offsets::customization_data::FHair : offsets::customization_data::Hair)));
            if (!female)
                entity.beard = ReadDataEnumId(mem.Read<uintptr_t>(cdata + offsets::customization_data::Beard));
            entity.gender = Names::Pretty(entity.gender);
            entity.outfit = Names::Pretty(entity.outfit);
            entity.hair = Names::Pretty(entity.hair);
            entity.beard = Names::Pretty(entity.beard);
        }
        uintptr_t cop = mem.Read<uintptr_t>(character + offsets::character::CopCustomize);
        if (IsPtr(cop)) {
            std::string copOutfit = ReadDataEnumId(mem.Read<uintptr_t>(cop + offsets::cop_customize::Data));
            if (!copOutfit.empty()) {
                entity.outfit = Names::Pretty(copOutfit);
                entity.policeUniform = true;
            }
        }
        const std::string roleLower = Names::Lower(entity.role);
        if (roleLower.find("cop") != std::string::npos && roleLower.find("hub") == std::string::npos)
            entity.policeUniform = true;
    }
    {
        uintptr_t leave = mem.Read<uintptr_t>(character + offsets::character::KnockdownLeave);
        if (IsPtr(leave))
            entity.leaveAddr = leave + 0x10;
        uintptr_t state = mem.Read<uintptr_t>(character + offsets::character::State);
        uintptr_t hidden = IsPtr(state) ? mem.Read<uintptr_t>(state + 0x20) : 0;
        if (IsPtr(hidden))
            entity.hiddenAddr = hidden + offsets::character_hidden::IsHidden;
    }
    uintptr_t vehicle = mem.Read<uintptr_t>(character + offsets::character::InVehicle);
    if (IsPtr(vehicle)) {
        uintptr_t idPrim = mem.Read<uintptr_t>(vehicle + offsets::in_vehicle::EntityId);
        entity.vehicleIdAddr = PrimitiveValueAddress(idPrim);
    }
    ReadSkeleton(character, entity);
    if (!entity.deadAddr) {
        uintptr_t deadModel = mem.Read<uintptr_t>(character + offsets::character::IsDead);
        entity.deadAddr = ModelValueAddress(deadModel);
    }
    return entity.posVec || entity.posX || entity.repPosAddr;
}


void RobRunSession::CacheGlobals() {
    if (!Memory::Get().IsConnected()) return;
    auto& mem = Memory::Get();
    mem.BeginDiscoveryBurst();
    SyncLinkStatusFromModules();
    const auto now = std::chrono::steady_clock::now();
    const bool haveRes = m_gameWidth.load() > 0 && m_gameHeight.load() > 0;
    const auto wait = haveRes ? std::chrono::seconds(20) : std::chrono::seconds(4);
    if (m_resRetry.time_since_epoch().count() == 0 || now - m_resRetry >= wait) {
        m_resRetry = now;
        int width = 0;
        int height = 0;
        if (Memory::Get().ReadUnityResolution(width, height) &&
            (width != m_gameWidth.load() || height != m_gameHeight.load())) {
            m_gameWidth.store(width);
            m_gameHeight.store(height);
            std::cout << "[+] Game resolution " << width << "x" << height << "\n" << std::flush;
        }
    }
    if (!FindBattleEntities()) {
        m_inGame = false;
        m_mapTransition.store(true);
        if (m_gameAssemblyPriorBuild &&
            offsets::IsPriorGameAssemblySize(m_gameAssemblySize) &&
            !m_battleEntities.load()) {
            static auto lastCr3 = std::chrono::steady_clock::time_point{};
            const auto nowCr3 = std::chrono::steady_clock::now();
            if (lastCr3.time_since_epoch().count() == 0 ||
                nowCr3 - lastCr3 >= std::chrono::seconds(15)) {
                lastCr3 = nowCr3;
                std::cout << "[*] Prior GameAssembly in memory — retrying CR3\n" << std::flush;
                mem.FixCr3();
                m_scannedOnce = false;
                m_klassMiss = false;
            }
            SetStatus(
                "Prior GameAssembly in RAM — restart Rob Run from Steam for best results.");
        }
        mem.EndDiscoveryBurst();
        return;
    }
    m_inGame = true;
    if (m_mapTransition.exchange(false)) {
        CacheActors();
        Memory::Get().BeginMotionBurst();
        CachePositions();
        Memory::Get().EndMotionBurst();
        std::cout << "[+] Instant refresh: actors + positions after map change\n" << std::flush;
    }
    mem.EndDiscoveryBurst();
}

void RobRunSession::CacheActors() {
    uintptr_t battle = m_battleEntities.load();
    if (!m_inGame || !battle) return;
    auto& mem = Memory::Get();

    uintptr_t world = mem.Read<uintptr_t>(battle + offsets::battle_entities::WorldEntities);
    uintptr_t slotsModel = mem.Read<uintptr_t>(battle + offsets::battle_entities::CharacterSlots);
    uintptr_t playerEnt = mem.Read<uintptr_t>(battle + offsets::battle_entities::Player);

    uintptr_t localCharacter = 0;
    int localId = -1;
    if (IsPtr(playerEnt)) {
        localCharacter = mem.Read<uintptr_t>(playerEnt + offsets::player_entity::ServerModel);
        localId = mem.Read<int>(playerEnt + offsets::player_entity::Id);
        uintptr_t client = mem.Read<uintptr_t>(playerEnt + offsets::player_entity::ClientModel);
        uintptr_t pos = 0;
        uintptr_t lookQuat = 0;
        uintptr_t yaw = 0;
        uintptr_t pitch = 0;
        uintptr_t follow = 0;
        if (IsPtr(client)) {
            uintptr_t movement = mem.Read<uintptr_t>(client + offsets::player_client::SetMovement);
            uintptr_t rotation = mem.Read<uintptr_t>(client + offsets::player_client::Rotation);
            uintptr_t cameraModel = mem.Read<uintptr_t>(client + offsets::player_client::CameraModel);
            if (IsPtr(movement))
                pos = movement + offsets::set_movement::Position;
            if (IsPtr(rotation)) {
                lookQuat = rotation + offsets::client_rotation::LookRotation;
                yaw = rotation + offsets::client_rotation::LookYaw;
                pitch = rotation + offsets::client_rotation::Pitch;
            }
            if (IsPtr(cameraModel))
                follow = cameraModel + offsets::player_camera::FollowPosition;
        }
        m_clientPos.store(pos);
        m_clientLookQuat.store(lookQuat);
        m_clientYaw.store(yaw);
        m_clientPitch.store(pitch);
        m_followPos.store(follow);
        const auto& aim = Config::Get().assist.aim;
        uintptr_t recoilObject = 0;
        uintptr_t recoilKlass = 0;
        uintptr_t fakeSpread = 0;
        uintptr_t fakeKlass = 0;
        uintptr_t spreadArgsObject = 0;
        uintptr_t spreadArgsKlass = 0;
        uintptr_t weaponSpreadModel = 0;
        if ((aim.noRecoil || aim.noSway || aim.noSpread) && IsPtr(client)) {
            if (aim.noRecoil) {
                uintptr_t recoil = mem.Read<uintptr_t>(client + offsets::player_client::Recoil);
                uintptr_t klass = IsPtr(recoil) ? mem.Read<uintptr_t>(recoil) : 0;
                if (IsNamedKlass(klass, "RecoilModel", "Game.Battle.Entities.Player.Recoil")) {
                    recoilObject = recoil;
                    recoilKlass = klass;
                }
            }
            if (aim.noSway) {
                uintptr_t fake = mem.Read<uintptr_t>(client + offsets::player_client::FakeSpread);
                uintptr_t klass = IsPtr(fake) ? mem.Read<uintptr_t>(fake) : 0;
                if (IsNamedKlass(klass, "PlayerFakeSpreadModel", "Game.Battle.Entities.Player.Spread")) {
                    fakeSpread = fake;
                    fakeKlass = klass;
                }
            }
        }
        if (aim.noSpread && IsPtr(localCharacter)) {
            uintptr_t priv = mem.Read<uintptr_t>(localCharacter + offsets::character::PrivateModel);
            if (IsPtr(priv)) {
                uintptr_t spreadArgs = mem.Read<uintptr_t>(priv + offsets::private_model::WeaponSpreadArgs);
                uintptr_t klass = IsPtr(spreadArgs) ? mem.Read<uintptr_t>(spreadArgs) : 0;
                if (IsNamedKlass(klass, "WeaponSpreadArgsModel", "Models.Models.WeaponSpreadArgs")) {
                    spreadArgsObject = spreadArgs;
                    spreadArgsKlass = klass;
                }
                uintptr_t spreadPrim = mem.Read<uintptr_t>(priv + offsets::private_model::WeaponSpread);
                if (IsPtr(spreadPrim))
                    weaponSpreadModel = spreadPrim;
            }
        }
        m_recoilObject.store(recoilObject);
        m_recoilKlass.store(recoilKlass);
        m_fakeSpread.store(fakeSpread);
        m_fakeKlass.store(fakeKlass);
        m_spreadArgsObject.store(spreadArgsObject);
        m_spreadArgsKlass.store(spreadArgsKlass);
        m_weaponSpreadModel.store(weaponSpreadModel);
        uintptr_t battleModel = m_battleModel.load();
        uintptr_t shot = 0;
        if (IsPtr(battleModel))
            shot = mem.Read<uintptr_t>(battleModel + offsets::battle_model::ShotInput);
        m_shotInput.store(IsPtr(shot) ? shot : 0);
        uintptr_t fireFlag = 0;
        if (IsPtr(localCharacter)) {
            uintptr_t fire = mem.Read<uintptr_t>(localCharacter + offsets::character::Fire);
            if (IsPtr(fire))
                fireFlag = PrimitiveValueAddress(mem.Read<uintptr_t>(fire + offsets::fire::IsFiring));
        }
        m_fireFlag.store(fireFlag);
        CacheSilentAim(localCharacter);
    } else {
        m_clientPos.store(0);
        m_clientLookQuat.store(0);
        m_clientYaw.store(0);
        m_clientPitch.store(0);
        m_followPos.store(0);
        m_shotInput.store(0);
        m_fireFlag.store(0);
        m_followCamNative.store(0);
        m_recoilObject.store(0);
        m_recoilKlass.store(0);
        m_fakeSpread.store(0);
        m_fakeKlass.store(0);
        m_spreadArgsObject.store(0);
        m_spreadArgsKlass.store(0);
        m_weaponSpreadModel.store(0);
        ClearSilentAim();
    }

    std::unordered_map<int, std::string> names;
    std::unordered_map<int, std::string> clans;
    std::unordered_map<int, int64_t> publicIds;
    std::unordered_map<int64_t, std::string> clanByPublic;
    if (IsPtr(slotsModel)) {
        uintptr_t slotDict = mem.Read<uintptr_t>(slotsModel + offsets::slots::ByEntityId);
        std::vector<DictItem> slots;
        if (ReadIntDictionary(slotDict, slots, 128)) {
            for (const auto& slot : slots) {
                uintptr_t data = mem.Read<uintptr_t>(slot.value + offsets::slot::Data);
                if (!IsPtr(data)) continue;
                uintptr_t nickPrim = mem.Read<uintptr_t>(data + offsets::slot_data::Nickname);
                uintptr_t clanPrim = mem.Read<uintptr_t>(data + offsets::slot_data::ClanTag);
                uintptr_t pubPrim = mem.Read<uintptr_t>(data + offsets::slot_data::PublicId);
                uintptr_t str = 0;
                uintptr_t clanStr = 0;
                if (IsPtr(nickPrim))
                    str = mem.Read<uintptr_t>(nickPrim + offsets::il2cpp::Primitive_Value);
                if (IsPtr(clanPrim))
                    clanStr = mem.Read<uintptr_t>(clanPrim + offsets::il2cpp::Primitive_Value);
                std::string name = ReadIl2CppString(str);
                std::string clan = ReadIl2CppString(clanStr);
                if (!name.empty())
                    names[slot.key] = name;
                if (!clan.empty())
                    clans[slot.key] = clan;
                uintptr_t pubAddr = PrimitiveValueAddress(pubPrim);
                int64_t pub = 0;
                if (pubAddr && mem.ReadOk(pubAddr, pub) && pub != 0) {
                    publicIds[slot.key] = pub;
                    if (!clan.empty())
                        clanByPublic[pub] = clan;
                }
            }
        }
    }

    std::vector<GameEntity> actors;
    std::unordered_set<uintptr_t> seen;

    auto remember = [&](GameEntity entity) {
        if (!entity.address) return;
        entity.generation = m_battleEpoch.load();
        seen.insert(entity.address);
        m_known[entity.address] = entity;
        actors.push_back(std::move(entity));
    };
    auto reuse = [&](uintptr_t address, EntityType type, int key) {
        auto it = m_known.find(address);
        if (it == m_known.end() || it->second.type != type) return false;
        GameEntity entity = it->second;
        entity.generation = m_battleEpoch.load();
        if (key > 0) entity.creationIndex = key;
        if (type == EntityType::Item) {
            bool numeric = !entity.displayName.empty();
            for (unsigned char c : entity.displayName) {
                if (!std::isdigit(c)) { numeric = false; break; }
            }
            if (numeric) return false;
        }
        seen.insert(address);
        actors.push_back(std::move(entity));
        return true;
    };

    auto bindPresence = [&](uintptr_t character, GameEntity& entity) {
        if (!entity.leaveAddr) {
            uintptr_t leave = mem.Read<uintptr_t>(character + offsets::character::KnockdownLeave);
            if (IsPtr(leave))
                entity.leaveAddr = leave + 0x10;
        }
        if (!entity.hiddenAddr) {
            uintptr_t state = mem.Read<uintptr_t>(character + offsets::character::State);
            uintptr_t hidden = IsPtr(state) ? mem.Read<uintptr_t>(state + 0x20) : 0;
            if (IsPtr(hidden))
                entity.hiddenAddr = hidden + offsets::character_hidden::IsHidden;
        }
    };

    auto addCharacter = [&](int key, uintptr_t character, bool requireKlass) {
        if (!IsPtr(character)) return;
        for (const auto& existing : actors) {
            if (existing.address == character) return;
        }
        if (auto it = m_known.find(character); it != m_known.end() && it->second.type == EntityType::Player) {
            GameEntity entity = it->second;
            entity.creationIndex = key > 0 ? key : entity.creationIndex;
            entity.isLocalPlayer = (character == localCharacter) || (key == localId && localId > 0);
            auto nameIt = names.find(entity.creationIndex);
            if (nameIt != names.end())
                entity.displayName = nameIt->second;
            auto clanIt = clans.find(entity.creationIndex);
            if (clanIt != clans.end())
                entity.clan = clanIt->second;
            auto pubIt = publicIds.find(entity.creationIndex);
            if (pubIt != publicIds.end())
                entity.ownerPublicId = pubIt->second;
            entity.role = ReadModelEnumId(mem.Read<uintptr_t>(character + offsets::character::Role));
            uintptr_t knockout = mem.Read<uintptr_t>(character + offsets::character::Knockout);
            if (IsPtr(knockout)) {
                entity.extra = ReadDataEnumId(mem.Read<uintptr_t>(knockout + offsets::knockout::Phase));
                std::string phase = entity.extra;
                for (char& c : phase)
                    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                entity.isKnocked = phase.find("knock") != std::string::npos ||
                                   phase.find("revive") != std::string::npos;
            }
            bindPresence(character, entity);
            if (entity.boneData && entity.boneBytes > 0 && entity.boneBytes <= 4096) {
                entity.boneFloats.resize(static_cast<size_t>(entity.boneBytes / sizeof(float)));
                if (!mem.ReadRaw(entity.boneData, entity.boneFloats.data(),
                                 static_cast<size_t>(entity.boneBytes)))
                    entity.boneFloats.clear();
                else
                    CacheSkeletonPose(entity);
            }
            remember(std::move(entity));
            return;
        }
        if (requireKlass) {
            uintptr_t klass = mem.Read<uintptr_t>(character);
            if (ReadKlassName(klass) != "BattleCharacterModel") return;
        }
        GameEntity entity;
        if (!ResolveCharacter(character, entity)) return;
        entity.creationIndex = key > 0 ? key : entity.creationIndex;
        entity.isLocalPlayer = (character == localCharacter) || (key == localId && localId > 0);
        auto nameIt = names.find(entity.creationIndex);
        if (nameIt != names.end())
            entity.displayName = nameIt->second;
        auto clanIt = clans.find(entity.creationIndex);
        if (clanIt != clans.end())
            entity.clan = clanIt->second;
        auto pubIt = publicIds.find(entity.creationIndex);
        if (pubIt != publicIds.end())
            entity.ownerPublicId = pubIt->second;
        remember(std::move(entity));
    };

    constexpr int kMaxBattlePlayers = 300;
    auto addCharacterDict = [&](uintptr_t dict, bool requireKlass) {
        std::vector<DictItem> characters;
        if (!ReadIntDictionary(dict, characters, kMaxBattlePlayers)) return;
        for (const auto& item : characters)
            addCharacter(item.key, item.value, requireKlass);
    };

    if (IsPtr(world)) {
        addCharacterDict(mem.Read<uintptr_t>(world + offsets::world::BattleCharacters), false);
        addCharacterDict(mem.Read<uintptr_t>(world + offsets::world::NotSpawnedCharacters), false);

        auto addBattleCharactersFromModels = [&](uintptr_t dict) {
            std::vector<DictItem> modelItems;
            if (!ReadIntDictionary(dict, modelItems, 5000)) return;
            for (const auto& item : modelItems) {
                uintptr_t klass = IsPtr(item.value) ? mem.Read<uintptr_t>(item.value) : 0;
                if (MatchesKlassName(klass, "BattleCharacterModel"))
                    addCharacter(item.key, item.value, false);
            }
        };
        addBattleCharactersFromModels(
            mem.Read<uintptr_t>(battle + offsets::battle_entities::Models));
        addBattleCharactersFromModels(
            mem.Read<uintptr_t>(battle + offsets::battle_entities::EntityModels));

        bool haveLocal = false;
        for (const auto& e : actors) {
            if (e.isLocalPlayer) { haveLocal = true; break; }
        }
        if (!haveLocal)
            addCharacter(localId, localCharacter, false);

        std::string localClan;
        int64_t localPublicId = 0;
        for (auto& e : actors) {
            if (e.type != EntityType::Player) continue;
            if (e.isLocalPlayer) {
                localClan = e.clan;
                localPublicId = e.ownerPublicId;
            }
        }
        if (localPublicId == 0 && IsPtr(playerEnt))
            mem.ReadOk(playerEnt + offsets::player_entity::PublicId, localPublicId);
        for (auto& e : actors) {
            if (e.type != EntityType::Player || e.isLocalPlayer) continue;
            if (!localClan.empty() && !e.clan.empty() && Names::Lower(e.clan) == Names::Lower(localClan))
                e.isTeammate = true;
        }

        uintptr_t vehicleDict = mem.Read<uintptr_t>(world + offsets::world::Vehicles);
        std::vector<DictItem> vehicles;
        if (ReadIntDictionary(vehicleDict, vehicles, 128)) {
            for (const auto& item : vehicles) {
                if (reuse(item.value, EntityType::Vehicle, item.key))
                    continue;
                GameEntity entity;
                entity.address = item.value;
                entity.creationIndex = item.key;
                entity.type = EntityType::Vehicle;
                entity.isEnabled = true;
                entity.displayName = ReadEnumId(mem.Read<uintptr_t>(item.value + offsets::vehicle::Type));
                if (entity.displayName.empty())
                    entity.displayName = "Vehicle";
                entity.isAlive = true;
                Vector3 bounds = mem.Read<Vector3>(item.value + offsets::vehicle::Bounds);
                Vector3 inertia = mem.Read<Vector3>(item.value + offsets::vehicle::InertiaSize);
                auto hullSane = [](const Vector3& v) {
                    return v.x > 0.4f && v.x < 4.5f &&
                           v.y > 0.4f && v.y < 3.2f &&
                           v.z > 0.8f && v.z < 8.5f;
                };
                auto volume = [](const Vector3& v) { return v.x * v.y * v.z; };
                if (hullSane(inertia) && (!hullSane(bounds) || volume(inertia) < volume(bounds)))
                    bounds = inertia;
                if (!hullSane(bounds))
                    bounds = { 1.8f, 1.45f, 4.2f };
                Vector3 center = mem.Read<Vector3>(item.value + offsets::vehicle::BoundsCenter);
                float centerY = mem.Read<float>(item.value + offsets::vehicle::CenterFromPivot);
                if (std::isfinite(centerY) && centerY > -0.5f && centerY < bounds.y)
                    center.y = centerY;
                if (!center.IsValid() || center.Length() > 6.f)
                    center = { 0.f, bounds.y * 0.5f, 0.f };
                entity.hasHull = true;
                entity.hullSize = bounds;
                entity.hullCenter = center;
                entity.markerHeight = bounds.y;
                entity.markerAspect = 0.5f;
                entity.rideId = mem.Read<int>(item.value + offsets::vehicle::Id);
                uintptr_t transform = mem.Read<uintptr_t>(item.value + offsets::vehicle::Transform);
                if (!IsPtr(transform)) continue;
                uintptr_t pos = mem.Read<uintptr_t>(transform + offsets::vehicle_transform::Position);
                if (!ResolveVectorPosition(pos, entity)) continue;
                entity.livePos = transform + offsets::vehicle_transform::PivotPosition;
                entity.liveRot = transform + offsets::vehicle_transform::PivotOrientation;
                remember(std::move(entity));
            }
        }

        auto bindPhysics = [&](uintptr_t dict, uintptr_t transformOff) {
            std::vector<DictItem> bodies;
            if (!ReadIntDictionary(dict, bodies, 128)) return;
            for (const auto& item : bodies) {
                uintptr_t transform = mem.Read<uintptr_t>(item.value + transformOff);
                if (!IsPtr(transform)) continue;
                int id = mem.Read<int>(item.value + offsets::car::Id);
                for (auto& e : actors) {
                    if (e.type != EntityType::Vehicle) continue;
                    if (e.rideId != id && e.creationIndex != id && e.creationIndex != item.key)
                        continue;
                    e.livePos = transform + offsets::vehicle_transform::PivotPosition;
                    e.liveRot = transform + offsets::vehicle_transform::PivotOrientation;
                    break;
                }
            }
        };
        bindPhysics(mem.Read<uintptr_t>(world + offsets::world::Cars), offsets::car::Transform);
        bindPhysics(mem.Read<uintptr_t>(world + offsets::world::Helicopters), offsets::helicopter::Transform);

        std::unordered_map<int, std::string> droppedBy;
        bool namesLoaded = false;
        auto loadDroppedNames = [&]() {
            if (namesLoaded) return;
            namesLoaded = true;
            std::vector<DictItem> inventories;
            uintptr_t invDict = mem.Read<uintptr_t>(world + offsets::world::LootInventories);
            if (!ReadIntDictionary(invDict, inventories, 2000)) return;
            for (const auto& inv : inventories) {
                int id = mem.Read<int>(inv.value + offsets::loot_inventory::Id);
                uintptr_t nameModel = mem.Read<uintptr_t>(inv.value + offsets::loot_inventory::Name);
                uintptr_t data = IsPtr(nameModel)
                    ? mem.Read<uintptr_t>(nameModel + offsets::il2cpp::PrimitiveModel_Data) : 0;
                uintptr_t str = IsPtr(data)
                    ? mem.Read<uintptr_t>(data + offsets::il2cpp::Primitive_Value) : 0;
                std::string name = ReadIl2CppString(str);
                if (id > 0 && !name.empty())
                    droppedBy[id] = name;
            }
        };

        auto bagTitle = [&](uintptr_t loot, int id) {
            loadDroppedNames();
            auto who = droppedBy.find(id);
            if (who != droppedBy.end())
                return who->second;
            std::string bag = ReadModelEnumId(mem.Read<uintptr_t>(loot + offsets::loot::Bag));
            bool numeric = !bag.empty();
            for (unsigned char c : bag) {
                if (!std::isdigit(c)) { numeric = false; break; }
            }
            bool cosmetic = bag.find("Bag") != std::string::npos ||
                            bag.find("Type") != std::string::npos ||
                            bag.find("Paint") != std::string::npos;
            if (!bag.empty() && !numeric && !cosmetic)
                return bag;
            std::string role = ReadModelEnumId(mem.Read<uintptr_t>(loot + offsets::loot::Role));
            if (!role.empty())
                return role + " bag";
            return std::string("Bag");
        };

        uintptr_t lootDict = mem.Read<uintptr_t>(world + offsets::world::Loots);
        std::vector<DictItem> loots;
        if (ReadIntDictionary(lootDict, loots, 2000)) {
            for (const auto& item : loots) {
                if (reuse(item.value, EntityType::DeathBag, item.key))
                    continue;
                uintptr_t klass = mem.Read<uintptr_t>(item.value);
                if (!IsPtr(klass)) continue;
                if (!m_lootKlass) {
                    if (ReadKlassName(klass) == "LootModel")
                        m_lootKlass = klass;
                }
                if (klass != m_lootKlass) continue;

                GameEntity entity;
                entity.address = item.value;
                entity.creationIndex = item.key;
                entity.type = EntityType::DeathBag;
                entity.isEnabled = true;
                int lootId = mem.Read<int>(item.value + offsets::loot::Id);
                entity.displayName = bagTitle(item.value, lootId > 0 ? lootId : item.key);
                entity.isAlive = true;
                uintptr_t transform = mem.Read<uintptr_t>(item.value + offsets::loot::Transform);
                if (!ResolveTransform(transform, entity)) continue;
                remember(std::move(entity));
            }
        }

        auto prettyItemName = [](std::string id) {
            return Names::Pretty(std::move(id));
        };
        auto delegateTarget = [&](uintptr_t func) -> uintptr_t {
            if (!IsPtr(func))
                return 0;
            uintptr_t target = 0;
            if (mem.ReadOk(func + offsets::il2cpp_delegate::Target, target) && IsPtr(target))
                return target;
            if (mem.ReadOk(func + 0x10, target) && IsPtr(target))
                return target;
            return 0;
        };
        auto closurePrice = [&](uintptr_t obj) -> int {
            if (!IsPtr(obj))
                return -1;
            for (uintptr_t off : { uintptr_t{0x10}, uintptr_t{0x14}, uintptr_t{0x18}, uintptr_t{0x1C} }) {
                int value = 0;
                if (!mem.ReadOk(obj + off, value))
                    continue;
                if (value >= 1 && value <= 250000)
                    return value;
            }
            return -1;
        };
        auto applyInventory = [&](uintptr_t inv, GameEntity& entity) {
            if (!IsPtr(inv))
                return;
            int rarity = -1;
            if (mem.ReadOk(inv + offsets::inventory_item::Rarity, rarity) &&
                rarity >= 0 && rarity <= 6)
                entity.lootTier = rarity;
            uint8_t type = 0xFF;
            if (mem.ReadOk(inv + offsets::inventory_item::Type, type) && type <= 40)
                entity.itemType = type;
            entity.isWeapon = Names::IsGunType(entity.itemType);
            if (IsPtr(mem.Read<uintptr_t>(inv + offsets::inventory_item::DecoderFunc)))
                entity.decoderCount = 1;
            const int price = closurePrice(delegateTarget(
                mem.Read<uintptr_t>(inv + offsets::inventory_item::BasePriceFunc)));
            if (price > 0)
                entity.cash = price;
            std::string invId = prettyItemName(ReadEnumId(inv));
            if (invId.empty() && entity.itemType >= 0) {
                if (const char* label = Names::ItemTypeLabel(entity.itemType))
                    invId = label;
            }
            if (!invId.empty())
                entity.displayName = invId;
        };
        auto applyLootItem = [&](uintptr_t lootItem, GameEntity& entity) {
            if (!IsPtr(lootItem))
                return;
            std::string id = prettyItemName(ReadEnumId(lootItem));
            uintptr_t inv = delegateTarget(mem.Read<uintptr_t>(lootItem + offsets::loot_item::ItemDescriptionFunc));
            applyInventory(inv, entity);
            if (entity.displayName.empty() && !id.empty())
                entity.displayName = id;
            if (entity.displayName.empty() && entity.itemType >= 0) {
                if (const char* label = Names::ItemTypeLabel(entity.itemType))
                    entity.displayName = label;
            }
        };

        auto addPlacedLoot = [&](uintptr_t dict, uintptr_t descOff, uintptr_t posOff, bool randomItem) {
            std::vector<DictItem> nodes;
            if (!ReadIntDictionary(dict, nodes, 400)) return;
            for (const auto& item : nodes) {
                uintptr_t taken = mem.Read<uintptr_t>(item.value + offsets::random_loot::IsTaken);
                if (IsPtr(taken) && mem.Read<uint8_t>(taken + offsets::il2cpp::Primitive_Value) != 0) {
                    m_known.erase(item.value);
                    continue;
                }
                if (reuse(item.value, EntityType::Item, item.key))
                    continue;
                uintptr_t desc = mem.Read<uintptr_t>(item.value + descOff);
                if (!IsPtr(desc)) continue;
                Vector3 pos = mem.Read<Vector3>(desc + posOff);
                if (!FinitePosition(pos)) continue;

                GameEntity entity;
                entity.address = item.value;
                entity.creationIndex = item.key;
                entity.type = EntityType::Item;
                entity.isEnabled = true;
                entity.isAlive = true;
                entity.position = pos;
                entity.posVec = desc + posOff;
                if (randomItem) {
                    uintptr_t itemData = mem.Read<uintptr_t>(item.value + offsets::random_loot::Item);
                    uintptr_t itemDesc = IsPtr(itemData)
                        ? mem.Read<uintptr_t>(itemData + offsets::il2cpp::Primitive_Value) : 0;
                    applyLootItem(itemDesc, entity);
                } else {
                    uintptr_t itemFunc = mem.Read<uintptr_t>(desc + offsets::static_loot_desc::ItemFunc);
                    applyLootItem(delegateTarget(itemFunc), entity);
                }
                if (entity.displayName.empty())
                    entity.displayName = prettyItemName(ReadEnumId(desc));
                if (entity.displayName.empty() && entity.itemType >= 0) {
                    if (const char* label = Names::ItemTypeLabel(entity.itemType))
                        entity.displayName = label;
                }
                if (entity.displayName.empty() || Names::IsNumericId(entity.displayName))
                    entity.displayName = "Loot";
                remember(std::move(entity));
            }
        };
        addPlacedLoot(mem.Read<uintptr_t>(world + offsets::world::RandomLoots),
                      offsets::random_loot::Description, offsets::random_loot_desc::Position, true);
        addPlacedLoot(mem.Read<uintptr_t>(world + offsets::world::StaticLoots),
                      offsets::static_loot::Description, offsets::static_loot_desc::Position, false);

        uintptr_t grenadeDict = mem.Read<uintptr_t>(world + offsets::world::Grenades);
        std::vector<DictItem> grenades;
        if (ReadIntDictionary(grenadeDict, grenades, 64)) {
            for (const auto& item : grenades) {
                if (reuse(item.value, EntityType::Grenade, item.key))
                    continue;
                uintptr_t transform = mem.Read<uintptr_t>(item.value + offsets::grenade::Transform);
                if (!IsPtr(transform)) continue;
                GameEntity entity;
                entity.address = item.value;
                entity.creationIndex = item.key;
                entity.type = EntityType::Grenade;
                entity.isEnabled = true;
                entity.isAlive = true;
                uintptr_t gdesc = mem.Read<uintptr_t>(item.value + offsets::grenade::Description);
                entity.displayName = prettyItemName(ReadEnumId(gdesc));
                if (entity.displayName.empty())
                    entity.displayName = "Grenade";
                if (IsPtr(gdesc)) {
                    uintptr_t dmg = mem.Read<uintptr_t>(gdesc + offsets::grenade_desc::InstantDamage);
                    uint8_t blind = 0, smoke = 0;
                    mem.ReadOk(gdesc + offsets::grenade_desc::InstantBlind, blind);
                    mem.ReadOk(gdesc + offsets::grenade_desc::Smoke, smoke);
                    float radius = 0.f;
                    mem.ReadOk(gdesc + offsets::grenade_desc::Radius, radius);
                    entity.blastRadius = radius > 0.5f && radius < 40.f ? radius : 0.f;
                    entity.grenadeKind = (IsPtr(dmg) || radius >= 3.f) && !smoke ? 1 : 2;
                    if (blind || smoke)
                        entity.grenadeKind = 2;
                    if (entity.grenadeKind == 1 && entity.blastRadius < 1.f)
                        entity.blastRadius = 8.f;
                }
                entity.posVec = transform + offsets::grenade_transform::Position;
                remember(std::move(entity));
            }
        }

        uintptr_t containerDict = mem.Read<uintptr_t>(world + offsets::world::Containers);
        std::vector<DictItem> containers;
        if (ReadIntDictionary(containerDict, containers, 400)) {
            for (const auto& item : containers) {
                if (reuse(item.value, EntityType::Container, item.key))
                    continue;
                uintptr_t position = mem.Read<uintptr_t>(item.value + offsets::container::Position);
                if (!IsPtr(position)) continue;
                Vector3 probe = mem.Read<Vector3>(position + 0x10);
                if (!FinitePosition(probe)) continue;

                uintptr_t emptyModel = mem.Read<uintptr_t>(item.value + offsets::container::IsEmpty);
                uintptr_t emptyAddr = ModelValueAddress(emptyModel);
                bool empty = emptyAddr && mem.Read<uint8_t>(emptyAddr) != 0;
                if (empty && !Config::Get().esp.showEmptyContainers) {
                    m_known.erase(item.value);
                    continue;
                }

                GameEntity entity;
                entity.address = item.value;
                entity.creationIndex = item.key;
                entity.type = EntityType::Container;
                entity.isEnabled = true;
                entity.isAlive = true;
                uintptr_t locDesc = mem.Read<uintptr_t>(item.value + offsets::container::Description);
                std::string locName = prettyItemName(ReadEnumId(locDesc));
                uintptr_t typeNamePtr = 0;
                uint8_t isAtm = 0;
                if (IsPtr(locDesc)) {
                    uintptr_t contFn = mem.Read<uintptr_t>(locDesc + offsets::location_container::ContainerDescriptionFunc);
                    uintptr_t contDesc = delegateTarget(contFn);
                    if (IsPtr(contDesc)) {
                        uintptr_t typeFn = mem.Read<uintptr_t>(contDesc + offsets::container_desc::TypeFunc);
                        locName = prettyItemName(ReadEnumId(delegateTarget(typeFn)));
                        if (locName.empty())
                            locName = prettyItemName(ReadEnumId(contDesc));
                    }
                    mem.ReadOk(locDesc + offsets::location_container::IsCashWithdrawal, isAtm);
                }
                const std::string lower = Names::Lower(locName);
                if (isAtm || lower.find("atm") != std::string::npos || lower.find("cash") != std::string::npos)
                    entity.type = EntityType::Atm;
                else if (lower.find("vault") != std::string::npos)
                    entity.type = EntityType::Vault;
                else if (lower.find("safe") != std::string::npos)
                    entity.type = EntityType::Safe;
                entity.displayName = locName.empty() ? "Container" : locName;
                entity.hasLoot = !empty;
                if (empty)
                    entity.extra = "empty";
                uintptr_t drill = mem.Read<uintptr_t>(item.value + offsets::container::DrillEndTs);
                uintptr_t drillAddr = ModelValueAddress(drill);
                if (drillAddr) {
                    int64_t endTs = 0;
                    if (mem.ReadOk(drillAddr, endTs) && endTs > 0)
                        entity.drillEndTs = endTs;
                }
                if (Config::Get().esp.showContainerContents) {
                    int invId = 0;
                    if (IsPtr(locDesc))
                        mem.ReadOk(locDesc + offsets::location_container::InventoryEntityId, invId);
                    if (invId > 0) {
                        std::vector<DictItem> invs;
                        if (ReadIntDictionary(mem.Read<uintptr_t>(world + offsets::world::ContainerInventories), invs, 400)) {
                            for (const auto& inv : invs) {
                                int id = mem.Read<int>(inv.value + offsets::container_inventory::Id);
                                if (id != invId && inv.key != invId) continue;
                                uintptr_t inventory = mem.Read<uintptr_t>(inv.value + offsets::container_inventory::Inventory);
                                uintptr_t slots = IsPtr(inventory)
                                    ? mem.Read<uintptr_t>(inventory + offsets::inventory_model::Slots) : 0;
                                uintptr_t dict = IsPtr(slots)
                                    ? mem.Read<uintptr_t>(slots + offsets::inventory_slots::Models) : 0;
                                std::vector<DictItem> slotItems;
                                if (!ReadIntDictionary(dict, slotItems, 48))
                                    break;
                                std::string preview;
                                int n = 0;
                                for (const auto& slot : slotItems) {
                                    uintptr_t itemModel = mem.Read<uintptr_t>(slot.value + offsets::inventory_slot::ItemModel);
                                    uintptr_t data = IsPtr(itemModel)
                                        ? mem.Read<uintptr_t>(itemModel + offsets::inventory_slot_item::Data) : 0;
                                    uintptr_t desc = IsPtr(data)
                                        ? mem.Read<uintptr_t>(data + offsets::il2cpp::Primitive_Value) : 0;
                                    std::string nm = prettyItemName(ReadEnumId(desc));
                                    if (nm.empty()) continue;
                                    if (!preview.empty()) preview += ", ";
                                    preview += nm;
                                    if (++n >= 4) break;
                                }
                                entity.contents = preview;
                                entity.hasLoot = entity.hasLoot || !preview.empty();
                                break;
                            }
                        }
                    }
                }
                entity.posVec = position + 0x10;
                remember(std::move(entity));
            }
        }

        uintptr_t cashDict = mem.Read<uintptr_t>(world + offsets::world::CashLoot);
        std::vector<DictItem> cashNodes;
        if (ReadIntDictionary(cashDict, cashNodes, 256)) {
            for (const auto& item : cashNodes) {
                if (reuse(item.value, EntityType::Item, item.key))
                    continue;
                uintptr_t desc = mem.Read<uintptr_t>(item.value + offsets::cash::Description);
                if (!IsPtr(desc)) continue;
                Vector3 pos = mem.Read<Vector3>(desc + offsets::cash_desc::Position);
                if (!FinitePosition(pos)) continue;
                uintptr_t amount = mem.Read<uintptr_t>(item.value + offsets::cash::Amount);
                uintptr_t amountAddr = PrimitiveValueAddress(amount);
                if (!amountAddr) continue;
                int cashAmt = mem.Read<int>(amountAddr);
                if (cashAmt <= 0) {
                    m_known.erase(item.value);
                    continue;
                }

                GameEntity entity;
                entity.address = item.value;
                entity.creationIndex = item.key;
                entity.type = EntityType::Item;
                entity.isEnabled = true;
                entity.isAlive = true;
                entity.displayName = "Cash";
                entity.cash = cashAmt;
                entity.cashAddr = amountAddr;
                entity.position = pos;
                entity.posVec = desc + offsets::cash_desc::Position;
                remember(std::move(entity));
            }
        }

        auto readList = [&](uintptr_t list, int maxN) {
            std::vector<uintptr_t> out;
            if (!IsPtr(list)) return out;
            uintptr_t items = mem.Read<uintptr_t>(list + offsets::list::Items);
            int size = mem.Read<int>(list + offsets::list::Size);
            if (!IsPtr(items) || size < 1 || size > maxN) return out;
            out.resize(static_cast<size_t>(size), 0);
            mem.ReadRaw(items + offsets::il2cpp::Array_First, out.data(),
                        static_cast<size_t>(size) * sizeof(uintptr_t));
            return out;
        };

        int64_t localPublic = localPublicId;
        if (localPublic == 0 && IsPtr(playerEnt))
            mem.ReadOk(playerEnt + offsets::player_entity::PublicId, localPublic);

        std::unordered_map<int, std::pair<int, int>> defenseByPrivate;
        {
            std::vector<DictItem> privates;
            if (ReadIntDictionary(mem.Read<uintptr_t>(world + offsets::world::DefenseBlockPrivate), privates, 64)) {
                for (const auto& item : privates) {
                    int id = mem.Read<int>(item.value + offsets::defense_private::Id);
                    if (id <= 0) id = item.key;
                    int maxPts = mem.Read<int>(item.value + offsets::defense_private::BaseDefensePoints);
                    int pts = maxPts;
                    uintptr_t ptsAddr = ModelValueAddress(
                        mem.Read<uintptr_t>(item.value + offsets::defense_private::DefensePoints));
                    if (ptsAddr)
                        mem.ReadOk(ptsAddr, pts);
                    defenseByPrivate[id] = { pts, maxPts };
                }
            }
        }

        uintptr_t hideDict = mem.Read<uintptr_t>(world + offsets::world::HideObjects);
        std::vector<DictItem> hides;
        if (ReadIntDictionary(hideDict, hides, 256)) {
            for (const auto& item : hides) {
                uintptr_t desc = mem.Read<uintptr_t>(item.value + offsets::hide_object::Description);
                if (!IsPtr(desc)) continue;
                Vector3 pos = mem.Read<Vector3>(desc + offsets::hide_desc::Position);
                if (!FinitePosition(pos)) continue;
                int occupant = mem.Read<int>(item.value + offsets::hide_object::OccupantId);
                for (auto& e : actors) {
                    if (e.type == EntityType::Player && occupant > 0 &&
                        (e.creationIndex == occupant || mem.Read<int>(e.address + offsets::character::Id) == occupant)) {
                        e.hidingInProp = true;
                        e.isHidden = true;
                        if (!FinitePosition(e.position)) {
                            e.position = pos;
                            e.posVec = desc + offsets::hide_desc::Position;
                        }
                        e.extra = e.extra.empty() ? "Hiding" : e.extra;
                    }
                }
                GameEntity entity;
                entity.address = item.value;
                entity.creationIndex = item.key;
                entity.type = EntityType::HideSpot;
                entity.isEnabled = true;
                entity.isAlive = true;
                entity.displayName = prettyItemName(ReadEnumId(desc));
                if (entity.displayName.empty())
                    entity.displayName = occupant > 0 ? "Occupied hide" : "Hide";
                entity.extra = occupant > 0 ? "occupied" : "";
                entity.hasLoot = occupant > 0;
                entity.position = pos;
                entity.posVec = desc + offsets::hide_desc::Position;
                remember(std::move(entity));
            }
        }

        auto addCargoDict = [&](uintptr_t dict) {
            std::vector<DictItem> nodes;
            if (!ReadIntDictionary(dict, nodes, 256)) return;
            for (const auto& item : nodes) {
                if (reuse(item.value, EntityType::Cargo, item.key))
                    continue;
                uintptr_t desc = mem.Read<uintptr_t>(item.value + offsets::cargo::Description);
                uintptr_t transform = mem.Read<uintptr_t>(item.value + offsets::cargo::Transform);
                GameEntity entity;
                entity.address = item.value;
                entity.creationIndex = item.key;
                entity.type = EntityType::Cargo;
                entity.isEnabled = true;
                entity.isAlive = true;
                entity.displayName = prettyItemName(ReadEnumId(desc));
                if (entity.displayName.empty())
                    entity.displayName = "Cargo";
                if (IsPtr(transform) && ResolveTransform(transform, entity))
                    remember(std::move(entity));
            }
        };
        addCargoDict(mem.Read<uintptr_t>(world + offsets::world::Cargos));
        addCargoDict(mem.Read<uintptr_t>(world + offsets::world::CollectionCargos));
        addCargoDict(mem.Read<uintptr_t>(world + offsets::world::BuildingCargos));

        uintptr_t furnitureDict = mem.Read<uintptr_t>(world + offsets::world::FurnitureLoots);
        std::vector<DictItem> furniture;
        if (ReadIntDictionary(furnitureDict, furniture, 256)) {
            for (const auto& item : furniture) {
                uintptr_t cargo = mem.Read<uintptr_t>(item.value + offsets::furniture_loot::HasCargo);
                uintptr_t cargoAddr = ModelValueAddress(cargo);
                if (!cargoAddr)
                    cargoAddr = PrimitiveValueAddress(cargo);
                uint8_t hasCargo = 1;
                if (cargoAddr)
                    mem.ReadOk(cargoAddr, hasCargo);
                if (hasCargo == 0) {
                    m_known.erase(item.value);
                    continue;
                }
                if (reuse(item.value, EntityType::Item, item.key))
                    continue;
                uintptr_t posModel = mem.Read<uintptr_t>(item.value + offsets::furniture_loot::Position);
                GameEntity entity;
                entity.address = item.value;
                entity.creationIndex = item.key;
                entity.type = EntityType::Item;
                entity.isEnabled = true;
                entity.isAlive = true;
                entity.itemType = 22;
                uintptr_t typeModel = mem.Read<uintptr_t>(item.value + offsets::furniture_loot::FurnitureType);
                entity.displayName = prettyItemName(ReadModelEnumId(typeModel));
                if (entity.displayName.empty())
                    entity.displayName = prettyItemName(ReadModelEnumId(
                        mem.Read<uintptr_t>(item.value + offsets::furniture_loot::FurnitureId)));
                if (entity.displayName.empty())
                    entity.displayName = "Furniture";
                if (IsPtr(posModel) && ResolveVectorPosition(posModel, entity))
                    remember(std::move(entity));
            }
        }

        uintptr_t nodeDict = mem.Read<uintptr_t>(world + offsets::world::LootNodes);
        std::vector<DictItem> nodes;
        if (ReadIntDictionary(nodeDict, nodes, 400)) {
            for (const auto& item : nodes) {
                if (reuse(item.value, EntityType::Item, item.key))
                    continue;
                uintptr_t emptyModel = mem.Read<uintptr_t>(item.value + offsets::loot_node::IsEmpty);
                uintptr_t emptyAddr = ModelValueAddress(emptyModel);
                if (emptyAddr && mem.Read<uint8_t>(emptyAddr) != 0) {
                    m_known.erase(item.value);
                    continue;
                }
                uintptr_t desc = mem.Read<uintptr_t>(item.value + offsets::loot_node::Description);
                if (!IsPtr(desc)) continue;
                Vector3 pos = mem.Read<Vector3>(desc + offsets::loot_node_desc::Position);
                if (!FinitePosition(pos)) continue;
                GameEntity entity;
                entity.address = item.value;
                entity.creationIndex = item.key;
                entity.type = EntityType::Item;
                entity.isEnabled = true;
                entity.isAlive = true;
                entity.displayName = prettyItemName(ReadEnumId(desc));
                uintptr_t lootFn = mem.Read<uintptr_t>(desc + offsets::loot_node_desc::LootFunc);
                applyLootItem(delegateTarget(lootFn), entity);
                if (entity.displayName.empty() || Names::IsNumericId(entity.displayName))
                    entity.displayName = "Loot node";
                entity.position = pos;
                entity.posVec = desc + offsets::loot_node_desc::Position;
                remember(std::move(entity));
            }
        }

        uintptr_t atmDict = mem.Read<uintptr_t>(world + offsets::world::CashWithdrawal);
        std::vector<DictItem> atms;
        if (ReadIntDictionary(atmDict, atms, 64)) {
            for (const auto& item : atms) {
                Vector3 pos = mem.Read<Vector3>(item.value + offsets::cash_withdrawal::InteractPoint);
                if (!FinitePosition(pos)) continue;
                GameEntity entity;
                entity.address = item.value;
                entity.creationIndex = item.key;
                entity.type = EntityType::Atm;
                entity.isEnabled = true;
                entity.isAlive = true;
                entity.displayName = "ATM";
                entity.position = pos;
                entity.posVec = item.value + offsets::cash_withdrawal::InteractPoint;
                remember(std::move(entity));
            }
        }

        uintptr_t plotDict = mem.Read<uintptr_t>(world + offsets::world::Plots);
        std::vector<DictItem> plots;
        if (ReadIntDictionary(plotDict, plots, 64)) {
            for (const auto& item : plots) {
                uintptr_t desc = mem.Read<uintptr_t>(item.value + offsets::plot::Description);
                Vector3 pos{};
                if (IsPtr(desc))
                    pos = mem.Read<Vector3>(desc + offsets::plot_desc::Position);
                if (!FinitePosition(pos)) continue;
                GameEntity entity;
                entity.address = item.value;
                entity.creationIndex = item.key;
                entity.type = EntityType::Base;
                entity.isEnabled = true;
                entity.isAlive = true;
                entity.displayName = prettyItemName(ReadEnumId(
                    mem.Read<uintptr_t>(item.value + offsets::plot::Slot)));
                if (entity.displayName.empty())
                    entity.displayName = "Base";
                uintptr_t data = mem.Read<uintptr_t>(item.value + offsets::plot::Data);
                uintptr_t ownerPrim = IsPtr(data)
                    ? mem.Read<uintptr_t>(data + offsets::plot_data::OwnerPublicId) : 0;
                uintptr_t ownerAddr = PrimitiveValueAddress(ownerPrim);
                if (ownerAddr)
                    mem.ReadOk(ownerAddr, entity.ownerPublicId);
                if (entity.ownerPublicId != 0 && localPublic != 0 &&
                    entity.ownerPublicId == localPublic)
                    entity.isTeammate = true;
                uintptr_t vip = mem.Read<uintptr_t>(item.value + offsets::plot::Vip);
                uintptr_t vipAddr = ModelValueAddress(vip);
                int64_t vipEnd = 0;
                if (vipAddr && mem.ReadOk(vipAddr, vipEnd))
                    entity.vip = vipEnd != 0;
                auto clanHit = clanByPublic.find(entity.ownerPublicId);
                if (clanHit != clanByPublic.end())
                    entity.clan = clanHit->second;
                if (entity.ownerPublicId != 0 && localPublic != 0 &&
                    entity.ownerPublicId == localPublic)
                    entity.isTeammate = true;
                if (!entity.isTeammate && !localClan.empty() && !entity.clan.empty() &&
                    Names::Lower(entity.clan) == Names::Lower(localClan))
                    entity.isClanBase = true;
                entity.hasLoot = IsPtr(mem.Read<uintptr_t>(item.value + offsets::plot::MainBuildingLoot));
                uintptr_t mainB = mem.Read<uintptr_t>(item.value + offsets::plot::MainBuilding);
                if (IsPtr(mainB)) {
                    uintptr_t def = mem.Read<uintptr_t>(mainB + offsets::main_building::DefenseBlock);
                    if (IsPtr(def)) {
                        uintptr_t raidAddr = ModelValueAddress(
                            mem.Read<uintptr_t>(def + offsets::defense_public::RaidLockTs));
                        if (raidAddr)
                            mem.ReadOk(raidAddr, entity.raidLockEndTs);
                        uintptr_t raidedAddr = ModelValueAddress(
                            mem.Read<uintptr_t>(def + offsets::defense_public::IsRaided));
                        uint8_t raided = 0;
                        if (raidedAddr)
                            mem.ReadOk(raidedAddr, raided);
                        entity.beingRaided = raided != 0;
                        const int64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch()).count();
                        entity.raidLocked = entity.raidLockEndTs > nowMs;
                        if (entity.beingRaided || entity.raidLocked)
                            entity.decoderCount = 1;
                        uintptr_t descModel = mem.Read<uintptr_t>(def + offsets::defense_public::Description);
                        uintptr_t locDesc = IsPtr(descModel)
                            ? mem.Read<uintptr_t>(descModel + offsets::description_model::Value) : 0;
                        uintptr_t ids = IsPtr(locDesc)
                            ? mem.Read<uintptr_t>(locDesc + offsets::location_defense::EntityIds) : 0;
                        int privateId = 0;
                        if (IsPtr(ids))
                            mem.ReadOk(ids + offsets::defense_ids::PrivateId, privateId);
                        auto fuse = defenseByPrivate.find(privateId);
                        if (fuse != defenseByPrivate.end()) {
                            entity.defensePoints = fuse->second.first;
                            entity.maxDefensePoints = fuse->second.second;
                        }
                    }
                    std::vector<DictItem> doors;
                    if (ReadIntDictionary(mem.Read<uintptr_t>(mainB + offsets::main_building::Doors), doors, 16)) {
                        for (const auto& door : doors) {
                            uintptr_t doorModel = mem.Read<uintptr_t>(door.value + offsets::builder_door::DoorModel);
                            uintptr_t state = IsPtr(doorModel)
                                ? mem.Read<uintptr_t>(doorModel + offsets::door_model::State) : 0;
                            uintptr_t prim = IsPtr(state)
                                ? mem.Read<uintptr_t>(state + offsets::door_state::Value) : 0;
                            std::string st = ReadDataEnumId(prim);
                            if (Names::Lower(st).find("opened") != std::string::npos)
                                entity.openDoors = true;
                        }
                    }
                    if (!entity.hasLoot) {
                        std::vector<DictItem> stores;
                        if (ReadIntDictionary(mem.Read<uintptr_t>(mainB + offsets::main_building::Storages), stores, 16))
                            entity.hasLoot = !stores.empty();
                    }
                }
                if (entity.maxDefensePoints > 0) {
                    entity.health = static_cast<float>(entity.defensePoints);
                    entity.maxHealth = static_cast<float>(entity.maxDefensePoints);
                }
                entity.position = pos;
                entity.posVec = IsPtr(desc) ? desc + offsets::plot_desc::Position : 0;
                remember(std::move(entity));
            }
        }

        uintptr_t buildingDict = mem.Read<uintptr_t>(world + offsets::world::Buildings);
        std::vector<DictItem> buildings;
        if (ReadIntDictionary(buildingDict, buildings, 128)) {
            for (const auto& item : buildings) {
                uintptr_t cams = mem.Read<uintptr_t>(item.value + offsets::building::Cameras);
                uintptr_t list = IsPtr(cams) ? mem.Read<uintptr_t>(cams + offsets::cameras_model::Models) : 0;
                for (uintptr_t cam : readList(list, 32)) {
                    if (!IsPtr(cam)) continue;
                    uintptr_t desc = mem.Read<uintptr_t>(cam + offsets::camera_model::Description);
                    if (!IsPtr(desc)) continue;
                    Vector3 pos = mem.Read<Vector3>(desc + offsets::location_camera::Position);
                    if (!FinitePosition(pos)) continue;
                    GameEntity entity;
                    entity.address = cam;
                    entity.creationIndex = item.key;
                    entity.type = EntityType::Camera;
                    entity.isEnabled = true;
                    entity.isAlive = true;
                    entity.displayName = prettyItemName(ReadEnumId(desc));
                    if (entity.displayName.empty())
                        entity.displayName = "Camera";
                    uintptr_t broken = mem.Read<uintptr_t>(cam + offsets::camera_model::IsBroken);
                    uintptr_t brokenAddr = PrimitiveValueAddress(broken);
                    uint8_t brokenByte = 0;
                    if (brokenAddr)
                        mem.ReadOk(brokenAddr, brokenByte);
                    entity.cameraBroken = brokenByte != 0;
                    entity.extra = entity.cameraBroken ? "shot out" : "";
                    float qx = 0, qy = 0, qz = 0, qw = 1.f;
                    mem.ReadOk(desc + offsets::location_camera::Orientation, qx);
                    mem.ReadOk(desc + offsets::location_camera::Orientation + 4, qy);
                    mem.ReadOk(desc + offsets::location_camera::Orientation + 8, qz);
                    mem.ReadOk(desc + offsets::location_camera::Orientation + 12, qw);
                    entity.aimForward = QuatForward(qx, qy, qz, qw);
                    entity.position = pos;
                    entity.posVec = desc + offsets::location_camera::Position;
                    remember(std::move(entity));
                }
                uintptr_t safe = mem.Read<uintptr_t>(item.value + offsets::building::SafeDoor);
                if (IsPtr(safe) && !reuse(safe, EntityType::Safe, item.key)) {
                    uintptr_t sdesc = mem.Read<uintptr_t>(safe + offsets::safe_door::Description);
                    GameEntity entity;
                    entity.address = safe;
                    entity.type = EntityType::Safe;
                    entity.isEnabled = true;
                    entity.isAlive = true;
                    entity.displayName = prettyItemName(ReadEnumId(sdesc));
                    if (entity.displayName.empty())
                        entity.displayName = "Safe";
                    uintptr_t endPrim = mem.Read<uintptr_t>(safe + offsets::safe_door::UnlockEndTs);
                    uintptr_t endAddr = PrimitiveValueAddress(endPrim);
                    if (endAddr)
                        mem.ReadOk(endAddr, entity.drillEndTs);
                    uintptr_t bdesc = mem.Read<uintptr_t>(item.value + offsets::building::Description);
                    if (IsPtr(bdesc)) {
                        entity.position = mem.Read<Vector3>(bdesc + offsets::building_desc::Position);
                        entity.posVec = bdesc + offsets::building_desc::Position;
                    }
                    if (FinitePosition(entity.position))
                        remember(std::move(entity));
                }
            }
        }

        uintptr_t rackDict = mem.Read<uintptr_t>(world + offsets::world::Shelvings);
        std::vector<DictItem> racks;
        if (ReadIntDictionary(rackDict, racks, 128)) {
            for (const auto& item : racks) {
                if (reuse(item.value, EntityType::Rack, item.key))
                    continue;
                GameEntity entity;
                entity.address = item.value;
                entity.creationIndex = item.key;
                entity.type = EntityType::Rack;
                entity.isEnabled = true;
                entity.isAlive = true;
                entity.displayName = "Rack";
                uintptr_t descModel = mem.Read<uintptr_t>(item.value + offsets::shelving::Description);
                uintptr_t desc = IsPtr(descModel) ? mem.Read<uintptr_t>(descModel + 0x10) : 0;
                std::string nm = prettyItemName(ReadEnumId(desc));
                if (!nm.empty())
                    entity.displayName = nm;
                if (IsPtr(desc)) {
                    Vector3 pos = mem.Read<Vector3>(desc + 0x30);
                    if (FinitePosition(pos)) {
                        entity.position = pos;
                        entity.posVec = desc + 0x30;
                    }
                }
                if (FinitePosition(entity.position) || entity.posVec)
                    remember(std::move(entity));
            }
        }

        uintptr_t pcDict = mem.Read<uintptr_t>(world + offsets::world::PlotContainers);
        std::vector<DictItem> pcs;
        if (ReadIntDictionary(pcDict, pcs, 128)) {
            for (const auto& item : pcs) {
                uintptr_t emptyModel = mem.Read<uintptr_t>(item.value + offsets::plot_container::IsEmpty);
                uintptr_t emptyAddr = ModelValueAddress(emptyModel);
                bool empty = emptyAddr && mem.Read<uint8_t>(emptyAddr) != 0;
                if (empty && !Config::Get().esp.showEmptyContainers) {
                    m_known.erase(item.value);
                    continue;
                }
                if (reuse(item.value, EntityType::Container, item.key))
                    continue;
                uintptr_t posModel = mem.Read<uintptr_t>(item.value + offsets::plot_container::Position);
                GameEntity entity;
                entity.address = item.value;
                entity.creationIndex = item.key;
                entity.type = EntityType::Container;
                entity.isEnabled = true;
                entity.isAlive = true;
                entity.displayName = prettyItemName(ReadEnumId(
                    mem.Read<uintptr_t>(item.value + offsets::plot_container::Description)));
                if (entity.displayName.empty())
                    entity.displayName = "Plot container";
                entity.hasLoot = !empty;
                if (empty)
                    entity.extra = "empty";
                if (IsPtr(posModel) && ResolveVectorPosition(posModel, entity))
                    remember(std::move(entity));
            }
        }
    }

    for (auto it = m_known.begin(); it != m_known.end(); ) {
        if (!seen.count(it->first))
            it = m_known.erase(it);
        else
            ++it;
    }

    int players = 0, vehicles = 0, items = 0, grenades = 0, containers = 0;
    for (const auto& e : actors) {
        if (e.type == EntityType::Player) ++players;
        else if (e.type == EntityType::Vehicle) ++vehicles;
        else if (e.type == EntityType::Item) ++items;
        else if (e.type == EntityType::Grenade) ++grenades;
        else if (e.type == EntityType::Container) ++containers;
    }

    Config::AutoTuneWorkersForEsp(players, m_inGame.load());

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_actors = actors;
        m_status = "Connected. players=" + std::to_string(players) +
                   " vehicles=" + std::to_string(vehicles) +
                   " loot=" + std::to_string(items) +
                   " grenades=" + std::to_string(grenades) +
                   " containers=" + std::to_string(containers) +
                   " local=" + std::string(IsPtr(localCharacter) ? "yes" : "no");
        static int lastPlayers = -1;
        if (lastPlayers != players) {
            lastPlayers = players;
            if (players == 0) {
                auto countOf = [&](uintptr_t dict) {
                    uintptr_t live = DictionaryObject(dict, 100000);
                    if (!live) return -1;
                    return mem.Read<int>(live + offsets::il2cpp::Dict_Count);
                };
                int battleCount = IsPtr(world)
                    ? countOf(mem.Read<uintptr_t>(world + offsets::world::BattleCharacters)) : -1;
                int waitingCount = IsPtr(world)
                    ? countOf(mem.Read<uintptr_t>(world + offsets::world::NotSpawnedCharacters)) : -1;
                int modelCount = countOf(mem.Read<uintptr_t>(battle + offsets::battle_entities::Models));
                int slotCount = IsPtr(slotsModel)
                    ? countOf(mem.Read<uintptr_t>(slotsModel + offsets::slots::ByEntityId)) : -1;
                std::cout << "[*] Connected, nothing to draw. characters=" << battleCount
                          << " waiting=" << waitingCount
                          << " models=" << modelCount
                          << " slots=" << slotCount
                          << " server=" << (IsPtr(localCharacter) ? "yes" : "no")
                          << " view=" << (m_clientPos.load() ? "yes" : "no")
                          << "\n" << std::flush;
            }
            else
                std::cout << "[+] Overlay live. players=" << players
                          << " vehicles=" << vehicles
                          << " loot=" << items << "\n" << std::flush;
        }
        m_camera.status = m_status;
    }

    AttachRenderedViews(actors);
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_actors = std::move(actors);
    }
}

void RobRunSession::AttachRenderedViews(std::vector<GameEntity>& actors) {
    for (auto& e : actors)
        e.renderedPos = e.renderedHeadPos = 0;

    auto& mem = Memory::Get();
    if (!m_viewUpdaterKlass) {
        const uintptr_t slots[] = {
            offsets::typeinfo::BattleEntitiesUpdater,
            offsets::typeinfo::BattleEntitiesUpdaterDump,
        };
        m_viewUpdaterKlass = ResolveNamedKlass(
            "BattleEntitiesUpdater", "Game.Battle", slots, std::size(slots));
    }
    if (!m_viewUpdaterKlass) return;

    auto usable = [&](uintptr_t updater) {
        if (!IsPtr(updater)) return false;
        if (mem.Read<uintptr_t>(updater) != m_viewUpdaterKlass) return false;
        uintptr_t battle = m_battleEntities.load();
        if (IsPtr(battle)) {
            uintptr_t model = mem.Read<uintptr_t>(
                updater + offsets::battle_entities_view_updater::BattleEntities);
            if (model != battle) return false;
        }
        return IsPtr(mem.Read<uintptr_t>(
            updater + offsets::battle_entities_view_updater::CharacterViews));
    };

    uintptr_t updater = m_viewUpdater.load();
    if (!usable(updater))
        return;

    auto nativePosition = [&](uintptr_t managed) -> uintptr_t {
        if (!IsPtr(managed)) return 0;
        uintptr_t native = mem.Read<uintptr_t>(managed + offsets::unity_transform::Native);
        if (!IsPtr(native)) return 0;
        Vector3 p = mem.Read<Vector3>(native + offsets::unity_transform::WorldPosition);
        if (!FinitePosition(p)) return 0;
        if (std::fabsf(p.x) > 50000.f || std::fabsf(p.y) > 50000.f || std::fabsf(p.z) > 50000.f)
            return 0;
        return native + offsets::unity_transform::WorldPosition;
    };

    std::unordered_map<int, uintptr_t> characters;
    std::unordered_map<int, uintptr_t> characterHeads;
    std::unordered_map<int, uintptr_t> colorModels;
    std::unordered_map<int, uintptr_t> followNatives;
    std::unordered_map<int, uintptr_t> cars;
    std::vector<DictItem> items;
    uintptr_t charDict = mem.Read<uintptr_t>(
        updater + offsets::battle_entities_view_updater::CharacterViews);
    if (ReadIntDictionary(charDict, items, 256)) {
        for (const auto& item : items) {
            if (item.key <= 0 || !IsPtr(item.value)) continue;
            uintptr_t movement = mem.Read<uintptr_t>(
                item.value + offsets::character_entity_view::MovementView);
            uintptr_t managed = IsPtr(movement)
                ? mem.Read<uintptr_t>(movement + offsets::movement_view::Transform) : 0;
            uintptr_t pos = nativePosition(managed);
            if (pos) characters[item.key] = pos;
            uintptr_t headManaged = mem.Read<uintptr_t>(
                item.value + offsets::character_entity_view::HeadPosition);
            uintptr_t headPos = nativePosition(headManaged);
            if (headPos) characterHeads[item.key] = headPos;
            uintptr_t customView = mem.Read<uintptr_t>(
                item.value + offsets::character_entity_view::CustomizationView);
            if (IsPtr(customView)) {
                uintptr_t color = mem.Read<uintptr_t>(
                    customView + offsets::customization_view::ColorModel);
                if (IsPtr(color))
                    colorModels[item.key] = color;
            }
            uintptr_t followManaged = mem.Read<uintptr_t>(
                item.value + offsets::character_entity_view::FollowCamera);
            uintptr_t followNative = nativePosition(followManaged);
            if (followNative)
                followNatives[item.key] = followNative;
        }
    }
    items.clear();
    uintptr_t carDict = mem.Read<uintptr_t>(
        updater + offsets::battle_entities_view_updater::CarViews);
    if (ReadIntDictionary(carDict, items, 128)) {
        for (const auto& item : items) {
            if (!IsPtr(item.value)) continue;
            uintptr_t managed = mem.Read<uintptr_t>(
                item.value + offsets::car_entity_view::Transform);
            uintptr_t pos = nativePosition(managed);
            if (pos) cars[item.key] = pos;
        }
    }

    int linked = 0;
    for (auto& e : actors) {
        const std::unordered_map<int, uintptr_t>* table = nullptr;
        if (e.type == EntityType::Player) table = &characters;
        else if (e.type == EntityType::Vehicle) table = &cars;
        else continue;
        if (e.creationIndex <= 0 && !(e.type == EntityType::Vehicle && e.rideId > 0))
            continue;
        auto it = table->find(e.creationIndex);
        if (it == table->end() && e.type == EntityType::Vehicle && e.rideId > 0)
            it = table->find(e.rideId);
        if (it == table->end()) continue;
        e.renderedPos = it->second;
        if (e.type == EntityType::Player) {
            auto hit = characterHeads.find(e.creationIndex);
            if (hit != characterHeads.end())
                e.renderedHeadPos = hit->second;
            auto color = colorModels.find(e.creationIndex);
            if (color != colorModels.end())
                e.colorModel = color->second;
            auto follow = followNatives.find(e.creationIndex);
            if (follow != followNatives.end())
                e.followCamNative = follow->second;
        }
        ++linked;
    }

    static int lastLogged = -1;
    if (linked != lastLogged) {
        lastLogged = linked;
        std::cout << "[+] Rendered views linked=" << linked
                  << " characters=" << characters.size()
                  << " cars=" << cars.size()
                  << " updater=0x" << std::hex << updater << std::dec
                  << "\n" << std::flush;
    }

    int localViewId = mem.Read<int>(updater + offsets::battle_entities_view_updater::PlayerEntityId);
    auto followLocal = followNatives.find(localViewId);
    if (followLocal != followNatives.end())
        m_followCamNative.store(followLocal->second);
}

void RobRunSession::CachePositions() {
    if (!m_inGame) return;

    std::vector<GameEntity> actors;
    std::string status;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        actors = m_actors;
        status = m_status;
    }
    if (actors.empty()) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_entities.clear();
        m_localPlayer = {};
        m_camera.valid = false;
        m_camera.status = status.empty() ? m_camera.status : status;
        m_camera.generation = m_snapshotGeneration.fetch_add(1, std::memory_order_relaxed) + 1;
        m_camera.captured = std::chrono::steady_clock::now();
        return;
    }

    struct Sample {
        float x = 0, y = 0, z = 0;
        int health = 0;
        int maxHealth = 0;
        uint8_t dead = 0;
        uint8_t leave = 0;
        uint8_t isHidden = 0;
        uint8_t crouch = 0;
        uint8_t sprinting = 0;
        uint8_t armor = 0;
        int armorAmount = 0;
        int vehicleId = -1;
        int cash = 0;
        float px = 0, py = 0, pz = 0;
        float rx = 0, ry = 0, rz = 0;
        float qx = 0, qy = 0, qz = 0, qw = 1.f;
        float lx = 0, ly = 0, lz = 0, yaw = 0, pitch = 0;
        float vx = 0, vy = 0, vz = 0;
        float hx = 0, hy = 0, hz = 0;
    };
    struct ClientSample {
        float position[3] = {};
        float quaternion[4] = { 0.f, 0.f, 0.f, 1.f };
        float yaw = 0, pitch = 0;
    };
    struct BattleCamSample {
        Vector3 position{};
        float orient[4] = { 0.f, 0.f, 0.f, 1.f };
        Vector3 forward{};
        Vector3 right{};
        Vector3 up{};
        float distance = 0.f;
    };
    std::vector<Sample> samples(actors.size());
    ClientSample clientSample;
    BattleCamSample battleCamSample;
    uintptr_t clientPos = m_clientPos.load();
    uintptr_t clientLookQuat = m_clientLookQuat.load();
    uintptr_t clientYawAddr = m_clientYaw.load();
    uintptr_t clientPitchAddr = m_clientPitch.load();
    uintptr_t battleCam = m_battleCamera.load();
    if (!IsPtr(battleCam) && IsPtr(m_battleModel.load())) {
        uintptr_t fromModel = 0;
        if (Memory::Get().ReadOk(m_battleModel.load() + offsets::battle_model::BattleCamera, fromModel) &&
            IsPtr(fromModel)) {
            m_battleCamera.store(fromModel);
            battleCam = fromModel;
        }
    }

    auto& scatter = Memory::Get().BorrowScatter();
    if (scatter.IsValid()) {
        for (size_t i = 0; i < actors.size(); ++i) {
            const auto& e = actors[i];
            auto& s = samples[i];
            if (e.livePos) scatter.Prepare(e.livePos, &s.px, sizeof(float) * 3);
            if (e.repPosAddr) scatter.Prepare(e.repPosAddr, &s.rx, sizeof(float) * 3);
            if (e.liveRot) scatter.Prepare(e.liveRot, &s.qx, sizeof(float) * 4);
            if (e.posVec)
                scatter.Prepare(e.posVec, &s.x, sizeof(float) * 3);
            else {
                if (e.posX) scatter.Prepare(e.posX, &s.x);
                if (e.posY) scatter.Prepare(e.posY, &s.y);
                if (e.posZ) scatter.Prepare(e.posZ, &s.z);
            }
            if (e.healthAddr) scatter.Prepare(e.healthAddr, &s.health);
            if (e.maxHealthAddr) scatter.Prepare(e.maxHealthAddr, &s.maxHealth);
            if (e.deadAddr) scatter.Prepare(e.deadAddr, &s.dead);
            if (e.leaveAddr) scatter.Prepare(e.leaveAddr, &s.leave);
            if (e.hiddenAddr) scatter.Prepare(e.hiddenAddr, &s.isHidden);
            if (e.crouchAddr) scatter.Prepare(e.crouchAddr, &s.crouch);
            if (e.sprintAddr) scatter.Prepare(e.sprintAddr, &s.sprinting);
            if (e.armorAddr) scatter.Prepare(e.armorAddr, &s.armor);
            if (e.armorAmountAddr) scatter.Prepare(e.armorAmountAddr, &s.armorAmount);
            if (e.vehicleIdAddr) scatter.Prepare(e.vehicleIdAddr, &s.vehicleId);
            if (e.cashAddr) scatter.Prepare(e.cashAddr, &s.cash);
            if (e.lookAddr) scatter.Prepare(e.lookAddr, &s.lx, sizeof(float) * 5);
            if (e.renderedPos) scatter.Prepare(e.renderedPos, &s.vx, sizeof(float) * 3);
            if (e.renderedHeadPos) scatter.Prepare(e.renderedHeadPos, &s.hx, sizeof(float) * 3);
            // Bone arrays stay on the actor pass. Pulling them here held the FPGA
            // across the 1ms tick and SNAP jumped to 1000ms+.
        }
        // Camera last, so this frame is drawn with the newest view.
        if (clientPos) scatter.Prepare(clientPos, clientSample.position, sizeof(clientSample.position));
        if (clientLookQuat)
            scatter.Prepare(clientLookQuat, clientSample.quaternion, sizeof(clientSample.quaternion));
        if (clientYawAddr) scatter.Prepare(clientYawAddr, &clientSample.yaw);
        if (clientPitchAddr) scatter.Prepare(clientPitchAddr, &clientSample.pitch);
        if (IsPtr(battleCam)) {
            scatter.Prepare(battleCam + offsets::battle_camera::Position,
                            &battleCamSample.position, sizeof(Vector3));
            scatter.Prepare(battleCam + offsets::battle_camera::Orientation,
                            battleCamSample.orient, sizeof(battleCamSample.orient));
            scatter.Prepare(battleCam + offsets::battle_camera::Forward,
                            &battleCamSample.forward, sizeof(Vector3));
            scatter.Prepare(battleCam + offsets::battle_camera::Right,
                            &battleCamSample.right, sizeof(Vector3));
            scatter.Prepare(battleCam + offsets::battle_camera::Up,
                            &battleCamSample.up, sizeof(Vector3));
            scatter.Prepare(battleCam + offsets::battle_camera::DistanceToFollowTarget,
                            &battleCamSample.distance);
        }
        scatter.Execute();
    }

    GameEntity local;
    bool haveLocal = false;
    for (size_t i = 0; i < actors.size(); ++i) {
        auto& e = actors[i];
        const auto& s = samples[i];
        e.position = { s.x, s.y, s.z };
        Vector3 live{ s.px, s.py, s.pz };
        if (e.livePos && FinitePosition(live) && UnitQuat(s.qx, s.qy, s.qz, s.qw))
            e.position = live;
        Vector3 replicated{ s.rx, s.ry, s.rz };
        if (e.type == EntityType::Player && e.repPosAddr && FinitePosition(replicated)) {
            if (!FinitePosition(e.position))
                e.position = replicated;
            else if (e.position.Distance(replicated) > 25.f)
                e.position = replicated;
        }
        e.aimForward = { s.lx, s.ly, s.lz };
        if (UnitQuat(s.qx, s.qy, s.qz, s.qw)) {
            e.hullQuat[0] = s.qx;
            e.hullQuat[1] = s.qy;
            e.hullQuat[2] = s.qz;
            e.hullQuat[3] = s.qw;
        }
        if (e.cashAddr)
            e.cash = s.cash;
        e.isEnabled = FinitePosition(e.position);
        if (e.type == EntityType::Player) {
            struct Gait {
                Vector3 pos;
                float phase = 0.f;
                bool has = false;
            };
            static std::unordered_map<uintptr_t, Gait> gait;
            auto& step = gait[e.address];
            if (step.has && FinitePosition(e.position)) {
                Vector3 delta = e.position - step.pos;
                e.velocity = delta;
                float moved = std::sqrt(delta.x * delta.x + delta.z * delta.z);
                if (moved < 3.f)
                    step.phase += moved * 5.5f;
            }
            step.pos = e.position;
            step.has = true;
            e.walkPhase = step.phase;
            e.maxHealth = s.maxHealth > 0 ? static_cast<float>(s.maxHealth) : 100.f;
            e.health = static_cast<float>(s.health);
            e.healthNormalized = e.maxHealth > 0.f ? e.health / e.maxHealth : 0.f;
            std::string role = e.role;
            for (char& c : role)
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            const bool hub = role == "hub" ||
                (role.find("hub") != std::string::npos && role.find("hubber") == std::string::npos);
            const bool dead = e.deadAddr != 0 && s.dead != 0;
            if (hub)
                e.isAlive = true;
            else if (dead)
                e.isAlive = false;
            else if (e.healthAddr == 0)
                e.isAlive = true;
            else
                e.isAlive = e.health > 0.f;
            // Only drop actors that are actually leaving the match — not hide-state.
            if (!hub && s.leave != 0 && !e.hidingInProp && !e.isHidden)
                e.isEnabled = false;
            e.isCrouching = s.crouch != 0;
            e.isSprinting = s.sprinting != 0;
            e.hasArmor = s.armor != 0 || s.armorAmount > 0;
            e.armorAmount = static_cast<float>(s.armorAmount);
            if (e.armorAmount > e.maxArmor)
                e.maxArmor = e.armorAmount;
            e.inVehicle = s.vehicleId >= 0;
            e.isHidden = e.isHidden || s.isHidden != 0;
            if (e.hidingInProp || e.isHidden) {
                if (FinitePosition(e.position))
                    e.isEnabled = true;
                if (e.extra.empty())
                    e.extra = "Hiding";
            }
        }
    }

    std::unordered_map<int, size_t> rideOf;
    for (size_t i = 0; i < actors.size(); ++i) {
        const auto& e = actors[i];
        if (e.type != EntityType::Vehicle || !e.isEnabled) continue;
        rideOf[e.creationIndex] = i;
        if (e.rideId >= 0)
            rideOf[e.rideId] = i;
    }
    for (size_t i = 0; i < actors.size(); ++i) {
        auto& e = actors[i];
        if (e.type != EntityType::Player || !e.inVehicle) continue;
        auto ride = rideOf.find(samples[i].vehicleId);
        if (ride == rideOf.end()) {
            // Vehicle not yet in the map. Keep the seated player enabled
            // so they do not disappear while the vehicle entity is late.
            if (FinitePosition(e.position))
                e.isEnabled = true;
            continue;
        }
        const auto& body = actors[ride->second];
        if (!FinitePosition(body.position)) continue;
        // The pivot is the physics origin. A standing box rooted there
        // sticks out through the roof. Sit the figure on the cabin floor.
        Vector3 seat = body.position;
        float sitHeight = 0.95f;
        if (body.hasHull && body.hullSize.y > 0.5f) {
            const auto& sample = samples[ride->second];
            Vector3 center = body.position + body.hullCenter;
            if (UnitQuat(sample.qx, sample.qy, sample.qz, sample.qw))
                center = body.position + QuatRotate(sample.qx, sample.qy, sample.qz, sample.qw, body.hullCenter);
            float floorY = center.y - body.hullSize.y * 0.5f;
            seat = center;
            seat.y = floorY + 0.08f;
            sitHeight = body.hullSize.y * 0.62f;
            if (sitHeight < 0.75f) sitHeight = 0.75f;
            if (sitHeight > 1.05f) sitHeight = 1.05f;
        }
        e.position = seat;
        e.markerHeight = sitHeight;
        e.markerAspect = 0.42f;
        e.isEnabled = true;
        if (UnitQuat(samples[ride->second].qx, samples[ride->second].qy,
                     samples[ride->second].qz, samples[ride->second].qw))
            e.aimForward = QuatForward(samples[ride->second].qx, samples[ride->second].qy,
                                       samples[ride->second].qz, samples[ride->second].qw);
    }

    for (size_t i = 0; i < actors.size(); ++i) {
        auto& e = actors[i];
        if (e.type != EntityType::Player) continue;
        const auto& s = samples[i];
        Vector3 head{ s.hx, s.hy, s.hz };
        if (FinitePosition(head))
            e.viewHead = head;
    }

    for (size_t i = 0; i < actors.size(); ++i) {
        auto& e = actors[i];
        if (!e.renderedPos) continue;
        const auto& s = samples[i];
        Vector3 rendered{ s.vx, s.vy, s.vz };
        if (!FinitePosition(rendered)) continue;
        if (e.type != EntityType::Player && e.type != EntityType::Vehicle)
            continue;
        const bool simOk = FinitePosition(e.position);
        const float gap = simOk ? e.position.Distance(rendered) : 0.f;
        const bool renderedIsHead = e.viewHead.IsValid() &&
            rendered.Distance(e.viewHead) < 0.45f;
        const bool renderedLooksLikeBody = !renderedIsHead &&
            (!e.viewHead.IsValid() || (e.viewHead.y - rendered.y) > 0.45f);
        // Mesh root only. Follow/head transforms sit on the shoulders and
        // make the stick draw on the local pawn instead of the other player.
        if (renderedLooksLikeBody && (!simOk || gap < 120.f))
            e.position = rendered;
    }

    for (size_t i = 0; i < actors.size(); ++i) {
        auto& e = actors[i];
        if (e.type != EntityType::Player) continue;
        if (!e.viewHead.IsValid() || !FinitePosition(e.position)) continue;
        float height = e.viewHead.y - e.position.y;
        if (height > 0.45f && height < 2.4f)
            e.markerHeight = height;
        if (e.inVehicle && height > 0.45f && height < 2.2f) {
            e.markerHeight = height;
            e.markerAspect = 0.42f;
        }
    }

    for (auto& e : actors) {
        if (e.type == EntityType::Player && e.boneCount >= 2 && !e.boneFloats.empty())
            CacheSkeletonPose(e);
    }

    for (const auto& e : actors) {
        if (e.isLocalPlayer && e.isEnabled) {
            local = e;
            haveLocal = true;
            break;
        }
    }

    CameraData camera;
    camera.status = status;
    camera.viewInfo.fov = 60.f;

    Vector3 viewPos{
        clientSample.position[0], clientSample.position[1], clientSample.position[2]
    };
    if (FinitePosition(viewPos) && !haveLocal) {
        local.position = viewPos;
        local.isLocalPlayer = true;
        local.isEnabled = true;
        haveLocal = true;
    }

    // W2S uses the local third-person lens, not the pawn look vector.
    // Game chain: Game+0xD0 -> GameModel+0x10 -> BattleModel+0x358 BattleCameraModel.
    // Position / Forward / Right / Up are the shoulder camera in world space.
    if (FinitePosition(battleCamSample.position)) {
        camera.viewInfo.location = battleCamSample.position;
        if (!ApplyCameraAxes(camera.viewInfo, battleCamSample.forward,
                             battleCamSample.right, battleCamSample.up) &&
            UnitQuat(battleCamSample.orient[0], battleCamSample.orient[1],
                     battleCamSample.orient[2], battleCamSample.orient[3])) {
            SetCameraBasis(camera.viewInfo, battleCamSample.orient[0],
                           battleCamSample.orient[1], battleCamSample.orient[2],
                           battleCamSample.orient[3]);
        }
        camera.valid = camera.viewInfo.location.IsValid() && camera.viewInfo.hasAxes;
    }
    ApplyFreecamCamera(camera);
    WriteMaterialChams(actors);

    {
        Vector3 clientBody{
            clientSample.position[0], clientSample.position[1], clientSample.position[2]
        };
        Vector3 hint = FinitePosition(clientBody) ? clientBody : camera.viewInfo.location;
        if (!haveLocal && FinitePosition(hint)) {
            float best = 4.f;
            GameEntity* pick = nullptr;
            for (auto& e : actors) {
                if (e.type != EntityType::Player)
                    continue;
                if (!FinitePosition(e.position))
                    continue;
                const float d = e.position.Distance(hint);
                if (d < best) {
                    best = d;
                    pick = &e;
                }
            }
            if (pick) {
                pick->isLocalPlayer = true;
                pick->isEnabled = true;
                local = *pick;
                haveLocal = true;
            }
        }
        if (!haveLocal && FinitePosition(clientBody)) {
            local.position = clientBody;
            local.isLocalPlayer = true;
            local.isEnabled = true;
            local.type = EntityType::Player;
            haveLocal = true;
        }
        if (haveLocal) {
            for (auto& e : actors) {
                if (e.type != EntityType::Player || e.isLocalPlayer || !e.isEnabled)
                    continue;
                if (FinitePosition(local.position) && e.position.Distance(local.position) < 1.25f)
                    e.isLocalPlayer = true;
            }
        }
    }

    static bool loggedCam = false;
    if (!loggedCam) {
        loggedCam = true;
        std::cout << "[+] Shoulder camera " << (camera.valid ? "valid" : "waiting")
                  << " fov=" << camera.viewInfo.fov
                  << " dist=" << battleCamSample.distance
                  << " cam=(" << camera.viewInfo.location.x << ","
                  << camera.viewInfo.location.y << ","
                  << camera.viewInfo.location.z << ")\n" << std::flush;
    }

    int enabledPlayers = 0;
    for (const auto& a : actors) {
        if (a.type == EntityType::Player && a.isEnabled && !a.isLocalPlayer)
            ++enabledPlayers;
    }
    Config::AutoTuneWorkersForEsp(enabledPlayers, true);

    actors.erase(std::remove_if(actors.begin(), actors.end(), [](const GameEntity& e) {
        if (e.isLocalPlayer || e.hidingInProp || e.isHidden)
            return false;
        return !e.isEnabled && e.type == EntityType::Player;
    }), actors.end());

    camera.generation = m_snapshotGeneration.fetch_add(1, std::memory_order_relaxed) + 1;
    camera.captured = std::chrono::steady_clock::now();
    const bool espReady = m_inGame.load() && m_battleEntities.load() && camera.valid;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_entities = std::move(actors);
        m_localPlayer = haveLocal ? local : GameEntity{};
        m_camera = std::move(camera);
    }
    if (espReady)
        m_espReady.store(true);
}

void RobRunSession::RefreshCamera(CameraData& camera) {
    if (!m_inGame.load()) return;
    uintptr_t battleCam = m_battleCamera.load();
    if (!IsPtr(battleCam) && IsPtr(m_battleModel.load())) {
        uintptr_t fromModel = 0;
        if (Memory::Get().ReadOk(m_battleModel.load() + offsets::battle_model::BattleCamera, fromModel) &&
            IsPtr(fromModel)) {
            m_battleCamera.store(fromModel);
            battleCam = fromModel;
        }
    }
    if (!IsPtr(battleCam)) {
        camera.valid = false;
        return;
    }

    struct BattleCamSample {
        Vector3 position{};
        float orient[4] = { 0.f, 0.f, 0.f, 1.f };
        Vector3 forward{};
        Vector3 right{};
        Vector3 up{};
    } battleCamSample;

    auto& scatter = Memory::Get().BorrowScatter();
    if (!scatter.IsValid()) return;
    scatter.Prepare(battleCam + offsets::battle_camera::Position,
                    &battleCamSample.position, sizeof(Vector3));
    scatter.Prepare(battleCam + offsets::battle_camera::Orientation,
                    battleCamSample.orient, sizeof(battleCamSample.orient));
    scatter.Prepare(battleCam + offsets::battle_camera::Forward,
                    &battleCamSample.forward, sizeof(Vector3));
    scatter.Prepare(battleCam + offsets::battle_camera::Right,
                    &battleCamSample.right, sizeof(Vector3));
    scatter.Prepare(battleCam + offsets::battle_camera::Up,
                    &battleCamSample.up, sizeof(Vector3));
    if (!scatter.Execute()) return;

    if (!FinitePosition(battleCamSample.position)) {
        camera.valid = false;
        return;
    }
    camera.viewInfo.location = battleCamSample.position;
    camera.viewInfo.fov = 60.f;
    if (!ApplyCameraAxes(camera.viewInfo, battleCamSample.forward,
                         battleCamSample.right, battleCamSample.up) &&
        UnitQuat(battleCamSample.orient[0], battleCamSample.orient[1],
                 battleCamSample.orient[2], battleCamSample.orient[3])) {
        SetCameraBasis(camera.viewInfo, battleCamSample.orient[0],
                       battleCamSample.orient[1], battleCamSample.orient[2],
                       battleCamSample.orient[3]);
    }
    camera.valid = camera.viewInfo.location.IsValid() && camera.viewInfo.hasAxes;
}

void RobRunSession::SetMemoryAimTarget(bool active, const Vector3& eye, const Vector3& targetWorld,
                                     float smooth) {
    std::lock_guard<std::mutex> lock(m_aimMutex);
    m_aimActive = active;
    if (!active)
        return;
    m_aimEye = eye;
    m_aimTarget = targetWorld;
    m_aimSmooth = smooth;
}

void RobRunSession::ServiceMemoryAim() {
    Vector3 eye;
    Vector3 target;
    float smooth = 1.f;
    {
        std::lock_guard<std::mutex> lock(m_aimMutex);
        if (!m_aimActive)
            return;
        eye = m_aimEye;
        target = m_aimTarget;
        smooth = m_aimSmooth;
    }
    if (Config::Get().assist.aim.silentAim)
        WriteSilentAim(eye, target);
    else
        WriteMemoryAim(eye, target, smooth);
}

bool RobRunSession::WriteMemoryAim(const Vector3& eye, const Vector3& targetWorld, float smooth) {
    uintptr_t yawAddr = m_clientYaw.load();
    uintptr_t pitchAddr = m_clientPitch.load();
    uintptr_t quatAddr = m_clientLookQuat.load();
    if (!yawAddr || !pitchAddr)
        return false;
    Vector3 delta = targetWorld - eye;
    float horiz = std::sqrtf(delta.x * delta.x + delta.z * delta.z);
    if (!std::isfinite(horiz) || horiz < 0.05f)
        return false;
    const float kDeg = 180.f / 3.14159265f;
    float targetYaw = std::atan2f(delta.x, delta.z) * kDeg;
    float targetPitch = std::atan2f(-delta.y, horiz) * kDeg;

    auto& mem = Memory::Get();
    float curYaw = mem.Read<float>(yawAddr);
    float curPitch = mem.Read<float>(pitchAddr);
    if (std::fabsf(curYaw) <= 6.5f)
        curYaw *= kDeg;
    if (std::fabsf(curPitch) <= 3.3f)
        curPitch *= kDeg;

    auto normDelta = [](float a) {
        while (a > 180.f) a -= 360.f;
        while (a < -180.f) a += 360.f;
        return a;
    };
    float smoothClamped = smooth;
    if (smoothClamped < 1.f) smoothClamped = 1.f;
    float newYaw = 0.f;
    float newPitch = 0.f;
    if (smoothClamped <= 1.01f) {
        newYaw = targetYaw;
        newPitch = targetPitch;
    } else {
        const float gain = 1.f / smoothClamped;
        newYaw = curYaw + normDelta(targetYaw - curYaw) * gain;
        newPitch = curPitch + (targetPitch - curPitch) * gain;
    }
    newPitch = std::clamp(newPitch, -89.f, 89.f);

    float q[4] = { 0.f, 0.f, 0.f, 1.f };
    const float yRad = newYaw * (3.14159265f / 180.f);
    const float pRad = newPitch * (3.14159265f / 180.f);
    const float cy = std::cosf(yRad * 0.5f);
    const float sy = std::sinf(yRad * 0.5f);
    const float cp = std::cosf(pRad * 0.5f);
    const float sp = std::sinf(pRad * 0.5f);
    q[0] = sp * cy;
    q[1] = -sy * cp;
    q[2] = sp * sy;
    q[3] = cp * cy;
    float qlen = std::sqrtf(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
    if (qlen > 0.01f) {
        for (float& c : q)
            c /= qlen;
    }

    auto scatter = mem.CreateScatterHandle();
    if (!scatter.IsValid())
        return false;
    scatter.PrepareWrite(yawAddr, &newYaw);
    scatter.PrepareWrite(pitchAddr, &newPitch);
    if (quatAddr)
        scatter.PrepareWrite(quatAddr, q, sizeof(q));
    return scatter.ExecuteWrite();
}

void RobRunSession::ClearSilentAim() {
    m_lookDir.store(0);
    m_lookInput.store(0);
    m_lastLook.store(0);
    m_replLook.store(0);
    m_shotInterp.store(0);
    m_shotArgsDir.store(0);
}

void RobRunSession::CacheSilentAim(uintptr_t character) {
    if (!IsPtr(character)) {
        ClearSilentAim();
        return;
    }
    auto& mem = Memory::Get();
    uintptr_t look = mem.Read<uintptr_t>(character + offsets::character::LookDirection);
    uintptr_t lookInput = mem.Read<uintptr_t>(character + offsets::character::LookInput);
    uintptr_t lastLook = mem.Read<uintptr_t>(character + offsets::character::LastLook);
    uintptr_t repl = mem.Read<uintptr_t>(character + offsets::character::ReplicatedLook);
    uintptr_t interp = mem.Read<uintptr_t>(character + offsets::character::ShotInterp);
    uintptr_t args = mem.Read<uintptr_t>(character + offsets::character::ShotArgs);
    m_lookDir.store(IsPtr(look) ? look : 0);
    m_lookInput.store(IsPtr(lookInput) ? lookInput : 0);
    m_lastLook.store(IsPtr(lastLook) ? lastLook : 0);
    uintptr_t replData = 0;
    if (IsPtr(repl))
        replData = mem.Read<uintptr_t>(repl + 0x10);
    m_replLook.store(PrimitiveValueAddress(replData));
    m_shotInterp.store(IsPtr(interp) ? interp : 0);
    uintptr_t dirPrim = 0;
    if (IsPtr(args))
        dirPrim = mem.Read<uintptr_t>(args + offsets::shot_args::Direction);
    m_shotArgsDir.store(PrimitiveValueAddress(dirPrim));
}

bool RobRunSession::WriteSilentAim(const Vector3& eye, const Vector3& targetWorld) {
    Vector3 delta = targetWorld - eye;
    float horiz = std::sqrtf(delta.x * delta.x + delta.z * delta.z);
    if (!std::isfinite(horiz) || horiz < 0.05f)
        return false;
    const float kDeg = 180.f / 3.14159265f;
    float yaw = std::atan2f(delta.x, delta.z) * kDeg;
    float pitch = std::atan2f(-delta.y, horiz) * kDeg;
    pitch = std::clamp(pitch, -89.f, 89.f);
    Vector3 forward, right, up;
    float quat[4]{};
    FreecamBasis(yaw, pitch, forward, right, up, quat);
    if (forward.Length() < 0.5f)
        return false;
    forward = forward.Normalized();

    auto& mem = Memory::Get();
    uintptr_t look = m_lookDir.load();
    uintptr_t lookInput = m_lookInput.load();
    uintptr_t lastLook = m_lastLook.load();
    uintptr_t repl = m_replLook.load();
    uintptr_t interp = m_shotInterp.load();
    uintptr_t argsDir = m_shotArgsDir.load();
    if (!IsPtr(look) && !IsPtr(interp) && !IsPtr(argsDir))
        return false;

    float writeYaw = yaw;
    float writePitch = pitch;
    if (IsPtr(look)) {
        float curYaw = mem.Read<float>(look + offsets::look::Yaw);
        if (std::fabsf(curYaw) <= 6.5f) {
            writeYaw = yaw * (3.14159265f / 180.f);
            writePitch = pitch * (3.14159265f / 180.f);
        }
    }

    auto scatter = mem.CreateScatterHandle();
    if (!scatter.IsValid())
        return false;

    uint8_t contains = 1;
    if (IsPtr(look)) {
        scatter.PrepareWrite(look + offsets::look::Forward, &forward, sizeof(forward));
        scatter.PrepareWrite(look + offsets::look::Yaw, &writeYaw);
        scatter.PrepareWrite(look + offsets::look::Pitch, &writePitch);
    }
    if (IsPtr(lookInput)) {
        scatter.PrepareWrite(lookInput + offsets::look_input::X, &writeYaw);
        scatter.PrepareWrite(lookInput + offsets::look_input::Y, &writePitch);
        scatter.PrepareWrite(lookInput + offsets::look_input::Contains, &contains, sizeof(contains));
    }
    if (IsPtr(lastLook)) {
        scatter.PrepareWrite(lastLook + offsets::last_look::Yaw, &writeYaw);
        scatter.PrepareWrite(lastLook + offsets::last_look::Pitch, &writePitch);
        scatter.PrepareWrite(lastLook + offsets::last_look::Direction, &forward, sizeof(forward));
    }
    if (IsPtr(repl))
        scatter.PrepareWrite(repl, &forward, sizeof(forward));
    if (IsPtr(interp))
        scatter.PrepareWrite(interp + offsets::shot_interp::LookDirection, &forward, sizeof(forward));
    if (IsPtr(argsDir))
        scatter.PrepareWrite(argsDir, &forward, sizeof(forward));
    return scatter.ExecuteWrite();
}

void RobRunSession::ApplyGun() {
    const auto& aim = Config::Get().assist.aim;
    if (!aim.noRecoil && !aim.noSway && !aim.noSpread)
        return;
    auto& mem = Memory::Get();
    auto stillLive = [&](uintptr_t object, uintptr_t klass) {
        if (!IsPtr(object) || (object & 7) || !IsPtr(klass)) return false;
        return mem.Read<uintptr_t>(object) == klass;
    };
    if (aim.noRecoil && stillLive(m_recoilObject.load(), m_recoilKlass.load())) {
        float zero[2] = { 0.f, 0.f };
        mem.WriteRaw(m_recoilObject.load() + offsets::recoil::Offset, zero, sizeof(zero));
    }
    uintptr_t fake = m_fakeSpread.load();
    if (aim.noSway && stillLive(fake, m_fakeKlass.load())) {
        float zeros[3] = { 0.f, 0.f, 0.f };
        mem.WriteRaw(fake + offsets::fake_spread::Spread, zeros, sizeof(zeros));
    }
    if (aim.noSpread) {
        uintptr_t args = m_spreadArgsObject.load();
        if (stillLive(args, m_spreadArgsKlass.load())) {
            uint8_t hasSpread = 0;
            float zero = 0.f;
            mem.WriteRaw(args + offsets::spread_args::HasSpread, &hasSpread, sizeof(hasSpread));
            mem.WriteRaw(args + offsets::spread_args::Spread, &zero, sizeof(zero));
            mem.WriteRaw(args + offsets::spread_args::ShotOffsetX, &zero, sizeof(zero));
            mem.WriteRaw(args + offsets::spread_args::ShotOffsetY, &zero, sizeof(zero));
        }
        uintptr_t spreadPrim = m_weaponSpreadModel.load();
        if (IsPtr(spreadPrim)) {
            float zero = 0.f;
            mem.WriteRaw(spreadPrim + offsets::il2cpp::Primitive_Value, &zero, sizeof(zero));
        }
    }
}

std::vector<GameEntity> RobRunSession::GetEntities() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_entities;
}

GameEntity RobRunSession::GetLocalPlayer() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_localPlayer;
}

CameraData RobRunSession::GetCamera() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_camera;
}

void RobRunSession::CopyRenderState(std::vector<GameEntity>& entities, GameEntity& local, CameraData& camera) {
    std::lock_guard<std::mutex> lock(m_mutex);
    entities = m_entities;
    local = m_localPlayer;
    camera = m_camera;
}

void RobRunSession::FreecamBasis(float yawDeg, float pitchDeg,
                                 Vector3& forward, Vector3& right, Vector3& up, float quat[4]) const {
    const float kDeg = 3.14159265f / 180.f;
    const float y = yawDeg * kDeg;
    const float p = pitchDeg * kDeg;
    const float cy = std::cosf(y);
    const float sy = std::sinf(y);
    const float cp = std::cosf(p);
    const float sp = std::sinf(p);
    forward = { sy * cp, -sp, cy * cp };
    right = { cy, 0.f, -sy };
    up = { sy * sp, cp, cy * sp };
    const float hy = y * 0.5f;
    const float hp = p * 0.5f;
    const float chy = std::cosf(hy);
    const float shy = std::sinf(hy);
    const float chp = std::cosf(hp);
    const float shp = std::sinf(hp);
    quat[0] = shp * chy;
    quat[1] = -shy * chp;
    quat[2] = shp * shy;
    quat[3] = chp * chy;
}

void RobRunSession::ApplyFreecamCamera(CameraData& camera) {
    if (!Config::Get().general.freeCam || !m_freecamOn.load())
        return;
    std::lock_guard<std::mutex> lock(m_freecamMutex);
    Vector3 forward, right, up;
    float quat[4] = { 0.f, 0.f, 0.f, 1.f };
    FreecamBasis(m_freecamYaw, m_freecamPitch, forward, right, up, quat);
    camera.viewInfo.location = m_freecamPos;
    ApplyCameraAxes(camera.viewInfo, forward, right, up);
    camera.valid = camera.viewInfo.location.IsValid() && camera.viewInfo.hasAxes;
}

void RobRunSession::UpdateFreecam(float dt, bool menuOpen) {
    auto& cfg = Config::Get().general;
    if (!cfg.freeCam) {
        m_freecamOn.store(false);
        m_freecamCursorInit = false;
        return;
    }

    CameraData cam;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        cam = m_camera;
    }

    std::lock_guard<std::mutex> lock(m_freecamMutex);
    if (!m_freecamOn.load()) {
        if (cam.valid && FinitePosition(cam.viewInfo.location)) {
            m_freecamPos = cam.viewInfo.location;
            m_freecamYaw = cam.viewInfo.rotation.yaw;
            m_freecamPitch = cam.viewInfo.rotation.pitch;
        }
        m_freecamOn.store(true);
        m_freecamCursorInit = false;
    }

    if (menuOpen) {
        m_freecamCursorInit = false;
        return;
    }

    POINT cur{};
    GetCursorPos(&cur);
    if (!m_freecamCursorInit) {
        m_freecamCursorX = cur.x;
        m_freecamCursorY = cur.y;
        m_freecamCursorInit = true;
    } else {
        m_freecamYaw += static_cast<float>(cur.x - m_freecamCursorX) * 0.12f;
        m_freecamPitch -= static_cast<float>(cur.y - m_freecamCursorY) * 0.12f;
        m_freecamPitch = std::clamp(m_freecamPitch, -89.f, 89.f);
        while (m_freecamYaw > 180.f) m_freecamYaw -= 360.f;
        while (m_freecamYaw < -180.f) m_freecamYaw += 360.f;
        m_freecamCursorX = cur.x;
        m_freecamCursorY = cur.y;
    }

    Vector3 forward, right, up;
    float quat[4]{};
    FreecamBasis(m_freecamYaw, m_freecamPitch, forward, right, up, quat);
    float speed = cfg.freeCamSpeed;
    if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0)
        speed *= 2.6f;
    if ((GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0)
        speed *= 0.35f;
    const float step = speed * (std::max)(dt, 0.001f);
    Vector3 flatFwd{ forward.x, 0.f, forward.z };
    if (flatFwd.Length() > 0.01f)
        flatFwd = flatFwd.Normalized();
    Vector3 flatRight{ right.x, 0.f, right.z };
    if (flatRight.Length() > 0.01f)
        flatRight = flatRight.Normalized();
    if ((GetAsyncKeyState('W') & 0x8000) != 0) m_freecamPos = m_freecamPos + flatFwd * step;
    if ((GetAsyncKeyState('S') & 0x8000) != 0) m_freecamPos = m_freecamPos - flatFwd * step;
    if ((GetAsyncKeyState('A') & 0x8000) != 0) m_freecamPos = m_freecamPos - flatRight * step;
    if ((GetAsyncKeyState('D') & 0x8000) != 0) m_freecamPos = m_freecamPos + flatRight * step;
    if ((GetAsyncKeyState(VK_SPACE) & 0x8000) != 0) m_freecamPos.y += step;
    if ((GetAsyncKeyState(VK_LCONTROL) & 0x8000) != 0) m_freecamPos.y -= step;
}

void RobRunSession::WriteFreecam() {
    if (!Config::Get().general.freeCam || !m_freecamOn.load())
        return;
    Vector3 pos;
    float yaw = 0.f;
    float pitch = 0.f;
    {
        std::lock_guard<std::mutex> lock(m_freecamMutex);
        pos = m_freecamPos;
        yaw = m_freecamYaw;
        pitch = m_freecamPitch;
    }
    if (!FinitePosition(pos))
        return;

    Vector3 forward, right, up;
    float quat[4] = { 0.f, 0.f, 0.f, 1.f };
    FreecamBasis(yaw, pitch, forward, right, up, quat);

    auto& mem = Memory::Get();
    uintptr_t follow = m_followPos.load();
    if (IsPtr(follow))
        mem.WriteRaw(follow, &pos, sizeof(pos));
    uintptr_t followNative = m_followCamNative.load();
    if (IsPtr(followNative))
        mem.WriteRaw(followNative, &pos, sizeof(pos));

    uintptr_t battleCam = m_battleCamera.load();
    if (IsPtr(battleCam)) {
        float dist = 0.05f;
        mem.WriteRaw(battleCam + offsets::battle_camera::Position, &pos, sizeof(pos));
        mem.WriteRaw(battleCam + offsets::battle_camera::Orientation, quat, sizeof(quat));
        mem.WriteRaw(battleCam + offsets::battle_camera::Forward, &forward, sizeof(forward));
        mem.WriteRaw(battleCam + offsets::battle_camera::Right, &right, sizeof(right));
        mem.WriteRaw(battleCam + offsets::battle_camera::Up, &up, sizeof(up));
        mem.WriteRaw(battleCam + offsets::battle_camera::DistanceToFollowTarget, &dist, sizeof(dist));
    }

    uintptr_t yawAddr = m_clientYaw.load();
    uintptr_t pitchAddr = m_clientPitch.load();
    if (IsPtr(yawAddr))
        mem.Write<float>(yawAddr, yaw);
    if (IsPtr(pitchAddr))
        mem.Write<float>(pitchAddr, pitch);
    uintptr_t quatAddr = m_clientLookQuat.load();
    if (IsPtr(quatAddr))
        mem.WriteRaw(quatAddr, quat, sizeof(quat));

    uintptr_t camFollow = m_cameraFollow.load();
    if (IsPtr(camFollow)) {
        float nearDist = 0.05f;
        Vector3 zero{};
        mem.WriteRaw(camFollow + offsets::third_person::CameraDistance, &nearDist, sizeof(nearDist));
        mem.WriteRaw(camFollow + offsets::third_person::ShoulderOffset, &zero, sizeof(zero));
    }
}

void RobRunSession::SetShotInput(bool down) {
    auto& mem = Memory::Get();
    uintptr_t shot = m_shotInput.load();
    if (IsPtr(shot)) {
        uint8_t triggered = down ? 1 : 0;
        int mode = 2; // PressAndHolding
        mem.WriteRaw(shot + offsets::shot_input::Triggered, &triggered, sizeof(triggered));
        if (down)
            mem.WriteRaw(shot + offsets::shot_input::Mode, &mode, sizeof(mode));
    }
    uintptr_t fire = m_fireFlag.load();
    if (IsPtr(fire)) {
        uint8_t firing = down ? 1 : 0;
        mem.WriteRaw(fire, &firing, sizeof(firing));
    }
}

void RobRunSession::WriteMaterialChams(std::vector<GameEntity>& actors) {
    const auto& esp = Config::Get().esp;
    if (!esp.materialChams)
        return;
    static uint32_t lastMs = 0;
    const uint32_t now = GetTickCount();
    if (now - lastMs < 80)
        return;
    lastMs = now;
    auto& mem = Memory::Get();
    for (const auto& e : actors) {
        if (e.type != EntityType::Player || !IsPtr(e.colorModel) || e.isLocalPlayer)
            continue;
        Colour c = Names::RoleColour(e.role, esp);
        if (e.isTeammate && esp.teamEsp)
            c = esp.teamColour;
        float color[4] = { c.r, c.g, c.b, 1.f };
        mem.WriteRaw(e.colorModel + offsets::customization_color::Skin, color, sizeof(color));
        mem.WriteRaw(e.colorModel + offsets::customization_color::Hair, color, sizeof(color));
        mem.WriteRaw(e.colorModel + offsets::customization_color::Beard, color, sizeof(color));
    }
}
