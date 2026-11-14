#pragma once
#include "robrun_mem.hpp"
#include "robrun_heap.hpp"
#include "robrun_il2cpp.hpp"
#include "../../../dma/src/engine/offsets.h"

inline uintptr_t ResolveKlassWord(RobRunMem& mem, uintptr_t gameAssembly, uintptr_t typeInfoRva)
{
    uintptr_t word = mem.Read<uintptr_t>(gameAssembly + typeInfoRva);
  uintptr_t cur = word;
    for (int depth = 0; depth < 4; ++depth) {
        if (!IsPtr(cur))
            break;
        if (cur == word && depth == 0) {
            // Il2CppClass* is often stored directly in the TypeInfo slot.
            return cur;
        }
        cur = mem.Read<uintptr_t>(cur);
    }
    return word;
}

struct RobRunWorld {
    uintptr_t battleEntities = 0;
    uintptr_t battleModel = 0;
    uintptr_t battleCamera = 0;
};

inline bool ValidateBattleEntities(RobRunMem& mem, uintptr_t object)
{
    if (!IsPtr(object))
        return false;
    uintptr_t world = mem.Read<uintptr_t>(object + offsets::battle_entities::WorldEntities);
    return IsPtr(world);
}

inline RobRunWorld AttachBattleWorld(RobRunMem& mem, uint32_t pid, uintptr_t gameAssembly,
                                     size_t gameAssemblySize)
{
    RobRunWorld w{};
    const uintptr_t imageLo = gameAssembly;
    const uintptr_t imageHi = gameAssembly + gameAssemblySize;

    const uintptr_t slots[] = {
        offsets::typeinfo::BattleModel,
        offsets::typeinfo::BattleModelDump,
    };
    uintptr_t battleKlass = 0;
    for (uintptr_t rva : slots) {
        battleKlass = ResolveKlassWord(mem, gameAssembly, rva);
        if (IsPtr(battleKlass))
            break;
    }
    if (!IsPtr(battleKlass))
        return w;

    const auto hits = HeapScanKlass(mem, pid, battleKlass, imageLo, imageHi, 16);
    for (uintptr_t hit : hits) {
        uintptr_t header = mem.Read<uintptr_t>(hit);
        if (header != battleKlass)
            continue;
        uintptr_t battleEntities = mem.Read<uintptr_t>(hit + offsets::battle_model::BattleEntities);
        if (!IsPtr(battleEntities) || !ValidateBattleEntities(mem, battleEntities))
            continue;
        w.battleModel = hit;
        w.battleEntities = battleEntities;
        uintptr_t cam = mem.Read<uintptr_t>(hit + offsets::battle_model::BattleCamera);
        if (IsPtr(cam))
            w.battleCamera = cam;
        return w;
    }

    // Fallback: scan for BattleEntitiesModel klass directly.
    uintptr_t entitiesKlass = ResolveKlassWord(mem, gameAssembly, offsets::typeinfo::BattleEntitiesModel);
    if (!IsPtr(entitiesKlass))
        entitiesKlass = ResolveKlassWord(mem, gameAssembly, offsets::typeinfo::BattleEntitiesModelDump);
    if (!IsPtr(entitiesKlass))
        return w;
    const auto entityHits = HeapScanKlass(mem, pid, entitiesKlass, imageLo, imageHi, 8);
    for (uintptr_t hit : entityHits) {
        if (mem.Read<uintptr_t>(hit) != entitiesKlass)
            continue;
        if (!ValidateBattleEntities(mem, hit))
            continue;
        w.battleEntities = hit;
        return w;
    }
    return w;
}
