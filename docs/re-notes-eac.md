# RE notes — Easy Anti-Cheat (EAC)

**Binary focus:** owned `EasyAntiCheat_EOS.sys` (current EOS) and `EasyAntiCheat.sys` (legacy).  
**Evidence rule:** **Verified** = REA PE tools, Authenticode, Defender/VirusTotal, guest `rea inspect-*` / Ghidra, host objdump, or PE header parse of the staged SHA-256. **Hypothesis** = public GitHub notes not re-proven on this SHA-256.  
**Scope:** static documentation plus a **lab-only** load of `C:\re\artifacts\` copies on guest `AI_workspace`. No live game, no host Program Files, no DSE disable, no bypass product. Lab services were stopped and deleted after the probe.

**Session:** 2026-11-17 (sample + `analyze`), **2026-11-18** (DriverEntry / minifilter dispatch), **2026-11-18 afternoon** (`.pdata` + plaintext EOS `FLT_REGISTRATION` + objdump of VM stubs), **2026-11-18 evening** (SECTION.6/7 VM interpreter map: handler addends, cookie/`r12` stub, opcode dispatch), **2026-11-18 night** (static lift of cookie-handler bodies after xor/cmp; guest hashes re-checked), **2026-11-18 dynamic** (guest `sc start` of hash-matched artifacts; runtime DOS devices). Guest `AI_workspace` via `VBoxManage guestcontrol` (share **`Z:\`**, not `D:`).

---

## Verified kernel facts

Static REA + Ghidra on guest copies under `C:\re\artifacts\`, then a **lab-only** `sc start` on `AI_workspace` (same hashes). Required fields: device names, IOCTL dispatch, IAT, entry.

| Field | EOS `EasyAntiCheat_EOS.sys` `26797A11…` | Legacy `EasyAntiCheat.sys` `DD60A8CD…` |
|-------|-----------------------------------------|----------------------------------------|
| **IAT** | Single import `FLTMGR.SYS!FltRegisterFilter`. Imphash `272647242dc096a27c50c9e914b7a808`. Compiler thunk `FF 25` at `0x140272940`; **zero** `E8` callers. | `FLTMGR.SYS!FltRegisterFilter` + `ntoskrnl.exe` `{__C_specific_handler, __chkstk, PsLookupProcessByProcessId, RtlDeleteRegistryValue, RtlPcToFileHeader, RtlWriteRegistryValue, ZwClose, ZwFlushKey, ZwOpenKey}`. Imphash `015b27e9d0715a67c574c45c768227b4`. Call site `FUN_1400650f8` @ `0x1400658d4`. |
| **Entry** | PE `AddressOfEntryPoint` RVA `0x272978` → `entry` `0x140272978` (43-byte MSVC wrapper) → `FUN_140272948` (**GS cookie** at `0x140319220`; `int 0x29` on fail) → `thunk_FUN_1403a572e` → `FUN_1403a572e` → `FUN_140381041` (R12=`RSP+8`) → `FUN_14038114f` (relocates caller ret by `0xffffffffffc8547d` into the `[r12+0x108]` stack gap) → `FUN_140123b1e` `jmp [r12+0x108]+0x41dcaa` → trampoline `0x140424211` → handler **`0x141b1df86`**. `.pdata` **5633** RUNTIME_FUNCTION rows (`.text` 1493 / `SECTION.7` 4139 / `SECTION.6` 1). | PE entry RVA `0xAFDE8` → `entry` `0x1400afde8` (23-byte trampoline) → `0x14071c246` in unnamed `SECTION.6`: `sub rsp,0x108` then XMM/GPR save frame (VM prolog, **not** a Ghidra procedure). **0** EOS-style cookie stubs. `.pdata` **650** rows (`.text` 649 / `SECTION.6` 1). |
| **Device names** | **Runtime (2026-11-18):** `\\.\EasyAntiCheat_EOS` **opened** (CreateFile access 0, win32=0) while minifilter `EasyAntiCheat_EOS` was RUNNING (`fltmc` altitude **370010**, 6 instances). `\\.\EasyAntiCheat_EOSSys` still win32=2. Static image still has **no** plaintext `\Device\…`; UTF-16 `\\EasyAntiCheat_EOS\\` at file `0x30e1ce` remains a cert-store fragment. | **Runtime:** `\\.\EasyAntiCheat` **opened** (win32=0) while minifilter `EasyAntiCheat` was RUNNING (`fltmc` altitude **370020**, 6 instances). Matches static Unicode `\Device\EasyAntiCheat` + `\DosDevices\EasyAntiCheat` (file `0xaecc0` / `0xaec70`). |
| **IOCTL / dispatch** | **Plaintext minifilter table found** (not IRP `DEVICE_CONTROL`). `FLT_REGISTRATION` at VA **`0x1402f6230`** (file `0x2f5030`, `.rdata`): Size **112** (`0x70`), Version **`0x0203`**, Flags 0, empty `FLT_CONTEXT_REGISTRATION` (`ContextType=0xFFFF` at `0x1402f66e0`). Ops at `0x1402f6720`: `{MJ 0x00 CREATE, Pre 0x14023e237}`, `{MJ 0x06 SET_INFORMATION, Pre 0x14023e237}`, `{MJ 0xFF ACQUIRE_FOR_SECTION_SYNCHRONIZATION, Pre 0x14023e269}`, `{MJ 0x80 END}`. Unload `0x14023e209`. Each `.text` callback is a **5-byte `jmp` into SECTION.7** then the same VM stub as DriverEntry (`mov [r12+0x190], imm64; add eax,[r12+0x108]; jmp rax`). Naive `0x0022xxxx` scan is coincidental. | Minifilter: `FLT_REGISTRATION` `DAT_1401060f0` Size 104 Version `0x0202`; **all instance/name/tx callbacks NULL**; context table is terminator-only (`0xFFFF` at `0x140105d08`). Ops `{MJ 0xFF, Pre FUN_1400650ac → jmp 0x140064e48}` then `{MJ 0x80}`. Real pre-op at `0x140064e48` checks CallbackData, `MajorFunction==0xFF`, `CR8==0` (PASSIVE), then `FUN_1400855d0` dynamic resolve. Unload `0x1400650b4` resolves a second hash (`0x402778e3d078ecd2`) and zeros cookie `DAT_140186fc0`. IRP `DEVICE_CONTROL` body remains the `SECTION.6` blob. |

Evidence IDs: EOS entry `ev_37c1df25…` / real-entry `ev_8177f93e…` / `FUN_140381041` `ev_ce486a5b…` / `FUN_140123b1e` `ev_8ce58779…`; legacy entry `ev_6d24798e…`; `FltRegisterFilter` `ev_1a862564…`. JSON: `ac-captures-host/2026-11-18/` (`vm-interpreter-summary.json`, `vm-interpreter-map.json`, `vm-interpreter-follow.json`, `handler-lift-summary.json`, `handler-lift.json`, `handler-lift-follow.json`, `dyn-lab-load.json`, `dyn-lab-minifilter.json`).

---

## Dynamic lab load (AI_workspace, 2026-11-18)

Guest `AIWORKSPACE` only. Images: `C:\re\artifacts\{EasyAntiCheat_EOS,EasyAntiCheat,BEDaisy}.sys` hash-matched. Live Program Files paths **absent** on the guest. `bcdedit` showed **no** `testsigning` / `nointegritychecks` / `debug`. Device Guard CI enforcement **2** (enforced), VBS **0**, HVCI unset, Vulnerable Driver Blocklist **1**. DSE was **not** turned off. WinDbg/kd **not** installed; REA native runtime tools are Linux-only — no kernel IOCTL dump.

| Step | EOS `26797A11…` | Legacy `DD60A8CD…` |
|------|-----------------|-------------------|
| Bare `sc create` + `sc start` as `LabDynEAC*` | Create SUCCESS. Start **ERROR 2**. Filter Manager event 5: `FltRegisterFilter` **0xC0000034** (`STATUS_OBJECT_NAME_NOT_FOUND`). CI did **not** reject the image. | Same: ERROR 2 + FltMgr **0xC0000034**. |
| Retry with `Services\<name>\Instances` + unused altitude | Service **RUNNING**. FltMgr event 6: loaded. `fltmc`: `EasyAntiCheat_EOS` altitude **370010**, **6** instances. | Service **RUNNING**. `fltmc`: `EasyAntiCheat` altitude **370020**, **6** instances. |
| DOS device probe (CreateFile access 0, **no** `DeviceIoControl`) | `\\.\EasyAntiCheat_EOS` **opened** win32=0. `\\.\EasyAntiCheat_EOSSys` win32=2. | `\\.\EasyAntiCheat` **opened** win32=0. |
| Cleanup | `sc stop` + `sc delete` SUCCESS. No leftover services. | Same. |

**IOCTL codes:** not captured. Drivers stayed loaded only long enough for the existence probe; no WinDbg `!devobj` / dispatch dump; no IOCTL payloads sent.

Logs: `ac-captures-host/2026-11-18/dyn-lab-probe.txt`, `dyn-lab-load.txt`, `dyn-lab-minifilter.txt`, `dyn-lab-ci-events.txt`.

---

## VM interpreter (SECTION.6 / SECTION.7)

Static pattern scan + host objdump of staged `26797A11…`. Drivers **not** loaded. Guest hashes on `AI_workspace` (`C:\re\artifacts\`) match.

**Where the VM lives:** unnamed **SECTION.7** (`0x140381000`–`0x1426d8000`, 37 056 512 RX, 4139 `.pdata` rows) is the interpreter + handler bodies. EOS **SECTION.6** is a small 33 KiB exec blob with **1** `.pdata` row — not the handler store. Legacy keeps the same save-frame idea in **its** SECTION.6 (`0x14071c246`) but has **zero** EOS cookie stubs.

### Cookie / r12 stub

`FUN_140381041` (`0x140381041`, start of SECTION.7) is the VM **context prolog**. It does **not** `sub rsp`; it writes into the `0x1c0` frame allocated by `FUN_1403a572e`, then:

1. `CALL FUN_14038114f` — `mov rax,0xffffffffffc8547d`; add to the return address (`0x1403810ea`); store at `[rsp+0x128]` **while** two pushes + the `call` are on the stack → lands in the 8-byte gap at `[rsp+0x110]` = **`[r12+0x108]`**.
2. Finishes spilling GPRs/XMMs (RCX at `[rsp+0x60]`, RDX at `[rsp+0x88]`, …).
3. `LEA R12,[RSP+0x8]` / `RET` — **r12 is the VM context pointer**.

Twin stub at `0x140381b12` uses a second reloc constant `0xffffffffffc84ab4`. Continuation stub `0x140381bca`: `rbp=[r12+0x198]`; `rax=[[r12+0x190]]`; `jmp rbp` (cookie slot treated as a pointer).

**Context slots (from save-frame + handler reads):**

| r12 offset | Role |
|------------|------|
| `+0x58` | saved RCX (DriverEntry `DriverObject`) |
| `+0x80` | saved RDX (`RegistryPath`) |
| `+0x108` | **dispatch base** = `0x140006567` after reloc |
| `+0x190` | **cookie** (per-stub imm64; handlers `xor`/`cmp` it) |
| `+0x198` | continuation target (indirect `jmp rbp`) |

MSVC **GS cookie** at `0x140319220` (`FUN_140272948`) is a **different** object from the VM cookie at `[r12+0x190]`.

### Opcode dispatch

Not a 256-entry `jmp [table+opcode*8]` (0 `FF 24 C5` hits; no 32+ consecutive image-VA QWORD runs). Dispatch is a **computed jump**:

```
movabs rax, <cookie>          ; unique per stub
mov    [r12+0x190], rax
mov    eax, <addend>
add    rax, [r12+0x108]       ; vm_base
jmp    rax
```

| Count | What |
|------:|------|
| **8144** | complete cookie stubs (`.text` 2324 / SECTION.7 5820) |
| **8144** | unique cookies (1:1 with stubs) |
| **3903** | unique addends (handler selectors; max 4 stubs share one addend) |
| **14809** | `mov [r12+0x190], rax` |
| **14761** | `add rax, [r12+0x108]` |
| **4676 / 415 / 635** | `mov rax/rbx/rcx, [r12+0x190]` (cookie consumed inside handlers) |

Every unique addend lands on a **5-byte `jmp rel32`**, then a real body starting `push rbp; mov rbp, rsp` (3899/3903; 4 are `REX.WB`). Final bodies: SECTION.7 **3768**, `.text` **135**. Handler VA = `0x140006567 + addend`.

The **handler table is the stub set itself** (cookie + addend pairs), not a pointer array in `.rdata`.

### Known handler landings (followed)

| Site | Addend | Trampoline | Body |
|------|--------|------------|------|
| DriverEntry `FUN_140123b1e` | `0x41dcaa` | `0x140424211` | `0x141b1df86` — spills `[r12+0x58/0x80/…]`, loads cookie into `rbx`, `xor`/`cmp` vs immediates |
| FilterUnload | `0x448d15` | `0x14044f27c` | `0x1421c00e0` |
| Pre CREATE / SET_INFORMATION | `0x3c903b` | `0x1403cf5a2` | `0x140e85f02` |
| Pre ACQUIRE_FOR_SECTION_SYNCHRONIZATION | `0x464925` | (jmp rel32) | `0x142618c9e` |

Objdump: `ac-captures-host/2026-11-18/objdump-eos-handler-driverentry.txt`, `objdump-eos-handler-create.txt`, `objdump-eos-381041.txt`, `objdump-eos-381b12.txt`, `objdump-eos-handler-driverentry-after-xor.txt`, `objdump-eos-vm-shift-frame-381325.txt`, `objdump-eos-vm-restore-38116c.txt`.

### After cookie `xor` / `cmp` (static lift)

Capstone census of all **3903** unique addend bodies (window 0x280, cookie taken from one stub per addend) + Unicorn file-byte emu of the four named landings. Image bytes only; **not** a loaded driver.

| Census | Count |
|--------|------:|
| handlers scanned | **3903** |
| cookie `xor` in window | **3220** |
| cookie `cmp` (imm via `movabs` into the other operand) | **3551** |
| nested cookie stub in window | **5** (7 kind hits) |
| `jmp <reg>` in window | **12** |
| no cookie ops in window | **190** |
| xor-decrypted immediates | **11289** |
| xor-decrypted values that are image VAs | **0** |

Cookie `xor` immediates are MBA keys, not recovered function pointers. `cmp`/`setb`/`seta` against consecutive constants fold to 0/1 once the stub cookie is substituted.

**Named runtime helpers (SECTION.7):**

| Analyst name | VA | Role |
|--------------|----|------|
| `EOS_vm_save_frame` | `0x140381041` | existing: spill GPRs/XMMs, `r12=rsp+8` |
| `EOS_vm_restore_frame` | `0x14038116c` | inverse of the save frame (reload XMM/GPRs from `[rsp+…]`) |
| `EOS_vm_shift_frame` | `0x140381325` | `rax=[r12+0x190]` cookie used as copy length; `rbx=[r12+0x198]` continuation; `mov rsp,r12`; `movups` 0x10-byte copy loop; `jmp rbx` |
| `EOS_vm_continuation` | `0x140381bca` | existing: `rbp=[r12+0x198]`; cookie slot treated as pointer; `jmp rbp` |

**Known handler bodies after xor/cmp (Unicorn on mapped PE):**

| Handler | Body | After xor/cmp | Stop |
|---------|------|---------------|------|
| DriverEntry | `0x141b1df86` | MBA `xor`/`cmp`/`setcc` then **676** insns | `jmp rax` → **`EOS_vm_shift_frame` `0x140381325`** |
| FilterUnload | `0x1421c00e0` | MBA then context reads | unmapped read @ 323 insns (fake `r12` objects) |
| Pre CREATE / SET_INFORMATION | `0x140e85f02` | cookie `xor` into `rcx`; `setb`/`seta`; `imul` kills stack-derived term; reconstructed `movups` into a VM slot | still in-body at 200 insns (no `jmp reg`) |
| Pre ACQUIRE_FOR_SECTION_SYNCHRONIZATION | `0x142618c9e` | MBA then | `jmp rax` → `0x1403d4566` (`jmp 0x140f1bd25`) → **another** `push rbp` handler that reloads `[r12+…]` |

`0x1403d4566` sits in a trampoline cluster: leave (`mov rsp,r12; call EOS_vm_restore_frame; lea rsp,[rsp+0x1c0]; call [rsp+8]`) and re-enter (`call EOS_vm_save_frame`) are **adjacent** bytes, not the taken edge. The taken edge is `jmp 0x140f1bd25` (chained handler).

**Static limit:** after the cookie check, bodies are more MBA + VM frame shift / chained handlers. No plaintext `IoCreateDevice`, `FltRegisterFilter` call, or `CTL_CODE` table appears in the lifted suffix. Runtime lab load later opened `\\.\EasyAntiCheat_EOS` (H-EAC-02 DOS name). IRP codes still unknown.

**Inference (not runtime):** addend ≈ virtualized basic-block / handler id; cookie ≈ per-call-site MBA key. Handler “work” is expressed as chained VM blocks, not a recovered C function.

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
| EOS FLT `.text` callbacks are **VM entry stubs**, same shape as `FUN_140123b1e`: `jmp SECTION.7` then `mov [r12+0x190],imm64; mov eax,off32; add rax,[r12+0x108]; jmp rax`. Unload `jmp 0x1403dae9e` / cookie `0x72b509b8134a12d7` / addend `0x448d15` → body `0x1421c00e0`. CREATE/SETINFO `jmp 0x14041e353` / cookie `0x6b2af190dd52023f` / addend `0x3c903b` → body `0x140e85f02`. Section-sync `jmp 0x1403eeaed` / cookie `0x6a382ae158204cfb` / addend `0x464925` → body `0x142618c9e`. | Host objdump `objdump-eos-flt-cb.txt` + follow `vm-interpreter-follow.json` | EOS `26797A11…` |
| EOS VM interpreter: **8144** complete cookie stubs, **8144** unique cookies, **3903** unique addends. Dispatch = `[r12+0x108]+addend` then `jmp rel32` then `push rbp` handler. No 256-entry pointer table. Cookie loads from `[r12+0x190]` in handlers (4676 `rax` / 415 `rbx` / 635 `rcx`). Twin r12 stub `0x140381b12`. | `map_vm_interpreters.py` + `vm-interpreter-summary.json` | EOS `26797A11…` |
| EOS cookie-handler lift: **3220/3903** bodies `xor` the cookie in the first 0x280; **3551** `cmp` it; **0** xor-decrypted immediates are image VAs; nested stubs **5**. DriverEntry `0x141b1df86` emu **676** insns then `jmp rax` **`EOS_vm_shift_frame` `0x140381325`**. Section-sync emu `jmp rax` `0x1403d4566` → handler `0x140f1bd25`. `EOS_vm_restore_frame` `0x14038116c` is the save-frame inverse. | `lift_vm_handlers.py` / `handler-lift-summary.json` / `handler-lift-follow.json`; objdump `objdump-eos-vm-shift-frame-381325.txt` | EOS `26797A11…` |
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
| H-EAC-02 | IOCTL / **device object** names for **this** EOS build are not in plaintext (unlike legacy `\Device\EasyAntiCheat`). UTF-16 `\\EasyAntiCheat_EOS\\` is a **cert-store** fragment, not a device. UM stem `EasyAntiCheat_EOSSys` is **not** the DOS name. | **DOS name resolved** — runtime `\\.\EasyAntiCheat_EOS` opened; `\\.\EasyAntiCheat_EOSSys` does **not** exist. IRP `DEVICE_CONTROL` codes still unknown. | Lab CreateFile (done). WinDbg `!devobj` blocked: no kd on guest. |
| H-EAC-03 | Integrity verifier tables described by `experienceds/eac-eos-research` (hash `7102C734…`, 22 MB) **do not apply** to `26797A11…` without a new pass. | High | Compare section map + RVA `0xDB338` trampoline — expected miss |
| H-EAC-04 | `adamvalves` `eac_internals.h` (TPM / stack-walk / import descriptor structs) is a useful **naming hint**, not a sizeof-accurate layout for this build. | Low | REA `procedure_pseudo_code` on DriverEntry path |
| H-EAC-05 | Public note that EOS `ObRegisterCallbacks` returns `OB_PREOP_SUCCESS` while AND-masking handle access is **policy inference from another build**. Do not treat as verified here. | Low | Callback body on **this** image |

Public IOCTL sketches from other hashes stay **out** of the Verified table.

## IOCTLs / devices

| Device / symlink | IOCTL (hex) | Direction | Status | Notes |
|------------------|------------|-----------|--------|-------|
| `\Device\EasyAntiCheat` + `\DosDevices\EasyAntiCheat` | *(no CTL_CODE captured)* | UM↔KM device objects | **Verified strings + runtime `\\.\EasyAntiCheat`** | Unicode at file `0xaecc0` / `0xaec70`. Lab CreateFile opened `\\.\EasyAntiCheat` (win32=0) while the minifilter was RUNNING. No `DeviceIoControl` sent. |
| Legacy minifilter | IRP_MJ `0xFF` then `0x80` | FilterMgr | **Verified `FLT_OPERATION_REGISTRATION` + runtime `fltmc`** | Pre `FUN_1400650ac` → `0x140064e48`. Lab: `EasyAntiCheat` altitude 370020, 6 instances. |
| EOS kernel device | — | UM↔KM | **Runtime `\\.\EasyAntiCheat_EOS`** | Lab CreateFile opened `\\.\EasyAntiCheat_EOS` (win32=0). `\\.\EasyAntiCheat_EOSSys` win32=2. Static UTF-16 `\\EasyAntiCheat_EOS\\` is still a cert-store fragment. |
| EOS minifilter (runtime) | MJ `0x00`, `0x06`, `0xFF`, `0x80` | FilterMgr | **Verified load** | `fltmc`: `EasyAntiCheat_EOS` altitude 370010, 6 instances. FltMgr event 6. |
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
| P2 | `FUN_14038114f` / VM context at `[r12+0x108]` | **Done (static)** — vm_base `0x140006567`; 8144 stubs; 3903 handlers followed through trampoline to `push rbp` bodies; DriverEntry body `0x141b1df86` |
| P2 | EOS cookie-handler bodies after xor/cmp | **Done (static limit)** — census 3220 xor / 3551 cmp; DriverEntry → `EOS_vm_shift_frame`; section-sync chains to `0x140f1bd25`; no plaintext device/IOCTL in the lifted suffix |
| P2 | EOS `FLT_REGISTRATION` `0x1402f6230` | **Done** — Size 112 / v0x0203 / MJ 0,6,FF + VM stubs |
| P2 | EOS `IoCreateDevice` name construction | **Done (runtime DOS name)** — `\\.\EasyAntiCheat_EOS`. `EasyAntiCheat_EOSSys` is **not** the device. IRP IOCTL codes still need WinDbg/`!devobj` (kd not on guest). |
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
| 2026-11-18 evening | 256-entry VM opcode pointer table in EOS | 0 `jmp [rax*8+abs]`; no 32+ image-VA QWORD runs. Dispatch is stub addends. |
| 2026-11-18 night | Treat cookie `xor` immediates as recovered pointers | **11289** decrypted values, **0** in-image. They are MBA keys; DriverEntry continues to `EOS_vm_shift_frame`, not `IoCreateDevice`. |
| 2026-11-18 dynamic | Treat `\\.\EasyAntiCheat_EOSSys` as the EOS device | Lab CreateFile win32=2. Real DOS name is `\\.\EasyAntiCheat_EOS`. |
