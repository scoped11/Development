# Delta Force (AntiCheat Expert)

Profile: `deltaforce` · AC: **ACE** · Profile file: `driver2/profiles/ace-deltaforce.json` · Dump: `driver2/dumps/deltaforce-ace_*`

## Method

**ACE + SGuard** stack (`ACE-GAME.sys`, `ACE-BASE.sys`, `SGuardSvc64`). Split-trust: **DMA Host A** for live; **ACE IOCTL matrix** on VM via `--ace 3f --delta 1f` style flags in profile.

Static: IDA target `ACE-GAME.sys` in dump manifest — `driver2/ACE_INTEGRATION.md`.

## Route

**Ranked (Host A):**

```text
SGuard + DeltaForceClient
  → dma/
  ✗ no BYOVD on Tier 0 session
```

**Lab (Host B):**

```text
--ac-replica deltaforce --driver2
  → 0x846 ACE strengthen, 0x84E Delta harden, 0x86A (profile)
```

```powershell
driver3\host-b.ps1 -Profile deltaforce -Execute -ForceMap -Inspector
# developer gate: -IncludeAce (auto via host-b flags)
```

## Read / write

| Host | Route | API |
|------|-------|-----|
| **A** | DMA | Live reads; prefer game-bundle offsets / `ACE-GAME` RE dump |
| **B** | Driver2 | `Driver2AceStrengthen` / `Driver2AceDelta` helpers in `driver2_api.h` for lab IOCTL; game mem still **0x805/0x806** |
| Map-time | `0x846`, `0x84E`, `0x86A` | pf_diag / mapper log |

## Tiers

| Ladder | Level | Notes |
|--------|-------|-------|
| **T0** | ACE usermode/kernel active | Host A |
| **D2** | Host B ACE profile map | `verify-pf-diag` delta set |
| **D3i** | Inspector | VM `-IncludeAce` |
| **D4** | Live ACE + mapper | **Not claimed** |

Aliases: `df`, `delta`, `ace-deltaforce`.
