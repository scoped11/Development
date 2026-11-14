#pragma once
#include "robrun_mem.hpp"
#include "robrun_heap.hpp"
#include "robrun_il2cpp.hpp"
#include "../../../dma/src/engine/offsets.h"

inline bool MatchesKlassName(RobRunMem& mem, uintptr_t klass, const char* name)
{
    if (!IsPtr(klass) || !name)
        return false;
    uintptr_t namePtr = mem.Read<uintptr_t>(klass + offsets::il2cpp::Class_Name);
    if (!IsPtr(namePtr))
        return false;
    return mem.ReadString(namePtr, 80) == name;
}

inline uintptr_t ResolveKlassWord(RobRunMem& mem, uintptr_t gameAssembly, uintptr_t typeInfoRva,
                                  const char* name, const char* nameSpace = nullptr)
{
    uintptr_t cur = mem.Read<uintptr_t>(gameAssembly + typeInfoRva);
    for (int depth = 0; depth < 4; ++depth) {
        if (!cur)
            return 0;
        if (MatchesKlassName(mem, cur, name))
            return cur;
        if (nameSpace) {
            uintptr_t ns = mem.Read<uintptr_t>(cur + offsets::il2cpp::Class_Namespace);
            if (IsPtr(ns) && mem.ReadString(ns, 80) == nameSpace && MatchesKlassName(mem, cur, name))
                return cur;
        }
        if (!IsPtr(cur))
            return 0;
        cur = mem.Read<uintptr_t>(cur);
    }
    return 0;
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
        battleKlass = ResolveKlassWord(mem, gameAssembly, rva, "BattleModel", "Game.Battle");
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
    uintptr_t entitiesKlass =
        ResolveKlassWord(mem, gameAssembly, offsets::typeinfo::BattleEntitiesModel,
                         "BattleEntitiesModel", "Game.Battle");
    if (!IsPtr(entitiesKlass))
        entitiesKlass = ResolveKlassWord(mem, gameAssembly, offsets::typeinfo::BattleEntitiesModelDump,
                                           "BattleEntitiesModel", "Game.Battle");
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
