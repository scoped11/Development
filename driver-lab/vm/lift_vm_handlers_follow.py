#!/usr/bin/env python3
"""Name EOS jmp-rax landings and BEDaisy jmp-rdx edges. Static file-byte emu only."""
from __future__ import annotations

import json
import os
import struct

from capstone import CS_ARCH_X86, CS_MODE_64, Cs
from unicorn import Uc, UcError
from unicorn.unicorn_const import UC_ARCH_X86, UC_HOOK_CODE, UC_HOOK_MEM_UNMAPPED, UC_MODE_64, UC_PROT_ALL
from unicorn.x86_const import (
    UC_X86_REG_EFLAGS,
    UC_X86_REG_R8,
    UC_X86_REG_R9,
    UC_X86_REG_R10,
    UC_X86_REG_R11,
    UC_X86_REG_R12,
    UC_X86_REG_R13,
    UC_X86_REG_RAX,
    UC_X86_REG_RBP,
    UC_X86_REG_RBX,
    UC_X86_REG_RCX,
    UC_X86_REG_RDI,
    UC_X86_REG_RDX,
    UC_X86_REG_RIP,
    UC_X86_REG_RSI,
    UC_X86_REG_RSP,
)

from lift_vm_handlers import (
    DEV_OUT,
    KNOWN_EOS,
    MD,
    OUT_DIR,
    REG64,
    VM_BASE,
    disasm_window,
    map_pe,
    name_target,
    setup_cpu,
    write_json,
)
from map_vm_interpreters import KNOWN, parse_pe, read_va, u64

REGS = {
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
}


def dump_regs(uc):
    out = {n: hex(uc.reg_read(r)) for n, r in REGS.items()}
    out["eflags"] = hex(uc.reg_read(UC_X86_REG_EFLAGS))
    return out


def emu_from(pe, start, max_insns=3000, smsw_patch=None, extra_maps=(), ctx_init=None):
    uc = Uc(UC_ARCH_X86, UC_MODE_64)
    map_pe(uc, pe)
    for base, sz in ((0x8000, 0x4000), (0x10000, 0x10000), (0x20000, 0x10000)):
        try:
            uc.mem_map(base, sz, UC_PROT_ALL)
        except UcError:
            pass
    for base, sz in extra_maps:
        try:
            uc.mem_map(base, sz, UC_PROT_ALL)
        except UcError:
            pass
    setup_cpu(uc, 0xA000)
    uc.reg_write(UC_X86_REG_RCX, 0x10100)
    uc.reg_write(UC_X86_REG_RDX, 0x10200)
    uc.reg_write(UC_X86_REG_R12, 0x12000)
    if ctx_init:
        for off, val in ctx_init.items():
            uc.mem_write(0x12000 + off, struct.pack("<Q", val & 0xFFFFFFFFFFFFFFFF))
    if smsw_patch is not None:
        try:
            uc.mem_write(0x1417C4BBD, smsw_patch)
        except UcError:
            pass
    st = {
        "insns": 0,
        "stop": None,
        "last_rip": start,
        "jmp_reg": [],
        "unmapped": [],
        "writes": [],
        "at_4cda": [],
    }

    def on_mem(uc_, access, address, size, value, user):
        rec = {"access": int(access), "addr": hex(address), "rip": hex(uc_.reg_read(UC_X86_REG_RIP)), "size": size}
        if access in (16, 17):  # WRITE / FETCH variants vary; keep all
            st["unmapped"].append(rec)
        else:
            st["unmapped"].append(rec)
        if len(st["unmapped"]) > 6:
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
            st["stop"] = "fetch"
            uc_.emu_stop()
            return
        if address == 0x1417C4CDA:
            st["at_4cda"].append({"n": st["insns"], "regs": dump_regs(uc_)})
            if len(st["at_4cda"]) > 4:
                st["stop"] = "loop_4cda"
                uc_.emu_stop()
                return
        if raw[:1] == b"\xff" or raw[:2] in (b"\x41\xff", b"\x49\xff"):
            try:
                ins = next(MD.disasm(raw[: size or 15], address))
            except StopIteration:
                ins = None
            if ins and ins.mnemonic == "jmp" and ins.op_str.lower() in REG64:
                tgt = uc_.reg_read(REG64[ins.op_str.lower()])
                st["jmp_reg"].append(
                    {
                        "from": hex(address),
                        "reg": ins.op_str.lower(),
                        "tgt": hex(tgt),
                        "named": name_target(pe, tgt),
                        "n": st["insns"],
                    }
                )
        if raw == b"\xc3":
            st["stop"] = "ret"
            uc_.emu_stop()

    uc.hook_add(UC_HOOK_CODE, on_code)
    uc.hook_add(UC_HOOK_MEM_UNMAPPED, on_mem)
    try:
        uc.emu_start(start, 0)
    except UcError as e:
        st["stop"] = f"uc_error:{e}"
    st["rip"] = hex(st["last_rip"])
    st["regs"] = dump_regs(uc)
    return st


def main():
    eos = parse_pe(KNOWN["eos"]["path"])
    be = parse_pe(KNOWN["bedaisy"]["path"])

    eos_sites = {
        "DriverEntry_cont_0x140381325": disasm_window(eos, 0x140381325, 0x80, 40),
        "PreSectionSync_cont_0x1403d4566": disasm_window(eos, 0x1403D4566, 0x80, 40),
        "FUN_140381bca_continuation": disasm_window(eos, 0x140381BCA, 0x40, 20),
        "FUN_140381b12_twin": disasm_window(eos, 0x140381B12, 0x40, 20),
        "FUN_140381041_save": disasm_window(eos, 0x140381041, 0x40, 16),
    }

    # PreCreate: catch the reconstructed store
    cookie = KNOWN_EOS["PreCreate_SetInfo"]["cookie"]
    pc = emu_from(
        eos,
        0x140E85F02,
        max_insns=200,
        ctx_init={0x190: cookie, 0x108: VM_BASE, 0x58: 0x10100, 0x80: 0x10200, 0xA0: 0x10300, 0x158: 0x1111},
        extra_maps=((0xE0000000, 0x200000),),
    )

    be_runs = {
        "entry_smsw_push1": emu_from(be, 0x1417C4BA1, max_insns=2000, smsw_patch=bytes.fromhex("6a015a90")),
        "entry_smsw_xor0": emu_from(be, 0x1417C4BA1, max_insns=2000, smsw_patch=bytes.fromhex("31d29090")),  # xor edx,edx; nop; nop
        "force_4d0c": emu_from(be, 0x1417C4D0C, max_insns=2000, smsw_patch=bytes.fromhex("6a015a90")),
        "force_4d07": emu_from(be, 0x1417C4D07, max_insns=400, smsw_patch=bytes.fromhex("6a015a90")),
    }

    # name dense jmp-reg sites in first 0x400 of land by following each unique tgt statically
    named_sites = {}
    for va in (0x1417C4CDA, 0x1417C4D07, 0x1417C4D0C, 0x1417C4CDC, 0x1417C4CF4, 0x1417C4BBD, 0x1417C5379, 0x1417EA428):
        named_sites[hex(va)] = {
            "named": name_target(be, va),
            "asm": disasm_window(be, va, 0x80, 24),
        }

    # compact be run view
    be_view = {}
    for k, st in be_runs.items():
        be_view[k] = {
            "stop": st["stop"],
            "insns": st["insns"],
            "rip": st["rip"],
            "jmp_reg": st["jmp_reg"][:12],
            "at_4cda": st["at_4cda"],
            "unmapped": st["unmapped"][:6],
            "regs": st["regs"],
        }

    report = {
        "when": "2026-11-18-handler-lift-follow",
        "scope": "static file-byte emu; drivers not loaded; no bypass",
        "eos_sites": eos_sites,
        "precreate_emu": {
            "stop": pc["stop"],
            "insns": pc["insns"],
            "rip": pc["rip"],
            "unmapped": pc["unmapped"][:8],
            "jmp_reg": pc["jmp_reg"][:8],
            "regs": pc["regs"],
        },
        "bedaisy_named_sites": named_sites,
        "bedaisy_runs": be_view,
    }
    paths, n = write_json("handler-lift-follow.json", report)
    print("wrote", paths, n)
    print("EOS cont 381325", eos_sites["DriverEntry_cont_0x140381325"][:8])
    print("EOS cont 3d4566", eos_sites["PreSectionSync_cont_0x1403d4566"][:8])
    print("PreCreate", pc["stop"], pc["unmapped"][:3])
    for k, v in be_view.items():
        print(k, v["stop"], v["insns"], "jmps", [(j["reg"], j["tgt"]) for j in v["jmp_reg"][:6]], "4cda", v["at_4cda"][:1])


if __name__ == "__main__":
    main()
