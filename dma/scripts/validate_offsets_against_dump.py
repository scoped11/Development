#!/usr/bin/env python3
"""Cross-check src/game/offsets.h field constants, TypeInfo, and getters against dump.cs."""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
_DUMPS = REPO.parent / "dumps"
DUMP = Path(sys.argv[1]) if len(sys.argv) > 1 else None
if DUMP is None:
    candidates = sorted(_DUMPS.glob("*/dump.cs"), key=lambda p: p.stat().st_mtime, reverse=True)
    DUMP = candidates[0] if candidates else _DUMPS / "2026-11-14-v39" / "dump.cs"

TYPEINFO_MAP = {
    "BattleEntitiesModel": "Game.Battle.BattleEntitiesModel",
    "BattleEntitiesModelUpdater": "Game.Battle.BattleEntitiesModelUpdater",
    "BattleEntitiesUpdater": "Game.Battle.BattleEntitiesUpdater",
    "CinemachineBrain": "Cinemachine.CinemachineBrain",
    "CinemachineCore": "Cinemachine.CinemachineCore",
    "CameraBrainUpdater": "Game.Battle.UiUpdater.CameraBrainUpdater",
    "Cinemachine3rdPersonFollow": "Cinemachine.Cinemachine3rdPersonFollow",
    "Game": "Game.Game",
    "GameModel": "Game.GameModel",
    "BattleModel": "Game.Battle.BattleModel",
    "BattleCameraModel": "Game.Battle.Entities.Player.Camera.BattleCameraModel",
    "BattleCameraModelUpdater": "Game.Battle.Entities.Player.Camera.BattleCameraModelUpdater",
}

HEADER_TYPEINFO = {
    "BattleEntitiesModel": 0x81D46B0,
    "BattleEntitiesModelUpdater": 0x81D4848,
    "BattleEntitiesUpdater": 0x81D4B78,
    "CinemachineBrain": 0x8162410,
    "CinemachineCore": 0x8162748,
    "CameraBrainUpdater": 0x81066B0,
    "Cinemachine3rdPersonFollow": 0x8161F40,
    "Game": 0x81455F8,
    "GameModel": 0x81465C0,
    "BattleModel": 0x81D8010,
    "BattleCameraModel": 0x81CB420,
    "BattleCameraModelUpdater": 0x81CB5B8,
}

GETTERS = [
    ("Game.Game", "get_GameModel", 0x904DA0),
    ("Game.GameModel", "Game.IDevelopGameModel.get_BattleModel", 0x7EB6B0),
    ("Game.Battle.BattleModel", "get_BattleEntitiesModel", 0x7A9F90),
    ("Game.Battle.BattleModel", "get_BattleCameraModel", 0x929CA0),
    ("Start.UnityEntry", "get_Game", 0x7A9000),
]

INHERIT = {
    "Game.Battle.EntityModel.Player.PlayerEntityModel": [
        "Game.Battle.EntityModel.EntityModel"
    ],
    "Game.Battle.EntityModel.Player.PlayerClientModel": [
        "Game.Battle.EntityModel.BattleCharacter.CharacterClientModel"
    ],
}


def normalize_field_name(name: str) -> str:
    name = name.strip()
    m = re.match(r"^<(.+)>k__BackingField$", name)
    return m.group(1) if m else name


class TypeDump:
    def __init__(self, fq: str) -> None:
        self.fq = fq
        self.fields: list[FieldDump] = []
        self.methods: dict[str, int] = {}


class FieldDump:
    def __init__(self, csharp_name: str, offset: int) -> None:
        self.csharp_name = csharp_name
        self.name = csharp_name
        self.offset = offset


CLASS_NS = re.compile(r"^// Namespace: (?P<ns>\S*)\s*$")
CLASS_LINE = re.compile(
    r"^(?:public |internal |protected |private )?(?:abstract |sealed |static )*(?:class|struct|interface) (?P<name>\S+)"
)
FIELD_LINE = re.compile(
    r"^\s+(?:public |private |protected |internal )(?:static )?(?:readonly )?(?P<ty>.+?) (?P<field>\S+); // (?P<off>0x[0-9A-Fa-f]+)"
)
RVA_LINE = re.compile(r"^\s+// RVA: (?P<rva>0x[0-9A-Fa-f]+)")
METHOD_LINE = re.compile(
    r"^\s+(?:public |private |protected |internal |static ).*? (?P<name>[\w.]+)\("
)


def parse_dump_cs(path: str) -> dict[str, TypeDump]:
    current_ns = ""
    current: TypeDump | None = None
    brace = 0
    last_rva: int | None = None
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
                last_rva = None
                continue
            brace += line.count("{") - line.count("}")
            mrva = RVA_LINE.match(line)
            if mrva:
                last_rva = int(mrva.group("rva"), 16)
            mf = FIELD_LINE.match(line)
            if mf:
                current.fields.append(
                    FieldDump(mf.group("field"), int(mf.group("off"), 16))
                )
            elif last_rva is not None:
                mm = METHOD_LINE.match(line)
                if mm:
                    current.methods[mm.group("name")] = last_rva
                    last_rva = None
            if brace <= 0 and current is not None:
                if current.fq not in out or len(current.fields) > len(out[current.fq].fields):
                    if current.fq in out:
                        current.methods.update(out[current.fq].methods)
                    out[current.fq] = current
                current = None
    return out


def field_candidates(name: str) -> list[str]:
    n = normalize_field_name(name)
    out: list[str] = []

    def add(x: str) -> None:
        x = normalize_field_name(x)
        if x and x not in out:
            out.append(x)

    add(n)
    stripped = n.lstrip("_")
    add(stripped)
    add("_" + stripped)
    if stripped:
        camel = stripped[0].lower() + stripped[1:]
        add(camel)
        add("_" + camel)
    for suf in ("Model", "View", "Description"):
        if not stripped.endswith(suf):
            add(stripped + suf)
            add("_" + stripped + suf)
        elif len(stripped) > len(suf):
            add(stripped[: -len(suf)])
    return out


def find_type(types: dict[str, TypeDump], fq: str) -> TypeDump | None:
    if fq in types:
        return types[fq]
    name = fq.split(".")[-1]
    matches = [k for k in types if k.endswith("." + name) or k == name]
    if len(matches) == 1:
        return types[matches[0]]
    tail = ".".join(fq.split(".")[-2:])
    matches2 = [k for k in types if k.endswith(tail)]
    if len(matches2) == 1:
        return types[matches2[0]]
    return None


def field_offset(td: TypeDump, name: str, types: dict[str, TypeDump] | None = None) -> int | None:
    want = set(field_candidates(name))
    for f in td.fields:
        got = {normalize_field_name(f.csharp_name), normalize_field_name(f.name)}
        got.update(field_candidates(f.csharp_name))
        if want & got:
            return f.offset
    if types:
        for base in INHERIT.get(td.fq, []):
            btd = find_type(types, base)
            if btd:
                got = field_offset(btd, name, None)
                if got is not None:
                    return got
    return None


# (fq type, field csharp name, offsets.h label, expected hex)
CHECKS: list[tuple[str, str, str, int]] = [
    ("Game.Game", "_viewContainer", "game::ViewContainer", 0x10),
    ("Game.Game", "_scheduler", "game::Scheduler", 0x18),
    ("Game.Game", "GameModel", "game::GameModel", 0xD0),
    ("Game.GameModel", "_battleModel", "game_model::BattleModel", 0x10),
    ("Game.Battle.BattleModel", "BattleEntitiesModel", "battle_model::BattleEntities", 0x90),
    ("Game.Battle.BattleModel", "ShotInputModel", "battle_model::ShotInput", 0x110),
    ("Game.Battle.BattleModel", "BattleCameraModel", "battle_model::BattleCamera", 0x358),
    ("Game.Battle.Entities.Player.Camera.BattleCameraModel", "Position", "battle_camera::Position", 0x10),
    ("Game.Battle.Entities.Player.Camera.BattleCameraModel", "Orientation", "battle_camera::Orientation", 0x1C),
    ("Game.Battle.Entities.Player.Camera.BattleCameraModel", "LookAtCameraOrientation", "battle_camera::LookAtOrientation", 0x2C),
    ("Game.Battle.Entities.Player.Camera.BattleCameraModel", "Forward", "battle_camera::Forward", 0x3C),
    ("Game.Battle.Entities.Player.Camera.BattleCameraModel", "Right", "battle_camera::Right", 0x48),
    ("Game.Battle.Entities.Player.Camera.BattleCameraModel", "Up", "battle_camera::Up", 0x54),
    ("Game.Battle.Entities.Player.Camera.BattleCameraModel", "DistanceToFollowTarget", "battle_camera::DistanceToFollowTarget", 0x60),
    ("Game.Battle.BattleEntitiesModel", "_models", "battle_entities::Models", 0x50),
    ("Game.Battle.BattleEntitiesModel", "_worldEntitiesModel", "battle_entities::WorldEntities", 0x60),
    ("Game.Battle.BattleEntitiesModel", "_characterSlotModel", "battle_entities::CharacterSlots", 0x80),
    ("Game.Battle.BattleEntitiesModel", "Player", "battle_entities::Player", 0xD0),
    ("Game.Battle.BattleEntitiesModel", "EntityModels", "battle_entities::EntityModels", 0xE0),
    ("Game.Battle.BattleEntitiesModelUpdater", "_battleEntitiesModel", "battle_entities_updater::BattleEntities", 0x50),
    ("Game.Battle.BattleEntitiesUpdater", "_battleEntitiesModel", "battle_entities_view_updater::BattleEntities", 0x18),
    ("Game.Battle.BattleEntitiesUpdater", "_playerEntityId", "battle_entities_view_updater::PlayerEntityId", 0x28),
    ("Game.Battle.BattleEntitiesUpdater", "_battleModel", "battle_entities_view_updater::BattleModel", 0x70),
    ("Game.Battle.BattleEntitiesUpdater", "_characterEntityViews", "battle_entities_view_updater::CharacterViews", 0x110),
    ("Game.Battle.BattleEntitiesUpdater", "_carEntityViews", "battle_entities_view_updater::CarViews", 0x118),
    ("Game.Battle.EntityModel.BattleCharacter.Views.BattleCharacterEntityView", "MovementView", "character_entity_view::MovementView", 0x20),
    ("Game.Battle.EntityModel.BattleCharacter.Views.BattleCharacterEntityView", "FollowCamera", "character_entity_view::FollowCamera", 0x58),
    ("Game.Battle.EntityModel.BattleCharacter.Views.BattleCharacterEntityView", "LookRotation", "character_entity_view::LookRotation", 0x80),
    ("Game.Battle.EntityModel.BattleCharacter.Views.BattleCharacterEntityView", "RendersVisibleView", "character_entity_view::RendersVisible", 0xB0),
    ("Game.Battle.EntityModel.BattleCharacter.Views.BattleCharacterEntityView", "CustomizationView", "character_entity_view::CustomizationView", 0xB8),
    ("Game.Battle.EntityModel.BattleCharacter.Views.BattleCharacterEntityView", "HeadPosition", "character_entity_view::HeadPosition", 0xE8),
    ("Models.Models.BattleCharacter.BattleCharacterModel", "PrivateModel", "character::PrivateModel", 0x10),
    ("Models.Models.BattleCharacter.BattleCharacterModel", "Id", "character::Id", 0x38),
    ("Models.Models.BattleCharacter.BattleCharacterModel", "Transform", "character::Transform", 0x58),
    ("Models.Models.BattleCharacter.BattleCharacterModel", "ReplicatedTransform", "character::ReplicatedTransform", 0x60),
    ("Models.Models.BattleCharacter.BattleCharacterModel", "LookDirection", "character::LookDirection", 0x70),
    ("Models.Models.BattleCharacter.BattleCharacterModel", "ReplicatedLookDirection", "character::ReplicatedLook", 0x78),
    ("Models.Models.BattleCharacter.BattleCharacterModel", "LastLookDirection", "character::LastLook", 0x88),
    ("Models.Models.BattleCharacter.BattleCharacterModel", "Input", "character::Input", 0x98),
    ("Models.Models.BattleCharacter.BattleCharacterModel", "LookDirectionInput", "character::LookInput", 0xB0),
    ("Models.Models.BattleCharacter.BattleCharacterModel", "FireModel", "character::Fire", 0xC0),
    ("Models.Models.BattleCharacter.BattleCharacterModel", "IsDead", "character::IsDead", 0xF0),
    ("Models.Models.BattleCharacter.BattleCharacterModel", "ShotArgsModel", "character::ShotArgs", 0xF8),
    ("Models.Models.BattleCharacter.BattleCharacterModel", "ShotInterpolationModel", "character::ShotInterp", 0x108),
    ("Models.Models.BattleCharacter.BattleCharacterModel", "SkeletonModel", "character::Skeleton", 0x138),
    ("Models.Models.BattleCharacter.BattleCharacterModel", "IsSprintingModel", "character::IsSprinting", 0x148),
    ("Models.Models.BattleCharacter.BattleCharacterModel", "CrouchModel", "character::Crouch", 0x160),
    ("Models.Models.BattleCharacter.BattleCharacterModel", "ItemInHandsModel", "character::ItemInHands", 0x170),
    ("Models.Models.BattleCharacter.BattleCharacterModel", "Role", "character::Role", 0x208),
    ("Models.Models.BattleCharacter.BattleCharacterModel", "InVehicleModel", "character::InVehicle", 0x1C8),
    ("Models.Models.BattleCharacter.BattleCharacterModel", "HasArmor", "character::HasArmor", 0x1B8),
    ("Models.Models.BattleCharacter.BattleCharacterModel", "StateModel", "character::State", 0x1E0),
    ("Models.Models.BattleCharacter.BattleCharacterModel", "CustomizationModel", "character::Customization", 0x230),
    ("Models.Models.BattleCharacter.BattleCharacterModel", "KnockoutModel", "character::Knockout", 0x298),
    ("Models.Models.BattleCharacter.BattleCharacterModel", "CopCustomizeModel", "character::CopCustomize", 0x2C8),
    ("Models.Models.BattleCharacter.BattleCharacterModel", "KnockdownLeaveStateModel", "character::KnockdownLeave", 0x2D8),
    ("Models.Models.Skeleton.SkeletonModel", "_skeletonDescription", "skeleton::Config", 0x10),
    ("Models.Models.Skeleton.SkeletonModel", "_bonesData", "skeleton::Bones", 0x18),
    ("Models.Models.Skeleton.SkeletonConfig", "BonesCount", "skeleton_config::BonesCount", 0x10),
    ("Models.Models.Skeleton.SkeletonConfig", "BoneDataSize", "skeleton_config::BoneDataSize", 0x14),
    ("Models.Models.Skeleton.SkeletonConfig", "BoneDataLocalPositionOrientationOffset", "skeleton_config::LocalOrientation", 0x18),
    ("Models.Models.Skeleton.SkeletonConfig", "BoneDataModelOrientationOffset", "skeleton_config::ModelOrientation", 0x1C),
    ("Models.Models.Skeleton.SkeletonConfig", "BoneDataLengthOffset", "skeleton_config::Length", 0x20),
    ("Models.Models.Skeleton.SkeletonConfig", "BonesHierarchyData", "skeleton_config::Hierarchy", 0x28),
    ("Models.Models.Skeleton.SkeletonConfig", "BonesHierarchyMaxChildrenCount", "skeleton_config::MaxChildren", 0x30),
    ("Models.Models.Skeleton.SkeletonConfig", "RootIndex", "skeleton_config::Chain", 0x34),
    ("Models.Models.LookDirection.LookDirectionModel", "_value", "look::Forward", 0x10),
    ("Models.Models.LookDirection.LookDirectionModel", "_yaw", "look::Yaw", 0x1C),
    ("Models.Models.LookDirection.LookDirectionModel", "_pitch", "look::Pitch", 0x20),
    ("Models.Models.ShotArgs.ShotArgsModel", "_position", "shot_args::Position", 0x10),
    ("Models.Models.ShotArgs.ShotArgsModel", "_direction", "shot_args::Direction", 0x18),
    ("Models.Models.ShotInterpolation.ShotInterpolationModel", "_transform", "shot_interp::Position", 0x10),
    ("Models.Models.ShotInterpolation.ShotInterpolationModel", "_lookDirection", "shot_interp::LookDirection", 0x1C),
    ("Game.Battle.EntityModel.Player.PlayerEntityModel", "Id", "player_entity::Id", 0x10),
    ("Game.Battle.EntityModel.Player.PlayerEntityModel", "ClientModel", "player_entity::ClientModel", 0x18),
    ("Game.Battle.EntityModel.Player.PlayerEntityModel", "ServerModel", "player_entity::ServerModel", 0x20),
    ("Game.Battle.EntityModel.Player.PlayerEntityModel", "PublicId", "player_entity::PublicId", 0x28),
    ("Game.Battle.EntityModel.Player.PlayerClientModel", "SetMovement", "player_client::SetMovement", 0x30),
    ("Game.Battle.EntityModel.Player.PlayerClientModel", "RotationModel", "player_client::Rotation", 0x38),
    ("Game.Battle.EntityModel.Player.PlayerClientModel", "CameraModel", "player_client::CameraModel", 0x218),
    ("Game.Battle.EntityModel.Player.PlayerClientModel", "RecoilModel", "player_client::Recoil", 0x2B8),
    ("Game.Battle.EntityModel.Player.PlayerClientModel", "FakeSpreadModel", "player_client::FakeSpread", 0x2C0),
    ("Game.Battle.Entities.Player.Recoil.RecoilModel", "RecoilOffset", "recoil::Offset", 0x10),
    ("Game.Battle.Entities.Player.Spread.PlayerFakeSpreadModel", "Spread", "fake_spread::Spread", 0x10),
    ("Game.Battle.Entities.Player.Spread.PlayerFakeSpreadModel", "Air", "fake_spread::Air", 0x14),
    ("Game.Battle.Entities.Player.Spread.PlayerFakeSpreadModel", "Move", "fake_spread::Move", 0x18),
    ("Models.Models.BattleCharacter.BattleCharacterPrivateModel", "Health", "private_model::Health", 0x1B0),
    ("Models.Models.BattleCharacter.BattleCharacterPrivateModel", "Armor", "private_model::Armor", 0x178),
    ("Models.Models.Health.HealthModel", "_health", "health::Amount", 0x10),
    ("Models.Models.Health.HealthModel", "_isDead", "health::IsDead", 0x18),
    ("Models.Models.Health.HealthModel", "_maxAmount", "health::MaxAmount", 0x20),
    ("Models.Models.WorldEntities.WorldEntitiesModel", "_notSpawnedBattleCharacters", "world::NotSpawnedCharacters", 0x10),
    ("Models.Models.WorldEntities.WorldEntitiesModel", "_battleCharacters", "world::BattleCharacters", 0x18),
    ("Models.Models.WorldEntities.WorldEntitiesModel", "_containers", "world::Containers", 0x20),
    ("Models.Models.WorldEntities.WorldEntitiesModel", "_accessEntities", "world::AccessEntities", 0x28),
    ("Models.Models.WorldEntities.WorldEntitiesModel", "_securityAlarms", "world::SecurityAlarms", 0x30),
    ("Models.Models.WorldEntities.WorldEntitiesModel", "_loots", "world::Loots", 0x38),
    ("Models.Models.WorldEntities.WorldEntitiesModel", "_vehicleLoots", "world::VehicleLoots", 0x40),
    ("Models.Models.WorldEntities.WorldEntitiesModel", "_doors", "world::Doors", 0x48),
    ("Models.Models.WorldEntities.WorldEntitiesModel", "_vehicles", "world::Vehicles", 0x58),
    ("Models.Models.WorldEntities.WorldEntitiesModel", "_cars", "world::Cars", 0x60),
    ("Models.Models.WorldEntities.WorldEntitiesModel", "_helicopters", "world::Helicopters", 0x68),
    ("Models.Models.WorldEntities.WorldEntitiesModel", "_breakableColliderSets", "world::BreakableColliderSets", 0x70),
    ("Models.Models.WorldEntities.WorldEntitiesModel", "_unlockEntities", "world::UnlockEntities", 0x80),
    ("Models.Models.WorldEntities.WorldEntitiesModel", "_breachEntities", "world::BreachEntities", 0x88),
    ("Models.Models.WorldEntities.WorldEntitiesModel", "_staticLootObjects", "world::StaticLoots", 0x98),
    ("Models.Models.WorldEntities.WorldEntitiesModel", "_randomLootObjects", "world::RandomLoots", 0xA0),
    ("Models.Models.WorldEntities.WorldEntitiesModel", "_lootNodes", "world::LootNodes", 0xB0),
    ("Models.Models.WorldEntities.WorldEntitiesModel", "_securityPanels", "world::SecurityPanels", 0xB8),
    ("Models.Models.WorldEntities.WorldEntitiesModel", "_cargos", "world::Cargos", 0xC8),
    ("Models.Models.WorldEntities.WorldEntitiesModel", "_collectionCargos", "world::CollectionCargos", 0xD0),
    ("Models.Models.WorldEntities.WorldEntitiesModel", "_buildingCargos", "world::BuildingCargos", 0x100),
    ("Models.Models.WorldEntities.WorldEntitiesModel", "_safeDoors", "world::SafeDoors", 0x110),
    ("Models.Models.WorldEntities.WorldEntitiesModel", "_storages", "world::Storages", 0x128),
    ("Models.Models.WorldEntities.WorldEntitiesModel", "_storageInventories", "world::StorageInventories", 0x130),
    ("Models.Models.WorldEntities.WorldEntitiesModel", "_plotContainerInventories", "world::PlotContainerInventories", 0x138),
    ("Models.Models.WorldEntities.WorldEntitiesModel", "_plotContainers", "world::PlotContainers", 0x140),
    ("Models.Models.WorldEntities.WorldEntitiesModel", "_shelvings", "world::Shelvings", 0x148),
    ("Models.Models.WorldEntities.WorldEntitiesModel", "_plots", "world::Plots", 0x150),
    ("Models.Models.WorldEntities.WorldEntitiesModel", "_lootInventories", "world::LootInventories", 0x160),
    ("Models.Models.WorldEntities.WorldEntitiesModel", "_containerInventories", "world::ContainerInventories", 0x168),
    ("Models.Models.WorldEntities.WorldEntitiesModel", "_buildings", "world::Buildings", 0x190),
    ("Models.Models.WorldEntities.WorldEntitiesModel", "_grenades", "world::Grenades", 0x1A0),
    ("Models.Models.WorldEntities.WorldEntitiesModel", "_hideObjects", "world::HideObjects", 0x1B0),
    ("Models.Models.WorldEntities.WorldEntitiesModel", "_playerObservers", "world::PlayerObservers", 0x1C0),
    ("Models.Models.WorldEntities.WorldEntitiesModel", "_furnitureLoots", "world::FurnitureLoots", 0x220),
    ("Models.Models.WorldEntities.WorldEntitiesModel", "_furnitureLootEntityInventories", "world::FurnitureLootInventories", 0x228),
    ("Models.Models.WorldEntities.WorldEntitiesModel", "_buildingLoots", "world::BuildingLoots", 0x238),
    ("Models.Models.WorldEntities.WorldEntitiesModel", "_cashLootNodes", "world::CashLoot", 0x268),
    ("Models.Models.WorldEntities.WorldEntitiesModel", "_defenseBlockPublicModels", "world::DefenseBlockPublic", 0x278),
    ("Models.Models.WorldEntities.WorldEntitiesModel", "_defenseBlockPrivateModels", "world::DefenseBlockPrivate", 0x280),
    ("Models.Models.WorldEntities.WorldEntitiesModel", "_lootableObjectGroups", "world::LootableObjectGroups", 0x310),
    ("Models.Models.WorldEntities.WorldEntitiesModel", "_cashWithdrawal", "world::CashWithdrawal", 0x460),
    ("Models.Models.ClientCharacterSlots.ClientCharacterSlotsModel", "_slotByEntityId", "slots::ByEntityId", 0x18),
    ("Models.Models.Vehicle.VehicleModel", "Id", "vehicle::Id", 0x10),
    ("Models.Models.Vehicle.VehicleModel", "Transform", "vehicle::Transform", 0x28),
    ("Models.Models.Vehicle.VehicleModel", "BoundingBoxLocalPositionRelativeToCenterOfMass", "vehicle::BoundsCenter", 0xA8),
    ("Models.Models.Vehicle.VehicleModel", "BoundingBoxSize", "vehicle::Bounds", 0xB4),
    ("Models.Models.Vehicle.VehicleModel", "InertiaBoxShapeSize", "vehicle::InertiaSize", 0xC0),
    ("Models.Models.Vehicle.VehicleModel", "VehicleType", "vehicle::Type", 0x118),
    ("Models.Models.Vehicle.VehicleModel", "VerticalLocalCenterFromGroundInRestRelativeToPivot", "vehicle::CenterFromPivot", 0x14C),
    ("Models.Models.LootModel.LootModel", "Id", "loot::Id", 0x10),
    ("Models.Models.LootModel.LootModel", "Transform", "loot::Transform", 0x20),
    ("Models.Models.LootModel.LootModel", "BagDescription", "loot::Bag", 0x30),
    ("Models.Models.LootModel.LootModel", "Role", "loot::Role", 0x38),
    ("Models.Models.Container.ContainerModel", "Description", "container::Description", 0x18),
    ("Models.Models.Container.ContainerModel", "PositionModel", "container::Position", 0x28),
    ("Models.Models.Container.ContainerModel", "UnlockState", "container::UnlockState", 0x30),
    ("Models.Models.Container.ContainerModel", "IsEmpty", "container::IsEmpty", 0x38),
    ("Models.Models.Container.ContainerModel", "DrillEndTs", "container::DrillEndTs", 0x40),
    ("Models.Models.Plot.PlotModel", "_data", "plot::Data", 0x10),
    ("Models.Models.Plot.PlotModel", "Id", "plot::Id", 0x18),
    ("Models.Models.Plot.PlotModel", "PlotSlotDescription", "plot::Slot", 0x20),
    ("Models.Models.Plot.PlotModel", "PlotDescription", "plot::Description", 0x28),
    ("Models.Models.Plot.PlotModel", "MainBuilding", "plot::MainBuilding", 0x58),
    ("Models.Models.Plot.PlotModel", "MainBuildingLoot", "plot::MainBuildingLoot", 0x80),
    ("Models.Models.Plot.PlotModel", "ClanBonus", "plot::ClanBonus", 0xA8),
    ("Models.Models.Plot.PlotModel", "Vip", "plot::Vip", 0xB0),
    ("Models.Models.Grenade.GrenadeModel", "Description", "grenade::Description", 0x18),
    ("Models.Models.Grenade.GrenadeModel", "Transform", "grenade::Transform", 0x30),
    ("Models.ReferencesProject.Descriptions.Inventory.InventoryItemDescription", "Type", "inventory_item::Type", 0x20),
    ("Models.ReferencesProject.Descriptions.Inventory.InventoryItemDescription", "Rarity", "inventory_item::Rarity", 0x30),
    ("Models.ReferencesProject.Descriptions.Inventory.InventoryItemDescription", "WeaponFunc", "inventory_item::WeaponFunc", 0x50),
    ("Models.ReferencesProject.Descriptions.Inventory.InventoryItemDescription", "BasePriceFunc", "inventory_item::BasePriceFunc", 0x78),
    ("Models.ReferencesProject.Descriptions.Inventory.InventoryItemDescription", "PlotBuildingDecoderDescriptionFunc", "inventory_item::DecoderFunc", 0x180),
    ("Game.Battle.Entities.Player.Camera.PlayerCameraModel", "FollowPosition", "player_camera::FollowPosition", 0x10),
    ("Game.Battle.Entities.Player.Camera.BattleCameraModelUpdater", "_model", "battle_camera_updater::Model", 0x10),
    ("Game.Battle.Entities.Player.Camera.BattleCameraModelUpdater", "_camera", "battle_camera_updater::Camera", 0x18),
    ("Game.Battle.Entities.Player.Camera.BattleCameraModelUpdater", "_characterFollowCamera", "battle_camera_updater::Follow", 0x20),
    ("Cinemachine.CinemachineCore", "sInstance", "cinemachine_core::Instance", 0x8),
    ("Cinemachine.CinemachineCore", "mActiveBrains", "cinemachine_core::ActiveBrains", 0x10),
    ("Cinemachine.CinemachineBrain", "m_UpdateMethod", "cinemachine_brain::UpdateMethod", 0x30),
    ("Cinemachine.CinemachineBrain", "m_OutputCamera", "cinemachine_brain::OutputCamera", 0x50),
    ("Cinemachine.CinemachineBrain", "m_LastFrameUpdated", "cinemachine_brain::LastFrame", 0x78),
    ("Cinemachine.CinemachineBrain", "CurrentCameraState", "cinemachine_brain::CurrentCameraState", 0xB0),
    ("Cinemachine.CameraState", "Lens", "cinemachine_brain::FieldOfView (Lens@0)", 0x0),
    ("Cinemachine.CameraState", "RawPosition", "cinemachine_brain::RawPosition (state+0x4C)", 0x4C),
    ("Cinemachine.CameraState", "RawOrientation", "cinemachine_brain::RawOrientation (state+0x58)", 0x58),
    ("Cinemachine.CameraState", "PositionCorrection", "cinemachine_brain::PositionCorrection (state+0x78)", 0x78),
    ("Cinemachine.CameraState", "OrientationCorrection", "cinemachine_brain::OrientationCorrection (state+0x84)", 0x84),
    ("Cinemachine.LensSettings", "FieldOfView", "cinemachine_brain::FieldOfView", 0x0),
    ("Cinemachine.Cinemachine3rdPersonFollow", "ShoulderOffset", "third_person::ShoulderOffset", 0x34),
    ("Cinemachine.Cinemachine3rdPersonFollow", "VerticalArmLength", "third_person::VerticalArmLength", 0x40),
    ("Cinemachine.Cinemachine3rdPersonFollow", "CameraSide", "third_person::CameraSide", 0x44),
    ("Cinemachine.Cinemachine3rdPersonFollow", "CameraDistance", "third_person::CameraDistance", 0x48),
    ("CinemachineTPSO.CinemachineAimingCameraFollow", "ShoulderOffsetAimed3stPerson", "third_person::ShoulderOffsetAimed3", 0x84),
    ("CinemachineTPSO.CinemachineAimingCameraFollow", "VerticalArmLengthAimed3stPerson", "third_person::VerticalArmLengthAimed3", 0x90),
    ("CinemachineTPSO.CinemachineAimingCameraFollow", "CameraDistanceAimed3stPerson", "third_person::CameraDistanceAimed3", 0x94),
    ("CinemachineTPSO.CinemachineAimingCameraFollow", "ShoulderOffsetAimed1stPerson", "third_person::ShoulderOffsetAimed1", 0x98),
    ("CinemachineTPSO.CinemachineAimingCameraFollow", "VerticalArmLengthAimed1stPerson", "third_person::VerticalArmLengthAimed1", 0xA4),
    ("CinemachineTPSO.CinemachineAimingCameraFollow", "CameraDistanceAimed1stPerson", "third_person::CameraDistanceAimed1", 0xA8),
    ("CinemachineTPSO.CinemachineAimingCameraFollow", "WeightAiming", "third_person::WeightAiming", 0xAC),
    ("CinemachineTPSO.CinemachineAimingCameraFollow", "WeightAimMode", "third_person::WeightAimMode", 0xB0),
    ("CinemachineTPSO.CinemachineAimingCameraFollow", "_cameraDistance", "third_person::CameraDistanceLive", 0xD0),
    ("Start.UnityEntry", "_viewContainer", "unity_entry::ViewContainer", 0x20),
    ("Start.UnityEntry", "_game", "unity_entry::Game", 0x28),
    ("Game.Battle.Views.CharacterCustomizationView", "_customizationColorModel", "customization_view::ColorModel", 0xD0),
    ("Game.Battle.Views.CharacterCustomizationView", "_bodyMaterialInstances", "customization_view::BodyMaterials", 0x88),
    ("Game.Battle.Views.CharacterCustomizationView", "_hairMaterialInstances", "customization_view::HairMaterials", 0x90),
    ("Game.Battle.Views.CharacterCustomizationView", "_beardMaterialInstances", "customization_view::BeardMaterials", 0x80),
    ("Game.Battle.Views.CharacterCustomizationView", "_headMaterialInstance", "customization_view::HeadMaterial", 0xA8),
]


def load_typeinfo(script_path: Path) -> dict[str, int]:
    data = json.loads(script_path.read_text(encoding="utf-8"))
    out: dict[str, int] = {}
    for entry in data.get("ScriptMetadata", []):
        name = entry.get("Name") or ""
        if not name.endswith("_TypeInfo"):
            continue
        addr = entry.get("Address")
        if isinstance(addr, int):
            out[name[: -len("_TypeInfo")]] = addr
    return out


def main() -> int:
    dump_path = Path(sys.argv[1]) if len(sys.argv) > 1 else DUMP
    if not dump_path.is_file():
        print(f"Missing {dump_path} — run scripts/dump-and-sync.ps1 first")
        return 1
    types = parse_dump_cs(str(dump_path))
    ok = diff = miss_t = miss_f = 0
    lines: list[str] = []
    for fq, field, label, exp in CHECKS:
        td = find_type(types, fq)
        if not td:
            miss_t += 1
            lines.append(f"MISSING TYPE | {label} | {fq}")
            continue
        got = field_offset(td, field, types)
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

    getter_ok = getter_diff = getter_miss = 0
    getter_lines: list[str] = []
    for fq, method, exp in GETTERS:
        td = find_type(types, fq)
        if not td:
            getter_miss += 1
            getter_lines.append(f"MISSING TYPE | {fq}.{method}")
            continue
        got = td.methods.get(method)
        if got is None:
            getter_miss += 1
            getter_lines.append(f"MISSING METHOD | {fq}.{method}")
            continue
        if got == exp:
            getter_ok += 1
            getter_lines.append(f"OK | {fq}.{method} | 0x{exp:X}")
        else:
            getter_diff += 1
            getter_lines.append(f"DIFF | {fq}.{method} | header=0x{exp:X} dump=0x{got:X}")

    ti_ok = ti_diff = ti_miss = 0
    ti_lines: list[str] = []
    script = dump_path.parent / "script.json"
    if script.is_file():
        meta = load_typeinfo(script)
        for key, fq in TYPEINFO_MAP.items():
            exp = HEADER_TYPEINFO[key]
            got = meta.get(fq)
            if got is None:
                ti_miss += 1
                ti_lines.append(f"MISSING | typeinfo::{key} | {fq}")
                continue
            if got == exp:
                ti_ok += 1
                ti_lines.append(f"OK | typeinfo::{key} | 0x{exp:X}")
            else:
                ti_diff += 1
                ti_lines.append(f"DIFF | typeinfo::{key} | header=0x{exp:X} dump=0x{got:X}")
    else:
        ti_lines.append("MISSING script.json")

    summary = (
        f"Summary: fields OK={ok} DIFF={diff} missing_type={miss_t} missing_field={miss_f}\n"
        f"Getters: OK={getter_ok} DIFF={getter_diff} missing={getter_miss}\n"
        f"TypeInfo: OK={ti_ok} DIFF={ti_diff} missing={ti_miss}\n"
    )
    print(summary)
    for line in lines:
        print(line)
    print("\n## Getters")
    for line in getter_lines:
        print(line)
    print("\n## TypeInfo")
    for line in ti_lines:
        print(line)

    report = dump_path.parent / "OFFSET_REPORT.md"
    body = [
        "# Offset sync",
        "",
        f"- Dump: `{dump_path.parent.name}` (c01ns Il2CppDumper v39.1, metadata v39)",
        f"- GameAssembly size: `0x8fdc000`",
        f"- dump.cs images: Models.dll typeStart 0, Battle.dll typeStart 5422",
        "",
        "## Summary",
        "",
        f"- Fields: OK={ok} DIFF={diff} missing_type={miss_t} missing_field={miss_f}",
        f"- Getters: OK={getter_ok} DIFF={getter_diff} missing={getter_miss}",
        f"- TypeInfo: OK={ti_ok} DIFF={ti_diff} missing={ti_miss}",
        "",
        "## TypeInfo",
        "",
        *[f"- `{line}`" for line in ti_lines],
        "",
        "## Getters",
        "",
        *[f"- `{line}`" for line in getter_lines],
        "",
        "## Field diffs / misses",
        "",
    ]
    problems = [ln for ln in lines if not ln.startswith("OK |")]
    body.extend([f"- `{ln}`" for ln in problems] if problems else ["- (none)"])
    body.extend(
        [
            "",
            "## Chain (unchanged)",
            "",
            "- `UnityEntry._game 0x28` → `Game.GameModel 0xD0` → `GameModel._battleModel 0x10`",
            "- `BattleModel.BattleEntitiesModel 0x90` / `BattleCameraModel 0x358`",
            "- `BattleEntitiesModel.WorldEntities 0x60` / `Player 0xD0`",
            "- `WorldEntitiesModel._battleCharacters 0x18`",
            "- Shoulder W2S: `BattleCameraModel` Position 0x10 Forward 0x3C",
            "- Silent aim: `LookDirection 0x70` / `ShotArgs 0xF8` / `ShotInterp 0x108`",
            "",
            "Open-generic Dictionary fields still print `0x0` in dump.cs; runtime layout stays `+0x10` buckets / `+0x18` entries / `+0x20` count.",
        ]
    )
    report.write_text("\n".join(body) + "\n", encoding="utf-8")
    print(f"\nWrote {report}")
    return 1 if diff or getter_diff or ti_diff else 0


if __name__ == "__main__":
    raise SystemExit(main())
