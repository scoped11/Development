"""IDA-thread RPC handlers. Imported only after idapro/IDAPython is live."""

from __future__ import annotations

import json
from http.server import BaseHTTPRequestHandler, HTTPServer, ThreadingHTTPServer
from typing import Any, Callable

import ida_bytes
import ida_funcs
import ida_kernwin
import ida_name
import ida_nalt
import ida_ua
import idaapi
import idautils
import idc

HOST = "127.0.0.1"
PORT = 13338
MAX_INSNS = 64
MAX_XREFS = 64
MAX_BYTES = 256
MAX_DECOMPILE_LINES = 80
MAX_FUNC_BYTES = 256


def _imagebase() -> int:
    try:
        import ida_ida

        return int(ida_ida.inf_get_imagebase())
    except Exception:
        return int(idaapi.get_imagebase())


def _parse_ea(value: Any) -> int:
    n = int(value) if isinstance(value, int) else int(str(value).strip(), 0)
    base = _imagebase()
    if n < base and n < 0x10000000:
        return base + n
    return n


def _on_ida_thread() -> bool:
    try:
        return not ida_kernwin.is_idaq()
    except Exception:
        return True


def _sync(fn: Callable[[], Any], write: bool = False) -> Any:
    if _on_ida_thread():
        return fn()
    box: dict[str, Any] = {}

    def thunk():
        try:
            box["ok"] = fn()
        except Exception as exc:
            box["err"] = f"{type(exc).__name__}: {exc}"
        return 1

    flag = ida_kernwin.MFF_WRITE if write else ida_kernwin.MFF_READ
    ida_kernwin.execute_sync(thunk, flag)
    if "err" in box:
        raise RuntimeError(box["err"])
    return box.get("ok")


def info() -> dict:
    hexrays = False
    try:
        import ida_hexrays

        hexrays = bool(ida_hexrays.init_hexrays_plugin())
    except Exception:
        pass
    base = _imagebase()
    return {
        "plugin": "robrun-offset-mcp",
        "backend": "idalib" if _on_ida_thread() else "idaq",
        "binary": ida_nalt.get_root_filename() or "",
        "idb": idc.get_idb_path() or "",
        "imagebase": hex(base),
        "input_path": ida_nalt.get_input_file_path() or "",
        "hexrays": hexrays,
    }


def disasm(params: dict) -> dict:
    ea = _parse_ea(params.get("ea") or params.get("rva"))
    count = max(1, min(int(params.get("count") or 16), MAX_INSNS))
    insns = []
    cur = ea
    stop = {ida_ua.NN_retn, ida_ua.NN_retf, ida_ua.NN_jmp}
    if hasattr(ida_ua, "NN_jmpni"):
        stop.add(ida_ua.NN_jmpni)
    for _ in range(count):
        insn = ida_ua.insn_t()
        n = ida_ua.decode_insn(insn, cur)
        if n <= 0:
            break
        text = idc.generate_disasm_line(cur, 0) or ""
        rec = {
            "ea": hex(cur),
            "rva": hex(cur - _imagebase()),
            "size": n,
            "text": idaapi.tag_remove(text),
        }
        ops = []
        for i in range(8):
            if insn.ops[i].type == ida_ua.o_void:
                break
            ops.append({"n": i, "type": int(insn.ops[i].type), "value": hex(idc.get_operand_value(cur, i))})
        if ops:
            rec["ops"] = ops
        insns.append(rec)
        if insn.itype in stop:
            break
        cur += n
    fn = ida_funcs.get_func(ea)
    return {
        "ea": hex(ea),
        "rva": hex(ea - _imagebase()),
        "insns": insns,
        "containing_func": hex(fn.start_ea) if fn else None,
        "containing_func_size": int(fn.size()) if fn else None,
        "source": "ida",
    }


def read_bytes(params: dict) -> dict:
    ea = _parse_ea(params.get("ea") or params.get("rva"))
    size = max(1, min(int(params.get("size") or 16), MAX_BYTES))
    raw = ida_bytes.get_bytes(ea, size) or b""
    return {"ea": hex(ea), "rva": hex(ea - _imagebase()), "hex": raw.hex(), "len": len(raw)}


def qword(params: dict) -> dict:
    ea = _parse_ea(params.get("ea") or params.get("rva"))
    val = ida_bytes.get_qword(ea)
    return {
        "ea": hex(ea),
        "rva": hex(ea - _imagebase()),
        "value": hex(val),
        "name": ida_name.get_ea_name(val) or "",
    }


def name_at(params: dict) -> dict:
    ea = _parse_ea(params.get("ea") or params.get("rva"))
    return {
        "ea": hex(ea),
        "rva": hex(ea - _imagebase()),
        "name": ida_name.get_ea_name(ea) or "",
        "flags": hex(ida_bytes.get_flags(ea)),
    }


def xrefs_to(params: dict) -> dict:
    ea = _parse_ea(params.get("ea") or params.get("rva"))
    limit = max(1, min(int(params.get("limit") or 32), MAX_XREFS))
    items = []
    for i, xref in enumerate(idautils.XrefsTo(ea, 0)):
        if i >= limit:
            break
        items.append(
            {
                "frm": hex(xref.frm),
                "frm_rva": hex(xref.frm - _imagebase()),
                "frm_name": ida_name.get_ea_name(xref.frm) or "",
                "type": int(xref.type),
                "iscode": bool(xref.iscode),
            }
        )
    return {"ea": hex(ea), "rva": hex(ea - _imagebase()), "xrefs": items, "count": len(items)}


def define_func(params: dict) -> dict:
    ea = _parse_ea(params.get("ea") or params.get("rva"))
    max_size = max(4, min(int(params.get("max_size") or 64), MAX_FUNC_BYTES))
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


def decompile_at(params: dict) -> dict:
    import ida_hexrays

    ea = _parse_ea(params.get("ea") or params.get("rva"))
    max_lines = max(1, min(int(params.get("max_lines") or 40), MAX_DECOMPILE_LINES))
    fn = ida_funcs.get_func(ea)
    if fn and fn.size() > MAX_FUNC_BYTES:
        return {
            "error": (
                f"containing function is {fn.size()} bytes starting {hex(fn.start_ea)}; "
                f"refusing wide decompile. Call ida_define_func first (max_size <= {MAX_FUNC_BYTES})."
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


HANDLERS = {
    "ping": lambda _p: {"ok": True},
    "info": lambda _p: info(),
    "disasm": disasm,
    "bytes": read_bytes,
    "qword": qword,
    "name": name_at,
    "xrefs_to": xrefs_to,
    "define_func": define_func,
    "decompile": decompile_at,
}


class Handler(BaseHTTPRequestHandler):
    def log_message(self, fmt: str, *args: Any) -> None:
        return

    def _send(self, code: int, payload: dict) -> None:
        raw = json.dumps(payload).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(raw)))
        self.end_headers()
        self.wfile.write(raw)

    def do_GET(self) -> None:  # noqa: N802
        if self.path in ("/", "/health"):
            try:
                result = _sync(info)
                self._send(200, {"ok": True, "result": result})
            except Exception as exc:
                self._send(500, {"ok": False, "error": str(exc)})
            return
        self._send(404, {"error": "not found"})

    def do_POST(self) -> None:  # noqa: N802
        if self.path != "/rpc":
            self._send(404, {"error": "not found"})
            return
        length = int(self.headers.get("Content-Length") or 0)
        if length <= 0 or length > 65536:
            self._send(400, {"jsonrpc": "2.0", "error": {"message": "bad body"}})
            return
        try:
            req = json.loads(self.rfile.read(length).decode("utf-8"))
        except Exception:
            self._send(400, {"jsonrpc": "2.0", "error": {"message": "invalid json"}})
            return
        method = req.get("method")
        params = req.get("params") or {}
        req_id = req.get("id")
        fn = HANDLERS.get(method)
        if not fn:
            self._send(
                200,
                {"jsonrpc": "2.0", "id": req_id, "error": {"message": f"unknown method {method}"}},
            )
            return
        write = method in ("define_func", "decompile")
        try:
            result = _sync(lambda: fn(params), write=write)
            self._send(200, {"jsonrpc": "2.0", "id": req_id, "result": result})
        except Exception as exc:
            self._send(200, {"jsonrpc": "2.0", "id": req_id, "error": {"message": str(exc)}})


def run_server(host: str = HOST, port: int = PORT, threaded: bool = False) -> None:
    cls = ThreadingHTTPServer if threaded else HTTPServer
    httpd = cls((host, port), Handler)
    httpd.serve_forever(poll_interval=0.3)
