#!/usr/bin/env python3
"""Static lift of EOS cookie-handler bodies and BEDaisy jmp rdx.

Maps PE file bytes only. Does not load drivers. No bypass.
"""
from __future__ import annotations

import json
import os
import struct
from collections import Counter, defaultdict

from capstone import CS_ARCH_X86, CS_MODE_64, Cs
from unicorn import Uc, UcError
from unicorn.unicorn_const import (
    UC_ARCH_X86,
    UC_HOOK_CODE,
    UC_HOOK_MEM_UNMAPPED,
    UC_MEM_READ,
    UC_MEM_FETCH,
    UC_MODE_64,
    UC_PROT_ALL,
)
from unicorn.x86_const import (
    UC_X86_REG_CR0,
    UC_X86_REG_CR4,
    UC_X86_REG_CS,
    UC_X86_REG_DS,
    UC_X86_REG_ES,
    UC_X86_REG_SS,
    UC_X86_REG_EFLAGS,
    UC_X86_REG_R8,
    UC_X86_REG_R9,
    UC_X86_REG_R10,
    UC_X86_REG_R11,
    UC_X86_REG_R12,
    UC_X86_REG_R13,
    UC_X86_REG_R14,
    UC_X86_REG_R15,
    UC_X86_REG_RAX,
    UC_X86_REG_RBP,
    UC_X86_REG_RBX,
    UC_X86_REG_RCX,
    UC_X86_REG_RDI,
    UC_X86_REG_RDX,
    UC_X86_REG_RIP,
    UC_X86_REG_RSI,
    UC_X86_REG_RSP,
    UC_X86_REG_XMM6,
    UC_X86_REG_XMM7,
)

from map_vm_interpreters import (
    ADD_108,
    JMP_RAX,
    KNOWN,
    MOVABS_RAX,
    STUB,
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

try:
    from map_vm_interpreters_follow import follow_jmp_chain as _follow
except Exception:
    _follow = None

WT_ROOT = r"C:\Users\bj284\Downloads\TraeTesting\cursor\Development-wt-lift-handlers-adce"
OUT_DIR = os.path.join(WT_ROOT, r"ac-captures-host\2026-11-18")
DEV_OUT = r"C:\Users\bj284\Downloads\TraeTesting\cursor\Development\ac-captures-host\2026-11-18"

VM_BASE = 0x140006567
PAGE = 0x1000
MD = Cs(CS_ARCH_X86, CS_MODE_64)
MD.detail = True

KNOWN_EOS = {
    "DriverEntry": {"addend": 0x41DCAA, "body": 0x141B1DF86, "cookie": 0x561C7F4B48E431A5},
    "FilterUnload": {"addend": 0x448D15, "body": 0x1421C00E0, "cookie": 0x72B509B8134A12D7},
    "PreCreate_SetInfo": {"addend": 0x3C903B, "body": 0x140E85F02, "cookie": 0x6B2AF190DD52023F},
    "PreSectionSync": {"addend": 0x464925, "body": 0x142618C9E, "cookie": 0x6A382AE158204CFB},
}

REG64 = {
    "rax": UC_X86_REG_RAX,
    "rbx": UC_X86_REG_RBX,
    "rcx": UC_X86_REG_RCX,
    "rdx": UC_X86_REG_RDX,
    "rsi": UC_X86_REG_RSI,
    "rdi": UC_X86_REG_RDI,
    "rbp": UC_X86_REG_RBP,
    "rsp": UC_X86_REG_RSP,
    "r8": UC_X86_REG_R8,
    "r9": UC_X86_REG_R9,
    "r10": UC_X86_REG_R10,
    "r11": UC_X86_REG_R11,
    "r12": UC_X86_REG_R12,
    "r13": UC_X86_REG_R13,
    "r14": UC_X86_REG_R14,
    "r15": UC_X86_REG_R15,
}


def align_down(x, a=PAGE):
    return x & ~(a - 1)


def align_up(x, a=PAGE):
    return (x + a - 1) & ~(a - 1)


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


def follow_rel_jmps(pe, va, max_hops=8):
    if _follow:
        hops, final, kind = _follow(pe, va, max_hops=max_hops)
        return hops, final, kind
    hops = []
    seen = set()
    cur = va
    for _ in range(max_hops):
        if cur in seen:
            return hops, cur, "cycle"
        seen.add(cur)
        b, s = read_va(pe, cur, 16)
        if not b:
            return hops, cur, "unmapped"
        hops.append({"va": hex(cur), "prefix": b[:8].hex(), "section": s["name"] if s else None})
        if b[0] == 0xE9 and len(b) >= 5:
            cur = (cur + 5 + i32(b, 1)) & 0xFFFFFFFFFFFFFFFF
            continue
        if b[0] == 0xEB and len(b) >= 2:
            rel = struct.unpack_from("<b", b, 1)[0]
            cur = (cur + 2 + rel) & 0xFFFFFFFFFFFFFFFF
            continue
        return hops, cur, "body"
    return hops, cur, "max_hops"


def is_r12_disp(insn, disp):
    if insn.mnemonic != "mov":
        return False
    ops = insn.op_str.replace(" ", "")
    return f"[r12+{disp:#x}]" in ops or f"[r12 + {disp:#x}]" in insn.op_str.replace(" ", "")


def insn_reads_r12(insn, disp):
    s = insn.op_str.replace(" ", "").lower()
    want = f"[r12+{disp:#x}]"
    alt = f"[r12+0x{disp:x}]"
    return want in s or alt in s or f"[r12+{disp}]" in s


def dst_reg(insn):
    if not insn.op_str:
        return None
    left = insn.op_str.split(",", 1)[0].strip().lower()
    if left.startswith("r") or left in ("eax", "ebx", "ecx", "edx", "esi", "edi", "ebp", "esp"):
        return left
    return None


def src_reg(insn):
    if "," not in insn.op_str:
        return None
    right = insn.op_str.split(",", 1)[1].strip().lower()
    if right.startswith("[") or right.startswith("0x") or right[0].isdigit():
        return None
    return right.split()[0]


def imm_of(insn):
    for p in insn.op_str.replace(",", " ").split():
        if p.lower().startswith("0x"):
            try:
                return int(p, 16)
            except ValueError:
                continue
    return None


def classify_post_cookie(pe, body_va, cookie, window=0x300):
    blob, sec = read_va(pe, body_va, window)
    if not blob:
        return {"error": "unmapped", "body": hex(body_va)}
    insns = list(MD.disasm(blob, body_va))
    cookie_reg = None
    last_imm = {}
    xor_decrypted = []
    cmps = []
    r12_reads = Counter()
    nested = False
    jmp_reg = []
    calls = []
    cookie_load_i = None
    first_xor_i = None
    post = []
    for i, insn in enumerate(insns):
        s = insn.op_str.replace(" ", "").lower()
        if "r12+" in s or "[r12]" in s:
            # pull disp if present
            if "0x190" in s:
                r12_reads[0x190] += 1
                if insn.mnemonic == "mov" and "[" in insn.op_str.split(",", 1)[-1]:
                    cookie_reg = dst_reg(insn)
                    if cookie_load_i is None:
                        cookie_load_i = i
            elif "0x108" in s:
                r12_reads[0x108] += 1
            else:
                # try parse +0xNN
                for tok in s.replace("[", " ").replace("]", " ").replace("+", " ").split():
                    if tok.startswith("0x"):
                        try:
                            r12_reads[int(tok, 16)] += 1
                        except ValueError:
                            pass
        if insn.mnemonic in ("movabs", "mov") and insn.op_str.lower().startswith(("rax,", "rbx,", "rcx,", "rdx,", "rsi,", "rdi,", "r8,", "r9,", "r10,", "r11,", "r12,", "r13,", "r14,", "r15,")):
            if "[" not in insn.op_str:
                imm = imm_of(insn)
                d = dst_reg(insn)
                if d and imm is not None and insn.size >= 5:
                    last_imm[d] = imm
        if insn.mnemonic == "xor" and cookie_reg:
            d = dst_reg(insn)
            src = src_reg(insn)
            if src == cookie_reg and d in last_imm:
                dec = last_imm[d] ^ cookie
                xor_decrypted.append(
                    {
                        "at": hex(insn.address),
                        "dst": d,
                        "imm": hex(last_imm[d]),
                        "decrypted": hex(dec),
                        "in_image": in_image(pe, dec),
                        "plus_vm_base": hex((VM_BASE + (dec & 0xFFFFFFFF)) & 0xFFFFFFFFFFFFFFFF),
                        "plus_vm_base_in_image": in_image(pe, (VM_BASE + (dec & 0xFFFFFFFF)) & 0xFFFFFFFFFFFFFFFF),
                    }
                )
                if first_xor_i is None:
                    first_xor_i = i
            elif d == cookie_reg and src and src in last_imm:
                dec = last_imm[src] ^ cookie
                xor_decrypted.append(
                    {
                        "at": hex(insn.address),
                        "dst": d,
                        "imm": hex(last_imm[src]),
                        "decrypted": hex(dec),
                        "in_image": in_image(pe, dec),
                        "plus_vm_base": hex((VM_BASE + (dec & 0xFFFFFFFF)) & 0xFFFFFFFFFFFFFFFF),
                        "plus_vm_base_in_image": in_image(pe, (VM_BASE + (dec & 0xFFFFFFFF)) & 0xFFFFFFFFFFFFFFFF),
                    }
                )
                if first_xor_i is None:
                    first_xor_i = i
        if insn.mnemonic == "cmp" and cookie_reg:
            d = dst_reg(insn)
            src = src_reg(insn)
            imm = imm_of(insn)
            if imm is None and src in last_imm:
                imm = last_imm[src]
            if imm is None and d in last_imm and src == cookie_reg:
                imm = last_imm[d]
            if imm is not None and (d == cookie_reg or src == cookie_reg):
                c = cookie
                cmps.append(
                    {
                        "at": hex(insn.address),
                        "imm": hex(imm),
                        "cookie_lt": c < imm,
                        "cookie_eq": c == imm,
                        "cookie_gt": c > imm,
                        "setb": int(c < imm),
                        "seta": int(c > imm),
                        "sete": int(c == imm),
                    }
                )
        if insn.mnemonic == "call":
            calls.append({"at": hex(insn.address), "op": insn.op_str, "bytes": insn.bytes.hex()})
        if insn.mnemonic == "jmp" and insn.op_str.lower() in REG64:
            jmp_reg.append({"at": hex(insn.address), "reg": insn.op_str.lower()})
        if insn.bytes[:2] == MOVABS_RAX and STUB in blob[insn.address - body_va : insn.address - body_va + 33]:
            nested = True

    window_imms = [v for v in last_imm.values()]
    for rec in xor_decrypted:
        dec = int(rec["decrypted"], 16)
        wraps = []
        for imm in window_imms:
            s = (dec + imm) & 0xFFFFFFFFFFFFFFFF
            if s == 0:
                wraps.append({"plus": hex(imm), "sum": "0"})
            elif in_image(pe, s):
                wraps.append({"plus": hex(imm), "sum": hex(s), "in_image": True})
        rec["wraps"] = wraps[:6]

    if first_xor_i is not None:
        for insn in insns[first_xor_i : first_xor_i + 48]:
            post.append(f"{insn.address:#x}: {insn.mnemonic} {insn.op_str}")

    # detect nested cookie stub in window
    rel = body_va
    off = 0
    nested_stub = None
    window_off, _ = va_to_off(pe, body_va)
    if window_off is not None:
        chunk = pe["data"][window_off : window_off + window]
        for s_off in find_all(chunk, STUB):
            st = parse_cookie_stub(chunk, s_off)
            if st:
                nested_stub = {
                    "at": hex(body_va + st["file_off"]),
                    "cookie": hex(st["cookie"]),
                    "addend": hex(st["addend"]),
                    "next_handler": hex((VM_BASE + st["addend"]) & 0xFFFFFFFFFFFFFFFF),
                }
                nested = True
                break

    kinds = []
    if nested:
        kinds.append("nested_cookie_stub")
    if jmp_reg:
        kinds.append("jmp_reg")
    if calls:
        kinds.append("call")
    if xor_decrypted:
        kinds.append("cookie_xor")
    if cmps:
        kinds.append("cookie_cmp")
    if not kinds:
        kinds.append("no_cookie_ops_in_window")

    return {
        "body": hex(body_va),
        "section": sec["name"] if sec else None,
        "cookie": hex(cookie),
        "cookie_reg": cookie_reg,
        "cookie_load_i": cookie_load_i,
        "xor_decrypted": xor_decrypted[:12],
        "cmps": cmps[:12],
        "r12_reads_top": {hex(k): v for k, v in r12_reads.most_common(12)},
        "nested_stub": nested_stub,
        "jmp_reg": jmp_reg[:8],
        "calls": calls[:8],
        "kinds": kinds,
        "post_xor_asm": post[:40],
        "insn_count_window": len(insns),
    }


def in_image(pe, va):
    if va is None:
        return False
    lo = pe["image_base"]
    hi = lo + max(s["va"] + max(s["vsize"], s["rawsz"]) for s in pe["sections"])
    return lo <= va < hi


def map_pe(uc, pe):
    """Map each PE section as one contiguous Unicorn region (not per-page)."""
    ib = pe["image_base"]
    mapped = []
    for s in pe["sections"]:
        va = ib + s["va"]
        start = align_down(va)
        size = align_up(max(s["vsize"], s["rawsz"], PAGE) + (va - start))
        try:
            uc.mem_map(start, size, UC_PROT_ALL)
            mapped.append((start, size))
        except UcError:
            # overlap with a previous section — ignore
            pass
        raw = pe["data"][s["rawptr"] : s["rawptr"] + s["rawsz"]]
        if raw:
            try:
                uc.mem_write(va, raw)
            except UcError:
                pass
    return mapped


def setup_cpu(uc, rsp, extra_cr0=None):
    # Do not write CS/DS/SS or CR0/CR4 — those #GP in this Unicorn build.
    uc.reg_write(UC_X86_REG_RSP, rsp)
    uc.reg_write(UC_X86_REG_RBP, rsp)
    uc.reg_write(UC_X86_REG_EFLAGS, 0x202)


def emu_trace(pe, start_va, regs, stack_va=0x0000000000080000, max_insns=2500, stop_outside=None):
    uc = Uc(UC_ARCH_X86, UC_MODE_64)
    map_pe(uc, pe)
    stack_base = align_down(stack_va - 0x8000)
    try:
        uc.mem_map(stack_base, 0x10000, UC_PROT_ALL)
    except UcError:
        pass
    # scratch / fake objects
    try:
        uc.mem_map(0x10000, 0x4000, UC_PROT_ALL)
    except UcError:
        pass
    setup_cpu(uc, stack_va)
    for name, val in regs.items():
        uc.reg_write(REG64[name], val)
    st = {
        "insns": 0,
        "jmp_rax": [],
        "jmp_rdx": [],
        "jmp_other": [],
        "calls": [],
        "unmapped": [],
        "trace": [],
        "stop": None,
        "last_rip": start_va,
    }
    dispatcher = stop_outside

    def on_mem(uc_, access, address, size, value, user):
        st["unmapped"].append({"access": int(access), "addr": hex(address), "rip": hex(uc_.reg_read(UC_X86_REG_RIP))})
        if len(st["unmapped"]) > 8:
            uc_.emu_stop()
        return False

    def on_code(uc_, address, size, user):
        st["insns"] += 1
        st["last_rip"] = address
        if st["insns"] > max_insns:
            st["stop"] = "max_insns"
            uc_.emu_stop()
            return
        try:
            raw = bytes(uc_.mem_read(address, size))
        except UcError:
            st["stop"] = "fetch_fail"
            uc_.emu_stop()
            return
        # jmp rax / rdx / r8..
        if raw == b"\xff\xe0":
            tgt = uc_.reg_read(UC_X86_REG_RAX)
            st["jmp_rax"].append(hex(tgt))
            if dispatcher and not (dispatcher[0] <= tgt < dispatcher[1]):
                st["stop"] = "jmp_rax_outside"
                uc_.emu_stop()
                return
        elif raw == b"\xff\xe2":
            tgt = uc_.reg_read(UC_X86_REG_RDX)
            st["jmp_rdx"].append(hex(tgt))
            if dispatcher and not (dispatcher[0] <= tgt < dispatcher[1]):
                st["stop"] = "jmp_rdx_outside"
                uc_.emu_stop()
                return
        elif raw[:1] == b"\xe8":
            rel = struct.unpack_from("<i", raw, 1)[0] if len(raw) >= 5 else 0
            st["calls"].append(hex((address + 5 + rel) & 0xFFFFFFFFFFFFFFFF))
        if len(st["trace"]) < 80:
            try:
                ins = next(MD.disasm(raw, address))
                st["trace"].append(f"{address:#x}: {ins.mnemonic} {ins.op_str}")
            except StopIteration:
                st["trace"].append(f"{address:#x}: {raw.hex()}")
        if raw == b"\xc3":
            st["stop"] = "ret"
            uc_.emu_stop()

    uc.hook_add(UC_HOOK_CODE, on_code)
    uc.hook_add(UC_HOOK_MEM_UNMAPPED, on_mem)
    try:
        uc.emu_start(start_va, 0)
    except UcError as e:
        st["stop"] = f"uc_error:{e}"
    st["rip"] = hex(st["last_rip"])
    try:
        st["rax"] = hex(uc.reg_read(UC_X86_REG_RAX))
        st["rbx"] = hex(uc.reg_read(UC_X86_REG_RBX))
        st["rcx"] = hex(uc.reg_read(UC_X86_REG_RCX))
        st["rdx"] = hex(uc.reg_read(UC_X86_REG_RDX))
        st["rsi"] = hex(uc.reg_read(UC_X86_REG_RSI))
        st["rdi"] = hex(uc.reg_read(UC_X86_REG_RDI))
        st["r8"] = hex(uc.reg_read(UC_X86_REG_R8))
        st["r9"] = hex(uc.reg_read(UC_X86_REG_R9))
        st["r12"] = hex(uc.reg_read(UC_X86_REG_R12))
    except UcError:
        pass
    return st


def name_target(pe, va):
    if va is None:
        return {"va": None, "kind": "none"}
    b, s = read_va(pe, va, 32)
    if not b:
        return {"va": hex(va), "kind": "unmapped"}
    ins = list(MD.disasm(b, va))
    first = f"{ins[0].mnemonic} {ins[0].op_str}" if ins else b[:8].hex()
    kind = "code"
    if b[:3] == b"\x55\x48\x89":
        kind = "push_rbp_frame"
    elif b[:1] == b"\xc3":
        kind = "ret"
    elif b[:1] == b"\xe9":
        kind = "jmp_rel32"
        kind_tgt = (va + 5 + i32(b, 1)) & 0xFFFFFFFFFFFFFFFF
        return {"va": hex(va), "kind": kind, "first": first, "section": s["name"] if s else None, "rel32": hex(kind_tgt)}
    elif b[:2] == b"\xff\xe2":
        kind = "jmp_rdx"
    elif b[:2] == b"\xff\xe0":
        kind = "jmp_rax"
    elif b.startswith(bytes.fromhex("4881EC")) or b.startswith(bytes.fromhex("4883EC")):
        kind = "stack_alloc"
    return {
        "va": hex(va),
        "kind": kind,
        "first": first,
        "section": s["name"] if s else None,
        "prefix": b[:16].hex(),
    }


def lift_eos(eos):
    stubs = parse_all_stubs(eos)
    by_addend = defaultdict(list)
    for s in stubs:
        by_addend[s["addend"]].append(s)

    known_lift = {}
    for lab, meta in KNOWN_EOS.items():
        print("  known", lab, flush=True)
        hops, body, kind = follow_rel_jmps(eos, VM_BASE + meta["addend"], max_hops=12)
        cookie = meta["cookie"]
        static = classify_post_cookie(eos, body, cookie, window=0x400)
        emu2 = emu_eos_handler(eos, body, cookie, ctx_va=0x12000)
        known_lift[lab] = {
            "addend": hex(meta["addend"]),
            "declared_body": hex(meta["body"]),
            "followed_body": hex(body),
            "follow_kind": kind,
            "hops": hops,
            "static": static,
            "emu": emu2,
        }

    # unique-addend census after xor/cmp
    print("  census unique addends", len(by_addend), flush=True)
    kind_hist = Counter()
    xor_in_image = 0
    xor_total = 0
    nested = 0
    samples = []
    census_errors = 0
    seen = 0
    for addend, group in by_addend.items():
        st0 = group[0]
        hops, body, kind = follow_rel_jmps(eos, VM_BASE + addend, max_hops=8)
        rec = classify_post_cookie(eos, body, st0["cookie"], window=0x280)
        for k in rec.get("kinds", []):
            kind_hist[k] += 1
        xor_total += len(rec.get("xor_decrypted") or [])
        xor_in_image += sum(1 for x in rec.get("xor_decrypted") or [] if x.get("in_image"))
        if rec.get("nested_stub"):
            nested += 1
        seen += 1
        if len(samples) < 24:
            samples.append(
                {
                    "addend": hex(addend),
                    "stubs": len(group),
                    "body": hex(body),
                    "kinds": rec.get("kinds"),
                    "xor": rec.get("xor_decrypted")[:4],
                    "cmps": rec.get("cmps")[:3],
                    "nested": rec.get("nested_stub"),
                }
            )
        if rec.get("error"):
            census_errors += 1
        if seen % 500 == 0:
            print("    scanned", seen, flush=True)

    return {
        "stubs": len(stubs),
        "unique_addends": len(by_addend),
        "census": {
            "handlers_scanned": seen,
            "kind_hist": dict(kind_hist),
            "xor_decrypted_total": xor_total,
            "xor_decrypted_in_image": xor_in_image,
            "nested_cookie_stub_handlers": nested,
            "errors": census_errors,
        },
        "sample_unique_handlers": samples,
        "known": known_lift,
    }


def emu_eos_handler(pe, body, cookie, ctx_va=0x12000, max_insns=4000):
    uc = Uc(UC_ARCH_X86, UC_MODE_64)
    map_pe(uc, pe)
    for base, sz in ((0x8000, 0x2000), (0x10000, 0x8000)):
        try:
            uc.mem_map(base, sz, UC_PROT_ALL)
        except UcError:
            pass
    def w64(addr, val):
        uc.mem_write(addr, struct.pack("<Q", val & 0xFFFFFFFFFFFFFFFF))

    w64(ctx_va + 0x20, 0x10300)
    w64(ctx_va + 0x58, 0x10100)
    w64(ctx_va + 0x80, 0x10200)
    w64(ctx_va + 0x88, 0)
    w64(ctx_va + 0x90, 0)
    w64(ctx_va + 0x98, 0)
    w64(ctx_va + 0xA0, 0)
    w64(ctx_va + 0xB8, 0)
    w64(ctx_va + 0xC0, 0)
    w64(ctx_va + 0xF0, 0)
    w64(ctx_va + 0x108, VM_BASE)
    w64(ctx_va + 0x110, 0)
    w64(ctx_va + 0x118, 0)
    w64(ctx_va + 0x150, 0)
    w64(ctx_va + 0x188, 0)
    w64(ctx_va + 0x190, cookie)
    w64(ctx_va + 0x198, 0x140381BCA)
    setup_cpu(uc, 0x8F80)
    uc.reg_write(UC_X86_REG_R12, ctx_va)
    uc.reg_write(UC_X86_REG_RCX, 0x10100)
    uc.reg_write(UC_X86_REG_RDX, 0x10200)
    st = {
        "insns": 0,
        "jmp_rax": [],
        "jmp_rdx": [],
        "calls": [],
        "unmapped": [],
        "trace_after_xor": [],
        "seen_xor": False,
        "stop": None,
        "last_rip": body,
        "cookie_slot_end": None,
    }

    def on_mem(uc_, access, address, size, value, user):
        st["unmapped"].append(
            {"access": int(access), "addr": hex(address), "rip": hex(uc_.reg_read(UC_X86_REG_RIP)), "size": size}
        )
        if len(st["unmapped"]) > 12:
            st["stop"] = "unmapped"
            uc_.emu_stop()
        return False

    def on_code(uc_, address, size, user):
        st["insns"] += 1
        st["last_rip"] = address
        if st["insns"] > max_insns:
            st["stop"] = "max_insns"
            uc_.emu_stop()
            return
        try:
            raw = bytes(uc_.mem_read(address, min(size, 16)))
        except UcError:
            st["stop"] = "fetch_fail"
            uc_.emu_stop()
            return
        if raw[:1] == b"\x31" or raw[:2] in (b"\x48\x31", b"\x4c\x31", b"\x49\x31"):
            st["seen_xor"] = True
        if raw[:1] == b"\xff" or raw[:2] in (b"\x41\xff", b"\x49\xff"):
            try:
                ins = next(MD.disasm(raw[: size or 15], address))
            except StopIteration:
                ins = None
            if ins and ins.mnemonic == "jmp" and ins.op_str.lower() in REG64:
                tgt = uc_.reg_read(REG64[ins.op_str.lower()])
                key = "jmp_" + ins.op_str.lower()
                st.setdefault(key, []).append(hex(tgt))
                if key == "jmp_rax":
                    st["jmp_rax"].append(hex(tgt))
                    st["stop"] = "jmp_rax"
                    uc_.emu_stop()
                    return
                if key == "jmp_rdx":
                    st["jmp_rdx"].append(hex(tgt))
                    st["stop"] = "jmp_rdx"
                    uc_.emu_stop()
                    return
                st.setdefault("jmp_other", []).append({"reg": ins.op_str.lower(), "tgt": hex(tgt)})
                st["stop"] = "jmp_" + ins.op_str.lower()
                uc_.emu_stop()
                return
        if raw[:1] == b"\xe8" and len(raw) >= 5:
            rel = struct.unpack_from("<i", raw, 1)[0]
            st["calls"].append(hex((address + 5 + rel) & 0xFFFFFFFFFFFFFFFF))
        if raw == b"\xc3":
            st["stop"] = "ret"
            uc_.emu_stop()
            return
        if st["seen_xor"] and len(st["trace_after_xor"]) < 60:
            try:
                ins = next(MD.disasm(raw[: size or 15], address))
                st["trace_after_xor"].append(f"{address:#x}: {ins.mnemonic} {ins.op_str}")
            except (StopIteration, UcError):
                pass

    uc.hook_add(UC_HOOK_CODE, on_code)
    uc.hook_add(UC_HOOK_MEM_UNMAPPED, on_mem)
    try:
        uc.emu_start(body, 0)
    except UcError as e:
        st["stop"] = f"uc_error:{e}"
    st["rip"] = hex(st["last_rip"])
    regs_out = {}
    for n, r in REG64.items():
        try:
            regs_out[n] = hex(uc.reg_read(r))
        except UcError:
            pass
    st["regs"] = {k: regs_out[k] for k in ("rax", "rbx", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10", "r11", "r12") if k in regs_out}
    try:
        st["cookie_slot_end"] = hex(u64(bytes(uc.mem_read(ctx_va + 0x190, 8)), 0))
        st["dispatch_base_end"] = hex(u64(bytes(uc.mem_read(ctx_va + 0x108, 8)), 0))
        st["cont_slot_end"] = hex(u64(bytes(uc.mem_read(ctx_va + 0x198, 8)), 0))
    except Exception:
        pass
    st["jmp_rax_named"] = [name_target(pe, int(x, 16)) for x in st["jmp_rax"][:8]]
    st["jmp_rdx_named"] = [name_target(pe, int(x, 16)) for x in st["jmp_rdx"][:8]]
    st["calls_named"] = [name_target(pe, int(x, 16)) for x in st["calls"][:8]]
    return st


def disasm_window(pe, va, n, limit_insns=120):
    b, s = read_va(pe, va, n)
    if not b:
        return []
    out = []
    for insn in MD.disasm(b, va):
        out.append(f"{insn.address:#x}: {insn.mnemonic} {insn.op_str}")
        if len(out) >= limit_insns:
            break
    return out


def resolve_bedaisy(be):
    land = 0x1417C4BA1
    print("BEDaisy: disasm window", flush=True)
    # larger static window of the MBA dispatcher
    window_asm = disasm_window(be, land, 0x800, limit_insns=220)
    jmp_rdx_sites = []
    b, _ = read_va(be, land, 0xA00)
    if b:
        for insn in MD.disasm(b, land):
            if insn.mnemonic == "jmp" and insn.op_str.lower() in ("rdx", "rax", "rcx", "r8", "r9", "r10", "r11"):
                jmp_rdx_sites.append({"at": hex(insn.address), "reg": insn.op_str.lower()})

    # Unicorn: CR0.PG=1 so SMSW>>31 == 1 (kernel paging).
    ctx_dummy = 0x12000
    emu = None
    targets = []
    uc = Uc(UC_ARCH_X86, UC_MODE_64)
    map_pe(uc, be)
    for base, sz in ((0x8000, 0x4000), (0x10000, 0x8000)):
        try:
            uc.mem_map(base, sz, UC_PROT_ALL)
        except UcError:
            pass
    setup_cpu(uc, 0xA000, extra_cr0=0x80010033)
    # Replace `smsw rdx` (48 0F 01 E2) with `push 1; pop rdx; nop` so CR0.PG=1
    # is modeled without depending on Unicorn SMSW.
    try:
        uc.mem_write(0x1417C4BBD, bytes.fromhex("6a015a90"))
    except UcError:
        pass
    uc.reg_write(UC_X86_REG_RCX, 0x10100)
    uc.reg_write(UC_X86_REG_RDX, 0x10200)
    uc.reg_write(UC_X86_REG_R8, 0)
    uc.reg_write(UC_X86_REG_R9, 0)
    try:
        uc.reg_write(UC_X86_REG_XMM6, 0)
        uc.reg_write(UC_X86_REG_XMM7, 0)
    except UcError:
        pass
    st = {
        "insns": 0,
        "jmp_rdx": [],
        "jmp_rax": [],
        "calls": [],
        "unmapped": [],
        "stop": None,
        "last_rip": land,
        "smsw_seen": False,
        "trace": [],
    }
    dispatcher_lo = 0x1417C4BA1
    dispatcher_hi = 0x1417EB000  # includes lea rdi target 0x1417ea428

    def force_smsw(uc_):
        # If SMSW is not emulated, skip 4-byte `smsw rdx` and set rdx=1 (CR0.PG).
        uc_.reg_write(UC_X86_REG_RDX, 1)
        uc_.reg_write(UC_X86_REG_RIP, 0x1417C4BC1)

    def on_mem(uc_, access, address, size, value, user):
        st["unmapped"].append(
            {"access": int(access), "addr": hex(address), "rip": hex(uc_.reg_read(UC_X86_REG_RIP))}
        )
        if len(st["unmapped"]) > 16:
            st["stop"] = "unmapped"
            uc_.emu_stop()
        return False

    def on_code(uc_, address, size, user):
        st["insns"] += 1
        st["last_rip"] = address
        if st["insns"] > 8000:
            st["stop"] = "max_insns"
            uc_.emu_stop()
            return
        try:
            raw = bytes(uc_.mem_read(address, min(size, 16)))
        except UcError:
            st["stop"] = "fetch_fail"
            uc_.emu_stop()
            return
        if address == 0x1417C4BBD:
            st["smsw_seen"] = True
        if raw[:1] == b"\xff" or raw[:2] in (b"\x41\xff", b"\x49\xff"):
            try:
                ins = next(MD.disasm(raw[: size or 15], address))
            except StopIteration:
                ins = None
            if ins and ins.mnemonic == "jmp" and ins.op_str.lower() in REG64:
                tgt = uc_.reg_read(REG64[ins.op_str.lower()])
                rec = name_target(be, tgt)
                rec["from"] = hex(address)
                rec["reg"] = ins.op_str.lower()
                if ins.op_str.lower() == "rdx":
                    st["jmp_rdx"].append(rec)
                elif ins.op_str.lower() == "rax":
                    st["jmp_rax"].append(rec)
                else:
                    st.setdefault("jmp_other", []).append(rec)
                if not (dispatcher_lo <= tgt < dispatcher_hi):
                    st.setdefault("outside", []).append(rec)
                    # keep running so later flattening edges are collected
        if raw[:1] == b"\xe8" and len(raw) >= 5:
            rel = struct.unpack_from("<i", raw, 1)[0]
            st["calls"].append(hex((address + 5 + rel) & 0xFFFFFFFFFFFFFFFF))
        if raw == b"\xc3":
            st["stop"] = "ret"
            uc_.emu_stop()
            return
        if len(st["trace"]) < 100:
            try:
                ins = next(MD.disasm(raw[: size or 15], address))
                st["trace"].append(f"{address:#x}: {ins.mnemonic} {ins.op_str}")
            except StopIteration:
                pass

    uc.hook_add(UC_HOOK_CODE, on_code)
    uc.hook_add(UC_HOOK_MEM_UNMAPPED, on_mem)
    try:
        uc.emu_start(land, 0)
    except UcError as e:
        st["stop"] = f"uc_error:{e}"
    st["rip"] = hex(st["last_rip"])
    try:
        st["rdx"] = hex(uc.reg_read(UC_X86_REG_RDX))
        st["rax"] = hex(uc.reg_read(UC_X86_REG_RAX))
        st["rcx"] = hex(uc.reg_read(UC_X86_REG_RCX))
        st["rdi"] = hex(uc.reg_read(UC_X86_REG_RDI))
        st["rbp"] = hex(uc.reg_read(UC_X86_REG_RBP))
        st["r12"] = hex(uc.reg_read(UC_X86_REG_R12))
    except UcError:
        pass

    # Unique named targets
    uniq = []
    seen = set()
    for rec in st["jmp_rdx"]:
        k = rec.get("va")
        if k in seen:
            continue
        seen.add(k)
        uniq.append(rec)

    # Also name the statically recovered lea/sub constants from the first block.
    static_candidates = {
        "lea_r12_rip739": 0x1417C5379,
        "r12_minus_0x672": 0x1417C4D07,
        "lea_rdi_rip25796": 0x1417EA428,
        "rdi_minus_0x2571c": 0x1417C4D0C,
        "epilogue": 0x1417C4CDC,
        "mba_reenter": 0x1417C4BBD,
        "save_args": 0x1417C4CF4,
    }
    static_named = {k: name_target(be, v) for k, v in static_candidates.items()}

    # Follow first outside target a bit (disasm only)
    outside = [r for r in uniq if r.get("va") and not (dispatcher_lo <= int(r["va"], 16) < dispatcher_hi)]
    follow = []
    for r in outside[:8]:
        va = int(r["va"], 16)
        follow.append(
            {
                "va": hex(va),
                "named": name_target(be, va),
                "asm": disasm_window(be, va, 0x120, limit_insns=40),
            }
        )

    return {
        "land": hex(land),
        "jmp_reg_sites_in_window": jmp_rdx_sites,
        "window_asm_head": window_asm[:80],
        "emu": {
            "stop": st["stop"],
            "insns": st["insns"],
            "rip": st["rip"],
            "smsw_forced_pg": True,
            "regs": {k: st.get(k) for k in ("rax", "rcx", "rdx", "rdi", "rbp", "r12") if k in st},
            "jmp_rdx_unique": uniq,
            "jmp_rax_unique": st["jmp_rax"][:12],
            "calls": st["calls"][:12],
            "unmapped": st["unmapped"][:12],
            "trace_head": st["trace"][:40],
        },
        "static_lea_sub_targets": static_named,
        "outside_targets": follow,
    }


def write_json(name, obj):
    text = json.dumps(obj, indent=2)
    os.makedirs(OUT_DIR, exist_ok=True)
    os.makedirs(DEV_OUT, exist_ok=True)
    paths = []
    for d in (OUT_DIR, DEV_OUT):
        p = os.path.join(d, name)
        with open(p, "w", encoding="utf-8") as f:
            f.write(text)
        paths.append(p)
    return paths, len(text)


def main():
    print("parse EOS/BEDaisy PEs", flush=True)
    eos = parse_pe(KNOWN["eos"]["path"])
    be = parse_pe(KNOWN["bedaisy"]["path"])
    assert eos["sha256"] == KNOWN["eos"]["sha"]
    assert be["sha256"] == KNOWN["bedaisy"]["sha"]

    print("lifting EOS handlers…", flush=True)
    eos_lift = lift_eos(eos)
    print("EOS census", eos_lift["census"])
    for lab, rec in eos_lift["known"].items():
        emu = rec["emu"]
        print(
            lab,
            rec["followed_body"],
            "xor",
            len(rec["static"].get("xor_decrypted") or []),
            "stop",
            emu.get("stop"),
            "jmp_rax",
            emu.get("jmp_rax"),
            "insns",
            emu.get("insns"),
        )

    print("resolving BEDaisy jmp rdx…")
    be_lift = resolve_bedaisy(be)
    print("BE stop", be_lift["emu"]["stop"], "insns", be_lift["emu"]["insns"])
    print("BE jmp_rdx unique", be_lift["emu"]["jmp_rdx_unique"])
    print("BE outside", [x["va"] for x in be_lift["outside_targets"]])

    report = {
        "when": "2026-11-18-handler-lift",
        "scope": "static file-byte lift + unicorn on mapped PE; drivers not loaded; no bypass; share Z:",
        "eos_sha256": eos["sha256"],
        "bedaisy_sha256": be["sha256"],
        "eos": eos_lift,
        "bedaisy": be_lift,
    }
    paths, n = write_json("handler-lift.json", report)
    print("wrote", paths, "bytes", n)

    summary = {
        "when": report["when"],
        "scope": report["scope"],
        "eos": {
            "census": eos_lift["census"],
            "known": {
                lab: {
                    "body": rec["followed_body"],
                    "kinds": rec["static"].get("kinds"),
                    "xor_decrypted": rec["static"].get("xor_decrypted"),
                    "cmps": rec["static"].get("cmps"),
                    "nested_stub": rec["static"].get("nested_stub"),
                    "post_xor_asm": rec["static"].get("post_xor_asm")[:24],
                    "emu_stop": rec["emu"].get("stop"),
                    "emu_insns": rec["emu"].get("insns"),
                    "emu_jmp_rax": rec["emu"].get("jmp_rax_named"),
                    "emu_calls": rec["emu"].get("calls_named"),
                    "emu_regs": rec["emu"].get("regs"),
                    "emu_after_xor": rec["emu"].get("trace_after_xor")[:24],
                }
                for lab, rec in eos_lift["known"].items()
            },
        },
        "bedaisy": {
            "jmp_reg_sites": be_lift["jmp_reg_sites_in_window"],
            "emu_stop": be_lift["emu"]["stop"],
            "emu_insns": be_lift["emu"]["insns"],
            "jmp_rdx_unique": be_lift["emu"]["jmp_rdx_unique"],
            "static_lea_sub_targets": be_lift["static_lea_sub_targets"],
            "outside_targets": be_lift["outside_targets"],
            "trace_head": be_lift["emu"]["trace_head"],
        },
    }
    paths, n = write_json("handler-lift-summary.json", summary)
    print("wrote", paths, "bytes", n)


if __name__ == "__main__":
    main()
