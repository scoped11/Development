# EAC / BattlEye — curated public source index

**Vault edition:** 2026-11-17 (PFAB-5)  
**Scope:** Documentation and static RE references only. No live bypass validation, no cheat payloads.  
**Sibling lab map (local):** [byovd/docs/research/16_EAC_BE_KERNEL_RESEARCH.md](../byovd/docs/research/16_EAC_BE_KERNEL_RESEARCH.md)

## How to read this table

| Column | Meaning |
|--------|---------|
| **Topic** | Primary artifact or subsystem (driver, usermode, IOCTL, comms, tooling). |
| **Last activity** | GitHub `pushed_at` or doc revision date. |
| **Risk / legitimacy** | `Research` = notes/static analysis; `Cheat-adjacent` = bypass-oriented code (index for threat modeling only); `Defensive` = blue-team / integrity testing. |
| **Relevance** | Why it belongs in the vault (not an endorsement). |

**Indexed entries:** 28 (24 GitHub repos + 4 official / paper links).

---

## Easy Anti-Cheat (kernel & usermode)

| Repo / doc | Topic | License | Stars | Last activity | Risk / legitimacy | Relevance |
|------------|-------|---------|------:|---------------|-------------------|-----------|
| [adamvalves/easyanticheat-kernel-driver-re](https://github.com/adamvalves/easyanticheat-kernel-driver-re) | driver, obfuscation, stack walk, TPM | MIT | 7 | 2026-11-02 | Research | Strong static RE narrative; Ghidra/IDA oriented; verify IOCTLs on **your** build. |
| [experienceds/eac-eos-research](https://github.com/experienceds/eac-eos-research) | driver, integrity tables, tamper response | Unspecified | 3 | 2026-07-22 | Research | EOS driver internals (verifier tables); good hypothesis checklist. |
| [goldzik1/eac-eos-driver-analysis](https://github.com/goldzik1/eac-eos-driver-analysis) | driver, educational notes | Unspecified | 2 | 2026-07-17 | Research | Lightweight intro to EOS `.sys` layout. |
| [BishopTopG/all-about-eac](https://github.com/BishopTopG/all-about-eac) | driver, memflow, forensics | CC-BY-4.0 | 3 | 2026-09-14 | Research | Single-build dossier; pairs with offline memory acquisition. |
| [Epic — EAC product overview](https://dev.epicgames.com/docs/game-services/anti-cheat/anti-cheat-overview) | usermode, service model | — | — | vendor doc | Official | Public architecture (no rule set); frame what ships vs what is unknown. |
| [Epic — Easy Anti-Cheat documentation hub](https://dev.epicgames.com/docs/game-services/anti-cheat) | integration, launcher | — | — | vendor doc | Official | SDK integration surface; not kernel RE but orients process model. |

---

## BattlEye (BEDaisy, BEService, BEClient)

| Repo / doc | Topic | License | Stars | Last activity | Risk / legitimacy | Relevance |
|------------|-------|---------|------:|---------------|-------------------|-----------|
| [Aki2k/BEDaisy](https://github.com/Aki2k/BEDaisy) | driver, IOCTL sketches | Unspecified | 152 | 2020-08-10 | Research (aged) | Historical BEDaisy RE; **build-specific** — use as xref hints only. |
| [experienceds/battleye-re](https://github.com/experienceds/battleye-re) | driver, IOCTL, comms | Unspecified | 9 | 2026-07-22 | Research | Newer reference pack for BEDaisy.sys. |
| [gmh5225/battleye-decryption](https://github.com/gmh5225/battleye-decryption) | BEService↔BEDaisy protocol | Unspecified | 0 | 2022-05-13 | Research / dual-use | Packet decryption lab; understand **wire format**, not live evasion. |
| [brandenbailey23/bedaisy-unload-fault-0x7e](https://github.com/brandenbailey23/bedaisy-unload-fault-0x7e) | driver, minifilter unload | Unspecified | 1 | 2026-10-01 | Research | Driver lifecycle / stability case study (BSOD forensics). |
| [UnDERL1/bedaisy-reversal](https://github.com/UnDERL1/bedaisy-reversal) | driver, pseudocode snippets | Unspecified | 0 | 2022-04-23 | Research (aged) | Supplementary snippets; cross-check with REA. |
| [tym32167/arma3beclient](https://github.com/tym32167/arma3beclient) | BEClient, tooling | Apache-2.0 | 36 | 2022-12-07 | Research | User-mode BE tooling example (Arma 3); not kernel driver RE. |
| [BattlEye — public site / FAQ](https://www.battleye.com/) | product, policy | — | — | vendor doc | Official | What BattlEye claims publicly (kernel component presence, support model). |

---

## Cross-vendor & platform (kernel anti-cheat patterns)

| Repo / doc | Topic | License | Stars | Last activity | Risk / legitimacy | Relevance |
|------------|-------|---------|------:|---------------|-------------------|-----------|
| [Microsoft — ObRegisterCallbacks](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-obregistercallbacks) | callbacks, handle stripping | — | — | vendor doc | Official | Platform primitive both AC families are expected to use or resemble. |
| [Microsoft — PsSetLoadImageNotifyRoutine](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntddk/nf-ntddk-pssetloadimagenotifyroutine) | image load notify | — | — | vendor doc | Official | Module-load telemetry baseline. |
| [vasie1337/kernel-anticheat](https://github.com/vasie1337/kernel-anticheat) | driver, integrity lab | MIT (archived) | 191 | 2025-06-16 | Defensive | Open teaching driver: compare callback patterns without proprietary binaries. |
| [hcmzah/data-ptr-comm](https://github.com/hcmzah/data-ptr-comm) | driver, usermode↔kernel comms | Unspecified | 213 | 2022-02-09 | Cheat-adjacent | Classic comms pattern cited in AC discussions; study **mechanism**, do not ship. |
| [svespalec/tower-of-flaws](https://github.com/svespalec/tower-of-flaws) | third-party AC driver exploit research | Unspecified | 22 | 2026-03-17 | Research / dual-use | Methodology for **other** AC drivers; threat-model comparison only. |

---

## Agent RE toolchain (use inside VM guest)

| Repo / doc | Topic | License | Stars | Last activity | Risk / legitimacy | Relevance |
|------------|-------|---------|------:|---------------|-------------------|-----------|
| [morluto/rea](https://github.com/morluto/rea) | REA MCP, Ghidra/IDA automation | MIT | — | 2026-11-18 | Tooling | Primary agent workflow per [rea-ai-reversing skill](../../Reversing/.cursor/skills/rea-ai-reversing/SKILL.md). |
| [rea.today — Get started](https://rea.today/get-started) | setup, engines | — | — | vendor doc | Official | Install + MCP wiring for Cursor. |
| [SamNZito/P_Finger_AI_Reversing](https://github.com/SamNZito/P_Finger_AI_Reversing) | project notes, harness | Unspecified | 0 | 2026-11-18 | Research | Sam's long-lived RE notes repo; sync verified findings from `re-notes-*.md`. |
| [memflow/memflow](https://github.com/memflow/memflow) | physical memory, OS introspection | MIT | 400+ | active | Tooling | Used by `all-about-eac`-style offline analysis (guest or dump). |

---

## Indexed for threat awareness only (do not implement)

These appear in GitHub searches; they are listed so you can recognize cheat-oriented repos in the wild. **Do not clone for operational use.**

| Repo | Topic | Risk / legitimacy | Note |
|------|-------|-------------------|------|
| [Deox-XD/BEDaisy-Bypass](https://github.com/Deox-XD/BEDaisy-Bypass) | bypass / callbacks | Cheat-adjacent | Bypass implementation; out of project scope. |
| [emlinhax/beservice_intcallbacks](https://github.com/emlinhax/beservice_intcallbacks) | bypass | Cheat-adjacent | Historical vulnerability abuse write-up. |
| [avalanchesounddepict/ac-fm-shield](https://github.com/avalanchesounddepict/ac-fm-shield) | bypass, spoofer | Cheat-adjacent | Marketing-style cheat stack; ignore for RE workflow. |

---

## Public papers & long-form (non-GitHub)

| Title | Topic | Link | Relevance |
|-------|-------|------|-----------|
| *Cheat-Proofing Online Games* (Chikhaoui et al., survey) | game security economics | [IEEE Xplore — search](https://ieeexplore.ieee.org/search/searchresult.jsp?queryText=cheat-proofing%20online%20games) | Frames client-side limits; motivates kernel AC without vendor specifics. |
| *SoK: Computer-Aided Cybercrime in the Wild* (broad) | underground markets | [ACM DL search](https://dl.acm.org/action/doSearch?AllField=anti-cheat%20kernel) | Context for why closed-source AC exists; not a how-to. |
| Riot Vanguard deep-dive (comparative) | kernel AC design patterns | [Riot Vanguard security](https://www.riotgames.com/en/news/vanguard-security) | Not EAC/BE but explains industry callback / driver stance. |

---

## Next curation pass

1. Record **exact** `EasyAntiCheat_EOS.sys` / `BEDaisy.sys` version hashes from an owned install into `re-notes-*.md` (Verified section).
2. Dedupe against `github-sources.md` when adding new search hits.
3. Promote rows to **Verified** only after REA evidence (address + xref) on your VM build.