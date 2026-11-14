# crack_extract

MCP: `driverlab-crack` → `crack_extract`

Extracts an installer into `driver-lab/audit/inbox/oem-extract/crack/` and returns every `.sys` with SHA256.

| Wrapper | Tool |
|---------|------|
| Zip, NSIS, cab, MSI, many EXEs | 7-Zip (`driver-lab/tools/7z-portable/7z.exe`) |
| InstallShield cabinet (`ISc(`, `data1.cab`) | unshield 1.6.2 (`driver-lab/tools/unshield/unshield.exe`) |
| Inno Setup (`Inno Setup Setup Data`) | `innounp` from winget when it is installed |

7-Zip does not unpack InstallShield `data1.cab`. The archive SHA256 for the vendored unshield build is `662fefefc6988a3744ddc6b5b8d2e96ac21876d532f7d23a597c4ca797764a6b` (ScoopInstaller Binary, `unshield-1.6.2-x64.7z`).

```powershell
python driver-lab\crack\crack_cli.py extract <installer>
```

`installer_kind` is `installshield`, `inno`, `msi`, or `plain`. `notes` is set when an InstallShield cabinet was seen and unshield did not succeed.
