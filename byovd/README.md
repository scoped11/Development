# BYOVD kit (lab)

**Read [START_HERE.md](START_HERE.md) first.**

Kernel map research: Intel/NAL-style **vehicle** + **Betest** payload. Default path is **map + process/kernel R/W**, not automatic anti-cheat neutralization.

### Lane D (Rust / EAC blocked BYOVD on game PC)

**driver3** = Lane D SDK: **Host A** DMA (`../dma/`) + **Host B** `driver2/` on a no-AC VM; sync `build_id` in `docs/lab_manifest.json`. See [driver3/README.md](driver3/README.md) · [docs/12_lane_d_split_trust.md](docs/12_lane_d_split_trust.md) · `.\driver3\lane-gate.ps1 -Game rust`.

## Reading order

| # | Doc | Question it answers |
|---|---|---|
| 1 | [docs/01_read_write.md](docs/01_read_write.md) | How do I open the device and R/W memory? |
| 2 | [docs/02_architecture.md](docs/02_architecture.md) | How does mapping / existing bypass IOCTLs work? |
| 3 | [docs/03_research.md](docs/03_research.md) | EAC/BE research + Lab I |
| 4 | [docs/04_detection_surfaces.md](docs/04_detection_surfaces.md) | What’s still visible? What *not* to add yet? |
| — | [docs/12_lane_d_split_trust.md](docs/12_lane_d_split_trust.md) | Rust/EAC: DMA + lab BYOVD split (not hide on game PC) |
| — | [docs/13_greenfield_eac_rust.md](docs/13_greenfield_eac_rust.md) | Greenfield driver vs non-driver for Rust |
| — | [docs/14_eac_cookbook.md](docs/14_eac_cookbook.md) | **EAC cookbook** — recipes + lanes |

Bypasses were **not removed** — they are **opt-in** (`--eac`, `--battleye`, `--ac-preempt`, `--full`). See doc 2.

## Layout

```
byovd/
  START_HERE.md
  README.md
  build.ps1
  run-map-stable.cmd
  bin/                 mapper_nal.exe, Betest.sys
  include/             betest_ioctl.h  (usermode R/W contract)
  examples/            process_rw.cpp
  src/mapper/          mapper_nal + primitives
  src/driver/          BetestDriver.c
  vehicles/            drop NalDrvIntel.sys here
  docs/01–04           kit docs
  docs/research/       bible ch. 14–16 copies
  reference/kdmapper/  upstream comparison
  tools/smoke_nal.cpp
```

## Build / map

```powershell
.\build.ps1
# put vehicle .sys in vehicles\ or bin\
.\run-map-stable.cmd
```

Then `examples\process_rw.cpp` (see doc 1). Log: `%TEMP%\pf_diag.tmp`.

Requires VS 2022 + WDK. Vehicle `.sys` files are **not** shipped.

## Sync

Sources originated from `Downloads\masterlist-master\masterlist-master`. Prefer editing `byovd\src\` and `.\build.ps1`.
