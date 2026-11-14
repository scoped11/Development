"""Run a catalog workflow: profile, VM qualify, or installer extract."""
from __future__ import annotations

import json
import sys
from pathlib import Path

ARSENAL = Path(__file__).resolve().parent
CRACK = ARSENAL.parent / "crack"
VM_MCP = ARSENAL.parent / "vm" / "mcp"
for p in (str(CRACK), str(VM_MCP)):
    if p not in sys.path:
        sys.path.insert(0, p)

from crack_lib import (  # noqa: E402
    crack_driver_profile,
    extract_installer,
    merge_qualify_result,
    resolve_path,
)
from lab_bg import queue_bg_job  # noqa: E402

CATALOG = ARSENAL / "catalog.json"
HOST_SYS = {
    "siv": ARSENAL.parent / "audit" / "inbox" / "oem-sys" / "x64" / "SIVX64.sys",
    "sonix": ARSENAL.parent / "audit" / "inbox" / "oem-sys" / "x64" / "SONiXDDRx64.sys",
    "ioc": ARSENAL.parent / "audit" / "inbox" / "oem-sys" / "x64" / "iocbios2.sys",
}


def load_catalog() -> dict:
    return json.loads(CATALOG.read_text(encoding="utf-8"))


def run_workflow(
    name: str,
    *,
    sys_path: str = "",
    preset: str = "",
    installer: str = "",
    timeout_sec: int = 180,
    write_card: bool = True,
) -> dict:
    cat = load_catalog()
    wf = (cat.get("workflows") or {}).get(name)
    if not wf:
        return {"error": f"unknown workflow {name}", "known": list((cat.get("workflows") or {}).keys())}

    steps = wf.get("steps") or []
    out: dict = {"workflow": name, "steps": []}

    if "profile" in steps:
        path = sys_path or (str(HOST_SYS[preset]) if preset in HOST_SYS else "")
        if not path:
            return {"error": "profile needs sys_path or preset"}
        card = crack_driver_profile(path, write_card=write_card)
        out["steps"].append({"step": "profile", "sha256": card.get("sha256"), "card": card.get("card_path"), "error": card.get("error")})
        out["profile"] = {
            "name": card.get("name"),
            "sha256": card.get("sha256"),
            "byovd": card.get("byovd"),
            "devices": (card.get("device_string_candidates") or [])[:12],
            "lane": (card.get("lane") or {}).get("routing"),
            "card_path": card.get("card_path"),
        }
        if card.get("error"):
            return out

    if "qualify" in steps:
        if not preset:
            return {"error": "qualify needs preset siv|sonix|ioc"}
        ps = f"& 'Z:\\driver-lab\\vm\\bg\\guest-driver-crack-qualify.ps1' -Preset {preset}"
        code, stdout, stderr, job = queue_bg_job(
            "exec", powershell=ps, timeout_sec=timeout_sec, stall_sec=min(120, timeout_sec)
        )
        step = {"step": "qualify", "host_exit": code, "guest_exit": None, "job_id": None}
        if job:
            step["job_id"] = job.get("id")
            step["guest_exit"] = job.get("exit")
            step["stdout_tail"] = str(job.get("stdout") or "").splitlines()[-12:]
            sha = (out.get("profile") or {}).get("sha256") or ""
            if sha:
                merge_qualify_result(sha[:16], str(job.get("stdout") or ""), int(job.get("exit") or 1))
        else:
            step["stderr"] = stderr[-500:]
            step["stdout"] = stdout[-500:]
        out["steps"].append(step)

    if "extract" in steps:
        if not installer:
            return {"error": "extract_profile needs installer path"}
        extracted = extract_installer(installer)
        out["steps"].append({"step": "extract", "sys_count": extracted.get("sys_count"), "root": extracted.get("extract_root")})
        cards = []
        for row in extracted.get("sys_files") or []:
            card = crack_driver_profile(row["path"], write_card=write_card)
            cards.append(
                {
                    "name": card.get("name"),
                    "sha256": card.get("sha256"),
                    "tier": (card.get("byovd") or {}).get("tier"),
                    "card": card.get("card_path"),
                }
            )
        out["steps"].append({"step": "profile_each", "count": len(cards), "cards": cards})

    return out


def main() -> int:
    import argparse

    p = argparse.ArgumentParser(description="DriverLab arsenal workflow runner")
    p.add_argument("workflow", nargs="?", default="list")
    p.add_argument("--sys", default="")
    p.add_argument("--preset", default="")
    p.add_argument("--installer", default="")
    p.add_argument("--timeout", type=int, default=180)
    args = p.parse_args()
    if args.workflow == "list":
        print(json.dumps(load_catalog(), indent=2))
        return 0
    result = run_workflow(
        args.workflow,
        sys_path=args.sys,
        preset=args.preset,
        installer=args.installer,
        timeout_sec=args.timeout,
    )
    print(json.dumps(result, indent=2))
    return 0 if "error" not in result else 1


if __name__ == "__main__":
    raise SystemExit(main())
