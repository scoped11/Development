"""Elevated worker queue (host-bg-admin.ps1) — shared by driverlab-vm and research hub."""
from __future__ import annotations

import json
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BG = ROOT / "bg"
HOST_BG = BG / "host-bg-admin.ps1"


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


def read_bg_job(job_id: str) -> dict | None:
    path = BG / "out" / f"{job_id}.json"
    if not path.is_file():
        return None
    try:
        return json.loads(path.read_text(encoding="utf-8-sig"))
    except json.JSONDecodeError:
        return {"id": job_id, "parse_error": True, "raw": path.read_text(encoding="utf-8-sig", errors="replace")}


def queue_bg_job(
    action: str,
    *,
    powershell: str | None = None,
    timeout_sec: int = 180,
    stall_sec: int = 120,
    poll_sec: int = 10,
    vm_name: str | None = None,
) -> tuple[int, str, str, dict | None]:
    extra = ["-Action", action, "-TimeoutSec", str(timeout_sec), "-PollSec", str(poll_sec), "-StallSec", str(stall_sec)]
    if vm_name:
        extra.extend(["-VmName", vm_name])
    if powershell:
        extra.extend(["-PowerShell", powershell])
    code, out, err = run_host_ps(HOST_BG, extra_args=extra, timeout_s=timeout_sec + 60)
    job = None
    for line in out.splitlines():
        if "id=" in line and "queued" in line:
            try:
                frag = line.split("id=", 1)[1].split()[0]
                job = read_bg_job(frag)
            except IndexError:
                pass
    if job is None and code == 0 and out.strip().startswith("{"):
        try:
            job = json.loads(out)
        except json.JSONDecodeError:
            pass
    return code, out, err, job


def bg_worker_snapshot(tail: int = 12) -> dict:
    snap: dict = {"paths": {"heartbeat": str(BG / "worker.heartbeat"), "worker_log": str(BG / "worker.log")}}
    hb = BG / "worker.heartbeat"
    if hb.is_file():
        snap["heartbeat_utc"] = hb.read_text(encoding="utf-8", errors="replace").strip()
    wl = BG / "worker.log"
    if wl.is_file():
        lines = wl.read_text(encoding="utf-8", errors="replace").splitlines()
        snap["worker_log_tail"] = lines[-tail:] if len(lines) > tail else lines
    out_dir = BG / "out"
    if out_dir.is_dir():
        results = sorted(out_dir.glob("*.json"), key=lambda p: p.stat().st_mtime, reverse=True)[:5]
        snap["recent_jobs"] = [p.stem for p in results]
    return snap
