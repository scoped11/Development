# Greenfield vehicle hunt (not LOLDrivers)

**RTCore, iqvw/NAL, gdrv, capcom, …** are on [LOLDrivers](https://www.loldrivers.io/). That means:

- Windows **vulnerable driver blocklist** (when enabled) targets their hashes.
- **Paid EAC/BE** and most kernel AC stacks block them by name, hash, or IOCTL telemetry.
- Your **mapper fallback list** is a **lab convenience**, not a live Rust/R6 strategy ([Lane D](12_lane_d_split_trust.md)).

Treat a LOLDrivers match as **`skip_already_public_lol`** — useful only to explain why `sc start` or CreateFile fails, not as a “find.”

## What to hunt instead

| Signal | Why |
|--------|-----|
| **Tier ≥ 2** import heuristic (`MmMapIoSpace`, HAL PCI, `ZwMapViewOfSection`, …) | BYOVD-shaped surface |
| **LOLDrivers = no** | Not on public death list (yet) |
| **Third-party / OEM** signer | Anti-cheat less likely to have year-old hash |
| **IDA: weak IOCTL gate** | Arbitrary phys/kernel R/W without obvious auth |

Examples on a gaming PC are often **peripheral OEM**, **fan/clock tools**, **old motherboard utilities**, **game anti-cheat kernel modules you already run** (e.g. ACE-*) — research ethics and ToS are your call; technically they are not in LOLDrivers.

## Pipeline

```powershell
cd driver-lab\scripts
.\enumerate-local-drivers.ps1 -IncludeMicrosoftWhcp
.\check-driver-blocklists.ps1          # tags MapperVehicle column
.\greenfield-driver-candidates.ps1 -MinTier 2 -CopyToInbox
```

Then IDA inbox copies ([17_local_driver_ida_audit.md](../../driver-lab/docs/17_local_driver_ida_audit.md)).

## Proof tool: `mapper_nal` or a better fit

**Goal** is the same for DriverLab: load payload via kernel R/W (phys map + CR3 walk + `KernelPrimitives`). **Vehicle** is not locked to `mapper_nal.exe`.

After IDA classifies the IOCTL surface:

| IDA shape | Lab proof tool | RwTest / full map |
|-----------|----------------|-------------------|
| **Arbitrary phys → usermode VA** (MmMapIoSpace IOCTL, bounded size only) | Extend `primitive_*.hpp` **or** tiny `phys_probe.exe` if IOCTL layout is ugly | `mapper_nal --driver <new>` when primitive is wired |
| **Fixed phys / port I/O / MMIO for one device** | Dedicated mini-mapper or IOCTL fuzzer — **not** `mapper_nal` | No — wrong primitive for kernel walk |
| **Script / crypto gate** (e.g. PawnIO bytecode + RSA) | Separate client that speaks that protocol | No — unless you forge/trust scripts (out of scope) |
| **Process/handle only** (rzpnk-style) | Skip for phys map lane | No |

Prefer the **smallest** tool that proves the primitive on the VM: `phys_probe` (CreateFile + one IOCTL + read MZ at known low phys) before investing in `main_nal` stealth/DKOM. Promote to `mapper_nal` only when the driver passes **arbitrary page map** and CR3 bootstrap on 26200.

## Live title check (still no `sc start`)

[Method 18 — BFD](18_block_fingerprint_delta.md): device open delta with AC up. Confirms **block** without loading a new `.sys`.

## Honest outcome rate

Most machines yield **zero** greenfield vehicles that are both exploitable and loadable under paid AC. When that happens, the correct method is still **Lane D** (DMA + lab VM proof), not another public LOL driver.
