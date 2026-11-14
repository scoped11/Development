# Windows Code Integrity blockage — and the method we use

**Lab:** DriverLab-E-26200 (guest ~26200). Private. Not a ranked / live-title claim.

**Date locked in:** 2026-11-14.

This is the note for the wall we hit and the lane we stay on.

---

## The blockage

Windows **Code Integrity (CI)** refused to map our own image as a new kernel driver. Cloned EAC / BattlEye / ACE never saw it in memory.

`sc start` of a **new** `.sys` is a CI load. If CI rejects the file, the module is not in `PsLoadedModuleList`. Anti-cheat cannot “flag” a driver that did not load.

### What we measured on the guest

| Image | What we did | Result | CI event |
|-------|-------------|--------|----------|
| `C:\DriverLab\vault\graft\VmLabDrv.eac-grafted.sys` (EAC cert table copied onto VmLabDrv) | `sc create GraftForeign` + `sc start` | **StartService 577** — Windows cannot verify the digital signature | **3004** — file hash could not be found (`\DriverLab\vault\graft\VmLabDrv.eac-grafted.sys`) |
| `C:\DriverLab\vault\rw\VmLabDrv.sys` (lab-signed / staged) | `sc start GraftLab` | **577** + CI **3004** | same class of reject |
| `wcifs.grafted.sys` (identity graft — digest-identical copy, Authenticode **Valid**) | `sc start` as `GraftIdentity` / `GraftId.sys` | **StartService 2** (wcifs already loaded; not a signature reject) | no 3004 |

Job: `2d85ea1be564` (`driver-lab/vm/bg/graft-load-then-ac.json`). ACE-BASE + ACE-ADVT were **RUNNING** after the failed loads. No ACE/BE log hit named the graft. ACE-GAME still self-stops (`win32=31`) with no game — unrelated to CI.

Get-AuthenticodeSignature on disk (AC up or down) does **not** bypass this. Foreign graft stays **HashMismatch**. Identity graft stays **Valid** and still cannot be a second `wcifs`.

### What this is

CI is checking a **new image** at load time:

1. Authenticode digest of the PE (excluding the cert table).
2. Catalog / publisher / HVCI policy on this boot.

A **foreign cert-table graft** copies someone else’s PKCS#7 onto a different PE. The signer string may look like WHQL / Epic / Microsoft. The **hash does not match**. CI treats that as damaged or malicious (`577` / `3004`), not as a trusted driver.

A **Valid** identity graft only proves the checksum/PKCS#7 code is right when the digest region is byte-identical to the source. It is not a way to load unsigned payload.

### What we already tried that is not the method

| Attempt | Outcome on this guest |
|---------|------------------------|
| Authenticode graft + `sc start` | **577** / CI **3004** on foreign payload |
| Lab supplemental WDAC (`Apply-LabCiSupplemental.ps1`, policy `E9C39255-…`) | Force-trusted / lab publisher still **`0xc0000428`** / **577** — deprioritized |
| Testsigning as the load path | Deprioritized. Not default CI. Not the ranked-shaped path |
| “Load it then see if cloned ACs flag it” | CI blocked the load first. ACs had nothing resident |

Microsoft attestation (`SIGNING_PATH.md`) is Phase 2 (EV + Partner Center). It is **not** what we are doing now.

---

## The method we are using

**Image-backed stomp on an already-loaded, already-trusted signed victim.**

CI already accepted that victim at boot (`wcifs` on this lab). We do **not** `sc start` our cheat `.sys`. We:

1. Open a **signed BYOVD vehicle** (lab: MyPort) — CI allows that signed image.
2. Use its kernel R/W to write **`Driver2Stomp`** into a **dead / `0xCC` region** of the victim’s in-memory `.text` (`PlacementRegionStrict` — no live `.text`).
3. Hijack the victim’s existing `IRP_MJ_DEVICE_CONTROL` so cheat IOCTLs (`0x870` / `0x805` / `0x806`) run with **RIP inside the signed module**.
4. Leave the **on-disk** victim bytes unchanged.

CI never evaluates a new unsigned / HashMismatch image. That is the around-CI load. Residual vs AC is disk-vs-memory on the victim, not `sc start` 577.

Track name in the lab: **Track A**. Engine: `byovd/src/mapper/stomp.cpp`. Payload: `byovd/driver2/src/Driver2Stomp.c`. Guest orchestrator: `driver-lab/vm/bg/guest-stomp-lane-full.ps1`. Detail: `STOMP_BYPASS.md`.

### Lab order (do not reverse)

Mock / cloned AC **after** the driver is in. Same as `15_test_ac_replicas.md`.

1. `guest-stop-ac-replica.ps1`
2. `stomp_plan` must pass for the victim (`PlacementRegionStrict`).
3. Live stomp (`guest-stomp-live-map.ps1`). Host `stomp_plan` (2026-11-14): `Driver2Stomp` **SizeOfImage `0x11C0`**, need **`0xF80`** after header skip `0x240`. **`wcifs` / `luafv` / `dam` fail** (`PlacementRegionStrict`). Only inbox hit: **`pacer.sys`** (`cluster_rva=0x15040`). Guest service for that image is **Psched**; device **`\\.\Psched`** opens. Do not treat `wcifs` as plan-green until `stomp_plan` passes on the guest build.
4. `Driver2Client.exe --device \\.\WCIFS probe` (cheat before AC).
5. Start cloned replica (`Start-AcReplicaServices.ps1`).
6. Probe again.

Do **not** live-stomp `fltmgr` / storage / `ntfs` (0x3B). After **0x3B** or **0xDA**: clean boot, `guest-bsod-check.ps1`, re-sync `BUILD.txt`, plan/dry-run before the next live map.

### What this method is not

- Not disabling HVCI, Secure Boot, or CI on the guest as the product path.
- Not `sc start` of `VmLabDrv` / grafted files.
- Not cert-table graft as a substitute for a real signature.
- Not a claim that cloned ACE/EAC/BE “bless” the module, or that ranked CI/AC will miss disk-vs-memory.

---

## Pointers

| Doc / artifact | Role |
|----------------|------|
| This file | CI wall + **method we are using** |
| `STOMP_BYPASS.md` | Stomp engine, placement, VM scripts, BSOD log |
| `SIGNING_PATH.md` | Later attestation `sc start` path — not current |
| `driver-lab/vm/bg/graft-load-then-ac.json` | 577 / 3004 evidence |
| `driver-lab/vm/offline/AuthenticodeGraft.ps1` | Track B measurement only |
