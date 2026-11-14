# BYOVD vehicles — load primitive (not game R/W)

**Method:** Signed **vulnerable** Windows drivers abused only to **map** `Driver2.sys` / `RwTest.sys` into kernel on **Host B**. Mapper: `bin/mapper_nal.exe`. Fallback order: **rtcore → corsair → nal → trixx → throttlestop → siv** (`--driver <name>`).

**Route:**

```text
disk: vehicles\RTCore64.sys  →  bin\
service / manual map (mapper_nal)
  → hygiene on vehicle DRIVER_OBJECT
  → Driver2.sys DriverEntry (pool payload)
  → vehicle device left running (default)
```

| Device (open test) | Path |
|--------------------|------|
| RTCore | `\\.\RTCore64` |
| Nal | `\\.\Nal` |
| Corsair | `\\.\CorsairLLAccess` |
| Trixx | `\\.\TrixxSvc` |
| ThrottleStop | `\\.\ThrottleStop` |
| SIV | `\\.\SIVDRIVER` |

Preflight: `scripts\ac-preflight.ps1 -Driver rtcore` (or matching `-Driver`).

**Read / write:**

| What | How |
|------|-----|
| Vehicle IOCTLs | **Lab only** — Intel/NAL RW primitives; **not** the cheat comm path after map |
| Game memory | **Do not** use vehicle IOCTLs — open **`Driver2.sys`** device after map ([payload-driver2.md](payload-driver2.md)) |

**Tiers:**

| ID | Situation |
|----|-----------|
| **T0** | AC up + `CreateFile(\\.\Vehicle)` fails — **no map** on game PC |
| **T1** | Vehicle opens, no AC — `map-secure-rwtest.ps1` / `--ac-safe` on VM |
| **T2** | Lab I manifest — `docs/lab_manifest.json` |
| **D Host B** | Vehicle used only on VM snapshot without game AC |

See `docs/11_ac_block_vs_hide_plan.md`, `driver2/VEHICLE_STEALTH.md`.
