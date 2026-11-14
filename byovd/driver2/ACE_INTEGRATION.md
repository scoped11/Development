# driver2 — ACE (AntiCheat Expert) integration

First-class **Delta Force** and **Arena Breakout** paths with Tencent ACE IOCTLs.

## Profiles

| Profile | Title | IOCTLs |
|---------|-------|--------|
| `deltaforce` | Delta Force | **`0x846`** strengthen, **`0x84E`** Delta, **`0x86A`** VGK max, **`0x869`** |
| `arenabreakout` | Arena Breakout Infinite | **`0x846`**, **`0x86A`**, **`0x869`** |

## Quick map (VM)

```powershell
.\bin\Driver2Runner.exe --profile deltaforce --mode bypass --force-map --inspector
.\bin\Driver2Runner.exe --profile abi --mode bypass --force-map
# aliases: ace, delta, df, arena, ace-deltaforce
```

## Mapper argv (bypass)

```text
--ac-replica deltaforce --driver2 --ac-replica-bypass --ace 3f --delta 1f --ace-vgk
--ac-replica arenabreakout --driver2 --ac-replica-bypass --ace 3f --ace-vgk
```

Early **`0x846`** (+ **`0x84E`** on Delta) runs right after callback strip.

## Captures / IDA

| Title | Bundle |
|-------|--------|
| Delta | `dumps/deltaforce-ace_*` — use **`kernel/ACE-GAME.sys`** from **game bundle** |
| Arena | `dumps/arenabreakout-ace_*` |

## Usermode API

```cpp
Driver2AceStrengthen(h, ACE_STR_ALL);
Driver2AceDelta(h, ACE_DELTA_ALL);   // Delta Force only
```

CLI:

```text
Driver2Client.exe ace-strengthen
Driver2Client.exe ace-delta
```

## Verify

```powershell
.\byovd\scripts\verify-pf-diag.ps1 -Profile deltaforce
.\byovd\scripts\verify-pf-diag.ps1 -Profile arenabreakout
```

## Gates

```powershell
.\driver2\developer-approval-gate.ps1 -Execute -ForceMap -IncludeAce
```

## Live ranked

ACE titles often block BYOVD (**`\\.\ACE-GAME`** + RTCore deny). Map on **no-AC VM** only unless your preflight says Tier 1.
