# AC fingerprints — bans, PC identity, and driver signals

**Source:** `driver2/dumps/*` manifests + bundles captured on build **26200.9974** (2026-11-13).  
**Grade:** Public AC behavior is **closed source**. This doc mixes **fixture facts** (paths/hashes in your dumps) with **industry-heuristic** ban and kernel-sensor models. Use for lab threat modeling and inspector Q&A — not as a live evasion recipe.

Related: `VEHICLE_STEALTH.md`, `docs/04_detection_surfaces.md`, `docs/11_ac_block_vs_hide_plan.md`, `driver-lab/data/ac_eac_be_vehicle_policy.json`.

---

## How bans usually layer (all three families)

Anti-cheats rarely ban on **one** byte. They correlate **client telemetry → kernel facts → account/backend**.

| Layer | Typical signals (heuristic) | In **your** captures |
|-------|----------------------------|----------------------|
| **Account / entitlement** | Platform IDs, session tickets, report volume | EAC: `productid` / `sandboxid` / `deploymentid` in `Settings.json` |
| **Client build integrity** | Game + AC DLL/sys versions, cert chain | EAC certs in dump; ACE `sguard_product_rc`; BE `BEClient_x64.dll` |
| **Machine / environment** | OS build, disk layout, SMBIOS/disk/NIC serials*, TPM*, display/GPU IDs* | `host_os_build` in every `manifest.json` |
| **Kernel driver plane** | Loaded modules, device objects, callbacks, pool/RIP, IOCTL patterns | Hashes in manifest; policy JSON for blocked BYOVD names |
| **Behavior** | Aim/ESP stats, memory scan hits, manual reports | **Not** in static dumps — dominant on live bans |

\*Exact collection differs by vendor and title; your mapper lab IOCTLs (`--hwid`, `--tpm-spoof`) exist because **cheat devs assume** these dimensions get sent server-side — we do **not** have packet captures here proving each field.

**Inspector line:** A “HWID spoofer” in usermode does not answer kernel driver detection. A “driver SDK” does not answer account bans.

---

## EAC (Rust EOS) — `dumps/rust-eac-eos_*`

### PC / account fingerprint (from dump)

| Artifact | Path in bundle | Why it matters |
|----------|----------------|----------------|
| Epic EOS product identity | `eac/EasyAntiCheat/Settings.json` | `productid`, `sandboxid`, `deploymentid` tie this install to **Epic’s EOS backend** — bans/sanctions are often **account + product**, not only “this disk”. |
| Executable binding | same file → `RustClient.exe` | Launches are validated against expected game binary. |
| Shared EOS kernel driver | `kernel/EasyAntiCheat_EOS.sys` | SHA **A38FA3D4…** in `manifest.json` — one driver serves **many** EOS titles; reputation can be **cross-game**. |
| Trust material | `eac/EasyAntiCheat/Certificates/base.bin`, `runtime.conf`, `base.cer` | Client authenticity / update chain. |

### What EAC-class stack is believed to hunt in **drivers** (kernel)

| Signal class | Mechanism (platform + public research) | Your kit / policy |
|--------------|----------------------------------------|-------------------|
| **Known vulnerable BYOVD** | Blockload / refuse `\\.\RTCore64`, `\\.\Nal`, hash deny | `ac_eac_be_vehicle_policy.json` → `paid_eac.block_*` |
| **Load forensics** | PiDDB, CI hash buckets, WdFilter runtime lists | Hygiene in mapper (`vehicle_hygiene.hpp`); does not remove **in-memory** vehicle |
| **Unknown executable pool** | RIP outside any module image | Manual-mapped `Driver2` pool — `04_detection_surfaces.md` |
| **New cheat device + IOCTL fan** | Walk `DRIVER_OBJECT`, IRP table | Betest device; dynamic `\\.\MsFlt*` names (weak) |
| **Image-load / process notify** | `PsSetLoadImageNotifyRoutine`, process callbacks | Strip IOCTLs in lab profiles (`0x81B`, `0x851` improve path) |
| **Handle / Ob plane** | `ObRegisterCallbacks` on game process | Why external R/W from usermode is gated even when kernel R/W exists |
| **Integrity** | In-memory PE vs on-disk | Spoofing **filename** of RTCore does not fix hash at load |

### Tier-0 on your host (Rust + EAC)

Expect **vehicle block before map** (`lane-d-preflight.ps1`, `docs/12_lane_d_split_trust.md`). Fingerprinting question for Rust PC is: *“Can we open BYOVD at all?”* — not *“Can we hide it after block?”*

---

## BattlEye (R6) — `dumps/r6-battleye_*`

### PC / account fingerprint (from dump)

| Artifact | Path | Notes |
|----------|------|--------|
| Game binding | `battleye/game/BELauncher.ini` | `GameID=r6s` — BE scopes policy **per title**. |
| Usermode service | `usermode/BEService_r6s.exe` | Anchor when **`BEDaisy.sys` missing** on disk (your capture). |
| Client | `battleye/game/BEClient_x64.dll`, `BEService_x64.exe` | Versioned user stack; kernel driver often installed **at game launch**, not in game folder. |
| Locale / disclosure | `battleye/game/Privacy/*.txt` | Indicates BE collects data under stated privacy text — not a field list. |

**Manifest gap:** No `BEDaisy.sys` in bundle → kernel RE for BE on this host is **runtime/service install**, not static game dir.

### What BE is believed to hunt in **drivers**

| Signal | Heuristic | Your policy dump |
|--------|-----------|------------------|
| Classic BYOVD filenames | iqvw, Nal, RTCore, gdrv, capcom, … | `paid_battleye.block_filename_patterns` |
| Kernel module `BEDaisy.sys` | BE’s own signed driver runs callbacks/scans | Lab IOCTL `0x852` replica only |
| Foreign kernel R/W to game | Via callbacks + integrity | Same Ob/handle plane as EAC |
| Mapper artifacts | Suspicious loads, non-list execution | Same pool/module story as `04_detection_surfaces.md` |
| Global bans | Often **very sticky** HWID cluster (community reports) | Spoof IOCTLs in mapper are **lab**, not ranked proof |

---

## ACE (Delta Force, Arena Breakout) — `dumps/deltaforce-ace_*`, `dumps/arenabreakout-ace_*`

### PC / account fingerprint (from dump)

| Artifact | Delta Force manifest | Arena Breakout manifest |
|----------|----------------------|-------------------------|
| SGuard build | `sguard_product_rc`: **1.376.2512.2118** | **1.369.2511.2095** |
| `ACE-GAME.sys` identity | Game bundle hash **C3FE27FC…** ≠ system32 **2B6DB055…** (`kernel_match: false`) | Bundle = system32 hash (`kernel_match: true`) |
| IDA primary | Use `kernel/ACE-GAME.sys` from **game bundle** | `kernel/ACE-GAME.sys` |
| Usermode stack | `anticheatexpert/` tree: SGuard, `ids.dat`, `pld.dat`, ACE-CORE/BASE | Same pattern |

ACE bundles **Tencent AntiCheat Expert + SGuard**. Fingerprint dimensions (heuristic): **account**, **SGuard/ACE build ids**, **kernel module hashes**, **machine attestation** (often stronger in CN-market titles than pure BYOVD name checks).

### Device surface (policy)

`paid_ace.block_device_patterns` includes **`\\.\ACE-GAME`** — ACE titles may watch **their own** device namespace as well as LOL drivers.

### What ACE is believed to hunt in **drivers**

| Signal | Evidence in your repo | Notes |
|--------|----------------------|--------|
| BYOVD blocklist overlap with EAC/BE | `ac_eac_be_vehicle_policy.json` | RTCore/Nal/iqvw class |
| Dual `ACE-GAME` sources | Delta manifest note | Wrong `.sys` for RE = wrong IOCTL semantics |
| ACE kernel modules | `ACE-BASE`, `ACE-ADVT`, `ACE-CORE`, `ACE-GAME` in dump | Version pinning in `ac_host_captures.json` |
| Lab improve path | Mapper `0x846`, `0x84E`, `0x86A` | Measures **client IOCTL story**, not ban immunity |

---

## Unified “what they look for in a cheat driver” (checklist)

Use this when a cheat inspector asks: *“What will get me caught?”*

| Check | EAC (EOS) | BattlEye | ACE |
|-------|-----------|----------|-----|
| Known BYOVD loaded / device open | **High** | **High** | **High** ( + ACE device rules) |
| Unsigned / manual-mapped payload in pool | **High** | **High** | **High** |
| Custom `\\.\` device + wide IOCTL range | **Medium–High** | **Medium–High** | **Medium–High** |
| PiDDB / CI / WdFilter traces only scrubbed | **Low alone** | **Low alone** | **Low alone** |
| DKOM unlink vehicle only | **Low** | **Low** | **Low** |
| Patched vehicle `MajorFunction` | **High** (if scanned) | **High** | **High** |
| Mapper process + open handle to vehicle | **Medium** | **Medium** | **Medium** |
| Account + behavior backend | **Dominant** | **Dominant** | **Dominant** |

**driver2 default** targets **comm + lab IOCTL parity**, not invisibility on rows marked High.

---

## Machine-readable index

`dumps/ac_fingerprints_index.json` — hashes, EOS ids, SGuard builds, policy refs. Regenerate when captures change:

```powershell
.\driver2\build-fingerprint-index.ps1
```

---

## Evidence bundle for peer review

When answering ban/fingerprint questions, attach:

1. This file + `VEHICLE_STEALTH.md`  
2. Relevant `dumps/*/manifest.json`  
3. `ac_host_captures.json` (or copy under `driver2/dumps/`)  
4. Host `block-fingerprint-delta` JSON if Tier-0 block was measured (`docs/18_block_fingerprint_delta.md`)
