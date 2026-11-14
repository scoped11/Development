# SONiXDDRx64 + iocbios2 — IDA (host) / validate on VM

Host static analysis (sessions `951aed9d`, `68aefd67`). VM load/open/DeviceIoControl belongs in `guest-greenfield-research-order.ps1`.

## SONiXDDRx64.sys

| Field | Value |
|--------|--------|
| SHA256 | `85a4ce446d9ccd93c2f14d2e0f30ea673812ce740438abb199a8047510614a76` |
| Device | `\\.\SONiXDDR_0_0_0_1` (`\Device\SONiXDDR_0_0_0_1`) |
| SDDL | `D:P(A;;GA;;;SY)(A;;GA;;;BA)` — admin open |
| LOLDrivers | Listed (sonixddrx64.sys) |

**Phys primitive** — `sub_1400013B0` via IOCTL **`0x9C406684`** (-1673501052):

- Input 16 bytes: `u64 phys` @+0, `u32 width` @+8 (1/2/8), `u32 count` @+12  
- Phys must be **≥ 0xC0000** and **phys+size ≤ 0xFFFFF** (first megabyte only — not full RAM)  
- `MmMapIoSpace` → copy to user buffer → unmap (not persistent map)

Same IOCTL family as **MyPortIO** (`0x9C406680`/`684`) — Polychrome bundle sibling.

## iocbios2.sys (Intel XTU component)

| Field | Value |
|--------|--------|
| SHA256 | `0cc564c334d2492e69706acf09986f1cfa125f29cb8cc4b7c3be0f41c02cd60d` |
| FileVer | 7.13.1.5 (inf in ASRock MBU) |
| Device | `\\.\iocbios2` |
| SDDL | `D:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;GR;;;WD)(A;;GR;;;RC)` |
| MS / LOL | Not in local blocklist grep; not in LOL CSV |

**Not a naive phys-map IOCTL vehicle.** `IRP_MJ_DEVICE_CONTROL` = `sub_140001290` — Intel XTU mailbox/PCI init (`sub_140002AC4` maps one PCI BAR via `MmMapIoSpace` for board bring-up). IOCTLs use `0x89FFE4xx` / `0x89FFE3xx` family (buffered, size caps 0xC00). Requires **`sub_140009598`** session gate + often **XTU services** — expect bare `sc start` load OK but IOCTL path needs companion stack.

## VM workflow (vault)

1. `vm\vbox\resize-driverlab-storage.ps1` — 128 GB **DriverLab-Vault.vdi** on SATA port 1 → **D:**  
2. `guest-vault-init.ps1` — `D:\installers`, `D:\drivers`, …  
3. `guest-greenfield-research-order.ps1` — downloads/tests on VM; **`guest-export-stripped-driver.ps1`** → `audit\inbox\oem-sys\from-vm\<hash>\`  
4. Host: `scripts\pull-vm-stripped-drivers.ps1` → `oem-sys\x64` for IDA only  

Full zip/installers stay on **D:**; host sees **.sys + manifest.json** only.
