# Dump selected EAC procedures via Ghidra decompiler. Run against an already
# imported program with -noanalysis (or after a completed analyze).
# @category Analysis
# @runtime Jython

from ghidra.app.decompiler import DecompInterface
from ghidra.util.task import ConsoleTaskMonitor
import os

monitor = ConsoleTaskMonitor()
listing = currentProgram.getListing()
space = currentProgram.getAddressFactory().getDefaultAddressSpace()
fm = currentProgram.getFunctionManager()

out_dir = r"C:\re\artifacts"
try:
    os.makedirs(out_dir)
except Exception:
    pass

# Addresses of interest. EOS vs legacy decided by image name.
name = currentProgram.getName().lower()
if "eos" in name or "26797" in name:
    targets = [
        ("entry", "140272978"),
        ("FUN_140272948", "140272948"),
        ("FUN_1403a572e", "1403a572e"),
        ("FUN_140381041", "140381041"),
        ("FUN_14038114f", "14038114f"),
        ("FUN_140123b1e", "140123b1e"),
        ("IAT_thunk_Flt", "140272940"),
        ("FLT_unload_14023e209", "14023e209"),
        ("FLT_pre_create_14023e237", "14023e237"),
        ("FLT_pre_section_14023e269", "14023e269"),
        ("FLT_reg_1402f6230", "1402f6230"),
    ]
    out_name = "eac-eos-decomp.txt"
elif "easya" in name or "dd60" in name:
    targets = [
        ("entry", "1400afde8"),
        ("FUN_1400650f8", "1400650f8"),
        ("FUN_1400650ac", "1400650ac"),
        ("FUN_1400650b4", "1400650b4"),
        ("FUN_1400855d0", "1400855d0"),
        ("section6_14071c246", "14071c246"),
        ("pre_real_140064e48", "140064e48"),
        ("FLT_reg_1401060f0", "1401060f0"),
    ]
    ]
    out_name = "eac-legacy-decomp.txt"
else:
    targets = []
    out_name = "eac-unknown-decomp.txt"

ifc = DecompInterface()
ifc.openProgram(currentProgram)

lines = []
lines.append("program=%s" % currentProgram.getName())
lines.append("image_base=%s" % currentProgram.getImageBase())
lines.append("func_count=%d" % fm.getFunctionCount())

for label, h in targets:
    addr = space.getAddress(h)
    fn = fm.getFunctionAt(addr)
    if fn is None:
        fn = fm.getFunctionContaining(addr)
    lines.append("")
    lines.append("=== %s %s fn=%s ===" % (label, addr, fn.getName() if fn else "NONE"))
    ins = listing.getInstructions(addr, True)
    n = 0
    while ins.hasNext() and n < 40:
        i = ins.next()
        if fn is not None and not fn.getBody().contains(i.getAddress()):
            if n > 0:
                break
        lines.append("  %s  %s" % (i.getAddress(), i.toString()))
        n += 1
    if fn is not None:
        res = ifc.decompileFunction(fn, 60, monitor)
        if res and res.decompileCompleted():
            lines.append("C:")
            lines.append(res.getDecompiledFunction().getC())
        else:
            err = res.getErrorMessage() if res else "no result"
            lines.append("decompile_fail=%s" % err)
    else:
        lines.append("no_function")

text = "\n".join(lines) + "\n"
out_path = os.path.join(out_dir, out_name)
f = open(out_path, "w")
f.write(text)
f.close()
share = os.path.join(r"Z:\ac-captures-host\2026-11-18", out_name)
try:
    f2 = open(share, "w")
    f2.write(text)
    f2.close()
except Exception as e:
    print("share copy failed: %s" % e)
print("WROTE %s lines=%d" % (out_path, len(lines)))
