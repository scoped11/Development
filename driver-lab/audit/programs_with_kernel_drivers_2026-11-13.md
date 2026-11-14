# Programs that ship a kernel driver (research, 2026-11-13)

Lab inventory of **desktop programs whose installer drops a `.sys`**. Filter for this pass: **Windows 11 25H2 (build 26200)** with the default vulnerable-driver blocklist, and **not marked vulnerable**.

Checked against Microsoft `DriverPolicy_Enforced.xml` from `aka.ms/VulnerableDriverBlockList` (policy **10.0.29545.0**) and the local LOLDrivers CSV. A driver fails the filter if Windows version-blocks or hash-blocks it, or if LOLDrivers already lists that file.

## Passes 25H2 and is not marked

| Program | Driver we can point at | Why it passes |
|---------|------------------------|---------------|
| **PawnIO 2.2.0** (already in inbox) | `PawnIO.sys` | Hash not in the Microsoft policy and not in LOLDrivers. This is the driver Fan Control V238+, LibreHardwareMonitor, OpenRGB 1.1, CapFrameX, and current L-Connect install so they keep working after WinRing0 was blocked. Script gate, not a raw map IOCTL. |
| **HWiNFO 8.60** (10 Nov 2026) | `HWiNFO64A.sys` newer than **8.98.0.0** | Microsoft only blocks `HWiNFO32/64A/64I.SYS` through driver version **8.98.0.0** (the 2018 fix line). The app has shipped an HVCI-compatible driver since 7.16. The 8.60 file is not in the inbox yet — hash it before treating a download as clear. |
| **AMD Ryzen Master driver 2.2.0.0** | `AMDRyzenMasterDriver.sys` extracted from ASRock A-Tuning | This hash is in neither list. KDU’s block note is **≤ 2.0.0.0**. 2.2 is the HVCI-signed AMD sensor driver. Prefer the current official Ryzen Master package over the copy bundled in A-Tuning. |

CPU-Z is allowed only when `cpuz.sys` is **newer than 1.0.4.9**. The installer in `oem-raw` has not been unpacked, so that version is unknown.

## Fails the filter

| Driver | 25H2 / mark |
|--------|-------------|
| `gdrv.sys` (Gigabyte RGB Fusion, App Center) | **Every version.** Filename rule, max version 65535. Will not load. |
| WinRing0 (ThrottleStop, EVGA Precision, Fan Control ≤ V237, old CAM, old L-Connect) | Hash rules. Defender also flags it. These programs moved to PawnIO or broke. |
| `RTCore64.sys` (MSI Afterburner) | Many hash rules. Lab loads are not a 25H2-clear vehicle. |
| `GLCKIO2.sys` (old Polychrome) | Hash-blocked. Current Polychrome deletes it. |
| `MsIo64.sys`, `MyPortIO_x64.sys`, `SONiXDDRx64.sys` from Polychrome 2.0.190/2.0.208 | These exact hashes are **not** in the Microsoft hash list. LOLDrivers lists all three (MyPortIO entry is June 2026 and cites the Polychrome zip). Marked. |
| `CorsairLLAccess64.sys` **1.0.24.0** | Microsoft’s version rule stops at **1.0.23.65535**, which is why 1.0.24 still started on the 26200 VM. LOLDrivers lists this hash. Marked in the public catalog. |
| `NTIOLib.sys` (MSI Center / Dragon Center) | Blocked through file version **1.0.0.0**. Newer builds are a known family; do not treat them as clear without a hash check. |
| `AsIO3.sys` (Armoury Crate before the June 2025 update) | Public CVE-2025-3464 / CVE-2025-1533. A post-patch build is a different file and has to be hashed before it counts. |
| Old `HWiNFO64A/I.SYS` ≤ 8.98 | On the Microsoft version rule and in LOLDrivers (`hwinfo64i.sys`). |

`cam_driver_mb.sys`, `iocbios2.sys` 7.12, `ICCWDT.sys` 11.7, and `AsrNfDrv000.sys` are absent from both lists. They are not phys-map drivers. `cam_driver_mb` has no version resource, so Memory Integrity may still refuse it.

## What to download under this filter

1. HWiNFO **8.60** portable from hwinfo.com — confirm the driver file version is **above 8.98.0.0**, then hash it against the policy.
2. Keep **PawnIO 2.2.0**. That is the 25H2-supported kernel path for the fan and RGB apps that used to ship WinRing0.
3. Current **AMD Ryzen Master**, and compare its driver hash to the 2.2.0.0 file already extracted.

Drop Gigabyte gdrv, Afterburner RTCore, WinRing0 apps, Polychrome’s MsIo/MyPortIO/SONiX, and Corsair 1.0.24 from the “not marked” list.

Microsoft’s vulnerable-driver blocklist in use here is **10.0.29545.0** (May 2026 download; still the published revision as of Nov 2026). Win11 enables that list by default. A name on the list means `sc start` fails when Memory Integrity / the blocklist is on — useful as a lab note, not as a live vehicle.

## Why these programs have a driver

User mode cannot read SMBus, MSRs, EC mailboxes, or map device MMIO. RGB, fan curves, DRAM SPD, and sensor dashboards need one of:

| Lineage | What the driver is for | Typical device |
|---------|------------------------|----------------|
| **WinRing0** | Port I/O, MSR, PCI config, sometimes phys | `\WinRing0_1_2_0` |
| **WinIo / MICSYS (MsIo)** | Same family, RGB OEM fork | `\MsIo` |
| **PawnIO** | Signed driver + RSA-gated bytecode modules | `pawnio.eu` — not a raw map IOCTL |
| **Vendor one-off** | Board RGB, GPU tweak, or a private sensor driver | AsIO, GIO, RTCore, CorsairLL, HWiNFO |

The 2025–2026 shift: Defender treats unsigned/old **WinRing0** as `Trojan:Win32/Vigorf.A`. Fan Control, LibreHardwareMonitor, OpenRGB, CapFrameX, and Lian Li L-Connect dropped the bundled WinRing0 and now **prompt for PawnIO**. HWiNFO’s author (The Verge, 2025) described WinRing0 as unrestricted protected-resource access once loaded.

## Sensor / fan / bench (still the densest kernel surface)

| Program | Driver (current or last known) | 2026 status | Lab note |
|---------|--------------------------------|-------------|----------|
| **HWiNFO** 8.60 | `HWiNFO64.sys` / `HWiNFO64A.sys` (versioned `HWiNFO64A_*.sys`) | Still ships its own signed driver. Loads while the app runs unless “persistent driver” is on. | Best **non-WinRing0** sensor driver still distributed as a normal download. Third parties (old Thermaltake, ASUS, iCUE, GPU-Z) have also dropped copies. |
| **CPU-Z** / **GPU-Z** (CPUID) | `cpuz*.sys` | Installer in `oem-raw` (`cpu-z_2.20.2`). Driver is embedded; 7z PE unpack does not yield the `.sys`. | Needs Inno/guest install. Historically a small MSR/phys helper. |
| **ThrottleStop** | `WinRing0x64.sys` (KDU id 55, ≤ 3.0) | Lab already probes `--driver ts`. Must run from `%TEMP%`, not a network share (`StartService` 193). | Blocklist-class. Lab only. |
| **Core Temp** | `ALSysIO64.sys` | KDU id 29, ≤ 2.0.11 | Same class as WinRing0. |
| **LibreHardwareMonitor** | was WinRing0; **PawnIO** since PR 1857 (Sep 2025) | No bundled `.sys` in current builds. | Method 3, not mapper. |
| **Fan Control** ≥ V238 (Sep 2025) | PawnIO build of LHM. V237 and below shipped `FanControl.sys` (WinRing0). | GitHub “latest” asset name is not `FanControl.zip` (404 from host). Pin **V238** `FanControl_238_net_4_8_Installer.exe`. | Current build = PawnIO. Old build = WinRing0. |
| **CapFrameX** ≥ 1.8.1 (Jan 2026) | PawnIO | Release notes: “smarter PawnIO driver management.” | Method 3. |
| **ZenTimings**, **OmenMon**, **FanCtrl**, older **Panorama9** | WinRing0 | Named in the 2025 Defender wave (The Verge). | Old installers only. |
| **MSI Afterburner** 4.6.x | `RTCore64.sys` (≤ 4.6.2 build 15658 on KDU; later builds may drop or resign it) | Official hosts: msi.com and guru3d.com only. Host could not reach guru3d. | Already a mapper vehicle. Blocklist page-hash. |
| **EVGA Precision X1 / XOC** | `WinRing0x64.sys` (X1); older XOC also used an EIO/WinRing0 path | Download is account-gated on evga.com. | Blocklist by name. |
| **AIDA64** | Private sensor driver (not WinRing0) | Commercial. | Worth a guest install if a licensed copy exists; not in oem-raw. |
| **PassMark PerformanceTest / OSForensics** | `DirectIO64.sys` | Blocklist family (42 hash rules). | Skip for greenfield. |
| **SidebarMonitor** (2025) | **none** | Reads Ryzen Master SDK + NVML/ADLX + PDH. | Negative example: a monitor that refused to ship a driver. |

## Motherboard RGB / tuning (OEM suites)

These are the programs that still unpack a vendor `.sys` inside the installer. Several are already in `oem-raw`.

| Program | Driver | Where we stand |
|---------|--------|----------------|
| **ASRock Polychrome RGB 2.0.190 / 2.0.208** | **`MsIo64.sys`**, `MyPortIO_x64.sys`, `SONiXDDRx64.sys`, `AsrNfDrv000.sys`. Install script **deletes** old `GLCKIO2.sys`. | **Extracted** via innounp. MsIo is the Patriot/MICSYS phys driver (KDU id 4, CVE-2019-18845). Blocklist family `msio.sys` (8 rules) — hash-specific, so this build may or may not match. MyPortIO is tier 3 on dumpbin (`MmMapIoSpace`). |
| **ASRock Motherboard Utility / A-Tuning 4.1.11** | `iocbios2.sys`, `ICCWDT.sys`, `AMDRyzenMasterDriver*.sys` | **Extracted**. Tier 3. These are board-flash / Intel watchdog / AMD, not a retail mouse driver. |
| **ASRock older** | `GLCKIO2.sys` (Polychrome ≤ 1.0.4), `AsrDrv101–107`, `AxtuDrv`, `AppShopDrv103` | On the blocklist (`glckio2`, `asrdrv10*`). Not in the 2.0.208 package. |
| **ASUS Armoury Crate / AI Suite / GPU Tweak** | `AsIO3.sys` (current), older `AsIO2.sys`, `ATSZIO64.sys` (WinFlash) | Talos 2025: AsIO3 in Armoury Crate 5.9.9–6.1.18 (CVE-2025-3464 auth bypass; CVE-2025-1533). Current builds are patched — still the driver to diff if a **new** AsIO ships. Not downloaded yet. |
| **Gigabyte RGB Fusion / App Center / GCC** | `gdrv.sys` (`\Device\GIO`) | `RGB_Fusion_B26.0923.1.zip` is an **InstallShield-style UpdPack**, not Inno. No `.sys` yet. Blocklist **by name**. Win11 24H2 users report `gdrv.sys` refused. |
| **MSI Center / Dragon Center / Mystic Light** | `NTIOLib_X64.sys`, `WinIO64.sys`; Afterburner separately has RTCore | Not in oem-raw. Historical phys + MSR + port IOCTL surface (VoidSec / ReWolf). Dragon Center also shipped EneTechIo. |
| **G.SKILL Trident Z Lighting** | `EneIo64.sys` | KDU id 6. Cert-block class. |
| **Patriot Viper RGB** | `MsIo64.sys` | Same binary family as Polychrome’s copy. Patriot’s own URL 404’d; Polychrome already gave us the file. |
| **Thermaltake TT RGB Plus / TOUGHRAM** | `EneTechIo64.sys` | RGB Plus zip 404. KDU id 8. |
| **NZXT CAM** | old: `WinRing0x64.sys` (CVE-2020-13515). Current inbox: **`cam_driver_mb.sys`** | IDA: GPIO/SIO only — **not** a phys-map vehicle. |
| **Lian Li L-Connect 3** ≥ ~2.1.17 (2026) | Was WinRing0; **PawnIO** now | Static download URL 404. Guest or current Lian Li page. |
| **Corsair iCUE** | **`CorsairLLAccess64.sys`** plus HID/audio (`CorsairVHid`, `CorsairGamingAudio`) | **Lab vehicle.** Phys map IOCTL works on the VM. Audio driver is not a mapper. |
| **Alienware Command Center** | AWCC kernel helpers (board-specific) | Not pulled. |

## Peripheral suites (often *not* a phys driver)

SignalRGB’s conflict list is the practical “these programs own the device” set. Kernel presence is uneven: many are HID/user-mode and only look like driver software.

| Program | Kernel? | Notes |
|---------|---------|--------|
| **OpenRGB 1.1** | No own driver. **PawnIO** for RAM / old boards (install from pawnio.eu, or the MSI “background service” option). | MSI already in oem-raw. Method 3. |
| **SignalRGB** | Historically WinRing0; current installer URL returned non-binaries from this host. | Re-pull from the in-app updater on a guest. |
| **Logitech G HUB / LGS** | Installer on disk is a **stub**. Payload is user-mode + optional HID filter after a full install. | Guest install, then `enumerate-local-drivers.ps1`. |
| **Razer Synapse 3/4** | `rzpnk.sys` on older Synapse (process/handle, not phys). Synapse 4 installer is in oem-raw (15 MB) — unpack did not surface a phys driver. | IDA only if a `.sys` appears after guest install. |
| **SteelSeries GG / Engine** | `ssdevfactory`, `sssmbus`, HID filters — already in inbox. | Not phys-map. |
| **Elgato 4K / Wave Link** | User-mode until the MSI’s driver installer runs on a guest. | `guest-install-elgato-dump.ps1`. |
| **Wooting Wootility**, **Glorious Core**, **HyperX NGENUITY** | HID / web installers. NGENUITY file on disk is a 489 KB stub. | Skip for kernel hunt unless enumerate finds a filter. |
| **Cooler Master MasterPlus+** | RGB service; driver name not confirmed in this pass. | Download next if a guest has a CM device. |

## Other programs that install a kernel driver (out of the RGB lane)

Useful so the hunt does not pretend RGB is the only source.

| Class | Program | Driver | Why it is usually a skip |
|-------|---------|--------|--------------------------|
| Anti-cheat | Vanguard, EAC, BattlEye, FACEIT, mhyprot/ACE | their own `.sys` | ToS; not a BYOVD donor. FACEIT did conflict with early PawnIO (fixed in PawnIO 2.1.0 per Fan Control notes). |
| Virtual gamepad | **ViGEmBus** | `ViGEmBus.sys` | Already extracted. Bus driver, not phys. |
| Audio | **Voicemeeter**, Corsair audio | virtual audio `.sys` | KS/audio, not map. |
| OEM diagnostics | Dell `DBUtil`, Lenovo `LDD` / `LnvMSRIO`, HP `EtdSupport`, Huawei `PhyMemx64` | various | Heavily blocklisted. |
| AMD | **Ryzen Master**, Adrenalin | `AMDRyzenMasterDriver.sys`, `PdFwKrnl.sys`, `AODDriver` | Copies inside ASRock A-Tuning. Blocklist on old versions. |
| Intel | ME tools, XTU | `PmxDrv`, `ICCWDT.sys` | ICCWDT already extracted from A-Tuning. |
| Backup | some disk-image tools | `psmounterex.sys` | Added to the blocklist in the **April 2026** cumulative (CVE-2023-43896). |

## What this means for the lab

Three piles, matching the existing method table:

1. **Own driver, phys-shaped, possibly not the famous hash** — HWiNFO current, ASRock Polychrome **MyPortIO** / **MsIo64** (hash-check the blocklist before a VM load), ASUS **AsIO3** from a *post-patch* Armoury Crate (diff against the 2025 vulnerable builds), MSI **NTIOLib** from current MSI Center.
2. **Own driver, already a known vehicle** — CorsairLL (working), RTCore, ThrottleStop, gdrv, GLCKIO2, EneIo, DirectIO, DBUtil. Lab only.
3. **PawnIO consumers** — Fan Control, LHM, OpenRGB, CapFrameX, L-Connect 3, new SignalRGB. One driver, script gate. Do not add them to `mapper_nal`.

## Download queue (not done)

| Target | Why |
|--------|-----|
| HWiNFO 8.60 portable from hwinfo.com | Current private driver, not WinRing0. |
| ASUS Armoury Crate current | AsIO3 after the June 2025 patch — compare to Talos’s 5.9.13 build. |
| MSI Center current | NTIOLib generation actually shipping in 2026. |
| Fan Control **V238** installer (pinned tag, not `latest/download/FanControl.zip`) | Confirms PawnIO-only; archive the old WinRing0 build separately if a mirror exists. |
| CPU-Z via innounp on the existing `cpu-z_2.20.2-msi-en.exe` | Driver is inside the Inno payload. |
| Gigabyte UpdPack | Needs an InstallShield extractor or a guest install to reach `gdrv.sys`. |

## Sources

- KDU provider table: https://github.com/hfiref0x/kdu/blob/master/Help/providers.md
- WinRing0 Defender wave: https://www.theverge.com/report/629259/winring0-windows-defender-fan-control-pc-monitoring-alert-quarantine
- Fan Control V238 (drops WinRing0): https://github.com/Rem0o/FanControl.Releases/releases/tag/V238
- LHM PawnIO switch: https://github.com/LibreHardwareMonitor/LibreHardwareMonitor/pull/1857
- OpenRGB 1.1 PawnIO requirement: https://codeberg.org/OpenRGB/OpenRGB/releases
- Armoury Crate AsIO3 (2025): https://blog.talosintelligence.com/decrement-by-one-to-rule-them-all/
- MSI driver set: https://voidsec.com/multiple-vulnerabilities-in-msi-products/
- Blocklist download (10.0.29545.0): https://www.microsoft.com/en-us/download/details.aspx?id=108667
- Blocklist policy: https://learn.microsoft.com/en-us/windows/security/application-security/application-control/app-control-for-business/design/microsoft-recommended-driver-block-rules
- HWiNFO driver FAQ: https://www.hwinfo.com/download/ and https://hwinfo.dev/faq.html
- iCUE conflict list (who else owns the hardware): https://www.corsair.com/us/en/explorer/gamer/accessories/list-of-programs-known-to-conflict-with-corsair-icue/
