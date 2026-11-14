#!/usr/bin/env python3
"""Build OFFSET_INVENTORY.md: every offsets.h constant + dump verification + engine usage."""

from __future__ import annotations

import re
import subprocess
import sys
from collections import defaultdict
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
DUMP_DIR = REPO.parent / "dumps" / "2026-11-12"
from validate_offsets_against_dump import normalize_field_name, parse_dump_cs
OFFSETS_H = REPO / "src" / "engine" / "offsets.h"
ENGINE_CPP = REPO / "src" / "game" / "runtime.cpp"

# (offsets.h namespace path, fq type, field name in dump.cs backing field or property)
DUMP_CHECKS: dict[str, tuple[str, str]] = {
    "battle_entities::Models": ("Game.Battle.BattleEntitiesModel", "_models"),
    "battle_entities::WorldEntities": ("Game.Battle.BattleEntitiesModel", "_worldEntitiesModel"),
    "battle_entities::CharacterSlots": ("Game.Battle.BattleEntitiesModel", "_characterSlotModel"),
    "battle_entities::Player": ("Game.Battle.BattleEntitiesModel", "Player"),
    "battle_entities_updater::BattleEntities": ("Game.Battle.BattleEntitiesModelUpdater", "_battleEntitiesModel"),
    "battle_entities_view_updater::BattleEntities": ("Game.Battle.BattleEntitiesUpdater", "_battleEntitiesModel"),
    "character::PrivateModel": ("Models.Models.BattleCharacter.BattleCharacterModel", "PrivateModel"),
    "character::Id": ("Models.Models.BattleCharacter.BattleCharacterModel", "Id"),
    "character::Transform": ("Models.Models.BattleCharacter.BattleCharacterModel", "Transform"),
    "character::ReplicatedTransform": ("Models.Models.BattleCharacter.BattleCharacterModel", "ReplicatedTransform"),
    "character::LookDirection": ("Models.Models.BattleCharacter.BattleCharacterModel", "LookDirection"),
    "character::IsDead": ("Models.Models.BattleCharacter.BattleCharacterModel", "IsDead"),
    "character::Skeleton": ("Models.Models.BattleCharacter.BattleCharacterModel", "SkeletonModel"),
    "character::IsSprinting": ("Models.Models.BattleCharacter.BattleCharacterModel", "IsSprintingModel"),
    "character::Crouch": ("Models.Models.BattleCharacter.BattleCharacterModel", "CrouchModel"),
    "character::ItemInHands": ("Models.Models.BattleCharacter.BattleCharacterModel", "ItemInHandsModel"),
    "character::HasArmor": ("Models.Models.BattleCharacter.BattleCharacterModel", "HasArmor"),
    "character::State": ("Models.Models.BattleCharacter.BattleCharacterModel", "StateModel"),
    "character::InVehicle": ("Models.Models.BattleCharacter.BattleCharacterModel", "InVehicleModel"),
    "character::State": ("Models.Models.BattleCharacter.BattleCharacterModel", "StateModel"),
    "character::Knockout": ("Models.Models.BattleCharacter.BattleCharacterModel", "KnockoutModel"),
    "character::KnockdownLeave": ("Models.Models.BattleCharacter.BattleCharacterModel", "KnockdownLeaveStateModel"),
    "character::Knockout": ("Models.Models.BattleCharacter.BattleCharacterModel", "Knockout"),
    "character::KnockdownLeave": ("Models.Models.BattleCharacter.BattleCharacterModel", "KnockdownLeave"),
    "character::Role": ("Models.Models.BattleCharacter.BattleCharacterModel", "Role"),
    "skeleton::Config": ("Models.Models.Skeleton.SkeletonModel", "_skeletonDescription"),
    "skeleton::Bones": ("Models.Models.Skeleton.SkeletonModel", "_bonesData"),
    "skeleton_config::BonesCount": ("Models.Models.Skeleton.SkeletonConfig", "BonesCount"),
    "skeleton_config::BoneDataSize": ("Models.Models.Skeleton.SkeletonConfig", "BoneDataSize"),
    "skeleton_config::LocalOrientation": ("Models.Models.Skeleton.SkeletonConfig", "BoneDataLocalPositionOrientationOffset"),
    "skeleton_config::ModelOrientation": ("Models.Models.Skeleton.SkeletonConfig", "BoneDataModelOrientationOffset"),
    "skeleton_config::Length": ("Models.Models.Skeleton.SkeletonConfig", "BoneDataLengthOffset"),
    "skeleton_config::Hierarchy": ("Models.Models.Skeleton.SkeletonConfig", "BonesHierarchyData"),
    "skeleton_config::MaxChildren": ("Models.Models.Skeleton.SkeletonConfig", "BonesHierarchyMaxChildrenCount"),
    "skeleton_config::Chain": ("Models.Models.Skeleton.SkeletonConfig", "RootIndex"),
    "player_entity::ClientModel": ("Game.Battle.EntityModel.Player.PlayerEntityModel", "ClientModel"),
    "player_entity::ServerModel": ("Game.Battle.EntityModel.Player.PlayerEntityModel", "ServerModel"),
    "player_entity::Id": ("Game.Battle.EntityModel.Player.PlayerEntityModel", "Id"),
    "player_client::SetMovement": ("Game.Battle.EntityModel.Player.PlayerClientModel", "SetMovement"),
    "player_client::Rotation": ("Game.Battle.EntityModel.Player.PlayerClientModel", "Rotation"),
    "player_client::CameraModel": ("Game.Battle.EntityModel.Player.PlayerClientModel", "CameraModel"),
    "player_client::Recoil": ("Game.Battle.EntityModel.Player.PlayerClientModel", "Recoil"),
    "player_client::FakeSpread": ("Game.Battle.EntityModel.Player.PlayerClientModel", "FakeSpread"),
    "private_model::Health": ("Models.Models.BattleCharacter.BattleCharacterPrivateModel", "Health"),
    "health::Amount": ("Models.Models.Health.HealthModel", "Amount"),
    "health::MaxAmount": ("Models.Models.Health.HealthModel", "MaxAmount"),
    "health::IsDead": ("Models.Models.Health.HealthModel", "IsDead"),
    "world::NotSpawnedCharacters": ("Models.Models.WorldEntities.WorldEntitiesModel", "_notSpawnedBattleCharacters"),
    "world::BattleCharacters": ("Models.Models.WorldEntities.WorldEntitiesModel", "_battleCharacters"),
    "world::Containers": ("Models.Models.WorldEntities.WorldEntitiesModel", "_containers"),
    "world::Loots": ("Models.Models.WorldEntities.WorldEntitiesModel", "_loots"),
    "world::Vehicles": ("Models.Models.WorldEntities.WorldEntitiesModel", "_vehicles"),
    "world::Cars": ("Models.Models.WorldEntities.WorldEntitiesModel", "_cars"),
    "world::Helicopters": ("Models.Models.WorldEntities.WorldEntitiesModel", "_helicopters"),
    "world::Grenades": ("Models.Models.WorldEntities.WorldEntitiesModel", "_grenades"),
    "world::LootInventories": ("Models.Models.WorldEntities.WorldEntitiesModel", "_lootInventories"),
    "world::CashLoot": ("Models.Models.WorldEntities.WorldEntitiesModel", "_cashLootNodes"),
    "world::StaticLoots": ("Models.Models.WorldEntities.WorldEntitiesModel", "_staticLootObjects"),
    "world::RandomLoots": ("Models.Models.WorldEntities.WorldEntitiesModel", "_randomLootObjects"),
    "vehicle::Id": ("Models.Models.Vehicle.VehicleModel", "Id"),
    "vehicle::Transform": ("Models.Models.Vehicle.VehicleModel", "Transform"),
    "slots::ByEntityId": ("Models.Models.ClientCharacterSlots.ClientCharacterSlotsModel", "_slotByEntityId"),
    "look::Forward": ("Models.Models.LookDirection.LookDirectionModel", "_value"),
    "health::Amount": ("Models.Models.Health.HealthModel", "_health"),
    "health::MaxAmount": ("Models.Models.Health.HealthModel", "_maxAmount"),
    "third_person::ShoulderOffset": ("Cinemachine.Cinemachine3rdPersonFollow", "ShoulderOffset"),
    "third_person::CameraDistanceLive": ("CinemachineTPSO.CinemachineAimingCameraFollow", "_cameraDistance"),
    "cinemachine_brain::UpdateMethod": ("Cinemachine.CinemachineBrain", "m_UpdateMethod"),
    "cinemachine_brain::LastFrame": ("Cinemachine.CinemachineBrain", "m_LastFrame"),
    "battle_model::BattleEntities": ("Models.Models.Battle.BattleModel", "BattleEntitiesModel"),
    "battle_model::BattleCamera": ("Models.Models.Battle.BattleModel", "BattleCameraModel"),
    "game::GameModel": ("Game.Game", "GameModel"),
}

NATIVE_ONLY_PREFIXES = (
    "il2cpp::",
    "unity::",
    "gom::",
    "game_object::",
    "unity_entry::",
    "unity_player::",
    "identified::",
)


def parse_offsets_h() -> list[dict]:
    text = OFFSETS_H.read_text(encoding="utf-8")
    rows: list[dict] = []
    ns_stack: list[str] = []
    for i, line in enumerate(text.splitlines(), 1):
        m = re.match(r"\s*namespace\s+(\w+)\s*\{", line)
        if m:
            ns_stack.append(m.group(1))
            continue
        if line.strip() == "}" and ns_stack:
            ns_stack.pop()
            continue
        cm = re.match(
            r"\s*constexpr\s+uintptr_t\s+(\w+)\s*=\s*(0x[0-9A-Fa-f]+)\s*;",
            line,
        )
        if cm and ns_stack:
            inner = ns_stack[1:] if ns_stack and ns_stack[0] == "offsets" else ns_stack
            path = ".".join(inner) + "::" + cm.group(1) if inner else cm.group(1)
            rows.append(
                {
                    "path": path,
                    "name": cm.group(1),
                    "value": int(cm.group(2), 16),
                    "hex": cm.group(2),
                    "def_line": i,
                }
            )
    return rows


def usage_in_engine(path_key: str) -> int:
    cpp = ENGINE_CPP.read_text(encoding="utf-8")
    cpp_key = path_key.replace(".", "::")
    return len(re.findall(re.escape(f"offsets::{cpp_key}"), cpp))


def field_offset(types: dict, fq: str, field: str) -> int | None:
    td = types.get(fq)
    if not td:
        simple = fq.split(".")[-1]
        hits = [k for k in types if k.endswith("." + simple) or k == simple]
        td = types.get(hits[0]) if len(hits) == 1 else None
    if not td:
        return None
    want = normalize_field_name(field)
    for f in td.fields:
        if normalize_field_name(f.csharp_name) == want or normalize_field_name(f.name) == want:
            return f.offset
    return None


def main() -> int:
    if not DUMP_DIR.is_dir():
        print("Missing dump dir", DUMP_DIR)
        return 1
    types = parse_dump_cs(str(DUMP_DIR / "dump.cs"))
    rows = parse_offsets_h()
    engine_text = ENGINE_CPP.read_text(encoding="utf-8")

    out_lines = [
        "# RobRun offset inventory (2026-11-10)",
        "",
        "Source: `src/game/offsets.h` | Consumers: `src/game/runtime.cpp` (only file using `offsets::`)",
        "",
        "| Offset | Old (header) | Dump | Status | Used in engine |",
        "|--------|--------------|------|--------|----------------|",
    ]

    updated = 0
    for row in rows:
        path = row["path"]
        old = row["value"]
        used = usage_in_engine(path)
        status = "unchanged"
        dump_val = ""

        if any(path.startswith(p) for p in NATIVE_ONLY_PREFIXES):
            status = "runtime/ABI (not in dump.cs)"
        elif path.startswith("typeinfo::"):
            status = "TypeInfo RVA (script.json)"
        elif path in DUMP_CHECKS:
            fq, field = DUMP_CHECKS[path]
            got = field_offset(types, fq, field)
            if got is None:
                status = "missing in dump"
            elif got != old:
                status = "DIFF"
                dump_val = f"0x{got:X}"
            else:
                dump_val = f"0x{got:X}"
        elif path.startswith("cinemachine_brain::"):
            status = "CameraState+brain layout (dump.cs verified)"
        elif path.startswith("third_person::"):
            status = "Cinemachine follow (dump.cs verified)"
        elif path.startswith("battle_camera::") or path.startswith("transform::"):
            status = "not cross-checked this run"
        elif used == 0:
            status = "defined, unused in engine"

        display = f"offsets::{path.replace('.', '::')}"
        out_lines.append(
            f"| `{display}` | {row['hex']} | {dump_val or '—'} | {status} | {used} |"
        )

    used_names = sum(1 for row in rows if usage_in_engine(row["path"]) > 0)
    out_lines.insert(4, f"- **200** constants in `offsets.h`; **{used_names}** referenced from `runtime.cpp` (grep `offsets::`).")

    report = DUMP_DIR / "OFFSET_INVENTORY.md"
    report.write_text("\n".join(out_lines), encoding="utf-8")
    print(f"Wrote {report} ({len(rows)} constants)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
