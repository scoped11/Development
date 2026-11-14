# IDA / offset MCP — RobRun

Use **`robrun-ida`** for offset work. Do not use stock `ida-pro-mcp` `decompile`, `disasm`, `survey_binary`, `search_text`, or `analyze_*` on GameAssembly IL2CPP getters — those snap to a 600–1500 line containing function and miss the dump RVA.

## Servers

| Cursor MCP | Port / mode | Use |
|---|---|---|
| **robrun-ida** | PE + dump; IDA plugin `127.0.0.1:13338` | Exact RVA disasm, TypeInfo, getters, bounded xrefs |
| ida-pro-mcp / user-ida-pro-mcp | 13337 / idalib | Only when you already have a small named function |

## Install plugin

```powershell
.\dma\ida-offset-mcp\install-plugin.ps1
```

Nothing to click. `robrun-ida` spawns a hidden idalib worker (`python -m robrun_ida_mcp.idalib_worker`, `CREATE_NO_WINDOW`) that opens `GameAssembly.dll.i64` and serves `http://127.0.0.1:13338`. Log: `debug/ida-offset-worker.log`. PE/dump tools work even before the worker is up.

Reload Cursor MCP servers after the first install (`robrun-ida` in `~\.cursor\mcp.json`).

## Tools (all bounded)

**No IDA (live DLL + `dumps/<date>/`):**

- `offset_status`
- `lookup_typeinfo` / `lookup_method` — script.json index, max 20 hits
- `field_from_dump` — one class, one field from dump.cs
- `disasm_rva` — exact RVA/VA, ≤64 insns, `GameAssembly` or `UnityPlayer`
- `confirm_getter` — `mov … [rcx+disp]; ret`
- `confirm_singleton` — TypeInfo → klass+0xB8 → [0]
- `rip_globals` — RIP-relative slots in one method
- `pe_bytes`

**IDA plugin:**

- `ida_info`
- `ida_disasm` — still exact EA, does not expand the blob
- `ida_xrefs_to` — one address, ≤64
- `ida_qword` / `ida_bytes` / `ida_name_at`
- `ida_define_func` — tight function ≤256 bytes at this RVA
- `ida_decompile` — Hex-Rays, ≤80 lines; **refuses** functions larger than 256 bytes

## Typical confirm

1. `lookup_method("Game.Game$$get_GameModel")` → RVA
2. `confirm_getter(rva)` → `0xd0`
3. `lookup_typeinfo("Game.Game")` → TypeInfo RVA
4. `confirm_singleton` on `Content.ContentContext$$get_Instance`
5. `ida_xrefs_to(typeinfo_rva)` if you need code refs (slots are 0 in a static IDB)

TypeInfo **slots** in the PE are often 0 until runtime. Confirm the **code** that loads them, not the static qword.

## Paths

- Game: `D:\SteamLibrary\steamapps\common\Rob Run`
- Dump: `Development\dumps\2026-11-12\`
- Plugin source: `dma/ida-offset-mcp/plugin/robrun_offset_mcp.py`
- Server: `dma/ida-offset-mcp/robrun_ida_mcp`
