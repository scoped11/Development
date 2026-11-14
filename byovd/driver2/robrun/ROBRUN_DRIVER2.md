# Rob Run + Driver2

Rob Run installs **EasyAntiCheat** (`EasyAntiCheat/`, `start_protected_game.exe`). A plain lab map with the ACE/delta profile does **not** match the game.

## What we use

| Step | Command |
|------|---------|
| Map (admin, once per boot) | `DriverLoader.exe` → **6**, or `.\driver2\robrun-lab-map.ps1` |
| Driver profile | `--profile robrun` → EAC replica + `--eac 7f --encptr f` on session map |
| Cheat | `CheatLoader.exe` → Rob Run tile (applies `EAC improve` IOCTL when `ROB RUN.exe` is running) |

## Will it work?

**Lab expectation:** same class of access as other EAC titles on this stack — kernel `ReadProcess` + session `0x844` improvements, not usermode `ReadProcessMemory`.

**Caveats (honest):**

- Not verified ban-safe; EAC may still see suspicious external reads or overlay.
- Your DMA path already reads `GameAssembly`; Driver2 should be equivalent if map + IOCTLs succeed and `Driver2Client probe` passes on **game PID/VA**.
- If reads fail: map **before** launching Rob Run when you can; ensure game was started via EAC (`ROB RUN.exe` running, not only launcher).

## Quick validation

```powershell
# After map + game in menu or match:
.\bin\Driver2Client.exe   # use built-in read test with ROB RUN pid + GameAssembly VA
```
