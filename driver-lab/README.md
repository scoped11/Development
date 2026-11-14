# Driver lab (Rob Run)

Single place for **kernel driver research**, **BYOVD kit**, **AC bypass lab paths**, and **DMA** — without moving the source trees.

| Path | Contents |
|------|----------|
| [../byovd/](../byovd/) | `mapper_nal`, RwTest/Betest, vehicles, hygiene, replica bypass |
| [../research/](../research/) | Bible + `14_KERNEL_DRIVERS`, `15_BYOVD`, `16_EAC_BE` |
| [../dma/](../dma/) | Live-read overlay (Lane D Host A) |
| [docs/](docs/) | Curated copies / index of bypass & method docs |
| [tools/drivers/](tools/drivers/README.md) | Arsenal + crack MCP/CLI for inbox driver qualification |
| [loader/](loader/) | `DriverLabLoader.exe` — splash + lab map actions |
| [assets/](assets/) | `loading_animation.gif` / `.mp4` (same style as DMA splash) |

## Quick links (canonical sources)

- EAC cookbook: `byovd/docs/14_eac_cookbook.md`
- AC replicas (Rust / R6): `byovd/docs/15_test_ac_replicas.md`
- Lane D: `byovd/docs/12_lane_d_split_trust.md`
- Bypass catalog: `byovd/docs/05_existing_bypass_catalog.md`
- Research: `research/16_EAC_BE_KERNEL_RESEARCH.md`

## Loader

Build:

```powershell
.\build-loader.ps1
```

Run (admin):

```text
bin\DriverLabLoader.exe
```

**Real AC on + replica tests:** Method **TH2** — `byovd/docs/16_timed_ac_harness_method.md` (map only on lab VM).

Does **not** start Rust or R6 — lab map / bypass only. See `loader/README.md`.
