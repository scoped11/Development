# RE notes — Easy Anti-Cheat (EAC)

**Binary focus:** owned `EasyAntiCheat_EOS.sys` (current EOS) and `EasyAntiCheat.sys` (legacy).  
**Evidence rule:** **Verified** = REA PE tools, Authenticode, Defender/VirusTotal, or guest `rea inspect-*` / PE header parse. **Hypothesis** = public GitHub notes not re-proven on this SHA-256.  
**Scope:** static documentation only. No live bypass, hypervisor patches, or cheat payloads. Driver images were **not** loaded.

**Session:** 2026-11-17. Guest `AI_workspace` via `VBoxManage guestcontrol` (share **`Z:\`**, not `D:`). Guest copies: `C:\re\artifacts\`.

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
| Legacy PE entry `entry` at `0x1400afde8` is a **23-byte trampoline** (not real DriverEntry). Pseudocode: `(*(code *)&LAB_14071c246)(param_1);` — target `0x14071c246` sits in unnamed `SECTION.6`. Incoming DATA xref from PE header `0x1400000a8`. | Guest REA `function` `ev_6d24798e…` on `C:\re\artifacts\EasyAntiCheat.sys` | legacy `DD60A8CD…` |
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
| H-EAC-01 | Remaining kernel APIs (`ObRegisterCallbacks`, process/image notify, etc.) are **dynamically resolved**, which is why the EOS IAT is only `FltRegisterFilter`. | High (layout) | Ghidra xrefs from unnamed section / import decrypt; **not** done on this hash yet |
| H-EAC-02 | IOCTL / device object names for **this** EOS build are not in plaintext (unlike legacy `\Device\EasyAntiCheat`). UM string `EasyAntiCheat_EOSSys` may be a service/device stem. | Medium | Ghidra `IoCreateDevice` / `IoCreateSymbolicLink` on this SHA |
| H-EAC-03 | Integrity verifier tables described by `experienceds/eac-eos-research` (hash `7102C734…`, 22 MB) **do not apply** to `26797A11…` without a new pass. | High | Compare section map + RVA `0xDB338` trampoline — expected miss |
| H-EAC-04 | `adamvalves` `eac_internals.h` (TPM / stack-walk / import descriptor structs) is a useful **naming hint**, not a sizeof-accurate layout for this build. | Low | REA `procedure_pseudo_code` on DriverEntry path |
| H-EAC-05 | Public note that EOS `ObRegisterCallbacks` returns `OB_PREOP_SUCCESS` while AND-masking handle access is **policy inference from another build**. Do not treat as verified here. | Low | Callback body on **this** image |

Public IOCTL sketches from other hashes stay **out** of the Verified table.

## IOCTLs / devices

| Device / symlink | IOCTL (hex) | Direction | Status | Notes |
|------------------|------------|-----------|--------|-------|
| `\Device\EasyAntiCheat` + `\DosDevices\EasyAntiCheat` | *(none recovered)* | UM↔KM | **Verified device strings on legacy only** | Legacy `DD60A8CD…` Unicode. No CTL_CODE immediates extracted this session. |
| EOS kernel | — | — | **Unverified on `26797A11…`** | No `\Device\…` Unicode hit in EOS `.sys`. UM `EasyAntiCheat_EOSSys` is a **hypothesis** stem. |
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
| P1 | EOS entry RVA `0x272978` | `decompile` / `function` — trampoline vs real DriverEntry |
| P1 | `FltRegisterFilter` IAT thunk `0x14030fdd8` | xrefs — minifilter callbacks |
| P2 | `IoCreateDevice` / device name construction | strings may be decoded; start from UM `EasyAntiCheat_EOSSys` |
| P2 | Unnamed VA `0x381000` blob | entropy / decrypt-at-init **documentation only** |
| P3 | Re-check adamvalves / experienceds RVAs | expected mismatch vs 22 MB public hash |

**Out of scope:** live attach, callback unhooking, rollback-to-old-driver PoCs (`itsunderscores/EAC-Rollback` not cloned).

## Dead ends

| Date | Item | Reason |
|------|------|--------|
| 2026-11-17 | Public GitHub `.sys` bodies | `filename:EasyAntiCheat_EOS.sys` / `EasyAntiCheat.sys` code search: no PE; one header dump `EBalloon/EasyAntiCheat-SRC` skipped (not a signed driver) |
| 2026-11-17 | REA `inspect_pe_certificates` on EOS | PKCS#7 ASN.1 nesting > 32; Authenticode still Valid via Windows |
| 2026-11-17 | REA `inspect_binary_hardening` | MCP output schema rejected Windows paths (`path must match ^/`) |
| 2026-11-17 | Manifest hash `A38FA3D4…` | Driver on disk has been updated; old hash is historical only |
