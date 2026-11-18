# RE notes — Easy Anti-Cheat (EAC)

**Binary focus:** `EasyAntiCheat_EOS.sys`, `EasyAntiCheat.sys` (legacy naming), usermode launcher/service (game-dependent).  
**Evidence rule:** per [rea-ai-reversing](../../Reversing/.cursor/skills/rea-ai-reversing/SKILL.md) — no address lands in **Verified** without REA output.

## Verified

| Claim | Evidence | Build / hash |
|-------|----------|--------------|
| *(empty)* | — | Record SHA256 of driver from **your** install path when starting VM RE |

**Public reference only (not verified on our build):**

- Third-party repos document import obfuscation, stack walking, and TPM-related paths on specific EOS builds — treat as **reading list** until REA confirms on owned samples ([index](eac-be-research-index.md)).

## Hypothesis

| ID | Hypothesis | Confidence | Falsify with |
|----|------------|------------|--------------|
| H-EAC-01 | EOS kernel driver registers image-load and thread/process notify routines for game process lineage. | Medium | REA: `DriverEntry` → callback registration xrefs |
| H-EAC-02 | IOCTL surface is narrow vs usermode service; bulk enforcement may live in worker threads, not IRP dispatch. | Low | `grep` device names + `MajorFunction[IRP_MJ_DEVICE_CONTROL]` decompile |
| H-EAC-03 | Integrity subsystem uses table-driven verifiers (per `experienceds/eac-eos-research` public notes). | Low | Compare table count/size across game patch |
| H-EAC-04 | Usermode stack includes EOS bootstrap + game-specific module; kernel driver version tracks anti-cheat update channel, not game version. | Medium | Side-by-side file versions from install dirs |

## IOCTLs / devices

| Device / symlink | IOCTL (hex) | Direction | Status | Notes |
|------------------|------------|-----------|--------|-------|
| *(stub)* | — | — | Unverified | Fill from REA `procedure_callees` on dispatch routines |

## Components (expected stack)

| Layer | Artifact (typical) | Role |
|-------|-------------------|------|
| Kernel | `EasyAntiCheat_EOS.sys` | Callbacks, integrity, attestation hooks |
| Service / UM | EAC service + launcher (per game) | Policy, updates, reporting |
| Game | Protected process | Target of handle strip / module policy |

## Next steps (VM + REA)

1. Copy **one** labeled `EasyAntiCheat_EOS.sys` into `Development\ac-captures-host\` or a dated folder under the shared mount (see [vm-checklist.md](vm-checklist.md)).
2. In guest Cursor: `open_binary` → `binary_overview` → list imports/sections.
3. Name `DriverEntry` and IOCTL dispatch; log RVAs in **Verified** with Ghidra project path.
4. Cross-check public repos in [eac-be-research-index.md](eac-be-research-index.md) — **re-verify every IOCTL**.
5. Update [16_EAC_BE_KERNEL_RESEARCH.md](../byovd/docs/research/16_EAC_BE_KERNEL_RESEARCH.md) only with defensive forensics (no bypass IOCTLs).

## Dead ends

| Date | Item | Reason |
|------|------|--------|
| — | — | — |