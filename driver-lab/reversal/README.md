# Host reversal workbench

The main PC owns the heavy path: full PE fixup, disk-vs-dump compare, staged copies for IDA, and MCP tickets that tell Cursor exactly what to open next.

The lab VM only loads drivers and (optionally) hands you a memory dump. It does not run IDA.

## Integrated pipeline (what a serious T&E lab actually wires)

| Layer | Role |
|-------|------|
| **Artifact store** | SHA256-named trees under `audit/crack/<sha16>/` — `card.json`, `ida_ticket.json`, manifests |
| **Static triage** | `crack_profile`, `crack_pe_unpack` on host |
| **Acquisition** | VM `qualify` or kernel debugger `.writemem` when packed |
| **Transform** | `pe_fixup` maps raw section layout → virtual (IDA-readable) |
| **Verify** | `pe_compare` entropy drop proves runtime decrypt, not DriverStore copy |
| **Analysis** | `user-ida-pro-mcp` on staged path from ticket |

Russia/China/NSA-style shops do not use one monolithic “decrypt button.” They chain **cataloged binaries → triage score → isolated execution → dumped image → analyst IDA session** with every hop producing a JSON receipt. That is what `reversal_route` encodes.

Kernel access for decryption means **breaking after the stub in the debugger and dumping the module**, not guessing keys on disk. The host can run WinDbg against the lab VM (or kdnet) while IDA stays local.

## CLI

```powershell
python driver-lab\reversal\reversal_bridge.py route driver-lab\audit\inbox\oem-sys\x64\SIVX64.sys
python driver-lab\reversal\reversal_bridge.py route SIVX64.sys --qualify --preset siv
python driver-lab\reversal\reversal_bridge.py compare disk.sys dump.bin
python driver-lab\reversal\reversal_bridge.py fixup dump.bin
python driver-lab\reversal\reversal_bridge.py route disk.sys --dump dump.bin
```

## IDA (Cursor)

After `route`, open the ticket:

```text
audit/crack/<sha16>/ida_ticket.json → ida_input_path
```

MCP:

```text
user-ida-pro-mcp → idb_open input_path=<ida_input_path> mode=prefer_headless
```

Then follow `ida_sequence` in the ticket (`survey_binary`, `search_text`, decompile IOCTL path).

## Dump registry (closed loop)

1. Guest or host registers dump → `audit/dumps/inbox/<sha16>/`  
2. Host validates (PE gates, score, rejects disk-identical copies)  
3. `reversal auto` picks best dump, fixups, updates `ida_ticket.json`

```powershell
python driver-lab\reversal\reversal_bridge.py auto packed.sys --preset siv
python driver-lab\reversal\kd_dump.py dump packed.sys --wait-load
python driver-lab\reversal\dump_registry.py scan-all
```

`kd_dump.py` uses WinDbg `kd.exe` on `\\.\pipe\DriverLabKD`. The pipe is off while the VM is running; power off once and run `vm\host-enable-kd-uart.ps1`, then the guest `guest-enable-kd-serial.ps1`, then reboot. A dry run writes the WinDbg scripts and a receipt without attaching.

## MCP

`crack_reversal_auto`, `crack_reversal_route`, `crack_dump_sync`, `crack_dump_register`, `crack_dump_pick`, `crack_pe_fixup`, `crack_pe_compare`.

Does not bypass anti-cheat.