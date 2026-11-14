# ARK Survival Ascended (BattlEye)

Profile: `arksa` · AC: **BattlEye** · Profile file: `driver2/profiles/be-arksa.json` · Dump: shares BE bundle (`r6-battleye_*` until title-specific capture)

## Method

**BattlEye family** — same IOCTL lab bar as R6 (`0x852`), different usermode anchors (`ShooterGame`, `BEService`). **Lane D:** DMA live on ranked host; `be-arksa` profile on VM.

## Route

**Ranked (Host A):** `dma/` + `driver3\ranked-play.ps1 -Profile arksa`

**Lab (Host B):**

```text
--ac-replica arksa --driver2 --battleye 1f
  → same BE 0x852 gate as r6 in verify-pf-diag
```

```powershell
driver3\host-b.ps1 -Profile arksa -Execute -ForceMap
```

## Read / write

| Host | Route | API |
|------|-------|-----|
| **A** | DMA | Read `ShooterGame` / ARK process memory via overlay |
| **B** | Driver2 | Standard `0x805`/`0x806` after `0x870`; PID from game exe |

## Tiers

| Ladder | Level | Notes |
|--------|-------|-------|
| **T0** | BE live on game PC | DMA only |
| **D2** | `kernel_proof_arksa.json` | Host B |
| **D3** | Developer gate | Use `-IncludeBe` |
| **D4** | Live kernel | **Not claimed** |

Aliases: `ark`, `be-arksa`, `be-ark`.
