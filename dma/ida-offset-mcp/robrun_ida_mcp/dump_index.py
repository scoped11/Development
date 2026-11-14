from __future__ import annotations

import json
import re
from pathlib import Path

from .paths import dump_dir

MAX_HITS = 20
IMAGEBASE_IL2CPP = 0x180000000
_rev_methods: dict[int, list[str]] | None = None
_rev_typeinfo: dict[int, list[str]] | None = None
_rev_key: str | None = None


def _index_path() -> Path:
    return dump_dir() / ".offset_index.json"


def _scan_script_json(script: Path) -> dict:
    typeinfo: dict[str, int] = {}
    methods: dict[str, int] = {}
    addr: int | None = None
    with script.open("r", encoding="utf-8", errors="ignore") as fh:
        for line in fh:
            if '"Address":' in line:
                try:
                    addr = int(line.split(":", 1)[1].strip().rstrip(","))
                except ValueError:
                    addr = None
                continue
            if '"Name":' not in line or addr is None:
                continue
            raw = line.split(":", 1)[1].strip().rstrip(",")
            try:
                name = json.loads(raw)
            except json.JSONDecodeError:
                addr = None
                continue
            if not isinstance(name, str):
                addr = None
                continue
            if name.endswith("_TypeInfo"):
                typeinfo[name[: -len("_TypeInfo")]] = addr
            elif "$$" in name:
                methods[name] = addr
            addr = None
    return {"typeinfo": typeinfo, "methods": methods}


def load_index(force: bool = False) -> dict:
    dump = dump_dir()
    cache = _index_path()
    script = dump / "script.json"
    if not script.is_file():
        raise FileNotFoundError(f"script.json missing in {dump}")
    if not force and cache.is_file() and cache.stat().st_mtime >= script.stat().st_mtime:
        return json.loads(cache.read_text(encoding="utf-8"))
    data = _scan_script_json(script)
    data["dump"] = str(dump)
    cache.write_text(json.dumps(data), encoding="utf-8")
    return data


def lookup_typeinfo(query: str, limit: int = MAX_HITS) -> dict:
    q = query.strip()
    if not q:
        return {"error": "empty query"}
    idx = load_index()
    items = idx.get("typeinfo") or {}
    hits = []
    ql = q.lower()
    if q in items:
        hits.append({"name": q, "rva": hex(items[q]), "va": hex(IMAGEBASE_IL2CPP + items[q])})
    else:
        for name, rva in items.items():
            if ql in name.lower():
                hits.append({"name": name, "rva": hex(rva), "va": hex(IMAGEBASE_IL2CPP + rva)})
                if len(hits) >= max(1, min(limit, MAX_HITS)):
                    break
    return {"query": q, "hits": hits, "count": len(hits), "indexed": len(items)}


def lookup_method(query: str, limit: int = MAX_HITS) -> dict:
    q = query.strip()
    if not q:
        return {"error": "empty query"}
    idx = load_index()
    items = idx.get("methods") or {}
    hits = []
    ql = q.lower()
    if q in items:
        hits.append({"name": q, "rva": hex(items[q]), "va": hex(IMAGEBASE_IL2CPP + items[q])})
    else:
        for name, rva in items.items():
            if ql in name.lower():
                hits.append({"name": name, "rva": hex(rva), "va": hex(IMAGEBASE_IL2CPP + rva)})
                if len(hits) >= max(1, min(limit, MAX_HITS)):
                    break
    return {"query": q, "hits": hits, "count": len(hits), "indexed": len(items)}


def _ensure_reverse() -> None:
    global _rev_methods, _rev_typeinfo, _rev_key
    idx = load_index()
    key = idx.get("dump")
    if _rev_key == key and _rev_methods is not None and _rev_typeinfo is not None:
        return
    methods: dict[int, list[str]] = {}
    for name, rva in (idx.get("methods") or {}).items():
        methods.setdefault(int(rva), []).append(name)
    typeinfo: dict[int, list[str]] = {}
    for name, rva in (idx.get("typeinfo") or {}).items():
        typeinfo.setdefault(int(rva), []).append(name)
    _rev_methods = methods
    _rev_typeinfo = typeinfo
    _rev_key = key


def _parse_rva(value: str | int) -> int:
    if isinstance(value, int):
        n = int(value)
    else:
        n = int(str(value).strip(), 0)
    if n >= IMAGEBASE_IL2CPP:
        n -= IMAGEBASE_IL2CPP
    return n


def _rank_dump_name(name: str) -> tuple:
    compiler = 1 if ("<>" in name or "DisplayClass" in name) else 0
    ns = 0 if name.startswith(("Game.", "Start.", "Content.")) else 1
    getter = 0 if "$$get_" in name or ".get_" in name else 1
    return (compiler, ns, getter, name.count("."), len(name), name)


def names_at_rva(addr: str | int, limit: int = MAX_HITS) -> dict:
    """Reverse lookup: dump method and TypeInfo names at one RVA."""
    rva = _parse_rva(addr)
    _ensure_reverse()
    cap = max(1, min(int(limit), MAX_HITS))
    methods = sorted((_rev_methods or {}).get(rva, []), key=_rank_dump_name)
    typeinfo = sorted((_rev_typeinfo or {}).get(rva, []), key=_rank_dump_name)
    return {
        "rva": hex(rva),
        "va": hex(IMAGEBASE_IL2CPP + rva),
        "methods": methods[:cap],
        "method_count": len(methods),
        "typeinfo": typeinfo[:cap],
        "typeinfo_count": len(typeinfo),
        "truncated": len(methods) > cap or len(typeinfo) > cap,
    }


_CLASS = re.compile(
    r"^(?:public |internal |protected |private )?(?:abstract |sealed |static )*(?:class|struct) (?P<name>\S+)"
)
_FIELD = re.compile(
    r"^\s+(?:public |private |protected |internal )(?:static )?(?:readonly )?"
    r"(?P<ty>.+?) (?P<field>\S+); // (?P<off>0x[0-9A-Fa-f]+)"
)
_NS = re.compile(r"^// Namespace: (?P<ns>\S*)\s*$")


def field_from_dump(type_name: str, field_name: str) -> dict:
    dump_cs = dump_dir() / "dump.cs"
    if not dump_cs.is_file():
        return {"error": f"dump.cs missing in {dump_dir()}"}
    want = type_name.strip()
    short = want.split(".")[-1]
    field = field_name.strip().lstrip("_")
    current_ns = ""
    in_class = False
    brace = 0
    fields: list[dict] = []
    fq = ""
    with dump_cs.open("r", encoding="utf-8", errors="ignore") as fh:
        for line in fh:
            mns = _NS.match(line)
            if mns and not in_class:
                current_ns = mns.group("ns")
                continue
            if not in_class:
                mcl = _CLASS.match(line)
                if not mcl:
                    continue
                name = mcl.group("name").split("<")[0]
                fq = f"{current_ns}.{name}" if current_ns else name
                if fq == want or name == short or fq.endswith("." + want):
                    in_class = True
                    brace = line.count("{") - line.count("}")
                continue
            brace += line.count("{") - line.count("}")
            mf = _FIELD.match(line)
            if mf:
                raw = mf.group("field")
                pretty = raw
                bm = re.match(r"^<(.+)>k__BackingField$", raw)
                if bm:
                    pretty = bm.group(1)
                fields.append(
                    {
                        "name": pretty,
                        "csharp": raw,
                        "offset": mf.group("off"),
                        "type": mf.group("ty").strip(),
                    }
                )
            if brace <= 0:
                break
    if not in_class:
        return {"error": f"type not found: {want}"}
    match = [f for f in fields if f["name"] == field or f["csharp"] == field_name or f["name"] == field_name]
    return {
        "type": fq,
        "field": field_name,
        "match": match[0] if match else None,
        "fields": None if match else fields[:40],
    }
