# driver2 — Easy Anti-Cheat (EOS) integration

First-class **Rust / EOS** path: map, **`0x844`** improve + **`0x851`** encrypted pointers + bypass sweep.

## Quick map (VM lab)

```powershell
.\driver2\build.ps1
.\bin\Driver2Runner.exe --profile rust --mode bypass --force-map --inspector
# aliases: eac, eac-rust, eos
.\driver2\everything.ps1 -Profile eac -Target vm -Execute -ForceMap
```

## IOCTL stack (EAC profile)

| IOCTL | Role |
|-------|------|
| **`0x844`** | EAC improve (`EAC_IMP_*` — OB, reports, CR3, handles, minifilter, …) |
| **`0x851`** | EOS encrypted pointer lab (detect/decrypt/static/runtime) |
| **`0x869`** | EAC/BE max-tier bypass bundle |
| **`0x81B`** | Preempt callback strip |

Mapper sends **early `0x844` + `0x851`** after strip on `--ac-replica rust` + `--driver2`.

## Usermode API

```cpp
Driver2EacImprove(h, EAC_IMP_ALL);
Driver2EosEncPtr(h, EAC_ENC_PTR_ALL);
Driver2ReadProcess(h, pid, va, buf, n);
```

CLI:

```text
Driver2Client.exe eac-improve
Driver2Client.exe eac-encptr
Driver2Client.exe probe
```

## Capture

`dumps/rust-eac-eos_*` — `EasyAntiCheat_EOS.sys`, `Settings.json` (product/sandbox/deployment ids).

## Verify

```powershell
.\byovd\scripts\verify-pf-diag.ps1 -Profile rust
```

Expect **`0x844`**, **`0x851`**, **`0x869`**, **callback strip** in `%TEMP%\pf_diag.tmp`.

## Gates

```powershell
.\driver2\developer-approval-gate.ps1 -Execute -ForceMap -Inspector
```

## Live ranked (honest)

Rust + EAC on the game PC is usually **Tier 0** for RTCore/BYOVD. Use **Lane D** (`lane-d-preflight.ps1 -Game rust`) for live reads; map only on **no-EAC VM**.
