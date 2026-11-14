#!/usr/bin/env python3
"""
DriverLab Research Hub MCP — local catalog + hash intel + VM probe router.

This is NOT a password-cracking or OSINT/phishing stack. It indexes your lab
notes (LOLDrivers SHA256, resourcedrivers, audit markdown) and routes elevated
VM jobs through the same queue as driverlab-vm.

Requires: pip install mcp
"""
from __future__ import annotations

import asyncio
import json
import re
import sys
from pathlib import Path

HUB = Path(__file__).resolve().parent
VM_MCP = HUB.parent / "vm" / "mcp"
if str(VM_MCP) not in sys.path:
    sys.path.insert(0, str(VM_MCP))

from lab_bg import bg_worker_snapshot, queue_bg_job, read_bg_job  # noqa: E402
from lab_lib import (  # noqa: E402
    AUDIT,
    DEV,
    LAB,
    OEM_PRESETS,
    driver_fingerprint,
    installer_fingerprint,
    latest_greenfield_report,
    lookup_loldrivers,
    resolve_path,
    search_loldrivers_name,
    sha256_file,
)

INDEX_PATH = HUB / "index.json"
AUDIT_DOCS = sorted((AUDIT).glob("*.md"))


def pack(body: dict) -> str:
    return json.dumps(body, indent=2)


def build_index() -> dict:
    vehicles = []
    res = AUDIT / "resourcedrivers.md"
    if res.is_file():
        text = res.read_text(encoding="utf-8", errors="replace")
        for m in re.finditer(r"`([^`]+\.sys)`", text):
            vehicles.append({"driver": m.group(1), "source": "resourcedrivers.md"})
    docs = [{"path": str(p), "name": p.name} for p in AUDIT_DOCS]
    presets = list(OEM_PRESETS.keys())
    return {
        "repo": str(DEV),
        "lab_root": str(LAB),
        "audit_docs": docs,
        "vehicle_mentions": vehicles[:80],
        "oem_presets": presets,
        "disclaimer": "Hash tools = SHA256 file identity + LOLDrivers/policy lookup only.",
    }


def load_index() -> dict:
    if INDEX_PATH.is_file():
        try:
            return json.loads(INDEX_PATH.read_text(encoding="utf-8"))
        except json.JSONDecodeError:
            pass
    return build_index()


def search_docs(query: str, limit: int = 8) -> list[dict]:
    q = query.lower()
    hits: list[dict] = []
    for path in AUDIT_DOCS:
        try:
            lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
        except OSError:
            continue
        for i, line in enumerate(lines):
            if q in line.lower():
                start = max(0, i - 1)
                end = min(len(lines), i + 2)
                hits.append(
                    {
                        "file": path.name,
                        "path": str(path),
                        "line": i + 1,
                        "snippet": "\n".join(lines[start:end]),
                    }
                )
                if len(hits) >= limit:
                    return hits
    return hits


async def main():
    try:
        from mcp.server.fastmcp import FastMCP
    except ImportError:
        sys.stderr.write("pip install mcp\n")
        sys.exit(1)

    mcp = FastMCP("driverlab-research")

    @mcp.tool()
    def hub_catalog(refresh: bool = False) -> str:
        """List audit docs, OEM VM presets, and where lab data lives."""
        idx = build_index() if refresh else load_index()
        if refresh:
            INDEX_PATH.write_text(json.dumps(idx, indent=2), encoding="utf-8")
        return pack(idx)

    @mcp.tool()
    def hub_search(query: str, limit: int = 8) -> str:
        """Full-text search across driver-lab/audit/*.md (VM results, IOCTL notes, OEM status)."""
        return pack({"query": query, "hits": search_docs(query, limit=limit)})

    @mcp.tool()
    def hub_hash_lookup(path: str, include_loldrivers: bool = False) -> str:
        """SHA256 + version (+ optional LOLDrivers). Use driverlab-crack crack_profile for qualification."""
        p = resolve_path(path)
        if not p.is_file():
            return pack({"error": f"not found: {path}"})
        return pack(driver_fingerprint(str(p), include_loldrivers=include_loldrivers))

    @mcp.tool()
    def hub_loldrivers_name(driver_name: str = "SIVX64.sys") -> str:
        """LOLDrivers entries by filename when your SHA differs from cataloged builds (e.g. SIV 5.88 vs 5.85)."""
        return pack({"pattern": driver_name, "hits": search_loldrivers_name(driver_name)})

    @mcp.tool()
    def hub_installer_inspect(path: str) -> str:
        """PE/ZIP/NSIS/InstallShield fingerprint (catches HTML masquerading as HWiNFO zip)."""
        p = resolve_path(path)
        if not p.is_file():
            return pack({"error": f"not found: {path}"})
        info = installer_fingerprint(p)
        if p.suffix.lower() in {".sys", ".dll", ".exe"} and "sha256" not in info:
            info["sha256"] = sha256_file(p)
        return pack(info)

    @mcp.tool()
    def hub_oem_run(preset: str = "siv_phys", timeout_sec: int | None = None) -> str:
        """Queue elevated guest OEM script (same worker as driverlab-vm)."""
        key = preset.strip().lower()
        if key not in OEM_PRESETS:
            return pack({"error": f"unknown preset; use hub_catalog for oem_presets"})
        cfg = OEM_PRESETS[key]
        t = int(timeout_sec) if timeout_sec else int(cfg["timeout_sec"])
        ps = f"& '{cfg['script']}'"
        if cfg.get("powershell_args"):
            ps += " " + str(cfg["powershell_args"])
        code, out, err, job = queue_bg_job("exec", powershell=ps, timeout_sec=t, stall_sec=int(cfg["stall_sec"]))
        return pack(
            {
                "host_exit": code,
                "host_stdout": out,
                "host_stderr": err,
                "guest_job": job,
                "preset": key,
            }
        )

    @mcp.tool()
    def hub_job(job_id: str) -> str:
        """Read driver-lab/vm/bg/out/{id}.json."""
        job = read_bg_job(job_id)
        return pack(job if job else {"error": "not found", "id": job_id})

    @mcp.tool()
    def hub_worker_status() -> str:
        """Worker heartbeat + recent job ids."""
        return pack(bg_worker_snapshot())

    @mcp.tool()
    def hub_greenfield() -> str:
        """Latest greenfield JSON or log tail from VM share."""
        return pack(latest_greenfield_report())

    @mcp.tool()
    def hub_vehicle_flags() -> str:
        """mapper_nal --driver flags from byovd vehicles docs."""
        vdoc = DEV / "byovd" / "driver3" / "drivers" / "vehicles.md"
        if not vdoc.is_file():
            return pack({"error": "vehicles.md missing"})
        return pack({"path": str(vdoc), "content": vdoc.read_text(encoding="utf-8", errors="replace")[:12000]})

    await mcp.run_stdio_async()


if __name__ == "__main__":
    asyncio.run(main())
