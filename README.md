# Development workspace

Shared VirtualBox folder: host `C:\Users\bj284\Downloads\TraeTesting\cursor\Development` → guest `D:\hostdev` (see [docs/vm-checklist.md](docs/vm-checklist.md)). **Run REA, Ghidra, and kernel RE on the VM guest only** unless a task explicitly says host.

## EAC / BattlEye research vault (PFAB-5)

Defensive static RE index — no live bypass, no cheat payloads.

| Path | Purpose |
|------|---------|
| [docs/eac-be-research-index.md](docs/eac-be-research-index.md) | Curated GitHub + official sources |
| [docs/github-sources.md](docs/github-sources.md) | Search queries and raw top hits |
| [docs/re-notes-eac.md](docs/re-notes-eac.md) | Living notes (EAC): Verified / Hypothesis / IOCTLs |
| [docs/re-notes-be.md](docs/re-notes-be.md) | Living notes (BattlEye) |
| [docs/vm-checklist.md](docs/vm-checklist.md) | Shared folder, sync, REA+MCP on guest |

REA workflow: [Reversing/.cursor/skills/rea-ai-reversing/SKILL.md](../Reversing/.cursor/skills/rea-ai-reversing/SKILL.md) · [rea.today/get-started](https://rea.today/get-started) · Related lab map: [byovd/docs/research/16_EAC_BE_KERNEL_RESEARCH.md](byovd/docs/research/16_EAC_BE_KERNEL_RESEARCH.md).

---

## Rob Run (game) workspace

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
