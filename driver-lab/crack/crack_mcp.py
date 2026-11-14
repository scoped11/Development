#!/usr/bin/env python3
"""DriverLab Crack MCP — static qualify + lane matrix + VM load/open queue."""
from __future__ import annotations

import asyncio
import json
import sys
from pathlib import Path

CRACK = Path(__file__).resolve().parent
VM_MCP = CRACK.parent / "vm" / "mcp"
for p in (str(VM_MCP), str(CRACK)):
    if p not in sys.path:
        sys.path.insert(0, p)

ARSENAL = CRACK.parent / "arsenal"
if str(ARSENAL) not in sys.path:
    sys.path.insert(0, str(ARSENAL))

from crack_lib import (  # noqa: E402
    crack_driver_profile,
    extract_installer,
    find_sys_files,
    merge_qualify_result,
    resolve_path,
    write_crack_card,
)
from lab_bg import queue_bg_job, read_bg_job, bg_worker_snapshot  # noqa: E402
from arsenal_run import load_catalog, run_workflow  # noqa: E402


def pack(body: dict) -> str:
    return json.dumps(body, indent=2)


async def main():
    try:
        from mcp.server.fastmcp import FastMCP
    except ImportError:
        sys.stderr.write("pip install mcp\n")
        sys.exit(1)

    mcp = FastMCP("driverlab-crack")

    @mcp.tool()
    def crack_profile(path: str, write_card: bool = True) -> str:
        """Static BYOVD profile + device strings + paid-lane assess (no LOLDrivers as vehicle pick)."""
        return pack(crack_driver_profile(path, write_card=write_card))

    @mcp.tool()
    def crack_extract(installer_path: str, max_depth: int = 4) -> str:
        """7z recursive extract -> inventory .sys under audit/inbox/oem-extract/crack/."""
        return pack(extract_installer(installer_path, max_depth=max_depth))

    @mcp.tool()
    def crack_find_sys(directory: str, limit: int = 100) -> str:
        """List .sys under a tree with SHA256."""
        root = resolve_path(directory)
        return pack({"root": str(root), "sys": find_sys_files(root, limit=limit)})

    @mcp.tool()
    def crack_lane_matrix(path: str) -> str:
        """Explicit paid-tier routing — anti_cheat_bypass is always false."""
        prof = crack_driver_profile(path, write_card=False)
        if "error" in prof:
            return pack(prof)
        return pack({"sha256": prof["sha256"], "name": prof["name"], "lane": prof["lane"], "byovd": prof["byovd"]})

    @mcp.tool()
    def crack_qualify_vm(
        sys_path: str = r"Z:\driver-lab\audit\inbox\oem-sys\x64\SIVX64.sys",
        preset: str = "",
        timeout_sec: int = 180,
    ) -> str:
        """Queue guest load + CreateFile probe; merges result into audit/crack/*/card.json when possible."""
        prof = crack_driver_profile(sys_path, write_card=True)
        if "error" in prof:
            return pack(prof)
        if preset:
            ps = f"& 'Z:\\driver-lab\\vm\\bg\\guest-driver-crack-qualify.ps1' -Preset {preset}"
        else:
            ps = f"& 'Z:\\driver-lab\\vm\\bg\\guest-driver-crack-qualify.ps1' -SysPath '{sys_path}'"
        code, out, err, job = queue_bg_job("exec", powershell=ps, timeout_sec=timeout_sec, stall_sec=min(120, timeout_sec))
        body = {
            "host_exit": code,
            "host_stdout": out,
            "host_stderr": err,
            "guest_job": job,
            "profile_card": prof.get("card_path"),
        }
        if job and job.get("stdout") is not None:
            merged = merge_qualify_result(prof["sha256"][:16], str(job.get("stdout") or ""), int(job.get("exit", 1)))
            if merged:
                body["merged_card"] = merged.get("vm_qualify")
        return pack(body)

    @mcp.tool()
    def crack_job(job_id: str) -> str:
        """Read worker result JSON."""
        job = read_bg_job(job_id)
        return pack(job if job else {"error": "not found", "id": job_id})

    @mcp.tool()
    def crack_worker_status() -> str:
        return pack(bg_worker_snapshot())

    @mcp.tool()
    def arsenal_catalog() -> str:
        """List DriverLab arsenal workflows (profile, qualify, extract_profile)."""
        return pack(load_catalog())

    @mcp.tool()
    def arsenal_run(
        workflow: str = "profile",
        sys_path: str = "",
        preset: str = "",
        installer: str = "",
        timeout_sec: int = 180,
    ) -> str:
        """Run one catalog workflow. qualify uses guest -Preset (no hand-escaped device paths)."""
        return pack(
            run_workflow(
                workflow,
                sys_path=sys_path,
                preset=preset,
                installer=installer,
                timeout_sec=timeout_sec,
            )
        )

    await mcp.run_stdio_async()


if __name__ == "__main__":
    asyncio.run(main())
