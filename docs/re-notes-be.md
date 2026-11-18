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

## Next RE targets (concrete — from [eac-be-research-index.md](eac-be-research-index.md))

| Priority | Target | REA entry points | Public xref (hypothesis only) |
|----------|--------|------------------|-------------------------------|
| P0 | `BEDaisy.sys` | `open_binary` → `inspect_pe_load_config` → `binary_overview` | experienceds/battleye-re |
| P1 | Filter stack | xrefs `FltRegisterFilter`, `FltStartFiltering` | H-BE-01; Aki2k/BEDaisy (aged) |
| P1 | Driver init | `DriverEntry` / unload (`DriverUnload`) — stability only | brandenbailey23 unload fault study |
| P2 | IOCTL dispatch | `IRP_MJ_DEVICE_CONTROL` handler; device object names via `search_strings` | experienceds + Aki2k — **build-specific** |
| P2 | Callbacks | notify routines, handle stripping patterns | ObRegisterCallbacks / process notify xrefs |
| P3 | `BEService.exe` | static imports + RPC/ALPC surfaces; **no live attach** | gmh5225/battleye-decryption (wire format hints) |
| P3 | `BEClient.dll` | export table, thin shim vs service split | H-BE-03; tym32167/arma3beclient (UM only) |
| P4 | Version matrix | file versions beside game season | H-BE-04 — lab manifest on share |

**Do not promote IOCTL values from GitHub into Verified without guest REA evidence.**

## Next steps (VM + REA)

1. Guest Ghidra pipeline: [rea-vm-smoke-test.md](rea-vm-smoke-test.md).
2. Stage `BEDaisy.sys` (+ optional `BEService.exe`) under `ac-captures-host\<date>\`; local copy for Ghidra.
3. REA P0–P2 table; name minifilter callbacks if imports present.
4. Document unload / lifecycle — [bedaisy-unload-fault-0x7e](https://github.com/brandenbailey23/bedaisy-unload-fault-0x7e) as **stability** research only.
5. Protocol: read-only notes from `battleye-decryption` — no live game traffic.

## Dead ends

| Date | Item | Reason |
|------|------|--------|
| — | — | — |