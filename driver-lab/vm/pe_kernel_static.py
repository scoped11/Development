#!/usr/bin/env python3
"""Static PE facts for EAC/BE kernel drivers. No execution, no driver load."""
from __future__ import annotations

import hashlib
import json
import os
import struct
import sys
from collections import Counter

KERNEL_ASCII = [
    b"FltRegisterFilter",
    b"FltUnregisterFilter",
    b"FltCreateCommunicationPort",
    b"FltCloseCommunicationPort",
    b"FltSendMessage",
    b"IoCreateDevice",
    b"IoCreateSymbolicLink",
    b"IoDeleteDevice",
    b"IoDeleteSymbolicLink",
    b"IoGetCurrentProcess",
    b"ObRegisterCallbacks",
    b"ObUnRegisterCallbacks",
    b"CmRegisterCallback",
    b"CmRegisterCallbackEx",
    b"CmUnRegisterCallback",
    b"PsSetCreateProcessNotifyRoutine",
    b"PsSetCreateProcessNotifyRoutineEx",
    b"PsSetCreateThreadNotifyRoutine",
    b"PsSetLoadImageNotifyRoutine",
    b"PsSetLoadImageNotifyRoutineEx",
    b"PsRemoveLoadImageNotifyRoutine",
    b"PsLookupProcessByProcessId",
    b"MmCopyVirtualMemory",
    b"MmGetSystemRoutineAddress",
    b"MmIsAddressValid",
    b"ZwQuerySystemInformation",
    b"NtQuerySystemInformation",
    b"NtReadVirtualMemory",
    b"NtWriteVirtualMemory",
    b"KeStackAttachProcess",
    b"KeUnstackDetachProcess",
    b"RtlPcToFileHeader",
    b"RtlGetVersion",
    b"ExAllocatePool",
    b"ExAllocatePoolWithTag",
    b"ExAllocatePool2",
    b"ObReferenceObjectByHandle",
    b"ObOpenObjectByPointer",
    b"ZwOpenProcess",
    b"ZwTerminateProcess",
    b"DEVICE_CONTROL",
    b"IRP_MJ",
    b"\\Device\\",
    b"\\DosDevices\\",
    b"BattlEye",
    b"BEDaisy",
    b"EasyAntiCheat",
]

KERNEL_UTF16 = [
    "\\Device\\",
    "\\DosDevices\\",
    "\\\\.\\",
    "BattlEye",
    "BEDaisy",
    "EasyAntiCheat",
    "EasyAntiCheat_EOS",
    "EasyAntiCheat_EOSSys",
]


def u16(b, o):
    return struct.unpack_from("<H", b, o)[0]


def u32(b, o):
    return struct.unpack_from("<I", b, o)[0]


def u64(b, o):
    return struct.unpack_from("<Q", b, o)[0]


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest().upper()


def parse_pe(path):
    with open(path, "rb") as f:
        data = f.read()
    if data[:2] != b"MZ":
        raise ValueError("not MZ")
    e_lfanew = u32(data, 0x3C)
    if data[e_lfanew : e_lfanew + 4] != b"PE\0\0":
        raise ValueError("not PE")
    coff = e_lfanew + 4
    machine = u16(data, coff)
    nsec = u16(data, coff + 2)
    timedate = u32(data, coff + 4)
    opt_size = u16(data, coff + 16)
    chars = u16(data, coff + 18)
    opt = coff + 20
    magic = u16(data, opt)
    if magic != 0x20B:
        raise ValueError(f"not PE32+ magic={magic:#x}")
    entry_rva = u32(data, opt + 16)
    image_base = u64(data, opt + 24)
    section_align = u32(data, opt + 32)
    file_align = u32(data, opt + 36)
    subsystem = u16(data, opt + 68)
    dllchars = u16(data, opt + 70)
    num_dirs = u32(data, opt + 108)
    dirs = []
    for i in range(min(num_dirs, 16)):
        rva = u32(data, opt + 112 + i * 8)
        size = u32(data, opt + 116 + i * 8)
        dirs.append({"index": i, "rva": rva, "size": size})
    sec_off = opt + opt_size
    sections = []
    for i in range(nsec):
        o = sec_off + i * 40
        name = data[o : o + 8].split(b"\0", 1)[0].decode("ascii", "replace")
        vsize = u32(data, o + 8)
        va = u32(data, o + 12)
        rawsz = u32(data, o + 16)
        rawptr = u32(data, o + 20)
        chars_s = u32(data, o + 36)
        sections.append(
            {
                "name": name or f"SECTION.{i}",
                "index": i,
                "va": va,
                "vsize": vsize,
                "rawptr": rawptr,
                "rawsz": rawsz,
                "chars": chars_s,
                "exec": bool(chars_s & 0x20000000),
                "nx": not bool(chars_s & 0x20000000),
            }
        )
    return {
        "path": path,
        "size": len(data),
        "sha256": hashlib.sha256(data).hexdigest().upper(),
        "machine": machine,
        "timedate": timedate,
        "characteristics": chars,
        "entry_rva": entry_rva,
        "image_base": image_base,
        "section_align": section_align,
        "file_align": file_align,
        "subsystem": subsystem,
        "dllchars": dllchars,
        "dirs": dirs,
        "sections": sections,
        "data": data,
    }


def rva_to_off(pe, rva):
    for s in pe["sections"]:
        end = s["va"] + max(s["vsize"], s["rawsz"])
        if s["va"] <= rva < end:
            delta = rva - s["va"]
            if delta < s["rawsz"]:
                return s["rawptr"] + delta, s
            return None, s
    return None, None


def va_to_off(pe, va):
    return rva_to_off(pe, va - pe["image_base"])


def section_at_rva(pe, rva):
    _, s = rva_to_off(pe, rva)
    return s


def read_rva(pe, rva, n):
    off, s = rva_to_off(pe, rva)
    if off is None:
        return None, s
    return pe["data"][off : off + n], s


def parse_pdata(pe):
    d = pe["dirs"][3] if len(pe["dirs"]) > 3 else None
    if not d or not d["rva"] or not d["size"]:
        return {"present": False}
    off, sec = rva_to_off(pe, d["rva"])
    if off is None:
        return {"present": True, "unreadable": True, "rva": d["rva"], "size": d["size"]}
    blob = pe["data"][off : off + d["size"]]
    n = d["size"] // 12
    starts = []
    sec_counts = Counter()
    for i in range(n):
        begin = u32(blob, i * 12)
        end = u32(blob, i * 12 + 4)
        unwind = u32(blob, i * 12 + 8)
        s = section_at_rva(pe, begin)
        name = s["name"] if s else "?"
        sec_counts[name] += 1
        starts.append({"begin_rva": begin, "end_rva": end, "unwind_rva": unwind, "section": name})
    # keep only non-blob (small executable) functions for listing
    small = [
        x
        for x in starts
        if x["section"] in (".text", "INIT", "PAGE")
        or (x["end_rva"] - x["begin_rva"]) <= 0x2000
        and x["section"] not in ("SECTION.6", "SECTION.7")
    ]
    return {
        "present": True,
        "rva": d["rva"],
        "size": d["size"],
        "count": n,
        "section_counts": dict(sec_counts),
        "text_or_init": [
            {
                "begin_va": hex(pe["image_base"] + x["begin_rva"]),
                "end_va": hex(pe["image_base"] + x["end_rva"]),
                "size": x["end_rva"] - x["begin_rva"],
                "section": x["section"],
            }
            for x in starts
            if x["section"] in (".text", "INIT", "PAGE", ".CRT")
        ],
        "first20": [
            {
                "begin_va": hex(pe["image_base"] + x["begin_rva"]),
                "end_va": hex(pe["image_base"] + x["end_rva"]),
                "size": x["end_rva"] - x["begin_rva"],
                "section": x["section"],
            }
            for x in starts[:20]
        ],
        "last20": [
            {
                "begin_va": hex(pe["image_base"] + x["begin_rva"]),
                "end_va": hex(pe["image_base"] + x["end_rva"]),
                "size": x["end_rva"] - x["begin_rva"],
                "section": x["section"],
            }
            for x in starts[-20:]
        ],
    }


def parse_debug(pe):
    d = pe["dirs"][6] if len(pe["dirs"]) > 6 else None
    if not d or not d["rva"]:
        return {"present": False}
    off, _ = rva_to_off(pe, d["rva"])
    if off is None:
        return {"present": True, "unreadable": True}
    out = []
    remain = d["size"]
    p = off
    while remain >= 28:
        dtype = u32(pe["data"], p + 12)
        size = u32(pe["data"], p + 16)
        addr = u32(pe["data"], p + 20)
        raw = u32(pe["data"], p + 24)
        rec = {"type": dtype, "size": size, "addr": addr, "rawptr": raw}
        if dtype == 2 and raw + 24 <= len(pe["data"]):  # CODEVIEW
            sig = pe["data"][raw : raw + 4]
            rec["sig"] = sig.decode("ascii", "replace")
            if sig == b"RSDS":
                guid = pe["data"][raw + 4 : raw + 20]
                age = u32(pe["data"], raw + 20)
                pdb = pe["data"][raw + 24 : raw + size].split(b"\0", 1)[0].decode("ascii", "replace")
                rec["guid"] = guid.hex()
                rec["age"] = age
                rec["pdb"] = pdb
        out.append(rec)
        p += 28
        remain -= 28
    return {"present": True, "entries": out}


def extract_c_strings(blob, min_len=8):
    found = []
    cur = []
    start = 0
    for i, ch in enumerate(blob):
        if 32 <= ch < 127:
            if not cur:
                start = i
            cur.append(ch)
        else:
            if len(cur) >= min_len:
                found.append((start, bytes(cur).decode("ascii")))
            cur = []
    if len(cur) >= min_len:
        found.append((start, bytes(cur).decode("ascii")))
    return found


def find_ascii(pe, needles):
    data = pe["data"]
    hits = []
    for n in needles:
        start = 0
        while True:
            i = data.find(n, start)
            if i < 0:
                break
            # extend to NUL
            end = data.find(b"\0", i)
            s = data[i : end if end > 0 and end - i < 200 else i + len(n)]
            hits.append({"off": hex(i), "match": s.decode("ascii", "replace")})
            start = i + 1
    return hits


def find_utf16(pe, needles):
    data = pe["data"]
    hits = []
    for n in needles:
        pat = n.encode("utf-16le")
        start = 0
        while True:
            i = data.find(pat, start)
            if i < 0:
                break
            # walk back/forward for full UTF-16 token
            lo = i
            while lo >= 2 and data[lo - 2 : lo] != b"\0\0" and 32 <= data[lo - 2] < 127 and data[lo - 1] == 0:
                lo -= 2
            hi = i + len(pat)
            while hi + 2 <= len(data) and data[hi : hi + 2] != b"\0\0" and 32 <= data[hi] < 127 and data[hi + 1] == 0:
                hi += 2
            s = data[lo:hi].decode("utf-16le", "replace")
            hits.append({"off": hex(i), "match": s})
            start = i + 2
    return hits


def parse_flt_reg(pe, va):
    off, sec = va_to_off(pe, va)
    if off is None:
        return {"error": "unmapped", "va": hex(va)}
    blob = pe["data"][off : off + 104]
    size = u16(blob, 0)
    ver = u16(blob, 2)
    flags = u32(blob, 4)
    fields = [
        ("ContextRegistration", 8),
        ("OperationRegistration", 16),
        ("FilterUnloadCallback", 24),
        ("InstanceSetupCallback", 32),
        ("InstanceQueryTeardownCallback", 40),
        ("InstanceTeardownStartCallback", 48),
        ("InstanceTeardownCompleteCallback", 56),
        ("GenerateFileNameCallback", 64),
        ("NormalizeNameComponentCallback", 72),
        ("NormalizeContextCleanupCallback", 80),
        ("TransactionNotificationCallback", 88),
        ("NormalizeNameComponentExCallback", 96),
    ]
    # Size 104 includes SectionNotificationCallback at offset 104? Wait:
    # 0x0202 adds SectionNotificationCallback after NormalizeNameComponentEx.
    # Layout of 13 pointers after Flags:
    # 8 + 13*8 = 112 would be 0x70, but notes say Size 104 (0x68).
    # Size 104 = 2+2+4 + 12*8 = 8+96 = 104. So 12 pointers, last is NormalizeNameComponentEx
    # (SectionNotificationCallback is in 0x0203 / later). Version 0x0202 with size 104
    # matches 12 callback/table pointers.
    out = {
        "va": hex(va),
        "file_off": hex(off),
        "section": sec["name"] if sec else None,
        "Size": size,
        "Version": hex(ver),
        "Flags": flags,
        "raw_hex": blob.hex(),
        "ptrs": {},
    }
    for name, o in fields:
        if o + 8 <= len(blob):
            p = u64(blob, o)
            out["ptrs"][name] = hex(p)
    return out


def parse_flt_ops(pe, va, max_recs=16):
    recs = []
    for i in range(max_recs):
        off, sec = va_to_off(pe, va + i * 32)
        if off is None:
            break
        mj = u16(pe["data"], off)
        flags = u16(pe["data"], off + 2)
        pre = u64(pe["data"], off + 8)
        post = u64(pe["data"], off + 16)
        recs.append(
            {
                "i": i,
                "MajorFunction": hex(mj),
                "Flags": flags,
                "Pre": hex(pre),
                "Post": hex(post),
            }
        )
        if mj == 0x80:  # IRP_MJ_OPERATION_END
            break
    return recs


def name_blob_around(pe, off, window=0x800):
    lo = max(0, off - 0x40)
    hi = min(len(pe["data"]), off + window)
    blob = pe["data"][lo:hi]
    strings = extract_c_strings(blob, min_len=6)
    return {
        "window_off": hex(lo),
        "strings": [{"rel": hex(lo + s[0]), "s": s[1]} for s in strings],
    }


def hexdump(pe, off, n):
    b = pe["data"][off : off + n]
    return b.hex()


def disasm_hint(pe, va, n=64):
    """Minimal x86-64 linear dump of first n bytes (hex + obvious jmps)."""
    off, sec = va_to_off(pe, va)
    if off is None:
        return {"error": "unmapped", "va": hex(va)}
    b = pe["data"][off : off + n]
    ins = []
    i = 0
    while i < len(b):
        # relative jmp/call
        if b[i] == 0xE9 and i + 5 <= len(b):
            rel = struct.unpack_from("<i", b, i + 1)[0]
            tgt = va + i + 5 + rel
            ins.append({"off": hex(va + i), "op": "jmp", "rel32": hex(rel), "tgt": hex(tgt)})
            i += 5
            continue
        if b[i] == 0xE8 and i + 5 <= len(b):
            rel = struct.unpack_from("<i", b, i + 1)[0]
            tgt = va + i + 5 + rel
            ins.append({"off": hex(va + i), "op": "call", "rel32": hex(rel), "tgt": hex(tgt)})
            i += 5
            continue
        if b[i] == 0xEB and i + 2 <= len(b):
            rel = struct.unpack_from("<b", b, i + 1)[0]
            tgt = va + i + 2 + rel
            ins.append({"off": hex(va + i), "op": "jmp8", "tgt": hex(tgt)})
            i += 2
            continue
        if b[i] == 0xC3:
            ins.append({"off": hex(va + i), "op": "ret"})
            i += 1
            continue
        if b[i] == 0x90:
            ins.append({"off": hex(va + i), "op": "nop"})
            i += 1
            continue
        # FF 25 / FF 15 rip-rel
        if i + 6 <= len(b) and b[i] == 0xFF and b[i + 1] in (0x15, 0x25):
            rel = struct.unpack_from("<i", b, i + 2)[0]
            tgt = va + i + 6 + rel
            ins.append(
                {
                    "off": hex(va + i),
                    "op": "jmp_iat" if b[i + 1] == 0x25 else "call_iat",
                    "mem": hex(tgt),
                }
            )
            i += 6
            continue
        ins.append({"off": hex(va + i), "op": "db", "b": f"{b[i]:02x}"})
        i += 1
        if len(ins) > 40:
            break
    return {
        "va": hex(va),
        "file_off": hex(off),
        "section": sec["name"] if sec else None,
        "hex": b.hex(),
        "linear": ins[:40],
    }


def summarize(pe, extra=None):
    entry_va = pe["image_base"] + pe["entry_rva"]
    entry_off, entry_sec = va_to_off(pe, entry_va)
    dirs_named = [
        "export",
        "import",
        "resource",
        "exception",
        "security",
        "basereloc",
        "debug",
        "arch",
        "globalptr",
        "tls",
        "loadcfg",
        "boundimp",
        "iat",
        "delayimp",
        "clr",
        "reserved",
    ]
    dirmap = {}
    for d in pe["dirs"]:
        name = dirs_named[d["index"]] if d["index"] < len(dirs_named) else str(d["index"])
        dirmap[name] = {"rva": hex(d["rva"]), "size": d["size"], "present": bool(d["rva"])}
    out = {
        "path": pe["path"],
        "size": pe["size"],
        "sha256": pe["sha256"],
        "machine": hex(pe["machine"]),
        "subsystem": pe["subsystem"],
        "dllchars": hex(pe["dllchars"]),
        "image_base": hex(pe["image_base"]),
        "entry_rva": hex(pe["entry_rva"]),
        "entry_va": hex(entry_va),
        "entry_file_off": hex(entry_off) if entry_off is not None else None,
        "entry_section": entry_sec["name"] if entry_sec else None,
        "sections": [
            {
                "name": s["name"],
                "va": hex(pe["image_base"] + s["va"]),
                "rva": hex(s["va"]),
                "vsize": s["vsize"],
                "rawptr": hex(s["rawptr"]),
                "rawsz": s["rawsz"],
                "chars": hex(s["chars"]),
                "exec": s["exec"],
            }
            for s in pe["sections"]
        ],
        "dirs": dirmap,
        "pdata": parse_pdata(pe),
        "debug": parse_debug(pe),
        "ascii_kernel": find_ascii(pe, KERNEL_ASCII),
        "utf16_kernel": find_utf16(pe, KERNEL_UTF16),
        "entry_linear": disasm_hint(pe, entry_va, 32),
    }
    if extra:
        out.update(extra)
    return out


def main():
    outdir = sys.argv[1] if len(sys.argv) > 1 else r"C:\Users\bj284\Downloads\TraeTesting\cursor\Development\ac-captures-host\2026-11-18"
    files = {
        "eos": r"C:\Users\bj284\Downloads\TraeTesting\cursor\Development\ac-captures-host\2026-11-17\EasyAntiCheat_EOS.sys",
        "legacy": r"C:\Users\bj284\Downloads\TraeTesting\cursor\Development\ac-captures-host\2026-11-17\EasyAntiCheat.sys",
        "bedaisy": r"C:\Users\bj284\Downloads\TraeTesting\cursor\Development\ac-captures-host\2026-11-18\BEDaisy.sys",
    }
    os.makedirs(outdir, exist_ok=True)
    results = {}

    # BEDaisy extras
    be = parse_pe(files["bedaisy"])
    be_extra = {
        "name_blob_0x25bf0": name_blob_around(be, 0x25BF0, 0xC00),
        "init_0x14002f000": disasm_hint(be, 0x14002F000, 80),
        "text_0x1400157c8": disasm_hint(be, 0x1400157C8, 128),
        "text_0x1400066bf": disasm_hint(be, 0x1400066BF, 128),
        "cookie_0x14002f02c": disasm_hint(be, 0x14002F02C, 64),
        "entry_plus": disasm_hint(be, 0x1425CCEA8, 24),
        "cyclic_0x14261cead": disasm_hint(be, 0x14261CEAD, 16),
        "cyclic_0x14261cea8": disasm_hint(be, 0x14261CEA8, 16),
    }
    results["bedaisy"] = summarize(be, be_extra)

    # Legacy extras
    lg = parse_pe(files["legacy"])
    flt = parse_flt_reg(lg, 0x1401060F0)
    ops = parse_flt_ops(lg, 0x140106160)
    lg_extra = {
        "flt_registration": flt,
        "flt_ops": ops,
        "pre_FUN_1400650ac": disasm_hint(lg, 0x1400650AC, 80),
        "unload_FUN_1400650b4": disasm_hint(lg, 0x1400650B4, 80),
        "resolver_FUN_1400855d0": disasm_hint(lg, 0x1400855D0, 96),
        "section6_entry_0x14071c246": disasm_hint(lg, 0x14071C246, 96),
        "trampoline_0x1400afde8": disasm_hint(lg, 0x1400AFDE8, 32),
    }
    results["legacy"] = summarize(lg, lg_extra)

    # EOS extras
    eos = parse_pe(files["eos"])
    # hunt FLT_REGISTRATION-like (Size=104, Version=0x0202) in first 8MB of mapped rdata
    flt_cands = []
    data = eos["data"]
    # scan file for 68 00 02 02 00 00 00 00 (Size=0x68, Ver=0x0202, Flags=0)
    pat = bytes.fromhex("6800020200000000")
    start = 0
    while True:
        i = data.find(pat, start)
        if i < 0:
            break
        flt_cands.append({"file_off": hex(i), "hex": data[i : i + 104].hex()})
        start = i + 1
        if len(flt_cands) >= 8:
            break
    eos_extra = {
        "flt_reg_candidates": flt_cands,
        "FUN_14038114f": disasm_hint(eos, 0x14038114F, 48),
        "FUN_140272948": disasm_hint(eos, 0x140272948, 48),
        "FUN_140272978": disasm_hint(eos, 0x140272978, 48),
        "FUN_1403a572e": disasm_hint(eos, 0x1403A572E, 32),
        "FUN_140123b1e": disasm_hint(eos, 0x140123B1E, 40),
        "utf16_around_0x30f3ce": None,
    }
    off, _ = rva_to_off(eos, 0x30F3CE)
    if off is not None:
        lo = max(0, off - 0x40)
        hi = min(len(eos["data"]), off + 0x80)
        chunk = eos["data"][lo:hi]
        # decode utf16 fragments
        try:
            eos_extra["utf16_around_0x30f3ce"] = {
                "file_off": hex(lo),
                "utf16": chunk.decode("utf-16le", "replace"),
                "hex": chunk.hex(),
            }
        except Exception as e:
            eos_extra["utf16_around_0x30f3ce"] = {"error": str(e), "hex": chunk.hex()}
    results["eos"] = summarize(eos, eos_extra)

    outp = os.path.join(outdir, "pe-kernel-static.json")
    with open(outp, "w", encoding="utf-8") as f:
        json.dump(results, f, indent=2)
    print("WROTE", outp)

    # compact stdout
    for k, r in results.items():
        print(f"\n=== {k} {r['sha256'][:16]}… entry {r['entry_va']} sec={r['entry_section']} ===")
        print(" pdata", r["pdata"].get("count"), "by_sec", r["pdata"].get("section_counts"))
        print(" text/init funcs", len(r["pdata"].get("text_or_init") or []))
        print(" ascii hits", len(r["ascii_kernel"]), "utf16 hits", len(r["utf16_kernel"]))
        if k == "legacy":
            print(" FLT", r["flt_registration"]["ptrs"])
            print(" OPS", r["flt_ops"])
        if k == "eos":
            print(" FLT cands", len(r["flt_reg_candidates"]))
            print(" 38114f", r["FUN_14038114f"]["linear"][:12])
        if k == "bedaisy":
            print(" name blob strings", [s["s"] for s in r["name_blob_0x25bf0"]["strings"][:40]])
            print(" INIT", r["init_0x14002f000"]["linear"][:8])
            print(" 157c8", r["text_0x1400157c8"]["linear"][:12])
            print(" 66bf", r["text_0x1400066bf"]["linear"][:12])


if __name__ == "__main__":
    main()
