# Kernel drivers: Windows architecture, I/O, security and authorized lab study

**Research edition:** 12 November 2026 | **Scope:** cross-platform Windows kernel-mode driver engineering, defensive security mechanisms, and permitted development fixtures. This chapter deepens the short kernel sections in [01_UNIFIED_CHEAT_ARCHITECTURES.md](01_UNIFIED_CHEAT_ARCHITECTURES.md) and [10_ANTICHEAT_AND_SECURITY.md](10_ANTICHEAT_AND_SECURITY.md). It is **not** a guide to loading unauthorized drivers, disabling platform protections, bypassing anti-cheat, or acquiring protected-process memory. Sources: [13_SOURCE_CATALOG.md](13_SOURCE_CATALOG.md) (K01–K16). Examples are illustrative unless a run record establishes execution.

## 1. Why kernel drivers matter in this research collection

Three ideas are often conflated and must stay separate:

| Idea | Meaning | What it does **not** imply |
|---|---|---|
| **Kernel-mode driver** | A module running in supervisor mode with access to kernel APIs, IRPs, and (when permitted) physical/device resources | Unrestricted read of every process or invisible operation |
| **Publisher kernel anti-cheat** | A vendor-maintained kernel component described in public policy (e.g. RICOCHET, Vanguard) [G04,G02] | A public specification of IOCTLs, offsets, or detection rules |
| **User-mode “driver-assisted” tool** | A separate process plus a **documented** OS or vendor driver path (handle rights, filter stack) | Legitimate entitlement to tamper with a protected title |

Kernel placement increases **visibility and capability surface** for *authorized* components (security products, storage, GPUs, acquisition devices with kernel transport). It also increases **fault domain**: a bug can destabilize the whole system. Chapters 01 and 10 explicitly reject ranking kernel placement as “safest” or “invisible” relative to user-mode or DMA architectures.

## 2. Privilege and execution context

On Windows x64, user-mode applications run with restricted access to virtual address spaces and kernel objects. Kernel-mode code runs at a higher privilege level and uses a different set of APIs (WDM/KMDF/NDIS/WDF headers). The **process** that initiated an I/O request and the **thread context** in which a dispatch routine runs are not always the same; highest-level drivers (e.g. file systems) often run in the originating application thread at **PASSIVE_LEVEL** [K05].

| Context | Typical caller | Typical use in legitimate software |
|---|---|---|
| User mode | Application, service | Games, overlays, documented `ReadProcessMemory` with rights granted by policy |
| Kernel mode | Driver dispatch, DPC, ISR | Device I/O, filtering, security callbacks, power management |
| Firmware / hypervisor | Platform, VBS | Measured boot, memory integrity (HVCI), IOMMU policy [K08,H01] |

**Research rule:** A capability in kernel mode is only established by tracing **which API**, **which object**, **which access mask**, and **which policy gate** (signing, CI, protected process, callback stripping) actually permits it—not by the word “driver” in a product name.

## 3. Driver models and stack position

Microsoft’s current recommended path for new kernel drivers is **KMDF** (Kernel-Mode Driver Framework) on top of WDM concepts [K01]. **UMDF** runs part of the stack in user mode with a kernel reflector—useful for devices that do not need full kernel privilege [K02]. Legacy **WDM** samples and third-party stacks still appear in the field.

| Model | Runs in | Typical role | Analysis note |
|---|---|---|---|
| KMDF function driver | Kernel | Device-specific logic, IOCTL handling | Start from WDK KMDF Echo / Toaster samples [K03] |
| WDM legacy | Kernel | Older stacks, some security filters | IRQL and locking rules stricter; more manual IRP handling |
| UMDF 2 | User (+ reflector) | USB, printers, some class drivers | Smaller kernel footprint; not “weak security” by default |
| Filter driver | Kernel (upper/lower) | Interpose on existing stack (storage, network) | Position in stack determines which IRPs are visible |
| Bus driver | Kernel | Enumerate child devices | PnP and power IRPs dominate |

A **driver is not one binary**. Installation packages may include `.sys`, coinstallers, catalog files, and user-mode components. Version each artifact separately in evidence records.

## 4. I/O architecture: devices, IRPs, and IOCTLs

### 4.1 Objects and stacks

Applications open devices through the Win32 / NT namespace (`\\.\DeviceName` → `CreateFile`). The I/O manager builds **IRPs** (I/O Request Packets) and passes them down a **device stack** of `DEVICE_OBJECT` layers. Each layer has dispatch routines for major functions (`IRP_MJ_READ`, `IRP_MJ_WRITE`, `IRP_MJ_DEVICE_CONTROL`, …) [K04].

Understanding a driver means answering:

1. Which **device object** does user mode open?
2. Which **dispatch routine** handles the major function?
3. For IOCTLs, what is the full **32-bit control code** and **transfer type**?
4. What **buffer lengths** and **access rights** are validated?

### 4.2 IOCTL buffer methods

For `IRP_MJ_DEVICE_CONTROL`, the transfer method is encoded in the IOCTL value [K06,K07]:

| Method | I/O manager behavior | Engineering note |
|---|---|---|
| **METHOD_BUFFERED** | Copies in/out via system buffer | Recommended default; still requires length checks and zeroing sensitive leftovers [K07] |
| **METHOD_IN_DIRECT / OUT_DIRECT** | MDL for locked user pages | High-performance paths; check `MmGetSystemAddressForMdlSafe` failures [K07] |
| **METHOD_NEITHER** | Passes user VA + length | Caller thread context; highest manual validation burden; avoid when possible [K07] |

**Security design checklist** (standard facts from Microsoft driver security guidance [K07,K08]):

- Define IOCTL function codes ≥ 0x800 for vendor-private IOCTLs; set **RequiredAccess** so the I/O manager enforces handle rights.
- Compare the **entire 32-bit** IOCTL in dispatch; never match partial bitfields.
- Honor `InputBufferLength` / `OutputBufferLength`; never read or write past the supplied buffer.
- Zero driver-allocated buffers that return data to user mode.
- Optional stricter checks: `IoValidateDeviceIoControlAccess` for read/write rights on the handle [K07].

### 4.3 Synthetic IOCTL fixture (owned lab, not a game interface)

Illustrative contract for a KMDF Echo-style test driver:

```text
CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)
Input:  struct { uint32_t magic; uint32_t length; }  // magic = 0x45435631 ("ECV1")
        followed by `length` bytes payload (max 4096 in fixture)
Output: same layout, payload reversed byte-for-byte
Errors: STATUS_INVALID_PARAMETER (bad magic/length), STATUS_BUFFER_TOO_SMALL
```

**Expected lab outcomes:** matching reverse buffer; rejection at `length > 4096`; no pool leak across 10⁴ iterations (Driver Verifier pool tracking on a test VM). **Negative case:** user mode passes `length = 0xFFFFFFFF` → driver must fail without allocating.

## 5. IRQL, threading, and stability

**IRQL** (Interrupt Request Level) constrains which kernel APIs are legal. Violations cause undefined behavior or bug checks [K05,K09].

| IRQL (simplified) | Masking | Typical driver work |
|---|---|---|
| PASSIVE_LEVEL | None | `DriverEntry`, most dispatch, `DriverUnload`, worker threads |
| APC_LEVEL | APCs | Some paging-path read/write dispatch |
| DISPATCH_LEVEL | APC + dispatcher | `DpcForIsr`, `IoTimer`, some power paths |
| DIRQL | Device interrupt level | ISR, synchronized miniport sections |

**Rules of thumb for research reading (not implementation shortcuts):**

- Dispatch at PASSIVE_LEVEL may use pageable memory and blocking waits; at DISPATCH_LEVEL and above, do not wait indefinitely or touch pageable code/data carelessly [K05].
- Callback paths registered with `ObRegisterCallbacks` run at **≤ APC_LEVEL** [K10]; keep pre-operation callbacks short; defer heavy work per Microsoft guidance [K11].
- IRQL mismatches are a common **legitimate** failure mode when auditing third-party drivers (stability), not proof of malicious intent.

## 6. Memory: pools, MDLs, and user pointers

Kernel drivers allocate from **nonpaged pool** (today: prefer **NonPagedPoolNx**—non-executable) for structures that must be reachable at raised IRQL [K08]. **MDLs** describe locked physical pages for direct I/O. User virtual addresses passed into kernel must be validated in the correct thread context; probing and capture rules differ by method [K06].

| Mechanism | Legitimate use | Failure to document |
|---|---|---|
| NonPagedPoolNx allocation | Driver state, IOCTL staging buffers | Pool tag leaks, use-after-free |
| MDL + direct I/O | DMA engines, high throughput | Mapping failure, double-fetch races |
| `MmCopyVirtualMemory` (when available to caller) | Cross-process copy in **authorized** tools | Wrong target, race with unmapped memory |

**HVCI / memory integrity:** Kernel code must not rely on writable+executable pages or self-modifying kernel text. Drivers intended for production should follow HVCI-compatible practices: NX pool, no W+X sections, no dynamic kernel codegen [K08,K12]. This is a **platform requirement** trend, not a cheat-specific patch.

## 7. Signing, loading policy, and lifecycle

64-bit Windows requires **kernel-mode code signing** subject to policy edition [K13]. Practical buckets:

| Deployment | Typical signing / policy | Research stance |
|---|---|---|
| Retail driver (WHQL / attestation) | Microsoft-trusted chain | Treat as production artifact with version + catalog hash |
| Enterprise / test VM | Test signing (`TESTSIGNING`), Secure Boot off if documented | **Only** on isolated lab machines; never mixed with protected-game testing on main OS |
| Blocked / revoked | CI blocklist, expired cert | Record `Code Integrity` event; do not instruct circumvention |

**Code Integrity** and **Driver Signing Policy** enforce which `.sys` files load. **Virtualization-based security** (VBS) and **HVCI** further restrict what runs in kernel [K08,K12]. Publisher anti-cheat components are distributed as signed products; their **internal** IOCTLs and data structures remain **unresolved** in public research unless disclosed.

Unload and upgrade paths matter: filter drivers must unregister callbacks (`ObUnRegisterCallbacks`) before unload [K10]. PnP remove IRPs must drain queues. Evidence of “ghost” callbacks after crash is a stability finding, not a game-offset finding.

## 8. OS mechanisms relevant to process and handle security

These are **documented platform features** used by security and management software. Descriptions below are for defensive architecture literacy [K10,K11,K14].

### 8.1 ObRegisterCallbacks

`ObRegisterCallbacks` registers pre/post routines for **handle create/duplicate** on process, thread, and desktop objects [K10]. A pre-callback may **strip** requested access rights (e.g. remove `PROCESS_VM_READ`) before the handle is granted. Requirements and constraints:

- Callbacks must reside in a **signed** kernel image (`STATUS_ACCESS_DENIED` otherwise) [K10].
- Registration uses an **altitude** string; collisions return `STATUS_FLT_INSTANCE_ALTITUDE_COLLISION` [K10].
- Callbacks should be minimal: no user-mode round trips, no blocking IPC, no unsafe reentrancy [K11].

**Defensive reading for external diagnostics:** If a product relies on `OpenProcess` + memory read, **another kernel module** may reduce granted rights on protected targets. That is a policy interaction, not proof that user-mode reads are impossible in every configuration.

### 8.2 Process / thread / image notify routines

`PsSetCreateProcessNotifyRoutineEx`, load-image notify, and related APIs fire on process/thread/image events [K11,K14]. Anti-cheat and EDR designs may use these for **telemetry** (new modules, suspicious parents). Public docs do not map notify usage to specific games.

### 8.3 ETW and WMI

**Event Tracing for Windows** can be emitted from kernel providers (see WDK EventDrv sample category [K03]). Security analytics often correlate ETW with user-mode events. ETW configuration is out of scope for game memory layout research but belongs in **platform security** dossiers.

## 9. Kernel drivers in the cheat-architecture picture (engineering only)

Relating chapter 01 architectures to **this** chapter:

| Data path | Kernel involvement | Trust boundary crossed |
|---|---|---|
| Pure user external | None (only user APIs) | Process handle rights, ACLs, `ReadProcessMemory` failures [M03] |
| Vendor security driver | May filter handles, collect telemetry | Competes with other kernel registrants; policy not public |
| Custom unsigned kernel module | Would require disabled CI / test policy | **Out of scope** for authorized lab; widens legal and stability risk |
| DMA stack | Kernel **transport** driver for USB/PCIe device | Device buffers and IOMMU—not arbitrary VA [H01,H05] |
| Publisher anti-cheat | Signed product driver + user service | Publisher-stated scope only [G04,G02] |

No row implies bypass of server authority (chapter 06). Kernel read of client memory still only exposes **client-visible** state.

## 10. Relationship to DMA and LeechCore

LeechCore and similar libraries may use **vendor-supported kernel drivers** to talk to hardware or authorized memory sources [H05,H08]. That is a **device/driver contract**, not automatic visibility into an arbitrary protected process. Chapter 01’s layer list (FPGA → USB bridge → acquisition library → interpreter) should record **which `.sys`** is loaded, its version, and whether the source is loopback, crash dump, or driver-registered buffer—not assume “kernel driver” means full RAM.

## 11. Authorized lab plan (proposed, not executed)

**Lab H — KMDF IOCTL echo on isolated test VM**

| Step | Input | Expected | Negative |
|---|---|---|---|
| H1 | WDK KMDF Echo sample built for test-signing VM | Device opens, buffered read/write works | Mismatched arch / unsigned on retail CI |
| H2 | Custom ECV1 IOCTL per §4.3 | Payload reversed; lengths enforced | Oversized length rejected |
| H3 | Driver Verifier enabled (pool, IRQL) | No bugchecks in 1h fuzz of IOCTL | IRQL violation → stop and capture dump |
| H4 | ObCallback sample (optional) protecting notepad.exe [K10] | Stripped handle blocks kill until deprotect | Document altitude string used |
| H5 | Document CI/HVCI state on VM | Manifest records VBS on/off | Do not disable on production gaming image |

Outputs: `driver_manifest.json` (`.sys` hash, cert thumbprint, OS build, CI policy), `ioctl_fuzz.jsonl`, verifier log hash. Grade per chapter 12 rubric: **fixture-verified** only after real runs.

## 12. Evidence record extension

Add optional fields when `kind` relates to kernel artifacts:

```json
{
  "record_id": "kmdf-echo-fixture-001",
  "kind": "ioctl_contract",
  "driver_sha256": "<actual hash>",
  "signing": {"publisher": "<CN>", "thumbprint": "<sha1>", "catalog": "<path or none>"},
  "ioctl": "0x222000",
  "transfer_type": "METHOD_BUFFERED",
  "irql_observed": "PASSIVE_LEVEL",
  "tool": "WDK <version>",
  "status": "unresolved",
  "limitations": ["illustrative; no lab run"]
}
```

## 13. Common analysis mistakes

1. Treating **any** `.sys` as equivalent to **physical memory access**.
2. Inferring IOCTL semantics from a single `CTL_CODE` constant without dispatch disassembly.
3. Confusing **filter altitude** with **load order guarantees** across Windows builds.
4. Using publisher marketing (“kernel-level anti-cheat”) as a **detection certainty** claim.
5. Promoting **test-signing lab** behavior to **retail OS** expectations.
6. Ignoring **IRQL** when reading decompiled driver pseudocode (apparently valid API calls may be illegal at that level).

## 14. Status and boundaries

| Topic | Status in this edition |
|---|---|
| WDM/KMDF/IOCTL/IRQL/signing/HVCI | Standard facts tied to Microsoft documentation [K01–K13] |
| ObRegisterCallbacks / notify routines | Standard platform semantics; sample behavior from Microsoft samples [K10,K11] |
| Publisher anti-cheat drivers | Publisher-reported high-level role only [G04,G02,G09] |
| Lab H | Proposed; **not executed** |
| Protected-title driver reverse engineering | **Out of scope** |
| Circumvention (DSE off, blocked cert load, callback unlinking) | **Not documented** |

## Sources (K-series)

| ID | Link | Use |
|---|---|---|
| K01 | [Introduction to kernel-mode driver architecture](https://learn.microsoft.com/en-us/windows-hardware/drivers/gettingstarted/) | WDK overview |
| K02 | [User-Mode Driver Framework](https://learn.microsoft.com/en-us/windows-hardware/drivers/wdf/user-mode-driver-framework-architecture) | UMDF role |
| K03 | [General driver samples](https://learn.microsoft.com/en-us/windows-hardware/drivers/samples/general-driver-samples) | Echo, ObCallback, IOCTL samples |
| K04 | [Handling IRPs](https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/handling-irps) | IRP flow |
| K05 | [Dispatch routines and IRQLs](https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/dispatch-routines-and-irqls) | IRQL by routine |
| K06 | [Methods for accessing data buffers](https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/methods-for-accessing-data-buffers) | Buffered/direct/neither |
| K07 | [Driver security checklist](https://learn.microsoft.com/en-us/windows-hardware/drivers/driversecurity/driver-security-checklist) | IOCTL and pool rules |
| K08 | [Security issues for I/O control codes](https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/security-issues-for-i-o-control-codes) | IOCTL definition rules |
| K09 | [Managing hardware priorities (IRQL)](https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/managing-hardware-priorities) | IRQL API restrictions |
| K10 | [ObRegisterCallbacks](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-obregistercallbacks) | Handle callback registration |
| K11 | [Kernel-mode process/thread manager guidelines](https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/windows-kernel-mode-process-and-thread-manager) | Callback best practices |
| K12 | [Implement HVCI-compatible code](https://learn.microsoft.com/en-us/windows-hardware/drivers/driversecurity/implement-hvci-compatible-code) | Memory integrity |
| K13 | [Kernel-mode code signing policy](https://learn.microsoft.com/en-us/windows-hardware/drivers/install/kernel-mode-code-signing-policy--windows-vista-and-later-) | Signing matrix |
| K14 | [PsSetCreateProcessNotifyRoutineEx](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntddk/nf-ntddk-pssetcreateprocessnotifyroutineex) | Process notify API surface |
| K15 | [Driver Verifier](https://learn.microsoft.com/en-us/windows-hardware/drivers/devtest/driver-verifier) | Lab hardening tool |
| K16 | [Control Device IOCTL sample](https://learn.microsoft.com/en-us/samples/microsoft/windows-driver-samples/ioctl/) | Transfer type demonstration |

## 15. BYOVD in lab work (stability, not evasion)

**BYOVD** (bring your own vulnerable driver) means loading a **still-validly-signed but flawed** kernel driver and using a known bug to reach privileged read/write—not loading arbitrary unsigned code with one click. Microsoft’s [vulnerable driver blocklist](https://learn.microsoft.com/en-us/windows/security/application-security/application-control/app-control-for-business/design/microsoft-recommended-driver-block-rules) and the Defender ASR rule *Block abuse of exploited vulnerable signed drivers* target this class; see [15_BYOVD_AND_DRIVER_TRUST.md](15_BYOVD_AND_DRIVER_TRUST.md) when present.

**Project stability lessons** (masterlist / NAL map path, reported 2026-11):

| Failure mode | Mechanism | Mitigation |
|---|---|---|
| Post-map BSOD | Preemptive AC-neutralize IOCTLs on every map | Default **RW-only**: map + `IOCTL_READ_PROCESS_MEM` only; pass `--ac-preempt` or `--full` only in isolated lab |
| Load-time BSOD | DriverEntry auto-patching EAC/BE and image-load notify nuking AC `DriverEntry` | Default: `g_allowAutoAcNeutralize=0`; mapper `--rw-only` skips post-map AC IOCTL sweep; cleanup must not `ControlService(STOP)` BYOVD after map (`g_mapSucceeded`) |
| Exit BSOD | `ControlService(STOP)` on BYOVD while mapped code still uses its `DRIVER_OBJECT` | Delete service entry only; leave `.sys` loaded until reboot (`--no-cleanup` also valid) |
| Read faults | `KeStackAttachProcess` + blind `RtlCopyMemory` | Prefer `MmCopyVirtualMemory` when resolved |

Do not disable HVCI, blocklist, or Secure Boot on a daily-use OS to “fix” map failures—use a test VM or a signed/test-signing lab per chapter 12.

Cross-references: **M03** (`ReadProcessMemory`), **H01/H05/H08** (DMA transport drivers), **G04/G02/G09** (publisher kernel components), **12** (evidence grading).
