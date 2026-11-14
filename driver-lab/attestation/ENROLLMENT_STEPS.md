# Attestation enrollment — step by step (with links)

Use this for **Microsoft attestation-signed** `Driver2.sys` on a **private** lab VM.  
Two tracks: **sole proprietor (no LLC)** or **LLC first** — pick one and keep the **same legal name** everywhere.

---

## Track A — Sole proprietor (no registered company)

### A1. Request D-U-N-S (free)

1. **Lookup** (optional): [D-U-N-S Lookup](https://www.dnb.com/duns-number/lookup.html) — search your **legal name** + city.
2. **Request**: [Get a D-U-N-S Number](https://www.dnb.com/duns-number.html) → **Get Started**.
3. SMB hub (background reading): [D&B D-U-N-S for small business](https://www.dnb.com/en-us/smb/duns.html).
4. On the form use:
   - **Legal name:** your **full legal name** (government ID), or sole prop as D&B labels it
   - **Type:** Sole proprietorship / Individual / Self-employed
   - **Address & phone:** real, reachable
5. Wait for email with **9-digit D-U-N-S** (free path often **~30 business days**).  
   Expedited (paid): offered during checkout on D&B site.

### A2. EV code signing certificate

1. Pick a CA and confirm they support **your case**:
   - [DigiCert — code signing hub](https://www.digicert.com/signing/code-signing-certificates)
   - [Sectigo — EV code signing](https://www.sectigo.com/ssl-certificates-tls/code-signing)
   - [GlobalSign — code signing](https://www.globalsign.com/en/code-signing-certificate)
2. Before paying, ask sales/chat: *“Sole proprietor / individual — EV for Microsoft kernel attestation — is D-U-N-S required?”*
3. Complete identity validation (ID, calls, token ship).
4. Install cert on **USB token**; note **thumbprint** in local `enrollment.json`.

**Guides**

- [Microsoft — driver signing overview](https://learn.microsoft.com/en-us/windows-hardware/drivers/install/driver-signing)
- [Microsoft — EV prerequisites (historical reference)](https://learn.microsoft.com/en-us/windows-hardware/drivers/dashboard/get-started-with-ev-signing)

### A3. Microsoft Partner Center (hardware)

1. Sign in: [Partner Center — Hardware](https://partner.microsoft.com/dashboard/hardware)
2. Create/join partner account: [Microsoft Partner Network](https://partner.microsoft.com/)
3. Enroll **Windows Hardware Partner Program** / driver signing (terms + fee if shown).
4. **Register your EV certificate** in dashboard (wizard name varies — “Certificate”, “EV Code Signing”).
5. Wait until submissions are allowed (no “EV not registered” errors).

**Docs**

- [Attestation signing a kernel driver](https://learn.microsoft.com/en-us/windows-hardware/drivers/dashboard/attestation-signing-a-kernel-driver-for-public-release)
- [Hardware Partner Center](https://learn.microsoft.com/en-us/windows-hardware/drivers/dashboard/)

### A4. Stage driver package (this repo)

```powershell
cd C:\Development

# Local file — not committed (see attestation\.gitignore)
Copy-Item driver-lab\attestation\enrollment.template.json driver-lab\attestation\enrollment.json
# Edit: legal_name, duns, business_phone, business_address, inf_manufacturer (same as legal_name)

.\driver-lab\attestation\Apply-EnrollmentToInf.ps1
.\driver-lab\attestation\Prepare-AttestationSubmit.ps1
```

Upload folder:  
`driver-lab\attestation\submit-staging\driver_package\`  
(`Driver2.sys`, `Driver2LabAttest.inf`, `.cat`)

### A5. Submit attestation

1. Partner Center → **New driver submission**
2. Choose **Attestation signing** (not full HLK/WHQL unless you intend that)
3. Upload package; INF manufacturer must match enrollment name
4. Download Microsoft-signed `.sys` / `.cat` when ready

### A6. Import to lab (private only)

```powershell
.\driver-lab\attestation\Import-AttestationPackage.ps1 -Folder <portal-download-folder>
```

Test on **DriverLab-E** snapshot only — do **not** publish binaries.

---

## Track B — Form an LLC first (cleaner for portals)

### B1. LLC (your state)

- **IRS EIN (free, online):** [Apply for an EIN](https://www.irs.gov/businesses/small-businesses-self-employed/apply-for-an-employer-identification-number-ein-online)
- LLC filing: your **Secretary of State** site (e.g. search “`[your state]` LLC formation online”).

### B2. D-U-N-S for the LLC

Same as **A1**, but legal name = **exact LLC name** on formation documents; use EIN where asked.

Then continue **A2 → A6** with the **LLC name** on EV, Partner Center, and `inf_manufacturer`.

---

## Name alignment checklist

| Place | Must match |
|-------|------------|
| D&B application | Legal name |
| EV certificate `O=` | Legal name |
| Partner Center company | Legal name |
| `enrollment.json` → `legal_name` / `inf_manufacturer` | Legal name |
| `Driver2LabAttest.inf` → `ManufacturerName` | via `Apply-EnrollmentToInf.ps1` |

---

## While D-U-N-S / EV is pending (no attestation yet)

| Goal | Doc / script |
|------|----------------|
| Lab-signed load + AC order | [OTL method](../docs/28_otl_trusted_load_method.md), `guest-otl-method.ps1` |
| BYOVD / mapper / replicas | [Test AC replicas](../../byovd/docs/15_test_ac_replicas.md) |
| Attestation overview | [SIGNING_PATH.md](../../byovd/docs/SIGNING_PATH.md) |
| Business identity notes | [BUSINESS_ENROLLMENT.md](./BUSINESS_ENROLLMENT.md) |

---

## Local files

| File | Purpose |
|------|---------|
| `enrollment.template.json` | Template |
| `enrollment.json` | **You** fill — gitignored |
| `BUSINESS_ENROLLMENT.md` | D-U-N-S / identity rules |
| `CHECKLIST.md` | Short checklist |
| `Prepare-AttestationSubmit.ps1` | Build + `inf2cat` |
| `Import-AttestationPackage.ps1` | After portal download |

---

## Support links

| Vendor | Link |
|--------|------|
| D&B customer | [D&B Customer Service](https://www.dnb.com/en-us/smb/contact-us.html) |
| Partner Center help | In-dashboard **Help** / [Partner Center support](https://partner.microsoft.com/support) |