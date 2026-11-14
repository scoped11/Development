# Attestation signing — what to complete (you fill in forms)

**Full walkthrough with links:** [ENROLLMENT_STEPS.md](./ENROLLMENT_STEPS.md) (sole prop + LLC tracks).

## Before the portal

| Step | Action | Notes |
|------|--------|--------|
| 1 | **EV code signing certificate** | DigiCert, Sectigo, GlobalSign, etc. — org/individual identity verification |
| 2 | **Register hardware program** | [Partner Center → Hardware](https://partner.microsoft.com/dashboard/hardware) (browser tab) |
| 3 | **Sign in** | Same Microsoft work/school or MSA you will bind to Partner Center |
| 4 | **Legal entity** | Company name + address matching EV cert, or sole prop per CA rules |

## EV cert (buy / renew)

Typical fields you will fill:

- Organization legal name (match future Partner Center profile)
- D-U-N-S number (often required for org EV)
- Business address, phone
- Authorized signer contact email
- Token/USB HSM delivery address (many CAs ship hardware for EV)

**Do not** use the lab offline `DriverLabOffline.pfx` for portal submission — attestation requires **your EV** key.

## Partner Center hardware enrollment

On [hardware dashboard](https://partner.microsoft.com/dashboard/hardware):

1. Accept program terms / **Windows Hardware Partner Program**
2. **Associate your EV certificate** (upload CSR or link cert after installation)
3. Complete **identity verification** (Microsoft may review org)
4. Note your **Publisher ID** / hardware account email for receipts in `attestation\enrollment.json`

## After approval (repo scripts — later)

```powershell
# Host, after you download Microsoft-signed package from portal:
driver-lab\attestation\Import-AttestationPackage.ps1 -Folder <path-to-signed-drop>
driver-lab\attestation\Prepare-AttestationSubmit.ps1   # before first submit only
```

## Submit policy (private lab)

- One **release** build per submission (`Driver2.sys` service driver, not `Driver2Stomp.sys`)
- Never publish `.sys`/`.cat` — avoids LOLDrivers / AC hash feeds
- Test only on **DriverLab-E** snapshot; rotate after any leak

## Links (open as needed)

| Resource | URL |
|----------|-----|
| Hardware dashboard | https://partner.microsoft.com/dashboard/hardware |
| Driver signing requirements | https://learn.microsoft.com/en-us/windows-hardware/drivers/install/driver-signing |
| Attestation signing | https://learn.microsoft.com/en-us/windows-hardware/drivers/dashboard/attestation-signing-a-kernel-driver-for-public-release |
| Create an EV cert | Your CA vendor portal (DigiCert, Sectigo, …) |