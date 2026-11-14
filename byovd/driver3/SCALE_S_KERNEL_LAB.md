# Scale S — Kernel Lab (no FPGA)

**What you asked for:** S-tier **without a DMA card**.

**What this is:** Full **Driver2 kernel SDK** on the **VM** — read/write via `Driver2Client` / `Driver2Session`, your cheat EXE, hooks in **your** process on the lab machine.

**What this is NOT:**

- Live **Rust / R6 / Arena Breakout with AC on the game PC** with no hardware (that needs **FPGA `dma/`** or mapping on the game PC — D4, not claimed).
- Undetected live ranked.

## One PC (no second tower)

Same as today: **game PC + VirtualBox VM**. No FPGA.

```text
Host A — optional (game for offsets / dumps only; no ranked DMA path)
Host B — VM: map Driver2 → your cheat .exe uses IOCTL reads/writes
```

## Enable

```powershell
.\set-live-read-method.ps1 -Method kernel_lab_s
.\reach-scale-s-kernel-lab.ps1 -AllTitles
```

## Play / dev loop (no FPGA)

Inside **elevated VM**:

```powershell
cd Z:\byovd
.\driver2\Driver2Runner.exe --profile rust --mode bypass --force-map
.\bin\Driver2Client.exe pread <pid> <va> <n>
```

Your C++ cheat: link `driver2/sdk`, `Driver2Session::ReadProcess` — **external** to target or attach in VM with **no live AC**.

Hooks: in **your** EXE (overlay, detours on your code), or kernel plumbing via Driver2 — **not** “DMA internal” into Steam’s game on the AC host.

## S-tier bar (kernel_lab_s)

Same VM proof as Scale S (`vm_hostb`, `kernel_proof`, dynamic inspector), **no** `dma_ship_path` check — replaced by **driver2_sdk_ship**.

## Switch back to ranked + FPGA

```powershell
.\set-live-read-method.ps1 -Method dma
```
