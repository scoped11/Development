from __future__ import annotations

import struct
from dataclasses import dataclass, field
from pathlib import Path

import capstone

from .paths import module_path

MAX_INSNS = 64
IMAGEBASE_IL2CPP = 0x180000000


IMAGE_SCN_MEM_EXECUTE = 0x20000000


@dataclass
class Section:
    name: str
    va: int
    vsize: int
    raw: int
    rawsize: int
    chars: int = 0

    @property
    def executable(self) -> bool:
        return bool(self.chars & IMAGE_SCN_MEM_EXECUTE) or self.name.startswith(".text")


@dataclass
class PE:
    path: Path
    imagebase: int
    size_of_image: int
    sections: list[Section] = field(default_factory=list)
    data: bytes = b""

    def rva_to_off(self, rva: int) -> int | None:
        for sec in self.sections:
            span = max(sec.vsize, sec.rawsize)
            if sec.va <= rva < sec.va + span:
                off = sec.raw + (rva - sec.va)
                if 0 <= off < len(self.data):
                    return off
        return None


_pe_cache: dict[str, PE] = {}


_pe_mtime: dict[str, float] = {}


def load_pe(module: str = "GameAssembly") -> PE:
    path = module_path(module)
    key = str(path.resolve())
    mtime = path.stat().st_mtime
    cached = _pe_cache.get(key)
    if cached and _pe_mtime.get(key) == mtime:
        return cached
    data = path.read_bytes()
    e_lfanew = struct.unpack_from("<I", data, 0x3C)[0]
    coff = e_lfanew + 4
    nsec = struct.unpack_from("<H", data, coff + 2)[0]
    optsz = struct.unpack_from("<H", data, coff + 16)[0]
    opt = coff + 20
    magic = struct.unpack_from("<H", data, opt)[0]
    if magic != 0x20B:
        raise ValueError(f"{path} is not PE32+")
    imagebase = struct.unpack_from("<Q", data, opt + 24)[0]
    size_of_image = struct.unpack_from("<I", data, opt + 56)[0]
    sec_off = opt + optsz
    sections: list[Section] = []
    for i in range(nsec):
        o = sec_off + i * 40
        name = data[o : o + 8].split(b"\0", 1)[0].decode("ascii", "replace")
        vsize, va, rawsize, rawptr = struct.unpack_from("<IIII", data, o + 8)
        chars = struct.unpack_from("<I", data, o + 36)[0]
        sections.append(Section(name, va, vsize, rawptr, rawsize, chars))
    pe = PE(path, imagebase, size_of_image, sections, data)
    _pe_cache[key] = pe
    _pe_mtime[key] = mtime
    return pe


def parse_addr(addr: str | int, imagebase: int) -> tuple[int, int]:
    """Return (rva, va). Accepts RVA, preferred VA, or decimal."""
    if isinstance(addr, str):
        n = int(addr.strip(), 0)
    else:
        n = int(addr)
    if n >= imagebase:
        return n - imagebase, n
    return n, imagebase + n


def _rip_target(insn: capstone.CsInsn) -> int | None:
    for op in insn.operands:
        if op.type == capstone.x86.X86_OP_MEM and op.mem.base == capstone.x86.X86_REG_RIP:
            return insn.address + insn.size + op.mem.disp
        if op.type == capstone.x86.X86_OP_IMM and insn.mnemonic.startswith("j"):
            return int(op.imm)
        if op.type == capstone.x86.X86_OP_IMM and insn.mnemonic == "call":
            return int(op.imm)
    return None


def disasm_rva(addr: str | int, count: int = 16, module: str = "GameAssembly") -> dict:
    count = max(1, min(int(count), MAX_INSNS))
    pe = load_pe(module)
    rva, va = parse_addr(addr, pe.imagebase)
    off = pe.rva_to_off(rva)
    if off is None:
        return {"error": f"RVA 0x{rva:X} not in {pe.path.name}", "rva": hex(rva)}
    cs = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
    cs.detail = True
    blob = pe.data[off : off + count * 16]
    insns = []
    for insn in cs.disasm(blob, va):
        target = _rip_target(insn)
        rec = {
            "ea": hex(insn.address),
            "rva": hex(insn.address - pe.imagebase),
            "bytes": insn.bytes.hex(),
            "text": f"{insn.mnemonic} {insn.op_str}".strip(),
        }
        if target is not None:
            rec["target"] = hex(target)
            rec["target_rva"] = hex(target - pe.imagebase) if target >= pe.imagebase else hex(target)
        insns.append(rec)
        if len(insns) >= count:
            break
        if insn.mnemonic in ("ret", "jmp") and len(insns) >= 2:
            break
    return {
        "module": pe.path.name,
        "imagebase": hex(pe.imagebase),
        "size_of_image": hex(pe.size_of_image),
        "rva": hex(rva),
        "va": hex(va),
        "insns": insns,
        "source": "pe",
    }


def read_bytes(addr: str | int, size: int = 16, module: str = "GameAssembly") -> dict:
    size = max(1, min(int(size), 256))
    pe = load_pe(module)
    rva, va = parse_addr(addr, pe.imagebase)
    off = pe.rva_to_off(rva)
    if off is None:
        return {"error": f"RVA 0x{rva:X} not in {pe.path.name}"}
    raw = pe.data[off : off + size]
    return {
        "rva": hex(rva),
        "va": hex(va),
        "hex": raw.hex(),
        "len": len(raw),
        "source": "pe",
    }


def confirm_getter(addr: str | int, module: str = "GameAssembly") -> dict:
    info = disasm_rva(addr, count=8, module=module)
    insns = info.get("insns") or []
    if not insns:
        return {**info, "ok": False, "reason": "no instructions"}
    first = insns[0]["text"]
    disp = None
    kind = None
    import re

    m = re.search(r"\[(?:rcx|ecx) \+ (0x[0-9a-fA-F]+)\]", first)
    if not m:
        m = re.search(r"\[(?:rcx|ecx)\+(0x[0-9a-fA-F]+)\]", first)
    if m:
        disp = int(m.group(1), 16)
        if first.startswith("movss") or first.startswith("movsd"):
            kind = "float"
        elif first.startswith("movzx"):
            kind = "byte"
        elif first.startswith("mov eax") or first.startswith("mov eax,"):
            kind = "int"
        else:
            kind = "ptr"
    ended = any(x["text"].startswith("ret") for x in insns[:4])
    return {
        "ok": disp is not None and ended,
        "field_offset": hex(disp) if disp is not None else None,
        "kind": kind,
        "insns": insns[:6],
        "rva": info.get("rva"),
        "module": info.get("module"),
        "source": "pe",
    }


def confirm_singleton(addr: str | int, module: str = "GameAssembly") -> dict:
    info = disasm_rva(addr, count=24, module=module)
    insns = info.get("insns") or []
    pe = load_pe(module)
    typeinfo_rva = None
    saw_b8 = False
    saw_deref0 = False
    for insn in insns:
        text = insn["text"]
        if insn.get("target_rva") and (
            text.startswith("lea rcx") or text.startswith("mov rax, qword ptr [rip")
        ):
            tr = int(insn["target_rva"], 16)
            if 0x7F00000 <= tr <= 0x8F00000:
                typeinfo_rva = hex(tr)
        if "+ 0xb8" in text or "+0xb8" in text:
            saw_b8 = True
        if text in ("mov rax, qword ptr [rax]", "mov rax, qword ptr [rax]"):
            saw_deref0 = True
        if "qword ptr [rax]" in text and text.startswith("mov rax") and "+ 0xb8" not in text:
            if saw_b8:
                saw_deref0 = True
    return {
        "ok": bool(typeinfo_rva and saw_b8 and saw_deref0),
        "pattern": "TypeInfo -> klass+0xB8 static_fields -> [0]",
        "typeinfo_rva": typeinfo_rva,
        "typeinfo_va": hex(pe.imagebase + int(typeinfo_rva, 16)) if typeinfo_rva else None,
        "saw_static_fields": saw_b8,
        "saw_instance0": saw_deref0,
        "insns": insns,
        "rva": info.get("rva"),
        "source": "pe",
    }


def rip_globals(addr: str | int, count: int = 40, module: str = "GameAssembly") -> dict:
    info = disasm_rva(addr, count=count, module=module)
    pe = load_pe(module)
    hits = []
    for insn in info.get("insns") or []:
        if not insn.get("target_rva"):
            continue
        tr = int(insn["target_rva"], 16)
        if tr >= pe.size_of_image:
            continue
        hits.append(
            {
                "at": insn["rva"],
                "text": insn["text"],
                "target_rva": hex(tr),
                "target_va": hex(pe.imagebase + tr),
            }
        )
    return {
        "rva": info.get("rva"),
        "rip_targets": hits,
        "count": len(hits),
        "source": "pe",
    }


# Tiny IL2CPP getter thunks: mov rAX/ss/zx, [rcx+disp]; ret
_THUNK_PATTERNS: tuple[tuple[str, bytes, int, bytes], ...] = (
    ("ptr", bytes.fromhex("488b41"), 1, bytes.fromhex("c3")),
    ("ptr", bytes.fromhex("488b81"), 4, bytes.fromhex("c3")),
    ("int", bytes.fromhex("8b41"), 1, bytes.fromhex("c3")),
    ("int", bytes.fromhex("8b81"), 4, bytes.fromhex("c3")),
    ("byte", bytes.fromhex("0fb641"), 1, bytes.fromhex("c3")),
    ("byte", bytes.fromhex("0fb681"), 4, bytes.fromhex("c3")),
    ("float", bytes.fromhex("f30f1041"), 1, bytes.fromhex("c3")),
    ("float", bytes.fromhex("f30f1081"), 4, bytes.fromhex("c3")),
)


def _parse_disp(disp: str | int | None) -> int | None:
    if disp is None or disp == "":
        return None
    if isinstance(disp, int):
        return int(disp)
    return int(str(disp).strip(), 0)


def find_thunks(
    disp: str | int | None = None,
    kind: str | None = None,
    module: str = "GameAssembly",
    limit: int = 40,
) -> dict:
    """Find `mov [rcx+disp]; ret` getter thunks in executable sections of the live PE."""
    want = _parse_disp(disp)
    if want is None:
        return {"error": "disp is required (e.g. 0xD0, 0x28, 0x90, 0x358)"}
    kind_f = (kind or "").strip().lower() or None
    if kind_f == "all":
        kind_f = None
    limit = max(1, min(int(limit), 80))
    pe = load_pe(module)
    hits: list[dict] = []
    scanned = 0
    for sec in pe.sections:
        if not sec.executable or sec.rawsize <= 0:
            continue
        blob = pe.data[sec.raw : sec.raw + sec.rawsize]
        scanned += len(blob)
        for knd, prefix, dlen, suffix in _THUNK_PATTERNS:
            if kind_f and knd != kind_f:
                continue
            plen = len(prefix) + dlen + len(suffix)
            start = 0
            while len(hits) < limit:
                i = blob.find(prefix, start)
                if i < 0 or i + plen > len(blob):
                    break
                if blob[i + len(prefix) + dlen : i + plen] != suffix:
                    start = i + 1
                    continue
                raw_disp = blob[i + len(prefix) : i + len(prefix) + dlen]
                found = int.from_bytes(raw_disp, "little", signed=(dlen == 1))
                if found != want:
                    start = i + 1
                    continue
                rva = sec.va + i
                hits.append(
                    {
                        "rva": hex(rva),
                        "va": hex(pe.imagebase + rva),
                        "kind": knd,
                        "field_offset": hex(found),
                        "bytes": blob[i : i + plen].hex(),
                        "size": plen,
                    }
                )
                start = i + plen
    return {
        "disp": hex(want),
        "kind": kind_f or "all",
        "hits": hits,
        "count": len(hits),
        "truncated": len(hits) >= limit,
        "scanned_bytes": scanned,
        "module": pe.path.name,
        "source": "pe",
    }
