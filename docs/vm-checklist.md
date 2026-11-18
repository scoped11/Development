# VM checklist — EAC/BE static RE (REA + Ghidra)

Sam runs **REA, Ghidra, and Cursor MCP on the guest Windows VM**. The host (this cloud agent / your daily OS) may only see the VirtualBox shared folder copy.

**Live lab VM (2026-11-18):** VirtualBox name **`AI_workspace`** (running). Docs still say `AI-Workspace` for the parity target; use the registered name in `VBoxManage` / `lab.credentials.json` until renamed.

### Hot-add session (no host shutdown — 2026-11-18)

| Step | Result |
|------|--------|
| `VBoxManage list runningvms` | **`AI_workspace`** `{ff1ce5c5-9717-45f7-9cbd-88a2e123816d}` |
| Guest Additions (host-reported) | **7.2.10 r174163**, run level **3** |
| GA ISO attach (SATA port 1) | **VBoxGuestAdditions.iso** attached |
| Transient shared folder | **`hostdev`** → `C:\Users\bj284\Downloads\TraeTesting\cursor\Development`, auto-mount **`D:\hostdev`** (added by VM UUID while GUI session held lock) |
| `sharedfolder add` on VM name alone | Failed: machine locked for GUI session |
| `controlvm … sharedfolder` (VBox 7.2.10) | Not available (invalid parameter) |
| NAT forwards (`enable-vm-access.ps1`) | **guest-ssh** `127.0.0.1:2222`, **guest-winrm** `127.0.0.1:55985` |
| `guestcontrol` (bootstrap / `rea doctor`) | Blocked: guest **execution service not ready** (VBoxTray up, **VBoxService** guest-control path not healthy) |
| Guest desktop | Logged in (**vboxuser** per unattended install + VBox log) |

**Still open on guest (VM stays on):** complete Guest Additions install from the mounted ISO so **VBoxService** starts, then confirm `\\vboxsvr\hostdev` or `D:\hostdev`, run `bootstrap-vm-tools.ps1`, then `npx rea-agents doctor`. Placeholder: [rea-doctor-guest-snapshot.txt](rea-doctor-guest-snapshot.txt).

## Paths

| Side | Path | Purpose |
|------|------|---------|
| **Host** (Windows) | `C:\Users\bj284\Downloads\TraeTesting\cursor\Development` | VirtualBox shared folder source |
| **Guest** (typical GA mount) | `D:\hostdev` or `\\VBOXSVR\hostdev` | Same tree as host Development (per [driver-lab/vm/README.md](../driver-lab/vm/README.md)) |
| **Reversing skill repo** | Host: `C:\Users\bj284\Downloads\TraeTesting\cursor\Reversing` | Clone or mount into guest if Cursor project needs skills |
| **This vault** | `D:\hostdev\docs\` | Indexes + `re-notes-*.md` |

## Sync direction

```
Host writes (Cursor agent / git pull)  →  Guest reads (REA, Ghidra)
Guest writes (re-notes, Ghidra project) →  Host reads (backup, git commit)
```

1. After host updates docs, **refresh guest view** (Guest Additions shared folder is usually immediate; if stale, restart VM or remount).
2. Large binaries: copy to guest **local disk** (e.g. `C:\re\artifacts\`) for Ghidra performance; keep hashes in `re-notes-*.md` on the share.
3. Do **not** run live EAC/BE-protected titles in this lab VM unless that is a deliberate, isolated experiment — static `.sys` RE does not require it.

## VirtualBox shared folder setup (guest)

1. Install **Guest Additions** (Devices → Insert Guest Additions CD).
2. VirtualBox Manager → VM **Settings → Shared Folders**:
   - Folder path: `C:\Users\bj284\Downloads\TraeTesting\cursor\Development`
   - Folder name: `hostdev`
   - Mount point: `D:\hostdev`
   - Auto-mount + Make Permanent: **on**
3. Guest: confirm `dir D:\hostdev\docs\eac-be-research-index.md` exists.

## REA + MCP inside guest (confirm)

Run in an **elevated** or normal PowerShell on the **guest**:

```powershell
node -v    # expect 22.19+, 24.11+, or 26+ per REA docs
npx.cmd rea-agents@latest doctor
npx.cmd rea-agents@latest setup   # select Cursor; approve MCP install
```

## Ghidra + REA (guest)

| Item | Value |
|------|--------|
| Bootstrap (guest, once) | `D:\hostdev\driver-lab\vm\bootstrap-vm-tools.ps1` |
| Install script (guest) | `D:\hostdev\driver-lab\vm\install-ghidra-rea-guest.ps1` |
| Expected Ghidra path | `C:\Tools\ghidra_12.1_PUBLIC` (override with `-InstallRoot`) |
| MCP env | `GHIDRA_INSTALL_DIR` + `JAVA_HOME` in `%USERPROFILE%\.cursor\mcp.json` — see [mcp-rea-guest.example.json](mcp-rea-guest.example.json) |
| Smoke test doc | [rea-vm-smoke-test.md](rea-vm-smoke-test.md) |

**Host vs guest paths**

```
Host:  C:\Users\bj284\Downloads\TraeTesting\cursor\Development
Guest: D:\hostdev          (wired by driver-lab\vm\wire-ai-workspace-lab.ps1)
       Z:\                  (some reboots; Invoke-GuestScript probes Z/D/Y/E/X)
       \\vboxsvr\hostdev     (UNC; always works when GA is up)
```

**VBoxManage Guest Control** needs an **interactive guest login**. If host automation fails with `VBOX_E_GSTCTL_GUEST_ERROR`, sign in to the VM once, then re-run `host-guest-exec.ps1`.

Checklist:

- [ ] Ghidra (or IDA) installed on **guest**, same machine as Cursor
- [ ] Cursor opens project from `D:\hostdev\...` or local clone
- [ ] MCP panel shows `rea` tools (`open_binary`, `procedure_pseudo_code`, …)
- [ ] `docs/re-notes-eac.md` and `docs/re-notes-be.md` open from shared folder
- [ ] Test: `open_binary` on a **non-production** PE (e.g. lab driver), not on live game EXE

Skill reference (host path; copy into guest Cursor skills if needed):

- `C:\Users\bj284\Downloads\TraeTesting\cursor\Reversing\.cursor\skills\rea-ai-reversing\SKILL.md`

## REA workflow (first EAC/BE session)

1. **Scope** in chat: "static documentation only, owned driver image, no live anti-cheat evasion."
2. Place binary: `D:\hostdev\ac-captures-host\<date>\EasyAntiCheat_EOS.sys` (or `BEDaisy.sys`).
3. Agent: `open_binary` → `inspect_pe_load_config` / `binary_overview` → name entry + IOCTL dispatch.
4. Record SHA256:

   ```powershell
   Get-FileHash D:\hostdev\ac-captures-host\<file>.sys -Algorithm SHA256
   ```

5. Move claims from **Hypothesis** → **Verified** only with addresses in notes.
6. `git add docs/re-notes-*.md` on host when satisfied.

## Stop lines (project scope)

- No kernel bypass, mapper validation against live EAC/BE, or injector testing.
- No copying cheat payloads from GitHub blocklist in [github-sources.md](github-sources.md).
- IOCTL tables in notes are for **understanding**, not operational neutralization.

## Related local docs

- [vm-driver-inventory.md](vm-driver-inventory.md)
- [rea-doctor-guest-expected.md](rea-doctor-guest-expected.md)
- [eac-be-research-index.md](eac-be-research-index.md)
- [../byovd/docs/research/16_EAC_BE_KERNEL_RESEARCH.md](../byovd/docs/research/16_EAC_BE_KERNEL_RESEARCH.md)
- [../driver-lab/vm/README.md](../driver-lab/vm/README.md)