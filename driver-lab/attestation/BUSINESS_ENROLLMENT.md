# Business identity for EV + Partner Center (real records only)

Microsoft and CAs verify **real** organizations. We **do not** invent D-U-N-S, EIN, or legal names in this repo.

## What you need

| Item | How to get it | Store in repo |
|------|----------------|---------------|
| **Legal entity name** | Your LLC/corp registration or sole-prop legal name | `enrollment.json` → use after you choose |
| **D-U-N-S (9 digits)** | Free request at Dun & Bradstreet | `enrollment.json` → `duns` |
| **Business address** | Matches registration + utility/bank | CA + Partner Center forms |
| **EIN** (US org) | IRS if you have an LLC/corp | CA form only — **not** in git |
| **Phone** | Listed business or registered agent | CA validation call |

## Get a D-U-N-S (free)

1. https://www.dnb.com/duns-number/lookup.html — check if you already have one.
2. If none: https://www.dnb.com/duns-number/get-a-duns.html — **Request a D-U-N-S Number** (free for standard wait).
3. Use the **exact legal name** you will put on the EV cert.
4. Processing: often **~30 business days** free; paid expedite optional.

After D&B assigns a number, copy it to `enrollment.json` (local only; add `enrollment.json` to `.gitignore`).

## Name alignment

These must match **as closely as portals allow**:

- D&B legal name  
- EV certificate `Organization (O)=`  
- Partner Center company profile  
- `Driver2LabAttest.inf` → `ManufacturerName=` (edit before `Prepare-AttestationSubmit.ps1`)

## Sole proprietor / individual

Some CAs issue **individual** EV with government ID instead of D-U-N-S. Partner Center may still require business verification — follow Microsoft’s wizard. Do not fake a shell company.

## Local enrollment file

```powershell
Copy-Item driver-lab\attestation\enrollment.template.json driver-lab\attestation\enrollment.json
# Edit: legal_name, duns, ev_*, partner_center_email
```