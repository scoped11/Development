# RE notes — BattlEye (BE)

**Binary focus:** offline `BEDaisy.sys` copied from `C:\Users\bj284\Downloads\TraeTesting\cursor\Reversing\BEDaisy.sys` (not live Program Files BattlEye). Unturned’s installer still did **not** drop a `.sys` under Common Files; `BEService` was left **Stopped**.  
**Evidence rule:** **Verified** only for files in this lab. Public BEDaisy IOCTL tables stay **Hypothesis** unless re-proven on SHA-256 `E7BCF344…`.  
**Scope:** static documentation only. No live bypass, no callback interception, no cheat payloads, **drivers not loaded**. `BEService` was **not** started. Live BattlEye under Program Files was **not** touched.

**Session:** 2026-11-17 (hunt), **2026-11-18** (Unturned BE install + UM REA), 12:14 (Sam path empty), **12:25–12:30** (offline Reversing copy + host/guest REA), **afternoon** (skip auto-analyze: objdump + `.pdata` + packed-name blob; Ghidra `-noanalysis`). Guest share **`Z:\`**. Guest kernel copy: `C:\re\artifacts\BEDaisy.sys`. Live Program Files unused.

---

## Verified kernel facts

Static only. Drivers **not** loaded. `BEService` left **Stopped**. Required fields: device names, IOCTL dispatch, IAT, entry.

| Field | This lab — offline `BEDaisy.sys` `E7BCF344…` |
|-------|----------------------------------------------|
| **Kernel sample** | **Present** as a regular PE. Source: `C:\Users\bj284\Downloads\TraeTesting\cursor\Reversing\BEDaisy.sys` (mtime 12:23, 40 060 656 bytes). Staged `ac-captures-host\2026-11-18\BEDaisy.sys` and guest `C:\re\artifacts\BEDaisy.sys` (hash match). Live path `C:\Program Files (x86)\Common Files\BattlEye\BEDaisy.sys` was **not** used. |
| **IAT** | Single import `ntoskrnl.exe!RtlPcToFileHeader` (hint 0). No delay-load. Imphash MD5 `1e68d6eb7a47969d04bab0ee75c0eafa`. Host REA `inspect_pe_imports` `ev_ed538fa0…`; guest CLI `ev_f8a61d74…`. Packed `.rdata` name blob at file `0x25bf0` concatenates Filter Manager + `Nt*`/`Zw*` names **without NULs or IAT slots** (dynamic-resolve layout). Also ASCII `ntoskrnl.exe` at `0x270c8`, `sha512` / `LibTomMath`. |
| **Entry** | PE32+ amd64, image base `0x140000000`, subsystem native (1), `DllCharacteristics=0x1E0`. `AddressOfEntryPoint` RVA `0x25ccea8` → VA **`0x1425ccea8`** in unnamed section 6 (~39 763 968 raw bytes @ VA `0x31000`). REA `inspect_with_objdump` `ev_598c61ac…`: `pei-x86-64`, start `0x00000001425ccea8`. `1425ccea8: jmp 0x14261cead` then **cyclic** `E9 F6 FF FF FF` thunks (`14261cead ⇄ 14261cea8`). Adjacent unused `1425ccead: jmp 0x1400066bf`. INIT `0x14002f000: jmp 0x1400157c8` (obfuscated `.text`: opaque-predicate `call`/`jmp r8/r11/rcx/rdi` landing in SECTION.6 at `0x1417c4ba1`). `0x14002f02c` is `__security_init_cookie` (cookie VA `0x14002a138`, `rdtsc`). `.pdata` **4241** RUNTIME_FUNCTION rows (`.text` **553**, INIT **5**, SECTION.6 **3683**). PDB `BEDaisy.pdb` (RSDS GUID `50b7c339-248f-4066-9488-e4637a1a7de1`, age 20). **Do not auto-analyze:** `CreateThunkFunctionCmd` SOE. Guest Ghidra 12.1 **`-import -overwrite -noanalysis` + Java `DumpLimitedDisasm.java` succeeded** (project `C:\re\ghidra-projects\BEDaisyNoA`, program `/BEDaisy.sys`, save OK, EXIT 0). Jython `.py` post-scripts are stubbed on this Ghidra. AddressSet-restricted disasm of PE entry / INIT / `.text` **did not** recurse the cyclic thunks. Ghidra blocks: `.text` `0x140001000–0x1400257ff`; second `.text` `0x14002e000`; INIT `0x14002f000`; unnamed **SECTION.6** `0x140031000–0x14261cfff` (exec); SECTION.7 `0x14261d000` **non-exec** 4 KiB; `.pdata` `0x14262a000`. Limited dump: `ac-captures-host/2026-11-18/bedaisy-limited-disasm.txt`. |
| **Device names** | ASCII `BEDaisy` only as `BEDaisy.pdb`. UTF-16 `\Device\BattlEye` / `\DosDevices\BattlEye` / `\\.\BattlEye` hits = **0**. ASCII `\Device\` / `\DosDevices\` = **0**. UTF-16 `BattlEye` hit is publisher string `BattlEye Innovations e.K` in the cert overlay. **H-BE-01 not confirmed** on this hash. |
| **IOCTL / dispatch** | No plaintext `MajorFunction` / `CTL_CODE` / `FLT_REGISTRATION` header (`68 00 02 02` / `70 00 03 02` **absent**). Little-endian scan of the experienceds immediates: **11/12 absent**; `0x0022830F` at file `0x1e670` is **`0F 83` `JAE rel32`**, not `mov imm32` — **false positive**. Disk IOCTL immediates `0x2D1400` / `0x7C088` appear only inside the unnamed blob (coincidental). INIT/`.text` path is an obfuscated trampoline into SECTION.6, not a recovered IRP dispatch table. H-BE-02/03 remain Hypothesis for *computed* codes. |

Sidecar: `ac-captures-host/2026-11-18/hashes.json`. Guest copy hash-matched at `C:\re\artifacts\BEDaisy.sys`. Authenticode **Valid** (WHQL leaf Microsoft Hardware Compatibility Publisher; nested PKCS#7 names **BattlEye Innovations e.K.**, Reutlingen). Defender: `MpCmdRun -ScanType 3` **skipped** the `.sys` path; identical bytes scanned as `.bin` **found no threats**; `Start-MpScan -ScanType CustomScan` completed. **Driver not loaded.**

---

## Sample hunt

| Location | Result |
|----------|--------|
| `C:\Users\bj284\Downloads\TraeTesting\cursor\Reversing\BEDaisy.sys` | **Offline copy used** (40 060 656 bytes, SHA-256 `E7BCF344EEFA7A4EBEA9B048E376958A5AD711489249033FA480A7EC8B6C018E`) |
| GitHub `filename:BEDaisy.sys` | **0** PE hits (2026-11-17) |
| Host `C:\Windows\System32\drivers`, DriverStore, CatRoot, inf | **no** `BEDaisy.sys` after 2026-11-18 install |
| Guest `C:\Windows\System32\drivers`, Program Files | **no** installed `BEDaisy.sys` (lab copy only at `C:\re\artifacts\`) |
| `C:\Program Files (x86)\Common Files\BattlEye\BEDaisy.sys` (Sam-stated host path, 12:14) | **Not present.** Folder is UM `BEService*.exe` only. **Not copied from here.** |
| `C:\Program Files (x86)\Common Files\BattlEye\` | `BEService.exe` + `BEService_unturned.exe` (**same SHA-256**, 2026-11-18 11:29) plus older `BEService_{eft,fn,wt}.exe` |
| `C:\Program Files (x86)\Steam\steamapps\common\Unturned\BattlEye\` | `BEClient_x64.dll`, `BEServer_x64.dll`, `BEService_x64.exe`, `Unturned_BE.exe` (installer stub), no `.sys` |
| `HKLM\SYSTEM\CurrentControlSet\Services\BEService` | Type 16 / Start 3 / ImagePath Common Files `BEService.exe` / **Stopped** — **no** `BEDaisy` service key |
| Rainbow Six Siege Steam tree | replay crumbs only — no `BattlEye\` kernel drop |

**Conclusion (Verified hunt):** Unturned installer registered the **usermode service** only. Kernel bytes for this pass came from Sam’s **offline Reversing folder copy**, hashed/Defender-scanned, staged to `ac-captures-host\2026-11-18\`, then guest Ghidra. Live BattlEye under Program Files was not read or started.

---

## Sample identity (this lab)

| Artifact | Bytes | SHA-256 | Source | Guest copy |
|----------|------:|---------|--------|------------|
| `BEDaisy.sys` | 40 060 656 | `E7BCF344EEFA7A4EBEA9B048E376958A5AD711489249033FA480A7EC8B6C018E` | `Reversing\BEDaisy.sys` (offline) | `C:\re\artifacts\BEDaisy.sys` (hash match) |

PE timestamp **2026-10-12T13:11:53Z**. 10 sections; unnamed section 6 is the ~39.8 MB execute blob. No export directory (host `ev_00fed766…`; guest `ev_ee071044…`). No resource directory (`ev_0cec969a…`). TLS none. Load-config size 280; security cookie VA `0x14002a138`; GuardFlags `256` (`IMAGE_GUARD_CF_INSTRUMENTED` **linker claim only**); Guard CF function table VA `0` (`ev_6e0f9b9f…`).

**Packed `.rdata` API blob (file `0x25bf0`, concatenated, no NULs) — Verified names present as substrings:** `FltGetRequestorProcess`, `FltReleaseFileNameInformation`, `FltRegisterFilter`, `FltUnregisterFilter`, `NtReadVirtualMemory`, `FltGetFileNameInformation`, `FltStartFiltering`, `FltQueryInformationFile`, `ZwSuspendThread`, `NtWriteVirtualMemory`, `ZwGetContextThread`, `ZwProtectVirtualMemory`, `NtDeviceIoControlFile`. Plus NUL-terminated `RtlPcToFileHeader`, `ntoskrnl.exe`, `sha512`, `LibTomMath`, `BEDaisy.pdb`. **Not IAT.** Supports H-BE-04 (minifilter + memory APIs resolved at runtime). `ObRegisterCallbacks` / `PsSetLoadImageNotifyRoutine` strings were **not** in `.rdata`.

---

## Verified (usermode BattlEye + kernel hunt, this lab)

Windows Defender `MpCmdRun -ScanType 3` on UM `ac-captures-host\2026-11-18\`: **no threats** (exit 0). Authenticode **Valid**, signer **BattlEye Innovations e.K.** (Reutlingen, DE). Sidecar: `ac-captures-host/2026-11-18/hashes.json`. Guest copies (hash-matched): `C:\re\artifacts\{BEService.exe,BEService_x64.exe,BEClient_x64.dll,BEServer_x64.dll,BEDaisy.sys}`.

| File | Bytes | SHA-256 | Notes |
|------|------:|---------|-------|
| `BEDaisy.sys` | 40 060 656 | `E7BCF344EEFA7A4EBEA9B048E376958A5AD711489249033FA480A7EC8B6C018E` | Offline Reversing copy; WHQL + nested BattlEye publisher |
| `BEService.exe` | 29 005 032 | `DD3D72DF4E9866C2E02D64460ED86AE170334133A78C98A8574B2BD94BA1DCD0` | Common Files, 2026-11-18 install |
| `BEService_unturned.exe` | 29 005 032 | **same as `BEService.exe`** | duplicate copy |
| `BEService_x64.exe` | 18 663 720 | `1D325B52B55FE35997A5669ED609D1C8877B7FB1EDF599AA736A0FD86704395A` | Unturned `BattlEye\` drop |
| `BEClient_x64.dll` | 6 111 496 | `1241EE26CF859988150186536ED05F37462889EB30D72C895B5402C16719C5EB` | game client |
| `BEServer_x64.dll` | 382 984 | `F22EF1FB4A3C5E2E40C74F960EB21DC1C489D818E28683CFAB96CE4BA020A52B` | server; **unpacked-looking** IAT |
| `Unturned_BE.exe` | 1 970 408 | `D960107CC071D0BD361F5244BE51620FD623C8F45C1926CDE84F075FB7B8C4D7` | installer stub (`Install_BattlEye.bat` → `..\Unturned_BE.exe 1 0`) |
| `BEService_eft.exe` | 9 712 432 | `5E21159D8DF5C537202BD35A4A60ABEDA5D5029E6A9FA9111862065A759E4C20` | older (2023) |
| `BEService_fn.exe` | 15 044 872 | `258E18C264EBE68BAFF92EB928878A902B99A5FB4454881CCDB50EF4C7136E6C` | older (2023) |
| `BEService_wt.exe` | 15 762 728 | `0D46298C87248794569BB3AAD62255C899D0128EA0B1218484FC8DB43911B55B` | older (2025) |

**`BEService.exe` packer (kernel-payload carrier, not a loadable `.sys`):** PE32+ amd64, 14 sections, entry RVA `0x42b12dd` in `.packer1`, **no** `IMAGE_DIRECTORY_ENTRY_IMPORT`. `.packer1` raw `0x1ba6600` @ entropy **8.000**. Other sections have `PointerToRawData=0`. Resource directory RVA is **not file-backed** (REA `inspect_pe_resources` invalid_format). ASCII/UTF-16 `BEDaisy` / `\Device\BattlEye` hits = **0**. Nested MZ+PE count = **0**. Host REA `inspect_pe_imports` `ev_009691a292d2282b…` (empty IAT); `inspect_native_exports` `ev_adb0f36ad807a3c2…` (no export directory). **This packed image is a different artifact from the offline `BEDaisy.sys` PE.**

**`BEService_x64.exe` IAT** (guest `ev_02b913f0…` / host `ev_43c9bf7b…`): `WINTRUST.dll!CryptCATAdminAcquireContext`, `CRYPT32.dll!CryptQueryObject`, `WS2_32.dll` ordinal 9, `KERNEL32.dll!Thread32First`, `ADVAPI32.dll!DeleteService`. Imphash MD5 `0bfd891e6c49b3ef80af8a3e1b358e8d`. Custom `.be0/.be1/.be2` sections; `.be2` entropy 7.966.

**`BEClient_x64.dll` IAT** (host `ev_155b67258fb14ce9…` / guest `ev_43f171f0…`): `GetProcAddress` / `LoadLibraryA` present — DeviceIoControl may be dynamic. Imphash `ef803836ebaf57a5a1aaf069d46c11dc`.

**`BEServer_x64.dll` IAT** (`ev_6f50b75f…`): large `KERNEL32` + `WS2_32` ordinal set; `IsDebuggerPresent`; imphash `844cb05e4cc0a42f8c80d78974befe40`.

**`Unturned_BE.exe` UTF-16:** `BattlEye\BEService.exe`, `BattlEye Service`, `\\BattlEye\\BEService_` — installer paths only, no `BEDaisy.sys`.

Older `BEService_eft.exe` IAT (`ev_4ed1d576…`) still holds: 8 descriptors / imphash `5295c1c524b901c52966528564329268`.

---

## Hypothesis (kernel — public notes vs this hash)

| ID | Hypothesis | Confidence on `E7BCF344…` | Falsify / status |
|----|------------|---------------------------|------------------|
| H-BE-01 | Device `\Device\BattlEye` / `\DosDevices\BattlEye` / `\\.\BattlEye` | **Low on this hash** — no plaintext | Ghidra `IoCreateDevice` / encrypted strings. Static strings = **0**. |
| H-BE-02 | IOCTL surface uses `FILE_DEVICE_UNKNOWN (0x22)` + `METHOD_NEITHER (3)` | Medium (public) | Dispatch decompile after following entry trampolines |
| H-BE-03 | Historical IOCTL immediates (experienceds table) | **Rejected as literals** on this hash | 11/12 LE immediates absent; `0x0022830F` is `JAE` opcode, not an IOCTL dword |
| H-BE-04 | Minifilter registration (`FltRegisterFilter`) and/or image-load notify | Medium-high for Flt* (name blob); **low** for image-notify (no `PsSetLoadImage*` in `.rdata`) | IAT is only `RtlPcToFileHeader`; Flt* + `NtRead/WriteVirtualMemory` live in packed names at `0x25bf0`. No plaintext `FLT_REGISTRATION` header. |
| H-BE-05 | Disk identity IOCTLs to `\Device\Harddisk0\DR0` (`0x2D1400`, `0x7C088`) | Low — blob-only hits | Need Ghidra xrefs, not raw `find` |

### Public IOCTL table (Hypothesis — experienceds/battleye-re)

Copied as a **checklist**. Literal immediates were **not** found in `E7BCF344…` (except the `0x0022830F` / `JAE` collision):

| IOCTL | Func# | Access | Public category | Literal in `E7BCF344…` |
|-------|-------|--------|-----------------|------------------------|
| `0x0022001F` | 7 | ANY | handshake / init | **no** |
| `0x0022022B` | 138 | ANY | ping / heartbeat | **no** |
| `0x0022035B` | 214 | ANY | query state | **no** |
| `0x0022408F` | 35 | READ | read process info | **no** |
| `0x0022409B` | 38 | READ | read memory range | **no** |
| `0x00224387` | 225 | READ | read module list | **no** |
| `0x0022438F` | 227 | READ | read callback status | **no** |
| `0x00228183` | 96 | WRITE | write signal | **no** |
| `0x0022826B` | 154 | WRITE | write scan target | **no** |
| `0x0022830F` | 195 | WRITE | write memory (2 sites claimed) | **collision only** (`0F 83` JAE at `0x1e670`) |
| `0x002283F7` | 253 | WRITE | write config | **no** |
| `0x0022C20F` | 131 | R+W | bidirectional exchange | **no** |

## IOCTLs / devices (this lab)

| Device / symlink | IOCTL (hex) | Direction | Status | Notes |
|------------------|------------|-----------|--------|-------|
| *(no plaintext `\Device\BattlEye`)* | experienceds table | — | **Not in literals** | Dispatch after VM/jmp trampoline **not recovered** from PE tables. Follow guest `function` at `0x1425ccea8`. |

## Components

| Layer | Artifact | Status here |
|-------|----------|-------------|
| Kernel | `BEDaisy.sys` | **Staged** from offline Reversing copy; Defender-clean; guest hash-matched; **not loaded** |
| Service | `BEService.exe` / `_unturned` / `_x64` | Present, packed, signed, Defender-clean, staged |
| Client | `BEClient_x64.dll` | Staged |
| Server | `BEServer_x64.dll` | Staged |

## Not cloned (blocklist / dual-use)

| Repo | Why skipped |
|------|-------------|
| `gmh5225/bedaisy-bypass` | Bypass implementation |
| `Deox-XD/BEDaisy-Bypass` | Bypass implementation |
| `Aki2k/BEDaisy` | Historical RE **plus** IAT-hook reconstruction — cited as hypothesis only, not cloned as a sample |
| `itsunderscores/EAC-Rollback` | EAC (not BE) rollback PoC — not a clean analysis sample |

Research clones (docs/JSON, **no** `.sys`): `research-clones\battleye-re`.

## Next steps

1. **Done:** hash + Defender + stage offline copy; guest IAT; `.pdata`; packed-name inventory; INIT/`.text` objdump. Live Program Files BattlEye remains untouched; `BEService` remains Stopped.
2. **Done (workaround):** Ghidra headless **cannot** auto-analyze this hash (`CreateThunkFunctionCmd` SOE on cyclic `E9 F6 FF FF FF`). Import with **`-noanalysis`**. Ghidra 12.1 on the guest has **no Jython** — post-scripts must be **Java** (`DumpLimitedDisasm.java`) or PyGhidra. Host MinGW `objdump --start-address/--stop-address` is the reliable limited disasm path. Do **not** load the driver.
3. Follow obfuscated `.text` at `0x1400157c8` (`jmp 0x1417c4ba1` in SECTION.6 via `jmp rdi`) if device/IOCTL recovery is still needed — still a VM/opaque-predicate stub.
4. UM-only follow-up: `GetProcAddress` in `BEClient_x64.dll` — still no live attach.

## Dead ends

| Date | Item | Reason |
|------|------|--------|
| 2026-11-17 | Kernel `BEDaisy.sys` on disk / GitHub | Not on host, guest, or public GitHub as a PE |
| 2026-11-17 | R6 Siege folder | Incomplete / replay-only; no BattlEye payload |
| 2026-11-18 | Unturned BE install | Service registered **Stopped**; no `.sys` dropped; `.packer1` has no nested PE |
| 2026-11-18 12:14 | Sam path `C:\Program Files (x86)\Common Files\BattlEye\BEDaisy.sys` | File not found. Superseded by offline Reversing copy. |
| 2026-11-18 12:30 | experienceds IOCTL immediates | Not present as LE dwords in `E7BCF344…` |
| 2026-11-18 12:41 | Guest Ghidra `analyze` / `function` | Cyclic PE-entry thunks (`E9 F6 FF FF FF`) → `CreateThunkFunctionCmd` SOE; bridge never starts |
| 2026-11-18 afternoon | Guest Ghidra `DumpLimitedDisasm.py` | Ghidra 12.1 Jython stub: “install Jython or port to Java/PyGhidra”. `-noanalysis` **import itself succeeded**. Java `DumpLimitedDisasm.java` is the follow-up. |
