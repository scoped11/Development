#!/usr/bin/env python3
"""stdio MCP: DriverLab guest via host-guest-exec.ps1 (VirtualBox Guest Additions)."""
from __future__ import annotations

import asyncio
import json
import os
import subprocess
import sys
from pathlib import Path

from mcp_stdio_bootstrap import configure_stdio_logging

configure_stdio_logging()

ROOT = Path(__file__).resolve().parents[1]
BG = ROOT / "bg"
EXEC = ROOT / "host-guest-exec.ps1"
HOST_BG = BG / "host-bg-admin.ps1"
VM_NAME = os.environ.get("DRIVERLAB_VM", "DriverLab-E-26200")
DEFAULT_TIMEOUT = 120_000

try:
    from lab_lib import (
        LAB,
        OEM_PRESETS,
        driver_fingerprint,
        latest_greenfield_report,
    )
except ImportError:
    _mcp_dir = Path(__file__).resolve().parent
    if str(_mcp_dir) not in sys.path:
        sys.path.insert(0, str(_mcp_dir))
    from lab_lib import (
        LAB,
        OEM_PRESETS,
        driver_fingerprint,
        latest_greenfield_report,
    )

GUEST_PS = r"C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe"
SETUP_CANDIDATES = [
    r"Z:\driver-lab\vm\setup-guest-remote.ps1",
    r"D:\hostdev\driver-lab\vm\setup-guest-remote.ps1",
]


def run_host_ps(script: Path, extra_args: list[str] | None = None, timeout_s: int = 600) -> tuple[int, str, str]:
    args = [
        "powershell",
        "-NoProfile",
        "-ExecutionPolicy",
        "Bypass",
        "-File",
        str(script),
    ]
    if extra_args:
        args.extend(extra_args)
    proc = subprocess.run(args, capture_output=True, text=True, cwd=str(ROOT), timeout=timeout_s)
    return proc.returncode, proc.stdout, proc.stderr


def run_guest(command: list[str], timeout_ms: int = DEFAULT_TIMEOUT, vm_name: str | None = None) -> tuple[int, str, str]:
    vm = vm_name or VM_NAME
    inner = ",".join(repr(c) for c in command)
    ps_cmd = (
        f"& '{EXEC}' -VmName '{vm}' -TimeoutMs {timeout_ms} "
        f"-Command @({inner})"
    )
    proc = subprocess.run(
        [
            "powershell",
            "-NoProfile",
            "-ExecutionPolicy",
            "Bypass",
            "-Command",
            ps_cmd,
        ],
        capture_output=True,
        text=True,
        cwd=str(ROOT),
    )
    return proc.returncode, proc.stdout, proc.stderr


def pack_result(code: int, out: str, err: str, **extra) -> str:
    body = {"exit": code, "stdout": out, "stderr": err, "vm": VM_NAME}
    body.update(extra)
    return json.dumps(body, indent=2)


from lab_bg import bg_worker_snapshot, queue_bg_job, read_bg_job  # noqa: E402


async def main():
    try:
        from mcp.server.fastmcp import FastMCP
    except ImportError:
        sys.stderr.write("pip install mcp  # or: pip install -r requirements.txt\n")
        sys.exit(1)

    mcp = FastMCP("driverlab-vm")

    @mcp.tool()
    def vm_ensure_admin() -> str:
        """Headless: disable guest UAC via offline VDI patch if needed, then register elevated workers. No RDP."""
        script = ROOT / "ensure-guest-admin.ps1"
        code, out, err = run_host_ps(script, timeout_s=900)
        return pack_result(code, out, err)

    @mcp.tool()
    def vm_exec_admin(powershell_command: str, timeout_ms: int = DEFAULT_TIMEOUT, stall_sec: int = 120) -> str:
        """Elevated guest PowerShell via background worker (no RDP). Returns host wait output + guest job JSON."""
        t = max(30, timeout_ms // 1000)
        code, out, err, job = queue_bg_job(
            "exec",
            powershell=powershell_command,
            timeout_sec=t,
            stall_sec=stall_sec,
        )
        return pack_result(code, out, err, guest_job=job)

    @mcp.tool()
    def vm_bg_job(job_id: str) -> str:
        """Read elevated worker result JSON from driver-lab/vm/bg/out/{id}.json (host share)."""
        job = read_bg_job(job_id)
        if job is None:
            return pack_result(2, "", f"no result for id={job_id}")
        return json.dumps(job, indent=2)

    @mcp.tool()
    def vm_bg_worker_status(log_tail_lines: int = 15) -> str:
        """Heartbeat + worker.log tail + last five job ids (host Z: share)."""
        return json.dumps(bg_worker_snapshot(tail=log_tail_lines), indent=2)

    @mcp.tool()
    def vm_bg_run_script(
        script_z_path: str = r"Z:\driver-lab\vm\bg\guest-siv-open-privilege.ps1",
        timeout_sec: int = 240,
        stall_sec: int = 120,
    ) -> str:
        """Queue an elevated guest -File script on the VM share; waits for worker result JSON."""
        ps = f"& '{script_z_path}'"
        code, out, err, job = queue_bg_job("exec", powershell=ps, timeout_sec=timeout_sec, stall_sec=stall_sec)
        return pack_result(code, out, err, guest_job=job, script=script_z_path)

    @mcp.tool()
    def vm_siv_open_probe(timeout_sec: int = 200) -> str:
        """Elevated guest: load SIVX64, enable SeLoadDriverPrivilege, try CreateFile on SIVDRIVER."""
        ps = r"& 'Z:\driver-lab\vm\bg\guest-siv-open-privilege.ps1'"
        code, out, err, job = queue_bg_job("exec", powershell=ps, timeout_sec=timeout_sec, stall_sec=90)
        return pack_result(code, out, err, guest_job=job)

    @mcp.tool()
    def vm_siv_phys_smoke(timeout_sec: int = 120) -> str:
        """Elevated guest: 16-byte physical read at PA 0x1000 via SIV IOCTL 0x10. No write, no mapper."""
        ps = r"& 'Z:\driver-lab\vm\bg\guest-siv-phys-smoke.ps1'"
        code, out, err, job = queue_bg_job("exec", powershell=ps, timeout_sec=timeout_sec, stall_sec=75)
        return pack_result(code, out, err, guest_job=job)

    @mcp.tool()
    def vm_siv_diagnose(timeout_sec: int = 120) -> str:
        """Guest: SIVDriver service state, privs, CreateFile with/without SIV64X running."""
        ps = r"& 'Z:\driver-lab\vm\bg\guest-siv-diagnose.ps1'"
        code, out, err, job = queue_bg_job("exec", powershell=ps, timeout_sec=timeout_sec, stall_sec=75)
        return pack_result(code, out, err, guest_job=job)

    @mcp.tool()
    def vm_bg_kill_stuck() -> str:
        """Queue guest-kill-stuck-mapper.ps1 on the elevated worker (unstick hung greenfield jobs)."""
        ps = r"& 'Z:\driver-lab\vm\bg\guest-kill-stuck-mapper.ps1'"
        code, out, err, job = queue_bg_job("exec", powershell=ps, timeout_sec=90, stall_sec=45)
        return pack_result(code, out, err, guest_job=job)

    @mcp.tool()
    def lab_driver_fingerprint(path: str, include_loldrivers: bool = False) -> str:
        """Host: SHA256 + version for .sys; optional LOLDrivers. Prefer driverlab-crack crack_profile for vehicles."""
        return json.dumps(driver_fingerprint(path, include_loldrivers=include_loldrivers), indent=2)

    @mcp.tool()
    def lab_crack_profile(path: str, write_card: bool = True) -> str:
        """Static BYOVD card + paid-lane assess (no AC bypass)."""
        crack_dir = Path(__file__).resolve().parents[2] / "crack"
        if str(crack_dir) not in sys.path:
            sys.path.insert(0, str(crack_dir))
        from crack_lib import crack_driver_profile  # noqa: WPS433

        return json.dumps(crack_driver_profile(path, write_card=write_card), indent=2)

    @mcp.tool()
    def lab_oem_presets() -> str:
        """List preset names for lab_oem_run (siv_open, siv_phys, load_open_smoke, driverstore, greenfield, giga_extract)."""
        return json.dumps(OEM_PRESETS, indent=2)

    @mcp.tool()
    def lab_oem_run(
        preset: str = "siv_phys",
        timeout_sec: int | None = None,
        stall_sec: int | None = None,
    ) -> str:
        """One-shot elevated guest OEM probe. Waits for bg/out job JSON. Use lab_oem_presets for names."""
        key = preset.strip().lower()
        if key not in OEM_PRESETS:
            return pack_result(2, "", f"unknown preset {preset!r}; see lab_oem_presets")
        cfg = OEM_PRESETS[key]
        t = timeout_sec if timeout_sec is not None else int(cfg["timeout_sec"])
        s = stall_sec if stall_sec is not None else int(cfg["stall_sec"])
        ps = f"& '{cfg['script']}'"
        if cfg.get("powershell_args"):
            ps += " " + str(cfg["powershell_args"])
        code, out, err, job = queue_bg_job("exec", powershell=ps, timeout_sec=t, stall_sec=s)
        return pack_result(code, out, err, guest_job=job, preset=key, blurb=cfg.get("blurb"))

    @mcp.tool()
    def lab_greenfield_report() -> str:
        """Read latest greenfield_research_vm.json or log tail from Z: share (no VM round-trip)."""
        return json.dumps(latest_greenfield_report(), indent=2)

    @mcp.tool()
    def lab_pull_stripped(hash_prefix: str = "") -> str:
        """List stripped exports under audit/inbox/oem-sys/from-vm (optional hash dir prefix)."""
        root = LAB / "audit" / "inbox" / "oem-sys" / "from-vm"
        if not root.is_dir():
            return json.dumps({"error": "from-vm missing", "path": str(root)})
        dirs = sorted(root.iterdir(), key=lambda p: p.stat().st_mtime, reverse=True)
        if hash_prefix:
            dirs = [d for d in dirs if d.is_dir() and d.name.lower().startswith(hash_prefix.lower())]
        rows = []
        for d in dirs[:20]:
            if not d.is_dir():
                continue
            manifest = d / "manifest.json"
            sys_files = list(d.glob("*.sys"))
            rows.append(
                {
                    "dir": d.name,
                    "manifest": manifest.is_file(),
                    "sys": [f.name for f in sys_files],
                }
            )
        return json.dumps({"root": str(root), "exports": rows}, indent=2)

    @mcp.tool()
    def vm_bg_whoami() -> str:
        """Confirm the looping elevated worker is admin. No RDP, no guest UI."""
        script = ROOT / "bg" / "host-bg-admin.ps1"
        code, out, err = run_host_ps(script, extra_args=["-Action", "whoami", "-TimeoutSec", "60"], timeout_s=90)
        return pack_result(code, out, err)

    @mcp.tool()
    def vm_map_ts() -> str:
        """VM-only ThrottleStop vehicle: load TsSvc, open \\\\.\\TsSvc, map RwTest.sys. No host kernel."""
        script = ROOT / "bg" / "host-bg-admin.ps1"
        code, out, err = run_host_ps(script, extra_args=["-Action", "map_ts", "-TimeoutSec", "180"], timeout_s=240)
        return pack_result(code, out, err)

    @mcp.tool()
    def vm_probe_vehicles() -> str:
        """Elevated RwTest map probe (ts/corsair/rtcore/siv/nal) inside the VM only."""
        script = ROOT / "run-vm-vehicle-probe-from-host.ps1"
        ensure = ROOT / "ensure-guest-admin.ps1"
        code, out, err = run_host_ps(ensure, timeout_s=900)
        if code != 0:
            return pack_result(code, out, err)
        code, out, err = run_host_ps(script, timeout_s=600)
        return pack_result(code, out, err)

    @mcp.tool()
    def vm_exec(powershell_command: str, timeout_ms: int = DEFAULT_TIMEOUT) -> str:
        """Run PowerShell -Command inside the lab guest (Guest Additions; admin after vm_ensure_admin)."""
        code, out, err = run_guest(
            [GUEST_PS, "-NoProfile", "-Command", powershell_command],
            timeout_ms=timeout_ms,
        )
        return pack_result(code, out, err)

    @mcp.tool()
    def vm_hostname(timeout_ms: int = 60_000) -> str:
        """Quick GA health check — should print driverlab-e."""
        return vm_exec("hostname", timeout_ms=timeout_ms)

    @mcp.tool()
    def vm_driver3_smoke(profile: str = "rust") -> str:
        """Run driver3 plan smoke on Z:\\byovd\\driver3\\vm-guest-smoke.ps1 (no kernel map)."""
        script = r"Z:\byovd\driver3\vm-guest-smoke.ps1"
        code, out, err = run_guest(
            [
                GUEST_PS,
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                script,
                "-Profile",
                profile,
            ],
            timeout_ms=180_000,
        )
        return pack_result(code, out, err)

    @mcp.tool()
    def vm_driver3_lane_gate(profile: str = "rust") -> str:
        """Run driver3 lane-gate.ps1 for a title profile."""
        script = r"Z:\byovd\driver3\lane-gate.ps1"
        code, out, err = run_guest(
            [
                GUEST_PS,
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                script,
                "-Game",
                profile,
            ],
            timeout_ms=120_000,
        )
        return pack_result(code, out, err)

    @mcp.tool()
    def vm_driver3_request_hostb(
        profile: str = "rust",
        skip_prove: bool = True,
        force_map: bool = True,
    ) -> str:
        """Queue driver3 Host B map via Driver3ElevatedWorker (register task once in guest)."""
        script = r"Z:\byovd\driver3\vm-request-hostb.ps1"
        flags = []
        if force_map:
            flags.append("-ForceMap")
        if skip_prove:
            flags.append("-SkipProve")
        code, out, err = run_guest(
            [
                GUEST_PS,
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                script,
                "-Profile",
                profile,
            ]
            + flags,
            timeout_ms=120_000,
        )
        return pack_result(code, out, err)

    @mcp.tool()
    def host_driver3_developer_nod(all_titles: bool = True, skip_vm: bool = False) -> str:
        """Run developer-nod.ps1 on the host (respect checklist → developer_nod.json)."""
        dev = Path(__file__).resolve().parents[3]
        script = dev / "byovd" / "driver3" / "developer-nod.ps1"
        args = [
            "powershell",
            "-NoProfile",
            "-ExecutionPolicy",
            "Bypass",
            "-File",
            str(script),
        ]
        if all_titles:
            args.append("-AllTitles")
        if skip_vm:
            args.append("-SkipVm")
        proc = subprocess.run(args, capture_output=True, text=True, cwd=str(dev))
        return pack_result(proc.returncode, proc.stdout, proc.stderr)

    @mcp.tool()
    def host_driver3_s_tier(all_titles: bool = True, vm_map: bool = False) -> str:
        """Run reach-scale-s.ps1 on the host → s_tier.json."""
        dev = Path(__file__).resolve().parents[3]
        script = dev / "byovd" / "driver3" / "reach-scale-s.ps1"
        args = [
            "powershell",
            "-NoProfile",
            "-ExecutionPolicy",
            "Bypass",
            "-File",
            str(script),
        ]
        if all_titles:
            args.append("-AllTitles")
        if vm_map:
            args.append("-VmMap")
        else:
            args.append("-SkipVm")
        proc = subprocess.run(args, capture_output=True, text=True, cwd=str(dev))
        return pack_result(proc.returncode, proc.stdout, proc.stderr)

    @mcp.tool()
    def host_driver3_live_ready(profile: str = "rust") -> str:
        """Run Test-LiveReady.ps1 on the host for a title profile."""
        dev = Path(__file__).resolve().parents[3]
        script = dev / "byovd" / "driver3" / "Test-LiveReady.ps1"
        proc = subprocess.run(
            [
                "powershell",
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                str(script),
                "-Game",
                profile,
            ],
            capture_output=True,
            text=True,
            cwd=str(dev),
        )
        return pack_result(proc.returncode, proc.stdout, proc.stderr)

    @mcp.tool()
    def vm_bootstrap() -> str:
        """Run setup-guest-remote.ps1 in guest (SSH/WinRM; uses Z: or D:\\hostdev share)."""
        setup = None
        for candidate in SETUP_CANDIDATES:
            code, out, _ = run_guest(
                [GUEST_PS, "-NoProfile", "-Command", f"Test-Path '{candidate}'"],
                timeout_ms=30_000,
            )
            if code == 0 and "True" in out:
                setup = candidate
                break
        if not setup:
            return pack_result(
                1,
                "",
                f"No setup script on guest. Tried: {SETUP_CANDIDATES}. Mount Development share as Z:.",
            )
        code, out, err = run_guest(
            [
                GUEST_PS,
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                setup,
            ],
            timeout_ms=600_000,
        )
        return pack_result(code, out, err)

    await mcp.run_stdio_async()


if __name__ == "__main__":
    asyncio.run(main())
