# Walkthrough example (what a full lab session looks like)

This is the **script** we follow. On a machine **without** a mapped driver, step 4 fails — that is expected (see “What we ran on the dev PC” below).

## 1. Manifest

```powershell
cd C:\Users\baile\OneDrive\Desktop\Development\byovd
Copy-Item docs\lab_manifest.template.json docs\lab_manifest.json
# Edit os_build, hvci, payload_sha256
Get-FileHash bin\Betest.sys -Algorithm SHA256
```

Example payload hash (your build may differ):

```
F82BCE23C0D462443203232D4BACBFAF14E00060845D89048C5ED2B55504E709
```

## 2. Build

```powershell
.\build.ps1
```

Output: `bin\mapper_nal.exe`, `bin\Betest.sys`.

## 3. Vehicle + map (admin VM recommended)

```powershell
Copy-Item vehicles\NalDrvIntel.sys bin\
# Run elevated:
.\run-map-stable.cmd
```

Expected log tail (`%TEMP%\pf_diag.tmp`):

```text
mapper_nal started
[*] RW-only: map + Betest R/W IOCTLs only ...
DriverEntry complete — Betest IRP handlers now active ...
[*] RW-only: skipped automatic preempt sweep ...
[*] BYOVD cleanup: mapped driver active — not stopping BYOVD ...
```

## 4. Prove R/W (usermode)

```powershell
# After fixing include/betest_ioctl.h and building once:
cl /EHsc /std:c++17 examples\process_rw.cpp /I include /Fe:bin\process_rw.exe
bin\process_rw.exe
```

**Success:**

```text
read 0x1122334455667788 (expect 0x1122334455667788)
```

**Before map (normal on dev PC):**

```text
Betest device not found. Map first: run-map-stable.cmd
```

## 5. Read a real process (overlay-style)

```cpp
HANDLE h = BetestOpenDevice();
BetestSessionAuth(h);
DWORD pid = 1234;  // notepad.exe, your own test proc
uint64_t addr = 0x...;  // from debugger
uint8_t buf[64];
BetestReadProcess(h, pid, addr, buf, sizeof(buf));
```

## 6. Optional research flags (isolated snapshot only)

```text
bin\mapper_nal.exe bin\Betest.sys --driver nal --eac 01
```

One bit at a time — see `05_existing_bypass_catalog.md`.

## 7. Lab I note (one line)

Create `docs/lab_i_notes.md`:

```markdown
## 2026-11-12 vm-1 rw-only
- Manifest: docs/lab_manifest.json
- PiDDB: heuristic / not measured
- Device: \\.\MsFlt3 (example)
- Pool exec: yes (ManualMap)
```

## What we ran on the dev PC (2026-11-12)

| Step | Result |
|---|---|
| `build.ps1` | Already had `bin\mapper_nal.exe` + `Betest.sys` |
| `bin\process_rw.exe` | Exit 1 — **no mapped driver** (expected) |
| `mapper_nal.exe` (no args) | Usage + `pf_diag.tmp` created |

Kernel map was **not** executed in the assistant session (needs admin + vehicle `.sys` + lab acceptance of BSOD risk).
