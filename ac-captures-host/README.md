# Anti-cheat artifact staging (offline)

Place **owned** captures here for static RE on the guest. Keep large binaries on guest local disk (`C:\re\artifacts\`); store SHA256 in `docs/re-notes-*.md`.

Example layout:

```
ac-captures-host/2026-11-17/EasyAntiCheat_EOS.sys
ac-captures-host/2026-11-17/BEDaisy.sys
ac-captures-host/smoke/notepad-copy.exe   # optional pipeline test
```

Do not commit proprietary game binaries unless policy allows.