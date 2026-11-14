# build/ — approved binaries only

This folder is intentionally empty.

The debug overlay is compiled into `../debug/`. This `build/` tree is written **only** after Kyle explicitly approves:

```powershell
.\dma\scripts\build-windows.ps1 -Section Build -IApprove
```

Do not copy `debug\RobRunDMA.exe` here by hand.
