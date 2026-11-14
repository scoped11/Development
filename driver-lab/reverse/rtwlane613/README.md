# rtwlane613.sys — clone reverse (pass 1)

| Field | Value |
|-------|--------|
| Source | `C:\WINDOWS\system32\drivers\rtwlane613.sys` |
| Service | Realtek Wireless LAN 802.11ax PCI-E |
| Device (unicode) | `\Device\Realtek_Wifi_Device` → `\\.\Realtek_Wifi_Device` |
| LOLDrivers | no (this SHA) |

**Rules:** static only — no `sc start`, no service install.

## Clone layout

```
bin/rtwlane613.sys    # working copy for IDA (.i64 beside it)
notes/                # pass findings
```

## Pass checklist

- [ ] `survey_binary` — imports, strings, entry
- [ ] IOCTL / WDF dispatcher
- [ ] `MmMapIoSpaceEx` call chain — user-influenced phys?
- [ ] Device ACL / who can open `\\.\Realtek_Wifi_Device`
- [ ] Verdict: vehicle candidate Y/N

## Queue

1. ~~rtwlane613~~ (done)
2. ~~TeeDriverW10x64~~ (done — see `../TeeDriverW10x64/notes/PASS1.md`)
3. **Next:** `../iaStorVD/` (not started)
