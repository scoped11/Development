# Pass 2 — Flt port gate fingerprint (Rust/EAC + R6/BE research)

**Hypothesis tested:** SystemInformer exposes a **hidden `\\.\` IOCTL bypass** or **weak Flt connect** usable without the real client (mapper / cheat process).

**Verdict:** **Rejected** on static IDA + host probes. Memory R/W exists only as **KPH message ID ~7** behind a **stacked gate**. Not a BYOVD vehicle.

**IDA session:** `a748ba21` | **SHA:** `F2103085…`

## Live / host tests (no `sc start`)

| Check | Result |
|-------|--------|
| `HKLM\…\Services\KSystemInformer` | **Absent** — installed under `Program Files`, not a registered boot service |
| `ksi.sys` / `ksid.sys` next to install | **Missing** (likely shipped inside driver package / loaded only by SI setup) |
| `CreateFile("\\.\KSystemInformer")` etc. | **err 203** (no kernel device object; not err-2-only path) |
| Import table | **No `IoCreateDevice`** xref in IDB — comms surface is **Flt port only** |

Reproduce: `driver-lab/scripts/probe-systeminformer-static.ps1`

## Attack surface map

```
User process
    │  FilterConnectCommunicationPort (needs Flt manager + port name + connect blob)
    ▼
ConnectNotifyCallback
    ├─ SePrivilegeCheck (LUID low part **20** → SeDebugPrivilege)
    ├─ sub_140001F68 → registered KphClient for calling process
    ├─ sub_1400284F8 → “clean” mask (**(flags & 0x2F2) == 0x2F2**)
    ├─ sub_14002891C → connection context + event handle
    │     └─ sub_14003F1EC → sections + **sub_140041010** (copy/verify connect material)
    └─ sub_140002488 → **max 32** live clients (spinlock table)

MessageNotifyCallback
    ├─ Caller process **must match** port cookie process
    ├─ **Usermode only** (`ExGetPreviousMode() == 1`)
    ├─ Header: type **8**, size **≥ 460**, ID **1…0x30**
    ├─ sub_140009900 header validation
    ├─ Per-ID access mask: `(clientCaps & required) == required`
    └─ Dispatch table `unk_1400223A0` (3 slots per ID: validate, handler, access)

Memory R/W primitive: **sub_1400431E4**
  - User VA: `MmCopyVirtualMemory` (needs process handle)
  - Kernel VA: `PsLoadedModuleResource` + `sub_140041174` + `MmCopyMemory` (cap **0x200** stack)
  - Handler wrapper: **sub_14002AA90** → data ref **~message ID 7** in `KSIRO` (`0x140022448` triple)

Crypto: **BCryptVerifySignature** in **sub_1400425DC** / **sub_14004246C** (client attestation chain).

## Dual-driver stack

- **SystemInformer.sys** — minifilter + KPH server (`FltRegisterFilter`, `FltCreateCommunicationPort` in **sub_14002A214**).
- **`KsiInitialize`** in **DriverEntry** path (**sub_140038188**) — **`ksi.sys`** supplies DPC/APC/work-item plumbing.
- Filter port SD: `FltBuildDefaultSecurityDescriptor(0x1F0001)`.

Registry parameters (**sub_14003AF90**): `…\Parameters` values **`PortName`**, **`SystemProcessName`**, **`Altitude`**, **`Flags`** → globals like `qword_140024190` (port name).

String artifact: **`\KSystemInformer`** @ `0x140023720` (internal name; not a user `\\.\` symlink).

## Rust (EAC) / R6 (BattlEye) implication

| Question | Answer |
|----------|--------|
| Map unsigned driver via SI `.sys` alone? | **No** — no IOCTL device, no phys map |
| Custom tool + `SeDebugPrivilege`? | Still needs **signed SI connect blob** + client registration; not a second RTCore |
| Run real SI during Rust/R6? | **Threat actor / AC conflict band** — kernel assist + minifilter + debug priv |
| Research value | **Document gated kernel R/W** for Lab I / cookbook **lane “monitoring driver”** vs **BYOVD tier** |

## Suggested VM-only follow-up (not run here)

1. Install SI in **no-AC VM**, capture one successful connect (Instrument `FilterConnectCommunicationPort` in `SystemInformer.exe`).
2. Diff loaded modules vs your **BFD** JSON quiet vs SI-running.
3. Do **not** use on same session as Rust/R6 per `docs/12_lane_d_split_trust.md`.
