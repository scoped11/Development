# ida-pro-mcp offset tools

Exact-RVA helpers live in the already-connected **ida-pro-mcp** server
(`python -m ida_pro_mcp.idalib_supervisor --stdio`).

Install (after pip upgrade of ida-pro-mcp):

```
Copy-Item dma\ida-offset-mcp\ida_pro_mcp_ext\api_offset.py `
  "$env:LOCALAPPDATA\Programs\Python\Python311\Lib\site-packages\ida_pro_mcp\ida_mcp\api_offset.py"
```

Then keep `from . import api_offset` in that package's `__init__.py`.

Reload the MCP in Cursor after install. Open GameAssembly with `idb_open`,
then use `offset_*` instead of `disasm` / `decompile` / `survey_binary`.
