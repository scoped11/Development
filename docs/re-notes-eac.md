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

## Next RE targets (concrete — from [eac-be-research-index.md](eac-be-research-index.md))

Prioritize **owned** `EasyAntiCheat_EOS.sys` on guest local disk (`C:\re\artifacts\`) for Ghidra I/O; keep SHA256 on the share.

| Priority | Target | REA entry points | Public xref (hypothesis only) |
|----------|--------|------------------|-------------------------------|
| P0 | `EasyAntiCheat_EOS.sys` | `open_binary` → `inspect_pe_load_config` → `binary_overview` | adamvalves/easyanticheat-kernel-driver-re |
| P1 | Driver init | `search_procedures` DriverEntry / `GsDriverEntry` → `procedure_pseudo_code` | Image-load + process notify registration |
| P1 | Callbacks | xrefs to `PsSetCreateProcessNotifyRoutine`, `PsSetLoadImageNotifyRoutine`, `ObRegisterCallbacks` | Microsoft WDK docs + H-EAC-01 |
| P2 | IOCTL / devices | `MajorFunction[IRP_MJ_DEVICE_CONTROL]` decompile; `search_strings` device names | experienceds/eac-eos-research IOCTL sketches |
| P2 | Integrity tables | large RO data regions; pointer arrays before dispatch | experienceds + goldzik1 EOS layout notes |
| P3 | Obfuscation / stack | import thunk patterns, stack-walk helpers | adamvalves (re-verify on build) |
| P3 | TPM / attestation path | string xrefs `Tpm`, `EkCert`, platform hooks | adamvalves narrative — may be absent on some builds |
| P4 | Usermode (optional static) | EAC service binary from game install dir — imports only | Epic integration docs (process model) |

**Symbols to name first session:** `DriverEntry`, primary `IRP_MJ_DEVICE_CONTROL` handler, any `IoCreateDevice` / `IoCreateSymbolicLink` pair.

## Next steps (VM + REA)

1. Complete [install-ghidra-rea-guest.ps1](../driver-lab/vm/install-ghidra-rea-guest.ps1) + [rea-vm-smoke-test.md](rea-vm-smoke-test.md) on guest.
2. Copy **one** labeled `EasyAntiCheat_EOS.sys` into `ac-captures-host\<date>\` on the share; hash with `Get-FileHash`.
3. Guest Cursor: `open_binary` on **local copy** → `binary_overview` → imports/sections.
4. Work P0–P2 table; promote rows to **Verified** only with REA addresses.
5. Cross-check [eac-be-research-index.md](eac-be-research-index.md) — **re-verify every IOCTL**.
6. Update [16_EAC_BE_KERNEL_RESEARCH.md](../byovd/docs/research/16_EAC_BE_KERNEL_RESEARCH.md) for defensive forensics only.

## Dead ends

| Date | Item | Reason |
|------|------|--------|
| — | — | — |