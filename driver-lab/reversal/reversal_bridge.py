"""
Host-side reversal workbench — ties crack cards, PE unpack, VM qualify, and IDA MCP.

Runs on the main PC (heavy static RE, full disk, headless IDA). The lab VM only
loads/opens and supplies dumps; it does not host IDA.

Lab / T&E only — not anti-cheat bypass.
"""
from __future__ import annotations

import json
import sys
from datetime import datetime, timezone
from pathlib import Path

CRACK = Path(__file__).resolve().parent.parent / "crack"
if str(CRACK) not in sys.path:
    sys.path.insert(0, str(CRACK))

from crack_lib import (  # noqa: E402
    CRACK_ROOT,
    crack_driver_profile,
    pe_compare_disk_dump,
    pe_fixup_mapped_for_ida,
    pe_unpack_assess,
    resolve_path,
    stage_binary_for_ida,
)
from dump_registry import (  # noqa: E402
    link_manifest_to_ticket,
    pick_best_dump,
    register_memory_dump,
    scan_all,
)

PACKED_VERDICTS = frozenset(
    {"likely_encrypted_or_packed", "likely_packer_named", "pe_truncated", "bad_pe_signature"}
)

IDA_TOOL_ORDER = [
    {"tool": "idb_open", "args": {"mode": "prefer_headless", "init_hexrays": True}},
    {"tool": "survey_binary", "args": {}},
    {"tool": "search_text", "hint": "MmMapIoSpace, IoCreateDevice, IRP_MJ_DEVICE_CONTROL"},
    {"tool": "find_regex", "hint": r"\\\\\.\\\\|\\\\Device\\\\"},
    {"tool": "decompile", "hint": "DriverEntry and IRP_MJ_DEVICE_CONTROL dispatch"},
    {"tool": "xrefs_to", "hint": "ioctl switch / MajorFunction table"},
]


def infer_qualify_preset(profile: dict) -> str | None:
    name = (profile.get("name") or "").upper()
    for token, preset in (
        ("SIV", "siv"),
        ("SONIX", "sonix"),
        ("IOC", "ioc"),
    ):
        if token in name:
            return preset
    devices = " ".join(profile.get("device_string_candidates") or [])
    if "SIVDRIVER" in devices.upper():
        return "siv"
    if "SONIX" in devices.upper():
        return "sonix"
    if "iocbios" in devices.lower():
        return "ioc"
    return None


def write_ida_ticket(
    profile: dict,
    *,
    ida_input: str,
    route: str,
    pe_verdict: str,
    compare: dict | None = None,
    fixup: dict | None = None,
    extra: dict | None = None,
) -> Path:
    sha = profile["sha256"]
    dest = CRACK_ROOT / sha[:16]
    dest.mkdir(parents=True, exist_ok=True)
    ticket_path = dest / "ida_ticket.json"
    body: dict = {
        "schema": "driverlab_ida_ticket_v1",
        "utc": datetime.now(timezone.utc).isoformat(),
        "capability_id": profile.get("capability_id"),
        "sha256": sha,
        "name": profile.get("name"),
        "route": route,
        "host": "main_pc",
        "ida_input_path": ida_input,
        "pe_unpack_verdict": pe_verdict,
        "card_path": str(dest / "card.json"),
        "ida_mcp_namespace": "user-ida-pro-mcp",
        "ida_open": {
            "tool": "idb_open",
            "input_path": ida_input,
            "mode": "prefer_headless",
            "run_auto_analysis": True,
            "init_hexrays": True,
        },
        "ida_sequence": IDA_TOOL_ORDER,
        "device_string_candidates": (profile.get("device_string_candidates") or [])[:16],
        "byovd_tier": (profile.get("byovd") or {}).get("tier"),
        "lane": profile.get("lane"),
    }
    if compare:
        body["disk_vs_dump"] = compare
    if fixup:
        body["pe_fixup"] = fixup
    if extra:
        body.update(extra)
    ticket_path.write_text(json.dumps(body, indent=2), encoding="utf-8")
    return ticket_path


def reversal_route(
    path: str,
    *,
    write_card: bool = True,
    memory_dump: str = "",
    auto_fixup_dump: bool = True,
    queue_vm_qualify: bool = False,
    preset: str = "",
    timeout_sec: int = 180,
    sync_registry: bool = False,
    auto_dump: bool = False,
    register_dump: str = "",
) -> dict:
    """
    One host decision: static IDA now vs qualify/dump path for packed drivers.

    memory_dump: path to .writemem file from kernel debugger (optional).
    sync_registry: run host-drop + pending guest manifest validation first.
    auto_dump: pick highest-scoring validated dump from registry.
    register_dump: validate and ingest a new dump before routing.
    """
    prof = crack_driver_profile(path, write_card=write_card)
    if "error" in prof:
        return prof

    registry_extra: dict = {}
    if sync_registry or auto_dump or register_dump:
        registry_extra["registry_sync"] = scan_all()

    dump_manifest: dict | None = None
    if register_dump:
        reg = register_memory_dump(
            disk_sys_path=prof["path"],
            dump_path=register_dump,
        )
        registry_extra["register_dump"] = reg
        if reg.get("manifest"):
            dump_manifest = reg["manifest"]
        if reg.get("status") == "validated":
            fix = (reg.get("manifest") or {}).get("ida_fixup_path")
            memory_dump = fix or (reg.get("manifest") or {}).get("dump_path") or register_dump

    if auto_dump and not memory_dump:
        picked = pick_best_dump(prof["path"])
        registry_extra["auto_pick"] = picked
        if picked.get("ida_fixup_path"):
            memory_dump = picked["ida_fixup_path"]
        elif picked.get("dump_path"):
            memory_dump = picked["dump_path"]
        if picked.get("best"):
            dump_manifest = picked["best"]

    pe_verdict = (prof.get("pe_unpack") or {}).get("verdict") or "unknown"
    packed = pe_verdict in PACKED_VERDICTS or bool((prof.get("pe_unpack") or {}).get("zero_raw_executable"))

    staged = stage_binary_for_ida(prof["path"])
    ida_input = staged.get("staged_path") or prof["path"]
    fixup = None
    compare = None
    extra: dict = dict(registry_extra)

    if memory_dump:
        dp = resolve_path(memory_dump)
        if not dp.is_file():
            return {"error": f"memory_dump not found: {memory_dump}", "profile": prof}
        compare = pe_compare_disk_dump(prof["path"], dp)
        if auto_fixup_dump:
            fix_dest = dp.parent / f"{dp.stem}.ida-fixed{dp.suffix}"
            fixup = pe_fixup_mapped_for_ida(dp, fix_dest)
            if "dest" in fixup:
                ida_input = fixup["dest"]
                stage2 = stage_binary_for_ida(ida_input)
                ida_input = stage2.get("staged_path", ida_input)
        route = "fix_dump_then_ida"
        packed = False
    elif packed:
        route = "vm_qualify_then_dump"
        extra["packed_workflow"] = {
            "step1": "crack_qualify_vm (guest load + CreateFile)",
            "step2": "Kernel dump after DriverEntry — guest-register-memory-dump.ps1 or host host-drop JSON",
            "step3": "crack_reversal_auto or reversal_route --auto-dump --sync-registry",
            "step4": "idb_open on ida_input_path from ida_ticket.json",
            "doc": "driver-lab/tools/drivers/lab_post_load_dump.md",
        }
        extra["suggested_preset"] = preset or infer_qualify_preset(prof)
    else:
        route = "static_ida"
        extra["ida_ready"] = True

    ticket = write_ida_ticket(
        prof,
        ida_input=ida_input,
        route=route,
        pe_verdict=pe_verdict,
        compare=compare,
        fixup=fixup,
        extra=extra,
    )

    out: dict = {
        "route": route,
        "packed": packed,
        "pe_unpack_verdict": pe_verdict,
        "ida_input_path": ida_input,
        "ida_ticket": str(ticket),
        "staged": staged,
        "profile_sha256": prof["sha256"],
        "card_path": prof.get("card_path"),
        "ida_mcp": {
            "next": f"idb_open input_path={ida_input}",
            "sequence": [step["tool"] for step in IDA_TOOL_ORDER],
        },
    }
    if compare:
        out["disk_vs_dump"] = compare
    if fixup:
        out["pe_fixup"] = fixup
    if extra.get("suggested_preset"):
        out["suggested_qualify_preset"] = extra["suggested_preset"]
    if registry_extra.get("auto_pick"):
        out["auto_pick"] = registry_extra["auto_pick"]
    if registry_extra.get("register_dump"):
        out["register_dump"] = registry_extra["register_dump"]
    if registry_extra.get("registry_sync"):
        out["registry_sync"] = registry_extra["registry_sync"]
    if dump_manifest:
        out["dump_manifest_score"] = dump_manifest.get("score")
        link_manifest_to_ticket(prof["sha256"], ticket, dump_manifest)

    if queue_vm_qualify and packed and route == "vm_qualify_then_dump":
        try:
            VM_MCP = Path(__file__).resolve().parent.parent / "vm" / "mcp"
            if str(VM_MCP) not in sys.path:
                sys.path.insert(0, str(VM_MCP))
            from lab_bg import queue_bg_job  # noqa: E402
            from crack_lib import merge_qualify_result  # noqa: E402

            use_preset = preset or extra.get("suggested_preset") or ""
            if use_preset:
                ps = f"& 'Z:\\driver-lab\\vm\\bg\\guest-driver-crack-qualify.ps1' -Preset {use_preset}"
            else:
                ps = f"& 'Z:\\driver-lab\\vm\\bg\\guest-driver-crack-qualify.ps1' -SysPath '{prof['path']}'"
            code, stdout, stderr, job = queue_bg_job(
                "exec", powershell=ps, timeout_sec=timeout_sec, stall_sec=min(120, timeout_sec)
            )
            out["qualify_job"] = {"host_exit": code, "job": job, "stderr_tail": (stderr or "")[-400:]}
            if job and job.get("stdout"):
                merge_qualify_result(prof["sha256"][:16], str(job["stdout"]), int(job.get("exit", 1)))
        except Exception as exc:  # noqa: BLE001
            out["qualify_job"] = {"error": str(exc)}

    return out


def reversal_auto(
    path: str,
    *,
    preset: str = "",
    queue_vm_qualify: bool = True,
    timeout_sec: int = 180,
) -> dict:
    """Sync dump registry, auto-pick validated dump if present, qualify when still packed."""
    out = reversal_route(
        path,
        sync_registry=True,
        auto_dump=True,
        queue_vm_qualify=queue_vm_qualify,
        preset=preset,
        timeout_sec=timeout_sec,
    )
    out["pipeline"] = "reversal_auto"
    if out.get("route") == "vm_qualify_then_dump" and not out.get("auto_pick", {}).get("dump_path"):
        out["awaiting_dump"] = {
            "kd": "python driver-lab\\reversal\\kd_dump.py dump <disk.sys> --wait-load",
            "uart_once": "Power off VM, then driver-lab\\vm\\host-enable-kd-uart.ps1 and guest-enable-kd-serial.ps1",
            "manual": "& 'Z:\\driver-lab\\vm\\bg\\guest-register-memory-dump.ps1' -DumpPath <dump> -DiskSysPath <sys>",
        }
    return out


def main() -> int:
    import argparse

    p = argparse.ArgumentParser(description="Host reversal bridge (crack → IDA ticket)")
    sub = p.add_subparsers(dest="cmd", required=True)

    r = sub.add_parser("route", help="Build ida_ticket.json and stage binary for IDA")
    r.add_argument("path")
    r.add_argument("--dump", default="", help="Post-load memory image (.writemem)")
    r.add_argument("--no-fixup", action="store_true")
    r.add_argument("--qualify", action="store_true", help="Queue VM qualify when packed")
    r.add_argument("--preset", default="")
    r.add_argument("--sync-registry", action="store_true", help="Validate pending guest dumps + host-drop")
    r.add_argument("--auto-dump", action="store_true", help="Use best validated dump from registry")
    r.add_argument("--register-dump", default="", help="Ingest a .writemem file then route")

    auto = sub.add_parser("auto", help="sync-registry + auto-dump + qualify-if-packed")
    auto.add_argument("path")
    auto.add_argument("--preset", default="")
    auto.add_argument("--no-qualify", action="store_true")

    f = sub.add_parser("fixup", help="PE-bear style layout fix for memory dump")
    f.add_argument("path")
    f.add_argument("-o", "--out", default="")

    c = sub.add_parser("compare", help="Disk vs dump section entropy")
    c.add_argument("disk")
    c.add_argument("dump")

    args = p.parse_args()
    if args.cmd == "auto":
        out = reversal_auto(
            args.path,
            preset=args.preset,
            queue_vm_qualify=not args.no_qualify,
        )
    elif args.cmd == "route":
        out = reversal_route(
            args.path,
            memory_dump=args.dump,
            auto_fixup_dump=not args.no_fixup,
            queue_vm_qualify=args.qualify,
            preset=args.preset,
            sync_registry=args.sync_registry,
            auto_dump=args.auto_dump,
            register_dump=args.register_dump,
        )
    elif args.cmd == "fixup":
        out = pe_fixup_mapped_for_ida(args.path, args.out or None)
    else:
        out = pe_compare_disk_dump(args.disk, args.dump)
    print(json.dumps(out, indent=2))
    return 0 if "error" not in out else 1


if __name__ == "__main__":
    raise SystemExit(main())