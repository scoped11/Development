# EAC cookbook (lab)

**Audience:** Kyle / Rob Run workspace — **isolated lab**, manifests, Rust + EasyAntiCheat (EOS) research.

**This is not** a live ranked bypass playbook. It is a **recipe index**: pick a **lane**, run a **phase**, record **evidence**. Success = reproducible reads or a graded artifact table — not “no ban.”

---

## 0. Pick your method first (catalog)

| ID | Method | Where it runs | Needs kernel on game PC? | EAC/Rust live fit |
|----|--------|----------------|---------------------------|-------------------|
| **C** | Il2Cpp / dumps / IDA MCP | Any host | No | **Best** for structs/offsets |
| **D** | [Lane D split-trust](12_lane_d_split_trust.md) | Game PC + no-AC VM | **No** on game PC | **Default** when BYOVD blocked |
| **DMA** | Leech/VMM overlay (`../dma/`) | Game PC (2nd machine) | No | Live reads if IOMMU allows |
| **A** | BYOVD + RwTest (`--ac-safe`) | No-AC VM only | Only on lab VM | Kernel proof, not Rust session |
| **A+** | BYOVD + Betest IOCTLs | No-AC VM | Lab only | **Hypothesis tests** (one flag/snapshot) |
| **B** | Lab I forensics | Snapshot VM | After map | Measure traces |
| **H** | Hygiene-only map | No-AC VM | Lab | PiDDB/CI/WdFilter scrub (see `vehicle_hygiene.hpp`) |
| — | Usermode RPM/debug | Unprotected targets | No | Not Rust protected process |
| — | Private signed driver | N/A (not in repo) | If loaded with EAC up | High scrutiny; separate project |
| — | Hypervisor / VBS guest | N/A (not in repo) | Different stack | Research-only |
| ✗ | Hide LOL driver on game PC | — | — | **No recipe** — use **D** or **C** |

**Lane D summary (Rust default when blocked):**

> Lane D is **not** kernel hide on the Rust machine. When EAC blocks BYOVD, **do not map on that session**. Split **DMA live** (game PC) + **BYOVD lab** (no-AC VM); sync **`build_id` + offsets**.

### 0b. Other methods (extended catalog)

These exist in research / tooling. **In-tree** means we already have a recipe. **Out** = consider later, no how-to here. **Skip** = malware-class or live-cheat ops we will not add.

| ID | Method | In tree? | What it buys | EAC / Rust note |
|----|--------|----------|--------------|-----------------|
| **C2** | DummyDll / script.json / `dump.cs` only | Yes (`dumps/`) | Types without live memory | Always start here |
| **C3** | IDA + `dma/ida-offset-mcp` | Yes | Exact RVAs vs guesswork | Offline |
| **C4** | Replay / demo / packet capture | Out | Layout from recordings | Protocol ≠ memory; server still sees |
| **C5** | Crash dump / hang dump of game | Out | One-shot memory image | Debugger/dump often flagged if taken **live** under EAC |
| **DMA2** | FPGA / PCILeech (same VMM API) | Partial (`dma/` Leech) | Faster/more stable DMA | Same IOMMU class as Lane D |
| **DMA3** | VM host reads guest RAM (VMware/Hyper-V/QEMU) | Out | “DMA” without FPGA | Game-in-VM + EAC often refuses; lab-friendly |
| **DMA4** | Capture card / second PC display | Out | Visual ESP only | No process memory |
| **A2** | kdmapper / KDU comparison | Ref only (`reference/kdmapper/`) | Trace parity | Mix **after** R2, never instead of R2 |
| **A3** | Signed OEM driver not on public LOL list | Out | Different vehicle | Same **class** once listed; not a hide |
| **A4** | WHQL driver you ship (EV cert) | Out | Legal load path | EAC still scans **behavior** + image |
| **H2** | Shared memory / pipe / socket UM↔KM | Partial (IOCTL wrap) | Quieter than new `\Device\Cheat` | RwTest wrap is this idea on vehicle |
| **H3** | Module stomp / PE in signed `.text` | **`STOMP_BYPASS.md`** (lab) | Hide pool PE | Residual disk vs memory; **not default** |
| **DBG** | WinDbg / KDNET / livekd | Out | Lab I gold | EAC anti-debug; use **after** snapshot or no EAC |
| **ETW** | ETW / WPR traces of EAC | Out | How EAC behaves | Defensive / measurement |
| **NET** | Steam/EOS traffic (TLS intercept) | Out | Matchmaking / report path | Not R/W; legal/ToS |
| **HV** | Type-1 hypervisor introspection | Out | Read guest without guest driver | Big project; EAC VBS/HVCI aware |
| **FW** | SMM / UEFI / firmware | **Skip** | Below OS | Out of scope; not lab cookbook |
| **EXP** | Windows 0-day / PPL / CI exploit | **Skip** | Unsigned kernel without BYOVD | Exploits — not this kit |
| **UM** | Inject / overlay in `RustClient.exe` | **Skip** for live | Easy on paper | EAC usermode + kernel watch this |

**Practical leftovers that are still worth doing in *this* workspace:**

1. **C2 + C3** until offsets are fixture-verified (you already dump).
2. **DMA2/3** if FPGA/VM host is easier than BYOVD (Lane D Host A variants).
3. **C5** on a **no-EAC** build or post-crash dump for structs.
4. **DBG + B** on snapshot for Lab I (not while ranked Rust runs).
5. **H2** is already RwTest IOCTL wrap — don’t invent a second device.

Everything else is either **duplicate of D/A** or **Skip**.

---

## 1. Gate recipes (run before cooking)

### Recipe G1 — “Is this host map-safe?”

```powershell
cd byovd
.\scripts\ac-preflight.ps1 -Driver rtcore
# 0 = device ok, 1 = fix vehicle, 2 = Tier 0 (AC + no device)
```

```text
bin\mapper_nal.exe --ac-preflight --driver rtcore
```

Log: `%TEMP%\pf_diag*.tmp`

### Recipe G2 — “Rust + EAC?”

```powershell
.\scripts\lane-d-preflight.ps1 -Game rust
```

| Exit | Meaning | Next recipe |
|------|---------|-------------|
| 2 | Lane D | **R1** + **R4** |
| 1 | Vehicle not loaded | Copy `vehicles\RTCore64.sys` → `bin\`, retry |
| 0 | BYOVD open, no AC procs | **R2** on this host *or* still **R1** if you will launch Rust later |

### Recipe G3 — Manifest (every machine, every week)

Copy `docs/lab_manifest.json`. Fill:

- `os_build`, `hvci`, `vulnerable_driver_blocklist`
- `vehicle`, `payload_sha256`, `mode` (`ac-safe` / `rw-only`)
- `lane_d.build_id` when doing Rust
- `ac_preflight_exit`

---

## 2. Core recipes

### R1 — Lane D (Rust / EAC blocks BYOVD on game PC)

**Host A — game PC**

1. `dma\scripts\dump-and-sync.ps1` (or existing dump) → update offsets.
2. Set `build_id` in manifest + `dma\config.json`.
3. `dma\scripts\build-windows.ps1` → run overlay on **Rust session** (understand IOMMU / Kernel DMA Protection).
4. **Do not** run `map-secure-rwtest.ps1` here while EAC is up.

**Host B — lab VM (snapshot, no EAC)**

1. `byovd\build.ps1`
2. `powershell -File map-secure-rwtest.ps1`
3. `bin\process_rw.exe --emit-manifest` → paste JSON into `lane_d.kernel_proof`
4. Optional Phase 2: WinDbg before/after (`06_better_research_method.md`)

**Sync check:** one field at known offset — DMA on A vs dump on C; kernel proof on B only validates IOCTL path.

### R2 — Stable kernel R/W only (no EAC on box)

```powershell
.\build.ps1
powershell -File map-secure-rwtest.ps1
bin\process_rw.exe
```

Expect: `read 0x1122334455667788`.

Rules:

- `--ac-safe` (no `--full`, no `--auto`, no `--eac-be`)
- Do **not** `sc stop` vehicle after success — reboot to clean.

### R3 — Full Betest map (lab experiments, not default)

```text
copy vehicles\RTCore64.sys bin\
bin\mapper_nal.exe bin\Betest.sys --driver rtcore --no-auth --rw-only
```

Only after **R2** passes on **two cold boots**.

### R4 — Static layout only (no kernel fight)

```powershell
cd ..
dma\scripts\dump-and-sync.ps1
dma\scripts\validate_offsets_against_dump.py
```

Docs: `dma\docs\IDA_MCP.md`, `dumps\<date>\`.

No BYOVD required.

### R5 — Lab I diff (forensics)

On **no-AC** snapshot:

1. Baseline: note vehicle in module list, pool, log lines.
2. **R2** once.
3. Post-map same checks → save `docs/lab_runs/<date>-post.txt`.
4. Grade rows per `03_research.md` / `research/16_*.md`.

Optional second snapshot: single mapper flag from **§4** — never stack.

### R7 — AC replica (Rust or R6 test harness)

```powershell
.\scripts\map-ac-replica-lab.ps1 -Profile rust -Mode prove
.\scripts\map-ac-replica-lab.ps1 -Profile r6  -Mode prove
.\scripts\map-ac-replica-lab.ps1 -Profile rust -Mode ioctl   # Betest, snapshot only
.\scripts\map-ac-replica-lab.ps1 -Profile rust -Mode bypass  # live-test harness (Betest + 0x869)
```

See **`15_test_ac_replicas.md`**. Real AC already running: **TH2** (`16_timed_ac_harness_method.md`). `ac_replica_profiles.json`.

### R6 — Vehicle fallback chain

If `OpenRtCore` fails (no AC):

Mapper tries: `rtcore` → `corsair` → `ts` → `siv` → `trixx` → `nal` (unless `--no-fallback`).

Place `.sys` in `vehicles\` or `bin\`.

---

## 3. EAC-specific notes (what the cookbook assumes)

| Topic | Lab assumption |
|-------|----------------|
| **EOS / Rust** | `EasyAntiCheat_EOS` + kernel module; usermode service + per-title config |
| **Vehicle deny** | Known LOL names/hashes; runtime module scans |
| **Mapped code** | Heuristics on non-module executable pages, stacks, dispatch (see `04_detection_surfaces.md`) |
| **Hygiene** | Reduces **some** load-history traces; does **not** equal EAC bypass |
| **HVCI trend** | Tightens BYOVD at OS layer; record in manifest |

Deep model: `research/16_EAC_BE_KERNEL_RESEARCH.md`, bible `research/10_ANTICHEAT_AND_SECURITY.md`.

---

## 4. Betest / mapper “experiments” (Tier 3 — one per snapshot)

**Never** first map of the day. Never on game PC with Rust running.

| Hypothesis | Mapper (lab VM) | Risk |
|------------|-----------------|------|
| EAC improve bitmask | `--eac 01` (single bit) | High |
| Callback strip | Betest `0x807` via doc 05 | High |
| NMI neut | `--nmi` → `0x868` | High |
| EAC/BE max **test** | `--eac-be` → `0x869` | Very high |
| BattlEye parallel | `--battleye 1f` | High |
| DKOM unlink vehicle | `--dkom-unlink` | **PG** |

Full flag list: **`05_existing_bypass_catalog.md`**. IOCTL `#define`s: `src/driver/BetestDriver.c` (~52–130).

**Stop if:** bugcheck, PatchGuard `0x109`, or you intended to use a protected multiplayer client.

---

## 5. Hide / block confusion (quick lookup)

| You want | Use | Don’t use |
|--------|-----|-----------|
| AC blocked RTCore on Rust PC | **R1** Lane D | PiDDB scrub on same PC |
| Fewer load traces, vehicle works, no AC | **R2** `--ac-safe` | `--hide-all` until Lab I |
| Undetected Rust | — | No recipe in this repo |
| Live offsets | **R4** + **DMA** | Kernel map on game PC |
| Prove IOCTL works | **R2** on VM | `process_rw` on Rust with EAC |

Tiers: `11_ac_block_vs_hide_plan.md`.

---

## 6. Command cheat sheet

| Goal | Command |
|------|---------|
| Build kit | `byovd\build.ps1` |
| Secure RwTest map | `map-secure-rwtest.ps1` |
| AC-safe map | `mapper_nal.exe … --ac-safe` |
| Kernel proof JSON | `process_rw.exe --emit-manifest` |
| AC + vehicle gate | `scripts\ac-preflight.ps1` |
| Rust Lane D gate | `scripts\lane-d-preflight.ps1 -Game rust` |
| DMA build | `dma\scripts\build-windows.ps1` |
| Dump + offsets | `dma\scripts\dump-and-sync.ps1` |

---

## 7. Other methods (outside this kit — when to consider)

| Method | Use when | Cookbook stance |
|--------|----------|-----------------|
| **Kernel DMA Protection audit** | DMA fails or zero reads | Fix hardware/IOMMU first |
| **Offline attach / dump analysis** | No live need | **R4** / **C5** |
| **Test signing / custom .sys** | Deliberate CI lab | Not default; watermark/policy |
| **Community mapper (kdmapper)** | Compare traces only | `reference/kdmapper/` — don't mix with first stable map |
| **Game private server / no EAC** | Validate overlay UI | Lane C + optional **R2** |
| **VM host RAM read (DMA3)** | Game in VM you own | Lab; EAC often blocks nested/HVCI |
| **FPGA PCILeech (DMA2)** | Hardware DMA instead of BYOVD | Same Lane D Host A |
| **ETW of EAC.sys** | Learn callbacks without mapping | Measurement only |
| Full table | See **§0b** | |

---

## 8. Weekly menu (solo)

| Day | Recipe |
|-----|--------|
| Mon | **G1** + **G3** manifest |
| Tue | **R2** or **R1** Host B proof |
| Wed | **R4** offsets |
| Thu | **R5** Lab I row |
| Fri | One **§4** experiment *or* DMA validation **R1** sync |

---

## 9. Doc map

| Doc | Role |
|-----|------|
| **14 (this)** | Recipes + method catalog |
| [15](15_test_ac_replicas.md) | **Rust / R6 test replicas** — prove vs ioctl |
| `ac_replica_profiles.json` | Machine-readable rust/r6 lab flags |
| [06](06_better_research_method.md) | Phases 0–5 |
| [11](11_ac_block_vs_hide_plan.md) | Block vs hide tiers |
| [12](12_lane_d_split_trust.md) | Lane D detail |
| [13](13_greenfield_eac_rust.md) | Greenfield Rust |
| [05](05_existing_bypass_catalog.md) | IOCTL / mapper inventory |
| [04](04_detection_surfaces.md) | What stays visible |
| [01](01_read_write.md) | IOCTL contract |

---

## 10. Stop rules (always)

1. No `--full` on first successful map.
2. No map on Tier 0 host (preflight exit 2) unless `--force-map` in **lab** only.
3. No vehicle `sc stop` after successful map.
4. No protected live title as the **only** test harness.
5. One experimental IOCTL family per VM snapshot.

When in doubt: **R4** (static) + **R1** (Lane D) beats another hide flag.
