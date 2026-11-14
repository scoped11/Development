#pragma once
#include "robrun_mem.hpp"
#include "../../../dma/src/engine/offsets.h"
#include "../../../dma/src/math/vector.h"
#include <vector>

struct DictItem {
    int key = 0;
    uintptr_t value = 0;
};

inline uintptr_t DictObject(RobRunMem& mem, uintptr_t dict, int maxCount)
{
    if (!IsPtr(dict) || maxCount < 0)
        return 0;
    auto usable = [&](uintptr_t object, int& count) {
        count = -1;
        if (!IsPtr(object))
            return false;
        int live = mem.Read<int>(object + offsets::il2cpp::Dict_Count);
        int freeCount = mem.Read<int>(object + offsets::il2cpp::Dict_FreeCount);
        if (live < 0 || live > maxCount)
            return false;
        if (freeCount < 0 || freeCount > live)
            return false;
        if (live > 0) {
            uintptr_t entries = mem.Read<uintptr_t>(object + offsets::il2cpp::Dict_Entries);
            if (!IsPtr(entries))
                return false;
            int length = mem.Read<int>(entries + offsets::il2cpp::Array_Length);
            if (length < live || length > 100000)
                return false;
        }
        count = live;
        return true;
    };
  int outerCount = -1;
    if (usable(dict, outerCount))
        return dict;
    uintptr_t inner = mem.Read<uintptr_t>(dict + offsets::il2cpp::Dict_Buckets);
    int innerCount = -1;
    if (usable(inner, innerCount))
        return inner;
    return 0;
}

inline bool ReadIntDictionary(RobRunMem& mem, uintptr_t dict, std::vector<DictItem>& out,
                              int maxCount)
{
    out.clear();
    dict = DictObject(mem, dict, maxCount);
    if (!dict)
        return false;
    int count = mem.Read<int>(dict + offsets::il2cpp::Dict_Count);
    if (count < 0 || count > maxCount)
        return false;
    if (count == 0)
        return true;
    uintptr_t entries = mem.Read<uintptr_t>(dict + offsets::il2cpp::Dict_Entries);
    if (!IsPtr(entries))
        return false;
    int length = mem.Read<int>(entries + offsets::il2cpp::Array_Length);
    if (length < count || length > 100000)
        return false;
    const size_t bytes = static_cast<size_t>(length) * offsets::il2cpp::Entry_Stride;
    std::vector<uint8_t> raw(bytes);
    if (!mem.ReadRaw(entries + offsets::il2cpp::Array_First, raw.data(), bytes))
        return false;
    out.reserve(static_cast<size_t>(count));
    for (int i = 0; i < length; ++i) {
        const uint8_t* entry = raw.data() + static_cast<size_t>(i) * offsets::il2cpp::Entry_Stride;
        int hash = *reinterpret_cast<const int*>(entry + offsets::il2cpp::Entry_Hash);
        if (hash < 0)
            continue;
        int key = *reinterpret_cast<const int*>(entry + offsets::il2cpp::Entry_Key);
        uintptr_t value = *reinterpret_cast<const uintptr_t*>(entry + offsets::il2cpp::Entry_Value);
        if (!IsPtr(value))
            continue;
        out.push_back({ key, value });
    }
    return true;
}

inline bool FinitePos(const Vector3& v)
{
    return v.IsValid() && v.Length() > 0.05f && v.Length() < 50000.f;
}

inline bool ReadCharacterPosition(RobRunMem& mem, uintptr_t character, Vector3& out)
{
    uintptr_t repl = mem.Read<uintptr_t>(character + offsets::character::ReplicatedTransform);
    if (IsPtr(repl)) {
        uintptr_t posPrim = mem.Read<uintptr_t>(repl + offsets::replicated::Position);
        if (IsPtr(posPrim)) {
            Vector3 p = mem.Read<Vector3>(posPrim + offsets::il2cpp::Primitive_Value);
            if (FinitePos(p)) {
                out = p;
                return true;
            }
        }
    }
    uintptr_t transform = mem.Read<uintptr_t>(character + offsets::character::Transform);
    if (!IsPtr(transform))
        return false;
    auto axis = [&](uintptr_t prim) {
        if (!IsPtr(prim))
            return 0.f;
        uintptr_t data = mem.Read<uintptr_t>(prim + offsets::il2cpp::PrimitiveModel_Data);
        if (!IsPtr(data))
            return mem.Read<float>(prim + offsets::il2cpp::Primitive_Value);
        return mem.Read<float>(data + offsets::il2cpp::Primitive_Value);
    };
    float x = axis(mem.Read<uintptr_t>(transform + offsets::transform::X));
    float y = axis(mem.Read<uintptr_t>(transform + offsets::transform::Y));
    float z = axis(mem.Read<uintptr_t>(transform + offsets::transform::Z));
    out = { x, y, z };
    return FinitePos(out);
}
