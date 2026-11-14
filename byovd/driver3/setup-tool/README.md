# Driver3Setup.exe

Interactive + automatic setup for **driver3** on the **game PC (Host A)**, with a built-in cheat-developer orientation.

## Build

```powershell
.\driver3\setup-tool\build.ps1 -CopyToByovdBin
```

Output: `driver3\bin\Driver3Setup.exe` (and optional `byovd\bin\`).

## Run

```text
Driver3Setup.exe              # menu
Driver3Setup.exe --auto       # full Host A pipeline + cheat guide at end
Driver3Setup.exe --explain    # cheat integration guide only
```

Place the exe in `driver3\` or `driver3\bin\` so it finds `setup-host-driver3.ps1`.

VM Host B still needs elevated guest setup once — see `docs\REACH_100_PERCENT.md`.
