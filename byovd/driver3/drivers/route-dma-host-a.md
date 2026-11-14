# DMA — Host A live read route (all titles)

**Method:** **Lane D / driver3 Host A** — physical or device DMA into game PC RAM **without** loading BYOVD on the same session as live EAC/BE/ACE. Offsets come from dumps/Il2Cpp; `build_id` must match `docs/lab_manifest.json`.

**Route:**

```text
Game PC (AC on)
  RobRunDMA.exe / LeechCore → target process physical pages
  ← offsets.h / dump-derived RVAs (dma/scripts, ../dumps/)
  NO \\.\RTCore64 map on this host when Tier 0
```

| Step | Command |
|------|---------|
| Gate | `driver3\lane-gate.ps1 -Game <profile>` |
| Build | `dma\scripts\build-windows.ps1` |
| Play ranked | `driver3\ranked-play.ps1 -Profile <profile> -Execute` |
| Sync | `driver3\sync-lab-manifest.ps1 -Game <profile> -BuildId <id> -DmaStatus ok` |

**Read / write:**

| Op | How |
|----|-----|
| **Read** | Overlay/engine reads guest VA via DMA + your page tables / CR3 walk (project in `dma/`) |
| **Write** | Same path if your build enables it — **higher** behavioral risk on live AC |
| **Kernel** | **Not** via DMA usermode IOCTL; kernel proof stays on Host B |

Detail: `dma/README.md`, cookbook **R1** in `docs/14_eac_cookbook.md`.

**Tiers:**

| ID | Bar |
|----|-----|
| **T0** | AC on + BYOVD blocked → **only** this route on game PC |
| **D1** | DMA built + `lane_d.build_id` set |
| **D4** | Live ranked — **DMA R/W only**; kernel map **not claimed** |
