# Pass 1 — SystemInformer.sys (KSystemInformer)

**SHA256:** `F21030859857E7CE3C7069E4BAFFBABC921FE15C18B6F63A99E2B7C25B7C1071`  
**IDA:** `a748ba21`  
**Installed:** `C:\Program Files\SystemInformer\` — **not** in `driverquery` (not loaded on this host)

## Live probe (no `sc start`)

| Path | Result |
|------|--------|
| `\\.\KSystemInformer` | **err 2** (not present — driver not running) |
| `\\.\SystemInformer` / `\\.\KProcessHacker3` | err 2 |

## Architecture (not classic `\\.\` IOCTL driver)

- **Minifilter** (`FltRegisterFilter`, `FltCreateCommunicationPort`, `FltSendMessage`)
- **`ksi.dll`** exports (`KsiInitialize`, DPC/APC helpers) — dual driver stack
- **Client auth:** `BCryptVerifySignature`, `SeGetCachedSigningLevel`, `SystemProcessName` registry strings
- PDB path: `KSystemInformer\bin\Release64\SystemInformer.pdb`

Legacy `\\.\KSystemInformer` **DeviceIoControl** table is **not** the primary surface on this build.

## Memory primitives

**`sub_1400431E4`** (called from message handler):

| Mode | Mechanism |
|------|-----------|
| User VA (per-process) | `ObReferenceObjectByHandle` + **`MmCopyVirtualMemory`** |
| Kernel / high VA | `PsLoadedModuleResource` + **`MmCopyMemory`** (chunk cap **0x200** on stack, else pool) via `sub_140041174` validation |

This is **KPH-style cross-process / kernel read-write**, gated by **filter port messages** from a **verified System Informer client**, not arbitrary handles from `mapper_nal.exe`.

## LOLDrivers

This **SHA** is **not** in the CSV row we checked. Older **KProcessHacker** builds **are** on LOLDrivers (`kprocesshacker.sys` tags) — same family, different binary.

## Mapper vehicle?

**No** — for the same reasons as KslD, but different gate:

| Requirement | SystemInformer |
|-------------|----------------|
| Load path | SI service / filter attach |
| Call path | **Flt port** + signed client protocol |
| Map unsigned Betest | Still need **separate** code execution primitive |
| Drop into `mapper_nal --driver` | **Not supported** without reimplementing the entire KPH message + crypto layer |

## Lab-only next step (if you insist)

On an **isolated VM**: install SI, let it load the driver, capture **Flt communication** from `SystemInformer.exe` (Wireshark won’t help — kernel port). That’s a **protocol RE** project, not a one-line vehicle swap.

**On Rust/EAC host:** expect block like other KPH-class tools.

**Pass 2 (gates, no IOCTL bypass):** [PASS2_gate_fingerprint.md](PASS2_gate_fingerprint.md)
