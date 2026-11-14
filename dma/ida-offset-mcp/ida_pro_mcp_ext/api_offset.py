"""Exact-RVA offset tools for IL2CPP. Does not snap to containing functions."""

from __future__ import annotations

import re
import sys
from pathlib import Path
from typing import Annotated, Any

import ida_bytes
import ida_funcs
import ida_hexrays
import ida_name
import ida_nalt
import ida_ua
import idaapi
import idautils
import idc

from .rpc import tool
from .sync import idasync
from .utils import parse_address

MAX_INSNS = 64
MAX_FUNC_BYTES = 256
MAX_DECOMPILE_LINES = 80
_DUMP_PKG = Path(r"C:\Users\baile\OneDrive\Desktop\Development\dma\ida-offset-mcp")
if str(_DUMP_PKG) not in sys.path:
    sys.path.insert(0, str(_DUMP_PKG))


def _imagebase() -> int:
    try:
        import ida_ida

        return int(ida_ida.inf_get_imagebase())
    except Exception:
        return int(idaapi.get_imagebase())


def _ea(addr: str | int) -> int:
    ea = parse_address(addr)
    base = _imagebase()
    if 0 < ea < 0x10000000:
        ea += base
    return ea


def _disasm_exact(ea: int, count: int) -> list[dict[str, Any]]:
    count = max(1, min(int(count), MAX_INSNS))
    insns: list[dict[str, Any]] = []
    cur = ea
    base = _imagebase()
    for _ in range(count):
        insn = ida_ua.insn_t()
        n = ida_ua.decode_insn(insn, cur)
        if n <= 0:
            break
        text = idaapi.tag_remove(idc.generate_disasm_line(cur, 0) or "")
        rec: dict[str, Any] = {
            "ea": hex(cur),
            "rva": hex(cur - base),
            "size": n,
            "text": text,
        }
        for i in range(8):
            if insn.ops[i].type == ida_ua.o_void:
                break
            val = idc.get_operand_value(cur, i)
            if insn.ops[i].type in (ida_ua.o_mem, ida_ua.o_near, ida_ua.o_far):
                rec["target"] = hex(val)
                rec["target_rva"] = hex(val - base) if val >= base else hex(val)
        insns.append(rec)
        low = text.lower().lstrip()
        if low.startswith("ret") or low.startswith("jmp "):
            break
        cur += n
    return insns


@tool
@idasync
def offset_status() -> dict[str, Any]:
    """Rob Run offset helper: imagebase, IDB, dump path. Prefer offset_* over disasm/decompile for IL2CPP getters."""
    base = _imagebase()
    return {
        "binary": ida_nalt.get_root_filename() or "",
        "idb": idc.get_idb_path() or "",
        "imagebase": hex(base),
        "input_path": ida_nalt.get_input_file_path() or "",
        "dump": r"C:\Users\baile\OneDrive\Desktop\Development\dumps\2026-11-12",
        "note": "Use offset_disasm / offset_confirm_getter for dump RVAs. Do not use disasm/decompile (they snap to 600-1500 line IL2CPP blobs).",
    }


@tool
@idasync
def offset_disasm(
    addr: Annotated[str, "RVA (0x904DA0) or VA (0x180904DA0). Does not snap to function start."],
    count: Annotated[int, "Max instructions (default 16, max 64)"] = 16,
) -> dict[str, Any]:
    """Disassemble at an exact RVA. Stops at ret. Use this instead of disasm for IL2CPP getters."""
    ea = _ea(addr)
    fn = ida_funcs.get_func(ea)
    return {
        "ea": hex(ea),
        "rva": hex(ea - _imagebase()),
        "insns": _disasm_exact(ea, count),
        "containing_func": hex(fn.start_ea) if fn else None,
        "containing_func_size": int(fn.size()) if fn else None,
    }


@tool
@idasync
def offset_confirm_getter(
    addr: Annotated[str, "Dump method RVA, e.g. Game.get_GameModel 0x904DA0"],
) -> dict[str, Any]:
    """Confirm a tiny IL2CPP getter: mov rax/ss,[rcx+disp]; ret. Returns field_offset."""
    ea = _ea(addr)
    insns = _disasm_exact(ea, 8)
    if not insns:
        pe = _pe_call("confirm_getter", addr)
        if isinstance(pe, dict) and pe.get("ok"):
            return {
                "ok": True,
                "field_offset": pe.get("field_offset"),
                "kind": pe.get("kind"),
                "insns": [],
                "ea": hex(ea),
                "rva": hex(ea - _imagebase()),
                "ida_ok": False,
                "pe": pe,
                "trust": "pe",
            }
        return {"ok": False, "reason": "no instructions", "ea": hex(ea), "pe": pe}
    first = insns[0]["text"]
    disp = None
    kind = None
    m = re.search(r"\[(?:rcx|ecx) \+ (0x[0-9a-fA-F]+)\]", first, re.I)
    if not m:
        m = re.search(r"\[(?:rcx|ecx)\+(0x[0-9a-fA-F]+)\]", first, re.I)
    if m:
        disp = int(m.group(1), 16)
        low = first.lower()
        if "movss" in low or "movsd" in low:
            kind = "float"
        elif "movzx" in low:
            kind = "byte"
        elif low.startswith("mov eax"):
            kind = "int"
        else:
            kind = "ptr"
    ended = any(x["text"].lower().startswith("ret") for x in insns[:4])
    return {
        "ok": disp is not None and ended,
        "field_offset": hex(disp) if disp is not None else None,
        "kind": kind,
        "insns": insns[:6],
        "ea": hex(ea),
        "rva": hex(ea - _imagebase()),
    }


@tool
@idasync
def offset_confirm_singleton(
    addr: Annotated[str, "get_Instance RVA, e.g. ContentContext.get_Instance 0xF052F0"],
) -> dict[str, Any]:
    """Confirm TypeInfo -> klass+0xB8 static_fields -> [0] singleton load."""
    ea = _ea(addr)
    insns = _disasm_exact(ea, 24)
    typeinfo_rva = None
    saw_b8 = False
    saw_deref0 = False
    for insn in insns:
        text = insn["text"].lower()
        if insn.get("target_rva") and (
            text.startswith("lea rcx") or "qword ptr [rip" in text and text.startswith("mov rax")
        ):
            tr = int(insn["target_rva"], 16)
            if 0x7F00000 <= tr <= 0x8F00000:
                typeinfo_rva = hex(tr)
        if "+ 0xb8" in text or "+0xb8" in text:
            saw_b8 = True
        if text in ("mov rax, [rax]", "mov rax, qword ptr [rax]"):
            saw_deref0 = True
    return {
        "ok": bool(typeinfo_rva and saw_b8 and saw_deref0),
        "pattern": "TypeInfo -> klass+0xB8 static_fields -> [0]",
        "typeinfo_rva": typeinfo_rva,
        "saw_static_fields": saw_b8,
        "saw_instance0": saw_deref0,
        "insns": insns,
        "ea": hex(ea),
        "rva": hex(ea - _imagebase()),
    }


@tool
@idasync
def offset_rip_globals(
    addr: Annotated[str, "Method RVA to scan for RIP-relative TypeInfo slots"],
    count: Annotated[int, "Max instructions (default 40, max 64)"] = 40,
) -> dict[str, Any]:
    """List RIP-relative globals in one method (TypeInfo candidates)."""
    ea = _ea(addr)
    insns = _disasm_exact(ea, count)
    hits = []
    for insn in insns:
        if insn.get("target_rva"):
            hits.append(
                {
                    "at": insn["rva"],
                    "text": insn["text"],
                    "target_rva": insn["target_rva"],
                    "target": insn.get("target"),
                }
            )
    return {"ea": hex(ea), "rva": hex(ea - _imagebase()), "rip_targets": hits, "count": len(hits)}


@tool
@idasync
def offset_define_func(
    addr: Annotated[str, "RVA to create a tight function so Hex-Rays is not the IL2CPP blob"],
    max_size: Annotated[int, "Function size in bytes (default 64, max 256)"] = 64,
) -> dict[str, Any]:
    """Create a small function at this RVA. Call before offset_decompile."""
    ea = _ea(addr)
    max_size = max(4, min(int(max_size), MAX_FUNC_BYTES))
    existing = ida_funcs.get_func(ea)
    if existing and existing.start_ea != ea:
        ida_funcs.del_func(existing.start_ea)
    ok = ida_funcs.add_func(ea, ea + max_size)
    if not ok:
        ok = bool(ida_funcs.add_func(ea))
        if ok:
            ida_funcs.set_func_end(ea, ea + max_size)
    fn = ida_funcs.get_func(ea)
    return {
        "ok": bool(fn and fn.start_ea == ea),
        "ea": hex(ea),
        "end": hex(fn.end_ea) if fn else None,
        "size": int(fn.size()) if fn else None,
    }


@tool
@idasync
def offset_decompile(
    addr: Annotated[str, "RVA. Refuses if containing function is larger than 256 bytes."],
    max_lines: Annotated[int, "Max pseudocode lines (default 40, max 80)"] = 40,
) -> dict[str, Any]:
    """Hex-Rays at this RVA only if the function is tiny. Otherwise use offset_define_func first."""
    ea = _ea(addr)
    max_lines = max(1, min(int(max_lines), MAX_DECOMPILE_LINES))
    fn = ida_funcs.get_func(ea)
    if fn and fn.size() > MAX_FUNC_BYTES:
        return {
            "error": (
                f"containing function is {fn.size()} bytes at {hex(fn.start_ea)}; "
                f"call offset_define_func first (max_size <= {MAX_FUNC_BYTES})."
            ),
            "containing_func": hex(fn.start_ea),
            "size": int(fn.size()),
        }
    if not ida_hexrays.init_hexrays_plugin():
        return {"error": "Hex-Rays not available"}
    cfunc = ida_hexrays.decompile(ea)
    if not cfunc:
        return {"error": f"decompile failed at {hex(ea)}"}
    lines = str(cfunc).splitlines()
    return {
        "ea": hex(ea),
        "rva": hex(ea - _imagebase()),
        "code": "\n".join(lines[:max_lines]),
        "total_lines": len(lines),
        "truncated": len(lines) > max_lines,
    }


@tool
@idasync
def offset_xrefs_to(
    addr: Annotated[str, "RVA or VA of a TypeInfo slot or code"],
    limit: Annotated[int, "Max xrefs (default 32, max 64)"] = 32,
) -> dict[str, Any]:
    """Bounded xrefs to one RVA. Does not scan the database."""
    ea = _ea(addr)
    limit = max(1, min(int(limit), 64))
    items = []
    base = _imagebase()
    for i, xref in enumerate(idautils.XrefsTo(ea, 0)):
        if i >= limit:
            break
        items.append(
            {
                "frm": hex(xref.frm),
                "frm_rva": hex(xref.frm - base),
                "frm_name": idc.get_name(xref.frm) or "",
                "iscode": bool(xref.iscode),
            }
        )
    return {"ea": hex(ea), "rva": hex(ea - base), "xrefs": items, "count": len(items)}


def _dump():
    from robrun_ida_mcp import dump_index

    return dump_index


def _pe():
    from robrun_ida_mcp import pe

    return pe


def _pe_call(name: str, addr: str, **kwargs) -> dict[str, Any] | None:
    try:
        return getattr(_pe(), name)(addr, **kwargs)
    except Exception as exc:
        return {"error": str(exc), "source": "pe"}


def _ida_sym(dump_name: str, suffix: str = "") -> str:
    n = (
        dump_name.replace("$$", "__")
        .replace(".", "_")
        .replace("<", "_")
        .replace(">", "_")
        .replace(",", "_")
        .replace(" ", "")
    )
    n = re.sub(r"[^0-9A-Za-z_]", "_", n)
    if n and n[0].isdigit():
        n = "_" + n
    n = (n[:200] or "unnamed") + suffix
    return n


@tool
def offset_lookup_typeinfo(
    query: Annotated[str, "Type name, e.g. Game.Game or Content.ContentContext"],
    limit: Annotated[int, "Max hits (default 10, max 20)"] = 10,
) -> dict[str, Any]:
    """Look up a TypeInfo RVA from script.json (no database scan)."""
    try:
        return _dump().lookup_typeinfo(query, limit=limit)
    except Exception as exc:
        return {"error": str(exc)}


@tool
def offset_lookup_method(
    query: Annotated[str, "Method name, e.g. Start.UnityEntry$$get_Game"],
    limit: Annotated[int, "Max hits (default 10, max 20)"] = 10,
) -> dict[str, Any]:
    """Look up a method RVA from script.json (no database scan)."""
    try:
        return _dump().lookup_method(query, limit=limit)
    except Exception as exc:
        return {"error": str(exc)}


@tool
def offset_field_from_dump(
    type_name: Annotated[str, "Fully qualified or short class name, e.g. Game.Game"],
    field_name: Annotated[str, "Field name, e.g. GameModel or _game"],
) -> dict[str, Any]:
    """Field offset from dump.cs for one type."""
    try:
        return _dump().field_from_dump(type_name, field_name)
    except Exception as exc:
        return {"error": str(exc)}


@tool
def offset_names_at_rva(
    addr: Annotated[str, "RVA or VA, e.g. 0x904DA0"],
    limit: Annotated[int, "Max names (default 20)"] = 20,
) -> dict[str, Any]:
    """Dump method and TypeInfo names that share this RVA (shared IL2CPP thunks)."""
    try:
        return _dump().names_at_rva(addr, limit=limit)
    except Exception as exc:
        return {"error": str(exc)}


@tool
def offset_find_thunk(
    disp: Annotated[str, "Field displacement, e.g. 0xD0 0x28 0x90 0x10 0x358"],
    kind: Annotated[str, "ptr, int, byte, float, or all"] = "ptr",
    limit: Annotated[int, "Max thunks (default 20, max 80)"] = 20,
) -> dict[str, Any]:
    """Find live-PE `mov [rcx+disp]; ret` getter thunks and join dump names. Does not use IDA."""
    try:
        found = _pe().find_thunks(disp=disp, kind=kind, limit=limit)
    except Exception as exc:
        return {"error": str(exc)}
    dump = _dump()
    for hit in found.get("hits") or []:
        names = dump.names_at_rva(hit["rva"], limit=10)
        hit["dump_methods"] = names.get("methods") or []
        hit["dump_method_count"] = names.get("method_count") or 0
        hit["dump_typeinfo"] = names.get("typeinfo") or []
    return found


@tool
@idasync
def offset_apply_dump_names(
    query: Annotated[str, "Dump name substring, e.g. Game.Game$$get_ or Content.ContentContext"],
    confirm_pe: Annotated[bool, "Skip RVAs whose live PE bytes differ from the IDB"] = True,
    limit: Annotated[int, "Max symbols to apply (default 20, max 40)"] = 20,
    force: Annotated[bool, "Apply even when IDB bytes do not match the live PE"] = False,
) -> dict[str, Any]:
    """Name IDA symbols from script.json at dump RVAs. Skips stale IDB bytes unless force=true."""
    q = query.strip()
    if not q:
        return {"error": "query required"}
    limit = max(1, min(int(limit), 40))
    dump = _dump()
    pe = _pe()
    base = _imagebase()
    methods = (dump.lookup_method(q, limit=limit).get("hits") or [])[:limit]
    types = (dump.lookup_typeinfo(q, limit=limit).get("hits") or [])[:limit]
    applied: list[dict[str, Any]] = []
    skipped: list[dict[str, Any]] = []

    def _pe_hex(rva: int, size: int) -> str | None:
        info = pe.read_bytes(hex(rva), size=size)
        if not isinstance(info, dict) or info.get("error"):
            return None
        return info.get("hex")

    def _ida_hex(ea: int, size: int) -> str | None:
        raw = ida_bytes.get_bytes(ea, size)
        if not raw:
            return None
        return raw.hex()

    seen: set[int] = set()
    for hit in methods:
        rva = int(hit["rva"], 16)
        if rva in seen:
            continue
        seen.add(rva)
        ea = base + rva
        size = 8
        pe_hex = _pe_hex(rva, size)
        ida_hex = _ida_hex(ea, size) if pe_hex else None
        mismatch = bool(pe_hex and ida_hex and pe_hex[: min(len(pe_hex), len(ida_hex))] != ida_hex[: min(len(pe_hex), len(ida_hex))])
        if confirm_pe and pe_hex is None:
            skipped.append({"rva": hit["rva"], "name": hit["name"], "reason": "rva not in live PE"})
            continue
        if confirm_pe and mismatch and not force:
            skipped.append(
                {
                    "rva": hit["rva"],
                    "name": hit["name"],
                    "reason": "idb_bytes_differ_from_live_pe",
                    "pe": pe_hex,
                    "idb": ida_hex,
                }
            )
            continue
        sym = _ida_sym(hit["name"])
        getter = pe.confirm_getter(hex(rva))
        if isinstance(getter, dict) and getter.get("ok"):
            nbytes = sum(len(x.get("bytes") or "") // 2 for x in (getter.get("insns") or [])) or 8
            existing = ida_funcs.get_func(ea)
            if existing and existing.start_ea != ea:
                ida_funcs.del_func(existing.start_ea)
            ida_funcs.add_func(ea, ea + nbytes)
        ok = bool(ida_name.set_name(ea, sym, ida_name.SN_NOWARN | ida_name.SN_FORCE))
        applied.append(
            {
                "kind": "method",
                "rva": hit["rva"],
                "ea": hex(ea),
                "dump": hit["name"],
                "ida": idc.get_name(ea) or sym,
                "ok": ok,
                "mismatch": mismatch,
            }
        )
        if len(applied) >= limit:
            break

    for hit in types:
        if len(applied) >= limit:
            break
        rva = int(hit["rva"], 16)
        if rva in seen:
            continue
        seen.add(rva)
        ea = base + rva
        pe_hex = _pe_hex(rva, 8)
        ida_hex = _ida_hex(ea, 8) if pe_hex else None
        mismatch = bool(pe_hex and ida_hex and pe_hex[:16] != ida_hex[:16])
        if confirm_pe and mismatch and not force:
            skipped.append(
                {
                    "rva": hit["rva"],
                    "name": hit["name"],
                    "reason": "idb_bytes_differ_from_live_pe",
                    "pe": pe_hex,
                    "idb": ida_hex,
                }
            )
            continue
        try:
            idc.create_qword(ea)
        except Exception:
            pass
        sym = _ida_sym(hit["name"], "_TypeInfo")
        ok = bool(ida_name.set_name(ea, sym, ida_name.SN_NOWARN | ida_name.SN_FORCE))
        applied.append(
            {
                "kind": "typeinfo",
                "rva": hit["rva"],
                "ea": hex(ea),
                "dump": hit["name"],
                "ida": idc.get_name(ea) or sym,
                "ok": ok,
                "mismatch": mismatch,
            }
        )

    return {
        "query": q,
        "applied": applied,
        "applied_count": len(applied),
        "skipped": skipped,
        "skipped_count": len(skipped),
        "note": "force=true required when the IDB is behind the live GameAssembly.dll",
    }
