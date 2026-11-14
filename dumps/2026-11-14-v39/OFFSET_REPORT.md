# Offset sync

- Dump: `2026-11-14-v39` (c01ns Il2CppDumper v39.1, metadata v39)
- GameAssembly size: `0x8fdc000`
- dump.cs images: Models.dll typeStart 0, Battle.dll typeStart 5422

## Summary

- Fields: OK=201 DIFF=0 missing_type=0 missing_field=0
- Getters: OK=5 DIFF=0 missing=0
- TypeInfo: OK=12 DIFF=0 missing=0

## TypeInfo

- `OK | typeinfo::BattleEntitiesModel | 0x81D46B0`
- `OK | typeinfo::BattleEntitiesModelUpdater | 0x81D4848`
- `OK | typeinfo::BattleEntitiesUpdater | 0x81D4B78`
- `OK | typeinfo::CinemachineBrain | 0x8162410`
- `OK | typeinfo::CinemachineCore | 0x8162748`
- `OK | typeinfo::CameraBrainUpdater | 0x81066B0`
- `OK | typeinfo::Cinemachine3rdPersonFollow | 0x8161F40`
- `OK | typeinfo::Game | 0x81455F8`
- `OK | typeinfo::GameModel | 0x81465C0`
- `OK | typeinfo::BattleModel | 0x81D8010`
- `OK | typeinfo::BattleCameraModel | 0x81CB420`
- `OK | typeinfo::BattleCameraModelUpdater | 0x81CB5B8`

## Getters

- `OK | Game.Game.get_GameModel | 0x904DA0`
- `OK | Game.GameModel.Game.IDevelopGameModel.get_BattleModel | 0x7EB6B0`
- `OK | Game.Battle.BattleModel.get_BattleEntitiesModel | 0x7A9F90`
- `OK | Game.Battle.BattleModel.get_BattleCameraModel | 0x929CA0`
- `OK | Start.UnityEntry.get_Game | 0x7A9000`

## Field diffs / misses

- (none)

## Chain (unchanged)

- `UnityEntry._game 0x28` → `Game.GameModel 0xD0` → `GameModel._battleModel 0x10`
- `BattleModel.BattleEntitiesModel 0x90` / `BattleCameraModel 0x358`
- `BattleEntitiesModel.WorldEntities 0x60` / `Player 0xD0`
- `WorldEntitiesModel._battleCharacters 0x18`
- Shoulder W2S: `BattleCameraModel` Position 0x10 Forward 0x3C
- Silent aim: `LookDirection 0x70` / `ShotArgs 0xF8` / `ShotInterp 0x108`

Open-generic Dictionary fields still print `0x0` in dump.cs; runtime layout stays `+0x10` buckets / `+0x18` entries / `+0x20` count.
