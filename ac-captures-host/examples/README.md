# Examples

## process_rw.cpp

Minimal flow after `mapper_nal` maps **RwTest.sys** or **Betest.sys**:

1. `BetestOpenDevice()` — scan `\\.\MsFlt0` … names
2. `BetestSessionAuth(h)` — `IOCTL 0x870`
3. `BetestReadProcess(h, pid, &localMarker, &out, 8)` — `IOCTL 0x805`

Build (Developer Command Prompt):

```text
cl /EHsc /std:c++17 process_rw.cpp /I..\sdk /Feprocess_rw.exe
```

Map first from `..\..\byovd` (see `..\docs\DRIVER_LAB_GUIDE.md`).

`--emit-manifest` prints JSON for Lane D kernel proof scripts.
