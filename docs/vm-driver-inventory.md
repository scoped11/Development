# VM driver inventory — EAC / BattlEye / lab PE

**Scanned:** 2026-11-17 (host share `C:\Users\bj284\Downloads\TraeTesting\cursor\Development`)  
**Guest mirror:** `D:\hostdev\` (same relative paths) · UNC `\\vboxsvr\hostdev`  
**VM:** `AI-Workspace` (running; Guest Control blocked — see [Blockers](#blockers))

Sam owns production anti-cheat binaries on the **guest**. This table records what is on the **share today** plus canonical guest-only paths when the `.sys` never left the VM disk.

## Path convention

| Side | Root |
|------|------|
| Host | `C:\Users\bj284\Downloads\TraeTesting\cursor\Development` |
| Guest (share) | `D:\hostdev` |
| Guest (local Ghidra I/O) | `C:\re\artifacts\` (create as needed) |
| Guest (optional staging) | `D:\ac\samples\` (mirror of `ac-captures-host\<date>\` when copying off `C:`) |

## Anti-cheat kernel targets (owned builds)

| Component | Guessed role | On host share? | Size | SHA256 | Host path | Guest share path | Ready for Ghidra |
|-----------|--------------|----------------|------|--------|-----------|------------------|------------------|
| `EasyAntiCheat_EOS.sys` | EAC EOS kernel driver | **No** (manifest only) | 42 884 760 (captured 2026-11-13) | `A38FA3D4158FBD4503DC4D4BA303493B25B9DCFEEDB81D4386F2BB71888CBE7C` | — | `D:\hostdev\ac-captures-host\<date>\EasyAntiCheat_EOS.sys` (after copy) | **After guest copy** |
| `BEDaisy.sys` | BattlEye kernel driver | **No** | — | — | — | `D:\hostdev\ac-captures-host\<date>\BEDaisy.sys` (after copy) | **After guest copy** |

**Verified on share (metadata, not driver bodies):**

| Artifact | Role | Host path | Guest path |
|----------|------|-----------|------------|
| Rust EAC manifest | Documents EOS driver hash/path | `byovd\driver2\dumps\rust-eac-eos_2026-11-13_0204\manifest.json` | `D:\hostdev\byovd\driver2\dumps\rust-eac-eos_2026-11-13_0204\manifest.json` |
| R6 BattlEye manifest | Game bundle metadata (no kernel `.sys` in tree) | `byovd\driver2\dumps\r6-battleye_2026-11-13_0214\manifest.json` | Same under `D:\hostdev\...` |
| `ac_host_captures.json` | Profile → module names / hashes | `ac-captures-host\config\ac_host_captures.json` | `D:\hostdev\ac-captures-host\config\...` |

**Guest canonical sources (copy to share, do not commit unless policy allows):**

| Component | Typical guest source |
|-----------|---------------------|
| `EasyAntiCheat_EOS.sys` | `C:\Program Files (x86)\EasyAntiCheat_EOS\EasyAntiCheat_EOS.sys` |
| `BEDaisy.sys` | `C:\Windows\System32\drivers\BEDaisy.sys`, or `C:\Program Files (x86)\Common Files\BattlEye\BEDaisy.sys`, or game `BattlEye\` after install |

**Sam — stage on guest after copy:**

```powershell
New-Item -ItemType Directory -Force -Path 'D:\ac\samples','C:\re\artifacts' | Out-Null
Copy-Item 'C:\Program Files (x86)\EasyAntiCheat_EOS\EasyAntiCheat_EOS.sys' 'D:\hostdev\ac-captures-host\2026-11-17\'
Copy-Item 'D:\hostdev\ac-captures-host\2026-11-17\EasyAntiCheat_EOS.sys' 'C:\re\artifacts\'
Get-FileHash 'C:\re\artifacts\EasyAntiCheat_EOS.sys' -Algorithm SHA256
```

Repeat for `BEDaisy.sys` when present on the VM (R6/Ark: install BE kernel once, then dump per `driver-lab\scripts\dump-title-anticheat.ps1`).

## Lab / smoke-test kernel PE (on share — use for REA pipeline)

These are **owned lab drivers**, not EAC/BE. Safe default for [rea-vm-smoke-test.md](rea-vm-smoke-test.md).

| File | Component | Size | SHA256 | PE | Host path | Guest share path | Ready for Ghidra |
|------|-----------|------|--------|-----|-----------|------------------|------------------|
| `Betest.sys` | BYOVD lab test driver | 171 520 | `3BC356C9300ADC233D3FF264802856AA372FDB6515BF1103AA6E6DE70C0FD019` | x64 | `byovd\bin\Betest.sys` | `D:\hostdev\byovd\bin\Betest.sys` | **Yes** (after guest Ghidra install) |
| `BetestDriver.sys` | Same image as Betest (duplicate) | 171 520 | same | x64 | `byovd\bin\BetestDriver.sys` | `D:\hostdev\byovd\bin\BetestDriver.sys` | **Yes** |
| `RwTest.sys` | Minimal R/W lab driver | 6 656 | `796FD1082B547670A0894F13779E1C41C2853D9E37433D2A5C9A29D1ED333505` | x64 | `byovd\bin\RwTest.sys` | `D:\hostdev\byovd\bin\RwTest.sys` | **Yes** |
| `Driver2.sys` | Driver2 lab image | 171 008 | `D9A071895D4A86DA955A135BAD4C8AF6BE085D0909BD63DC0494C594F50609DD` | x64 | `byovd\driver2\bin\Driver2.sys` | `D:\hostdev\byovd\driver2\bin\Driver2.sys` | **Yes** |

Recommended Ghidra working copy (guest):

```text
C:\re\artifacts\Betest.sys   ← copy from D:\hostdev\byovd\bin\Betest.sys
```

## Usermode / bundle fragments (share)

| File | Component | Size | Host path | Guest path | Ready for Ghidra |
|------|-----------|------|-----------|------------|------------------|
| `BEClient_x64.cfg` | BE client config stub | 27 | `byovd\driver2\dumps\r6-battleye_2026-11-13_0214\battleye\game\` | `D:\hostdev\...` | No (not PE) |
| EAC `Settings.json`, certs, localization | EOS runtime bundle (Rust) | various | `byovd\driver2\dumps\rust-eac-eos_2026-11-13_0204\eac\EasyAntiCheat\` | `D:\hostdev\...` | Partial (certs only) |

No `BEService.exe` / `BEClient.dll` / `EasyAntiCheat_EOS.sys` **files** were found under the share trees searched (`ac-captures-host`, `byovd\driver2\dumps`, `driver-lab\dumps`, `driver-lab\audit\inbox\oem-sys`).

## Tools bootstrap (guest)

| Script | Purpose |
|--------|---------|
| [driver-lab/vm/bootstrap-vm-tools.ps1](../driver-lab/vm/bootstrap-vm-tools.ps1) | Entry point: JDK + Ghidra + REA MCP |
| [driver-lab/vm/install-ghidra-rea-guest.ps1](../driver-lab/vm/install-ghidra-rea-guest.ps1) | Implementation (called by bootstrap) |
| [docs/mcp-rea-guest.example.json](mcp-rea-guest.example.json) | MCP `rea` env template |
| [docs/rea-doctor-guest-expected.md](rea-doctor-guest-expected.md) | Expected `rea doctor` output on guest |

## Blockers

| Item | Status |
|------|--------|
| `VBoxManage guestcontrol` on host (2026-11-17) | **Failed** — `VBOX_E_GSTCTL_GUEST_ERROR` / Guest Additions not ready |
| VM power | **Running** (`VMState=running`, GUI session) |
| EAC/BE `.sys` on share | **Missing** — metadata + lab drivers only |

**Remediation:** Sign in to `AI-Workspace` interactively, confirm Guest Additions, re-run `driver-lab\vm\host-guest-exec.ps1` or run `bootstrap-vm-tools.ps1` **inside** the guest.

## Related

- [vm-checklist.md](vm-checklist.md)
- [re-notes-eac.md](re-notes-eac.md) · [re-notes-be.md](re-notes-be.md)
- `driver-lab\scripts\dump-rust-eac.ps1` · `dump-title-anticheat.ps1`