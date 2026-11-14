# Post-load dump

Use this when `crack_pe_unpack` returns `likely_encrypted_or_packed` or `likely_packer_named`.

`guest-export-stripped-driver.ps1` copies the file Windows stored (the inbox path or a DriverStore copy). If `DriverEntry` decrypts code in memory, that copy is still the packed file. Open the memory image in IDA after the stub has run.

Kernel debug is not attached until UART is enabled (the VM must be powered off for that one-time change). After that, `kd_dump.py` dumps from the host. Do this on DriverLab-E-26200 only, and only for a driver you already qualified there.

1. Load the driver in the lab VM (`crack_qualify_vm` or `sc start` on that guest).
2. From the kernel debugger, break when that module loads (`sxe ld:<name>.sys`).
3. Break again after the unpack stub finishes: on the return from the entry point, or on `nt!IoCreateDevice` / `nt!IoCreateSymbolicLink` once you have confirmed the break is inside this driver.
4. Dump then, before `MiFreeInitializationCode` discards `INIT`. `.writemem` the module range from its base through `SizeOfImage` (`lmvm <name>`).
5. The dump is a mapped image. In PE-bear set each section's raw pointer to its virtual address and its raw size to its virtual size so IDA can parse it.
6. **Host kd (preferred):** power the VM off once, run `driver-lab\vm\host-enable-kd-uart.ps1`, then elevated guest `guest-enable-kd-serial.ps1`, reboot the guest. After that:
   `python driver-lab\reversal\kd_dump.py dump <disk.sys> --wait-load`
   That runs `lm`, `.writemem` into `audit/dumps/raw`, writes a host-drop sidecar, and registers the dump.
7. **Manual guest copy:** `guest-register-memory-dump.ps1` if you already have a dump file.
8. **Host:** `python driver-lab\reversal\reversal_bridge.py auto <disk.sys>` — scores the dump, fixups, writes `ida_ticket.json`.
9. Alternative host-only: drop JSON in `audit/dumps/host-drop/` then `crack_dump_sync`.
10. `idb_open` on `ida_input_path` from the ticket.

This does not devirtualize VMProtect. It only saves the image after the packer restored it. It does not bypass anti-cheat.
