# OTL — Ordered Trust Load (lab method)

**OTL** is DriverLab’s **non-forgery** way to exercise **your** test AC replicas against a driver that **looks and loads like production-signed code** on the VM: Authenticode valid, **testsigning off**, supplemental WDAC allows the publisher, then **IOCTL survives** after L2 replica start.

Distinct from:

| Lane | What loads | Signing story |
|------|------------|---------------|
| **BYOVD / stomp** | Mapped payload in signed victim | Victim is legit; your code is **memory**, not a signed `.sys` |
| **Partner attestation** | `Driver2Signed` + `.cat` | Real Microsoft catalog (private hosts only) |
| **OTL** | `VmLabDrv.signed.sys` via vault `NtLoadDriver` | **Lab publisher + CI supplement + local root** |

OTL does **not** claim live Rust/R6 bypass—only **your L2 replica pass criteria** with the **mandatory order** below.

## Mandatory order (do not reorder)

AC docs that say “map then start replica” apply to **mapper_nal**. OTL is the **service-load** lane:

```
O0  Legitimacy gate   → staged .sys verifies; BCD production; CI supplemental authorized
O1  AC quiescent      → replica kernel/user processes stopped (nothing blocking load)
O2  Load driver       → Install-OfflineLabRw (signed image only)
O3  Attach client     → VmLabClient (\\.\VmLab0 session + marker R/W)
O4  Arm replica       → Install-AcReplicaL2 -LoadKernel -StartUsermode (+ profile setup)
O5  Survive probe     → VmLabClient again (must pass with AC up)
```

If O5 fails, you learned **detection under replica**, not that signing failed. Fix bypass IOCTLs or use stomp lane separately.

## One-shot guest run

```powershell
# After guest-establish-lab-trust.ps1 -Phase once (CI + reboot) at least once:
& Z:\driver-lab\vm\bg\guest-otl-method.ps1 -Profile rust -Phase full
```

Phases: `o0` … `o5`, `full`, `status`.

Artifacts: `vm/bg/otl-method-result.json`, `vm/bg/otl-legitimacy.json`.

## O0 legitimacy rubric (what “looks legit” means here)

| Check | Pass |
|-------|------|
| `Get-AuthenticodeSignature` | `Valid` |
| `signtool verify /pa /ph` | exit 0 |
| `testsigning` / `nointegritychecks` | **No** |
| Supplemental policy (`driverlab-ci.json`) | **Authorized** + **Enforced** after reboot |
| Publisher in stores | Root + TrustedPublisher (Import-LabPublisher) |

This is **legitimate on your VM**, not “Microsoft WHQL.” AC replicas still see **unknown publisher**. Grafting a captured vendor PKCS#7 onto `VmLabDrv` is **not** implemented. To have enumerators see an **already** Microsoft-signed inbox module, use borrowed identity: `31_signed_module_replica.md`.

## Host staging

```powershell
# Elevated host once:
driver-lab\vm\offline\host-prepare-offline-lab.ps1
driver-lab\vm\offline\Test-LabDriverLegitimacy.ps1 -SysPath driver-lab\vm\vault\staged\VmLabDrv.signed.sys
```

Guest builds supplemental (ConfigCI) via `guest-establish-lab-trust.ps1 -Phase once`.

## Manifest snippet

```json
"otl": {
  "profile": "rust",
  "legitimacy_ok": true,
  "load_st": "0x0",
  "client_before_ac": 0,
  "client_after_ac": 0,
  "testsigning": false
}
```

## L2 kernel hardening

Replica `.sys` files are mirrored to `%SystemRoot%\System32\drivers\` and started with  
`binPath=\SystemRoot\System32\drivers\<name>.sys` (not Program Files).  
OTL **O4** fails closed unless `sc query` reports **RUNNING** for each profile kernel service (`-StrictKernel`).

## See also

- `26_vm_kernel_driver_lab.md` — Betest/mapper parity (different order)
- `../../byovd/docs/15_test_ac_replicas.md` — replica L2 definitions
- `vm/bg/guest-establish-lab-trust.ps1` — CI + sign bootstrap
- `vm/offline/Test-LabDriverLegitimacy.ps1` — O0 implementation