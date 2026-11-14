# Arena Breakout Infinite (ACE)

Profile: `arenabreakout` · AC: **ACE** · Profile file: `driver2/profiles/ace-arenabreakout.json` · Dump: `driver2/dumps/arenabreakout-ace_*`

## Method

Same **ACE family** as Delta Force but **without Delta-specific** `0x84E` emphasis in pass criteria — strengthen **`0x846`** + `0x86A` in replica docs. **Lane D** split identical: DMA ranked, driver2 on VM.

## Route

**Ranked (Host A):** `SGuard` + `ABInfinite` → `dma/`

**Lab (Host B):**

```text
--ac-replica arenabreakout --driver2
  → ACE 0x846 / 0x86A in bypass log
```

```powershell
driver3\host-b.ps1 -Profile arenabreakout -Execute -ForceMap
```

## Read / write

| Host | Route | API |
|------|-------|-----|
| **A** | DMA | Overlay R/W on game process |
| **B** | Driver2 | `0x870` + `0x805`/`0x806`; optional `Driver2AceStrengthen` in lab |

## Tiers

| Ladder | Level | Notes |
|--------|-------|-------|
| **T0** | ACE running | DMA on game PC |
| **D2** | `kernel_proof_arenabreakout.json` | VM |
| **D3** | `-IncludeAce` gate | VM |
| **D4** | Live kernel | **Not claimed** |

Aliases: `abi`, `arena`, `ace-arenabreakout`.
