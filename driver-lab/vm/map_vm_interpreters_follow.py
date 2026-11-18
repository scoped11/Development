#!/usr/bin/env python3
"""Follow EOS jmp trampolines and dump BEDaisy candidate handler table. Static only."""
from __future__ import annotations

import json
import os
import struct
from collections import Counter, defaultdict

from map_vm_interpreters import (
    KNOWN,
    OUT_DIR,
    WT_OUT,
    ADD_108,
    JMP_RAX,
    MOVABS_RAX,
    STUB,
    classify_prefix,
    dump_va_window,
    find_all,
    i32,
    off_to_va,
    parse_cookie_stub,
    parse_pe,
    read_va,
    u32,
    u64,
    va_to_off,
)

LEA_VARIANTS = {
    "r12": bytes.fromhex("4C8D642408"),
    "r13": bytes.fromhex("4C8D6C2408"),
    "r14": bytes.fromhex("4C8D742408"),
    "r15": bytes.fromhex("4C8D7C2408"),
    "rbp": bytes.fromhex("488D6C2408"),
    "rbx": bytes.fromhex("488D5C2408"),
    "rsi": bytes.fromhex("488D742408"),
    "rdi": bytes.fromhex("488D7C2408"),
}

# rip-relative lea r12, [rip+disp32] = 4C 8D 25
LEA_RIP = {
    "r12": bytes.fromhex("4C8D25"),
    "r13": bytes.fromhex("4C8D2D"),
    "r14": bytes.fromhex("4C8D35"),
    "r15": bytes.fromhex("4C8D3D"),
}


def follow_jmp_chain(pe, va, max_hops=8):
    hops = []
    seen = set()
    cur = va
    for _ in range(max_hops):
        if cur in seen:
            hops.append({"va": hex(cur), "kind": "cycle"})
            return hops, cur, "cycle"
        seen.add(cur)
        b, s = read_va(pe, cur, 16)
        if not b:
            hops.append({"va": hex(cur), "kind": "unmapped"})
            return hops, cur, "unmapped"
        kind = classify_prefix(b)
        hops.append({"va": hex(cur), "kind": kind, "prefix": b[:8].hex(), "section": s["name"] if s else None})
        if b[0] == 0xE9 and len(b) >= 5:
            cur = (cur + 5 + i32(b, 1)) & 0xFFFFFFFFFFFFFFFF
            continue
        if b[0] == 0xEB and len(b) >= 2:
            rel = struct.unpack_from("<b", b, 1)[0]
            cur = (cur + 2 + rel) & 0xFFFFFFFFFFFFFFFF
            continue
        if b[0] == 0xE8 and len(b) >= 5:
            # call then likely not a trampoline; stop
            return hops, cur, kind
        return hops, cur, kind
    return hops, cur, "max_hops"


def parse_all_stubs(pe):
    stubs = []
    for off in find_all(pe["data"], STUB):
        st = parse_cookie_stub(pe["data"], off)
        if not st:
            continue
        va, sec = off_to_va(pe, st["file_off"])
        st["va"] = va
        st["section"] = sec["name"] if sec else None
        stubs.append(st)
    return stubs


def dump_table(pe, va, count):
    rows = []
    for i in range(count):
        slot = va + i * 8
        b, _ = read_va(pe, slot, 8)
        if not b or len(b) < 8:
            rows.append({"i": i, "slot": hex(slot), "ptr": None})
            continue
        ptr = u64(b, 0)
        blob, s = read_va(pe, ptr, 24)
        rows.append(
            {
                "i": i,
                "slot": hex(slot),
                "ptr": hex(ptr),
                "section": s["name"] if s else None,
                "kind": classify_prefix(blob or b""),
                "prefix": blob.hex() if blob else None,
            }
        )
    return rows


def scan_reg_context(pe):
    data = pe["data"]
    out = {"lea_rsp8": {}, "lea_rip": {}, "sib_disp32_store": {}, "sib_disp32_add": {}}
    for name, pat in LEA_VARIANTS.items():
        hits = find_all(data, pat)
        out["lea_rsp8"][name] = {
            "count": len(hits),
            "sample_va": [hex(off_to_va(pe, h)[0]) for h in hits[:6] if off_to_va(pe, h)[0]],
        }
    for name, pat in LEA_RIP.items():
        hits = find_all(data, pat)
        out["lea_rip"][name] = {"count": len(hits), "sample_off": [hex(h) for h in hits[:6]]}

    # 49 89 84 24 = mov [r12+disp32], rax
    # 4D 89 84 24 = mov [r12+disp32], r8
    # 49 89 85 xx = mov [r13+disp32], rax  (ModRM 85 = [r13+disp32] with REX.B)
    sib_pats = {
        "r12_rax": bytes.fromhex("49898424"),
        "r12_r8": bytes.fromhex("4D898424"),
        "r13_rax_modrm85": bytes.fromhex("498985"),  # disp32 follows, no SIB
        "rbp_rax_modrm85": bytes.fromhex("488985"),
        "r14_rax_sib": bytes.fromhex("49898426"),  # SIB with r14? actually 24 is r12, 26 might be other
        "r13_rax_sib": bytes.fromhex("49898425"),  # SIB r13? 25 = disp32 rip-relative actually
    }
    for name, pat in sib_pats.items():
        hits = find_all(data, pat)
        out["sib_disp32_store"][name] = {"count": len(hits), "sample_off": [hex(h) for h in hits[:4]]}

    add_pats = {
        "r12_rax": bytes.fromhex("49038424"),
        "r13_rax_modrm85": bytes.fromhex("490385"),
        "rbp_rax_modrm85": bytes.fromhex("480385"),
    }
    for name, pat in add_pats.items():
        hits = find_all(data, pat)
        out["sib_disp32_add"][name] = {"count": len(hits), "sample_off": [hex(h) for h in hits[:4]]}
    return out


def movzx_examples(pe, limit=12):
    data = pe["data"]
    pref = bytes.fromhex("410FB68424")  # movzx eax, byte [r12+disp32]
    rows = []
    for off in find_all(data, pref)[:limit]:
        disp = u32(data, off + 5)
        va, sec = off_to_va(pe, off)
        ctx = data[off : off + 24]
        rows.append(
            {
                "va": hex(va) if va else None,
                "section": sec["name"] if sec else None,
                "disp": hex(disp),
                "bytes": ctx.hex(),
            }
        )
    return rows


def jmp_abs_sites(pe):
    """FF 24 C5 abs32  => jmp qword [rax*8 + abs]"""
    data = pe["data"]
    rows = []
    for off in find_all(data, bytes.fromhex("FF24C5")):
        abs32 = u32(data, off + 3)
        va, sec = off_to_va(pe, off)
        rows.append(
            {
                "va": hex(va) if va else None,
                "section": sec["name"] if sec else None,
                "table_abs32": hex(abs32),
                "pre": data[max(0, off - 12) : off].hex(),
            }
        )
    return rows


def main():
    eos = parse_pe(KNOWN["eos"]["path"])
    be = parse_pe(KNOWN["bedaisy"]["path"])
    leg = parse_pe(KNOWN["legacy"]["path"])
    vm_base = 0x140006567

    stubs = parse_all_stubs(eos)
    by_addend = defaultdict(list)
    for s in stubs:
        by_addend[s["addend"]].append(s["va"])

    final_kinds = Counter()
    final_sec = Counter()
    hop1_kinds = Counter()
    samples = []
    # one stub per unique addend
    for addend, vas in list(by_addend.items()):
        h_va = (vm_base + addend) & 0xFFFFFFFFFFFFFFFF
        hops, final, kind = follow_jmp_chain(eos, h_va)
        hop1_kinds[hops[0]["kind"] if hops else "none"] += 1
        final_kinds[kind] += 1
        if hops:
            final_sec[hops[-1].get("section") or "?"] += 1
        if len(samples) < 20:
            samples.append(
                {
                    "addend": hex(addend),
                    "stub_count": len(vas),
                    "stub0": hex(vas[0]),
                    "hops": hops,
                    "final_kind": kind,
                }
            )

    # shared addends (many cookies → one handler)
    shared = sorted(((len(v), k) for k, v in by_addend.items()), reverse=True)[:15]

    # DriverEntry / FLT known addends
    known_addends = {
        "driverentry": 0x41DCAA,
        "unload": 0x448D15,
        "create": 0x3C903B,
        "section_sync": 0x464925,
    }
    known_follow = {}
    for lab, addend in known_addends.items():
        hops, final, kind = follow_jmp_chain(eos, vm_base + addend, max_hops=12)
        blob, s = read_va(eos, final, 64)
        known_follow[lab] = {
            "addend": hex(addend),
            "final": hex(final),
            "final_kind": kind,
            "final_section": s["name"] if s else None,
            "hops": hops,
            "final_hex": blob.hex() if blob else None,
        }

    lea_eos = []
    for off in find_all(eos["data"], bytes.fromhex("4C8D642408")):
        va, sec = off_to_va(eos, off)
        blob, _ = read_va(eos, va - 8 if va else 0, 24)
        lea_eos.append(
            {
                "va": hex(va) if va else None,
                "section": sec["name"] if sec else None,
                "window": (eos["data"][off - 8 : off + 16].hex() if off >= 8 else None),
            }
        )

    be_table = dump_table(be, 0x140027490, 50)
    be_kinds = Counter(r["kind"] for r in be_table)

    # BEDaisy section6 land follow
    be_land_hops, be_land_final, be_land_kind = follow_jmp_chain(be, 0x1417C4BA1, max_hops=16)
    be_entry_hops, be_entry_final, be_entry_kind = follow_jmp_chain(be, 0x1425CCEA8, max_hops=16)
    be_init_hops, be_init_final, be_init_kind = follow_jmp_chain(be, 0x14002F000, max_hops=8)

    # cyclic thunk region bounds
    cyclic_offs = find_all(be["data"], bytes.fromhex("E9F6FFFFFF"))
    cyclic_vas = []
    for off in (cyclic_offs[0], cyclic_offs[-1]) if cyclic_offs else []:
        va, sec = off_to_va(be, off)
        cyclic_vas.append({"off": hex(off), "va": hex(va) if va else None, "section": sec["name"] if sec else None})

    report = {
        "eos": {
            "stubs": len(stubs),
            "unique_addends": len(by_addend),
            "top_shared_addends": [{"count": n, "addend": hex(a)} for n, a in shared],
            "hop1_kinds": dict(hop1_kinds),
            "final_kinds": dict(final_kinds),
            "final_sections": dict(final_sec),
            "sample_chains": samples,
            "known_follow": known_follow,
            "lea_r12_rsp8": lea_eos,
            "movzx_eax_r12_examples": movzx_examples(eos, 15),
            "reg_context": scan_reg_context(eos),
        },
        "legacy": {
            "reg_context": scan_reg_context(leg),
            "jmp_abs_rax8": jmp_abs_sites(leg),
        },
        "bedaisy": {
            "reg_context": scan_reg_context(be),
            "handler_table_0x140027490": {
                "count": len(be_table),
                "kind_hist": dict(be_kinds),
                "rows": be_table,
            },
            "section6_land": {"hops": be_land_hops, "final": hex(be_land_final), "kind": be_land_kind},
            "pe_entry": {"hops": be_entry_hops, "final": hex(be_entry_final), "kind": be_entry_kind},
            "init": {"hops": be_init_hops, "final": hex(be_init_final), "kind": be_init_kind},
            "cyclic_region": {
                "count": len(cyclic_offs),
                "span": cyclic_vas,
            },
            "jmp_abs_rax8": jmp_abs_sites(be),
            "windows": [
                dump_va_window(be, 0x1417C4BA1, 0x80, "section6_land"),
                dump_va_window(be, 0x140019814, 0x40, "table_handler0"),
                dump_va_window(be, 0x140027490, 0x40, "table_bytes"),
            ],
        },
    }

    text = json.dumps(report, indent=2)
    for dest_dir in (OUT_DIR, WT_OUT):
        os.makedirs(dest_dir, exist_ok=True)
        dest = os.path.join(dest_dir, "vm-interpreter-follow.json")
        with open(dest, "w", encoding="utf-8") as f:
            f.write(text)
        print("wrote", dest)

    print("EOS hop1", dict(hop1_kinds))
    print("EOS final", dict(final_kinds))
    print("EOS final sec", dict(final_sec))
    print("EOS top shared", report["eos"]["top_shared_addends"][:8])
    for lab, rec in known_follow.items():
        print(lab, rec["final"], rec["final_kind"], rec["final_hex"][:40] if rec["final_hex"] else None)
    print("BE table kinds", dict(be_kinds))
    print("BE land", be_land_kind, hex(be_land_final), be_land_hops[:6])
    print("BE entry", be_entry_kind, hex(be_entry_final), be_entry_hops[:8])
    print("BE lea_rsp8", report["bedaisy"]["reg_context"]["lea_rsp8"])
    print("BE stores", report["bedaisy"]["reg_context"]["sib_disp32_store"])
    print("legacy jmp_abs", report["legacy"]["jmp_abs_rax8"])
    print("legacy lea", report["legacy"]["reg_context"]["lea_rsp8"])


if __name__ == "__main__":
    main()
