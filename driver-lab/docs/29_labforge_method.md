# LabForge — lab Authenticode (not Microsoft countersign)

**LabForge** is DriverLab’s **own-driver signing method**. It is **not** cryptographic forgery of Microsoft’s keys, **not** sigthief grafting, and **not** Corsair/RTCore BYOVD.

What “forge” means **here**: on **DriverLab-E**, we **manufacture a complete local trust story** so **our** `.sys` presents as **Valid Authenticode**, loads with **testsigning off**, then we attach the client and start **your** AC replicas.

```
LF0  Lab CA in Root + TrustedPublisher (Ensure-OfflineLabPublisher)
LF1  Sign VmLabDrv.sys  (signtool /fd SHA256 /ph)
LF2  Supplemental WDAC   (publisher allowed under 26xx default CI)
LF3  Load (NtLoad / sc)  → attach VmLabClient
LF4  Start replica L2    (EAC / BE / ACE dumps)
LF5  Survive             (same client IOCTL)
```

## What it is / is not

| Claim | LabForge |
|-------|----------|
| Looks signed in Explorer / `Get-AuthenticodeSignature` on the **lab VM** | Yes, if LF0–LF1 applied |
| Loads without testsigning watermark on that VM | Yes, if LF2 authorized + reboot |
| Microsoft Hardware attestation / WHQL | **No** — that is `attestation/` + EV + portal |
| Stolen EV / grafted PKCS#7 / Flame-style collision | **Not implemented** — disk identity of an inbox victim is `31_signed_module_replica.md` |
| Corsair / MyPort / RTCore as the payload | **No** — those are vehicles; LabForge payload is **VmLabDrv** |
| Live EAC/BE/ACE on a clean PC | **No** — unknown publisher; replicas only |

## Relation to OTL

**OTL** (`28_otl_trusted_load_method.md`) is the **ordered harness**.  
**LabForge** is the **signing/trust recipe** OTL uses at O0–O2.

| Piece | Script |
|-------|--------|
| CA + stores | `vm/offline/Ensure-OfflineLabPublisher.ps1` |
| Sign | `vm/offline/Sign-LabDriver.ps1` |
| CI | `Build-LabCiSupplemental.ps1` + `Apply-LabCiSupplemental.ps1` |
| Load | `Install-OfflineLabRw.ps1` |
| Full AC order | `vm/bg/guest-labforge-ac.ps1` |
| Looks like **another** signed module | `31_signed_module_replica.md` + `guest-signed-module-replica.ps1` (victim disk, not lab CA) |

## Guest one-liner

```powershell
# First boot of a clone (sign + CI + reboot):
& Z:\driver-lab\vm\bg\guest-establish-lab-trust.ps1 -Phase once

# After reboot — LabForge + replica survive (default rust EAC):
& Z:\driver-lab\vm\bg\guest-labforge-ac.ps1 -Profile rust
```

Artifacts: `vm/bg/labforge-ac-result.json`, `vm/bg/otl-legitimacy.json`.

## Fail closed

`NtLoad` **`0xC0000428`** / `sc` **577** means CI did **not** authorize the publisher (supplemental not enforced). Re-run `guest-establish-lab-trust.ps1 -Phase once` and **reboot** — do not switch to Corsair for this method.