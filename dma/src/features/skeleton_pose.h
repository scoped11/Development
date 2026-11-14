#pragma once

#include "engine/engine.h"
#include "math/vector.h"

#include <utility>
#include <vector>

namespace SkeletonPose {

struct Result {
    bool ok = false;
    std::vector<Vector3> positions;
    std::vector<std::pair<int, int>> segments;
    Vector3 pelvis{};
    Vector3 chest{};
    Vector3 neck{};
    Vector3 head{};
};

// FK from the parent table, model/local quats, and bone length or packed xyz.
Result Build(const GameEntity& ent);

} // namespace SkeletonPose
