# RE notes — BattlEye (BE)

**Binary focus:** `BEDaisy.sys` (kernel), `BEService.exe`, `BEClient.dll` (game-dependent).  
**Evidence rule:** per [rea-ai-reversing](../../Reversing/.cursor/skills/rea-ai-reversing/SKILL.md).

## Verified

| Claim | Evidence | Build / hash |
|-------|----------|--------------|
| *(empty)* | — | SHA256 `BEDaisy.sys` from owned install |

**Public reference only:**

- `Aki2k/BEDaisy` and `experienceds/battleye-re` document historical IOCTL and dispatch shapes — **build-specific**; must be re-proven on your sample.

## Hypothesis

| ID | Hypothesis | Confidence | Falsify with |
|----|------------|------------|--------------|
| H-BE-01 | BEDaisy uses filter driver / callback stack consistent with minifilter unload paths (see unload fault repo). | Medium | REA on unload routines + flt symbols |
| H-BE-02 | BEService↔BEDaisy channel is encrypted multi-layer framing (per public decrypt repos). | Low | Static analysis of BEService only — no live tap |
| H-BE-03 | BEClient in usermode is thin RPC shim; policy enforcement split between service and kernel. | Medium | Import table + RPC/decompile split |
| H-BE-04 | Driver image updates independently of game client; hash discipline required per season. | High | Version matrix in lab manifest |

## IOCTLs / devices

| Device / symlink | IOCTL (hex) | Direction | Status | Notes |
|------------------|------------|-----------|--------|-------|
| *(stub)* | — | — | Unverified | Start from experienceds/battleye-re **as hypothesis** |

## Components

| Layer | Artifact | Role |
|-------|----------|------|
| Kernel | `BEDaisy.sys` | Monitoring, callbacks, enforcement hooks |
| Service | `BEService.exe` | Session broker, comms to kernel |
| Client | `BEClient.dll` | In-process integration |

## Next steps (VM + REA)

1. Stage `BEDaisy.sys` (+ optional `BEService.exe`) from an **offline** capture on the shared folder.
2. REA: PE load config, driver init, major function table.
3. Map minifilter registration if present (`FltRegisterFilter` xrefs).
4. Document unload / lifecycle in notes — tie to [bedaisy-unload-fault-0x7e](https://github.com/brandenbailey23/bedaisy-unload-fault-0x7e) only as **stability** research.
5. For protocol shape, read-only review of `battleye-decryption` — do not attach to live games.

## Dead ends

| Date | Item | Reason |
|------|------|--------|
| — | — | — |