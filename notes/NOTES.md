# Rob Run — DMA Cheat: Working Notes & References

> Running log. Update as we go. Separate *verified* facts from *inference*.

## 0. Target Game

| Item | Value | Source |
|---|---|---|
| Game | **Rob Run** (Unity **IL2CPP**) | dir listing |
| Install path | `D:\SteamLibrary\steamapps\common\Rob Run` | user |
| IL2CPP binary | `GameAssembly.dll` (147,842,560 B) | dir listing |
| Metadata | `ROB RUN_Data\il2cpp_data\Metadata\global-metadata.dat` (34,367,080 B) | dir listing |
| **Metadata version** | **39** (uint32 LE at file offset 0) | read first 4 bytes = 0x27 |
| **Anti-cheat** | **EasyAntiCheat (EAC)** | `EasyAntiCheat/` dir |
| Launcher | `start_protected_game.exe` (EAC protected) | dir listing |
| Existing IDA DB | `GameAssembly.dll.i64` (2.1 GB) + .id0/.id1/.id2/.nam/.til | dir listing (dated 11/11) |

## 1. Tooling

| Tool | Version | Location | Notes |
|---|---|---|---|
| **wklin8607/Il2CppDumper** | **v6.7.48** (net8.0) | `C:\Users\baile\AppData\Local\Temp\opencode\Il2CppDumper\wklin8607-v6.7.48\Il2CppDumper.exe` | actively-maintained fork; we had v6.7.46 (too old) |
| old Il2CppDumper | v6.7.46 | `...\Il2CppDumper\Il2CppDumper.exe` | outdated |
| Cpp2IL | 2022.0.7 (latest *stable*) | — | too old for v39; not used |
| .NET runtime | .NET 8.0.28 (+ 6.0.36, 9.0.17) | `dotnet --list-runtimes` | dumper targets net8.0 ✓ |
| **IDA Pro** | **9.4** | `C:\Program Files\IDA Professional 9.4` | installed |
| **IDA MCP** | `ida-mcp.exe` (46 KB) | `C:\Users\baile\.local\bin\ida-mcp.exe` | configured in `opencode.json` (`--agent=opencode`) |

### Config decisions
- Set `RequireAnyKey: false` so the CLI dumper does not wait for a keypress.
- Keep `DumpFieldOffset`, `DumpMethodOffset`, `GenerateStruct` ON (needed for offsets + C++ SDK).
- `ForceIl2CppVersion: false` — let the dumper auto-detect v39.

## 2. References (URLs)

- wklin8607/Il2CppDumper (chosen dumper, active fork): https://github.com/wklin8607/Il2CppDumper
- Original (stagnant, v39 issues open): https://github.com/Perfare/Il2CppDumper (issues #891, #892 = "metadata 39 not yet reverse engineered" — but the wklin fork has since added support; verify by running)
- Il2CppInspectorRedux: https://github.com/LukeFZ/Il2CppInspectorRedux
- Cpp2IL releases: https://github.com/SamboyCoding/Cpp2IL/releases
- IDA MCP: configured via `opencode.json` → `ida` server.

## 3. Plan

1. **Dump** metadata v39 → `dump\` with v6.7.48. Verify class/method dumps succeed.
2. **Identify game structs** from `dump.cs` / `DummyDll`: `Player`, `Character`, `Transform` (position/rotation), `Camera`, health, etc. (Rob Run specific).
3. **Verify offsets via IDA Pro MCP**: cross-reference dumped field offsets + method addresses against the decompiled/IDA DB; find base pointers (e.g., `UnityEngine::Object`, camera, local player) for a working DMA read chain.
4. **Build SDK**: C++ struct definitions mirroring the dumped types (from `generate-struct.h` / `dump.cs`).
5. **Research DMA cheat building + optimization** (read scheduling, prefetch, polling frequency, batching reads, anti-cheat DMA detection).
6. **Build the DMA cheat**: reader (PC2/reader) + overlay/aimbot/ESP + KMBox input. Basic features first (ESP, health, distance, crosshair, basic aimbot), then add features found in the structure.

## 4. DMA product (2026-11-12)

Workspace: `C:\Users\baile\OneDrive\Desktop\Development`
- Source: `dma\`
- Debug overlay: `debug\RobRunDMA.exe` (default)
- Approved overlay: `build\` — empty until `-Section Build -IApprove`
- Dump: `dumps\2026-11-12\` (metadata v39)
- Dumper: `tools\il2cpp-dumper\` (wklin 6.7.48)
- TypeInfo `BattleEntitiesModel = 0x81D15E8`; GameAssembly `0x8CFE600`

## 5. Open Questions
- Live attach on the 2nd PC / FPGA firmware in `2ND-PC`
- IDA optional confirmation of ContentContext @ RVA 0xF053C0
