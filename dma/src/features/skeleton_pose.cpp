#include "skeleton_pose.h"

#include <cmath>
#include <deque>

namespace SkeletonPose {
namespace {

struct Quat {
    float x, y, z, w;
};

Quat MulQuat(Quat a, Quat b) {
    return {
        a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
        a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
        a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
        a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
    };
}

Vector3 RotateQuat(Quat q, Vector3 v) {
    Vector3 u{ q.x, q.y, q.z };
    return u * (2.f * u.Dot(v)) + v * (q.w * q.w - u.Dot(v)) + u.Cross(v) * (2.f * q.w);
}

bool UnitQuat(Quat q) {
    const float len = q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
    return len > 0.5f && len < 1.5f;
}

Quat Normalized(Quat q) {
    float len = std::sqrtf(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    if (!(len > 0.01f))
        return { 0.f, 0.f, 0.f, 1.f };
    return { q.x / len, q.y / len, q.z / len, q.w / len };
}

constexpr float kMaxBoneReach = 3.5f;

Quat ReadLocalQuat(const float* row, int stride, int localIndex) {
    if (localIndex < 0 || localIndex + 3 >= stride)
        return { 0.f, 0.f, 0.f, 1.f };
    Quat q{ row[localIndex], row[localIndex + 1], row[localIndex + 2], row[localIndex + 3] };
    return UnitQuat(q) ? Normalized(q) : Quat{ 0.f, 0.f, 0.f, 1.f };
}

Vector3 LocalTranslation(const GameEntity& ent, const float* row) {
    float len = 0.16f;
    if (ent.boneLength >= 0 && ent.boneLength < ent.boneStride) {
        const float stored = row[ent.boneLength];
        if (stored > 0.02f && stored < 2.5f)
            len = stored;
    }
    if (ent.boneLocal >= 0 && ent.boneLocal + 3 < ent.boneStride) {
        Quat packedQ{ row[ent.boneLocal], row[ent.boneLocal + 1],
                      row[ent.boneLocal + 2], row[ent.boneLocal + 3] };
        Vector3 packed{ row[ent.boneLocal], row[ent.boneLocal + 1], row[ent.boneLocal + 2] };
        const float mag = packed.Length();
        if (!UnitQuat(packedQ) && mag > 0.02f && mag < kMaxBoneReach)
            return packed;
    }
    return { 0.f, len, 0.f };
}

int RootBoneIndex(const GameEntity& ent) {
    if (ent.boneIdxRoot >= 0 && ent.boneIdxRoot < ent.boneCount)
        return ent.boneIdxRoot;
    for (int i = 0; i < ent.boneCount; ++i) {
        if (ent.boneParents[static_cast<size_t>(i)] < 0)
            return i;
    }
    return -1;
}

Vector3 PickNamed(const std::vector<Vector3>& pos, int index, const Vector3& fallback) {
    if (index < 0 || index >= static_cast<int>(pos.size()))
        return fallback;
    const Vector3& p = pos[static_cast<size_t>(index)];
    return p.IsValid() ? p : fallback;
}

bool BoneDataReady(const GameEntity& ent) {
    const int n = ent.boneCount;
    if (n < 2 || static_cast<int>(ent.boneParents.size()) != n)
        return false;
    if (ent.boneStride < 5 || ent.boneLocal < 0 || ent.boneLength < 0)
        return false;
    const size_t need = static_cast<size_t>(n) * static_cast<size_t>(ent.boneStride);
    return static_cast<size_t>(ent.boneFloats.size()) >= need;
}

// Engine merges native rendered view into ent.position before skeleton cache.
// If that view is the head/follow bone, drop to estimated feet so the stick
// is not drawn on the local shoulder camera.
Vector3 RootWorldPosition(const GameEntity& ent) {
    if (ent.viewHead.IsValid() && ent.position.Distance(ent.viewHead) < 0.55f) {
        float h = ent.markerHeight > 0.8f ? ent.markerHeight : 1.65f;
        Vector3 feet = ent.viewHead;
        feet.y -= h;
        return feet;
    }
    return ent.position;
}

} // namespace

Result Build(const GameEntity& ent) {
    Result out;
    if (!BoneDataReady(ent))
        return out;

    const int n = ent.boneCount;
    const int rootBone = RootBoneIndex(ent);
    if (rootBone < 0)
        return out;

    const float* data = ent.boneFloats.data();
    const Vector3 rootPos = RootWorldPosition(ent);
    if (!rootPos.IsValid())
        return out;

    const float* rootRow = data + rootBone * ent.boneStride;
    Quat rootLocal = ReadLocalQuat(rootRow, ent.boneStride, ent.boneLocal);
    Quat rootModel = ReadLocalQuat(rootRow, ent.boneStride, ent.boneModel);
    // Extra look-yaw was twisting the mesh off the body. Use the model's
    // orientation when it is a real quat; otherwise the local bone quat.
    Quat rootRot = UnitQuat(rootModel) ? rootModel : rootLocal;
    if (!UnitQuat(rootRot))
        rootRot = { 0.f, 0.f, 0.f, 1.f };

    out.positions.assign(static_cast<size_t>(n), rootPos);
    std::vector<Quat> rot(static_cast<size_t>(n));
    std::vector<char> solved(static_cast<size_t>(n), 0);
    std::deque<int> queue;

    out.positions[static_cast<size_t>(rootBone)] = rootPos;
    rot[static_cast<size_t>(rootBone)] = rootRot;
    solved[static_cast<size_t>(rootBone)] = 1;
    queue.push_back(rootBone);

    while (!queue.empty()) {
        const int parent = queue.front();
        queue.pop_front();
        if (parent < 0 || parent >= n || !solved[static_cast<size_t>(parent)])
            continue;

        const Vector3& parentPose = out.positions[static_cast<size_t>(parent)];
        const Quat& parentRot = rot[static_cast<size_t>(parent)];

        for (int bone = 0; bone < n; ++bone) {
            if (ent.boneParents[static_cast<size_t>(bone)] != parent ||
                solved[static_cast<size_t>(bone)])
                continue;

            const float* childRow = data + bone * ent.boneStride;
            Quat childLocal = ReadLocalQuat(childRow, ent.boneStride, ent.boneLocal);
            Quat childModel = ReadLocalQuat(childRow, ent.boneStride, ent.boneModel);
            Quat childRot = UnitQuat(childLocal) ? MulQuat(parentRot, childLocal)
                : (UnitQuat(childModel) ? childModel : parentRot);
            const Vector3 offset = RotateQuat(parentRot, LocalTranslation(ent, childRow));
            Vector3 childPos = parentPose + offset;
            if (!childPos.IsValid() || childPos.Distance(rootPos) > kMaxBoneReach)
                continue;

            out.positions[static_cast<size_t>(bone)] = childPos;
            rot[static_cast<size_t>(bone)] = childRot;
            solved[static_cast<size_t>(bone)] = 1;
            out.segments.emplace_back(parent, bone);
            queue.push_back(bone);
        }
    }

    if (out.segments.size() < 2)
        return out;

    int solvedCount = 0;
    for (int b = 0; b < n; ++b) {
        if (solved[static_cast<size_t>(b)])
            ++solvedCount;
    }
    if (solvedCount < 2)
        return out;

    out.pelvis = out.positions[static_cast<size_t>(rootBone)];
    out.chest = PickNamed(out.positions, ent.boneIdxSpine3,
                          PickNamed(out.positions, ent.boneIdxSpine2, out.pelvis));
    out.neck = PickNamed(out.positions, ent.boneIdxNeck, out.chest);
    if (ent.boneIdxHead2 >= 0 && ent.boneIdxHead2 < n && solved[static_cast<size_t>(ent.boneIdxHead2)])
        out.head = out.positions[static_cast<size_t>(ent.boneIdxHead2)];
    else if (ent.boneIdxHead1 >= 0 && ent.boneIdxHead1 < n && solved[static_cast<size_t>(ent.boneIdxHead1)])
        out.head = out.positions[static_cast<size_t>(ent.boneIdxHead1)];
    else
        out.head = out.neck;

    if (!out.head.IsValid() || out.head.Distance(rootPos) > kMaxBoneReach)
        return out;

    out.ok = true;
    return out;
}

} // namespace SkeletonPose
