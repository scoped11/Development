# RobRun Offset MCP

Exact-RVA tools for Rob Run IL2CPP. This is **not** the stock IDA survey/decompile MCP.

Stock `ida-pro-mcp` snaps Hex-Rays to the containing function. On this GameAssembly that is often a 600–1500 line blob, so getter RVAs from `dump.cs` look like the wrong function. This server never walks the database.

## What to use

| Job | Tool | Needs IDA? |
|---|---|---|
| TypeInfo RVA | `lookup_typeinfo` | no |
| Method RVA | `lookup_method` | no |
| dump.cs field | `field_from_dump` | no |
| Exact disasm | `disasm_rva` | no (live PE) |
| `mov [rcx+X]; ret` | `confirm_getter` | no |
| TypeInfo → +0xB8 → [0] | `confirm_singleton` | no |
| RIP globals in one method | `rip_globals` | no |
| Xrefs to one slot | `ida_xrefs_to` | yes |
| Tight Hex-Rays | `ida_define_func` then `ida_decompile` | yes |

## Install

```powershell
.\dma\ida-offset-mcp\install-plugin.ps1
```

The MCP starts a **hidden idalib worker** (no IDA window) on first `ida_*` call, or when the Cursor server launches. It opens `GameAssembly.dll.i64` and listens on `http://127.0.0.1:13338/rpc`.

GUI IDA is optional. Do not leave `ida.exe` visible for this workflow.

Cursor `mcp.json` entry `robrun-ida` launches:

```text
python -m robrun_ida_mcp
```

from this directory. Reload MCP servers after the first install.

## Ports

- `13337` — existing ida-pro-mcp (wide tools; do not use for IL2CPP getters)
- `13338` — this plugin (exact EA only)
