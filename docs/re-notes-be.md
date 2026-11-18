# RE notes — BattlEye (BE)

**Binary focus:** Sam installed BattlEye via Unturned on 2026-11-18. **`BEDaisy.sys` is still not on disk** (host or guest) after the service installer; kernel bytes stay packed inside `BEService.exe`. Usermode BE binaries were staged, hashed, and Defender-scanned.  
**Evidence rule:** **Verified** only for files in this lab. Public BEDaisy IOCTL tables stay **Hypothesis**.  
**Scope:** static documentation only. No live bypass, no callback interception, no cheat payloads, **drivers not loaded**.

**Session:** 2026-11-17 (hunt) and **2026-11-18** (Unturned BE install + REA PE). Guest share **`Z:\`**. Guest copies of UM: `C:\re\artifacts\`.

---

## Verified kernel facts

Static only. Drivers **not** loaded. `BEService` left **Stopped**. Required fields: device names, IOCTL dispatch, IAT, entry.

| Field | This lab (2026-11-18 Unturned install) |
|-------|----------------------------------------|
| **Kernel sample** | **`BEDaisy.sys` is not on disk** (host `System32\drivers` / DriverStore / Common Files / Unturned `BattlEye\`; guest same). **No** `HKLM\…\Services\BEDaisy`. UM service `BEService` exists (Type 16 / Start 3 / ImagePath Common Files `BEService.exe`) and is **Stopped**. |
| **IAT** | Kernel IAT **N/A** (no `.sys`). Packed `BEService.exe` `DD3D72DF…`: **no** `IMAGE_DIRECTORY_ENTRY_IMPORT`, imphash null, **no** export directory (host REA `inspect_pe_imports` `ev_009691a2…`, `inspect_native_exports` `ev_adb0f36a…`). `BEService_x64.exe` IAT: `WINTRUST!CryptCATAdminAcquireContext`, `CRYPT32!CryptQueryObject`, `WS2_32` ordinal 9, `KERNEL32!Thread32First`, `ADVAPI32!DeleteService` (guest `ev_02b913f0…`). |
| **Entry** | Kernel DriverEntry **N/A**. `BEService.exe` PE32+ entry RVA `0x42b12dd` in `.packer1` (raw `0x1ba6600`, entropy **8.000**). Nested MZ+PE count = **0**. |
| **Device names** | ASCII/UTF-16 `BEDaisy` / `\Device\BattlEye` hits = **0** in packed `BEService.exe`. `Unturned_BE.exe` only has installer strings (`BattlEye\BEService.exe`, `BattlEye Service`). Public `\Device\BattlEye` stays **H-BE-01**. |
| **IOCTL / dispatch** | Kernel IOCTL table **Blocked** — no loadable `BEDaisy.sys`. Packed `.packer1` is not a PE, so there is no dispatch routine to decompile without unpacking (unpacking would require running the service — **out of static scope**). H-BE-02/03 remain Hypothesis. |

Sidecar: `ac-captures-host/2026-11-18/hashes.json`. Guest UM copies hash-matched at `C:\re\artifacts\`. Defender `MpCmdRun -ScanType 3` **no threats**. Authenticode **Valid**, BattlEye Innovations e.K.

---

## Sample hunt

| Location | Result |
|----------|--------|
| GitHub `filename:BEDaisy.sys` | **0** PE hits (2026-11-17) |
| Host `C:\Windows\System32\drivers`, DriverStore, CatRoot, inf | **no** `BEDaisy.sys` after 2026-11-18 install |
| Guest `C:\Windows\System32\drivers`, Program Files, `C:\re` | **no** `BEDaisy.sys` |
| `C:\Program Files (x86)\Common Files\BattlEye\` | `BEService.exe` + `BEService_unturned.exe` (**same SHA-256**, 2026-11-18 11:29) plus older `BEService_{eft,fn,wt}.exe` |
| `C:\Program Files (x86)\Steam\steamapps\common\Unturned\BattlEye\` | `BEClient_x64.dll`, `BEServer_x64.dll`, `BEService_x64.exe`, `Unturned_BE.exe` (installer stub), no `.sys` |
| `HKLM\SYSTEM\CurrentControlSet\Services\BEService` | Type 16 / Start 3 / ImagePath Common Files `BEService.exe` / **Stopped** — **no** `BEDaisy` service key |
| Rainbow Six Siege Steam tree | replay crumbs only — no `BattlEye\` kernel drop |

**Conclusion (Verified hunt):** installer registered the **usermode service** and copied packed `BEService.exe`. It did **not** drop `BEDaisy.sys` onto `System32\drivers`. Static 1-byte XOR / nested-MZ scan of `.packer1` found **no** embedded PE. Kernel IOCTL / device / IAT / DriverEntry stay **blocked** until a dropped `BEDaisy.sys` exists (service start would load the driver — **out of static scope**).

---

## Verified (usermode BattlEye + kernel hunt, this lab)

Windows Defender `MpCmdRun -ScanType 3` on `ac-captures-host\2026-11-18\`: **no threats** (exit 0). Authenticode **Valid**, signer **BattlEye Innovations e.K.** (Reutlingen, DE). Sidecar: `ac-captures-host/2026-11-18/hashes.json`. Guest copies (hash-matched): `C:\re\artifacts\{BEService.exe,BEService_x64.exe,BEClient_x64.dll,BEServer_x64.dll}`.

| File | Bytes | SHA-256 | Notes |
|------|------:|---------|-------|
| `BEService.exe` | 29 005 032 | `DD3D72DF4E9866C2E02D64460ED86AE170334133A78C98A8574B2BD94BA1DCD0` | Common Files, 2026-11-18 install |
| `BEService_unturned.exe` | 29 005 032 | **same as `BEService.exe`** | duplicate copy |
| `BEService_x64.exe` | 18 663 720 | `1D325B52B55FE35997A5669ED609D1C8877B7FB1EDF599AA736A0FD86704395A` | Unturned `BattlEye\` drop |
| `BEClient_x64.dll` | 6 111 496 | `1241EE26CF859988150186536ED05F37462889EB30D72C895B5402C16719C5EB` | game client |
| `BEServer_x64.dll` | 382 984 | `F22EF1FB4A3C5E2E40C74F960EB21DC1C489D818E28683CFAB96CE4BA020A52B` | server; **unpacked-looking** IAT |
| `Unturned_BE.exe` | 1 970 408 | `D960107CC071D0BD361F5244BE51620FD623C8F45C1926CDE84F075FB7B8C4D7` | installer stub (`Install_BattlEye.bat` → `..\Unturned_BE.exe 1 0`) |
| `BEService_eft.exe` | 9 712 432 | `5E21159D8DF5C537202BD35A4A60ABEDA5D5029E6A9FA9111862065A759E4C20` | older (2023) |
| `BEService_fn.exe` | 15 044 872 | `258E18C264EBE68BAFF92EB928878A902B99A5FB4454881CCDB50EF4C7136E6C` | older (2023) |
| `BEService_wt.exe` | 15 762 728 | `0D46298C87248794569BB3AAD62255C899D0128EA0B1218484FC8DB43911B55B` | older (2025) |

**`BEService.exe` packer (kernel-payload carrier, not a loadable `.sys`):** PE32+ amd64, 14 sections, entry RVA `0x42b12dd` in `.packer1`, **no** `IMAGE_DIRECTORY_ENTRY_IMPORT`. `.packer1` raw `0x1ba6600` @ entropy **8.000**. Other sections have `PointerToRawData=0`. Resource directory RVA is **not file-backed** (REA `inspect_pe_resources` invalid_format). ASCII/UTF-16 `BEDaisy` / `\Device\BattlEye` hits = **0**. Nested MZ+PE count = **0**. Host REA `inspect_pe_imports` `ev_009691a292d2282b…` (empty IAT); `inspect_native_exports` `ev_adb0f36ad807a3c2…` (no export directory).

**`BEService_x64.exe` IAT** (guest `ev_02b913f0…` / host `ev_43c9bf7b…`): `WINTRUST.dll!CryptCATAdminAcquireContext`, `CRYPT32.dll!CryptQueryObject`, `WS2_32.dll` ordinal 9, `KERNEL32.dll!Thread32First`, `ADVAPI32.dll!DeleteService`. Imphash MD5 `0bfd891e6c49b3ef80af8a3e1b358e8d`. Custom `.be0/.be1/.be2` sections; `.be2` entropy 7.966.

**`BEClient_x64.dll` IAT** (host `ev_155b67258fb14ce9…` / guest `ev_43f171f0…`): `GetProcAddress` / `LoadLibraryA` present — DeviceIoControl may be dynamic. Imphash `ef803836ebaf57a5a1aaf069d46c11dc`.

**`BEServer_x64.dll` IAT** (`ev_6f50b75f…`): large `KERNEL32` + `WS2_32` ordinal set; `IsDebuggerPresent`; imphash `844cb05e4cc0a42f8c80d78974befe40`.

**`Unturned_BE.exe` UTF-16:** `BattlEye\BEService.exe`, `BattlEye Service`, `\\BattlEye\\BEService_` — installer paths only, no `BEDaisy.sys`.

Older `BEService_eft.exe` IAT (`ev_4ed1d576…`) still holds: 8 descriptors / imphash `5295c1c524b901c52966528564329268`.

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
| *(no kernel sample)* | — | — | **Blocked** | `BEDaisy.sys` not on disk after install. H-BE-01…03 remain Hypothesis. |

## Components

| Layer | Artifact | Status here |
|-------|----------|-------------|
| Kernel | `BEDaisy.sys` | **Missing** (packed inside `BEService.exe` `.packer1`; not extracted statically) |
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

1. **Static scope exhausted** for kernel until `BEDaisy.sys` is a regular file. Do **not** start `BEService` / launch Unturned to drop it unless Sam explicitly authorizes sample acquisition (that is runtime, not this pass).
2. When a `.sys` exists: Defender + Authenticode + SHA-256; stop if not BattlEye-signed / not Defender-clean.
3. Guest: `npx.cmd rea-agents@latest inspect-pe-imports C:\re\artifacts\BEDaisy.sys` then `function` on PE entry + `search-bytes` for `\Device\BattlEye`.
4. Re-prove or **reject** the experienceds IOCTL immediates on that hash.
5. UM-only follow-up: `GetProcAddress` in `BEClient_x64.dll` — still no live attach.

## Dead ends

| Date | Item | Reason |
|------|------|--------|
| 2026-11-17 | Kernel `BEDaisy.sys` | Not on host, guest, or public GitHub as a PE |
| 2026-11-17 | R6 Siege folder | Incomplete / replay-only; no BattlEye payload |
| 2026-11-18 | Unturned BE install | Service registered **Stopped**; no `BEDaisy.sys`; `.packer1` has no nested PE / no plaintext device strings |
