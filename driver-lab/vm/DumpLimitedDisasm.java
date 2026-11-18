// Limited Ghidra disassembly for packed BEDaisy.sys.
// Import with -noanalysis. Restrict DisassembleCommand to AddressSets so
// CreateThunkFunctionCmd cannot chase the cyclic PE-entry jmps.
//@category Analysis

import java.io.FileWriter;
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.mem.MemoryBlock;

public class DumpLimitedDisasm extends GhidraScript {

	private static final String[][] RANGES = {
		{ "PE_ENTRY", "1425ccea8", "16" },
		{ "CYCLIC_A", "14261cead", "16" },
		{ "CYCLIC_B", "14261cea8", "16" },
		{ "INIT", "14002f000", "96" },
		{ "COOKIE", "14002f02c", "80" },
		{ "TEXT_157c8", "1400157c8", "160" },
		{ "TEXT_66bf", "1400066bf", "160" },
		{ "TEXT_1000", "140001000", "96" },
	};

	@Override
	public void run() throws Exception {
		Memory mem = currentProgram.getMemory();
		StringBuilder sb = new StringBuilder();
		sb.append("program=").append(currentProgram.getName()).append('\n');
		sb.append("image_base=").append(currentProgram.getImageBase()).append('\n');
		sb.append("min=").append(mem.getMinAddress()).append(" max=").append(mem.getMaxAddress()).append('\n');
		for (MemoryBlock b : mem.getBlocks()) {
			sb.append("block name=").append(b.getName())
				.append(" start=").append(b.getStart())
				.append(" end=").append(b.getEnd())
				.append(" size=0x").append(Long.toHexString(b.getSize()))
				.append(" init=").append(b.isInitialized())
				.append(" exec=").append(b.isExecute())
				.append('\n');
		}

		for (String[] r : RANGES) {
			Address start = toAddr(r[1]);
			int size = Integer.parseInt(r[2]);
			Address end = start.add(size - 1);
			AddressSet aset = new AddressSet(start, end);
			DisassembleCommand cmd = new DisassembleCommand(aset, aset, true);
			boolean ok = cmd.applyTo(currentProgram, monitor);
			sb.append('\n').append("=== ").append(r[0]).append(' ').append(start)
				.append(" size=").append(size).append(" apply=").append(ok).append(" ===\n");
			InstructionIterator it = currentProgram.getListing().getInstructions(start, true);
			int n = 0;
			while (it.hasNext()) {
				Instruction ins = it.next();
				if (ins.getAddress().compareTo(end) > 0) {
					break;
				}
				sb.append(ins.getAddress()).append("  ").append(ins.toString()).append('\n');
				n++;
				if (n > 64) {
					sb.append("...truncated\n");
					break;
				}
			}
			if (n == 0) {
				try {
					byte[] buf = new byte[size];
					mem.getBytes(start, buf);
					sb.append("raw=");
					for (byte x : buf) {
						sb.append(String.format("%02x", x & 0xff));
					}
					sb.append('\n');
				}
				catch (Exception e) {
					sb.append("raw_error=").append(e).append('\n');
				}
			}
		}

		String text = sb.toString();
		writeFile("C:\\re\\artifacts\\bedaisy-limited-disasm.txt", text);
		try {
			writeFile("Z:\\ac-captures-host\\2026-11-18\\bedaisy-limited-disasm.txt", text);
		}
		catch (Exception e) {
			println("share copy failed: " + e);
		}
		println("WROTE limited disasm chars=" + text.length());
	}

	private void writeFile(String path, String text) throws Exception {
		FileWriter w = new FileWriter(path);
		w.write(text);
		w.close();
	}
}
