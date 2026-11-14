#include "engine.h"
#include "offsets.h"
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

float Clamp01(float v) {
    if (!(v > 0.f)) return 0.f;
    return v > 1.f ? 1.f : v;
}

float Lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

bool SaneRigDistance(float d) {
    return std::isfinite(d) && d >= 0.2f && d <= 12.f;
}

bool SaneRigOffset(float v) {
    return std::isfinite(v) && std::fabsf(v) <= 3.5f;
}

// Cinemachine puts the lens one arm-length above the shoulder and
// CameraDistance behind the hand. root is the follow target.
bool PlaceRigCamera(const float* raw, bool aiming, const Vector3& root,
                    const Vector3& forward, Vector3& eye,
                    float& outDist, float& outSide) {
    if (!raw || !FinitePosition(root)) return false;
    float ox = raw[0];
    float oy = raw[1];
    float oz = raw[2];
    float arm = raw[3];
    float side = raw[4];
    float dist = raw[5];
    if (!SaneRigOffset(ox) || !SaneRigOffset(oy) || !SaneRigOffset(oz)) return false;
    if (!SaneRigOffset(arm)) arm = 0.f;
    if (!SaneRigDistance(dist)) return false;
    if (!(side >= -0.05f && side <= 1.05f)) side = 1.f;

    auto at = [&](int offset) {
        return raw[(offset - static_cast<int>(offsets::third_person::ShoulderOffset)) / 4];
    };
    if (aiming) {
        float w = Clamp01(at(offsets::third_person::WeightAiming));
        float mode = Clamp01(at(offsets::third_person::WeightAimMode));
        auto blend = [&](float base, float third, float first) {
            return Lerp(Lerp(base, third, w), Lerp(base, first, w), mode);
        };
        float a3x = at(offsets::third_person::ShoulderOffsetAimed3);
        float a3y = at(offsets::third_person::ShoulderOffsetAimed3 + 4);
        float a3z = at(offsets::third_person::ShoulderOffsetAimed3 + 8);
        float a1x = at(offsets::third_person::ShoulderOffsetAimed1);
        float a1y = at(offsets::third_person::ShoulderOffsetAimed1 + 4);
        float a1z = at(offsets::third_person::ShoulderOffsetAimed1 + 8);
        if (SaneRigOffset(a3x) && SaneRigOffset(a3y) && SaneRigOffset(a3z) &&
            SaneRigOffset(a1x) && SaneRigOffset(a1y) && SaneRigOffset(a1z)) {
            ox = blend(ox, a3x, a1x);
            oy = blend(oy, a3y, a1y);
            oz = blend(oz, a3z, a1z);
        }
        float arm3 = at(offsets::third_person::VerticalArmLengthAimed3);
        float arm1 = at(offsets::third_person::VerticalArmLengthAimed1);
        if (SaneRigOffset(arm3) && SaneRigOffset(arm1))
            arm = blend(arm, arm3, arm1);
        float dist3 = at(offsets::third_person::CameraDistanceAimed3);
        float dist1 = at(offsets::third_person::CameraDistanceAimed1);
        if (SaneRigDistance(dist3) && dist1 >= 0.f && dist1 <= 12.f && std::isfinite(dist1))
            dist = blend(dist, dist3, dist1);
        float live = at(offsets::third_person::CameraDistanceLive);
        if (SaneRigDistance(live))
            dist = live;
    }

    float sideClamped = Clamp01(side);
    ox = Lerp(-ox, ox, sideClamped);

    Vector3 flat{ forward.x, 0.f, forward.z };
    float flatLen = flat.Length();
    if (flatLen < 0.2f)
        flat = { 0.f, 0.f, 1.f };
    else
        flat = flat / flatLen;
    Vector3 look = forward.Normalized();
    Vector3 right = Vector3{ 0.f, 1.f, 0.f }.Cross(flat);
    Vector3 hand = root + right * ox + Vector3{ 0.f, 1.f, 0.f } * (oy + arm) + flat * oz;
    eye = hand - look * dist;
    if (!FinitePosition(eye)) return false;
    if (eye.Distance(root) > 15.f) return false;
    if ((root - eye).Dot(look) < 0.15f) return false;
    outDist = dist;
    outSide = sideClamped;
    return true;
}

} // namespace

bool UnityEngine::Initialize(const std::string&) {
    m_inGame = false;
    m_battleEntities = 0;
    m_gameKlass = 0;
    m_gameObject.store(0);
    m_gameSearchRetry = {};
    m_heapSearchRetry = {};
    m_gomRetry = {};
    m_battleEvalFails = 0;
    m_battleKlass = 0;
    m_viewUpdater.store(0);
    m_viewUpdaterKlass = 0;
    m_brainKlass = 0;
    m_updaterKlass = 0;
    m_brainSearched = false;
    m_brainFallbackDone = false;
    m_brainCount.store(0);
    m_brainReject.store(0);
    m_rig.store(0);
    m_rigKind.store(0);
    m_rigKlass = 0;
    m_rigSearched = false;
    m_rigStringTried = false;
    m_battleCamera.store(0);
    m_battleCameraKlass.store(0);
    m_battleModel.store(0);
    m_battleCameraSearched = false;
    m_followPos.store(0);
    m_gameAssembly = 0;
    m_moduleReady.store(false);
    m_espReady.store(false);
    SetStatus("Waiting for GameAssembly.dll");
    std::cout << "[*] ROB RUN engine ready\n";
    return Memory::Get().IsConnected();
}

void UnityEngine::Shutdown() {
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
    m_brainKlass = 0;
    m_updaterKlass = 0;
    m_brainSearched = false;
    m_brainFallbackDone = false;
    m_brainCount.store(0);
    m_brainReject.store(0);
    m_rig.store(0);
    m_rigKind.store(0);
    m_rigKlass = 0;
    m_rigSearched = false;
    m_rigStringTried = false;
    m_battleCamera.store(0);
    m_battleCameraKlass.store(0);
    m_battleModel.store(0);
    m_battleCameraSearched = false;
    m_followPos.store(0);
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

void UnityEngine::SetStatus(const std::string& text) {
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

bool UnityEngine::EnsureModule() {
    if (m_gameAssembly) return true;
    auto& mem = Memory::Get();
    m_gameAssembly = mem.GetModuleBase("GameAssembly.dll");
    m_gameAssemblySize = mem.GetModuleSize("GameAssembly.dll");
    if (!m_gameAssembly) return false;
    m_moduleReady.store(true);
    std::cout << "[+] GameAssembly.dll @ 0x" << std::hex << m_gameAssembly
              << " size 0x" << m_gameAssemblySize << std::dec << "\n";
    return true;
}

std::string UnityEngine::ReadKlassName(uintptr_t klass) const {
    if (!IsPtr(klass)) return {};
    uintptr_t namePtr = Memory::Get().Read<uintptr_t>(klass + offsets::il2cpp::Class_Name);
    if (!IsPtr(namePtr)) return {};
    return Memory::Get().ReadString(namePtr, 80);
}

std::string UnityEngine::ReadEnumId(uintptr_t description) const {
    if (!IsPtr(description)) return {};
    uintptr_t str = Memory::Get().Read<uintptr_t>(description + offsets::identified::Id);
    return ReadIl2CppString(str);
}

std::string UnityEngine::ReadModelEnumId(uintptr_t primitiveModel) const {
    uintptr_t slot = ModelValueAddress(primitiveModel);
    if (!slot) return {};
    return ReadEnumId(Memory::Get().Read<uintptr_t>(slot));
}

std::string UnityEngine::ReadDataEnumId(uintptr_t primitive) const {
    uintptr_t slot = PrimitiveValueAddress(primitive);
    if (!slot) return {};
    return ReadEnumId(Memory::Get().Read<uintptr_t>(slot));
}

std::string UnityEngine::ReadIl2CppString(uintptr_t address) const {
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

uintptr_t UnityEngine::PrimitiveValueAddress(uintptr_t primitive) const {
    if (!IsPtr(primitive)) return 0;
    return primitive + offsets::il2cpp::Primitive_Value;
}

uintptr_t UnityEngine::ModelValueAddress(uintptr_t primitiveModel) const {
    if (!IsPtr(primitiveModel)) return 0;
    uintptr_t data = Memory::Get().Read<uintptr_t>(
        primitiveModel + offsets::il2cpp::PrimitiveModel_Data);
    return PrimitiveValueAddress(data);
}

uintptr_t UnityEngine::DictionaryObject(uintptr_t dict, int maxCount) const {
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

bool UnityEngine::ReadIntDictionary(uintptr_t dict, std::vector<DictItem>& out, int maxCount) const {
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

bool UnityEngine::EvaluateBattleEntities(uintptr_t object, BattleCandidate& result) const {
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

bool UnityEngine::ValidateBattleEntities(uintptr_t object, int& outCount, bool& outHasPlayer) const {
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

bool UnityEngine::MatchesKlassName(uintptr_t klass, const char* name) const {
    if (!IsPtr(klass) || !name)
        return false;
    return ReadKlassName(klass) == name;
}

bool UnityEngine::IsBattleKlass(uintptr_t klass) const {
    return IsNamedKlass(klass, "BattleEntitiesModel", "Game.Battle") ||
           MatchesKlassName(klass, "BattleEntitiesModel");
}

uintptr_t UnityEngine::KlassFromTypeInfoWord(uintptr_t word, const char* name,
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

uintptr_t UnityEngine::FindKlassByName(const char* name) const {
    if (!name || !m_gameAssembly)
        return 0;
    auto& mem = Memory::Get();
    const size_t length = std::strlen(name) + 1;

    auto tryRefs = [&](uintptr_t str) -> uintptr_t {
        if (!IsPtr(str) || mem.ReadString(str, 80) != name)
            return 0;
        auto refs = mem.SearchHeap(&str, sizeof(str), 8, 32, true);
        for (uintptr_t ref : refs) {
            if (ref < offsets::il2cpp::Class_Name)
                continue;
            const uintptr_t cand = ref - offsets::il2cpp::Class_Name;
            if (MatchesKlassName(cand, name))
                return cand;
        }
        return 0;
    };

    const uintptr_t gaEnd = m_gameAssembly + m_gameAssemblySize;
    constexpr uintptr_t kChunk = 0x400000;
    static uintptr_t cursor = 0;
    if (cursor < m_gameAssembly || cursor >= gaEnd)
        cursor = m_gameAssembly;
    const uintptr_t hi = (std::min)(gaEnd, cursor + kChunk);
    auto inModule = mem.SearchRange(name, length, 1, 8, cursor, hi, 12);
    cursor = hi >= gaEnd ? m_gameAssembly : hi;
    for (uintptr_t str : inModule) {
        if (uintptr_t klass = tryRefs(str))
            return klass;
    }
    return 0;
}

uintptr_t UnityEngine::FindNamedKlassNearGame(const char* name, const char* nameSpace) {
    if (!name || !m_gameKlass)
        return 0;
    auto& mem = Memory::Get();
    uintptr_t namePtr = 0;
    if (!mem.ReadOk(m_gameKlass + offsets::il2cpp::Class_Name, namePtr) || !IsPtr(namePtr))
        return 0;
    const size_t length = std::strlen(name) + 1;
    const uintptr_t span = 0x300000;
    const uintptr_t lo = namePtr > span ? namePtr - span : 0x10000;
    const uintptr_t mid = lo + 0x400000;
    auto hunt = [&](uintptr_t a, uintptr_t b) {
        if (b <= a)
            return std::vector<uintptr_t>{};
        return mem.SearchRange(name, length, 1, 8, a, b, 80);
    };
    auto strings = hunt(lo, (std::min)(mid, namePtr + span));
    if (strings.empty() && namePtr + span > mid)
        strings = hunt(mid, namePtr + span);
    for (uintptr_t str : strings) {
        if (mem.ReadString(str, 80) != name)
            continue;
        auto refs = mem.SearchHeap(&str, sizeof(str), 8, 32, true);
        for (uintptr_t ref : refs) {
            if (ref < offsets::il2cpp::Class_Name)
                continue;
            const uintptr_t cand = ref - offsets::il2cpp::Class_Name;
            if (IsNamedKlass(cand, name, nameSpace) || MatchesKlassName(cand, name))
                return cand;
        }
    }
    return 0;
}

bool UnityEngine::IsNamedKlass(
    uintptr_t klass, const char* name, const char* nameSpace) const {
    if (!IsPtr(klass) || !name || !nameSpace) return false;
    if (ReadKlassName(klass) != name) return false;
    uintptr_t ns = Memory::Get().Read<uintptr_t>(klass + offsets::il2cpp::Class_Namespace);
    if (!IsPtr(ns))
        return true;
    return Memory::Get().ReadString(ns, 80) == nameSpace;
}

uintptr_t UnityEngine::ScanTypeInfoRegionForKlass(const char* name, const char* nameSpace) {
    if (!m_gameAssembly || m_gameAssemblySize < 0x8200000)
        return 0;
    auto& mem = Memory::Get();
    auto scanWindow = [&](uintptr_t lo, uintptr_t hi) -> uintptr_t {
        if (lo + 8 > m_gameAssemblySize)
            return 0;
        if (hi > m_gameAssemblySize)
            hi = m_gameAssemblySize;
        for (uintptr_t rva = lo; rva + 8 <= hi; rva += 8) {
            const uintptr_t raw = mem.Read<uintptr_t>(m_gameAssembly + rva);
            const uintptr_t klass = KlassFromTypeInfoWord(raw, name, nameSpace);
            if (!klass)
                continue;
            m_battleTypeInfoRva = rva;
            std::cout << "[+] " << name << " TypeInfo slot rva 0x" << std::hex << rva << std::dec << "\n"
                      << std::flush;
            return klass;
        }
        return 0;
    };

    if (uintptr_t klass = scanWindow(0x08100000, 0x08200000))
        return klass;
    return scanWindow(0x07800000, 0x08600000);
}

uintptr_t UnityEngine::ResolveNamedKlass(
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

uintptr_t UnityEngine::ResolveBattleKlass() {
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

    static bool scannedNear = false;
    if (!scannedNear) {
        scannedNear = true;
        const uintptr_t centers[] = {
            offsets::typeinfo::BattleEntitiesModel,
            offsets::typeinfo::BattleEntitiesModelDump,
        };
        for (uintptr_t center : centers) {
            for (int delta = -0x400; delta <= 0x400; delta += 8) {
                uintptr_t rva = 0;
                if (delta >= 0) {
                    rva = center + static_cast<uintptr_t>(delta);
                } else {
                    const uintptr_t back = static_cast<uintptr_t>(-delta);
                    if (center < back)
                        continue;
                    rva = center - back;
                }
                if (rva + 8 > m_gameAssemblySize)
                    continue;
                uintptr_t word = 0;
                if (!mem.ReadOk(m_gameAssembly + rva, word))
                    continue;
                const uintptr_t klass =
                    KlassFromTypeInfoWord(word, "BattleEntitiesModel", "Game.Battle");
                if (klass)
                    return accept(rva, klass);
            }
        }
    }

    static bool namedTried = false;
    if (!namedTried) {
        namedTried = true;
        ResolveGameKlass();
        if (uintptr_t klass = FindKlassByName("BattleEntitiesModel"))
            return accept(0, klass);
        if (uintptr_t klass = FindNamedKlassNearGame("BattleEntitiesModel", "Game.Battle"))
            return accept(0, klass);
    }

    auto now = std::chrono::steady_clock::now();
    if (m_klassMiss && now - m_klassRetry < std::chrono::milliseconds(400))
        return 0;
    m_klassRetry = now;
    m_klassMiss = true;
    return 0;
}

uintptr_t UnityEngine::ResolveGameKlass() {
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

uintptr_t UnityEngine::BattleEntitiesFromGameModel(uintptr_t gameModel) {
    if (!IsPtr(gameModel))
        return 0;
    auto& mem = Memory::Get();
    uintptr_t modelHeader = 0;
    if (mem.ReadOk(gameModel, modelHeader) &&
        !MatchesKlassName(modelHeader, "GameModel"))
        return 0;
    uintptr_t battleModel = 0;
    if (!mem.ReadOk(gameModel + offsets::game_model::BattleModel, battleModel) || !IsPtr(battleModel))
        return 0;
    uintptr_t battleHeader = 0;
    if (mem.ReadOk(battleModel, battleHeader) &&
        !MatchesKlassName(battleHeader, "BattleModel"))
        return 0;
    m_battleModel.store(battleModel);
    uintptr_t camera = 0;
    if (mem.ReadOk(battleModel + offsets::battle_model::BattleCamera, camera) && IsPtr(camera))
        m_battleCamera.store(camera);
    uintptr_t battleEntities = 0;
    if (!mem.ReadOk(battleModel + offsets::battle_model::BattleEntities, battleEntities) ||
        !IsPtr(battleEntities))
        return 0;
    return battleEntities;
}

uintptr_t UnityEngine::BattleEntitiesFromGame(uintptr_t game) {
    if (!IsPtr(game))
        return 0;
    auto& mem = Memory::Get();
    uintptr_t gameModel = 0;
    if (!mem.ReadOk(game + offsets::game::GameModel, gameModel) || !IsPtr(gameModel))
        return 0;
    return BattleEntitiesFromGameModel(gameModel);
}

bool UnityEngine::LooksLikeGame(uintptr_t object, uintptr_t gameKlass) const {
    if (!IsPtr(object) || !gameKlass)
        return false;
    auto& mem = Memory::Get();
    uintptr_t header = 0;
    if (!mem.ReadOk(object, header) || header != gameKlass)
        return false;
    uintptr_t gameModel = 0;
    if (!mem.ReadOk(object + offsets::game::GameModel, gameModel))
        return false;
    if (!IsPtr(gameModel))
        return false;
    uintptr_t modelHeader = 0;
    if (mem.ReadOk(gameModel, modelHeader) &&
        !MatchesKlassName(modelHeader, "GameModel"))
        return false;
    return true;
}

uintptr_t UnityEngine::FindGameViaStatics(uintptr_t gameKlass) {
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

uintptr_t UnityEngine::FindGameViaHeap(uintptr_t gameKlass) {
    if (!gameKlass)
        return 0;
    auto& mem = Memory::Get();
    if (mem.MotionBurstDepth() > 0)
        return 0;
    const auto hits = mem.SearchHeap(&gameKlass, sizeof(gameKlass), 8, 24, true);
    static int heapLogs = 0;
    if (heapLogs < 12 || (heapLogs % 8) == 0) {
        Memory::Get().Log("[*] heap Game klass hits %zu\n", hits.size());
        std::cout << "[*] heap Game klass hits " << hits.size();
        if (!hits.empty()) {
            uintptr_t header = 0;
            uintptr_t model = 0;
            mem.ReadOk(hits[0], header);
            mem.ReadOk(hits[0] + offsets::game::GameModel, model);
            std::cout << " first 0x" << std::hex << hits[0]
                      << " hdr 0x" << header << " model 0x" << model << std::dec;
        }
        std::cout << "\n" << std::flush;
    }
    ++heapLogs;
    uintptr_t withModel = 0;
    for (uintptr_t hit : hits) {
        if (m_gameAssemblySize && hit >= m_gameAssembly &&
            hit < m_gameAssembly + m_gameAssemblySize)
            continue;
        if (!LooksLikeGame(hit, gameKlass))
            continue;
        uintptr_t gameModel = 0;
        if (mem.ReadOk(hit + offsets::game::GameModel, gameModel) && IsPtr(gameModel)) {
            if (BattleEntitiesFromGameModel(gameModel))
                return hit;
            if (!withModel)
                withModel = hit;
        } else if (!withModel) {
            withModel = hit;
        }
    }
    return withModel;
}

uintptr_t UnityEngine::FindGameViaUnityEntry(uintptr_t gameKlass) {
    if (!gameKlass)
        return 0;
    auto& mem = Memory::Get();
    if (mem.MotionBurstDepth() > 0)
        return 0;
    uintptr_t unityKlass = 0;
    static bool loggedEntry = false;
    for (uintptr_t rva : offsets::typeinfo::UnityEntryAwake) {
        uintptr_t word = 0;
        if (!mem.ReadOk(m_gameAssembly + rva, word))
            continue;
        if (!loggedEntry) {
            mem.Log("[*] UnityEntry slot rva 0x%llx word 0x%llx name=%s\n",
                    static_cast<unsigned long long>(rva),
                    static_cast<unsigned long long>(word),
                    ReadKlassName(word).c_str());
        }
        uintptr_t klass = KlassFromTypeInfoWord(word, "UnityEntry", "Start");
        if (!klass)
            klass = KlassFromTypeInfoWord(word, "UnityEntry", "");
        if (!klass && MatchesKlassName(word, "UnityEntry"))
            klass = word;
        if (!klass)
            continue;
        unityKlass = klass;
        break;
    }
    loggedEntry = true;
    static bool nearTried = false;
    if (!unityKlass && !nearTried) {
        nearTried = true;
        unityKlass = FindNamedKlassNearGame("UnityEntry", "Start");
        if (unityKlass)
            mem.Log("[+] UnityEntry klass via name 0x%llx\n",
                    static_cast<unsigned long long>(unityKlass));
    }
    if (!unityKlass)
        return 0;
    const auto hits = mem.SearchHeap(&unityKlass, sizeof(unityKlass), 8, 8, true);
    for (uintptr_t hit : hits) {
        if (m_gameAssemblySize && hit >= m_gameAssembly &&
            hit < m_gameAssembly + m_gameAssemblySize)
            continue;
        uintptr_t game = 0;
        if (!mem.ReadOk(hit + offsets::unity_entry::Game, game) || !IsPtr(game))
            continue;
        if (LooksLikeGame(game, gameKlass)) {
            std::cout << "[+] Game via UnityEntry @ 0x" << std::hex << hit
                      << " game=0x" << game << std::dec << "\n" << std::flush;
            return game;
        }
    }
    return 0;
}

uintptr_t UnityEngine::FindBattleViaHeap() {
    const auto now = std::chrono::steady_clock::now();
    if (m_heapSearchRetry.time_since_epoch().count() != 0 &&
        now - m_heapSearchRetry < std::chrono::milliseconds(120))
        return 0;
    m_heapSearchRetry = now;

    const uintptr_t klass = ResolveBattleKlass();
    if (!klass)
        return 0;
    auto& mem = Memory::Get();
    if (mem.MotionBurstDepth() > 0)
        return 0;
    const auto hits = mem.SearchHeap(&klass, sizeof(klass), 8, 32, true);
    BattleCandidate best;
    for (uintptr_t hit : hits) {
        if (m_gameAssemblySize && hit >= m_gameAssembly &&
            hit < m_gameAssembly + m_gameAssemblySize)
            continue;
        BattleCandidate candidate;
        if (!EvaluateBattleEntities(hit, candidate))
            continue;
        if (candidate.score > best.score)
            best = candidate;
    }
    return best.address;
}

uintptr_t UnityEngine::FindBattleViaBattleModel() {
    auto& mem = Memory::Get();
    if (mem.MotionBurstDepth() > 0)
        return 0;
    static auto retry = std::chrono::steady_clock::time_point{};
    const auto now = std::chrono::steady_clock::now();
    if (retry.time_since_epoch().count() != 0 &&
        now - retry < std::chrono::milliseconds(180))
        return 0;
    retry = now;
    const uintptr_t slots[] = {
        offsets::typeinfo::BattleModel,
        offsets::typeinfo::BattleModelDump,
    };
    uintptr_t klass = 0;
    for (uintptr_t rva : slots) {
        uintptr_t slotted = 0;
        if (!mem.ReadOk(m_gameAssembly + rva, slotted))
            continue;
        klass = KlassFromTypeInfoWord(slotted, "BattleModel", "Game.Battle");
        if (!klass)
            klass = KlassFromTypeInfoWord(slotted, "BattleModel", "");
        if (klass)
            break;
    }
    static bool logged = false;
    if (!klass)
        klass = FindKlassByName("BattleModel");
    if (!klass) {
        if (!logged) {
            logged = true;
            uintptr_t word = 0;
            mem.ReadOk(m_gameAssembly + offsets::typeinfo::BattleModel, word);
            mem.Log("[*] BattleModel TypeInfo word 0x%llx name=%s\n",
                    static_cast<unsigned long long>(word),
                    ReadKlassName(word).c_str());
        }
        return 0;
    }
    if (!logged) {
        logged = true;
        mem.Log("[+] BattleModel klass @ 0x%llx\n",
                static_cast<unsigned long long>(klass));
    }
    const auto hits = mem.SearchHeap(&klass, sizeof(klass), 8, 16, true);
    for (uintptr_t hit : hits) {
        if (m_gameAssemblySize && hit >= m_gameAssembly &&
            hit < m_gameAssembly + m_gameAssemblySize)
            continue;
        uintptr_t header = 0;
        if (!mem.ReadOk(hit, header) || header != klass)
            continue;
        uintptr_t battleEntities = 0;
        if (!mem.ReadOk(hit + offsets::battle_model::BattleEntities, battleEntities) ||
            !IsPtr(battleEntities))
            continue;
        m_battleModel.store(hit);
        uintptr_t camera = 0;
        if (mem.ReadOk(hit + offsets::battle_model::BattleCamera, camera) && IsPtr(camera))
            m_battleCamera.store(camera);
        return battleEntities;
    }
    return 0;
}

uintptr_t UnityEngine::FindBattleViaGameModel() {
    auto& mem = Memory::Get();
    if (mem.MotionBurstDepth() > 0)
        return 0;
    static auto retry = std::chrono::steady_clock::time_point{};
    const auto now = std::chrono::steady_clock::now();
    if (retry.time_since_epoch().count() != 0 &&
        now - retry < std::chrono::milliseconds(180))
        return 0;
    retry = now;
    const uintptr_t slots[] = {
        offsets::typeinfo::GameModel,
        offsets::typeinfo::GameModelDump,
    };
    uintptr_t klass = 0;
    for (uintptr_t rva : slots) {
        uintptr_t slotted = 0;
        if (!mem.ReadOk(m_gameAssembly + rva, slotted))
            continue;
        klass = KlassFromTypeInfoWord(slotted, "GameModel", "Game");
        if (!klass)
            klass = KlassFromTypeInfoWord(slotted, "GameModel", "");
        if (klass)
            break;
    }
    if (!klass)
        klass = FindKlassByName("GameModel");
    if (!klass)
        return 0;
    static bool logged = false;
    if (!logged) {
        logged = true;
        mem.Log("[+] GameModel klass @ 0x%llx\n",
                static_cast<unsigned long long>(klass));
    }
    const auto hits = mem.SearchHeap(&klass, sizeof(klass), 8, 16, true);
    for (uintptr_t hit : hits) {
        if (m_gameAssemblySize && hit >= m_gameAssembly &&
            hit < m_gameAssembly + m_gameAssemblySize)
            continue;
        uintptr_t header = 0;
        if (!mem.ReadOk(hit, header) || header != klass)
            continue;
        if (const uintptr_t battle = BattleEntitiesFromGameModel(hit))
            return battle;
    }
    return 0;
}

uintptr_t UnityEngine::GameFromNativeGameObject(uintptr_t nativeGo, uintptr_t gameKlass) const {
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

uintptr_t UnityEngine::GameFromNativeGameObjects(const std::vector<uintptr_t>& nativeGos,
                                                 uintptr_t gameKlass) const {
    if (nativeGos.empty() || !gameKlass)
        return 0;
    auto& mem = Memory::Get();
    auto& sc = mem.BorrowScatter();
    sc.SetMaxQueued(768);

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

uintptr_t UnityEngine::FindGameViaGom(uintptr_t gameKlass) {
    if (!gameKlass)
        return 0;
    auto& mem = Memory::Get();
    if (mem.MotionBurstDepth() > 0)
        return 0;
    if (!mem.UnityPlayerBase()) {
        static auto lastTry = std::chrono::steady_clock::time_point{};
        const auto t = std::chrono::steady_clock::now();
        if (lastTry.time_since_epoch().count() != 0 &&
            t - lastTry < std::chrono::milliseconds(400))
            return 0;
        lastTry = t;
        mem.EnsureUnityPlayer();
    }
    const uintptr_t unityPlayer = mem.UnityPlayerBase();
    if (!unityPlayer) {
        SetStatus("Waiting for UnityPlayer.dll");
        return 0;
    }

    const uintptr_t gomSlot = unityPlayer + offsets::unity_player::GameObjectManager;
    uintptr_t gomPtr = 0;
    if (!mem.ReadOk(gomSlot, gomPtr) || !IsPtr(gomPtr)) {
        static bool logged = false;
        if (!logged) {
            logged = true;
            mem.Log("[*] GOM slot 0x%llx empty\n",
                    static_cast<unsigned long long>(gomSlot));
        }
        return 0;
    }

    uint8_t gomRaw[0x28]{};
    if (!mem.ReadRaw(gomPtr, gomRaw, sizeof(gomRaw)))
        return 0;
    uintptr_t buckets = 0;
    uint64_t mask = 0;
    uint64_t maskAlt = 0;
    memcpy(&buckets, gomRaw + offsets::gom::Buckets, sizeof(buckets));
    memcpy(&mask, gomRaw + offsets::gom::Count, sizeof(mask));
    memcpy(&maskAlt, gomRaw + offsets::gom::MaskAlt, sizeof(maskAlt));

    auto hashEntries = [](uint64_t word) -> size_t {
        const uint32_t m = static_cast<uint32_t>(word);
        if (m >= 8 && (m & 7u) == 0) {
            const uint32_t n = m >> 3;
            if (n <= 1023 && (n & (n + 1u)) == 0)
                return static_cast<size_t>(n) + 1;
        }
        if (m != 0 && m <= 8191 && (m & (m + 1u)) == 0)
            return static_cast<size_t>(m) + 1;
        return 0;
    };
    size_t entries = hashEntries(mask);
    if (!entries)
        entries = hashEntries(maskAlt);
    if (entries > 512)
        entries = 512;

    std::vector<uintptr_t> lists;
    lists.reserve(entries ? entries : 8);
    if (IsPtr(buckets) && entries) {
        std::vector<uint8_t> bucketRaw(entries * offsets::gom::EntryStride);
        if (mem.ReadRaw(buckets, bucketRaw.data(), bucketRaw.size())) {
            for (size_t i = 0; i < entries; ++i) {
                const uint8_t* e = bucketRaw.data() + i * offsets::gom::EntryStride;
                int hash = 0;
                uintptr_t list8 = 0;
                uintptr_t list10 = 0;
                memcpy(&hash, e, sizeof(hash));
                memcpy(&list8, e + 8, sizeof(list8));
                memcpy(&list10, e + offsets::gom::EntryList, sizeof(list10));
                if (hash == offsets::gom::EmptyHash || hash == offsets::gom::DeletedHash)
                    continue;
                if (IsPtr(list8))
                    lists.push_back(list8);
                if (IsPtr(list10) && list10 != list8)
                    lists.push_back(list10);
            }
        }
    }
    lists.push_back(gomPtr + offsets::gom::EmbeddedList);

    static bool loggedGom = false;
    if (!loggedGom) {
        loggedGom = true;
        mem.Log("[*] GOM ptr 0x%llx buckets 0x%llx entries %zu lists %zu\n",
                static_cast<unsigned long long>(gomPtr),
                static_cast<unsigned long long>(buckets), entries, lists.size());
    }

    auto& sc = mem.BorrowScatter();
    sc.SetMaxQueued(768);

    struct GomCursor {
        std::vector<uintptr_t> frontier;
        std::unordered_set<uintptr_t> seenNode;
        std::unordered_set<uintptr_t> seenGo;
        int depth = 0;
        bool primed = false;
    };
    GomCursor cursor;
    if (!cursor.primed || cursor.frontier.empty()) {
        cursor = {};
        cursor.primed = true;
        std::vector<uintptr_t> firstNode(lists.size(), 0);
        for (size_t i = 0; i < lists.size(); ++i)
            sc.Prepare(lists[i] + offsets::gom::ListFirst, &firstNode[i]);
        sc.Execute();
        for (uintptr_t node : firstNode) {
            if (IsPtr(node) && cursor.seenNode.insert(node).second)
                cursor.frontier.push_back(node);
        }
    }

    std::vector<uintptr_t> gos;
    for (int hop = 0; hop < 4 && !cursor.frontier.empty() && gos.size() < 48; ++hop) {
        const size_t take = (std::min)(cursor.frontier.size(), size_t{24});
        std::vector<uintptr_t> nodes(cursor.frontier.begin(),
                                     cursor.frontier.begin() + static_cast<std::ptrdiff_t>(take));
        cursor.frontier.erase(cursor.frontier.begin(),
                              cursor.frontier.begin() + static_cast<std::ptrdiff_t>(take));
        std::vector<uintptr_t> nodeNext(nodes.size(), 0);
        std::vector<uintptr_t> nodeGo(nodes.size(), 0);
        for (size_t i = 0; i < nodes.size(); ++i) {
            sc.Prepare(nodes[i] + offsets::gom::NodeNext, &nodeNext[i]);
            sc.Prepare(nodes[i] + offsets::gom::NodeObject, &nodeGo[i]);
        }
        if (!nodes.empty())
            sc.Execute();
        for (size_t i = 0; i < nodes.size(); ++i) {
            if (IsPtr(nodeGo[i]) && cursor.seenGo.insert(nodeGo[i]).second)
                gos.push_back(nodeGo[i]);
            if (IsPtr(nodeNext[i]) && cursor.seenNode.insert(nodeNext[i]).second)
                cursor.frontier.push_back(nodeNext[i]);
        }
    }

    struct Head {
        uintptr_t comps10 = 0;
        uintptr_t comps20 = 0;
        int count = 0;
    };
    const size_t n = (std::min)(gos.size(), size_t{24});
    std::vector<Head> heads(n);
    for (size_t i = 0; i < n; ++i) {
        sc.Prepare(gos[i] + 0x10, &heads[i].comps10);
        sc.Prepare(gos[i] + offsets::game_object::Components, &heads[i].comps20);
        sc.Prepare(gos[i] + offsets::game_object::ComponentCount, &heads[i].count);
    }
    if (n)
        sc.Execute();

    std::vector<uintptr_t> native;
    native.reserve(n * 16);
    int liveCount = 0;
    int withComps = 0;
    for (size_t i = 0; i < n; ++i) {
        int c = heads[i].count;
        if (c < 1 || c > 24)
            continue;
        ++liveCount;
        if (c > 12)
            c = 12;
        uintptr_t comps = IsPtr(heads[i].comps20) ? heads[i].comps20 : heads[i].comps10;
        if (!IsPtr(comps))
            continue;
        ++withComps;
        const size_t base = native.size();
        native.resize(base + static_cast<size_t>(c) * 2, 0);
        for (int k = 0; k < c; ++k) {
            const uintptr_t pair = comps + static_cast<uintptr_t>(k) * offsets::game_object::ComponentStride;
            sc.Prepare(pair, &native[base + static_cast<size_t>(k) * 2]);
            sc.Prepare(pair + 0x08, &native[base + static_cast<size_t>(k) * 2 + 1]);
        }
    }
    if (!native.empty())
        sc.Execute();

    std::vector<uintptr_t> managed;
    for (uintptr_t ptr : native) {
        if (IsPtr(ptr))
            managed.push_back(ptr);
    }
    std::vector<uintptr_t> klass(managed.size(), 0);
    std::vector<uintptr_t> gameField(managed.size(), 0);
    for (size_t i = 0; i < managed.size(); ++i) {
        sc.Prepare(managed[i], &klass[i]);
        sc.Prepare(managed[i] + offsets::unity_entry::Game, &gameField[i]);
        for (uintptr_t off : { uintptr_t{0x10}, uintptr_t{0x18}, uintptr_t{0x28}, uintptr_t{0x30} })
            sc.Prepare(managed[i] + off, &gameField[i]);
    }
    // The loop above overwrites gameField with the last offset. Read the
    // managed slots properly instead.
    (void)gameField;
    if (!managed.empty())
        sc.Execute();

    std::vector<uintptr_t> inner;
    for (size_t i = 0; i < managed.size(); ++i) {
        if (klass[i] == gameKlass) {
            cursor = {};
            return managed[i];
        }
    }
    struct Slot { uintptr_t managed = 0; uintptr_t off = 0; };
    std::vector<Slot> slots;
    for (uintptr_t obj : managed) {
        for (uintptr_t off : { uintptr_t{0x10}, uintptr_t{0x18}, uintptr_t{0x28}, uintptr_t{0x30} })
            slots.push_back({ obj, off });
    }
    std::vector<uintptr_t> slotPtr(slots.size(), 0);
    for (size_t i = 0; i < slots.size(); ++i)
        sc.Prepare(slots[i].managed + slots[i].off, &slotPtr[i]);
    if (!slots.empty())
        sc.Execute();
    std::vector<uintptr_t> maybe;
    std::vector<uintptr_t> maybeKlass;
    for (uintptr_t p : slotPtr) {
        if (!IsPtr(p))
            continue;
        maybe.push_back(p);
    }
    maybeKlass.assign(maybe.size(), 0);
    for (size_t i = 0; i < maybe.size(); ++i)
        sc.Prepare(maybe[i], &maybeKlass[i]);
    if (!maybe.empty())
        sc.Execute();
    for (size_t i = 0; i < maybe.size(); ++i) {
        if (maybeKlass[i] == gameKlass) {
            cursor = {};
            return maybe[i];
        }
    }

    char detail[192];
    snprintf(detail, sizeof(detail),
             "UnityPlayer+0x%llx  objects %zu  with components %d",
             static_cast<unsigned long long>(offsets::unity_player::GameObjectManager),
             n, withComps);
    SetStatus("In menu");
    Splash::SetDetail(detail);
    static int gomNoGame = 0;
    if (gomNoGame < 6) {
        ++gomNoGame;
        mem.Log("[*] %s\n", detail);
    }
    return 0;
}

uintptr_t UnityEngine::FindGameObject() {
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

    const auto now = std::chrono::steady_clock::now();
    if (m_gameSearchRetry.time_since_epoch().count() != 0 &&
        now - m_gameSearchRetry < std::chrono::milliseconds(80))
        return cached;
    m_gameSearchRetry = now;

    // After Game is known, players, loot, and the match camera are direct reads:
    //   Game+0xD0 → GameModel+0x10 → BattleModel+0x90 / +0x358.
    // No heap walk. The Game instance is the object on UnityEntry+0x28,
    // reached from the fixed UnityPlayer GameObjectManager slot.
    if (const uintptr_t viaStatics = FindGameViaStatics(klass)) {
        m_gameObject.store(viaStatics);
        return viaStatics;
    }
    const bool gomDue = m_gomRetry.time_since_epoch().count() == 0 ||
                        now - m_gomRetry >= std::chrono::milliseconds(200);
    if (!gomDue)
        return cached;
    m_gomRetry = now;
    const uintptr_t viaGom = FindGameViaGom(klass);
    if (!viaGom)
        return cached;
    m_gameObject.store(viaGom);
    if (viaGom != cached) {
        std::cout << "[+] Game @ 0x" << std::hex << viaGom << std::dec
                  << " via UnityPlayer GOM\n" << std::flush;
        Memory::Get().Log("[+] Game @ 0x%llx via UnityPlayer GOM\n",
                          static_cast<unsigned long long>(viaGom));
    }
    return viaGom;
}

bool UnityEngine::CommitBattleWorld(const BattleCandidate& best, const char* source) {
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
    Splash::SetDetail("BattleEntities is live. ESP draws when the camera pose is valid.");
    return true;
}

void UnityEngine::SyncLinkStatusFromModules() {
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

bool UnityEngine::FindBattleEntities() {
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

bool UnityEngine::ResolveVectorPosition(uintptr_t primitive, GameEntity& entity) const {
    uintptr_t value = PrimitiveValueAddress(primitive);
    if (!value) return false;
    entity.posVec = value;
    entity.posX = entity.posY = entity.posZ = 0;
    return true;
}

bool UnityEngine::ResolveTransform(uintptr_t transform, GameEntity& entity) const {
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

void UnityEngine::ReadSkeleton(uintptr_t character, GameEntity& entity) const {
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
        std::ofstream arch(R"(C:\Users\baile\OneDrive\Desktop\Development\.cursor\skeleton-reference.log)",
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

bool UnityEngine::ResolveCharacter(uintptr_t character, GameEntity& entity) const {
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
    }

    uintptr_t crouch = mem.Read<uintptr_t>(character + offsets::character::Crouch);
    if (IsPtr(crouch)) {
        uintptr_t flag = mem.Read<uintptr_t>(crouch + offsets::crouch::IsCrouching);
        entity.crouchAddr = PrimitiveValueAddress(flag);
    }
    entity.sprintAddr = ModelValueAddress(mem.Read<uintptr_t>(character + offsets::character::IsSprinting));
    entity.armorAddr = ModelValueAddress(mem.Read<uintptr_t>(character + offsets::character::HasArmor));
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

void UnityEngine::FindCameraBrain() {
    if (m_brainCount.load() > 0 && m_brainReject.load() == 0) return;
    auto now = std::chrono::steady_clock::now();
    if (m_brainSearched && now - m_brainRetry < std::chrono::seconds(5))
        return;
    m_brainSearched = true;
    m_brainRetry = now;
    m_brainReject.store(0);
    for (auto& brain : m_brains)
        brain.store(0);
    m_brainCount.store(0);

    auto& mem = Memory::Get();
    if (!m_brainKlass) {
        uintptr_t klass = mem.Read<uintptr_t>(m_gameAssembly + offsets::typeinfo::CinemachineBrain);
        if (IsNamedKlass(klass, "CinemachineBrain", "Cinemachine"))
            m_brainKlass = klass;
    }
    if (!m_brainKlass) {
        static bool logged = false;
        if (!logged) {
            logged = true;
            uintptr_t klass = mem.Read<uintptr_t>(m_gameAssembly + offsets::typeinfo::CinemachineBrain);
            std::cout << "[-] CinemachineBrain slot 0x" << std::hex << klass << std::dec
                      << " name=" << ReadKlassName(klass) << "\n" << std::flush;
        }
        return;
    }
    if (!m_updaterKlass) {
        uintptr_t klass = mem.Read<uintptr_t>(m_gameAssembly + offsets::typeinfo::CameraBrainUpdater);
        if (IsNamedKlass(klass, "CameraBrainUpdater", "Game.Battle.UiUpdater"))
            m_updaterKlass = klass;
        else {
            static bool logged = false;
            if (!logged) {
                logged = true;
                std::cout << "[-] CameraBrainUpdater slot 0x" << std::hex << klass << std::dec
                          << " name=" << ReadKlassName(klass) << "\n" << std::flush;
            }
        }
    }

    auto inModule = [&](uintptr_t hit) {
        return m_gameAssemblySize && hit >= m_gameAssembly &&
               hit < m_gameAssembly + m_gameAssemblySize;
    };
    int found = 0;
    auto consider = [&](uintptr_t brain) {
        if (found >= 8 || !IsPtr(brain) || inModule(brain)) return;
        if (mem.Read<uintptr_t>(brain) != m_brainKlass) return;
        const int updateMethod = mem.Read<int>(brain + offsets::cinemachine_brain::UpdateMethod);
        if (updateMethod < 0 || updateMethod > 8) return;
        if (!IsPtr(mem.Read<uintptr_t>(brain + offsets::cinemachine_brain::CachedPtr))) return;
        float fov = mem.Read<float>(brain + offsets::cinemachine_brain::FieldOfView);
        int frame = mem.Read<int>(brain + offsets::cinemachine_brain::LastFrame);
        bool fovOk = fov >= 20.f && fov <= 150.f;
        if (!fovOk && frame <= 0) return;
        for (int i = 0; i < found; ++i) {
            if (m_brains[i].load() == brain) return;
        }
        m_brains[found].store(brain);
        ++found;
    };

    const char* how = "none";
    if (m_updaterKlass) {
        auto hits = mem.SearchHeap(&m_updaterKlass, sizeof(m_updaterKlass), 8, 32);
        for (uintptr_t hit : hits) {
            if (inModule(hit)) continue;
            if (mem.Read<uintptr_t>(hit) != m_updaterKlass) continue;
            consider(mem.Read<uintptr_t>(hit + offsets::camera_brain_updater::CameraBrain));
        }
        how = "updater";
    }
    if (found == 0 && !m_brainFallbackDone) {
        m_brainFallbackDone = true;
        auto hits = mem.SearchHeap(&m_brainKlass, sizeof(m_brainKlass), 8, 64);
        int checked = 0;
        for (uintptr_t hit : hits) {
            if (inModule(hit)) continue;
            if (mem.Read<uintptr_t>(hit) != m_brainKlass) continue;
            ++checked;
            consider(hit);
            if (found >= 4) break;
        }
        how = "class";
        if (found == 0) {
            std::cout << "[-] No live CinemachineBrain in " << checked
                      << " class hits\n" << std::flush;
        }
    }

    m_brainCount.store(found);
    if (found > 0) {
        std::cout << "[+] Cinemachine brains " << found << " via " << how
                  << "\n" << std::flush;
    }
}

void UnityEngine::FindCameraRig() {
    if (m_rig.load()) return;
    auto now = std::chrono::steady_clock::now();
    if (m_rigSearched && now - m_rigRetry < std::chrono::seconds(5))
        return;
    m_rigSearched = true;
    m_rigRetry = now;

    auto& mem = Memory::Get();
    if (!m_rigKlass) {
        uintptr_t klass = mem.Read<uintptr_t>(
            m_gameAssembly + offsets::typeinfo::Cinemachine3rdPersonFollow);
        if (IsNamedKlass(klass, "Cinemachine3rdPersonFollow", "Cinemachine")) {
            m_rigKlass = klass;
            m_rigKind.store(1);
        } else {
            static bool logged = false;
            if (!logged) {
                logged = true;
                std::cout << "[-] Cinemachine3rdPersonFollow slot 0x" << std::hex << klass
                          << std::dec << " name=" << ReadKlassName(klass) << "\n" << std::flush;
            }
            return;
        }
    }

    bool aiming = m_rigKind.load() == 2;
    auto hits = mem.SearchHeap(&m_rigKlass, sizeof(m_rigKlass), 8, 64);
    uintptr_t fallback = 0;
    uintptr_t live = 0;
    for (uintptr_t hit : hits) {
        if (m_gameAssemblySize && hit >= m_gameAssembly &&
            hit < m_gameAssembly + m_gameAssemblySize)
            continue;
        if (mem.Read<uintptr_t>(hit) != m_rigKlass) continue;
        if (!IsPtr(mem.Read<uintptr_t>(hit + offsets::cinemachine_brain::CachedPtr))) continue;
        float ox = mem.Read<float>(hit + offsets::third_person::ShoulderOffset);
        float dist = mem.Read<float>(hit + offsets::third_person::CameraDistance);
        float side = mem.Read<float>(hit + offsets::third_person::CameraSide);
        if (!SaneRigOffset(ox) || !SaneRigDistance(dist)) continue;
        if (!(side >= -0.05f && side <= 1.05f)) continue;
        if (!fallback) fallback = hit;
        if (!aiming) {
            live = hit;
            break;
        }
        float used = mem.Read<float>(hit + offsets::third_person::CameraDistanceLive);
        if (SaneRigDistance(used)) {
            live = hit;
            break;
        }
    }
    uintptr_t chosen = live ? live : fallback;
    if (!chosen) return;
    if (live)
        m_rigKind.store(aiming ? 2 : 1);
    m_rig.store(chosen);
    float dist = mem.Read<float>(chosen + (aiming && live
        ? offsets::third_person::CameraDistanceLive
        : offsets::third_person::CameraDistance));
    float side = mem.Read<float>(chosen + offsets::third_person::CameraSide);
    float ox = mem.Read<float>(chosen + offsets::third_person::ShoulderOffset);
    std::cout << "[+] Camera rig 0x" << std::hex << chosen << std::dec
              << (aiming ? " aiming" : " third")
              << " dist=" << dist << " side=" << side
              << " shoulder=" << ox << "\n" << std::flush;
}

void UnityEngine::FindViewUpdater() {
    if (m_viewUpdater.load())
        return;
    auto& mem = Memory::Get();
    if (!m_viewUpdaterKlass) {
        const uintptr_t slots[] = {
            offsets::typeinfo::BattleEntitiesUpdater,
            offsets::typeinfo::BattleEntitiesUpdaterDump,
        };
        m_viewUpdaterKlass = ResolveNamedKlass(
            "BattleEntitiesUpdater", "Game.Battle", slots, std::size(slots));
    }
    if (!m_viewUpdaterKlass)
        return;
    auto hits = mem.SearchHeap(&m_viewUpdaterKlass, sizeof(m_viewUpdaterKlass), 8, 32);
    for (uintptr_t hit : hits) {
        if (m_gameAssemblySize && hit >= m_gameAssembly &&
            hit < m_gameAssembly + m_gameAssemblySize)
            continue;
        uintptr_t header = 0;
        if (!mem.ReadOk(hit, header) || header != m_viewUpdaterKlass)
            continue;
        uintptr_t battle = m_battleEntities.load();
        if (IsPtr(battle)) {
            uintptr_t model = 0;
            if (!mem.ReadOk(hit + offsets::battle_entities_view_updater::BattleEntities, model) ||
                model != battle)
                continue;
        }
        uintptr_t views = 0;
        if (!mem.ReadOk(hit + offsets::battle_entities_view_updater::CharacterViews, views) ||
            !IsPtr(views))
            continue;
        m_viewUpdater.store(hit);
        std::cout << "[+] BattleEntitiesUpdater @ 0x" << std::hex << hit << std::dec << "\n"
                  << std::flush;
        return;
    }
}

void UnityEngine::CacheGlobals() {
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

void UnityEngine::CacheActors() {
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
    } else {
        m_clientPos.store(0);
        m_clientLookQuat.store(0);
        m_clientYaw.store(0);
        m_clientPitch.store(0);
        m_followPos.store(0);
        m_recoilObject.store(0);
        m_recoilKlass.store(0);
        m_fakeSpread.store(0);
        m_fakeKlass.store(0);
        m_spreadArgsObject.store(0);
        m_spreadArgsKlass.store(0);
        m_weaponSpreadModel.store(0);
    }

    std::unordered_map<int, std::string> names;
    std::unordered_map<int, std::string> clans;
    if (IsPtr(slotsModel)) {
        uintptr_t slotDict = mem.Read<uintptr_t>(slotsModel + offsets::slots::ByEntityId);
        std::vector<DictItem> slots;
        if (ReadIntDictionary(slotDict, slots, 128)) {
            for (const auto& slot : slots) {
                uintptr_t data = mem.Read<uintptr_t>(slot.value + offsets::slot::Data);
                if (!IsPtr(data)) continue;
                uintptr_t nickPrim = mem.Read<uintptr_t>(data + offsets::slot_data::Nickname);
                uintptr_t clanPrim = mem.Read<uintptr_t>(data + offsets::slot_data::ClanTag);
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
                if (reuse(item.value, EntityType::Item, item.key))
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
                entity.type = EntityType::Item;
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
            const auto slash = id.find_last_of("/.");
            if (slash != std::string::npos)
                id.erase(0, slash + 1);
            std::string out;
            out.reserve(id.size() + 8);
            for (size_t i = 0; i < id.size(); ++i) {
                const unsigned char c = static_cast<unsigned char>(id[i]);
                if (c == '_' || c == '-') {
                    if (!out.empty() && out.back() != ' ')
                        out.push_back(' ');
                    continue;
                }
                if (i > 0 && std::isupper(c) && std::islower(static_cast<unsigned char>(id[i - 1])))
                    out.push_back(' ');
                out.push_back(static_cast<char>(c));
            }
            return out;
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
        auto applyLootItem = [&](uintptr_t lootItem, GameEntity& entity) {
            if (!IsPtr(lootItem))
                return;
            std::string id = ReadEnumId(lootItem);
            uintptr_t inv = delegateTarget(mem.Read<uintptr_t>(lootItem + offsets::loot_item::ItemDescriptionFunc));
            if (IsPtr(inv)) {
                int rarity = -1;
                if (mem.ReadOk(inv + offsets::inventory_item::Rarity, rarity) &&
                    rarity >= 0 && rarity <= 6)
                    entity.lootTier = rarity;
                const int price = closurePrice(delegateTarget(
                    mem.Read<uintptr_t>(inv + offsets::inventory_item::BasePriceFunc)));
                if (price > 0)
                    entity.cash = price;
                std::string invId = ReadEnumId(inv);
                if (!invId.empty())
                    id = invId;
            }
            if (!id.empty())
                entity.displayName = prettyItemName(id);
            if (entity.cash > 0)
                entity.displayName += " $" + std::to_string(entity.cash);
        };

        auto addPlacedLoot = [&](uintptr_t dict, uintptr_t descOff, uintptr_t posOff, bool randomItem) {
            std::vector<DictItem> nodes;
            if (!ReadIntDictionary(dict, nodes, 400)) return;
            for (const auto& item : nodes) {
                if (reuse(item.value, EntityType::Item, item.key))
                    continue;
                uintptr_t taken = mem.Read<uintptr_t>(item.value + offsets::random_loot::IsTaken);
                if (IsPtr(taken) && mem.Read<uint8_t>(taken + offsets::il2cpp::Primitive_Value) != 0)
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
                bool numeric = !entity.displayName.empty();
                for (unsigned char c : entity.displayName) {
                    if (!std::isdigit(c)) { numeric = false; break; }
                }
                if (entity.displayName.empty() || numeric)
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
                entity.displayName = ReadEnumId(mem.Read<uintptr_t>(item.value + offsets::grenade::Description));
                if (entity.displayName.empty())
                    entity.displayName = "Grenade";
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
                if (empty && !Config::Get().esp.showEmptyContainers) continue;

                GameEntity entity;
                entity.address = item.value;
                entity.creationIndex = item.key;
                entity.type = EntityType::Container;
                entity.isEnabled = true;
                entity.isAlive = true;
                entity.displayName = ReadEnumId(mem.Read<uintptr_t>(item.value + offsets::container::Description));
                if (entity.displayName.empty())
                    entity.displayName = "Container";
                if (empty)
                    entity.extra = "empty";
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

                GameEntity entity;
                entity.address = item.value;
                entity.creationIndex = item.key;
                entity.type = EntityType::Item;
                entity.isEnabled = true;
                entity.isAlive = true;
                entity.displayName = "Cash";
                entity.cash = mem.Read<int>(amountAddr);
                entity.cashAddr = amountAddr;
                entity.position = pos;
                entity.posVec = desc + offsets::cash_desc::Position;
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

void UnityEngine::AttachRenderedViews(std::vector<GameEntity>& actors) {
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
}

void UnityEngine::CachePositions() {
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
    struct FollowSample {
        float position[3] = {};
    };
    struct BrainSample {
        int frame = 0;
        float fov = 0.f;
        float pose[7] = {};
        float correction[3] = {};
        float orient[4] = { 0.f, 0.f, 0.f, 1.f };
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
    FollowSample followSample;
    BattleCamSample battleCamSample;
    BrainSample brainSample[8];
    uintptr_t clientPos = m_clientPos.load();
    uintptr_t clientLookQuat = m_clientLookQuat.load();
    uintptr_t clientYawAddr = m_clientYaw.load();
    uintptr_t clientPitchAddr = m_clientPitch.load();
    uintptr_t followAddr = m_followPos.load();
    int brainCount = m_brainCount.load();
    if (brainCount < 0) brainCount = 0;
    if (brainCount > 8) brainCount = 8;
    uintptr_t brainAddr[8] = {};
    for (int i = 0; i < brainCount; ++i)
        brainAddr[i] = m_brains[i].load();
    int rigKind = m_rigKind.load();
    uintptr_t rigAddr = m_rig.load();
    float rigRaw[40] = {};
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
        if (followAddr)
            scatter.Prepare(followAddr, followSample.position, sizeof(followSample.position));
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
        if (rigAddr) {
            size_t rigBytes = rigKind == 2 ? sizeof(rigRaw) : sizeof(float) * 6;
            scatter.Prepare(rigAddr + offsets::third_person::ShoulderOffset, rigRaw, rigBytes);
        }
        for (int i = 0; i < brainCount; ++i) {
            if (!brainAddr[i]) continue;
            scatter.Prepare(brainAddr[i] + offsets::cinemachine_brain::LastFrame, &brainSample[i].frame);
            scatter.Prepare(brainAddr[i] + offsets::cinemachine_brain::FieldOfView, &brainSample[i].fov);
            scatter.Prepare(brainAddr[i] + offsets::cinemachine_brain::RawPose,
                            brainSample[i].pose, sizeof(brainSample[i].pose));
            scatter.Prepare(brainAddr[i] + offsets::cinemachine_brain::PositionCorrection,
                            brainSample[i].correction, sizeof(brainSample[i].correction));
            scatter.Prepare(brainAddr[i] + offsets::cinemachine_brain::OrientationCorrection,
                            brainSample[i].orient, sizeof(brainSample[i].orient));
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
            if (!hub && s.leave != 0)
                e.isEnabled = false;
            e.isCrouching = s.crouch != 0;
            e.isSprinting = s.sprinting != 0;
            e.hasArmor = s.armor != 0;
            e.inVehicle = s.vehicleId >= 0;
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
    const char* camSource = "none";
    camera.viewInfo.fov = 60.f;

    Vector3 viewPos{
        clientSample.position[0], clientSample.position[1], clientSample.position[2]
    };
    if (FinitePosition(viewPos)) {
        if (!haveLocal) {
            local.position = viewPos;
            local.isLocalPlayer = true;
            local.isEnabled = true;
            haveLocal = true;
        }
        if (UnitQuat(clientSample.quaternion[0], clientSample.quaternion[1],
                     clientSample.quaternion[2], clientSample.quaternion[3])) {
            SetCameraBasis(camera.viewInfo,
                           clientSample.quaternion[0], clientSample.quaternion[1],
                           clientSample.quaternion[2], clientSample.quaternion[3]);
        }
    }

    // Lens is over the shoulder, not on the character. BattleCameraModel
    // stores that world pose (Position + axes + distance-to-follow).
    Vector3 followPos{
        followSample.position[0], followSample.position[1], followSample.position[2]
    };
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
        if (camera.valid)
            camSource = "shoulder";
    }

    Vector3 anchor = FinitePosition(followPos) ? followPos
        : (FinitePosition(viewPos) ? viewPos : local.position);
    bool haveAnchor = FinitePosition(anchor);
    int bestBrain = -1;
    int bestFrame = -1;
    float bestDist = 1.0e9f;
    Vector3 bestPos;
    float bestQuat[4] = {};
    float bestFov = 0.f;
    int seenFrame = -1;
    float seenFov = 0.f;
    Vector3 seenPos;
    for (int i = 0; i < brainCount; ++i) {
        const auto& brain = brainSample[i];
        Vector3 raw{ brain.pose[0], brain.pose[1], brain.pose[2] };
        Vector3 corr{ brain.correction[0], brain.correction[1], brain.correction[2] };
        Vector3 pos = raw;
        if (FinitePosition(raw) && corr.IsValid() && corr.Length() < 20.f)
            pos = raw + corr;
        if (brain.frame > seenFrame) {
            seenFrame = brain.frame;
            seenFov = brain.fov;
            seenPos = pos;
        }
        if (!(brain.fov >= 20.f && brain.fov <= 150.f)) continue;
        if (!FinitePosition(pos)) continue;
        if (!UnitQuat(brain.pose[3], brain.pose[4], brain.pose[5], brain.pose[6])) continue;
        float dist = haveAnchor ? pos.Distance(anchor) : 0.f;
        if (haveAnchor && dist > 40.f) continue;
        // The brain ManualUpdate writes Time.frameCount. The one with the
        // newest frame is the camera that is actually rendering.
        if (brain.frame < bestFrame) continue;
        if (brain.frame == bestFrame && dist >= bestDist) continue;
        float q[4] = { brain.pose[3], brain.pose[4], brain.pose[5], brain.pose[6] };
        if (UnitQuat(brain.orient[0], brain.orient[1], brain.orient[2], brain.orient[3])) {
            float mixed[4];
            QuatMul(q, brain.orient, mixed);
            if (UnitQuat(mixed[0], mixed[1], mixed[2], mixed[3])) {
                q[0] = mixed[0]; q[1] = mixed[1]; q[2] = mixed[2]; q[3] = mixed[3];
            }
        }
        bestFrame = brain.frame;
        bestDist = dist;
        bestBrain = i;
        bestPos = pos;
        bestFov = brain.fov;
        bestQuat[0] = q[0]; bestQuat[1] = q[1]; bestQuat[2] = q[2]; bestQuat[3] = q[3];
    }
    float rigDist = 0.f;
    float rigSide = 0.f;
    Vector3 rigEye;
    Vector3 rigRoot = FinitePosition(followPos) ? followPos : anchor;
    Vector3 lookFwd = camera.viewInfo.hasAxes ? camera.viewInfo.axisForward : Vector3{};
    bool rigOk = rigAddr && lookFwd.Length() > 0.5f &&
        PlaceRigCamera(rigRaw, rigKind == 2, rigRoot, lookFwd, rigEye, rigDist, rigSide);
    if (bestBrain >= 0 && bestFov >= 20.f && bestFov <= 150.f)
        camera.viewInfo.fov = bestFov;
    if (bestBrain >= 0 &&
        (!camera.valid || bestPos.Distance(camera.viewInfo.location) < 8.f)) {
        camera.status = status;
        camera.viewInfo.location = bestPos;
        SetCameraBasis(camera.viewInfo, bestQuat[0], bestQuat[1], bestQuat[2], bestQuat[3]);
        camera.viewInfo.fov = bestFov;
        camera.valid = true;
        camSource = "view";
    } else if (!camera.valid && rigOk) {
        camera.status = status;
        camera.viewInfo.location = rigEye;
        camera.valid = true;
        camSource = "rig";
    } else if (!camera.valid && lookFwd.Length() > 0.5f && FinitePosition(rigRoot)) {
        float defRaw[6] = { 0.45f, 0.2f, 0.f, 0.7f, 1.f, 2.2f };
        if (PlaceRigCamera(defRaw, false, rigRoot, lookFwd, rigEye, rigDist, rigSide)) {
            camera.status = status;
            camera.viewInfo.location = rigEye;
            camera.valid = true;
            camSource = "rig-default";
        }
    }
    if (bestBrain < 0 && brainCount > 0 && seenFrame <= 0)
        m_brainReject.store(1);

    {
        Vector3 clientBody{
            clientSample.position[0], clientSample.position[1], clientSample.position[2]
        };
        auto markLocal = [&](GameEntity& e) {
            if (e.type != EntityType::Player || !e.isEnabled)
                return;
            e.isLocalPlayer = true;
            local = e;
            haveLocal = true;
        };
        if (!haveLocal && FinitePosition(clientBody)) {
            for (auto& e : actors) {
                if (e.type != EntityType::Player || !e.isEnabled)
                    continue;
                if (e.position.Distance(clientBody) < 1.7f) {
                    markLocal(e);
                    break;
                }
            }
        }
        if (haveLocal) {
            for (auto& e : actors) {
                if (e.type != EntityType::Player || e.isLocalPlayer || !e.isEnabled)
                    continue;
                if (FinitePosition(local.position) && e.position.Distance(local.position) < 1.25f)
                    e.isLocalPlayer = true;
                else if (camera.valid &&
                         e.position.Distance(camera.viewInfo.location) < 0.7f)
                    e.isLocalPlayer = true;
            }
        }
    }

    static bool publishedFrameLogged = false;
    static bool publishedViewLogged = false;
    static bool loggedMiss = false;
    bool viewLive = camSource[0] == 'v' || camSource[0] == 'r' || camSource[0] == 's';
    bool miss = !viewLive && (brainCount > 0 || rigAddr != 0);
    if (!publishedFrameLogged || (viewLive && !publishedViewLogged) || (miss && !loggedMiss)) {
        int enabledPlayers = 0;
        int enabledVehicles = 0;
        const GameEntity* firstOther = nullptr;
        for (const auto& e : actors) {
            if (!e.isEnabled) continue;
            if (e.type == EntityType::Player) {
                ++enabledPlayers;
                if (!e.isLocalPlayer && !firstOther) firstOther = &e;
            } else if (e.type == EntityType::Vehicle) {
                ++enabledVehicles;
            }
        }
        const auto& esp = Config::Get().esp;
        int bones = firstOther ? firstOther->boneCount : 0;
        Vector3 forward = camera.viewInfo.hasAxes
            ? camera.viewInfo.axisForward
            : Vector3{};
        if (!camera.viewInfo.hasAxes) {
            constexpr float d2r = 3.14159265358979323846f / 180.f;
            float yaw = camera.viewInfo.rotation.yaw * d2r;
            float pitch = camera.viewInfo.rotation.pitch * d2r;
            forward = {
                std::sinf(yaw) * std::cosf(pitch),
                std::sinf(pitch),
                std::cosf(yaw) * std::cosf(pitch)
            };
        }
        std::cout << "[+] Published frame: camera=" << (camera.valid ? "yes" : "no")
                  << " source=" << camSource
                  << " local=" << (haveLocal ? "yes" : "no")
                  << " players=" << enabledPlayers
                  << " vehicles=" << enabledVehicles
                  << " bones=" << bones
                  << " esp=" << (esp.enabled ? "on" : "off")
                  << " showP=" << (esp.showPlayers ? "on" : "off")
                  << " showV=" << (esp.showVehicles ? "on" : "off")
                  << " fov=" << camera.viewInfo.fov
                  << " rig=" << (rigAddr ? (rigKind == 2 ? "aim" : "third") : "no")
                  << " rigDist=" << rigDist
                  << " side=" << rigSide
                  << " brains=" << brainCount
                  << " frame=" << bestFrame
                  << " seenFrame=" << seenFrame
                  << " seenFov=" << seenFov
                  << " seen=(" << seenPos.x << "," << seenPos.y << "," << seenPos.z << ")"
                  << " cam=(" << camera.viewInfo.location.x << ","
                  << camera.viewInfo.location.y << ","
                  << camera.viewInfo.location.z << ")"
                  << " yaw=" << camera.viewInfo.rotation.yaw
                  << " pitch=" << camera.viewInfo.rotation.pitch;
        if (firstOther) {
            Vector3 delta = firstOther->position - camera.viewInfo.location;
            std::cout << " first=(" << firstOther->position.x << ","
                      << firstOther->position.y << "," << firstOther->position.z
                      << ") front=" << delta.Dot(forward);
        }
        std::cout << "\n" << std::flush;
        publishedFrameLogged = true;
        if (viewLive) publishedViewLogged = true;
        if (miss) loggedMiss = true;
    }

    int enabledPlayers = 0;
    for (const auto& a : actors) {
        if (a.type == EntityType::Player && a.isEnabled && !a.isLocalPlayer)
            ++enabledPlayers;
    }
    Config::AutoTuneWorkersForEsp(enabledPlayers, true);

    actors.erase(std::remove_if(actors.begin(), actors.end(), [](const GameEntity& e) {
        if (e.isLocalPlayer) return false;
        if (e.type != EntityType::Player || e.isAlive) return false;
        std::string role = e.role;
        for (char& c : role)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return role.find("hub") == std::string::npos;
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

void UnityEngine::RefreshCamera(CameraData& camera) {
    if (!camera.valid || !m_inGame.load()) return;
    int brainCount = m_brainCount.load();
    if (brainCount < 0) brainCount = 0;
    if (brainCount > 8) brainCount = 8;

    struct LookSample {
        float quaternion[4] = { 0.f, 0.f, 0.f, 1.f };
        float yaw = 0.f;
        float pitch = 0.f;
        float follow[3] = {};
    } look;
    struct Sample {
        int frame = 0;
        float fov = 0.f;
        float pose[7] = {};
        float correction[3] = {};
        float orient[4] = { 0.f, 0.f, 0.f, 1.f };
    };
    Sample sample[8];
    uintptr_t addr[8] = {};
    for (int i = 0; i < brainCount; ++i)
        addr[i] = m_brains[i].load();
    uintptr_t lookQuat = m_clientLookQuat.load();
    uintptr_t yawAddr = m_clientYaw.load();
    uintptr_t pitchAddr = m_clientPitch.load();
    uintptr_t followAddr = m_followPos.load();
    uintptr_t battleCam = m_battleCamera.load();
    struct BattleCamSample {
        Vector3 position{};
        float orient[4] = { 0.f, 0.f, 0.f, 1.f };
        Vector3 forward{};
        Vector3 right{};
        Vector3 up{};
    } battleCamSample;

    auto& scatter = Memory::Get().BorrowScatter();
    if (!scatter.IsValid()) return;
    if (lookQuat)
        scatter.Prepare(lookQuat, look.quaternion, sizeof(look.quaternion));
    if (yawAddr) scatter.Prepare(yawAddr, &look.yaw);
    if (pitchAddr) scatter.Prepare(pitchAddr, &look.pitch);
    if (followAddr) scatter.Prepare(followAddr, look.follow, sizeof(look.follow));
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
    }
    for (int i = 0; i < brainCount; ++i) {
        if (!addr[i]) continue;
        scatter.Prepare(addr[i] + offsets::cinemachine_brain::LastFrame, &sample[i].frame);
        scatter.Prepare(addr[i] + offsets::cinemachine_brain::FieldOfView, &sample[i].fov);
        scatter.Prepare(addr[i] + offsets::cinemachine_brain::RawPose,
                        sample[i].pose, sizeof(sample[i].pose));
        scatter.Prepare(addr[i] + offsets::cinemachine_brain::PositionCorrection,
                        sample[i].correction, sizeof(sample[i].correction));
        scatter.Prepare(addr[i] + offsets::cinemachine_brain::OrientationCorrection,
                        sample[i].orient, sizeof(sample[i].orient));
    }
    if (!scatter.Execute()) return;

    const Vector3 anchor = camera.viewInfo.location;
    int best = -1;
    int bestFrame = -1;
    float bestDist = 1.0e9f;
    Vector3 bestPos;
    float bestQuat[4] = {};
    float bestFov = 0.f;
    for (int i = 0; i < brainCount; ++i) {
        const auto& brain = sample[i];
        Vector3 raw{ brain.pose[0], brain.pose[1], brain.pose[2] };
        Vector3 corr{ brain.correction[0], brain.correction[1], brain.correction[2] };
        Vector3 pos = raw;
        if (FinitePosition(raw) && corr.IsValid() && corr.Length() < 20.f)
            pos = raw + corr;
        if (!(brain.fov >= 20.f && brain.fov <= 150.f)) continue;
        if (!FinitePosition(pos)) continue;
        if (!UnitQuat(brain.pose[3], brain.pose[4], brain.pose[5], brain.pose[6])) continue;
        float dist = pos.Distance(anchor);
        if (dist > 40.f) continue;
        if (brain.frame < bestFrame) continue;
        if (brain.frame == bestFrame && dist >= bestDist) continue;
        float q[4] = { brain.pose[3], brain.pose[4], brain.pose[5], brain.pose[6] };
        if (UnitQuat(brain.orient[0], brain.orient[1], brain.orient[2], brain.orient[3])) {
            float mixed[4];
            QuatMul(q, brain.orient, mixed);
            if (UnitQuat(mixed[0], mixed[1], mixed[2], mixed[3])) {
                q[0] = mixed[0]; q[1] = mixed[1]; q[2] = mixed[2]; q[3] = mixed[3];
            }
        }
        bestFrame = brain.frame;
        bestDist = dist;
        best = i;
        bestPos = pos;
        bestFov = brain.fov;
        bestQuat[0] = q[0]; bestQuat[1] = q[1]; bestQuat[2] = q[2]; bestQuat[3] = q[3];
    }
    if (best >= 0) {
        camera.viewInfo.location = bestPos;
        SetCameraBasis(camera.viewInfo, bestQuat[0], bestQuat[1], bestQuat[2], bestQuat[3]);
        camera.viewInfo.fov = bestFov;
        camera.valid = true;
        return;
    }

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
        if (camera.valid)
            return;
    }

    if (!UnitQuat(look.quaternion[0], look.quaternion[1], look.quaternion[2], look.quaternion[3]))
        return;
    Vector3 lookForward = QuatForward(
        look.quaternion[0], look.quaternion[1], look.quaternion[2], look.quaternion[3]);
    SetCameraBasis(camera.viewInfo,
                   look.quaternion[0], look.quaternion[1], look.quaternion[2], look.quaternion[3]);
    camera.viewInfo.rotation.yaw = LookYawDegrees(lookForward, look.yaw);
    camera.viewInfo.rotation.pitch = LookPitchDegrees(lookForward, look.pitch);
    camera.viewInfo.rotation.roll = 0.f;
    Vector3 follow{ look.follow[0], look.follow[1], look.follow[2] };
    Vector3 rigEye;
    float rigDist = 0.f;
    float rigSide = 0.f;
    float defRaw[6] = { 0.45f, 0.2f, 0.f, 0.7f, 1.f, 2.2f };
    Vector3 root = FinitePosition(follow) ? follow : camera.viewInfo.location;
    if (FinitePosition(root) &&
        PlaceRigCamera(defRaw, false, root, lookForward, rigEye, rigDist, rigSide))
        camera.viewInfo.location = rigEye;
    camera.valid = camera.viewInfo.location.IsValid();
}

void UnityEngine::SetMemoryAimTarget(bool active, const Vector3& eye, const Vector3& targetWorld,
                                     float smooth) {
    std::lock_guard<std::mutex> lock(m_aimMutex);
    m_aimActive = active;
    if (!active)
        return;
    m_aimEye = eye;
    m_aimTarget = targetWorld;
    m_aimSmooth = smooth;
}

void UnityEngine::ServiceMemoryAim() {
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
    WriteMemoryAim(eye, target, smooth);
}

bool UnityEngine::WriteMemoryAim(const Vector3& eye, const Vector3& targetWorld, float smooth) {
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

void UnityEngine::ApplyGun() {
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

std::vector<GameEntity> UnityEngine::GetEntities() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_entities;
}

GameEntity UnityEngine::GetLocalPlayer() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_localPlayer;
}

CameraData UnityEngine::GetCamera() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_camera;
}

void UnityEngine::CopyRenderState(std::vector<GameEntity>& entities, GameEntity& local, CameraData& camera) {
    std::lock_guard<std::mutex> lock(m_mutex);
    entities = m_entities;
    local = m_localPlayer;
    camera = m_camera;
}
