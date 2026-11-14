# Rob Run DMA

FPGA/VMMDLL overlay for Rob Run. Offsets are **compiled into the exe** (`src/game/offsets.h`); there is no runtime offset file. All writes are DMA (`VMMDLL_MemWrite` / scatter write).

ESP / aim world-to-screen uses **BattleCameraModel** (`BattleModel+0x358`): world position plus Forward/Right/Up of the **shoulder lens**. The pawn look quaternion is not the camera. Cinemachine brain pose is not used (that parked boxes in the sky). ESP is invalid until that shoulder pose is live.

Dump used: `dumps/2026-11-14-v39/` (c01ns Il2CppDumper v39.1, metadata v39, GameAssembly `0x8FDC000`).

## Features (2026-11-14)

### Aimbot

| Feature | Notes |
|---------|--------|
| Head / neck / chest / stomach / pelvis | Named skeleton bones; stomach is chest–pelvis blend |
| FOV circle | Dynamic FOV optional; cyan ring when silent aim is on |
| Linear / EMA / Bezier smoothing | Memory aim and KMBox |
| Target lock | Hold the same player while the aim key is down |
| Target priority | Closest to FOV, closest distance, or lowest health |
| Visible-only | Skips hide-object occupants (no physics raycast in the dump) |
| Prediction | `leadMs` / `leadPx` |
| Team check | Clan tag / `isTeammate` — **not** same role (cops are not teammates of other cops) |
| Memory aim | Writes client yaw/pitch (`CharacterRotationModel`) — camera snaps |
| **Silent aim** | Camera and mouse stay put; see below |
| Triggerbot | Writes `BattleModel.ShotInputModel._isTriggered` and `FireModel._isFiring` |
| No recoil / sway / spread | RecoilModel, PlayerFakeSpreadModel, WeaponSpreadArgsModel |

Hold the aim key on the overlay PC (F2 toggles aim). KMBox is used only when memory aim **and** silent aim are both off.

### Silent aim

Rob Run does not expose Source-style `UserCmd` packets over DMA. `ShotArgsSystem` builds the bullet from `LookDirectionModel` + `ShotInterpolationModel` into `ShotArgsModel.Direction`. Silent aim writes those models and **does not** write client yaw/pitch.

While a target is in FOV it scatter-writes:

| Object | Field | Offset |
|--------|--------|--------|
| `LookDirectionModel` | `_value` / `_yaw` / `_pitch` | `character+0x70` → `0x10` / `0x1C` / `0x20` |
| `LookDirectionInputModel` | `_x` / `_y` / `_contains` | `character+0xB0` → `0x10` / `0x14` / `0x18` |
| `LastLookDirectionModel` | yaw / pitch / direction | `character+0x88` |
| `ReplicatedLookDirectionModel` | `IPrimitiveData<Vector3>` | `character+0x78` → data `+0x10` → value `+0x18` |
| `ShotInterpolationModel._lookDirection` | Vector3 | `character+0x108+0x1C` |
| `ShotArgsModel._direction` | `IPrimitiveData<Vector3>` | `character+0xF8+0x18` → value `+0x18` |

Enable **Aim → Silent aim**. Same FOV, bones, and prediction as memory aim.

### ESP

Boxes, names, distance, health bars, armor bars (`ArmorModel` amount), skeleton, snaplines, team color (clan), cop/robber/settler/prisoner labels, male/female, outfit / hair / beard, police uniform (`CopCustomizeModel`), loot ESP with rarity colors and category filters, hide-object occupants, death bags, cameras, cargo, ATM/safe/vault drills, bases.

Chams: overlay flat / shaded / wireframe / outline body fills, plus DMA `CustomizationColorModel` skin/hair/beard color writes (`CharacterCustomizationView+0xD0`).

Segoe UI Semibold for ESP text.

Visible-only ESP skips cupboard/bin hiders. The dump has no readable physics raycast flag.

### Radar

Minimap, zoom, size, X/Y position (`-1` = default corner), rotate with camera, arrow blips, role colours, big radar pan/zoom/follow.

### Debug camera (freecam)

**F6.** WASD + mouse look, Space/Ctrl up-down, Shift speed. Overlay W2S follows the debug pose. DMA writes `PlayerCameraModel.FollowPosition`, the follow Transform world position, `BattleCameraModel` packed pose, and client yaw/pitch so the game camera tracks when the updater allows it.

## Layout

| Path | Role |
|------|------|
| `src/game/offsets.h` | Il2CPP RVAs + field offsets (sync from dump) |
| `src/game/types.hpp` | `GameEntity`, `CameraData` |
| `src/game/session.hpp` | `RobRunSession` public API |
| `src/game/runtime.cpp` | DMA read chain, scatter cache, camera, silent/memory writes |
| `src/core/memory.cpp` | VMM attach, GameAssembly pin |
| `src/features/*` | ESP, radar, aimbot, HUD |
| `src/render/*` | DX11 overlay, menu, splash |
| `../dumps/2026-11-14-v39/OFFSET_REPORT.md` | TypeInfo RVAs + overlay field notes |

## Build

```powershell
dma\scripts\build-windows.ps1
```

Output: `Development\debug\RobRunDMA.exe`

INSERT opens the menu. Footer: **Transparent** (see-through overlay) or **Fuser** (solid 2nd-PC), plus **Uninject**.

## After a game update

```powershell
dma\scripts\dump-and-sync.ps1
dma\scripts\build-windows.ps1
python dma\scripts\validate_offsets_against_dump.py
```

Dumps live under `Development\dumps\<date>\`.
