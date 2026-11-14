# Signed-module replica (borrowed identity)

**Status:** scripts are in tree. A full guest pass is **not** recorded yet (`signed-module-replica.json` is written only when the guest script runs). PKCS#7 **graft** (copy another driver’s Authenticode blob onto `VmLabDrv.sys`) is **not** implemented and will not be.

Lab L2 replica plus a payload that **enumerators treat as another already-signed module**. Disk stays the **inbox victim** (Microsoft Authenticode). Execution is **in that image’s memory** (stomp). This is **not** live ranked.

```
SM0  Victim on disk     wcifs.sys / luafv.sys — Authenticode Valid, Microsoft publisher
SM1  Snapshot hash      SHA-256 of the victim file
SM2  AC off             replica stopped
SM3  Stomp map          Driver2Stomp into victim .text (MyPort vehicle, then hygiene)
SM4  Appearance gate    same SHA-256, still Valid, Win32_SystemDriver still that name
SM5  Client             Driver2Client --device \\.\WCIFS probe
SM6  Replica L2         Start-AcReplicaServices (captured EAC/BE/ACE .sys, vendor-signed dumps)
SM7  Survive            same client probe with replica up
```

## What “looks like another signed module” means here

| Observer | What it should see |
|----------|--------------------|
| Explorer / `Get-AuthenticodeSignature` on **disk** | Victim file, **Valid**, original publisher |
| SCM / `driverquery` | Victim service / path (`System32\drivers\wcifs.sys`) |
| Replica dump `.sys` | Captured vendor signature **copied**, never re-signed with the lab CA |
| Memory / IOCTL | Payload at RIP inside victim range (stomp gates in `STOMP_BYPASS.md`) |

It does **not** mean Windows or ranked AC believe you are Microsoft WHQL, or that `VmLabDrv.sys` carries someone else’s PKCS#7.

## Scripts

| Script | Role |
|--------|------|
| `vm/bg/guest-signed-module-replica.ps1` | Appearance before/after, stomp, client, replica |
| `vm/bg/guest-stomp-lane-full.ps1` | Forwards to the script above (`-AcProfile`, `-Victim`, `-SkipStomp`) |
| `vm/offline/Test-SignedModuleAppearance.ps1` | Valid Authenticode, optional Microsoft subject, SHA-256 match, optional loaded |
| `vm/replica/Install-AcReplicaL2.ps1` | Records `authenticode` + `signer` on copied leaf dumps |

## Guest

```powershell
& Z:\driver-lab\vm\bg\guest-signed-module-replica.ps1 -Profile rust -Victim wcifs
```

Appearance-only (no map): `-SkipStomp`. Same entry via `guest-stomp-lane-full.ps1 -AcProfile rust -Victim wcifs`.

Artifacts: `vm/bg/signed-module-replica.json`, `signed-module-before.json`, `signed-module-after.json`.

## Fail closed

- Victim Authenticode not Valid → pick another inbox driver; do not graft.
- Hash changed after map → treat as corrupt / wrong tool; restore snapshot.
- Mapper timeout / `0x3B` → reboot snapshot; do not switch this lane to Corsair.
- Replica kernel `sc` 577 on the **dump** `.sys` → dump copy or CI, not this identity story.

Related: `STOMP_BYPASS.md`, `15_test_ac_replicas.md`, `29_labforge_method.md` (different story: **your** file signed by the lab CA).
