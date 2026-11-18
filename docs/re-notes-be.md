# RE notes — BattlEye (BE)

**Binary focus:** `BEDaisy.sys` (kernel) was **not found** on host or guest. Usermode `BEService_*.exe` from Sam’s install were hashed/scanned.  
**Evidence rule:** **Verified** only for files in this lab. Public BEDaisy IOCTL tables stay **Hypothesis**.  
**Scope:** static documentation only. No live bypass, no callback interception, no cheat payloads.

**Session:** 2026-11-17.

---

## Sample hunt

| Location | Result |
|----------|--------|
| GitHub `filename:BEDaisy.sys` | **0** PE hits |
| Host `C:\Windows\System32\drivers`, Program Files, Steam libraries (`C:` and `E:`) | **no** `BEDaisy.sys` |
| Guest `C:\Windows\System32\drivers`, Program Files, `C:\re` | **no** `BEDaisy.sys` |
| `C:\Program Files (x86)\Common Files\BattlEye\` | three **signed** `BEService_*.exe` (UM) |
| Rainbow Six Siege Steam tree | replay crumbs only — no `BattlEye\` kernel drop |
| Destiny 2 / Hunt | Hunt is **EAC EOS**, not BE |

**Conclusion:** this lab cannot Verified-fill a BEDaisy IOCTL table until Sam copies `BEDaisy.sys` from a title that actually installed the kernel (launch R6 / Ark / etc., then dump).

---

## Verified (usermode BattlEye, this host)

Windows Defender `MpCmdRun -ScanType 3` on each file: **no threats**. Authenticode **Valid**, signer **BattlEye Innovations e.K.** (Reutlingen, DE).

| File | Bytes | SHA-256 |
|------|------:|---------|
| `BEService_eft.exe` | 9 712 432 | `5E21159D8DF5C537202BD35A4A60ABEDA5D5029E6A9FA9111862065A759E4C20` |
| `BEService_fn.exe` | 15 044 872 | `258E18C264EBE68BAFF92EB928878A902B99A5FB4454881CCDB50EF4C7136E6C` |
| `BEService_wt.exe` | 15 762 728 | `0D46298C87248794569BB3AAD62255C899D0128EA0B1218484FC8DB43911B55B` |

`BEService_eft.exe` (REA `inspect_pe_imports` `ev_4ed1d576…`): PE32+ amd64. Standard IAT DLLs include `WINTRUST.dll!CryptCATAdminAcquireContext`, `CRYPT32.dll!CryptQueryObject`, `WS2_32.dll` (ordinal 9), `ADVAPI32.dll!OpenServiceW`, `USER32.dll!CharUpperBuffW`, and `KERNEL32.dll` `{Thread32First, GetVersion, LocalAlloc, LocalFree, GetModuleFileNameW, ExitProcess, LoadLibraryA, GetModuleHandleA, GetProcAddress}`. Imphash MD5 `5295c1c524b901c52966528564329268`. Coverage complete for the **declared** import table (14 named/ordinal rows across 8 descriptors). This does **not** prove DeviceIoControl usage — those APIs may be resolved via `GetProcAddress`.

These UM binaries were **not** copied into `C:\re\artifacts\` (kernel-first staging). Hashes recorded here for provenance.

---

## Hypothesis (kernel — public notes, **other builds**)

Do **not** promote to Verified without a `BEDaisy.sys` SHA-256 from this machine.

| ID | Hypothesis | Confidence | Falsify with |
|----|------------|------------|--------------|
| H-BE-01 | Device `\Device\BattlEye` / `\DosDevices\BattlEye` / `\\.\BattlEye` | Medium (public) | `strings` + `IoCreateDevice` on **owned** `.sys` |
| H-BE-02 | IOCTL surface uses `FILE_DEVICE_UNKNOWN (0x22)` + `METHOD_NEITHER (3)` | Medium (experienceds/battleye-re `docs/03-ioctl-dispatch.md`) | Dispatch decompile on owned hash |
| H-BE-03 | Historical IOCTL immediates (experienceds table): `0x0022001F`, `0x0022022B`, `0x0022035B`, `0x0022408F`, `0x0022409B`, `0x00224387`, `0x0022438F`, `0x00228183`, `0x0022826B`, `0x0022830F`, `0x002283F7`, `0x0022C20F` | **Low for current BE** — aged / other image | Search little-endian immediates in owned `.sys` |
| H-BE-04 | Minifilter registration (`FltRegisterFilter`) and/or image-load notify | Medium | IAT + xrefs on owned `.sys` |
| H-BE-05 | Disk identity IOCTLs to `\Device\Harddisk0\DR0` (`0x2D1400`, `0x7C088`) from Aki2k traces | Low (2019–2020 GoodEye log) | Static xrefs only if sample exists |

### Public IOCTL table (Hypothesis — experienceds/battleye-re)

Copied as a **checklist**, not as this-lab fact:

| IOCTL | Func# | Access | Public category |
|-------|-------|--------|-----------------|
| `0x0022001F` | 7 | ANY | handshake / init |
| `0x0022022B` | 138 | ANY | ping / heartbeat |
| `0x0022035B` | 214 | ANY | query state |
| `0x0022408F` | 35 | READ | read process info |
| `0x0022409B` | 38 | READ | read memory range |
| `0x00224387` | 225 | READ | read module list |
| `0x0022438F` | 227 | READ | read callback status |
| `0x00228183` | 96 | WRITE | write signal |
| `0x0022826B` | 154 | WRITE | write scan target |
| `0x0022830F` | 195 | WRITE | write memory (2 sites claimed) |
| `0x002283F7` | 253 | WRITE | write config |
| `0x0022C20F` | 131 | R+W | bidirectional exchange |

## IOCTLs / devices (this lab)

| Device / symlink | IOCTL (hex) | Direction | Status | Notes |
|------------------|------------|-----------|--------|-------|
| *(no kernel sample)* | — | — | **Blocked** | Need owned `BEDaisy.sys` |

## Components

| Layer | Artifact | Status here |
|-------|----------|-------------|
| Kernel | `BEDaisy.sys` | **Missing** |
| Service | `BEService_eft/fn/wt.exe` | Present, signed, Defender-clean |
| Client | `BEClient.dll` | Not staged |

## Not cloned (blocklist / dual-use)

| Repo | Why skipped |
|------|-------------|
| `gmh5225/bedaisy-bypass` | Bypass implementation |
| `Deox-XD/BEDaisy-Bypass` | Bypass implementation |
| `Aki2k/BEDaisy` | Historical RE **plus** IAT-hook reconstruction — cited as hypothesis only, not cloned as a sample |
| `itsunderscores/EAC-Rollback` | EAC (not BE) rollback PoC — not a clean analysis sample |

Research clones (docs/JSON, **no** `.sys`): `research-clones\battleye-re`.

## Next steps

1. After a BE title actually installs the kernel: copy `BEDaisy.sys` → `ac-captures-host\<date>\` and `C:\re\artifacts\`.
2. Defender + Authenticode + SHA-256; stop if not BattlEye-signed / not Defender-clean.
3. Guest: `npx.cmd rea-agents@latest inspect-pe-imports C:\re\artifacts\BEDaisy.sys` then `analyze` (Ghidra).
4. Re-prove or **reject** the experienceds IOCTL immediates on that hash.
5. UM-only follow-up (optional): `DeviceIoControl` / `NtDeviceIoControlFile` via `GetProcAddress` in `BEService_*.exe` — still no live attach.

## Dead ends

| Date | Item | Reason |
|------|------|--------|
| 2026-11-17 | Kernel `BEDaisy.sys` | Not on host, guest, or public GitHub as a PE |
| 2026-11-17 | R6 Siege folder | Incomplete / replay-only; no BattlEye payload |
