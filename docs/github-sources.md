# GitHub search log — Easy Anti-Cheat & BattlEye

**Collected:** 2026-11-17 via Cursor GitHub API (`search_repositories`).  
**Dedup:** by `full_name`; cheat-primary repos noted but not promoted to primary RE list.

## Queries run

| # | Query | Sort | `total_count` | Notes |
|---|--------|------|--------------:|-------|
| Q1 | `EasyAntiCheat reverse engineering` | stars desc | 3 | All hits are research-oriented (2026). |
| Q2 | `EasyAntiCheat driver kernel` | updated desc | 4 | Overlaps Q1; adds `hcmzah/data-ptr-comm` (comms pattern). |
| Q3 | `EasyAntiCheat` | stars desc | 50 | Broad; many Unity/EOS integration templates — most filtered out. |
| Q4 | `topic:easyanticheat` | default | 30 | Mix of game mods and SDK samples. |
| Q5 | `BattlEye reverse engineering` | stars desc | 2 | `Aki2k/BEDaisy`, `experienceds/battleye-re`. |
| Q6 | `BEDaisy BattlEye` | updated desc | 6 | Includes bypass repos; see blocklist below. |
| Q7 | `BEClient BattlEye` | updated desc | 1 | `tym32167/arma3beclient`. |
| Q8 | `BEService BattlEye` | default | 2 | Protocol + historical bypass. |
| Q9 | `topic:battleye` | stars desc | 30 | Mostly game servers / Arma tooling. |
| Q10 | `anti-cheat kernel driver research` | stars desc | 3 | Generic kernel AC research. |
| Q11 | `memflow anti-cheat` | default | 1 | `BishopTopG/all-about-eac`. |
| Q12 | `user:SamNZito reversing` | default | 1 | `SamNZito/P_Finger_AI_Reversing`. |

---

## Top hits — Easy Anti-Cheat (deduped)

| Repo | Stars | Updated | Description (truncated) |
|------|------:|---------|-------------------------|
| [adamvalves/easyanticheat-kernel-driver-re](https://github.com/adamvalves/easyanticheat-kernel-driver-re) | 7 | 2026-11-11 | Kernel driver RE: import obfuscation, stack walking, TPM attestation |
| [BishopTopG/all-about-eac](https://github.com/BishopTopG/all-about-eac) | 3 | 2026-11-13 | Memflow dossier for one `EasyAntiCheat_EOS.sys` build |
| [experienceds/eac-eos-research](https://github.com/experienceds/eac-eos-research) | 3 | 2026-11-13 | Integrity subsystem, verifier tables, tamper response |
| [goldzik1/eac-eos-driver-analysis](https://github.com/goldzik1/eac-eos-driver-analysis) | 2 | 2026-11-04 | Educational static notes on EOS driver |
| [hcmzah/data-ptr-comm](https://github.com/hcmzah/data-ptr-comm) | 213 | 2026-11-09 | UM/KM comms via swapped pointer (EAC/BE tagged topics) |

### Q3 noise (filtered) — typical non-RE results

Examples seen in the broad `EasyAntiCheat` search: Unity dedicated server packages, EOS plugin forks, launcher wrappers. **Not indexed** unless they contain driver teardown or PE analysis.

---

## Top hits — BattlEye (deduped)

| Repo | Stars | Updated | Description (truncated) |
|------|------:|---------|-------------------------|
| [Aki2k/BEDaisy](https://github.com/Aki2k/BEDaisy) | 152 | 2026-10-13 | Historical BEDaisy.sys RE |
| [experienceds/battleye-re](https://github.com/experienceds/battleye-re) | 9 | 2026-11-01 | BEDaisy reference / IOCTL focus |
| [gmh5225/battleye-decryption](https://github.com/gmh5225/battleye-decryption) | 0 | 2022-04-14 | BEService↔BEDaisy packet decrypt (lab) |
| [brandenbailey23/bedaisy-unload-fault-0x7e](https://github.com/brandenbailey23/bedaisy-unload-fault-0x7e) | 1 | 2026-11-01 | Minifilter unload fault analysis |
| [UnDERL1/bedaisy-reversal](https://github.com/UnDERL1/bedaisy-reversal) | 0 | 2022-04-23 | Pseudo snippets |
| [tym32167/arma3beclient](https://github.com/tym32167/arma3beclient) | 36 | 2026-07-17 | Arma 3 BEClient tool |

---

## Blocklist (logged, not curated for RE)

| Repo | Why logged |
|------|------------|
| [Deox-XD/BEDaisy-Bypass](https://github.com/Deox-XD/BEDaisy-Bypass) | Callback interception bypass source |
| [emlinhax/beservice_intcallbacks](https://github.com/emlinhax/beservice_intcallbacks) | Bypass-oriented PoC |
| [avalanchesounddepict/ac-fm-shield](https://github.com/avalanchesounddepict/ac-fm-shield) | Advertised EAC/BE bypass / spoofer stack |

---

## Tooling repos (always include in AC RE)

| Repo | Role |
|------|------|
| [morluto/rea](https://github.com/morluto/rea) | Agent-driven Ghidra/IDA MCP |
| [memflow/memflow](https://github.com/memflow/memflow) | Physical memory / VM introspection |
| [SamNZito/P_Finger_AI_Reversing](https://github.com/SamNZito/P_Finger_AI_Reversing) | Project notes sync target |

---

## Re-run locally (VM or host)

```powershell
# Requires GitHub CLI auth: gh auth login
gh search repos "EasyAntiCheat reverse engineering" --sort stars --limit 30
gh search repos "BattlEye BEDaisy" --sort updated --limit 30
gh search repos "topic:easyanticheat" --limit 30
gh search repos "topic:battleye" --sort stars --limit 30
```

Merge new `full_name` into [eac-be-research-index.md](eac-be-research-index.md); append query row to this file.