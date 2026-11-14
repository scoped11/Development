# Compromising drivers — research only (no implementation)

**Purpose:** Expand the hunt beyond inbox Corsair/MyPort/MsIo with **public intel** on signed drivers that expose physical memory, MMIO, MSR, or kernel copy primitives — suitable for BYOVD-style abuse **after admin load**. Cross-check with [`resourcedrivers.md`](resourcedrivers.md) for **26200 VM truth** (load/open/phys/map).

**Do not treat LOL listing as “blocked on our VM”** or non-listing as “safe on 26200.” Always **hash vs** [Microsoft Vulnerable Driver Blocklist](https://aka.ms/VulnerableDriverBlockList) (`DriverPolicy_Enforced.xml`, policy revision **10.0.29545.0** as of lab notes).

---

## Lab baseline (already in our pipeline)

| Driver | Exploit shape | 26200 VM (2026-11-13) | Mapper `--driver` | IDA |
|--------|---------------|------------------------|-------------------|-----|
| `MyPortIO_x64.sys` | Dword phys R/W `0x9C406680` / `0x9C40A684`, `\\.\MyPortIODev` | Load yes, **phys yes**, map **no** (CR3 stall) | `myport` | `0e7cd77a` |
| `CorsairLLAccess64.sys` | Full-page map `0x225374` | Load yes, **phys yes**, map **no** | `corsair` | needs dedicated IDB id |
| `MsIo64.sys` | Map/unmap `0x80102040` / `0x80102044` | Load yes, phys **no** (998) | `msio` | `b71c23ac` |
| `ThrottleStop.sys` | WinRing0-class | Loads; weak on 26200 | `ts` | partial |

**VM lesson (not a crash):** mapping or scanning **PA ≥ 0x100000000** via Corsair `MmMapIoSpace` **wedged** the mapper with no bugcheck; MyPort CR3 walks need **low RAM only** and sparse progress, not whole 512MB.

---

## Tier A — Strong phys / map primitive, acquire + hash + IDA next

High mapper fit if **this build’s hash** is not version/hash-blocked on 26200.

| Driver | Vendor / source | Device / IOCTL (reported) | Blocklist / LOL | Why pursue |
|--------|-----------------|---------------------------|-----------------|------------|
| **GDrv.sys** (Gigabyte **new variant**) | Gigabyte App Center / Fusion / `RGB_Fusion_*` zips | `\\.\GIOV3`; **unauth** `0xC3502000` phys read, `0xC350200C` PhysicalMemory section map; AES key `GIGABYTEPASSWORDD` on other IOCTLs | **Name `gdrv.sys` is version-blocked** in MS policy — **variant hash may differ** | [GDrv-VulnAnalysis](https://github.com/Paxton-g/GDrv-VulnAnalysis) claims a **hash not on public lists** (June 2026). **Must hash every file**; don’t assume Fusion zip = blocked variant. |
| **SIVX64.sys** | SIV (System Information Viewer) v5.85 | `\\.\SIVDRIVER`; cmd **0x14** mapped phys R/W (`MmMapIoSpaceEx`), cmds 0x10/0x13 bulk read | Not on HVCI blocklist per [LOLDrivers #287](https://github.com/magicsword-io/LOLDrivers/issues/287) | Resolver for `MmMapIoSpace` (no IAT) — static heuristics alone miss it. **mapper already has `primitive_siv.hpp`** but inbox not exercised on VM. |
| **AmdTools64.sys** v1.7.16.219 | AMD tools package | IOCTL `0xFFF028A4`, struct with Phys + Length → user VA | Reporter: **not** in dataset/blocklist at filing ([LOLDrivers #430](https://github.com/magicsword-io/LOLDrivers/issues/430)) | AMD-signed; classic full user-visible mapping. **Download from AMD tool install**, hash before VM. |
| **AppShopDrv103.sys** | ASRock APP Shop | 30+ IOCTLs, AES wrapper, MmMapIoSpace, MSR, PCI ([KDU / LOL #222](https://github.com/magicsword-io/LOLDrivers/issues/222)) | ASRock family often blocklisted — **hash-specific** | Same OEM ecosystem as Polychrome; different binary than MsIo/MyPort. |
| **WinIo64.sys** | WinIo library / OEM rebadges | Maps `\Device\PhysicalMemory`; [LOLDrivers](https://www.loldrivers.io) notes **LoadsDespiteHVCI: TRUE** on some builds | Often tracked; MSI **Dragon Center** ships related `WinIO64` | Useful if a **non-blocklisted hash** appears from a fresh MSI Center install. |
| **IOMap64.sys** | ASUS utilities | CVE-2024-41498 arbitrary phys ([LOLDrivers](https://www.loldrivers.io/drivers/f4990bdd-8821-4a3c-a11a-4651e645810c/)) | Listed | Post–Armoury Crate **patched AsIO3** still worth diffing; **IOMap64** is the documented phys class. Install ASUS suite in VM → DriverStore hash only. |
| **ppa_x64.sys** | PhyMem helper | `\\.\PhyMem` ([LOLDrivers #315](https://github.com/magicsword-io/LOLDrivers/issues/315)) | New 2026 catalog entry | Tiny; obvious BYOVD shape — likely **1275 quickly** but fast IOCTL proof. |
| **pstrip64.sys** | EnTech PowerStrip ≤ 3.90.736 | IOCTL `0x80002008` maps phys into caller process | CVE-2026-29923; [LOLDrivers YAML](https://github.com/magicsword-io/LOLDrivers/blob/main/yaml/1db2dd90-5b8e-43c3-a39b-61e0614fb2ab.yaml) | Public PoC uses **EPROCESS pool tag scan** (same problem we hit — but driver maps **whole regions**, not 4-byte IOCTLs). |
| **DriversCloud_amd64.sys** | CybelSoft DriversCloud | Phys read (2MB/call), MSR, port I/O, weak SDDL | [LOLDrivers entry](https://www.loldrivers.io/drivers/b3f7c8d2-4a19-4e5b-9d6c-2f8e1a3b7c04/) | Low-integrity open claimed — rare; still need hash check on 26200. |

---

## Tier B — Strong primitives, niche install or EV signed (industrial / B2B)

| Driver | Source | Primitive (reported) | Notes |
|--------|--------|----------------------|-------|
| **TcIo.sys** + **TcRouter.sys** | Beckhoff TwinCAT 3 | `PhysicalMemory` section map, many `MmMapIoSpace` sites, PCI, DMA | [LOLDrivers #296](https://github.com/magicsword-io/LOLDrivers/issues/296); 18 related drivers same cert. Legit on factory PCs, odd on gaming VM. |
| **HwRwDrvx64.sys** / **evga_kernel_driver-x64.sys** | KDU list / EVGA | Phys R/W (KDU vehicles) | Often blocklisted; only worth **new hash** from current EVGA X software. |

---

## Tier C — Chain / augment (not a lone `mapper_nal` vehicle)

| Component | Role | Source |
|-----------|------|--------|
| **Redirt.sys** | Kernel base leak IOCTL | [LinkedIn chain writeup](https://www.linkedin.com/posts/haider-mustafa-9502133a9_byovd-kernelexploitation-lpe-activity-7485030828616458240-FBx6) (Medium/GitHub linked) |
| **safezone04.sys** | VA→PA via `MmGetPhysicalAddress` | same |
| **LallaMon.sys** | Phys read via `MmMapIoSpace` | same |
| **WinRing0.sys** | Phys write | same |

Reported **not on MS blocklist** as a set (verify each hash). Useful for **research on CR3/EPROCESS discovery**, not necessarily RwTest map, until one IOCTL vehicle supplies stable bulk read.

---

## Tier D — Inbox already; **IDA / device name** before any new code

| File | Imports | Load VM | Online / notes |
|------|---------|---------|----------------|
| **SONiXDDRx64.sys** | MmMapIoSpace tier 3 | Yes; open **failed** (wrong name) | ASRock Polychrome 2.0.208; **not in MS hash list** in lab doc but **LOLDrivers lists** family. Device string **not recorded** — strings/IDA first. |
| **iocbios2.sys** | tier 3 | Yes; open err 2 | Board BIOS helper; IDA session `6203e2af` — **no IOCTL table in resourcedrivers** yet. |
| **AMDRyzenMasterDriver.sys** (2.2.0.0 claim) | tier 3 | **31** on bare `sc start` | [`programs_with_kernel_drivers`](programs_with_kernel_drivers_2026-11-13.md): **hash not in lists** vs older ≤2.0.0.0 — needs **full utility stack** or official RM install, not manual service. |
| **HWiNFO64A.sys** | — | not in matrix | Driver **> 8.98.0.0** may pass version rule; download HWiNFO 8.60, hash, guest install. |

---

## Tier E — Blocked, LOL, or wrong primitive (research only: avoid VM map cycles)

| Driver | Why skip for 26200 map |
|--------|-------------------------|
| `gdrv.sys` (classic) | Filename blocked to max version |
| `RTCore64.sys`, `iqvw64e`, NAL family | Blocklist / EAC-class |
| `cpuz165.sys` (carved) | **1275** version rule |
| `GLCKIO2.sys` | Removed from Polychrome 2.0.208; blocked |
| `PawnIO.sys` | Phys only via **signed bytecode** |
| `cam_driver_mb.sys`, `ssdevfactory.sys` | IDA: no arbitrary phys |
| `ViveRRAudio.sys` | OOB kernel **read** leak, not phys map |
| `rzpnk.sys` | Process handle abuse |

---

## OEM download map (where to get **new** hashes)

| Vendor suite | Expected `.sys` | Status in `oem-raw` |
|--------------|-----------------|---------------------|
| Gigabyte RGB Fusion / GCC | `gdrv.sys` / variants | NSIS `UpdPack` → `setup.exe` (InstallShield, embedded `Data.Cab`). No plaintext `.sys`. `/extract_all` on the guest hung 90s and wrote nothing (job `0521c0593964`) |
| ASUS Armoury Crate / AI Suite | `AsIO3.sys`, `IOMap64.sys` | Not downloaded |
| MSI Center / Dragon Center | `NTIOLib_X64.sys`, `WinIO64.sys` | Not in inbox |
| SIV | `SIVX64.sys` | Not in inbox — **winget/installer** |
| AMD Ryzen Master (official) | `AMDRyzenMasterDriver.sys` | A-Tuning copy extracted; prefer official |
| HWiNFO 8.60 | `HWiNFO64A.sys` | `oem-raw/hwi_860.zip` is a 100 KB HTML page, not an archive. `https://www.hwinfo.com/files/hwi64_860.exe` returns **403** from host and guest |
| Beckhoff TwinCAT | `TcIo.sys` | Industrial license |
| PowerStrip | `pstrip64.sys` | Old; CVE public |
| DriversCloud | `DriversCloud_amd64.sys` | Freeware installer |

Workflow: **install in VM only** → enumerate DriverStore → copy to `audit/inbox/oem-sys/x64` → hash vs policy → IDA (`survey_binary` + IOCTL dispatch) → add row to `resourcedrivers.md` → **only then** consider `primitive_*` / `--driver`.

---

## Exploit-fit vs our stack

| Need | Best research targets |
|------|----------------------|
| **Fast phys R/W** (like Corsair page) | SIVX64 cmd 0x14, GDrv unauth IOCTLs, AmdTools64 `0xFFF028A4`, pstrip64 region map, DriversCloud |
| **Survive CR3 bootstrap** | Prefer **full-page map** vehicles over MyPort 4-byte IOCTL scan; SIV / GDrv / AMD map structs may reduce IOCTL count during EPROCESS walk |
| **25H2 “not marked” (policy)** | PawnIO (not raw map), HWiNFO **>8.98**, AMD RM **2.2.0.0** (if hash clean) — per [`programs_with_kernel_drivers_2026-11-13.md`](programs_with_kernel_drivers_2026-11-13.md) |
| **Already on Polychrome disk** | **SONiXDDR** before hunting distant OEMs |

---

## VM vault + host strip (2026-11-13)

- **128 GB vault VDI** on guest **D:** (`vm/vbox/resize-driverlab-storage.ps1`). Installers and extracts live on the VM.
- **Stripped export:** `guest-export-stripped-driver.ps1` → `audit/inbox/oem-sys/from-vm/<hash>/` (`.sys` + `manifest.json` only).
- **Host IDA:** `scripts/pull-vm-stripped-drivers.ps1` → `oem-sys/x64`.
- **Guest runbook:** `guest-vault-init.ps1` then `guest-greenfield-research-order.ps1` (or `host-greenfield-vm-pipeline.ps1`).
- **IDA notes:** [sonix_iocbios_ida_2026-11-13.md](sonix_iocbios_ida_2026-11-13.md) — `\\.\SONiXDDR_0_0_0_1`, `\\.\iocbios2`, SONiX phys IOCTL `0x9C406684` (low phys only).

## Suggested research order (no code)

1. **Done (host IDA):** `SONiXDDRx64` + `iocbios2` device/IOCTL notes — VM validate open/load in greenfield script.
2. **Download & hash:** SIV 5.85 (`SIVX64.sys`), HWiNFO 8.60 driver, official **AMD RM 2.2** driver.
3. **Gigabyte:** extract Fusion/GCC package; hash **any** `GDrv*.sys`; compare to [GDrv-VulnAnalysis](https://github.com/Paxton-g/GDrv-VulnAnalysis) SHA claim.
4. **ASUS guest install** once — hunt `IOMap64.sys` / post-patch `AsIO3.sys` in DriverStore.
5. **LOLDrivers RSS / GitHub issues** (magicsword-io) monthly — candidates in 2026 issues #287, #296, #315, #430, #222.
6. **MSI Center** — fresh `WinIO64` / `NTIOLib` hashes only; do not assume Dragon Center 2020 IOCTL docs match current build.

---

## References

- [LOLDrivers](https://www.loldrivers.io/) / [magicsword-io/LOLDrivers](https://github.com/magicsword-io/LOLDrivers) — community catalog + 2026 issues.
- [GDrv-VulnAnalysis](https://github.com/Paxton-g/GDrv-VulnAnalysis) — unauth + AES IOCTL table, `\\.\GIOV3`.
- [RevEng physmem article](https://reveng.ai/blog/physmem-e-when-kernel-drivers-peek-into-memory) — IOMap64 / pattern language.
- Lab: `resourcedrivers.md`, `greenfield_ida_research_2026-11-13.md`, `programs_with_kernel_drivers_2026-11-13.md`, `peripheral_vehicle_research_2026-11-13.md`.

**Last updated:** 2026-11-13 (research pass; **no mapper/worker/VM changes** in this commit).
