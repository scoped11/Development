# Universal — all AC IOCTL masks (lab only)

Profile: `universal` · AC: **none** (combined masks) · Profile file: `driver2/profiles/universal.json`

## Method

**Integration / stress profile** — fires **EAC + BE + ACE** improve bundles together (`--ac-replica universal`) for cheat-dev SDK testing, **not** a single production title. Use on **Host B** snapshots **without** live game AC.

## Route

```text
VM (no game AC)
  → Driver2Runner --profile universal --mode ioctl|bypass
  → or everything.ps1 -Profile universal -Target vm -Execute
```

No ranked driver3 entry — use a **title profile** for `ranked-play.ps1`.

## Read / write

Same as [payload-driver2.md](payload-driver2.md):

- `0x870` session
- `0x805` / `0x806` process
- `0x802` kernel read (lab)

Map-time may hit **0x844, 0x851, 0x852, 0x846, 0x84E, 0x869** depending on flags — see `driver2/CAPABILITIES.md`.

## Tiers

| ID | Meaning |
|----|---------|
| **T1** | VM only |
| **D1–D2** | Comm + pf_diag for universal |
| **D3** | Part of developer gate profile list |
| **D4** | N/A |
