# DriverLoader + CheatLoader

Two executables for single-PC ranked-safe flow.

| EXE | Role | Admin |
|-----|------|-------|
| **DriverLoader.exe** | Map Driver2 (`ranked-safe-map.ps1`), probe | **Yes** |
| **CheatLoader.exe** | Menu: connect driver, launch cheats, read test, title IOCTLs | No* |

\*CheatLoader does not map; run DriverLoader first (elevated).

## Build

```powershell
.\driver2\loader-tool\build-loader.ps1
# or
.\driver2\build.ps1 -BuildLoaders
```

Output (canonical): `C:\...\byovd\driver2\bin\` — mapper, Driver2, loaders, ESP, games, brand.

Optional mirror: `build-loader.ps1 -MirrorToByovdBin`.

## Use

1. **Open lab:** `driver2\open-lab.ps1` or `driver2\bin\open-lab.cmd`
2. **Once per boot (admin):** `driver2\bin\DriverLoader.exe` — map Rob Run / Universal.
3. **CheatLoader.exe** (same folder) — **auto-detects** when Driver2 is already mapped (~1s poll). Click **Rob Run** to start/restart ESP.
3. ESP crashed? Click the tile again — **no remap**, game can stay open. Right-click launcher window to open DriverLoader if driver is missing.

Game art: drop `robrun.jpg` / `unturned.jpg` in `byovd\bin\games\` (or use Steam library cache automatically).

CLI:

```text
DriverLoader.exe -rust-live
DriverLoader.exe -probe
```

Cheat list: edit `loader-tool\cheats.manifest.json` or use menu option “custom EXE path”.
