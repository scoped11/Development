#!/usr/bin/env python3
"""Cross-check src/engine/offsets.h field constants against dump.cs."""

from __future__ import annotations

import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
DUMP = REPO.parent / "dumps" / "2026-11-12" / "dump.cs"


def normalize_field_name(name: str) -> str:
    name = name.strip()
    m = re.match(r"^<(.+)>k__BackingField$", name)
    return m.group(1) if m else name


class TypeDump:
    def __init__(self, fq: str) -> None:
        self.fq = fq
        self.fields: list = []


class FieldDump:
    def __init__(self, csharp_name: str, offset: int) -> None:
        self.csharp_name = csharp_name
        self.name = csharp_name
        self.offset = offset


CLASS_NS = re.compile(r"^// Namespace: (?P<ns>\S*)\s*$")
CLASS_LINE = re.compile(
    r"^(?:public |internal |protected |private )?(?:abstract |sealed |static )*(?:class|struct) (?P<name>\S+)"
)
FIELD_LINE = re.compile(
    r"^\s+(?:public |private |protected |internal )(?:static )?(?:readonly )?(?P<ty>.+?) (?P<field>\S+); // (?P<off>0x[0-9A-Fa-f]+)"
)


def parse_dump_cs(path: str) -> dict[str, TypeDump]:
    current_ns = ""
    current: TypeDump | None = None
    brace = 0
    out: dict[str, TypeDump] = {}
    with open(path, encoding="utf-8", errors="ignore") as fh:
        for line in fh:
            mns = CLASS_NS.match(line)
            if mns:
                current_ns = mns.group("ns")
                continue
            if current is None:
                mcl = CLASS_LINE.match(line)
                if not mcl:
                    continue
                name = mcl.group("name").split("<")[0]
                fq = f"{current_ns}.{name}" if current_ns else name
                current = TypeDump(fq)
                brace = line.count("{") - line.count("}")
                continue
            brace += line.count("{") - line.count("}")
            mf = FIELD_LINE.match(line)
            if mf:
                current.fields.append(
                    FieldDump(mf.group("field"), int(mf.group("off"), 16))
                )
            if brace <= 0 and current is not None:
                if current.fq not in out or len(current.fields) > len(out[current.fq].fields):
                    out[current.fq] = current
                current = None
    return out

# (fq type, field csharp name, offsets.h label, expected hex)
CHECKS: list[tuple[str, str, str, int]] = [
    ("Game.Battle.BattleEntitiesModel", "_models", "battle_entities::Models", 0x50),
    ("Game.Battle.BattleEntitiesModel", "_worldEntitiesModel", "battle_entities::WorldEntities", 0x60),
    ("Game.Battle.BattleEntitiesModel", "_characterSlotModel", "battle_entities::CharacterSlots", 0x80),
    ("Game.Battle.BattleEntitiesModel", "Player", "battle_entities::Player", 0xD0),
    ("Game.Battle.BattleEntitiesModelUpdater", "_battleEntitiesModel", "battle_entities_updater", 0x50),
    ("Models.Models.BattleCharacter.BattleCharacterModel", "PrivateModel", "character::PrivateModel", 0x10),
    ("Models.Models.BattleCharacter.BattleCharacterModel", "Id", "character::Id", 0x38),
    ("Models.Models.BattleCharacter.BattleCharacterModel", "Transform", "character::Transform", 0x58),
    ("Models.Models.BattleCharacter.BattleCharacterModel", "ReplicatedTransform", "character::ReplicatedTransform", 0x60),
    ("Models.Models.BattleCharacter.BattleCharacterModel", "LookDirection", "character::LookDirection", 0x70),
    ("Models.Models.BattleCharacter.BattleCharacterModel", "IsDead", "character::IsDead", 0xF0),
    ("Models.Models.BattleCharacter.BattleCharacterModel", "SkeletonModel", "character::Skeleton", 0x138),
    ("Models.Models.BattleCharacter.BattleCharacterModel", "IsSprintingModel", "character::IsSprinting", 0x148),
    ("Models.Models.BattleCharacter.BattleCharacterModel", "CrouchModel", "character::Crouch", 0x160),
    ("Models.Models.BattleCharacter.BattleCharacterModel", "ItemInHandsModel", "character::ItemInHands", 0x170),
    ("Models.Models.BattleCharacter.BattleCharacterModel", "Role", "character::Role", 0x208),
    ("Models.Models.BattleCharacter.BattleCharacterModel", "InVehicleModel", "character::InVehicle", 0x1C8),
    ("Models.Models.BattleCharacter.BattleCharacterModel", "HasArmor", "character::HasArmor", 0x1B8),
    ("Models.Models.BattleCharacter.BattleCharacterModel", "StateModel", "character::State", 0x1E0),
    ("Models.Models.BattleCharacter.BattleCharacterModel", "KnockoutModel", "character::Knockout", 0x298),
    ("Models.Models.BattleCharacter.BattleCharacterModel", "KnockdownLeaveStateModel", "character::KnockdownLeave", 0x2D8),
    ("Models.Models.Skeleton.SkeletonModel", "_skeletonDescription", "skeleton::Config", 0x10),
    ("Models.Models.Skeleton.SkeletonModel", "_bonesData", "skeleton::Bones", 0x18),
    ("Models.Models.Skeleton.SkeletonConfig", "BonesCount", "skeleton_config::BonesCount", 0x10),
    ("Models.Models.Skeleton.SkeletonConfig", "BoneDataSize", "skeleton_config::BoneDataSize", 0x14),
    ("Models.Models.Skeleton.SkeletonConfig", "BoneDataLocalPositionOrientationOffset", "LocalOrientation", 0x18),
    ("Models.Models.Skeleton.SkeletonConfig", "BoneDataModelOrientationOffset", "ModelOrientation", 0x1C),
    ("Models.Models.Skeleton.SkeletonConfig", "BoneDataLengthOffset", "Length", 0x20),
    ("Models.Models.Skeleton.SkeletonConfig", "BonesHierarchyData", "Hierarchy", 0x28),
    ("Models.Models.Skeleton.SkeletonConfig", "BonesHierarchyMaxChildrenCount", "MaxChildren", 0x30),
    ("Models.Models.Skeleton.SkeletonConfig", "RootIndex", "Chain[0]", 0x34),
    ("Models.Models.Skeleton.SkeletonConfig", "Head2Index", "Chain[6]", 0x4C),
    ("Models.Models.LookDirection.LookDirectionModel", "_value", "look::Forward", 0x10),
    ("Game.Battle.EntityModel.Player.PlayerEntityModel", "ClientModel", "player_entity::ClientModel", 0x18),
    ("Game.Battle.EntityModel.Player.PlayerEntityModel", "ServerModel", "player_entity::ServerModel", 0x20),
    ("Models.Models.BattleCharacter.BattleCharacterPrivateModel", "Health", "private_model::Health", 0x1B0),
    ("Models.Models.Health.HealthModel", "_health", "health::Amount", 0x10),
    ("Models.Models.Health.HealthModel", "_maxAmount", "health::MaxAmount", 0x20),
    ("Models.Models.WorldEntities.WorldEntitiesModel", "_battleCharacters", "world::BattleCharacters", 0x18),
    ("Models.Models.WorldEntities.WorldEntitiesModel", "_notSpawnedBattleCharacters", "world::NotSpawnedCharacters", 0x10),
    ("Models.Models.WorldEntities.WorldEntitiesModel", "_loots", "world::Loots", 0x38),
    ("Models.Models.WorldEntities.WorldEntitiesModel", "_grenades", "world::Grenades", 0x1A0),
    ("Models.Models.WorldEntities.WorldEntitiesModel", "_cashLootNodes", "world::CashLoot", 0x268),
    ("Models.Models.ClientCharacterSlots.ClientCharacterSlotsModel", "_slotByEntityId", "slots::ByEntityId", 0x18),
]


def field_offset(td, name: str) -> int | None:
    want = normalize_field_name(name)
    for f in td.fields:
        if normalize_field_name(f.csharp_name) == want or normalize_field_name(f.name) == want:
            return f.offset
    return None


def main() -> int:
    dump_path = Path(sys.argv[1]) if len(sys.argv) > 1 else DUMP
    if not dump_path.is_file():
        print(f"Missing {dump_path} — run scripts/dump-and-sync.ps1 first")
        return 1
    types = parse_dump_cs(str(dump_path))
    ok = diff = miss_t = miss_f = 0
    lines: list[str] = []
    for fq, field, label, exp in CHECKS:
        td = types.get(fq)
        if not td:
            # suffix match
            matches = [k for k in types if k.endswith(fq.split(".")[-1]) and fq.split(".")[-1] in k]
            td = types.get(matches[0]) if len(matches) == 1 else None
        if not td:
            miss_t += 1
            lines.append(f"MISSING TYPE | {label} | {fq}")
            continue
        got = field_offset(td, field)
        if got is None:
            miss_f += 1
            lines.append(f"MISSING FIELD | {label} | {fq}.{field}")
            continue
        if got == exp:
            ok += 1
            lines.append(f"OK | {label} | 0x{exp:X}")
        else:
            diff += 1
            lines.append(f"DIFF | {label} | header=0x{exp:X} dump=0x{got:X} | {fq}.{field}")

    print(f"Summary: OK={ok} DIFF={diff} missing_type={miss_t} missing_field={miss_f}\n")
    for line in lines:
        print(line)
    return 1 if diff else 0


if __name__ == "__main__":
    raise SystemExit(main())
