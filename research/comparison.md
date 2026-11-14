# Cheat Comparison: Internal vs External vs DMA

## Side-by-Side

| Factor | Internal | External | DMA |
|---|---|---|---|
| **Runs in** | Game process (injected DLL) | Separate process | Second PC (hardware) |
| **Data access** | Direct / native | `ReadProcessMemory` / driver | PCIe card reads RAM |
| **Visuals** | Game's own renderer (DX/Vulkan hooks) | Separate overlay window | Fuser + external display |
| **Input** | Direct (in-process) | Direct | KMBox / input device |
| **Detection risk** | High (hooks, module scanning, integrity checks) | Low–Medium (handle rights, module list) | Lowest (mostly invisible to software) |
| **Anti-cheat counter** | VAD walking, PE header scan, IAT/inline hook check | `ObRegisterCallbacks` stripping VM access, driver scanning | Hardware ID / PCIe slot fingerprinting |
| **Cost** | Low (just the cheat) | Low | High (card, firmware, 2nd PC, KMBox) |
| **Setup effort** | Medium (loader) | Low (run loader) | High (BIOS, firmware, cabling) |
| **Feature ceiling** | Highest (silent aim, chams, full control) | High | High for reads, needs round-trip for writes |
| **Performance cost** | Minimal | Overlay overhead, polling latency | Near zero on gaming PC |
| **Stability** | Cheat crash can crash game | Game usually survives cheat crash | Isolated from gaming PC entirely |
| **Streamproof** | Harder (in-game draw) | Easy (overlay) | Trivial (different screen) |

## Decision Guide

- **Most features, premium feel, account not precious** → Internal
- **Best balance, no extra hardware, casual competitive** → External
- **High-value account, serious competition, budget available** → DMA

## Key Takeaway

The arms race is a ladder: internal is the easiest for AC to find (it's inside the process), external is harder (AC must catch the *other* process and its driver), DMA is hardest (AC must notice hardware the OS itself may not fingerprint). Each rung up costs more money and more setup knowledge, which is what filters out casual cheaters.
