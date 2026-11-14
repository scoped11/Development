# Detection surfaces and “what else could we add?”

**There is no complete “undetected” stack** against EAC/BE plus backend. Adding more kernel surgery usually **increases** crash rate and inconsistency (empty unload slots, disk vs memory mismatch). This page is a **gap inventory** for Lab I — not a build list for live titles.

## What already looks loud (this kit)

| Surface | Why a kernel AC might care | Our state |
|---|---|---|
| Vehicle load | Signed-but-vulnerable `.sys` in PiDDB / CI / WdFilter | **Not scrubbed** |
| Vehicle stays loaded | We **must not STOP** Nal after map | Service + image remain |
| Pool executable Betest | RIP / scan outside module images | **No stomp** |
| New device + IOCTL table | Easy to walk `DRIVER_OBJECT` | Dynamic names only (weak) |
| `PsSetLoadImageNotifyRoutine(OnImageLoad)` | Extra notify slot | Always registered today |
| Large `.text` of Betest | Unique byte patterns if pool is scanned | Mutation IOCTL exists but **not** on RW-only path |
| Usermode `mapper_nal.exe` / handle to `\Device\…` | Process + handle table | Mapper exits; overlay may keep handle |

## Literature extras vs us (do **not** treat as “install these”)

| Technique | Aimed at | Residual / cost |
|---|---|---|
| PiDDB AVL delete | “Was iqvw loaded?” | Build-specific sigs; WdFilter/CI still hold data |
| CI hash bucket | Same | Version drift |
| `BaseDllName.Length = 0` | Skip `MmUnloadedDrivers` | Empty slots **are** an anomaly; we also **don’t unload** the vehicle |
| Module stomp + pool wipe | Image-backed RIP | Disk vs memory, victim `.text` use, PG |
| IOCTL hijack / XFG fptr | No new `\Device\Cheat` | Dispatch audit, `__guard_dispatch_icall_fptr` check |
| DKOM unlink vehicle from `PsLoadedModuleList` | Hide name | **0x109** — mapper already comments this |

kdmapper implements several of the first rows. **Copying them does not imply EAC/BE miss the rest** (pool, callbacks, server).

## Better research order (stable → measure → maybe one hygiene)

1. **Prove R/W** with `examples/process_rw.cpp` on a throwaway process.
2. **Lab I dumps** — list which of the table rows actually appear on **your** build.
3. **Do not enable `--full`** until (2) is written down.
4. Optional **vehicle-only** notes: you cannot hide a still-loaded Nal with unload-list tricks; PiDDB is about **load history**, not payload pool.
5. **DMA lane** is a different boundary (IOMMU / Kernel DMA Protection), not a kernel-stealth upgrade.

## What we will not add here

- New AC callback patches, hook kits, or “stronger” neutralize IOCTLs on the default path.
- Operational PiDDB / hash-bucket / stomp implementations for live games.
- Claims that any combination is undetectable.

If Lab I shows one **dominant** artifact (e.g. only PiDDB, not pool), document it in `docs/lab_i_notes.md` (create when you have a dump). Then decide whether that artifact is worth a **lab-flagged** experiment — separate from RW-only.

## Practical “more undetected” that is actually engineering

These reduce **self-inflicted** signal without new bypass code:

- Keep **RW-only** default (already).
- Overlay uses **only** process R/W IOCTLs; do not spray 40 CTF IOCTLs at launch.
- Do not `sc stop` Nal.
- Rebuild from this folder so mapper and `.sys` match.
- Isolate tests on a VM with HVCI recorded.

That is the highest leverage until Lab I data exists.
