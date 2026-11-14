# Start here

Read in this order. This kit is for **isolated lab** kernel mapping and process/kernel R/W. It is not a claim that EAC/BattlEye are “beaten.”

**Playing ranked (live AC)?** Start with **`.\driver3\ranked.ps1 -Game rust -Execute -SyncBuildId`** ([driver3/RANKED.md](driver3/RANKED.md)).

**Rust / EAC blocks BYOVD on the game PC?** Use **driver3** (Lane D SDK): [driver3/README.md](driver3/README.md) · [docs/12_lane_d_split_trust.md](docs/12_lane_d_split_trust.md) — **do not map on that session**; **Host A** = DMA live reads · **Host B** = `driver2` on VM; sync **`build_id`**. Gate: `.\driver3\lane-gate.ps1 -Game rust` (or elevated `.\scripts\lane-d-preflight.ps1 -Game rust`). Greenfield: [docs/13_greenfield_eac_rust.md](docs/13_greenfield_eac_rust.md).

1. **[docs/01_read_write.md](docs/01_read_write.md)** — how to talk to the mapped driver after map (device names, session auth, process + kernel R/W).
2. **[docs/02_architecture.md](docs/02_architecture.md)** — how the BYOVD map and existing bypass *surface* work (what code does, when it runs).
3. **[docs/03_research.md](docs/03_research.md)** — EAC/BE threat model, Lab I, bible chapters.
4. **[docs/04_detection_surfaces.md](docs/04_detection_surfaces.md)** — what still looks anomalous vs kdmapper-class literature, residual checks, what *not* to stack on the default path.
5. **[docs/05_existing_bypass_catalog.md](docs/05_existing_bypass_catalog.md)** — full inventory of bypass IOCTLs + mapper flags **you already have** (no new code required to “enable more”).
6. **[docs/06_better_research_method.md](docs/06_better_research_method.md)** — recommended phased method (stability → Lab I → one boundary → optional IOCTL experiments).
7. **[docs/11_ac_block_vs_hide_plan.md](docs/11_ac_block_vs_hide_plan.md)** — Tier 0/1 when AC blocks the vehicle (`--ac-safe`, preflight).
8. **[docs/12_lane_d_split_trust.md](docs/12_lane_d_split_trust.md)** — **Lane D** (DMA + BYOVD split, build_id sync).
9. **[docs/13_greenfield_eac_rust.md](docs/13_greenfield_eac_rust.md)** — starting fresh on Rust + EAC.
10. **[docs/14_eac_cookbook.md](docs/14_eac_cookbook.md)** — **EAC cookbook**: method catalog + recipes (R1–R6, gates, cheat sheet).
11. **[docs/15_test_ac_replicas.md](docs/15_test_ac_replicas.md)** — **Rust / R6 AC replicas** in your test env (`map-ac-replica-lab.ps1`).
12. **[docs/16_timed_ac_harness_method.md](docs/16_timed_ac_harness_method.md)** — **TH2** dual-host: real AC running + replica lab (no kernel on live).

Binaries: `bin\mapper_nal.exe`, `bin\Betest.sys`, **`bin\RwTest.sys`** (minimal test driver). See **[docs/07_test_driver.md](docs/07_test_driver.md)**. Rebuild: `.\build.ps1`.
