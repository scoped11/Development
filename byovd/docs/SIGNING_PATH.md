# Attestation signing path (Phase 2)

Only load path where **Secure Boot + HVCI** and **test signing off** accept the driver as Microsoft-attestation trusted. BYOVD + stomp remain the pre-cert primary lane. On DriverLab-E, supplemental “force trusted” experiments still hit **`0xc0000428`** — use stomp/BYOVD lab until attestation is filed.

**Playbook:** `driver-lab/attestation/CHECKLIST.md` · `Prepare-AttestationSubmit.ps1` · `Import-AttestationPackage.ps1`

## Steps

1. EV code signing certificate (~$300–500/yr).
2. Register at [Partner Center Hardware](https://partner.microsoft.com/dashboard/hardware).
3. Build `Driver2.sys` (full service driver — **not** `Driver2Stomp.sys`).
4. `driver-lab\attestation\Prepare-AttestationSubmit.ps1` → upload via portal **Attestation signing**.
5. `Import-AttestationPackage.ps1` → lab VM `sc create` only on private hosts.

## AC note

Never-published attestation binaries are not on Microsoft blocklists or public hash feeds. Do not distribute `.sys`/`.cat`.

## Relation to stomp

| Lane | Cert | Vehicle | Residual |
|------|------|---------|----------|
| Stomp | No | One-shot BYOVD | Disk-vs-memory on victim |
| Attestation | Yes | None | Lowest |