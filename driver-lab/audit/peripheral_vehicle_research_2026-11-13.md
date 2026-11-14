# Peripheral / headset vehicle research (2026-11-13)

Lab-only notes from local scan + web search. **LOLDrivers = blocked on Win11 policy + paid AC** — IOCTL study only in VM, not a live vehicle strategy. See `greenfield_ida_research_2026-11-13.md` for IDA verdicts on inbox drivers.

## Host scan (today)

| Driver | On host | Mapper flag | Notes |
|--------|---------|-------------|-------|
| RTCore64.sys | MSI Afterburner | `--driver rtcore` | Staged to `byovd/bin/` |
| ThrottleStop.sys | repo/bin only | `--driver ts` | PT walk fails on 26200 (MmNonCached) |
| CorsairLLAccess64.sys | **missing** | `--driver corsair` | Install **iCUE on VM only** |
| SIV / TRIXX / Nal | missing | — | Not installed |

`greenfield-driver-candidates.ps1` → **0** rows: `driver_byovd_heuristic_2026-11-13.csv` has no third-party tier≥2 (enumerate was inbox-only). Re-run `enumerate-local-drivers.ps1` on a machine with OEM software installed.

## Headset / VR (weaker primitives — not full BYOVD)

| Driver | Signer | Primitive | LOL | Lab value |
|--------|--------|-----------|-----|-----------|
| **ViveRRAudio.sys** 0.1.11.7 | HTC VIVE | IOCTL `0x9C40240F` OOB kernel **read** (info leak) | [yes](https://www.loldrivers.io/drivers/4cb95b41-43b4-4806-b536-ae5fd8c76b0e/) | Install Vive Business Streaming; **not** physmem map — reverse for leak/chain only |
| Northwave write-up | — | — | — | https://northwave-cybersecurity.com/vive-vr-headset-kernel-driver-vulnerable-for-out-of-bounds-memory-read |

## Peripheral RGB / tools (stronger — MmMapIoSpace class)

| Driver | Product | Primitive | LOL | VM next step |
|--------|---------|-----------|-----|--------------|
| **CorsairLLAccess64.sys** | Corsair iCUE | `MmMapIoSpace` + usermode map IOCTL `0x225374`; MSR/port/PCI (elevated open) | [yes](https://www.loldrivers.io/drivers/a9d9cbb7-b5f6-4e74-97a5-29993263280e/) | **Priority**: `winget install Corsair.iCUE` on guest → `fetch-vehicle.ps1` → `probe_vehicles -Drivers corsair` |
| CVE-2020-8808 | iCUE &lt; 3.25.60 | unprivileged map (patched) | — | Current driver needs high-integrity CreateFile — fine for elevated worker |
| **rzpnk.sys** | Razer Synapse | `ZwOpenProcess` etc. | yes | Process handle, not physmem — skip for RwTest |
| **IOMap64.sys** | ASUS tools | `MmMapIoSpace` arbitrary R/W (CVE-2024-41498) | [yes](https://www.loldrivers.io/drivers/f4990bdd-8821-4a3c-a11a-4651e645810c/) | Optional: install ASUS AI Suite / copy sample from [RevEng advisory](https://reveng.ai/blog/physmem-e-when-kernel-drivers-peek-into-memory) into VM inbox — add `--driver iomap` later |

## Mapper already supports

- `primitive_corsair.hpp` — full-page phys map (preferred over ThrottleStop 8-byte IOCTLs)
- `primitive_rtcore.hpp` — may be VGA-limited on current Afterburner builds

## Suggested VM order (26200)

1. **rtcore** — quick probe (no extra install if bin synced)
2. **corsair** — after iCUE install
3. **ts** — diagnostics only until cached MmMap path fixed
4. Headset drivers — IDA / IOCTL fuzz, not mapper until phys primitive confirmed
