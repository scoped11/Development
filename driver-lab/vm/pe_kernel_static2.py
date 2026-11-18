#!/usr/bin/env python3
"""Follow-up static facts: packed API names, FLT context, INIT pdata."""
from __future__ import annotations

import json
import struct
from pe_kernel_static import parse_pe, va_to_off, u16, u32, u64, parse_pdata, disasm_hint

APIS = [
    "FltRegisterFilter",
    "FltUnregisterFilter",
    "FltStartFiltering",
    "FltGetRequestorProcess",
    "FltGetRequestorProcessId",
    "FltReleaseFileNameInformation",
    "FltGetFileNameInformation",
    "FltQueryInformationFile",
    "FltSetInformationFile",
    "FltCreateCommunicationPort",
    "FltCloseCommunicationPort",
    "FltSendMessage",
    "FltBuildDefaultSecurityDescriptor",
    "FltFreeSecurityDescriptor",
    "NtReadVirtualMemory",
    "NtWriteVirtualMemory",
    "NtDeviceIoControlFile",
    "NtQuerySystemInformation",
    "NtQueryInformationProcess",
    "NtSetInformationProcess",
    "NtQueryInformationThread",
    "NtSetInformationThread",
    "ZwSuspendThread",
    "ZwResumeThread",
    "ZwGetContextThread",
    "ZwSetContextThread",
    "ZwProtectVirtualMemory",
    "ZwQueryVirtualMemory",
    "ZwOpenProcess",
    "ZwTerminateProcess",
    "ZwClose",
    "ObRegisterCallbacks",
    "ObUnRegisterCallbacks",
    "CmRegisterCallbackEx",
    "PsSetCreateProcessNotifyRoutine",
    "PsSetCreateProcessNotifyRoutineEx",
    "PsSetCreateThreadNotifyRoutine",
    "PsSetLoadImageNotifyRoutine",
    "PsSetLoadImageNotifyRoutineEx",
    "MmCopyVirtualMemory",
    "MmGetSystemRoutineAddress",
    "IoCreateDevice",
    "IoCreateSymbolicLink",
    "IoDeleteDevice",
    "IoGetCurrentProcess",
    "KeStackAttachProcess",
    "ExAllocatePoolWithTag",
    "ExAllocatePool2",
    "RtlPcToFileHeader",
    "RtlGetVersion",
    "sha512",
    "LibTomMath",
    "LibTomCrypt",
]


def find_apis(data, names):
    hits = []
    for n in names:
        b = n.encode("ascii")
        start = 0
        while True:
            i = data.find(b, start)
            if i < 0:
                break
            hits.append({"off": hex(i), "name": n})
            start = i + 1
    hits.sort(key=lambda x: int(x["off"], 16))
    return hits


def parse_flt_context(pe, va, max_recs=16):
    recs = []
    for i in range(max_recs):
        off, sec = va_to_off(pe, va + i * 40)
        if off is None:
            break
        # FLT_CONTEXT_REGISTRATION is 0x28 (40) on x64:
        # USHORT ContextType, USHORT Flags, ULONG pad?, then 5 pointers? Actually:
        # ContextType (2) + Flags (2) + ContextCleanupCallback (8) + Size (8) + PoolTag (4) + pad(4)
        # + ContextAllocate (8) + ContextFree (8) + Reserved (8) = 2+2+8+8+4+4+8+8+8 = 52?
        # MSDN x64: often 0x30. Let's dump 16-byte records and also 40/48/56.
        ctx_type = u16(pe["data"], off)
        flags = u16(pe["data"], off + 2)
        recs.append(
            {
                "i": i,
                "file_off": hex(off),
                "ContextType": hex(ctx_type),
                "Flags": flags,
                "hex40": pe["data"][off : off + 40].hex(),
            }
        )
        if ctx_type == 0xFFFF:
            break
    return recs


def main():
    be = parse_pe(
        r"C:\Users\bj284\Downloads\TraeTesting\cursor\Development\ac-captures-host\2026-11-18\BEDaisy.sys"
    )
    lg = parse_pe(
        r"C:\Users\bj284\Downloads\TraeTesting\cursor\Development\ac-captures-host\2026-11-17\EasyAntiCheat.sys"
    )
    eos = parse_pe(
        r"C:\Users\bj284\Downloads\TraeTesting\cursor\Development\ac-captures-host\2026-11-17\EasyAntiCheat_EOS.sys"
    )

    blob = be["data"][0x24C00 : 0x24C00 + 15360]
    api_hits = find_apis(be["data"], APIS)
    rdata_hits = [h for h in api_hits if 0x24C00 <= int(h["off"], 16) < 0x24C00 + 15360]

    pdata = parse_pdata(be)
    init_funcs = [x for x in pdata.get("text_or_init", []) if x["section"] == "INIT"]
    # largest .text functions
    text_funcs = [x for x in pdata.get("text_or_init", []) if x["section"] == ".text"]
    text_funcs_sorted = sorted(text_funcs, key=lambda x: -x["size"])[:15]

    ctx = parse_flt_context(lg, 0x140105D08)

    pre_real = disasm_hint(lg, 0x140064E48, 120)

    # EOS more utf16 around EasyAntiCheat_EOS
    off = 0x30E1D0
    chunk = eos["data"][off - 64 : off + 128]
    eos_utf = chunk.decode("utf-16le", "replace")

    # scan FLT sizes
    flt_pats = {
        "size68_ver202": bytes.fromhex("68000202"),
        "size70_ver202": bytes.fromhex("70000202"),
        "size68_ver203": bytes.fromhex("68000302"),
        "size70_ver203": bytes.fromhex("70000302"),
        "size60_ver201": bytes.fromhex("60000102"),
    }
    eos_flt = {k: hex(eos["data"].find(v)) for k, v in flt_pats.items()}
    be_flt = {k: hex(be["data"].find(v)) for k, v in flt_pats.items()}

    out = {
        "bedaisy_rdata_apis": rdata_hits,
        "bedaisy_all_api_hits_noncert": [
            h for h in api_hits if int(h["off"], 16) < 0x2500000
        ],
        "bedaisy_init_pdata": init_funcs,
        "bedaisy_largest_text": text_funcs_sorted,
        "legacy_flt_context_raw": ctx,
        "legacy_pre_real_0x140064e48": pre_real,
        "eos_utf16_around_EasyAntiCheat_EOS": eos_utf,
        "eos_flt_header_scan": eos_flt,
        "bedaisy_flt_header_scan": be_flt,
    }
    path = r"C:\Users\bj284\Downloads\TraeTesting\cursor\Development\ac-captures-host\2026-11-18\pe-kernel-static2.json"
    with open(path, "w", encoding="utf-8") as f:
        json.dump(out, f, indent=2)
    print("WROTE", path)
    print("rdata apis", [h["name"] for h in rdata_hits])
    print("init pdata", init_funcs)
    print("largest text", [(x["begin_va"], x["size"]) for x in text_funcs_sorted])
    print("eos flt scan", eos_flt)
    print("be flt scan", be_flt)
    print("eos utf16 snippet", repr(eos_utf))
    print("flt ctx types", [c["ContextType"] for c in ctx])
    print("pre_real", pre_real.get("linear", [])[:20])


if __name__ == "__main__":
    main()
