# Rob Run workspace

Everything for this title lives here: research, the IL2CPP dumper, dumps, DMA source, and two output sections.

| Path | What it is |
|------|------------|
| `research/` | Architecture notes and the game-analysis bible |
| `notes/` | Working log (`NOTES.md`, `PROGRESS.md`) |
| `dumps/` | Il2CppDumper output (`dump.cs`, `script.json`, DummyDll) |
| `tools/il2cpp-dumper/` | wklin **v6.7.48** (metadata **v39**) |
| `dma/` | Overlay source, CMake, LeechCore/VMM libs |
| `byovd/` | BYOVD lab map (RwTest); **Lane D**: live reads here + kernel proof on no-AC VM — [byovd/docs/12_lane_d_split_trust.md](byovd/docs/12_lane_d_split_trust.md) |
| `driver-lab/` | Index + **DriverLabLoader.exe** (splash + lab map actions) — [driver-lab/README.md](driver-lab/README.md) |
| **`debug/`** | Default compile + run output. Always safe to write. |
| **`build/`** | Approved-only binary. Stays empty until you say so. |

## Debug (default)

```powershell
.\dma\scripts\build-windows.ps1
.\debug\RobRunDMA.exe
```

INSERT opens the menu. Footer: **Transparent** (see-through overlay) or **Fuser** (solid 2nd-PC), plus **Uninject**.

Dump + offset sync also rebuilds **debug** only:

```powershell
.\dma\scripts\dump-and-sync.ps1
```

## Build (approval required)

Nothing is written under `build/` unless you pass both flags:

```powershell
.\dma\scripts\build-windows.ps1 -Section Build -IApprove
```

CMake also refuses to configure `build/` without `-DROBRUN_I_APPROVE=ON`.
