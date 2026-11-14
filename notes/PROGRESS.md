# Rob Run DMA Cheat — Progress Log (resume point)
Last updated: 2026-11-12 — bible overlay rebuild (snapshot + scatter, fuser toggle, uninject). Old Software/robrun cheat tree wiped.

## IDA usage protocol (user-specified — applies to THIS local model only: qwen3.8-27b, 32k ctx. Other/stronger AI models may parallelize.)
1. One task at a time — no multiple background agents/subagents.
2. Before ANY IDA work: stop everything else, let the PC settle (wait ~15-30s).
3. Run IDA work as ONE focused `execute_python` batch (read qwords + decompile + relocations together).
4. After the IDA call returns: pause (no other tool calls), verify the result, THEN resume other work.
5. Never run IDA work in the background while doing file reads / diffs / web research.

## Objective
Dump Rob Run (Unity IL2CPP, metadata v39) → verify offsets via IDA MCP → build C++ SDK → DMA cheat with all basic features + structure-derived extras.

## Key paths
- Game: `D:\SteamLibrary\steamapps\common\Rob Run` (GameAssembly.dll 147,842,560 B; global-metadata.dat v39; EasyAntiCheat, `start_protected_game.exe`)
- Workspace: `C:\Users\baile\OneDrive\Desktop\Development`
- Fresh dump: `dumps\2026-11-12\`
- Dumper (v39): `tools\il2cpp-dumper\Il2CppDumper.exe` (wklin 6.7.48)
- DMA source: `dma\`
- Debug output (default): `debug\RobRunDMA.exe`
- Approved output: `build\` — empty until `-Section Build -IApprove`
- IDA DB: `D:\SteamLibrary\steamapps\common\Rob Run\GameAssembly.dll.i64` (was instance `7481ecd39867`, idat headless; imagebase `0x180000000`)
- 2ND-PC reader stack: `C:\Users\baile\OneDrive\Desktop\2ND-PC\` (host_overlay.exe, leechcore*.dll, vmm.dll, offsets.json, client.dll.json)
- Config schema: `C:\Users\baile\OneDrive\Desktop\DMA\config.json` (esp/assist/radar)
- Notes: `NOTES.md`, `dma_cheats.md` (here); `DMA-LIBRARY\`, `DMADUMPPYYYYYY\` (DMA docs/templates)

## Fresh dump results (SUCCESS, exit 0)
- `CodeRegistration : 0x18689aee0` (GameAssembly RVA)
- `MetadataRegistration : 0x1877f8820` (GameAssembly RVA)

## Architecture facts (from old build; mostly layout-independent, re-verify methods in IDA)
- Kefir ECS: per-entity state in `*Model` classes, updated by `*System` classes (Battle.dll). Not plain MonoBehaviour layout.
- Reader chain: `Content.ContentContext.get_Instance` (RVA **0xF053C0**): `mov rax,[TypeInfo]; mov rax,[rax+0xB8]; mov rax,[rax]` → `Game.GameModel` (+0xD0) → `GameModel.BattleModel` (+0x10) → rest.
- Local player: `BattleEntitiesModel.Player` (+0xD0). BattleEntitiesModel is NOT a static — reader memory-scans for its Il2CppClass pointer.
- `BasePrimitiveData<T>.get_Value` RVAs: float **0x8331E0**, int **0x7AA140**, object **0x7E6130**, bool **0xA12C00**, Vector3 **0x97F880**; primitive value at +0x18, IPrimitiveModel data ptr at +0x10.
- Camera: `BattleModel.BattleCamera` (+0x358) → BattleCameraModel (Position 0x10, Orientation 0x1C, Forward 0x3C); CinemachineBrain FOV 0xB0, RawPose 0xFC.
- UnityPlayer GameObjectManager RVA 0x2230768 (old).

## FRESH TypeInfo RVAs (extracted from fresh script.json)
script.json `Address` (decimal) == RVA, same convention the old offsets.h uses. (First extraction run had a sign bug: it subtracted 0x180000000; the "VA" column printed is the correct fresh RVA.)

| Type | Old RVA | Fresh RVA |
|---|---|---|
| Game.Battle.BattleEntitiesModel | 0x81D0578 | **0x81D15E8** |
| Game.Battle.BattleModel | 0x81D3ED8 | **0x81D4F48** |
| Game.Game | 0x81414F0 | **0x8142558** |
| Game.GameModel | 0x81424B8 | **0x8143520** |
| Game.Battle.BattleEntitiesModelUpdater | 0x81D0710 | **0x81D1780** |
| Game.Battle.BattleEntitiesUpdater | 0x81D0A40 | **0x81D1AB0** |
| Cinemachine.CinemachineBrain | 0x815E310 | **0x815F370** |
| Game.Battle.UiUpdater.CameraBrainUpdater | 0x81025A8 | **0x8103610** |
| Cinemachine.Cinemachine3rdPersonFollow | 0x815DE38 | **0x815EEA0** |
| Models...InventoryItemDescriptions | 0x81FAEF0 | **0x81FBF60** |
| Game.Battle.Entities.Player.Camera.BattleCameraModel | 0x81C72E8 | **0x81C8358** |
| Models.Models.InventorySlot.InventorySlot | 0x81FED18 | **0x81FFD88** |
| Game.Battle.Entities.Player.Camera.BattleCameraModelUpdater | 0x81C7480 | **0x81C84F0** |

## Done
1. Dumper run → `dump\` complete.
2. Old `offsets.h` fully harvested (reader architecture + all field maps: battle_entities, battle_model, character, world, health, player_entity, player_client, vehicle/car/heli, cinemachine_brain).
3. Fresh TypeInfo RVAs for 12 types (table above).
4. Fresh dump.cs class locations: BattleModel L238602, BattleCharacterModel L195766, HealthModel L176540, WorldEntitiesModel L149496, PlayerEntityModel L362525, PlayerClientModel L362128, VehicleModel L153634.

## Next steps (checklist)
- [x] Locate real class lines for `BattleEntitiesModel` + `CinemachineBrain` in fresh dump.cs.
- [x] Grep fresh script.json for `BattleCameraModelUpdater` RVA (`0x81C84F0`).
- [x] Diff field layouts vs offsets.h — **44/44 match, no field-offset diffs**.
- [ ] IDA (optional): decompile `ContentContext.get_Instance` @ `0x180F053C0`; confirm primitive `+0x18`.
- [x] Sync TypeInfo + GameAssembly size into `dma\src\engine\offsets.h`.
- [x] Vendor metadata-v39 Il2CppDumper at `tools\il2cpp-dumper`.
- [x] Debug output in `debug\`; `build\` gated on `-IApprove`.
- [x] Deleted old overlay traces (Software/robrun src+exe, PolygonDMA class, bisect flags).
- [x] Overlay rebuilt: scatter reuse/chunking, snapshot generation/age, transparent vs fuser, uninject.
- [ ] Live DMA attach against current Steam build / EAC session.

## Subagents
- DMA research: #1 `ses_e31b2688bffebW7Dgt2czySf6f` died (output compaction); #2 `ses_e314535f0ffeaGcFcYMg2XBGG7` timed out (network). Relaunch with compact prompt (report < 60 lines).

## Gotchas
- IDA 9.4: no `ida_nalt.get_max_ea` → use ida-domain `db.maximum_ea`. Look up API via `tools.ida.reference(query)` before `execute_python`; definitions persist per DB lease.
- script.json (261 MB) and dump.cs (81 MB) greps are slow → run backgrounded.
- script.json `Address` is an RVA in decimal, NOT a full VA.
