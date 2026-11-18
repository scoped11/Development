# VM driver inventory — EAC / BattlEye / lab PE

**Scanned:** 2026-11-17 (driver hunt + REA) / 2026-11-18 (Unturned UM + offline `BEDaisy.sys`)  
**Host share:** `C:\Users\bj284\Downloads\TraeTesting\cursor\Development`  
**Guest share:** **`Z:\`** (confirmed 2026-11-17 guestcontrol). `D:\` is **not** the share. UNC `\\vboxsvr\hostdev`  
**Live VM (VBoxManage):** `AI_workspace` · 4 vCPU · **8192 MiB** RAM · `VMState=running` · Guest Additions **7.2.10** · `GuestAdditionsRunLevel=3`  
**Guest control:** **working** as `vboxuser` (`hostname` = `AIworkspace`)

Sam owns production anti-cheat binaries on the **guest**. This table records what is on the **share today** plus canonical guest-only paths when the `.sys` never left the VM disk.

## Path convention

| Side | Root |
|------|------|
| Host | `C:\Users\bj284\Downloads\TraeTesting\cursor\Development` |
| Guest (share) | `Z:\` (not `D:`) |
| Guest (local Ghidra I/O) | `C:\re\artifacts\` (create as needed) |
| Guest (optional staging) | `D:\ac\samples\` (mirror of `ac-captures-host\<date>\` when copying off `C:`) |

## Anti-cheat kernel targets (owned builds)

| Component | Guessed role | On host share? | Size | SHA256 | Host path | Guest share path | Ready for Ghidra |
|-----------|--------------|----------------|------|--------|-----------|------------------|------------------|
| `EasyAntiCheat_EOS.sys` | EAC EOS kernel driver | **Yes** (2026-11-17) | 40 474 272 | `26797A1122781CAF12F1A686E33A0883FA1E2895B40C2249932E319B338841C7` | `ac-captures-host\2026-11-17\` + `C:\Program Files (x86)\EasyAntiCheat_EOS\` | `Z:\ac-captures-host\2026-11-17\EasyAntiCheat_EOS.sys` | **Yes** — guest `C:\re\artifacts\` hash-matched. Epic-signed, Defender-clean, VT 0. |
| `EasyAntiCheat.sys` | EAC **legacy** kernel driver | **Yes** (2026-11-17) | 16 635 000 | `DD60A8CD0771B4FC548FDCEA68EA0FC78FBAAC58A77099731A62E70B03EB1C2A` | `ac-captures-host\2026-11-17\` + `C:\Program Files (x86)\EasyAntiCheat\` | `Z:\ac-captures-host\2026-11-17\EasyAntiCheat.sys` | **Yes** — guest copy hash-matched. EasyAntiCheat Oy signed, Defender-clean, VT 0. |
| `BEDaisy.sys` | BattlEye kernel driver | **Yes** (offline Reversing copy, 2026-11-18 12:25) | 40 060 656 | `E7BCF344EEFA7A4EBEA9B048E376958A5AD711489249033FA480A7EC8B6C018E` | `ac-captures-host\2026-11-18\` from `TraeTesting\cursor\Reversing\BEDaisy.sys` | `Z:\ac-captures-host\2026-11-18\BEDaisy.sys` | **Yes** — guest `C:\re\artifacts\` hash-matched. WHQL + nested BattlEye publisher. **Not loaded.** Live Program Files path unused. |

**Historical (not on disk now):** Rust dump 2026-11-13 documented EOS `A38FA3D4158FBD4503DC4D4BA303493B25B9DCFEEDB81D4386F2BB71888CBE7C` (42 884 760 bytes). Current EOS is a **different** hash/size.

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

**2026-11-17:** `EasyAntiCheat_EOS.sys` and `EasyAntiCheat.sys` are now under `ac-captures-host\2026-11-17\` (bodies gitignored; hashes in `hashes.json`). Host also has signed UM `BEService_eft/fn/wt.exe` under `C:\Program Files (x86)\Common Files\BattlEye\` — see [re-notes-be.md](re-notes-be.md).

**2026-11-18:** Unturned BattlEye install staged UM under `ac-captures-host\2026-11-18\` (gitignored bodies; `hashes.json` committed): `BEService.exe` / `BEService_unturned.exe` SHA-256 `DD3D72DF…`, `BEService_x64.exe` `1D325B52…`, `BEClient_x64.dll` `1241EE26…`, `BEServer_x64.dll` `F22EF1FB…`, `Unturned_BE.exe` `D960107C…`. Later the same day: offline `BEDaisy.sys` `E7BCF344…` from `TraeTesting\cursor\Reversing\` staged into the same folder and guest `C:\re\artifacts\` (hash-matched). Defender-clean; **not loaded**. Live Program Files BattlEye unused; `BEService` left **Stopped**.

## Tools bootstrap (guest)

| Script | Purpose |
|--------|---------|
| [driver-lab/vm/bootstrap-vm-tools.ps1](../driver-lab/vm/bootstrap-vm-tools.ps1) | Entry point: JDK + Ghidra + REA MCP |
| [driver-lab/vm/install-ghidra-rea-guest.ps1](../driver-lab/vm/install-ghidra-rea-guest.ps1) | Implementation (called by bootstrap) |
| [docs/mcp-rea-guest.example.json](mcp-rea-guest.example.json) | MCP `rea` env template |
| [docs/rea-doctor-guest-expected.md](rea-doctor-guest-expected.md) | Expected `rea doctor` output on guest |

## Blockers

| Item | Status (2026-11-17 evening) |
|------|---------------------|
| `hostdev` shared folder | **Transient mapping** → guest **`Z:\`** |
| Guest Additions | **7.2.10 r174163**, runlevel 3 |
| `VBoxManage guestcontrol` | **OK** (`vboxuser` / `lab.credentials.json`) |
| VM power | **Running** (GUI/Qt session) |
| EAC `.sys` on share | **Staged** EOS + legacy — see table above |
| `BEDaisy.sys` | **Still missing** after Unturned BE install (host + guest); see [re-notes-be.md](re-notes-be.md) Verified kernel facts |

**Remediation (Sam, in order):**

1. VirtualBox Manager → **`AI_workspace`** → close window **or** Power off → run host:  
   `powershell -File C:\Users\bj284\Downloads\TraeTesting\cursor\Development\driver-lab\vm\wire-ai-workspace-lab.ps1`  
   (wires `hostdev` → `D:\hostdev`, updates `host-lab-environment.json`).
2. Start VM, **sign in** interactively as `vboxuser` (or update `lab.credentials.json`).
3. Guest: **Devices → Insert Guest Additions CD** → install → **reboot** → confirm `VBoxManage guestproperty get AI_workspace /VirtualBox/GuestAdd/Version` returns a version string on host.
4. Guest elevated PS: `D:\hostdev\driver-lab\vm\bootstrap-vm-tools.ps1` then doctor line in [bootstrap-vm-tools.ps1](../driver-lab/vm/bootstrap-vm-tools.ps1).
5. Host: `.\host-guest-exec.ps1 -Command hostname` (should print guest name).

## Related

- [vm-checklist.md](vm-checklist.md)
- [re-notes-eac.md](re-notes-eac.md) · [re-notes-be.md](re-notes-be.md)
- `driver-lab\scripts\dump-rust-eac.ps1` · `dump-title-anticheat.ps1`