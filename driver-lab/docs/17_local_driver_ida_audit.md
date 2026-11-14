# Local driver IDA audit (your machine)

Goal: find **kernel drivers you already have** that expose BYOVD-style primitives **and are not already public LOL drivers** (see [19_greenfield_vehicle_hunt.md](../../byovd/docs/19_greenfield_vehicle_hunt.md)). Anything on LOLDrivers is blocked on Win11 policy + paid EAC/BE — IDA it for education, not for live mapper. Work in a **lab VM** when testing exploits.

## One-shot inventory

```powershell
cd driver-lab\scripts
.\enumerate-local-drivers.ps1 -CopyToInbox -IncludeMicrosoftWhcp
```

Outputs under `driver-lab/audit/`:

| File | Contents |
|------|----------|
| `driver_inventory_<date>.csv` | Every `driverquery` path + signer + SHA256 |
| `driver_byovd_heuristic_<date>.csv` | Tier 0–3 from `dumpbin /imports` |
| `inbox\` | Copies of high-signal / third-party `.sys` (safe to point IDA at) |

**Tier heuristic (quick, not proof):**

- **3** — three or more danger imports (e.g. `MmMapIoSpace` + `ZwMapViewOfSection` + HAL bus R/W)
- **2** — one or two danger imports
- **0** — none matched (VPN filter drivers often land here)

Always confirm in IDA: IOCTL dispatcher, access checks, pointer validation.

## IDA via `user-ida-pro-mcp`

1. Ensure IDA MCP is connected in Cursor (`user-ida-pro-mcp`, port **13337**).
2. **Copy** `.sys` into `driver-lab/audit/inbox/` (Program Files and `system32` are awkward for IDA write locks).
3. Open a session:

```text
idb_open  input_path=<absolute path to .sys>
```

Every other call needs `database=<session_id>` from the open result.

### Recommended tool sequence

| Step | MCP tool | What you learn |
|------|----------|----------------|
| 1 | `survey_binary` | Imports, strings (`\\Device\\`, `PhysicalMemory`), large functions |
| 2 | `find_regex` or `search_text` | `MmMapIoSpace`, `IRP_MJ_DEVICE_CONTROL`, device names |
| 3 | `decompile` on IOCTL handler | IOCTL codes, size checks, user buffers |
| 4 | `analyze_function` on map/read helpers | Physical path, missing bounds |
| 5 | `xrefs_to` on `DeviceControl` major func | Reachability from IOCTL table |

### BYOVD checklist (per driver)

- [ ] `MajorFunction[IRP_MJ_DEVICE_CONTROL]` dispatches on `IoControlCode`
- [ ] User buffer via `METHOD_NEITHER` / unprobed pointers
- [ ] `MmMapIoSpace` / `\\Device\\PhysicalMemory` / port I/O (`__in`/`__out`)
- [ ] `HalGetBusDataByOffset` / `HalSetBusDataByOffset` without ACL
- [ ] MSR (`__readmsr` / `__writemsr`) — ring-0 CPU control
- [ ] Symbolic link ACL (`\\DosDevices\\...`) — who can open `\\.\Name`?

### Blocklists (Windows 11 + EAC/BE paid tier)

```powershell
cd driver-lab\scripts
# Refresh LOLDrivers CSV may take up to ~2 minutes
.\check-driver-blocklists.ps1 -RefreshLoldrivers
.\check-driver-blocklists.ps1
```

| Column | Meaning |
|--------|---------|
| `Win11_VulnBlocklist` | SHA in [LOLDrivers](https://www.loldrivers.io/) → blocked when `VulnerableDriverBlocklistEnable=1` (+ Defender ASR rule) |
| `EAC_PaidTier_Heuristic` / `BE_PaidTier_Heuristic` | Filename/device policy in `data/ac_eac_be_vehicle_policy.json` — **not** Epic/Krafton’s private list |
| Registry | `HKLM\...\CI\Config\VulnerableDriverBlocklistEnable`, HVCI under `DeviceGuard\...\HypervisorEnforcedCodeIntegrity` |

On your host (2026-11-13): **blocklist OFF**, HVCI off — LOLDrivers hits still matter on other machines and when you turn policy on.

## Findings on this host (2026-11-13)

Static pass only — **not** proof of remote exploitability.

| Driver | State | BYOVD signal | Notes |
|--------|-------|----------------|-------|
| **RTCore64.sys** | In `byovd/vehicles/` | **Known LOL** | IOCTL `0x80002000+`: port I/O, MSI HAL PCI, `MmMapIoSpace` map/read/write (`0x80002040`–`0x8000204C`), MSR R/W. Already your mapper vehicle. |
| **ACE-GAME.sys** | Stopped | **High imports** | `MmMapIoSpace`, `MmGetPhysicalAddress`, `ZwMapViewOfSection`, HAL PCI — needs full IOCTL review; signed Tencent / WHCP. |
| **EasyAntiCheat_EOS.sys** | Running | No danger imports in quick scan | Large image; IOCTL surface worth IDA for logic bugs, not classic phys map. |
| VPN / ViGEm / npf | Mixed | None in import scan | WFP / tunnel / bus — poor BYOVD vehicles. |

### RTCore64 (IDA session reference)

- Device: `\\DosDevices\\RTCore64`
- Phys map helper uses `HalTranslateBusAddress` + `MmMapIoSpace` (`sub_11384`)
- Dispatcher: `sub_11438` — IOCTL cases include physical R/W and MSR

## MCP gaps (optional additions)

Current stack is enough for deep work if you use **IDA MCP** + **`enumerate-local-drivers.ps1`**. Optional extras that would speed triage:

| Tool idea | Why |
|-----------|-----|
| **PE/import MCP** (read-only) | Tier scoring without VS `dumpbin` |
| **LOLDrivers CSV matcher** | Auto-flag SHA256/name against public list |
| **Copy-to-inbox** MCP | One call from chat → `driver-lab/audit/inbox` |
| **driverquery → JSON** | Faster than shell for agents |

You do **not** need another decompiler if `user-ida-pro-mcp` stays up; robrun-ida is for IL2CPP only (`dma/docs/IDA_MCP.md`).

## Legal / hygiene

You stated you own these binaries. Keep exploitation tests on **isolated VMs**. Do not load unknown vulnerable drivers on your daily Rust/EAC partition (see `byovd/docs/12_lane_d_split_trust.md`).
