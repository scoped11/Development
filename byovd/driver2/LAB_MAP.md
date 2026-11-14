# Lab map — one driver, which profile?

## Short answer

| Layer | One map for all? |
|-------|------------------|
| **Kernel map (once per boot)** | **Yes** — use **`universal`** + `--mode session` (DriverLoader **8** or `-lab-universal`). Mapper applies EAC + BattlEye + ACE replica IOCTLs (`g_acReplicaLab == 6`). |
| **Right before you launch a cheat** | **Per game** — refresh the family that title actually runs (EAC pid IOCTL for Rob Run, BE for Unturned, ACE for Delta/ABI). CheatLoader does this from the game tile. |

Ranked-safe Rust/R6 stays **one title per map** on purpose (dump tracking, no `0x869`).

## Title-specific (lighter, optional)

| Game | Profile | Why |
|------|---------|-----|
| Rob Run | `robrun` → EAC | Folder has EasyAntiCheat only |
| Unturned | `r6` → BE | BattlEye |
| Delta / ABI | `deltaforce` / `arenabreakout` | ACE + VGK |

Use these if you only play one AC family and want a thinner mask than universal.

## Commands

```text
DriverLoader.exe -lab-universal     # Rob Run + Unturned + ACE lab on one PC
DriverLoader.exe -lab-robrun        # EAC-shaped only
Driver2Runner.exe --profile universal --mode session --driver rtcore --skip-rw
```

`profiles/universal.json` warns: heaviest mask — prefer title profile for production; universal is fine for a mixed game PC.
