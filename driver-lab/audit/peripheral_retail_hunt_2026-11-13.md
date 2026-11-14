# Retail peripheral list → kernel dump plan (105 items)

**Not** Microsoft approval, not AC-safe, not vulnerability claims — this maps retail SKUs to whether a **third-party `.sys`** is even plausible for our phys-map / DriverLab lane.

## Catalog

| File | Purpose |
|------|---------|
| `data/peripheral_list_105.txt` | Your 105 SKUs (verbatim) |
| `data/peripheral_retail_catalog_105.csv` | Hunt tier + lab action per SKU |
| `scripts/build-peripheral-catalog.ps1` | Regenerate after list edits |

```powershell
cd driver-lab\scripts
.\build-peripheral-catalog.ps1
```

## Hunt tiers (summary)

| Tier | Count (approx) | Meaning |
|------|----------------|---------|
| **hid** | 33 | USB HID — inbox only; **no `.sys` dump`** |
| **ghub** | 23 | Install **Logitech G HUB / Options+** on guest; `enumerate-local-drivers.ps1 -CopyToInbox` |
| **usb_audio** | 12 | USB audio — skip unless G HUB adds filter |
| **uvc** | 11 | `usbvideo.sys` — skip |
| **vm_install** | 7 | **Elgato** — run `guest-install-elgato-dump.ps1` (MSI + DRVInstaller) |
| **skip** | 11 | **Kingston** flash/SSD — USBSTOR only |
| **review** | 8 | Mostly POP/Lift/Ergo mice — treat as **hid** unless G HUB installed for that device |

## Online OEM pull (beyond Corsair / PawnIO / NZXT)

**Scripts:** `scripts/download-oem-online.ps1` → `scripts/extract-inno-oem.ps1` (Inno) → `scripts/extract-oem-to-inbox.ps1 -RefreshHeuristics`

| Source (official) | On disk | Kernel modules surfaced (2026-11-13) |
|-------------------|---------|--------------------------------------|
| ASRock Polychrome v2.0.208 | `oem-raw/PolychromeRGB_v2.0.208.zip` | **MsIo64.sys** (Micron/MICSYS phys — Patriot-class), **MyPortIO_x64.sys**, **SONiXDDRx64.sys**, **AsrNfDrv000.sys** (NF ISP — not phys tier) |
| ASRock MotherboardUtility v4.1.11 | `oem-raw/MotherboardUtility_v4.1.11.zip` | **iocbios2.sys**, **ICCWDT.sys**, AMDGPU **AMDRyzenMasterDriver**,*.sys |
| Gigabyte RGB Fusion B26 | `oem-raw/RGB_Fusion_B26.0923.1.zip` | UpdPack only (not Inno) — **gdrv.sys still nested**; needs silent install or InstallShield unpack on guest |
| MSI Afterburner | guru3d zip in manifest | **RTCore64.sys** — add to `VehiclePhysProbe` / mapper once unpacked from setup |
| HyperX NGENUITY stub | 489 KB installer | Web-only; need full Kingston DLL / guest install |
| FanControl / L-Connect / SignalRGB / Wootility | URLs flaky (404/HTML) | Fix URLs per release page; no `.sys` until download succeeds |

**Mapper-interest (new):** **MsIo64** + **MyPortIO_x64** from Polychrome — dumpbin tier=0 (no `MmMapIoSpace` import) but public CVEs document **arbitrary phys R/W** IOCTLs; queue IDA IOCTL table next.

## What we already dumped (host inbox)

Tier≥2 imports (dumpbin): **CorsairLLAccess64**, **PawnIO**, **cam_driver_mb**, **MyPortIO_x64**, **SONiXDDRx64**, **iocbios2** — full list `audit/driver_byovd_heuristic_oem_2026-11-13.csv` (28 binaries).

**G HUB** `lghub_installer.exe` on disk is a **stub** (no embedded `.sys`); full payload needs **VM install** or Logitech offline full installer.

**Elgato 4K Capture Utility** MSI extracts to user-mode (`DRVInstaller.exe`, `.ax`) — kernel modules only after guest install.

## Driver methods on VM (your steer)

| Method | Vehicle | Status 26200 |
|--------|---------|----------------|
| **1** Full-page phys map | CorsairLLAccess64 + `mapper_nal --driver corsair` | Phys IOCTL OK (`VehiclePhysProbe`); CR3 bootstrap reordered + page cap |
| **1** 4-byte phys IOCTL | RTCore64 + `--driver rtcore` | Probe in flight (elevated worker) |
| **1** 8-byte IOCTL | ThrottleStop `--driver ts` | Loads from `%TEMP%` only (not `Z:\`); PT read weak on 26200 |
| **3** Script gate | PawnIO | Needs signed `ioctl_*` pawn bytecode — not raw mapper |

## Next dumps aligned to this list

1. **Guest:** Install **Logitech G HUB** (for MX/Brio/StreamCam tier only — one install covers many SKUs).
2. **Guest:** `.\scripts\guest-install-elgato-dump.ps1` for Elgato capture tier.
3. Re-run `.\extract-oem-to-inbox.ps1 -RefreshHeuristics` + IDA on any new tier≥2 file.
4. Ignore HID/UVC/Kingston rows for kernel vehicle work unless enumerate surprises you.
