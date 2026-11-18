#!/usr/bin/env python3
from pe_kernel_static import parse_pe, rva_to_off, parse_flt_reg, parse_flt_ops, u16, u32, u64, extract_c_strings

def fileoff_to_va(pe, off):
    for s in pe["sections"]:
        if s["rawptr"] <= off < s["rawptr"] + s["rawsz"]:
            return pe["image_base"] + s["va"] + (off - s["rawptr"]), s
    return None, None

def main():
    eos = parse_pe(r"C:\Users\bj284\Downloads\TraeTesting\cursor\Development\ac-captures-host\2026-11-17\EasyAntiCheat_EOS.sys")
    be = parse_pe(r"C:\Users\bj284\Downloads\TraeTesting\cursor\Development\ac-captures-host\2026-11-18\BEDaisy.sys")

    off = 0x2F5030
    va, sec = fileoff_to_va(eos, off)
    print("EOS flt-like file 0x2f5030 va", hex(va) if va else None, "sec", sec["name"] if sec else None)
    blob = eos["data"][off : off + 112]
    print("size", u16(blob, 0), "ver", hex(u16(blob, 2)), "flags", u32(blob, 4))
    print("hex", blob.hex())
    if va:
        # Size 0x70 = 112 = 8 header + 13 pointers
        names = [
            "ContextRegistration",
            "OperationRegistration",
            "FilterUnloadCallback",
            "InstanceSetupCallback",
            "InstanceQueryTeardownCallback",
            "InstanceTeardownStartCallback",
            "InstanceTeardownCompleteCallback",
            "GenerateFileNameCallback",
            "NormalizeNameComponentCallback",
            "NormalizeContextCleanupCallback",
            "TransactionNotificationCallback",
            "NormalizeNameComponentExCallback",
            "SectionNotificationCallback",
        ]
        for i, n in enumerate(names):
            p = u64(blob, 8 + i * 8)
            print(f"  {n}={hex(p)}")
        ops_va = u64(blob, 16)
        if ops_va:
            recs = parse_flt_ops(eos, ops_va, max_recs=20)
            print("OPS", recs)

    # dump BEDaisy rdata strings with min_len 4, including non-NUL packed
    r = be["data"][0x24C00 : 0x24C00 + 15360]
    print("\nBE .rdata printable runs >=6:")
    for start, s in extract_c_strings(r, min_len=6):
        print(hex(0x24C00 + start), s)

    # sliding window for Flt/Nt/Zw/Ob/Ps/Mm/Io/Ke/Cm/Ex/Rtl
    prefixes = (b"Flt", b"Nt", b"Zw", b"Ob", b"Ps", b"Mm", b"Io", b"Ke", b"Cm", b"ExAllocate", b"Rtl")
    print("\nBE packed prefix hits in .rdata:")
    for pref in prefixes:
        i = 0
        while True:
            j = r.find(pref, i)
            if j < 0:
                break
            # take 4-40 printable
            k = j
            while k < len(r) and 32 <= r[k] < 127 and k - j < 48:
                k += 1
            print(hex(0x24C00 + j), r[j:k].decode("ascii"))
            i = j + 1

    # EOS more utf16 around 0x30e1d0
    chunk = eos["data"][0x30E000:0x30E400]
    # find utf16 runs
    print("\nEOS utf16 around 0x30e000:")
    i = 0
    while i + 4 < len(chunk):
        if 32 <= chunk[i] < 127 and chunk[i + 1] == 0:
            j = i
            chars = []
            while j + 2 <= len(chunk) and 32 <= chunk[j] < 127 and chunk[j + 1] == 0:
                chars.append(chr(chunk[j]))
                j += 2
            if len(chars) >= 6:
                print(hex(0x30E000 + i), "".join(chars))
            i = j + 2
        else:
            i += 1

if __name__ == "__main__":
    main()
