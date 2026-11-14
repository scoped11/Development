# Game Analysis Bible — deep research addendum

**Edition:** 12 November 2026. New files only. Keep the existing Game_Analysis_Research_Expanded collection and original archives unchanged. This folder is an independent, cross-title technical reference, not a Rob Run-specific development guide. Read the unified architecture document first and the source catalog alongside each chapter.

| File | Contents |
|---|---|
| [01_UNIFIED_CHEAT_ARCHITECTURES.md](01_UNIFIED_CHEAT_ARCHITECTURES.md) | Unified internal, external, kernel, DMA, vision, input and hybrid architectures; hardware/software layers, failure matrix |
| [02_MEMORY_AND_OFFSETS.md](02_MEMORY_AND_OFFSETS.md) | Address semantics, PE mapping, ABI, layout, snapshots and evidence records |
| [03_GAME_ENGINE_INTERNALS.md](03_GAME_ENGINE_INTERNALS.md) | Unity Mono/IL2CPP, Unreal, Source/Source 2, Godot, custom engines and dumper compatibility |
| [04_GAMEPLAY_FEATURE_TAXONOMY.md](04_GAMEPLAY_FEATURE_TAXONOMY.md) | Cheat terminology, dependencies, effects and authorized evaluation |
| [05_GRAPHICS_CAMERA_AND_MATH.md](05_GRAPHICS_CAMERA_AND_MATH.md) | Coordinate transforms, camera, FOV, rendering, timing, test inputs |
| [06_NETWORKING_AND_SERVER_AUTHORITY.md](06_NETWORKING_AND_SERVER_AUTHORITY.md) | Replication, authority, prediction, reconciliation, visibility, validation |
| [07_CRYPTOGRAPHY_AND_ASSET_FORMATS.md](07_CRYPTOGRAPHY_AND_ASSET_FORMATS.md) | Formats, compression, cryptographic parameters, synthetic parse/authentication fixtures |
| [08_REVERSE_ENGINEERING_WORKFLOW.md](08_REVERSE_ENGINEERING_WORKFLOW.md) | Static analysis, tools, evidence, compiler optimization, build comparison |
| [09_POPULAR_GAME_CASE_STUDIES.md](09_POPULAR_GAME_CASE_STUDIES.md) | Twelve public publisher case studies, not private target maps |
| [10_ANTICHEAT_AND_SECURITY.md](10_ANTICHEAT_AND_SECURITY.md) | Security layers, device isolation, server checks, measurement limitations |
| [11_CURSOR_IDA_RESEARCH_INTEGRATION.md](11_CURSOR_IDA_RESEARCH_INTEGRATION.md) | Proposed MCP-free architecture, schemas, operation contracts and acceptance tests |
| [12_RESEARCH_LAB_AND_VALIDATION.md](12_RESEARCH_LAB_AND_VALIDATION.md) | Seven controlled lab plans, detailed inputs, expected output, negative cases and evidence grading |
| [13_SOURCE_CATALOG.md](13_SOURCE_CATALOG.md) | Source IDs, direct URLs, publisher/standards distinctions, prior project evidence and fetch limits |
| [14_KERNEL_DRIVERS.md](14_KERNEL_DRIVERS.md) | Windows kernel driver models, IRPs/IOCTLs, IRQL, signing/HVCI, ObRegisterCallbacks; Lab H proposal |
| [15_BYOVD_AND_DRIVER_TRUST.md](15_BYOVD_AND_DRIVER_TRUST.md) | BYOVD definition, platform controls, kdmapper audit vs masterlist, lab stability |
| [16_EAC_BE_KERNEL_RESEARCH.md](16_EAC_BE_KERNEL_RESEARCH.md) | EAC/BattlEye kernel threat map, artifact matrix, Lab I proposal (defensive) |
| [17_HOST_AC_CAPTURES_2026-11-13.md](17_HOST_AC_CAPTURES_2026-11-13.md) | **Session (13 Nov 2026):** Host captures — Rust EOS, R6 BE, Delta/ABI ACE; SHA256, paths, dump script, gaps (BEDaisy, ARK) |
| [18_AC_REPLICA_MAPPER_INTEGRATION.md](18_AC_REPLICA_MAPPER_INTEGRATION.md) | **Session:** `ac_replica_profiles` v2, mapper `--ac-replica` titles, pf_diag gates, VM parity scripts |
| [19_HOST_LAB_VM_LANE_ARCHITECTURE.md](19_HOST_LAB_VM_LANE_ARCHITECTURE.md) | **Session:** Lane D/E, DriverLab VM, hijack constraints, Betest vs live host, confidence levels |
| [20_SESSION_DRIVER_OPS_AND_STABILITY.md](20_SESSION_DRIVER_OPS_AND_STABILITY.md) | **Session:** Map/R/W/hooks, IOCTL risk tiers, bundle index — companion to `ac-captures-host/docs/DRIVER_LAB_GUIDE.md` |

**Lab BYOVD kit (mapper + payload + kdmapper reference):** [`../byovd/README.md`](../byovd/README.md) — builds `mapper_nal.exe` + `Betest.sys`, vehicle driver folder, trace-surface notes.

**Host capture bundle (same session):** [`../ac-captures-host/README.md`](../ac-captures-host/README.md) — dumps + SDK + driver how-to; research narrative in chapters **17–20** above.

## How to use this collection in Cursor

1. Add the new folder alongside your existing Markdown files; do not overwrite them. Keep source references intact.
2. Read 01, 02 and 03 for shared architecture and terminology. Use 04–07 for a specific research question.
3. For a new title, begin with the public dossier template in 09 and identify version, platform, authorized interfaces and unknowns. Use 08 for owned/static artifacts and 12 for controlled validation.
4. Treat all examples, CLI commands and lab plans as proposed or synthetic unless a real run record establishes otherwise. Do not apply another title's offsets, assumptions or policies by analogy.
5. Preserve source IDs, hashes, build identity and status in all future findings. The prior Rob Run files remain historical project context. No prior project files were modified.

## Scope and limitations

This collection studies cheat categories, game security, systems architecture and authorized/offline research. It does not contain current live game offset maps, working competitive-game cheat implementations, anti-cheat bypasses, device spoofing, unauthorized memory acquisition procedures, or protected-title decryption keys. It does not claim to exhaust every game. Public publisher statements are not independently tested. No hardware, games, IDA, dumper or cryptographic tests were executed in creating the addendum. See the source catalog for unavailable pages and further-reading pointers.
