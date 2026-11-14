#!/usr/bin/env python3
"""Diff offsets.h expected constants against fresh_offsets.json."""

from __future__ import annotations

import json
import re
from pathlib import Path

DATA = json.loads(Path(__file__).with_name("fresh_offsets.json").read_text(encoding="utf-8"))
TYPES = DATA["types"]


def fields(fq: str) -> dict[str, tuple[str, int]]:
    td = TYPES[fq]
    out = {}
    for f in td["fields"]:
        name = f["name"]
        bare = re.sub(r"^<(.+)>k__BackingField$", r"\1", name)
        out[bare] = (f["type"], int(f["offset"], 16))
        out[name] = (f["type"], int(f["offset"], 16))
    return out


def show(fq: str, names: list[str]) -> None:
    print(f"\n## {fq}")
    if fq not in TYPES:
        print("  MISSING TYPE")
        return
    fs = fields(fq)
    for n in names:
        if n in fs:
            ty, off = fs[n]
            print(f"  {n:40s}  0x{off:X}  {ty}")
        else:
            hits = [k for k in fs if n.lower() in k.lower()]
            print(f"  {n:40s}  MISSING  candidates={hits[:8]}")


CHECKS = {
    "Game.Battle.BattleEntitiesModel": [
        "_models", "_worldEntitiesModel", "_characterSlotModel", "Player",
        "EntityModels", "_entityModels", "Models",
    ],
    "Game.Battle.BattleEntitiesModelUpdater": ["_battleEntitiesModel", "BattleEntities"],
    "Game.Game": ["GameModel", "_gameModel"],
    "Game.GameModel": ["BattleModel", "_battleModel"],
    "Game.Battle.BattleModel": ["BattleEntities", "_battleEntities", "BattleCamera", "_battleCamera"],
    "Game.Battle.Entities.Player.Camera.BattleCameraModel": [
        "Position", "Orientation", "Forward", "Right", "Up",
    ],
    "Models.Models.WorldEntities.WorldEntitiesModel": [
        "NotSpawnedBattleCharacters", "_notSpawnedBattleCharacters",
        "BattleCharacters", "_battleCharacters",
        "Containers", "_containers",
        "Loots", "_loots",
        "StaticLoots", "_staticLoots",
        "RandomLoots", "_randomLoots",
        "LootInventories", "_lootInventories",
        "Vehicles", "_vehicles",
        "Cars", "_cars",
        "Helicopters", "_helicopters",
        "Grenades", "_grenades",
        "CashLootNodes", "_cashLootNodes",
    ],
    "Models.Models.BattleCharacter.BattleCharacterModel": [
        "PrivateModel", "Id", "Transform", "ReplicatedTransform", "LookDirection",
        "IsDead", "SkeletonModel", "IsSprintingModel", "CrouchModel", "ItemInHandsModel",
        "HasArmor", "StateModel", "InVehicleModel", "KnockoutModel",
        "KnockdownLeaveStateModel", "Role",
    ],
    "Models.Models.Knockout.KnockoutModel": ["Phase", "_phase"],
    "Models.Models.Crouch.CrouchModel": ["IsCrouching", "_isCrouching"],
    "Models.Models.InVehicle.InVehicleModel": ["EntityId", "_entityId"],
    "Game.Battle.EntityModel.BattleCharacter.Location.IsHiddenModel": ["IsHidden", "_isHidden"],
    "Models.Models.Skeleton.SkeletonModel": ["_skeletonDescription", "_bonesData"],
    "Models.References.Character.SkeletonConfig": [
        "BonesCount", "BoneDataSize", "BoneDataLocalPositionOrientationOffset",
        "BoneDataModelOrientationOffset", "BoneDataLengthOffset", "BonesHierarchyData",
        "BonesHierarchyMaxChildrenCount", "RootIndex", "LegsIndex", "Spine2Index",
        "Spine3Index", "NeckIndex", "Head1Index", "Head2Index",
    ],
    "Models.Models.TransformModel.TransformModel": ["X", "Y", "Z", "_x", "_y", "_z"],
    "Models.Models.CharacterReplicatedTransform.CharacterReplicatedTransformModel": ["Position", "_position"],
    "Models.Models.LookDirection.LookDirectionModel": ["_value", "Yaw", "Pitch"],
    "Models.Models.BattleCharacterPrivate.BattleCharacterPrivateModel": [
        "Health", "WeaponSpreadArgs", "WeaponSpread", "CheatSprint",
    ],
    "Models.Models.WeaponSpreadArgs.WeaponSpreadArgsModel": [
        "HasSpread", "Spread", "ShotOffsetX", "ShotOffsetY",
    ],
    "Models.Models.Health.HealthModel": ["_health", "_isDead", "_maxAmount"],
    "Game.Battle.EntityModel.Player.PlayerEntityModel": ["Id", "ClientModel", "ServerModel", "PublicId"],
    "Game.Battle.EntityModel.Player.PlayerClientModel": [
        "SetMovement", "Rotation", "CameraModel", "Recoil", "FakeSpread",
    ],
    "Game.Battle.Entities.Player.Recoil.RecoilModel": ["Offset", "_offset"],
    "Game.Battle.Entities.Player.Spread.PlayerFakeSpreadModel": ["Spread", "Air", "Move"],
    "Game.Battle.Entities.Player.Models.PlayerCameraModel": ["FollowPosition"],
    "Game.Battle.UiUpdater.CameraBrainUpdater": ["CameraBrain", "_cameraBrain"],
    "Cinemachine.CinemachineBrain": [
        "m_UpdateMethod", "m_LastFrameUpdated",
    ],
    "Game.Battle.EntityModel.BattleCharacter.Movement.SetMovementModel": ["Position"],
    "Game.Global.Character.CharacterRotationModel": [
        "LookRotation", "LookYaw", "Yaw", "Pitch",
    ],
    "ClientCore.CharacterSlotsModel.ClientCharacterSlotsModel": ["_slotByEntityId"],
    "Models.Models.BattleCharacterSlots.BattleCharacterSlotModel": ["Data", "_data"],
    "Models.Data.BattleCharacterSlotData": ["Nickname", "ClanTag"],
    "Models.Models.Vehicle.VehicleModel": [
        "Id", "Transform", "Type", "BoundsCenter", "Bounds", "InertiaSize", "CenterFromPivot",
    ],
    "Models.Models.VehicleTransform.VehicleTransformModel": [
        "Position", "PivotPosition", "PivotOrientation",
    ],
    "Models.Models.Car.CarModel": ["Id", "Transform"],
    "Models.Models.Helicopter.HelicopterModel": ["Id", "Transform"],
    "Models.Models.LootModel.LootModel": ["Id", "Transform", "Bag", "Role"],
    "Models.Models.LootEntityInventory.LootEntityInventoryModel": ["Id", "Name"],
    "Models.Models.LootObjectModel.Random.RandomLootObjectModel": ["IsTaken", "Item", "Description"],
    "Models.Models.LootObjectModel.Static.StaticLootObjectModel": ["IsTaken", "Description"],
    "Models.Models.Container.ContainerModel": ["Description", "Position", "IsEmpty"],
    "Models.Models.Grenade.GrenadeModel": ["Description", "Transform"],
    "Models.Models.GrenadeTransform.GrenadeTransformModel": ["Position"],
    "Game.Battle.EntityModel.BattleCharacter.Views.BattleCharacterEntityView": [
        "MovementView", "FollowCamera", "LookRotation", "HeadPosition",
    ],
    "Game.Battle.EntityModel.BattleCharacter.CharacterMovementView": ["Transform"],
    "Game.Battle.Entities.Car.CarEntityView": ["Transform"],
    "Content.ContentContext": ["Instance", "_instance"],
}

# print ALL fields for types that need discovery
FULL = [
    "Game.Battle.BattleEntitiesModel",
    "Game.Battle.BattleModel",
    "Game.Battle.Entities.Player.Camera.BattleCameraModel",
    "Models.Models.WorldEntities.WorldEntitiesModel",
]


def main() -> None:
    for fq, names in CHECKS.items():
        show(fq, names)

    print("\n\n===== FULL FIELD DUMPS (selected) =====")
    for fq in FULL:
        print(f"\n## ALL {fq}")
        if fq not in TYPES:
            continue
        for f in TYPES[fq]["fields"]:
            name = f["name"]
            if name.startswith("<") and "k__BackingField" in name:
                name = re.sub(r"^<(.+)>k__BackingField$", r"\1", name)
            print(f"  0x{int(f['offset'],16):<4X}  {name:50s}  {f['type']}")


if __name__ == "__main__":
    main()
