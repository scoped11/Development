from __future__ import annotations

from typing import Any

from mcp.server.fastmcp import FastMCP

from . import dump_index, ida_client, pe
from .paths import dump_dir, game_dir

mcp = FastMCP(
    "robrun-ida",
    instructions=(
        "Exact RVA offset tools for Rob Run (Unity IL2CPP). "
        "Prefer these over stock IDA decompile/survey/search_text. "
        "disasm_rva/confirm_getter/confirm_singleton/rip_globals/lookup_* work from the PE + dump "
        "without IDA. ida_* tools need the RobRun Offset MCP plugin on 127.0.0.1:13338. "
        "Never request a database-wide scan."
    ),
)


def _err(exc: Exception) -> dict[str, Any]:
    return {"error": str(exc)}


@mcp.tool(description="Dump dir, game DLLs, and whether the IDA offset plugin is attached.")
def offset_status() -> dict[str, Any]:
    game = game_dir()
    dump = dump_dir()
    ida = ida_client.attached()
    return {
        "game": str(game),
        "dump": str(dump),
        "gameassembly": str(game / "GameAssembly.dll"),
        "unityplayer": str(game / "UnityPlayer.dll"),
        "ida": ida,
    }


@mcp.tool(description="Look up a TypeInfo RVA from script.json by exact or substring name. Max 20 hits.")
def lookup_typeinfo(query: str, limit: int = 10) -> dict[str, Any]:
    try:
        return dump_index.lookup_typeinfo(query, limit=limit)
    except Exception as exc:
        return _err(exc)


@mcp.tool(description="Look up a method RVA from script.json (Name like Start.UnityEntry$$get_Game). Max 20 hits.")
def lookup_method(query: str, limit: int = 10) -> dict[str, Any]:
    try:
        return dump_index.lookup_method(query, limit=limit)
    except Exception as exc:
        return _err(exc)


@mcp.tool(description="Field offset from dump.cs for one type. Pass Game.Game and GameModel or _game.")
def field_from_dump(type_name: str, field_name: str) -> dict[str, Any]:
    try:
        return dump_index.field_from_dump(type_name, field_name)
    except Exception as exc:
        return _err(exc)


@mcp.tool(
    description=(
        "Disassemble at an exact RVA/VA (does not snap to function start). "
        "count<=64. module=GameAssembly or UnityPlayer. source pe is the live DLL."
    )
)
def disasm_rva(addr: str, count: int = 16, module: str = "GameAssembly") -> dict[str, Any]:
    try:
        return pe.disasm_rva(addr, count=count, module=module)
    except Exception as exc:
        return _err(exc)


@mcp.tool(description="Confirm a tiny IL2CPP getter: mov rax/ss,[rcx+disp]; ret. Returns field_offset.")
def confirm_getter(addr: str, module: str = "GameAssembly") -> dict[str, Any]:
    try:
        return pe.confirm_getter(addr, module=module)
    except Exception as exc:
        return _err(exc)


@mcp.tool(
    description=(
        "Confirm ContentContext-style singleton: TypeInfo, klass+0xB8, [0]. "
        "Pass get_Instance RVA from lookup_method."
    )
)
def confirm_singleton(addr: str, module: str = "GameAssembly") -> dict[str, Any]:
    try:
        return pe.confirm_singleton(addr, module=module)
    except Exception as exc:
        return _err(exc)


@mcp.tool(description="List RIP-relative globals in one method (TypeInfo slot candidates). count<=64.")
def rip_globals(addr: str, count: int = 40, module: str = "GameAssembly") -> dict[str, Any]:
    try:
        return pe.rip_globals(addr, count=count, module=module)
    except Exception as exc:
        return _err(exc)


@mcp.tool(description="Dump method and TypeInfo names that share this RVA (shared IL2CPP thunks).")
def names_at_rva(addr: str, limit: int = 20) -> dict[str, Any]:
    try:
        return dump_index.names_at_rva(addr, limit=limit)
    except Exception as exc:
        return _err(exc)


@mcp.tool(
    description=(
        "Find live-PE `mov [rcx+disp]; ret` getter thunks. "
        "disp like 0xD0 0x28 0x90 0x10 0x358. kind=ptr|int|byte|float|all."
    )
)
def find_thunk(disp: str, kind: str = "ptr", limit: int = 20) -> dict[str, Any]:
    try:
        found = pe.find_thunks(disp=disp, kind=kind, limit=limit)
    except Exception as exc:
        return _err(exc)
    for hit in found.get("hits") or []:
        names = dump_index.names_at_rva(hit["rva"], limit=10)
        hit["dump_methods"] = names.get("methods") or []
        hit["dump_method_count"] = names.get("method_count") or 0
        hit["dump_typeinfo"] = names.get("typeinfo") or []
    return found


@mcp.tool(description="Read up to 256 bytes from the live PE at RVA/VA.")
def pe_bytes(addr: str, size: int = 16, module: str = "GameAssembly") -> dict[str, Any]:
    try:
        return pe.read_bytes(addr, size=size, module=module)
    except Exception as exc:
        return _err(exc)


@mcp.tool(description="Headless IDA identity (binary, idb, imagebase). Starts a hidden idalib worker if needed.")
def ida_info() -> dict[str, Any]:
    try:
        return ida_client.ensure_worker(wait=True)
    except Exception as exc:
        return _err(exc)


@mcp.tool(description="IDA disasm at exact EA/RVA. Does not expand to the containing IL2CPP blob. count<=64.")
def ida_disasm(addr: str, count: int = 16) -> dict[str, Any]:
    try:
        return ida_client.call("disasm", {"ea": addr, "count": count})
    except Exception as exc:
        return _err(exc)


@mcp.tool(description="Bounded xrefs to one RVA/VA. limit<=64. Use for TypeInfo slots.")
def ida_xrefs_to(addr: str, limit: int = 32) -> dict[str, Any]:
    try:
        return ida_client.call("xrefs_to", {"ea": addr, "limit": limit})
    except Exception as exc:
        return _err(exc)


@mcp.tool(description="Read a qword at RVA/VA from the open IDB (static TypeInfo slots are often 0).")
def ida_qword(addr: str) -> dict[str, Any]:
    try:
        return ida_client.call("qword", {"ea": addr})
    except Exception as exc:
        return _err(exc)


@mcp.tool(description="Read up to 256 bytes from the open IDB.")
def ida_bytes(addr: str, size: int = 16) -> dict[str, Any]:
    try:
        return ida_client.call("bytes", {"ea": addr, "size": size})
    except Exception as exc:
        return _err(exc)


@mcp.tool(description="Name at RVA/VA in the IDB.")
def ida_name_at(addr: str) -> dict[str, Any]:
    try:
        return ida_client.call("name", {"ea": addr})
    except Exception as exc:
        return _err(exc)


@mcp.tool(
    description=(
        "Create a tight function at this RVA so Hex-Rays is not the 1500-line IL2CPP blob. "
        "max_size<=256. Then call ida_decompile."
    )
)
def ida_define_func(addr: str, max_size: int = 64) -> dict[str, Any]:
    try:
        return ida_client.call("define_func", {"ea": addr, "max_size": max_size})
    except Exception as exc:
        return _err(exc)


@mcp.tool(
    description=(
        "Hex-Rays at RVA/VA, max_lines<=80. Refuses if the containing function is larger than 256 bytes; "
        "call ida_define_func first."
    )
)
def ida_decompile(addr: str, max_lines: int = 40) -> dict[str, Any]:
    try:
        return ida_client.call("decompile", {"ea": addr, "max_lines": max_lines})
    except Exception as exc:
        return _err(exc)


def main() -> None:
    import threading

    threading.Thread(target=lambda: ida_client.ensure_worker(wait=False), daemon=True).start()
    mcp.run(transport="stdio")
