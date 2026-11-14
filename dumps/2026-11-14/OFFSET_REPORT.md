# Offset sync

- Generated: 2026-11-14 UTC
- GameAssembly size: `0x8fdc000`

## TypeInfo updates

- `BattleEntitiesModel = 0x81D46B0  (Game.Battle.BattleEntitiesModel)`
- `BattleEntitiesModelUpdater = 0x81D4848  (Game.Battle.BattleEntitiesModelUpdater)`
- `BattleEntitiesUpdater = 0x81D4B78  (Game.Battle.BattleEntitiesUpdater)`
- `CinemachineBrain = 0x8162410  (Cinemachine.CinemachineBrain)`
- `CameraBrainUpdater = 0x81066B0  (Game.Battle.UiUpdater.CameraBrainUpdater)`
- `Cinemachine3rdPersonFollow = 0x8161F40  (Cinemachine.Cinemachine3rdPersonFollow)`
- `Game = 0x81455F8  (Game.Game)`
- `GameModel = 0x81465C0  (Game.GameModel)`
- `BattleModel = 0x81D8010  (Game.Battle.BattleModel)`
- `BattleCameraModel = 0x81CB420  (Game.Battle.Entities.Player.Camera.BattleCameraModel)`
- `BattleCameraModelUpdater = 0x81CB5B8  (Game.Battle.Entities.Player.Camera.BattleCameraModelUpdater)`

## Missing

- (none)

**Manual:** field offsets in `offsets.h` — diff `dump.cs` for layout changes.
Skeleton (`BattleCharacterModel+0x138`, `SkeletonConfig` chain) unchanged in this build.

## Overlay field notes (compiled into `offsets.h`)

W2S camera: `BattleModel.BattleCamera = 0x358` → `BattleCameraModel` Position `0x10`, Orientation `0x1C`, Forward `0x3C`, Right `0x48`, Up `0x54`, DistanceToFollowTarget `0x60`. Updater copies the Unity Transform every frame.

Silent aim (does **not** write client yaw/pitch). `ShotArgsSystem` uses look + interpolation:

| `BattleCharacterModel` | Offset | Type |
|------------------------|--------|------|
| LookDirection | 0x70 | `LookDirectionModel` (`_value` 0x10, `_yaw` 0x1C, `_pitch` 0x20) |
| ReplicatedLookDirection | 0x78 | `ReplicatedLookDirectionModel` (`IPrimitiveData<Vector3>` at 0x10, value at +0x18) |
| LastLookDirection | 0x88 | `LastLookDirectionModel` (Yaw 0x10, Pitch 0x14, Direction 0x18) |
| LookDirectionInput | 0xB0 | `LookDirectionInputModel` (`_x` 0x10, `_y` 0x14, `_contains` 0x18) |
| FireModel | 0xC0 | `_isFiring` `IPrimitiveData<bool>` at 0x10 |
| ShotArgsModel | 0xF8 | `_direction` `IPrimitiveData<Vector3>` at 0x18 |
| ShotInterpolationModel | 0x108 | `_lookDirection` Vector3 at 0x1C |

Triggerbot: `BattleModel.ShotInputModel = 0x110` (`Game.Battle.Inputs.InputModel._isTriggered` at 0x10, Mode at 0x18).

Freecam: `PlayerCameraModel.FollowPosition = 0x10`; `BattleCharacterEntityView.FollowCamera = 0x58` native Transform world position `+0x90`; `Cinemachine3rdPersonFollow.CameraDistance = 0x48`.

Material chams: `BattleCharacterEntityView.CustomizationView = 0xB8` → `CharacterCustomizationView._customizationColorModel = 0xD0` (`SkinColor` 0x10, `HairColor` 0x20, `BeardColor` 0xA0).

Armor bar: `BattleCharacterPrivateModel.Armor = 0x178` → `ArmorModel._amountModel = 0x20` → `AmmoModel._data = 0x10`.

Team: teammates are clan tag / plot owner public id, not `PlayerRoleDescription`. Cops vs robbers come from Role id (`character+0x208`).