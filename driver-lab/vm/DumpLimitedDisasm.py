# Limited Ghidra disassembly for packed BEDaisy.sys.
# Import with -noanalysis. Restrict DisassembleCommand to AddressSets so
# CreateThunkFunctionCmd cannot chase the cyclic PE-entry jmps.
# @category Analysis
# @runtime Jython

from ghidra.app.cmd.disassemble import DisassembleCommand
from ghidra.program.model.address import AddressSet
from ghidra.util.task import ConsoleTaskMonitor
import os

monitor = ConsoleTaskMonitor()
listing = currentProgram.getListing()
space = currentProgram.getAddressFactory().getDefaultAddressSpace()
mem = currentProgram.getMemory()

out_dir = r"C:\re\artifacts"
try:
    os.makedirs(out_dir)
except Exception:
    pass
out_path = os.path.join(out_dir, "bedaisy-limited-disasm.txt")

RANGES = [
    ("PE_ENTRY", "1425ccea8", 16),
    ("CYCLIC_A", "14261cead", 16),
    ("CYCLIC_B", "14261cea8", 16),
    ("INIT", "14002f000", 96),
    ("COOKIE", "14002f02c", 80),
    ("TEXT_157c8", "1400157c8", 160),
    ("TEXT_66bf", "1400066bf", 160),
    ("TEXT_1000", "140001000", 96),
]


def addr(h):
    return space.getAddress(h)


lines = []
lines.append("program=%s" % currentProgram.getName())
lines.append("image_base=%s" % currentProgram.getImageBase())
lines.append("min=%s max=%s" % (mem.getMinAddress(), mem.getMaxAddress()))
blocks = mem.getBlocks()
for b in blocks:
    lines.append(
        "block name=%s start=%s end=%s size=0x%x init=%s exec=%s"
        % (b.getName(), b.getStart(), b.getEnd(), b.getSize(), b.isInitialized(), b.isExecute())
    )

for name, h, size in RANGES:
    start = addr(h)
    end = start.add(size - 1)
    aset = AddressSet(start, end)
    cmd = DisassembleCommand(aset, aset, True)
    ok = cmd.applyTo(currentProgram, monitor)
    lines.append("")
    lines.append("=== %s %s size=%d apply=%s ===" % (name, start, size, ok))
    ins = listing.getInstructions(start, True)
    n = 0
    while ins.hasNext():
        i = ins.next()
        if i.getAddress().compareTo(end) > 0:
            break
        flows = []
        for f in i.getFlows():
            flows.append(str(f))
        lines.append("%s  %s  flows=%s" % (i.getAddress(), i.toString(), ",".join(flows)))
        n += 1
        if n > 64:
            lines.append("...truncated")
            break
    if n == 0:
        # dump raw bytes if disasm produced nothing
        try:
            buf = bytearray(size)
            mem.getBytes(start, buf)
            lines.append("raw=" + "".join("%02x" % (x & 0xFF) for x in buf))
        except Exception as e:
            lines.append("raw_error=%s" % e)

text = "\n".join(lines) + "\n"
f = open(out_path, "w")
f.write(text)
f.close()
# also copy to share
try:
    f2 = open(r"Z:\ac-captures-host\2026-11-18\bedaisy-limited-disasm.txt", "w")
    f2.write(text)
    f2.close()
except Exception as e:
    print("share copy failed: %s" % e)
print("WROTE %s lines=%d" % (out_path, len(lines)))
