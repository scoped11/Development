# Rob Run DMA source

CMake project for the DX11 overlay. Workspace root is one level up (`Development/`).

**Lane D (Split-Trust Calibration Bridge):** When EAC blocks BYOVD on the game PC, use **this tree for live reads** on the Rust session and map kernel only on a **no-AC lab VM** (`../byovd/`). Sync `build_id` + offsets — see [../byovd/docs/12_lane_d_split_trust.md](../byovd/docs/12_lane_d_split_trust.md).

- Default output: `../debug/RobRunDMA.exe`
- Approved output: `../debug` is used until you run `-Section Build -IApprove`, which writes `../build/`
- Dumper: `../tools/il2cpp-dumper/`
- Dumps: `../dumps/<date>/`
- Offset MCP: `ida-offset-mcp/` (Cursor server `robrun-ida`). Exact RVA confirms; do not use stock IDA `decompile`/`survey` on IL2CPP getters. See [docs/IDA_MCP.md](docs/IDA_MCP.md).

See the parent [README](../README.md).
