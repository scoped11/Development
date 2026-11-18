#!/usr/bin/env python3
from pe_kernel_static import parse_pe, parse_pdata, parse_flt_ops, u16, u32, u64, va_to_off

def find_pdata(pdata, va_hex):
    va = int(va_hex, 16)
    # pdata stores begin_va as hex strings in text_or_init; full list not kept.
    # re-parse from raw in caller.

def pdata_containing(pe, pdata_raw, va):
    rva = va - pe["image_base"]
    best = None
    for x in pdata_raw:
        if x["begin_rva"] <= rva < x["end_rva"]:
            return x
    return None

def parse_flt_context_x64(pe, va):
    recs = []
    # Observed empty terminator 0xFFFF at start for legacy. EOS may have entries.
    # FLT_CONTEXT_REGISTRATION x64 (WDK): typically 0x30
    # USHORT ContextType; USHORT Flags; ULONG Reserved;
    # PVOID ContextCleanupCallback; SIZE_T Size; ULONG PoolTag; ULONG Reserved1;
    # PVOID ContextAllocateCallback; PVOID ContextFreeCallback; PVOID Reserved2;
    for i in range(12):
        off, sec = va_to_off(pe, va + i * 0x30)
        if off is None:
            break
        blob = pe["data"][off : off + 0x30]
        ctx = u16(blob, 0)
        flags = u16(blob, 2)
        rec = {
            "i": i,
            "ContextType": hex(ctx),
            "Flags": flags,
            "hex": blob.hex(),
            "p0": hex(u64(blob, 8)),
            "p1": hex(u64(blob, 16)),
            "p2": hex(u32(blob, 24)),
        }
        recs.append(rec)
        if ctx == 0xFFFF:
            break
    return recs

def main():
    eos = parse_pe(r"C:\Users\bj284\Downloads\TraeTesting\cursor\Development\ac-captures-host\2026-11-17\EasyAntiCheat_EOS.sys")
    from pe_kernel_static import parse_pdata as _pp
    # reuse internal starts: re-read exception dir
    d = eos["dirs"][3]
    off, _ = va_to_off(eos, eos["image_base"] + d["rva"])
    blob = eos["data"][off : off + d["size"]]
    n = d["size"] // 12
    want = {0x23E209, 0x23E237, 0x23E269, 0x272948, 0x272978, 0x38114F, 0x381041, 0x123B1E}
    print("EOS pdata hits for FLT/entry:")
    for i in range(n):
        begin = u32(blob, i * 12)
        end = u32(blob, i * 12 + 4)
        for w in list(want):
            if begin <= w < end:
                print(f"  rva {hex(w)} in [{hex(eos['image_base']+begin)}, {hex(eos['image_base']+end)}) size={end-begin}")

    print("\nEOS FLT_CONTEXT at 0x1402f66e0:")
    for r in parse_flt_context_x64(eos, 0x1402F66E0):
        print(r)

    print("\nEOS FLT ops reconfirm:")
    print(parse_flt_ops(eos, 0x1402F6720))

if __name__ == "__main__":
    main()
