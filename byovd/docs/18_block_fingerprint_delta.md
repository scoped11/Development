# Method 18 — Block fingerprint delta (BFD)

**LOLDrivers entries are already blocked** everywhere that matters for paid titles — don’t hunt them as vehicles ([19_greenfield_vehicle_hunt.md](19_greenfield_vehicle_hunt.md)).

Public filename guesses also **do not** tell you what **production EAC/BE** blocks on **your** build. Method **18** measures **observable deltas** on a host you own, **without** `sc start`, `sc create`, or dropping new `.sys` files.

It answers three questions:

| Question | Evidence lane |
|----------|----------------|
| Would **Windows** block this hash if policy were on? | LOLDrivers + `VulnerableDriverBlocklistEnable` ([check-driver-blocklists.ps1](../../driver-lab/scripts/check-driver-blocklists.ps1)) |
| Did **this machine** ever CI-block a driver image? | Code Integrity event archaeology |
| Does **running AC** change access to known vehicle **devices**? | Device open matrix (CreateFile only — no service install) |

Lab confirmation (load vehicle on **replica VM** only) stays in [16_timed_ac_harness_method.md](16_timed_ac_harness_method.md).

## Procedure

### Step 0 — Manifest

```powershell
Copy-Item byovd\docs\lab_manifest.template.json byovd\docs\lab_manifest.json
# fill os_build, hvci, vulnerable_driver_blocklist
```

### Step 1 — Static pass (no kernel side effects)

```powershell
cd driver-lab\scripts
.\enumerate-local-drivers.ps1 -CopyToInbox
.\check-driver-blocklists.ps1
```

Produces inventory + `driver_blocklist_report_<date>.csv`.

### Step 2 — BFD capture (this method)

```powershell
.\block-fingerprint-delta.ps1 -OutJson ..\audit\bfd_<date>.json
```

**Optional:** run twice on the **same** host:

1. **AC quiet** — no Rust/R6/EAC/BE processes.
2. **AC live** — game launcher up until matchmaking (allow **up to ~2 minutes** for EAC/BE to attach kernel components).

Compare JSON: any device where `Win32Error` flips (e.g. `2` not found → `5` access denied) while `AcSeen=true` is a **Tier-0 block fingerprint** — stronger than a static list.

### Step 3 — Novel vehicle hunt (static only)

From `driver_byovd_heuristic_<date>.csv`, take rows where:

- `LOLDrivers = no`
- `Tier >= 2`
- `ThirdParty = true`

IDA those inbox copies ([17_local_driver_ida_audit.md](../../driver-lab/docs/17_local_driver_ida_audit.md)). Goal: find a **unsigned-policy-friendly** primitive that is **not** on LOLDrivers yet (rare; treat as research, not production).

### Step 4 — Lab only

Timed harness on replica VM — never on daily game partition ([12_lane_d_split_trust.md](12_lane_d_split_trust.md)).

## Interpreting device open errors

| `Win32Error` | Meaning |
|--------------|---------|
| 2 | `ERROR_FILE_NOT_FOUND` — device not present (service not loaded / driver not running) |
| 5 | `ERROR_ACCESS_DENIED` — object exists but caller denied (ACL **or** AC hook) |
| 3 | `ERROR_PATH_NOT_FOUND` — rare symlink edge |

**BFD rule:** same device, `AcSeen=false` → err 2, `AcSeen=true` → err 5 ⇒ treat as **AC device gate** (paid-tier behavior proxy).

Opening `\\.\RTCore64` does **not** load `RTCore64.sys` if the service is stopped; it only tests whether a device object already exists.

## CI event archaeology

Event IDs to mine (last 365 days):

- **3087** — vulnerable driver blocklist block (when enabled)
- **3033** / **3004** — signing level failures (often `.sys` / `.dll`)

The script extracts paths ending in `.sys` into the JSON artifact. Empty list does **not** prove a driver is allowed on all hosts — only that **this** machine did not log it.

## Relation to cookbook

| Cookbook | BFD use |
|----------|---------|
| **R1** Lane D | Production game PC: run Step 2 live vs quiet; if Tier-0 fingerprint → split trust, no map on that kernel |
| **G0** gates | Combine with `ac-preflight.ps1` exit codes |
| **TH2** | Lab half unchanged |

## What this method is not

- Not a leaked EAC/BE internal deny list.
- Not proof a driver is **safe** — only whether **your** session / OS **already** exhibited block signals.
- Not a substitute for IOCTL review on any candidate you might load in a VM.
