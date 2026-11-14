# Vehicle stealth — hide, encrypt, spoof (inspector truth table)

Cheat buyers often demand: *“Hide RTCore, encrypt it, spoof it as another driver.”* This doc is the **official** position for **driver2** — use it in peer review and chargeback defense.

## Summary

| Technique | Fixes RTCore **blocked** at load? | Reduces **lab** traces? | In driver2 **default**? |
|-----------|-----------------------------------|-------------------------|-------------------------|
| **Hide** (hygiene, quiet map, optional DKOM unlink) | No | Yes (partial) | Hygiene **yes**; DKOM **no** |
| **Encrypt** (pool/PE mutate on **payload**) | No | Mixed (signatures) | Lab IOCTLs only; **not** live vehicle XOR |
| **Spoof** (rename `.sys`, fake module name) | No | Misleading | **Not supported** (trust chain) |

**Tier 0 blocked** → VM, another vehicle, or Lane D — not more hide IOCTLs (`docs/11_ac_block_vs_hide_plan.md`).

---

## Hide

**What works (Tier 1 lab)**

1. **Pre–DriverEntry hygiene** — PiDDB, CI hash bucket, WdFilter bookkeeping, registry scrub without stopping vehicle (`vehicle_hygiene.hpp`).
2. **`--ac-safe` / `--inspector`** — quiet map profile; abort if AC up and vehicle never opens (unless `--force-map`).
3. **Keep vehicle service running** — required for stability; “hide by unloading” is invalid.
4. **Optional `PsLoadedModuleList` unlink** — `DkomUnlinkDriver(..., L"RTCore64.sys")` via `--dkom-unlink` / `--hide-all`. **PG-unsafe.** **Off** in `ApplyDriver2EverythingProfile()`.

**What hide does *not* do**

- Remove RTCore **code** from RAM, device object, or patched `MajorFunction`.
- Defeat **handle / pool / callback** heuristics (Tier 2 forensics).
- Bypass **Microsoft vulnerable-driver blocklist** or AC **hash deny** at load.

---

## Encrypt

**Payload (Driver2 / Betest)**

- Mapper may send **`0x832` mutate**, section scramble IOCTLs on **lab** paths.
- **Not** default for minimal stable comm.

**Vehicle (RTCore)**

- **`ENCRYPT_DRV_MEM`** intentionally **not** called — decrypt/execute mismatch → delayed BSOD (`main_nal.cpp`).

**Usermode**

- Mapper may use compile-time string encryption — reduces static analyst noise, not kernel AC.

**Inspector line:** encrypt is for **your cheat’s footprint and payload polymorphism**, not “encrypted RTCore AC can’t see.”

---

## Spoof

| Spoof type | Verdict |
|------------|---------|
| Rename `RTCore64.sys` on disk | Fails hash/block rules |
| Patch PE / re-sign without valid chain | **Out of scope** — breaks load or trust |
| Fake `BaseDllName` in module list | DKOM only; code still RTCore |
| “Present as WdFilter” | **Module stomp** — opt-in `Driver2Stomp` path (`STOMP_BYPASS.md`), not default pool map |

We **do not** ship or document RTCore re-signing or byte-patch to evade deny lists.

---

## Recommended inspector / dev map profile

```text
mapper_nal.exe bin\Driver2.sys --driver rtcore --no-auth --driver2
  --ac-replica <title> --ac-replica-bypass --ac-safe --stealth 3f
```

`Driver2Runner.exe --mode bypass --inspector` adds `--ac-safe --stealth 3f` automatically.

**Do not** use as default ranked response:

- `--hide-all` (DKOM unlink + extra signal)
- `--driver2-hide-symlink` (breaks `Driver2Client` reopen)
- `--full` / stacked AC spray on first map

---

## ACE / EAC / BE note

Strong AC still correlates **behavior** (manual map pool, foreign IRP, timing). Hide/encrypt/spoof of the **vehicle label** does not replace **title profiles** (`0x846` / `0x851` / `0x852`) or honest **Tier 0** when the vehicle will not open.

See `CHEAT_INSPECTOR.md` for the measurable gate.
