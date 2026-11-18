#!/usr/bin/env python3
"""Static map of EAC EOS / legacy / BEDaisy VM stubs. No driver load."""
from __future__ import annotations

import hashlib
import json
import os
import struct
from collections import Counter, defaultdict

STUB = bytes.fromhex("4989842490010000")  # mov [r12+0x190], rax
ADD_108 = bytes.fromhex("4903842408010000")  # add rax, [r12+0x108]
JMP_RAX = bytes.fromhex("FFE0")
LEA_R12_RSP8 = bytes.fromhex("4C8D642408")
SUB_RSP_108 = bytes.fromhex("4881EC08010000")
RELOC_MOV = bytes.fromhex("48C7C0")  # mov rax, imm32 (sign-extended)
MOVABS_RAX = bytes.fromhex("48B8")

KNOWN = {
    "eos": {
        "path": r"C:\Users\bj284\Downloads\TraeTesting\cursor\Development\ac-captures-host\2026-11-17\EasyAntiCheat_EOS.sys",
        "sha": "26797A1122781CAF12F1A686E33A0883FA1E2895B40C2249932E319B338841C7",
        "entry_va": 0x140272978,
        "gs_cookie_va": 0x140319220,
        "save_frame_va": 0x140381041,
        "reloc_va": 0x14038114F,
        "dispatch_stub_va": 0x140123B1E,
        "flt_cbs": {
            "unload": 0x14023E209,
            "create_setinfo": 0x14023E237,
            "section_sync": 0x14023E269,
        },
    },
    "legacy": {
        "path": r"C:\Users\bj284\Downloads\TraeTesting\cursor\Development\ac-captures-host\2026-11-17\EasyAntiCheat.sys",
        "sha": "DD60A8CD0771B4FC548FDCEA68EA0FC78FBAAC58A77099731A62E70B03EB1C2A",
        "entry_va": 0x1400AFDE8,
        "section6_entry": 0x14071C246,
    },
    "bedaisy": {
        "path": r"C:\Users\bj284\Downloads\TraeTesting\cursor\Development\ac-captures-host\2026-11-18\BEDaisy.sys",
        "sha": "E7BCF344EEFA7A4EBEA9B048E376958A5AD711489249033FA480A7EC8B6C018E",
        "entry_va": 0x1425CCEA8,
        "init_va": 0x14002F000,
        "opaque_va": 0x1400157C8,
        "section6_land": 0x1417C4BA1,
        "cookie_init": 0x14002F02C,
        "gs_cookie_va": 0x14002A138,
    },
}

OUT_DIR = r"C:\Users\bj284\Downloads\TraeTesting\cursor\Development\ac-captures-host\2026-11-18"
WT_OUT = r"C:\Users\bj284\Downloads\TraeTesting\cursor\Development-wt-vm-interp-1b34\ac-captures-host\2026-11-18"


def u16(b, o):
    return struct.unpack_from("<H", b, o)[0]


def u32(b, o):
    return struct.unpack_from("<I", b, o)[0]


def u64(b, o):
    return struct.unpack_from("<Q", b, o)[0]


def i32(b, o):
    return struct.unpack_from("<i", b, o)[0]


def sha256_bytes(data):
    return hashlib.sha256(data).hexdigest().upper()


def parse_pe(path):
    with open(path, "rb") as f:
        data = f.read()
    e_lfanew = u32(data, 0x3C)
    coff = e_lfanew + 4
    nsec = u16(data, coff + 2)
    opt_size = u16(data, coff + 16)
    opt = coff + 20
    entry_rva = u32(data, opt + 16)
    image_base = u64(data, opt + 24)
    num_dirs = u32(data, opt + 108)
    dirs = []
    for i in range(min(num_dirs, 16)):
        dirs.append({"rva": u32(data, opt + 112 + i * 8), "size": u32(data, opt + 116 + i * 8)})
    sec_off = opt + opt_size
    sections = []
    for i in range(nsec):
        o = sec_off + i * 40
        name = data[o : o + 8].split(b"\0", 1)[0].decode("ascii", "replace") or f"SECTION.{i}"
        sections.append(
            {
                "name": name,
                "index": i,
                "va": u32(data, o + 12),
                "vsize": u32(data, o + 8),
                "rawsz": u32(data, o + 16),
                "rawptr": u32(data, o + 20),
                "chars": u32(data, o + 36),
                "exec": bool(u32(data, o + 36) & 0x20000000),
            }
        )
    return {
        "path": path,
        "data": data,
        "sha256": sha256_bytes(data),
        "entry_rva": entry_rva,
        "image_base": image_base,
        "dirs": dirs,
        "sections": sections,
        "size": len(data),
    }


def rva_to_off(pe, rva):
    for s in pe["sections"]:
        if s["va"] <= rva < s["va"] + max(s["vsize"], s["rawsz"]):
            delta = rva - s["va"]
            if delta < s["rawsz"]:
                return s["rawptr"] + delta, s
            return None, s
    return None, None


def va_to_off(pe, va):
    return rva_to_off(pe, va - pe["image_base"])


def off_to_va(pe, off):
    for s in pe["sections"]:
        if s["rawptr"] <= off < s["rawptr"] + s["rawsz"]:
            return pe["image_base"] + s["va"] + (off - s["rawptr"]), s
    return None, None


def read_va(pe, va, n):
    off, s = va_to_off(pe, va)
    if off is None:
        return None, s
    return pe["data"][off : off + n], s


def find_all(hay, needle):
    out = []
    i = 0
    while True:
        j = hay.find(needle, i)
        if j < 0:
            return out
        out.append(j)
        i = j + 1


def classify_prefix(b):
    if not b:
        return "unmapped"
    if b.startswith(bytes.fromhex("48B8")) and STUB in b[:24]:
        return "nested_cookie_stub"
    if b.startswith(bytes.fromhex("49C7C0")) or b.startswith(bytes.fromhex("48C7C0")):
        return "mov_imm32_reg"
    if b.startswith(bytes.fromhex("4881EC")) or b.startswith(bytes.fromhex("4883EC")):
        return "stack_alloc"
    if b.startswith(bytes.fromhex("4C8D6424")):
        return "lea_r12_rsp"
    if b.startswith(bytes.fromhex("410FB6")) or b.startswith(bytes.fromhex("410FB7")):
        return "movzx_r12"
    if b.startswith(bytes.fromhex("FF24")) or b.startswith(bytes.fromhex("41FF24")):
        return "jmp_table"
    if b.startswith(bytes.fromhex("FFE0")):
        return "jmp_rax"
    if b.startswith(b"\xe9"):
        return "jmp_rel32"
    if b.startswith(b"\xeb"):
        return "jmp_rel8"
    if b.startswith(bytes.fromhex("4889")):
        return "mov_mem_or_reg"
    if b.startswith(bytes.fromhex("488B")):
        return "load_mem"
    if b.startswith(bytes.fromhex("4989")):
        return "store_r12"
    if b.startswith(bytes.fromhex("0F11")) or b.startswith(bytes.fromhex("440F11")):
        return "xmm_save"
    if b.startswith(bytes.fromhex("0F10")) or b.startswith(bytes.fromhex("440F10")):
        return "xmm_load"
    return f"other_{b[:1].hex()}"


def parse_cookie_stub(data, off):
    """Parse `movabs rax,imm64; mov [r12+0x190],rax; mov eax,imm32; add rax,[r12+0x108]; jmp rax`."""
    # allow leading junk: look back up to 16 bytes for 48 B8
    start = None
    for back in range(0, 17):
        s = off - back
        if s >= 0 and data[s : s + 2] == MOVABS_RAX:
            start = s
            break
    if start is None:
        return None
    if data[start + 10 : start + 18] != STUB:
        return None
    if data[start + 18] != 0xB8:  # mov eax, imm32
        return None
    if data[start + 23 : start + 31] != ADD_108:
        return None
    if data[start + 31 : start + 33] != JMP_RAX:
        return None
    cookie = u64(data, start + 2)
    addend = u32(data, start + 19)
    return {"file_off": start, "cookie": cookie, "addend": addend, "size": 33}


def r12_disp_hits(data, opcode_prefix):
    """opcode_prefix is bytes before disp32, e.g. 49 89 84 24."""
    hits = Counter()
    examples = defaultdict(list)
    i = 0
    n = len(data)
    plen = len(opcode_prefix)
    while True:
        j = data.find(opcode_prefix, i)
        if j < 0:
            break
        if j + plen + 4 <= n:
            disp = u32(data, j + plen)
            hits[disp] += 1
            if len(examples[disp]) < 3:
                examples[disp].append(j)
        i = j + 1
    return hits, examples


def find_pointer_runs(pe, min_len=16):
    """Aligned QWORD runs whose values all land in one executable section."""
    ib = pe["image_base"]
    exec_ranges = []
    for s in pe["sections"]:
        if s["exec"]:
            lo = ib + s["va"]
            hi = lo + s["vsize"]
            exec_ranges.append((lo, hi, s["name"]))

    def which(va):
        for lo, hi, name in exec_ranges:
            if lo <= va < hi:
                return name
        return None

    runs = []
    for s in pe["sections"]:
        if not s["exec"] and s["name"] not in (".rdata", "SECTION.7", "SECTION.6"):
            # still scan rdata-like
            pass
        blob = pe["data"][s["rawptr"] : s["rawptr"] + s["rawsz"]]
        i = 0
        blen = len(blob) - 8
        while i <= blen:
            if (s["rawptr"] + i) & 7:
                i += 8 - ((s["rawptr"] + i) & 7)
                continue
            va0 = u64(blob, i)
            sec0 = which(va0)
            if not sec0:
                i += 8
                continue
            k = 1
            while i + (k + 1) * 8 <= len(blob):
                v = u64(blob, i + k * 8)
                if which(v) != sec0:
                    break
                k += 1
            if k >= min_len:
                runs.append(
                    {
                        "section": s["name"],
                        "file_off": hex(s["rawptr"] + i),
                        "va": hex(ib + s["va"] + i),
                        "count": k,
                        "target_section": sec0,
                        "first": hex(va0),
                        "last": hex(u64(blob, i + (k - 1) * 8)),
                    }
                )
                i += k * 8
            else:
                i += 8
    runs.sort(key=lambda r: -r["count"])
    return runs[:30]


def scan_movzx_r12(data):
    # 41 0F B6 84 24 disp32  movzx eax, byte [r12+disp]
    # 41 0F B6 8C 24 disp32  movzx ecx, byte [r12+disp]
    # 41 0F B6 94 24         edx
    prefixes = {
        "eax": bytes.fromhex("410FB68424"),
        "ecx": bytes.fromhex("410FB68C24"),
        "edx": bytes.fromhex("410FB69424"),
        "eax_word": bytes.fromhex("410FB78424"),
        "ecx_word": bytes.fromhex("410FB78C24"),
    }
    out = {}
    for name, pref in prefixes.items():
        hits, ex = r12_disp_hits(data, pref)
        if hits:
            out[name] = {
                "count": sum(hits.values()),
                "disps": {hex(k): v for k, v in hits.most_common(12)},
                "example_offs": {hex(k): [hex(x) for x in ex[k]] for k, _ in hits.most_common(6)},
            }
    return out


def scan_jmp_tables(data):
    pats = {
        "jmp_abs_rax8": bytes.fromhex("FF24C5"),  # jmp [rax*8 + abs32]
        "jmp_r12_rax8": bytes.fromhex("41FF24C4"),
        "jmp_rcx_rax8": bytes.fromhex("FF24C1"),
        "rex_jmp_rcx_rax8": bytes.fromhex("48FF24C1"),
        "jmp_rdx_rax8": bytes.fromhex("FF24C2"),
    }
    out = {}
    for name, p in pats.items():
        hits = find_all(data, p)
        out[name] = {"count": len(hits), "sample_off": [hex(h) for h in hits[:8]]}
    return out


def pdata_starts(pe):
    if len(pe["dirs"]) < 4 or not pe["dirs"][3]["rva"]:
        return set(), {}
    off, _ = rva_to_off(pe, pe["dirs"][3]["rva"])
    if off is None:
        return set(), {}
    blob = pe["data"][off : off + pe["dirs"][3]["size"]]
    starts = set()
    by_sec = Counter()
    ib = pe["image_base"]
    for i in range(len(blob) // 12):
        begin = u32(blob, i * 12)
        starts.add(ib + begin)
        _, s = rva_to_off(pe, begin)
        by_sec[s["name"] if s else "?"] += 1
    return starts, dict(by_sec)


def map_image(name, pe, meta):
    data = pe["data"]
    ib = pe["image_base"]
    assert pe["sha256"] == meta["sha"], (name, pe["sha256"], meta["sha"])

    stub_offs = find_all(data, STUB)
    add_offs = find_all(data, ADD_108)
    stubs = []
    for off in stub_offs:
        st = parse_cookie_stub(data, off)
        if st:
            va, sec = off_to_va(pe, st["file_off"])
            st["va"] = hex(va) if va else None
            st["section"] = sec["name"] if sec else None
            stubs.append(st)

    cookies = Counter(s["cookie"] for s in stubs)
    addends = Counter(s["addend"] for s in stubs)
    sec_counts = Counter(s["section"] for s in stubs)

    reloc_hits = []
    for off in find_all(data, bytes.fromhex("48C7C07D54C8FF")):
        va, sec = off_to_va(pe, off)
        reloc_hits.append({"off": hex(off), "va": hex(va) if va else None, "section": sec["name"] if sec else None})
    # generic `mov rax, imm32; ... lea rbx,[rbx+rax]; mov [rsp+0x128], rbx`
    reloc_generic = []
    for off in find_all(data, bytes.fromhex("48899C2428010000")):  # mov [rsp+0x128], rbx
        va, sec = off_to_va(pe, off)
        reloc_generic.append(
            {
                "off": hex(off),
                "va": hex(va) if va else None,
                "section": sec["name"] if sec else None,
                "pre16": data[max(0, off - 16) : off].hex(),
            }
        )

    lea_r12 = []
    for off in find_all(data, LEA_R12_RSP8):
        va, sec = off_to_va(pe, off)
        lea_r12.append({"off": hex(off), "va": hex(va) if va else None, "section": sec["name"] if sec else None})

    # r12 stores/adds with any disp32
    store_hits, store_ex = r12_disp_hits(data, bytes.fromhex("49898424"))
    add_hits, add_ex = r12_disp_hits(data, bytes.fromhex("49038424"))

    # reloc base from known EOS sequence if present
    vm_base = None
    reloc_const = None
    if name == "eos":
        reloc_const = -0x37AB83
        vm_base = 0x1403810EA + reloc_const  # 0x140006567

    pdata, pdata_sec = pdata_starts(pe)
    handlers = []
    prefix_hist = Counter()
    pdata_hit = 0
    for s in stubs:
        if vm_base is None:
            h_va = None
        else:
            h_va = (vm_base + s["addend"]) & 0xFFFFFFFFFFFFFFFF
        rec = {
            "stub_va": s["va"],
            "section": s["section"],
            "cookie": hex(s["cookie"]),
            "addend": hex(s["addend"]),
            "handler_va": hex(h_va) if h_va else None,
        }
        if h_va:
            blob, hsec = read_va(pe, h_va, 32)
            rec["handler_section"] = hsec["name"] if hsec else None
            rec["handler_prefix"] = blob.hex() if blob else None
            kind = classify_prefix(blob or b"")
            rec["handler_kind"] = kind
            prefix_hist[kind] += 1
            if h_va in pdata:
                rec["pdata"] = True
                pdata_hit += 1
        handlers.append(rec)

    # classify known FLT / entry stubs
    labeled = {}
    want = {}
    if name == "eos":
        want = {
            "driverentry_dispatch": 0x140123B1E,
            "unload_text_jmp": 0x14023E209,
            "create_text_jmp": 0x14023E237,
            "section_text_jmp": 0x14023E269,
        }
    for lab, va in want.items():
        b, _ = read_va(pe, va, 40)
        labeled[lab] = b.hex() if b else None

    # follow 5-byte jmp at FLT callbacks
    jmp_follow = {}
    for lab, va in list(want.items()):
        b, _ = read_va(pe, va, 5)
        if b and b[0] == 0xE9:
            rel = i32(b, 1)
            tgt = va + 5 + rel
            jmp_follow[lab] = hex(tgt)

    pointer_runs = find_pointer_runs(pe, min_len=32)

    # sub rsp, 0x108 VM prolog
    prologs = []
    for off in find_all(data, SUB_RSP_108):
        va, sec = off_to_va(pe, off)
        prologs.append({"off": hex(off), "va": hex(va) if va else None, "section": sec["name"] if sec else None})

    # cyclic E9 F6 FF FF FF
    cyclic = find_all(data, bytes.fromhex("E9F6FFFFFF"))

    # sample unique handlers (first 12 unique addends)
    unique_handlers = []
    seen_add = set()
    for h in handlers:
        if h["addend"] in seen_add:
            continue
        seen_add.add(h["addend"])
        unique_handlers.append(h)
        if len(unique_handlers) >= 16:
            break

    ctx = {
        "r12_plus_0x190_cookie_slot": {
            "encoding": "mov [r12+0x190], rax",
            "hits_exact_stub_store": len(stub_offs),
            "parsed_complete_stubs": len(stubs),
        },
        "r12_plus_0x108_dispatch_base": {
            "encoding": "add rax, [r12+0x108]; jmp rax",
            "add_108_hits": len(add_offs),
        },
        "r12_store_disp32_top": {hex(k): v for k, v in store_hits.most_common(15)},
        "r12_add_disp32_top": {hex(k): v for k, v in add_hits.most_common(15)},
        "lea_r12_rsp8_count": len(lea_r12),
        "lea_r12_rsp8_sample": lea_r12[:12],
        "reloc_eos_const_hits": reloc_hits,
        "mov_rsp_0x128_rbx": reloc_generic[:12],
        "vm_base_va": hex(vm_base) if vm_base else None,
        "reloc_const": hex(reloc_const & 0xFFFFFFFFFFFFFFFF) if reloc_const is not None else None,
        "sub_rsp_108_count": len(prologs),
        "sub_rsp_108_sample": prologs[:12],
        "cyclic_e9_f6_count": len(cyclic),
        "cyclic_e9_f6_sample_off": [hex(x) for x in cyclic[:8]],
    }

    return {
        "name": name,
        "sha256": pe["sha256"],
        "image_base": hex(ib),
        "entry_rva": hex(pe["entry_rva"]),
        "sections": [
            {
                "name": s["name"],
                "va": hex(ib + s["va"]),
                "vsize": s["vsize"],
                "rawsz": s["rawsz"],
                "exec": s["exec"],
            }
            for s in pe["sections"]
        ],
        "pdata_section_counts": pdata_sec,
        "stub_section_counts": dict(sec_counts),
        "complete_stubs": len(stubs),
        "unique_cookies": len(cookies),
        "unique_addends": len(addends),
        "top_addends": {hex(k): v for k, v in addends.most_common(10)},
        "handler_prefix_hist": dict(prefix_hist),
        "handlers_with_pdata": pdata_hit,
        "labeled_entry_bytes": labeled,
        "flt_jmp_follow": jmp_follow,
        "context": ctx,
        "movzx_r12": scan_movzx_r12(data),
        "jmp_tables": scan_jmp_tables(data),
        "pointer_runs": pointer_runs,
        "unique_handler_sample": unique_handlers,
        "known_stubs": [
            h
            for h in handlers
            if h["stub_va"]
            in {
                "0x140123b1e",
                "0x1403dae9e",
                "0x14041e353",
                "0x1403eeaed",
            }
            or (h["stub_va"] and int(h["stub_va"], 16) in {0x140123B1E, 0x1403DAE9E, 0x14041E353, 0x1403EEAED})
        ],
    }


def dump_va_window(pe, va, n, label):
    b, s = read_va(pe, va, n)
    return {
        "label": label,
        "va": hex(va),
        "section": s["name"] if s else None,
        "n": n,
        "hex": b.hex() if b else None,
        "ascii": "".join(chr(x) if 32 <= x < 127 else "." for x in (b or b"")),
    }


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    os.makedirs(WT_OUT, exist_ok=True)
    report = {
        "when": "2026-11-18-vm-interp",
        "scope": "static only; drivers not loaded; share Z:; no bypass",
        "images": {},
        "windows": [],
    }
    pes = {}
    for name, meta in KNOWN.items():
        pe = parse_pe(meta["path"])
        pes[name] = pe
        report["images"][name] = map_image(name, pe, meta)

    eos = pes["eos"]
    be = pes["bedaisy"]
    leg = pes["legacy"]

    # EOS: save-frame, reloc, dispatch, computed handler, FLT landings
    eos_vm_base = 0x140006567
    windows = [
        dump_va_window(eos, 0x140381041, 0xB0, "eos_FUN_140381041_save_frame"),
        dump_va_window(eos, 0x14038114F, 0x30, "eos_FUN_14038114f_reloc"),
        dump_va_window(eos, 0x140123B1E, 0x30, "eos_FUN_140123b1e_dispatch"),
        dump_va_window(eos, eos_vm_base + 0x41DCAA, 0x80, "eos_handler_driverentry"),
        dump_va_window(eos, eos_vm_base + 0x448D15, 0x80, "eos_handler_unload"),
        dump_va_window(eos, eos_vm_base + 0x3C903B, 0x80, "eos_handler_create"),
        dump_va_window(eos, eos_vm_base + 0x464925, 0x80, "eos_handler_section_sync"),
        dump_va_window(leg, 0x14071C246, 0x80, "legacy_section6_vm_prolog"),
        dump_va_window(be, 0x1425CCEA8, 0x40, "bedaisy_pe_entry"),
        dump_va_window(be, 0x14002F000, 0x50, "bedaisy_init"),
        dump_va_window(be, 0x1400157C8, 0x80, "bedaisy_opaque_157c8"),
        dump_va_window(be, 0x1417C4BA1, 0x100, "bedaisy_section6_land"),
        dump_va_window(be, 0x14002F02C, 0x40, "bedaisy_security_init_cookie"),
    ]
    report["windows"] = windows

    # Compare stub presence across images
    report["comparison"] = {
        "eos_complete_stubs": report["images"]["eos"]["complete_stubs"],
        "legacy_complete_stubs": report["images"]["legacy"]["complete_stubs"],
        "bedaisy_complete_stubs": report["images"]["bedaisy"]["complete_stubs"],
        "eos_unique_cookies": report["images"]["eos"]["unique_cookies"],
        "bedaisy_r12_store_top": report["images"]["bedaisy"]["context"]["r12_store_disp32_top"],
        "bedaisy_r12_add_top": report["images"]["bedaisy"]["context"]["r12_add_disp32_top"],
        "bedaisy_lea_r12": report["images"]["bedaisy"]["context"]["lea_r12_rsp8_count"],
        "legacy_lea_r12": report["images"]["legacy"]["context"]["lea_r12_rsp8_count"],
        "legacy_sub_rsp_108": report["images"]["legacy"]["context"]["sub_rsp_108_count"],
        "bedaisy_sub_rsp_108": report["images"]["bedaisy"]["context"]["sub_rsp_108_count"],
    }

    text = json.dumps(report, indent=2)
    for dest in (
        os.path.join(OUT_DIR, "vm-interpreter-map.json"),
        os.path.join(WT_OUT, "vm-interpreter-map.json"),
    ):
        with open(dest, "w", encoding="utf-8") as f:
            f.write(text)
        print("wrote", dest, "bytes", len(text))

    # compact stdout for the operator
    for name in ("eos", "legacy", "bedaisy"):
        im = report["images"][name]
        print(
            f"{name}: stubs={im['complete_stubs']} cookies={im['unique_cookies']} "
            f"addends={im['unique_addends']} lea_r12={im['context']['lea_r12_rsp8_count']} "
            f"sub108={im['context']['sub_rsp_108_count']} cyclic={im['context']['cyclic_e9_f6_count']}"
        )
        print("  prefix_hist", im["handler_prefix_hist"])
        print("  r12_store", im["context"]["r12_store_disp32_top"])
        print("  r12_add", im["context"]["r12_add_disp32_top"])
        print("  movzx", {k: v["count"] for k, v in im["movzx_r12"].items()})
        print("  jmp_tables", {k: v["count"] for k, v in im["jmp_tables"].items()})
        print("  pointer_runs", im["pointer_runs"][:5])


if __name__ == "__main__":
    main()
