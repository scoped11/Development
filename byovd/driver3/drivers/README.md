# Driver routes — method, R/W, tiers

One page per **title profile** plus shared **vehicle**, **payload**, and **DMA** routes. Catalog: `../games.json`.

| Profile | AC | Doc | Ranked live R/W | Lab kernel R/W |
|---------|-----|-----|-----------------|----------------|
| `rust` | EAC EOS | [rust.md](rust.md) | DMA (Host A) | driver2 Host B |
| `r6` | BattlEye | [r6.md](r6.md) | DMA (Host A) | driver2 Host B |
| `arksa` | BattlEye | [arksa.md](arksa.md) | DMA (Host A) | driver2 Host B |
| `deltaforce` | ACE | [deltaforce.md](deltaforce.md) | DMA (Host A) | driver2 Host B |
| `arenabreakout` | ACE | [arenabreakout.md](arenabreakout.md) | DMA (Host A) | driver2 Host B |
| `universal` | none | [universal.md](universal.md) | N/A | driver2 Host B |
| BYOVD vehicles | — | [vehicles.md](vehicles.md) | **Never** on Tier 0 game PC | Map primitive only |
| Inbox qualify | — | [driver-lab/tools/drivers](../../../driver-lab/tools/drivers/README.md) | **Never** | `arsenal_run` / `crack_*` |
| `Driver2.sys` | — | [payload-driver2.md](payload-driver2.md) | **Not claimed** | Primary lab comm |
| DMA overlay | — | [route-dma-host-a.md](route-dma-host-a.md) | **Host A default** | Not used on VM |

**Tier keys (use together):**

| Ladder | IDs | Meaning |
|--------|-----|---------|
| AC block/hide | **T0–T3** | `docs/11_ac_block_vs_hide_plan.md` — can vehicle open under AC? |
| driver3 split-trust | **D0–D3i** | `../TIERS.md` — Host A/B + manifest |
| driver2 SDK | **D0–D4** | `driver2/DEVELOPER_APPROVAL.md` — map/ioctl/inspector |
| Replica lab | **L1–L3** | `docs/15_test_ac_replicas.md` — L3 = live ranked = Lane D only |

```powershell
.\driver3\lane-gate.ps1 -Game <profile>   # writes driver3_gate.json + tier hint (exit 2 = T0 / D Host A)
```
