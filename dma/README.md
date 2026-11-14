# Rob Run DMA

FPGA/VMMDLL overlay for Rob Run. Offsets are **compiled into the exe** (`src/game/offsets.h`); there is no runtime offset file.

ESP / aim world-to-screen uses **BattleCameraModel** (`BattleModel+0x358`): world position plus Forward/Right/Up of the **shoulder lens**. The pawn look quaternion is not the camera. Cinemachine brain pose is not used (that parked boxes in the sky). ESP is invalid until that shoulder pose is live.

## Layout

| Path | Role |
|------|------|
| `src/game/offsets.h` | Il2CPP RVAs + field offsets (sync from dump) |
| `src/game/types.hpp` | `GameEntity`, `CameraData` |
| `src/game/session.hpp` | `RobRunSession` public API |
| `src/game/runtime.cpp` | DMA read chain, scatter cache, camera |
| `src/core/memory.cpp` | VMM attach, GameAssembly pin |
| `src/features/*` | ESP, radar, aimbot |
| `src/render/*` | DX11 overlay, menu, splash |

## Build

```powershell
dma\scripts\build-windows.ps1
```

Output: `Development\debug\RobRunDMA.exe`

## After a game update

```powershell
dma\scripts\dump-and-sync.ps1
dma\scripts\build-windows.ps1
python dma\scripts\validate_offsets_against_dump.py
```

Dumps live under `Development\dumps\<date>\`.