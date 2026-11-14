# crack_pe_unpack

MCP: `driverlab-crack` → `crack_pe_unpack`

PE section entropy, packer string hints, stripped-import check, plus:

- executable section with raw size 0 and virtual size at least one page (`zero_raw_executable`)
- writable and executable sections (`rwx_sections`). `INIT` / `INITDATA` are listed and ignored for the plain-code warning; other RWX sections are `rwx_unexpected`
- executable section whose virtual size is at least 3× its raw size (`expanded_executable`)

Verdicts: `static_re_ok`, `high_entropy_sections`, `stripped_dense_text`, `likely_encrypted_or_packed`, `likely_packer_named`.

```powershell
python driver-lab\crack\crack_cli.py pe-unpack driver-lab\audit\inbox\oem-sys\x64\SIVX64.sys
```

This does not decrypt. When the verdict is `likely_encrypted_or_packed` or `likely_packer_named`, follow [lab_post_load_dump.md](lab_post_load_dump.md). A DriverStore copy is still the on-disk file.
