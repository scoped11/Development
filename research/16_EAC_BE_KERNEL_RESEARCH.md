# EAC and BattlEye: kernel research map (defensive)

**Edition:** 12 November 2026 | Companion to [10_ANTICHEAT_AND_SECURITY.md](10_ANTICHEAT_AND_SECURITY.md), [14_KERNEL_DRIVERS.md](14_KERNEL_DRIVERS.md), [15_BYOVD_AND_DRIVER_TRUST.md](15_BYOVD_AND_DRIVER_TRUST.md).

This chapter records **why** the BYOVD / mapper / module-stomping thread exists in the project: both major third-party kernel anti-cheats (Easy Anti-Cheat and BattlEye) run **signed kernel components** on protected titles and are widely assumed to combine **kernel telemetry with user-mode and server-side signals**. Public docs do not publish full detection rules. Here we map **observable Windows artifacts** to **research questions** and to the **current masterlist lab stack**—not bypass recipes.

## 1. Scope and stop lines

| In scope | Out of scope |
|---|---|
| Understanding kernel callback models, module lists, and driver-load forensics | Live protected-title testing, ranked play, or ban-evasion validation |
| Comparing kdmapper-class hygiene vs our mapper on an **isolated lab VM** | Step-by-step implementation of trace erasure or AC neutralization IOCTLs |
| WinDbg/kernel dumps **you own**, synthetic drivers, CTF-style fixtures | Publishing blocklisted IOCTL paths as operational guides |
| Threat modeling for fair-play engineering and incident response | Claims that any single technique “defeats” EAC or BE |

Rob Run / `Development` game RE (dumps, TypeInfo, DMA overlay) is separate from this kernel lane except where both need a **manifested OS build + HVCI/blocklist state**.

## 2. Public surface (what we can name without reverse claims)

| Vendor | Typical kernel image name (examples) | Public documentation grade |
|---|---|---|
| Epic / Easy Anti-Cheat | `EasyAntiCheat_EOS.sys`, game-bundled user launcher + service | Publisher states kernel + user components; no public rule set [G08-era titles vary] |
| BattlEye | `BEDaisy.sys` (and related service stack) | Bungie/Epic/etc. require BE for supported launch [G18] |

Both products are **closed source**. Treat any specific “EAC checks PiDDB” statement as **heuristic** until reproduced on an owned lab with labeled fixtures. Use evidence grades from [12_RESEARCH_LAB_AND_VALIDATION.md](12_RESEARCH_LAB_AND_VALIDATION.md).

## 3. Kernel capabilities class (platform, not vendor-specific)

Microsoft documents patterns that **commercial anti-cheats also use or resemble**:

- **ObRegisterCallbacks** — strip or audit handle rights (e.g. `PROCESS_VM_READ`) [K10,K11]; see [14_KERNEL_DRIVERS.md](14_KERNEL_DRIVERS.md) §8.
- **PsSetCreateProcessNotifyRoutine / PsSetLoadImageNotifyRoutine** — observe process and image load events.
- **PsLoadedModuleList** and module snapshots — enumerate signed images in kernel VA space.
- **Return-address and execution-region checks** — code must often fall inside a known module range (module stomping targets this assumption).
- **Integrity comparisons** — in-memory PE sections vs on-disk image (stomping does not remove this class of check).
- **Platform forensics on driver load** — PiDDB cache, Code Integrity hash buckets, WdFilter runtime driver records, `MmUnloadedDrivers` (discussed in kdmapper literature and WinDbg practice).

EAC and BE are expected to **compose** several of these with user-mode and backend analytics [G02,G04]. Our research expands the **kernel forensics and mapping** side so we know what a defender (or analyst) would see—not so we can assert undetectability.

## 4. Artifact → research question → our stack (EAC/BE context)

| Observable artifact | Why AC-class software might care | kdmapper-style mitigation (literature) | `mapper_nal` + Betest today | Lab check (no live AC match) |
|---|---|---|---|---|
| PiDDB entry for BYOVD image | Correlates “this .sys was loaded” across reboot tools | AVL delete from `PiDDBCacheTable` | **Not implemented** | WinDbg `!pi` / CI symbols on **captured dump** after controlled load |
| CI hash bucket list | Duplicate of load reputation data | `g_KernelHashBucketList` scrub | **Not implemented** | Compare before/after load on VM snapshot |
| WdFilter runtime driver list | Win10+ additional load bookkeeping | Runtime array scrub | **Not implemented** | Kernel debugger on VM with WdFilter present |
| `MmUnloadedDrivers` row | Prior vulnerable mapper vehicle still named | `BaseDllName.Length = 0` before unload | **Not implemented** | `.for` loop on `nt!MmUnloadedDrivers` (your screenshots) |
| Empty / garbage unload slot | Suggests manual list tampering | Same Length trick | N/A | Count `[len=00]` anomalies vs clean baseline |
| Executable **non-image-backed** pool | Outside any `PsLoadedModuleList` range | Module stomping into Microsoft `.sys` | **Pool `ManualMap` only** | `!address`, module list walk, RIP sampling on synthetic workload |
| New `\Device\` + custom IOCTL | Trivial enumeration from kernel | IOCTL hijack / XFG chokepoint | Betest device + IOCTL table | Static audit of driver object (lab driver only) |
| `PsLoadedModuleList` unlink | Hides BYOVD name; **PatchGuard risk** | DKOM unlink | Optional non–`--rw-only` path | PG bugcheck rate on test matrix |
| AC driver in unload list (`BEDaisy.sys`) | Normal service lifecycle; not proof of bypass | N/A | Visible when BE unloaded | Baseline documentation only |

This table is the **bridge** between Terry/kdmapper notes and your EAC/BE motivation: anti-cheats are not only looking for “cheat.sys” strings—they are looking for **inconsistent kernel state** (unknown RWX regions, orphan callbacks, suspicious load history, mismatched disk/memory images).

## 5. Research threads (expanded workstreams)

### 5.1 BYOVD evidence and stability

- **Question:** On our OS builds, which of PiDDB / hash bucket / WdFilter / unload list still record the **vehicle** driver after `mapper_nal` runs with `--rw-only`?
- **Discipline:** Record HVCI, vulnerable-driver blocklist, and Defender ASR state in manifest; never stop the BYOVD service while mapped code still references its `DRIVER_OBJECT` ([15_BYOVD_AND_DRIVER_TRUST.md](15_BYOVD_AND_DRIVER_TRUST.md)).
- **Stability-first default:** Defer Betest EAC/BE neutralization IOCTLs until explicitly armed ([14_KERNEL_DRIVERS.md](14_KERNEL_DRIVERS.md) §15)—reduces load-time BSOD and avoids conflating “map for RW” with “fight AC.”

### 5.2 Payload placement (pool vs stomp)

- **Question:** For **detection literacy**, what fraction of obvious signals come from **pool execution** vs **module list presence** vs **device object**?
- **Module stomping** (your reference material): relocations, victim `SizeOfImage`, MDL/physical write, `PostStompFinalize` pool wipe, return-address fixup—documents **counter-detection theory** and **residual risks** (disk compare, dispatch audit, `__guard_dispatch_icall_fptr` audit).
- **Project state:** stomp path **in-tree** (`byovd/docs/STOMP_BYPASS.md`); default remains pool map until lab gates pass.

### 5.3 Callback and handle plane

- **Question:** How do Ob callbacks and image-load notifies constrain external readers even when kernel R/W exists?
- Tie to DMA overlay work: kernel path and DMA path are **different trust boundaries** ([01_UNIFIED_CHEAT_ARCHITECTURES.md](01_UNIFIED_CHEAT_ARCHITECTURES.md)).

### 5.4 IOCTL and communication fingerprint

- **Question:** Enumerating `MajorFunction[IRP_MJ_DEVICE_CONTROL]` vs hijacking a quiet Microsoft driver—what changes in **static kernel inventory**?
- Document for **defender blue-team** checklist, not implementation.

## 6. Proposed Lab I — AC-adjacent kernel forensics (isolated)

**Objective:** On a VM **without** launching a protected multiplayer title, capture kernel state before/after loading a **synthetic** or CTF driver via approved BYOVD lab mapper.

**Inputs:** OS build, HVCI on/off, blocklist version, VM snapshot IDs, mapper flags (`--rw-only` vs full), dump paths.

**Procedure (high level):**

1. Baseline: export module list hash, optional full dump.
2. Load vehicle + map synthetic payload per manifest.
3. Second dump; diff PiDDB/unload list/module list **as observable facts**.
4. Unload/reboot per stability protocol; third dump.

**Pass criteria:** manifest complete; no undocumented live-AC testing; findings graded (Heuristic vs Fixture-verified).

**Fail / stop:** PG bugcheck, undocumented production game launch, or missing authorization record.

Add to [12_RESEARCH_LAB_AND_VALIDATION.md](12_RESEARCH_LAB_AND_VALIDATION.md) when Lab I is first scheduled.

## 7. How this connects to Rob Run

| Lane | Focus |
|---|---|
| `Development` dumps / TypeInfo / overlay | Game memory layout, authorized offline analysis |
| `masterlist` mapper + Betest | Kernel primitives and **AC-adjacent** behavior under lab flags |
| This chapter | Explain **why** kdmapper list hygiene and stomping appear in notes: **EAC/BE-class kernel monitoring** makes kernel **consistency and provenance** first-class research objects |

## 8. Status

Chapter added to align project intent with defensive research framing. No live EAC/BE enforcement testing. Implementations of trace scrub or stomping remain **unaudited gaps** relative to kdmapper unless explicitly built and recorded under Lab I.
