// Dump selected EAC procedures via Ghidra decompiler. Run -noanalysis on an imported program.
//@category Analysis

import java.io.FileWriter;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;

public class DumpEacFunctions extends GhidraScript {

	@Override
	public void run() throws Exception {
		String name = currentProgram.getName().toLowerCase();
		String[][] targets;
		String outName;
		if (name.contains("eos") || name.contains("26797")) {
			targets = new String[][] {
				{ "entry", "140272978" },
				{ "FUN_140272948", "140272948" },
				{ "FUN_1403a572e", "1403a572e" },
				{ "FUN_140381041", "140381041" },
				{ "FUN_14038114f", "14038114f" },
				{ "FUN_140123b1e", "140123b1e" },
				{ "FLT_unload_14023e209", "14023e209" },
				{ "FLT_pre_create_14023e237", "14023e237" },
				{ "FLT_pre_section_14023e269", "14023e269" },
			};
			outName = "eac-eos-decomp.txt";
		}
		else {
			targets = new String[][] {
				{ "entry", "1400afde8" },
				{ "FUN_1400650f8", "1400650f8" },
				{ "FUN_1400650ac", "1400650ac" },
				{ "FUN_1400650b4", "1400650b4" },
				{ "FUN_1400855d0", "1400855d0" },
				{ "section6_14071c246", "14071c246" },
				{ "pre_real_140064e48", "140064e48" },
			};
			outName = "eac-legacy-decomp.txt";
		}

		DecompInterface ifc = new DecompInterface();
		ifc.openProgram(currentProgram);
		StringBuilder sb = new StringBuilder();
		sb.append("program=").append(currentProgram.getName()).append('\n');
		sb.append("image_base=").append(currentProgram.getImageBase()).append('\n');
		sb.append("func_count=").append(currentProgram.getFunctionManager().getFunctionCount()).append('\n');

		for (String[] t : targets) {
			Address addr = toAddr(t[1]);
			Function fn = getFunctionAt(addr);
			if (fn == null) {
				fn = getFunctionContaining(addr);
			}
			sb.append('\n').append("=== ").append(t[0]).append(' ').append(addr)
				.append(" fn=").append(fn == null ? "NONE" : fn.getName()).append(" ===\n");
			InstructionIterator it = currentProgram.getListing().getInstructions(addr, true);
			int n = 0;
			while (it.hasNext() && n < 40) {
				Instruction ins = it.next();
				if (fn != null && !fn.getBody().contains(ins.getAddress()) && n > 0) {
					break;
				}
				sb.append("  ").append(ins.getAddress()).append("  ").append(ins.toString()).append('\n');
				n++;
			}
			if (fn != null) {
				DecompileResults res = ifc.decompileFunction(fn, 60, monitor);
				if (res != null && res.decompileCompleted()) {
					sb.append("C:\n").append(res.getDecompiledFunction().getC()).append('\n');
				}
				else {
					sb.append("decompile_fail=")
						.append(res == null ? "no result" : res.getErrorMessage()).append('\n');
				}
			}
			else {
				sb.append("no_function\n");
			}
		}

		String text = sb.toString();
		writeFile("C:\\re\\artifacts\\" + outName, text);
		try {
			writeFile("Z:\\ac-captures-host\\2026-11-18\\" + outName, text);
		}
		catch (Exception e) {
			println("share copy failed: " + e);
		}
		println("WROTE " + outName + " chars=" + text.length());
	}

	private void writeFile(String path, String text) throws Exception {
		FileWriter w = new FileWriter(path);
		w.write(text);
		w.close();
	}
}
