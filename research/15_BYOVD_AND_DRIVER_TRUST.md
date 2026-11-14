# BYOVD and driver trust boundaries

**Edition:** 12 November 2026 | Companion to [14_KERNEL_DRIVERS.md](14_KERNEL_DRIVERS.md), [16_EAC_BE_KERNEL_RESEARCH.md](16_EAC_BE_KERNEL_RESEARCH.md). Documents platform defenses and **lab stability** constraints. Not a list of exploitable drivers or bypass steps.

**Research intent:** kernel BYOVD and mapper hygiene in this repo are studied primarily to understand **Easy Anti-Cheat / BattlEye-class** kernel monitoring surfaces (load forensics, module provenance, execution region), on isolated lab VMs—not to validate bypasses on live protected titles.

## Definition

**BYOVD** — an attacker (or research tool) loads a **legitimate signed** kernel driver that contains a vulnerability (unsafe IOCTL, arbitrary physical/memory access, etc.) and uses that primitive to execute further kernel actions. Signing lets the image pass Code Integrity; the vulnerability supplies capability.

This is distinct from:

- A **publisher anti-cheat** driver (authorized product, unknown internals)
- A **whitelisted hardware driver** (GPU, storage, DMA bridge transport)
- **Test-signed** lab drivers on an isolated VM

Sources: Microsoft [recommended driver block rules](https://learn.microsoft.com/en-us/windows/security/application-security/application-control/app-control-for-business/design/microsoft-recommended-driver-block-rules) [K17], Defender [ASR vulnerable signed drivers rule](https://learn.microsoft.com/en-us/defender-endpoint/attack-surface-reduction-rules-reference) [K18].

## Platform responses

| Control | What it blocks | Limitation |
|---|---|---|
| Vulnerable driver blocklist | Known-bad hashes / certs at load | Not exhaustive; updated on a cadence |
| HVCI / memory integrity | Unsigned pool execution, W+X kernel pages | Does not stop all signed vulnerable IOCTL paths |
| ASR “vulnerable signed drivers” | **Writing** a blocked driver image to disk | Does not unload an already-present file |
| Kernel DMA Protection + IOMMU | Unrestricted physical DMA from peripherals | Separate from BYOVD software path |

Treat publisher claims about “blocking BYOVD” as **reported policy**, not a measured rate.

## Research and engineering discipline

1. **Separate map from neutralize** — mapping a helper driver for memory read should not automatically run PatchGuard-sensitive callback strips or AC patches.
2. **Never stop the vulnerable parent driver** while mapped code still references its `DRIVER_OBJECT` (documented BSOD on service stop).
3. **Record** blocklist/HVCI state in the manifest before any kernel experiment.
4. **Prefer** `MmCopyVirtualMemory` (or documented APIs) over attach-and-blast for process reads.

## kdmapper-class list cleaning vs this project (audit)

Classic **kdmapper** (Intel `iqvw64e` IOCTL map path) often tries to erase load forensics **before** unloading the vulnerable driver:

| Artifact | Typical kdmapper action | `masterlist` / `mapper_nal` + `Betest` (audited) |
|---|---|---|
| **PiDDBCacheTable** | Sig-scan `ci.dll` / `ntoskrnl`, AVL delete entry | **Not implemented** (no `piddb_*` sigs, no `RtlLookupElementGenericTableAvl` path) |
| **g_KernelHashBucketList** (`ci.dll`) | Remove hash-bucket entry for vulnerable image | **Not implemented** |
| **WdFilter** runtime driver arrays | Clear runtime driver list/count | **Not implemented** |
| **MmUnloadedDrivers** | Zero `KLDR_DATA_TABLE_ENTRY.BaseDllName.Length` on vehicle driver so `MiRememberUnloadedDriver` skips append | **Not implemented** in mapper (random names or stale slots can still appear — see lab WinDbg dumps) |
| **PsLoadedModuleList** | Sometimes unlink (PG-sensitive) | **Optional** `DkomUnlinkDriver` for BYOVD `.sys` name — only in non–`--rw-only` post-map sweep [PG-unsafe] |
| **Payload residence** | Pool RWX allocation | **`ManualMap` → `ExAllocatePool`/BYOVD alloc** — **non-image-backed** unless module stomping is added |
| **Module stomping** | Copy payload into signed victim `SizeOfImage`, wipe pool copy | **Not implemented** (no `WritePhysPage` / `PostStompFinalize` path) |
| **IOCTL surface** | New device | Betest `IoCreateDevice` under NAL `DRIVER_OBJECT` or dynamic names — enumerable |

`BetestDriver.c` **DeepHide** can zero **Betest’s own** `DriverSection` names when an IOCTL requests it; that is not the same as cleaning the **BYOVD vehicle** driver’s PiDDB/MmUnloaded records at map time.

**Implication:** Forensics and AC can still correlate (1) vulnerable driver load via PiDDB/hash bucket/WdFilter where enabled, (2) pool-executable mapping, (3) optional empty/random `MmUnloadedDrivers` slots. Stability-first default (`--rw-only`) also skips the PsLoadedModuleList DKOM sweep.

## Status

No live exploitation tests were run for this addendum. Operational fixes for the local masterlist BYOVD map path are described in §15 of chapter 14. This section records a **code audit**, not an endorsement to implement full kdmapper parity.
