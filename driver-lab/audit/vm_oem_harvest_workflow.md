# VM → driver → IDA workflow (26200 lab)

## On the guest (Windows)

1. **Tools on Z:** `driver-lab/tools/7z-portable/` (7z.exe + 7z.dll) — copied to `C:\DriverLab\tools\` by harvest scripts.
2. **Download / use setup** from `Z:\driver-lab\audit\inbox\oem-raw\` or `Invoke-WebRequest` inside the guest.
3. **Extract (no GUI):**
   - `guest-vm-harvest-extract.ps1` — non-admin via `vm_exec` (download + 7z + copy `.sys` to share).
   - `guest-vm-harvest-setup.ps1` — full path + `pnputil` (requires **elevated** worker).
4. **Output for host:** `Z:\driver-lab\audit\inbox\oem-sys\harvested-vm\<name>_latest\*.sys`
5. **Load test (elevated):** `guest-vm-driver-matrix.ps1` or `host-bg-admin.ps1 -Action driver_matrix`

**Do not** run `PawnIO_setup.exe` silent via worker — it hangs. Use 7-Zip on the embedded `105~` zstd/cab payload instead.

## On the host

1. Read harvested `.sys` from the share path above (same files as on `Development\...` via OneDrive sync).
2. **IDA:** `idb_open` on the harvested binary; `survey_binary` + dispatch decompile for IOCTL / `MmMapIoSpace`.
3. Optional: `host-vm-driver-sweep.ps1` after worker is healthy (reboot VM if worker wedged).

## Elevation paths

| Method | When |
|--------|------|
| `vm_exec` | Extract-only, no kernel load |
| `host-bg-admin.ps1` | Queues `Driver3ElevatedWorker` (preferred for `sc start` / `pnputil`) |
| `host-guest-exec.ps1` | VBox Guest Control (often **not** admin — load tests fail) |
| `host-guest-run-elevated.ps1` | schtasks `/RL HIGHEST` (needs password / policy) |
| VM **reboot** | Unstick worker after hung installer |

## Proven extract (PawnIO 2.2.0)

```
vm_exec → guest-vm-harvest-extract.ps1
→ Z:\driver-lab\audit\inbox\oem-sys\harvested-vm\pawnio_latest\PawnIO.sys
```

Compare SHA to inbox `x64\PawnIO.sys` (`fca6e7d5…`) — cab flatten may yield smaller duplicate; use largest file from full cab listing when adding picker logic.
