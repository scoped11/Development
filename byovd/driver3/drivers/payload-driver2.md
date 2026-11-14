# Driver2.sys — mapped payload (kernel R/W + AC IOCTL lab)

**Method:** Pool **payload** mapped via BYOVD on **Host B**. Implements full Betest dispatch table; symlink to usermode **kept by default** for dev comm (`--driver2-hide-symlink` optional).

**Route:**

```text
mapper_nal.exe ... --driver2 --ac-replica <profile> ...
  → Driver2.sys in kernel pool
  → \\.\MsFlt0..9 / WdFilter* scan (BetestOpenDevice)
  → IOCTL 0x870 session auth
  → 0x805/0x806 process R/W
```

| Artifact | Path |
|----------|------|
| Source | `driver2/src/Driver2Build.c` |
| Binary | `driver2/bin/Driver2.sys`, `bin/Driver2.sys` |
| Runner | `Driver2Runner.exe --profile <p> --mode prove\|ioctl\|bypass` |
| Client | `Driver2Client.exe probe \| pread \| pwrite \| kread` |
| SDK | `driver2/sdk/driver2_api.h`, `include/betest_ioctl.h` |

**Read / write:**

1. Map (VM): `driver3\host-b.ps1 -Profile <game> -Execute -ForceMap`
2. Open: `Driver2Open()` / `BetestOpenDevice()` — retry 24× built in
3. Auth: `IOCTL_SESSION_AUTH` **`0x870`**
4. Process read: `IOCTL_READ_PROCESS_MEM` **`0x805`** — `Driver2Client pread <pid> <va> <len>`
5. Process write: **`0x806`**
6. Kernel read: **`0x802`** — `Driver2Client kread ...`

Full table: `driver2/IOCTL_REFERENCE.md`, `docs/01_read_write.md`.

**Tiers:**

| ID | Bar |
|----|-----|
| **D0** | Built |
| **D1** | `Driver2Client probe` after bypass |
| **D2** | `verify-pf-diag.ps1` — strip + title IOCTL |
| **D3 / D3i** | `developer-approval-gate.ps1` / `cheat-inspector.ps1 -Execute` |
| **D4** | Live ranked map — **not claimed** (use DMA Host A) |
