# RE notes — Easy Anti-Cheat (EAC)

**Binary focus:** owned `EasyAntiCheat_EOS.sys` (current EOS) and `EasyAntiCheat.sys` (legacy).  
**Evidence rule:** **Verified** = REA PE tools, Authenticode, Defender/VirusTotal, guest `rea inspect-*` / Ghidra, host objdump, or PE header parse of the staged SHA-256. **Hypothesis** = public GitHub notes not re-proven on this SHA-256.  
**Scope:** static documentation only. No live bypass, hypervisor patches, or cheat payloads. Driver images were **not** loaded. Live Program Files was **not** re-copied this pass.

**Session:** 2026-11-17 (sample + `analyze`), **2026-11-18** (DriverEntry / minifilter dispatch), **2026-11-18 afternoon** (`.pdata` + plaintext EOS `FLT_REGISTRATION` + objdump of VM stubs). Guest `AI_workspace` via `VBoxManage guestcontrol` (share **`Z:\`**, not `D:`). Guest copies: `C:\re\artifacts\` (hash-matched).

---

## Verified kernel facts

Static REA + Ghidra on guest copies under `C:\re\artifacts\` (drivers **not** loaded). Required fields: device names, IOCTL dispatch, IAT, entry.

| Field | EOS `EasyAntiCheat_EOS.sys` `26797A11…` | Legacy `EasyAntiCheat.sys` `DD60A8CD…` |
|-------|-----------------------------------------|----------------------------------------|
| **IAT** | Single import `FLTMGR.SYS!FltRegisterFilter`. Imphash `272647242dc096a27c50c9e914b7a808`. Compiler thunk `FF 25` at `0x140272940`; **zero** `E8` callers. | `FLTMGR.SYS!FltRegisterFilter` + `ntoskrnl.exe` `{__C_specific_handler, __chkstk, PsLookupProcessByProcessId, RtlDeleteRegistryValue, RtlPcToFileHeader, RtlWriteRegistryValue, ZwClose, ZwFlushKey, ZwOpenKey}`. Imphash `015b27e9d0715a67c574c45c768227b4`. Call site `FUN_1400650f8` @ `0x1400658d4`. |
| **Entry** | PE `AddressOfEntryPoint` RVA `0x272978` → `entry` `0x140272978` (43-byte MSVC wrapper) → `FUN_140272948` (**GS cookie** at `0x140319220`; `int 0x29` on fail) → `thunk_FUN_1403a572e` → `FUN_1403a572e` → `FUN_140381041` (R12=`RSP+8`) → `FUN_14038114f` (relocates caller ret by `0xffffffffffc8547d` into `[rsp+0x128]`) → `FUN_140123b1e` `jmp [r12+0x108]+0x41dcaa`. `.pdata` **5633** RUNTIME_FUNCTION rows (`.text` 1493 / `SECTION.7` 4139 / `SECTION.6` 1). | PE entry RVA `0xAFDE8` → `entry` `0x1400afde8` (23-byte trampoline) → `0x14071c246` in unnamed `SECTION.6`: `sub rsp,0x108` then XMM/GPR save frame (VM prolog, **not** a Ghidra procedure). `.pdata` **650** rows (`.text` 649 / `SECTION.6` 1). |
| **Device names** | No plaintext `\Device\…`. UTF-16 `\\EasyAntiCheat_EOS\\` at file `0x30e1ce` sits in an **Authenticode cert-store** string blob (`%s\SOFTWARE\Microsoft\SystemCertificates\%s\Certificates\`, GUIDs `F750E6C3-…` / `00AAC56B-…`) — **not** a `\Device\` path. UM stem `EasyAntiCheat_EOSSys` remains **H-EAC-02**. | Unicode `\Device\EasyAntiCheat` + `\DosDevices\EasyAntiCheat` (file `0xaecc0` / `0xaec70`). Also UTF-16 `\Registry\Machine\Software\Wow6432Node\EasyAntiCheat` at file `0x11db6e`. |
| **IOCTL / dispatch** | **Plaintext minifilter table found** (not IRP `DEVICE_CONTROL`). `FLT_REGISTRATION` at VA **`0x1402f6230`** (file `0x2f5030`, `.rdata`): Size **112** (`0x70`), Version **`0x0203`**, Flags 0, empty `FLT_CONTEXT_REGISTRATION` (`ContextType=0xFFFF` at `0x1402f66e0`). Ops at `0x1402f6720`: `{MJ 0x00 CREATE, Pre 0x14023e237}`, `{MJ 0x06 SET_INFORMATION, Pre 0x14023e237}`, `{MJ 0xFF ACQUIRE_FOR_SECTION_SYNCHRONIZATION, Pre 0x14023e269}`, `{MJ 0x80 END}`. Unload `0x14023e209`. Each `.text` callback is a **5-byte `jmp` into SECTION.7** then the same VM stub as DriverEntry (`mov [r12+0x190], imm64; add eax,[r12+0x108]; jmp rax`). Naive `0x0022xxxx` scan is coincidental. | Minifilter: `FLT_REGISTRATION` `DAT_1401060f0` Size 104 Version `0x0202`; **all instance/name/tx callbacks NULL**; context table is terminator-only (`0xFFFF` at `0x140105d08`). Ops `{MJ 0xFF, Pre FUN_1400650ac → jmp 0x140064e48}` then `{MJ 0x80}`. Real pre-op at `0x140064e48` checks CallbackData, `MajorFunction==0xFF`, `CR8==0` (PASSIVE), then `FUN_1400855d0` dynamic resolve. Unload `0x1400650b4` resolves a second hash (`0x402778e3d078ecd2`) and zeros cookie `DAT_140186fc0`. IRP `DEVICE_CONTROL` body remains the `SECTION.6` blob. |

Evidence IDs: EOS entry `ev_37c1df25…` / real-entry `ev_8177f93e…` / `FUN_140381041` `ev_ce486a5b…` / `FUN_140123b1e` `ev_8ce58779…`; legacy entry `ev_6d24798e…`; `FltRegisterFilter` `ev_1a862564…`. JSON: `ac-captures-host/2026-11-18/`.

---

## Sample identity (this lab)

| Artifact | Bytes | SHA-256 | Source | Guest copy |
|----------|------:|---------|--------|------------|
| `EasyAntiCheat_EOS.sys` | 40 474 272 | `26797A1122781CAF12F1A686E33A0883FA1E2895B40C2249932E319B338841C7` | `C:\Program Files (x86)\EasyAntiCheat_EOS\` | `C:\re\artifacts\EasyAntiCheat_EOS.sys` (hash match) |
| `EasyAntiCheat.sys` | 16 635 000 | `DD60A8CD0771B4FC548FDCEA68EA0FC78FBAAC58A77099731A62E70B03EB1C2A` | `C:\Program Files (x86)\EasyAntiCheat\` | `C:\re\artifacts\EasyAntiCheat.sys` (hash match) |
| `EasyAntiCheat_EOS.exe` | 1 041 336 | `0BBE6D48F45FC155F1C1D029DF8DE67F221B7329010164EDA2DD06D7978340B9` | same EOS dir | UM only (not copied) |

Sidecar: `ac-captures-host/2026-11-17/hashes.json`. `.sys` bodies are gitignored.

**Hash drift:** earlier Rust dump manifest listed EOS SHA-256 `A38FA3D4158FBD4503DC4D4BA303493B25B9DCFEEDB81D4386F2BB71888CBE7C` (42 884 760 bytes, 2026-11-13). **This** on-disk EOS is a newer, smaller image (PE timestamp 2026-10-14). Do not mix IOCTL/RVA tables across hashes.

---

## Verified

| Claim | Evidence | Build / hash |
|-------|----------|--------------|
| EOS PE32+ x64 native driver (`Machine=0x8664`, `Subsystem=1`), 9 sections, image base `0x140000000`, `AddressOfEntryPoint` RVA `0x272978`, PE timestamp `2026-10-14T16:06:08Z` | Host Python PE parse of staged copy | EOS `26797A11…` |
| EOS `DllCharacteristics=0x1E0` = HIGH_ENTROPY_VA \| DYNAMIC_BASE \| FORCE_INTEGRITY \| NX_COMPAT; `Characteristics=0x22` = EXECUTABLE_IMAGE \| LARGE_ADDRESS_AWARE | PE optional header | EOS `26797A11…` |
| EOS **standard IAT is a single import:** `FLTMGR.SYS!FltRegisterFilter` (hint 178). Imphash MD5 `272647242dc096a27c50c9e914b7a808`. No delay-load directory. No export directory. No TLS directory. | REA `inspect_pe_imports` `ev_e86caded…` (host) and guest CLI `inspect-pe-imports` `ev_0aef3bfd…` on `C:\re\artifacts\…` | EOS `26797A11…` |
| EOS load-config size 320; security cookie VA `0x140319220`; GuardFlags raw `256` (`IMAGE_GUARD_CF_INSTRUMENTED` **linker claim only**); Guard CF function table VA `0`; TLS callbacks = none | REA `inspect_pe_load_config` `ev_e0926005…` / guest `ev_acc5c795…` | EOS `26797A11…` |
| EOS RT_VERSION resource present (type 16 / name 1 / lang 1033); payload SHA-256 `ce870b6a6cbd81cdbbd5b626027cc40ff2407c1975d006e05722134623ea4773` | REA `inspect_pe_resources` `ev_7bb58d3e…` | EOS `26797A11…` |
| EOS Authenticode **Valid**; signer **Epic Games Inc.** (DigiCert EV); Windows Defender `MpCmdRun -ScanType 3` **no threats**; VirusTotal **0 detections** (last analysis ~27 days before this session); file also WHQL-cross-signed per VT | `Get-AuthenticodeSignature`; Defender; VT GUI for this SHA-256 | EOS `26797A11…` |
| EOS section 7 (unnamed) is ~37 056 512 raw bytes at VA `0x381000`, chars `0x68000000` (MEM_EXECUTE \| MEM_READ \| MEM_NOT_PAGED, **no** `CNT_CODE`). Matches “large unnamed blob” layout in public EOS notes, **not** the same RVA/size as those notes (those used a 22 MB hash). | PE section table | EOS `26797A11…` |
| Legacy `EasyAntiCheat.sys` PE32+ x64 native, 7 sections, entry RVA `0xAFDE8`, timestamp `2023-09-21T13:05:04Z`, `DllCharacteristics=0x1E0` | PE parse | legacy `DD60A8CD…` |
| Legacy IAT: `FLTMGR.SYS!FltRegisterFilter` **plus** `ntoskrnl.exe` `{__C_specific_handler, __chkstk, PsLookupProcessByProcessId, RtlDeleteRegistryValue, RtlPcToFileHeader, RtlWriteRegistryValue, ZwClose, ZwFlushKey, ZwOpenKey}`. Imphash `015b27e9d0715a67c574c45c768227b4`. No exports. | REA `inspect_pe_imports` `ev_fd8efa9e…` | legacy `DD60A8CD…` |
| Legacy Unicode strings include `\Device\EasyAntiCheat` and `\DosDevices\EasyAntiCheat` | `strings -el` on staged copy | legacy `DD60A8CD…` |
| Legacy PE entry `entry` at `0x1400afde8` is a **23-byte trampoline** (not real DriverEntry). Pseudocode: `(*(code *)&LAB_14071c246)(param_1);` — target `0x14071c246` sits in unnamed `SECTION.6`. Incoming DATA xref from PE header `0x1400000a8`. Ghidra has **no** procedure at `0x14071c246` (undefined blob). | Guest REA `function` `ev_6d24798e…` (reconfirmed 2026-11-18) | legacy `DD60A8CD…` |
| Legacy `FltRegisterFilter` IAT slot RVA `0xb0000`; **one** `.text` call `FF 15` at `0x1400658d4` inside `FUN_1400650f8`. Decompiler: `FltRegisterFilter(param_1, &DAT_1401060f0, &DAT_140186fc0)`. If cookie `DAT_140186fc0 != 0`, returns `0xC0000718`. On success, two APIs are **dynamically resolved** (`FUN_1400855d0` + ROR/XOR constants `0x7ac0820eaa1bd2d7` / `0x402778e3d078ecd2`) and called with the filter cookie. | Guest REA `function` `ev_1a862564…`; host IAT call scan | legacy `DD60A8CD…` |
| Legacy `FLT_REGISTRATION` at `DAT_1401060f0`: Size **104** (`0x68`), Version **`0x0202`**, Flags **0**. `OperationRegistration` → `0x140106160`. 32-byte records: `{MajorFunction=0xFF (IRP_MJ_ACQUIRE_FOR_SECTION_SYNCHRONIZATION), Flags=0, Pre=FUN_1400650ac, Post=0}` then `{MajorFunction=0x80 (IRP_MJ_OPERATION_END)}`. `FilterUnloadCallback` ptr `0x1400650b4`. **InstanceSetup / QueryTeardown / Teardown / GenerateFileName / Normalize* / Transaction / NormalizeEx all NULL.** `ContextRegistration` at `0x140105d08` is terminator-only (`ContextType=0xFFFF`). This is the **minifilter dispatch table**, not an IRP `DEVICE_CONTROL` table. | PE parse `pe-kernel-static.json` + Ghidra names | legacy `DD60A8CD…` |
| Legacy minifilter Pre `FUN_1400650ac` is a 5-byte `jmp 0x140064e48`. Real body: `mov r11,rsp` frame; require `CallbackData` and `FltObjects` non-NULL; `cmp byte [CallbackData.Iopb+4], 0xFF`; `mov rax, cr8; test al,al` (must be PASSIVE); then `FUN_1400855d0` + `ror 7` XOR `0x2123d043afca4a6a` and a second resolve `rol 0xd` XOR `0xc3ac501b89afc032`. Fail path `0x14006508f`. | Host objdump `objdump-legacy-64e48.txt` / `objdump-legacy-650ac.txt` | legacy `DD60A8CD…` |
| Legacy FilterUnload `0x1400650b4`: if cookie `DAT_140186fc0 != 0`, `FUN_1400855d0` + `ror 0xf` XOR `0x402778e3d078ecd2`, `call rax` with cookie, then **zero** the cookie; return 0. | Host objdump of `0x1400650b4` | legacy `DD60A8CD…` |
| Legacy UTF-16 `\Registry\Machine\Software\Wow6432Node\EasyAntiCheat` at file `0x11db6e`. `.pdata` **650** functions (`.text` 649, `SECTION.6` 1). | Host PE/UTF-16 scan | legacy `DD60A8CD…` |
| Legacy `SECTION.6` trampoline target `0x14071c246`: `sub rsp,0x108` then XMM8/7/0/13/15/3/9/5 + R10/R15/R8 saves — same **VM context prolog** shape as EOS `FUN_140381041`, not a plaintext `DriverEntry`. | Host objdump `objdump-legacy-71c246.txt` | legacy `DD60A8CD…` |
| EOS PE entry `entry` at `0x140272978` is a **43-byte MSVC DriverEntry wrapper**, not a packed trampoline. Saves `RBX`/`RDI`, `param_1=RCX` / `param_2=RDX`, calls `FUN_140272948` then `thunk_FUN_1403a572e`. Incoming DATA xrefs: PE header `0x1400000a0` and `.pdata` `0x1426dc5f0`. | Guest REA `function` `ev_37c1df25…` | EOS `26797A11…` |
| EOS real-init chain: `thunk_FUN_1403a572e` (`0x1401a223f`, 5-byte thunk) → `FUN_1403a572e` (`0x1403a572e` in unnamed SECTION.7): `LEA RSP,[RSP-0x1c0]`; `CALL FUN_140381041`; `AND RSP,-10h`; `JMP FUN_140123b1e`. | Guest REA `function` `ev_8177f93e…` | EOS `26797A11…` |
| EOS `FUN_140381041` (`0x140381041`, 270 bytes, start of unnamed SECTION.7) is a **register/XMM save frame**. It sets `R12 = RSP+8` (VM context pointer) and `CALL FUN_14038114f`. | Guest REA `function` `ev_ce486a5b…` | EOS `26797A11…` |
| EOS `FUN_140123b1e` (33 bytes): `mov [r12+0x190], 0x561c7f4b48e431a5`; `jmp [r12+0x108]+0x41dcaa`. Ghidra: **indirect jump, jumptable not recovered**. DriverEntry / IOCTL body after this point is **not** a plaintext `MajorFunction` store in `.text`. | Guest REA `function` `ev_8ce58779…` | EOS `26797A11…` |
| EOS `FUN_140272948` is the MSVC **GS cookie** helper: loads `0x140319220`, compares to `0x2b992ddfa232`, writes `~cookie` to `0x140319228`, or `mov ecx,6; int 0x29` on failure. Called from the DriverEntry wrapper **before** the VM trampoline. | Host objdump `objdump-eos-272948.txt` | EOS `26797A11…` |
| EOS `FUN_14038114f` (29 bytes): `push rax/rbx`; `mov rax,0xffffffffffc8547d`; `mov rbx,[rsp+0x10]` (return of `CALL FUN_14038114f` = `0x1403810ea`); `lea rbx,[rbx+rax]`; `mov [rsp+0x128],rbx`; `pop/ret`. This writes a relocated pointer into the VM save frame, not a plaintext handler table. | Host objdump `objdump-eos-38114f.txt` | EOS `26797A11…` |
| EOS `.pdata` / exception directory: **5633** `RUNTIME_FUNCTION` entries — `.text` **1493**, unnamed `SECTION.7` **4139**, `SECTION.6` **1**. PE entry `0x140272978` is covered (`size=44`). FLT callback VAs `0x14023e209/237/269` have **no** own `.pdata` rows (5-byte jmps). | Host PE parse `pe-kernel-static.json` | EOS `26797A11…` |
| EOS **plaintext `FLT_REGISTRATION`** at VA `0x1402f6230` (file `0x2f5030`): Size **112**, Version **`0x0203`**, Flags 0. `ContextRegistration=0x1402f66e0` (terminator `ContextType=0xFFFF`, no contexts). `OperationRegistration=0x1402f6720`. `FilterUnloadCallback=0x14023e209`. Instance/name/tx/section callbacks all **NULL**. Size/`0x0202` pattern `68 00 02 02` is **absent**. | Host PE parse `pe_kernel_static3.py` | EOS `26797A11…` |
| EOS minifilter ops: MJ **`0x00` IRP_MJ_CREATE** Pre `0x14023e237`; MJ **`0x06` IRP_MJ_SET_INFORMATION** Pre `0x14023e237`; MJ **`0xFF` IRP_MJ_ACQUIRE_FOR_SECTION_SYNCHRONIZATION** Pre `0x14023e269`; MJ **`0x80` END**. | 32-byte records at `0x1402f6720` | EOS `26797A11…` |
| EOS FLT `.text` callbacks are **VM entry stubs**, same shape as `FUN_140123b1e`: `jmp SECTION.7` then `mov [r12+0x190],imm64; mov eax,off32; add rax,[r12+0x108]; jmp rax`. Unload `jmp 0x1403dae9e` / cookie `0x72b509b8134a12d7` / addend `0x448d15`. CREATE/SETINFO `jmp 0x14041e353` / cookie `0x6b2af190dd52023f` / addend `0x3c903b`. Section-sync `jmp 0x1403eeaed` / cookie `0x6a382ae158204cfb` / addend `0x464925`. Bodies after the computed jmp are **not** recovered. | Host objdump `objdump-eos-flt-cb.txt` | EOS `26797A11…` |
| EOS UTF-16 `\\EasyAntiCheat_EOS\\` at file `0x30e1ce` is adjacent to `SOFTWARE\Microsoft\SystemCertificates\%s\Certificates\`, `SoftpubAuthenticode`, and Authenticode GUIDs — a **cert-store path fragment**, not `\Device\EasyAntiCheat_EOS`. No `\Device\` / `\DosDevices\` UTF-16 in this image. | Host UTF-16 scan `pe-kernel-static.json` | EOS `26797A11…` |
| EOS `FltRegisterFilter` IAT RVA `0x30fdc8`; compiler thunk `FF 25` at `0x140272940` (immediately before PE entry). **Zero** `E8` callers of that thunk in any section — the static `FLT_REGISTRATION` is **not** referenced by a `.text` `call` (VM/dynamic call still required). | Host IAT/`FF25` scan of staged copy | EOS `26797A11…` |
| Legacy Authenticode **Valid** (timestamped 2023-09-21; leaf cert expired 2023-10-28 but counter-signed). Signer **EasyAntiCheat Oy**. Defender **no threats**. VT **0 detections**. | Authenticode + Defender + VT | legacy `DD60A8CD…` |
| EOS usermode `EasyAntiCheat_EOS.exe` is PE32 (i386), imports include `tbs.dll` `{Tbsi_Context_Create, Tbsi_GetDeviceInfo, Tbsip_Cancel_Commands, Tbsip_Context_Close, Tbsip_Submit_Command}`, plus service-control APIs (`CreateServiceW`, `StartServiceW`, …) and `IsDebuggerPresent` | REA `inspect_pe_imports` `ev_f3ec604c…` | exe `0BBE6D48…` |
| EOS UM Unicode strings: `EasyAntiCheat_EOSSys`, `Global\EasyAntiCheat_EOSEventDriver`, `SYSTEM\CurrentControlSet\Services\EasyAntiCheat_EOS\Attestation`, TPM quote-key registry paths | `strings -el` on `EasyAntiCheat_EOS.exe` | exe `0BBE6D48…` |
| GitHub code search `filename:EasyAntiCheat_EOS.sys` / `BEDaisy.sys` returned **no** driver bodies. Research clones under `research-clones\` contain **0** `.sys` files. | GitHub API + local clone inventory | — |

**Guest Ghidra `rea analyze` (legacy):** evidence `ev_3313422862d7f747cbf6a1f4feb6df5d45ae223e01367410804612dc7bd7df44`, document `target-dd60a8cd0771.sys`, image base `0x140000000`, **1184 procedures**, **83** defined strings. Unnamed **`SECTION.6`** `0x140192000`–`0x141040600` (15 394 304 bytes). PDB `EasyAntiCheat.pdb` referenced, **not loaded**. JSON: `ac-captures-host/2026-11-17/rea-analyze-legacy.json`.

**Guest Ghidra `rea analyze` (EOS):** evidence `ev_f0a3461e178e09f4d4c64689e14e13767699d38637c1627fc7cc3a66e7c0fbe4`, document `target-26797a112278.sys`, image base `0x140000000`, **6364 procedures**, **94** defined strings. Unnamed **`SECTION.7`** `0x140381000`–`0x1426d8000` (37 056 512 bytes). PDB **`Core.pdb`** (GUID `03f60cd6-11ce-6d40-4c4c-44205044422e`, not loaded). JSON: `ac-captures-host/2026-11-17/rea-analyze-eos.json`.

---

## Hypothesis

| ID | Hypothesis | Confidence | Falsify with |
|----|------------|------------|--------------|
| H-EAC-01 | Remaining kernel APIs (`ObRegisterCallbacks`, process/image notify, etc.) are **dynamically resolved**, which is why the EOS IAT is only `FltRegisterFilter`. Static `FLT_REGISTRATION` exists but is not `.text`-called. | High (layout) | Ghidra xrefs from unnamed section / import decrypt; **not** done on this hash yet |
| H-EAC-02 | IOCTL / **device object** names for **this** EOS build are not in plaintext (unlike legacy `\Device\EasyAntiCheat`). UTF-16 `\\EasyAntiCheat_EOS\\` is a **cert-store** fragment, not a device. UM `EasyAntiCheat_EOSSys` may still be a service/device stem. | Medium | Ghidra `IoCreateDevice` / `IoCreateSymbolicLink` on this SHA |
| H-EAC-03 | Integrity verifier tables described by `experienceds/eac-eos-research` (hash `7102C734…`, 22 MB) **do not apply** to `26797A11…` without a new pass. | High | Compare section map + RVA `0xDB338` trampoline — expected miss |
| H-EAC-04 | `adamvalves` `eac_internals.h` (TPM / stack-walk / import descriptor structs) is a useful **naming hint**, not a sizeof-accurate layout for this build. | Low | REA `procedure_pseudo_code` on DriverEntry path |
| H-EAC-05 | Public note that EOS `ObRegisterCallbacks` returns `OB_PREOP_SUCCESS` while AND-masking handle access is **policy inference from another build**. Do not treat as verified here. | Low | Callback body on **this** image |

Public IOCTL sketches from other hashes stay **out** of the Verified table.

## IOCTLs / devices

| Device / symlink | IOCTL (hex) | Direction | Status | Notes |
|------------------|------------|-----------|--------|-------|
| `\Device\EasyAntiCheat` + `\DosDevices\EasyAntiCheat` | *(no CTL_CODE immediates)* | UM↔KM device objects | **Verified strings on legacy** | Unicode at file `0xaecc0` / `0xaec70`. IRP `DEVICE_CONTROL` handler lives in unnamed `SECTION.6` (`0x14071c246`); not a Ghidra procedure. |
| Legacy minifilter | IRP_MJ `0xFF` then `0x80` | FilterMgr | **Verified `FLT_OPERATION_REGISTRATION`** | Pre `FUN_1400650ac` → `0x140064e48` (section-sync + IRQL check). Not a `FILE_DEVICE_UNKNOWN` IOCTL catalog. |
| EOS kernel device | — | — | **Device name still unverified on `26797A11…`** | UTF-16 `\\EasyAntiCheat_EOS\\` is a cert-store fragment. No `\Device\…` Unicode. Public `\Device\EasyAntiCheat_EOS` stays **H-EAC-02**. |
| EOS minifilter | MJ `0x00`, `0x06`, `0xFF`, `0x80` | FilterMgr | **Verified `FLT_REGISTRATION` `0x1402f6230` v0x0203** | Pre stubs VM-jump into `SECTION.7`. Not a CTL_CODE catalog. |
| EOS IOCTL / IRP MJ table | — | — | **Verified absent as plaintext at DriverEntry** | Entry chain ends in computed `jmp rax` (`FUN_140123b1e`). Naive `0x0022xxxx` immediates are **not** a CTL_CODE table (900+ coincidental hits). |
| *(experienceds / adamvalves tables)* | build-specific | — | Hypothesis / other hash | Do not copy into Verified |

## Components (this install)

| Layer | Artifact | Role (observed) |
|-------|----------|-----------------|
| Kernel (EOS) | `EasyAntiCheat_EOS.sys` | Declared minifilter registration import; large unnamed RX blob |
| Kernel (legacy) | `EasyAntiCheat.sys` | Minifilter import + small ntoskrnl IAT; plaintext `\Device\EasyAntiCheat` |
| Service / UM | `EasyAntiCheat_EOS.exe` | Service install/start, TBS/TPM client imports, named events |
| Game | Hunt Showdown 1896 EOS bundle, Rust (manifest), etc. | Product settings only; kernel body is shared EOS install dir |

## Next RE targets (this hash)

| Priority | Target | REA entry points |
|----------|--------|------------------|
| P0 | Guest Ghidra `rea analyze` on both staged `.sys` copies | **Done** — legacy 1184 procs (`ev_33134228…`); EOS 6364 procs (`ev_f0a3461e…`) |
| P1 | EOS entry RVA `0x272978` | **Done** — wrapper → `FUN_1403a572e` → VM stub `FUN_140123b1e` (`ev_37c1df25…` / `ev_8177f93e…` / `ev_8ce58779…`) |
| P1 | Legacy `FltRegisterFilter` | **Done** — `FUN_1400650f8` + `FLT_REGISTRATION` `DAT_1401060f0` (`ev_1a862564…`) |
| P2 | `FUN_14038114f` / VM context at `[r12+0x108]` | **Partial** — reloc addend + FLT stub cookies/addends documented; computed jmp **not** followed |
| P2 | EOS `FLT_REGISTRATION` `0x1402f6230` | **Done** — Size 112 / v0x0203 / MJ 0,6,FF + VM stubs |
| P2 | EOS `IoCreateDevice` name construction | still encoded; UM stem `EasyAntiCheat_EOSSys` remains H-EAC-02 |
| P3 | Re-check adamvalves / experienceds RVAs | expected mismatch vs 22 MB public hash |

**Out of scope:** live attach, callback unhooking, rollback-to-old-driver PoCs (`itsunderscores/EAC-Rollback` not cloned).

## Dead ends

| Date | Item | Reason |
|------|------|--------|
| 2026-11-17 | Public GitHub `.sys` bodies | `filename:EasyAntiCheat_EOS.sys` / `EasyAntiCheat.sys` code search: no PE; one header dump `EBalloon/EasyAntiCheat-SRC` skipped (not a signed driver) |
| 2026-11-17 | REA `inspect_pe_certificates` on EOS | PKCS#7 ASN.1 nesting > 32; Authenticode still Valid via Windows |
| 2026-11-17 | REA `inspect_binary_hardening` | MCP output schema rejected Windows paths (`path must match ^/`) |
| 2026-11-17 | Manifest hash `A38FA3D4…` | Driver on disk has been updated; old hash is historical only |
| 2026-11-18 | Guest `decompile --file-offset 0x6b3a46` (legacy `0x14071c246`) | `procedure_not_found` — trampoline target is undefined bytes in `SECTION.6`, not a Ghidra function |
| 2026-11-18 | Treat EOS `\\EasyAntiCheat_EOS\\` as `\Device\` | UTF-16 lives in the Authenticode cert-store string blob (`SystemCertificates\%s\Certificates\`) |
