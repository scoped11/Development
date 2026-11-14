"""
Memory-dump registry — validated manifests, inbox scan, auto-pick for reversal/IDA.

Host-owned (main PC). Guest registers via Z:\\driver-lab\\audit\\dumps\\...
"""
from __future__ import annotations

import json
import re
import struct
import uuid
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

import sys

_CRACK = Path(__file__).resolve().parent.parent / "crack"
if str(_CRACK) not in sys.path:
    sys.path.insert(0, str(_CRACK))

from crack_lib import (  # noqa: E402
    AUDIT,
    CRACK_ROOT,
    IDA_STAGING,
    pe_compare_disk_dump,
    pe_fixup_mapped_for_ida,
    pe_parse_layout,
    resolve_path,
    sha256_file,
)

DUMPS_ROOT = AUDIT / "dumps"
INBOX = DUMPS_ROOT / "inbox"
HOST_DROP = DUMPS_ROOT / "host-drop"
PROCESSED = DUMPS_ROOT / "processed"
FAILED = DUMPS_ROOT / "failed"
SCHEMA = "driverlab_dump_manifest_v2"

_HEX64 = re.compile(r"^[0-9a-f]{64}$", re.I)


def _utc_now() -> str:
    return datetime.now(timezone.utc).isoformat()


def _atomic_write_json(path: Path, body: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_suffix(path.suffix + ".tmp")
    tmp.write_text(json.dumps(body, indent=2), encoding="utf-8")
    tmp.replace(path)


def validate_dump_pe(path: Path, min_size: int = 0x400) -> dict:
    """Hard gates before a dump enters the registry."""
    out: dict = {"path": str(path), "ok": False, "checks": []}
    if not path.is_file():
        out["error"] = "not_found"
        return out
    size = path.stat().st_size
    out["size"] = size
    if size < min_size:
        out["checks"].append({"name": "min_size", "ok": False, "want": min_size, "got": size})
        out["error"] = "too_small"
        return out
    out["checks"].append({"name": "min_size", "ok": True})
    blob = path.read_bytes()[:0x1000]
    if blob[:2] != b"MZ":
        out["checks"].append({"name": "mz", "ok": False})
        out["error"] = "no_mz"
        return out
    out["checks"].append({"name": "mz", "ok": True})
    if len(blob) < 0x40:
        out["error"] = "truncated"
        return out
    e_lfanew = struct.unpack_from("<I", blob, 0x3C)[0]
    if e_lfanew + 4 > size:
        out["error"] = "bad_e_lfanew"
        return out
    with path.open("rb") as handle:
        handle.seek(e_lfanew)
        if handle.read(4) != b"PE\x00\x00":
            out["checks"].append({"name": "pe_sig", "ok": False})
            out["error"] = "no_pe"
            return out
    out["checks"].append({"name": "pe_sig", "ok": True})
    layout = pe_parse_layout(path)
    if layout.get("error"):
        out["error"] = layout["error"]
        return out
    secs = layout.get("sections") or []
    if not secs:
        out["error"] = "no_sections"
        return out
    out["checks"].append({"name": "sections", "ok": True, "count": len(secs)})
    text = next((s for s in secs if s["name"] in (".text", "PAGE", "INIT")), secs[0])
    out["text_entropy"] = text.get("entropy")
    out["layout"] = {"num_sections": len(secs), "file_alignment": layout.get("file_alignment")}
    out["ok"] = True
    return out


def _canonical_inbox_dir(disk_sha256: str) -> Path:
    sha = disk_sha256.lower()
    if not _HEX64.match(sha):
        raise ValueError(f"bad disk_sha256: {disk_sha256}")
    return INBOX / sha[:16]


def _list_manifests(sha_prefix: str) -> list[dict]:
    root = INBOX / sha_prefix.lower()[:16]
    if not root.is_dir():
        root = PROCESSED / sha_prefix.lower()[:16]
    if not root.is_dir():
        return []
    rows: list[dict] = []
    for manifest in root.rglob("manifest.json"):
        try:
            data = json.loads(manifest.read_text(encoding="utf-8"))
            data["_manifest_path"] = str(manifest)
            data["_dump_dir"] = str(manifest.parent)
            rows.append(data)
        except (json.JSONDecodeError, OSError):
            continue
    rows.sort(key=lambda r: r.get("utc") or "", reverse=True)
    return rows


def register_memory_dump(
    *,
    disk_sys_path: str | Path,
    dump_path: str | Path,
    capture_method: str = "writemem",
    kernel_base: str = "",
    size_of_image: int = 0,
    note: str = "",
    move_into_registry: bool = True,
) -> dict:
    """
    Copy (or reference) a post-load image, validate, write manifest v2.
    Returns manifest body; status validated or rejected.
    """
    disk = resolve_path(str(disk_sys_path)) if not isinstance(disk_sys_path, Path) else disk_sys_path
    dump = resolve_path(str(dump_path)) if not isinstance(dump_path, Path) else dump_path
    if not disk.is_file():
        return {"error": f"disk_sys not found: {disk_sys_path}"}
    if not dump.is_file():
        return {"error": f"dump not found: {dump_path}"}

    disk_sha = sha256_file(disk)
    dump_sha = sha256_file(dump)
    val = validate_dump_pe(dump)
    dump_id = uuid.uuid4().hex[:12]
    inbox_dir = _canonical_inbox_dir(disk_sha) / dump_id
    inbox_dir.mkdir(parents=True, exist_ok=True)

    for existing in _list_manifests(disk_sha[:16]):
        if existing.get("dump_sha256") == dump_sha and existing.get("status") == "validated":
            return {
                "status": "duplicate",
                "manifest": existing,
                "message": "Dump already registered with same SHA256",
            }

    dest_dump = inbox_dir / f"{disk.stem}.mem-{dump_id}.bin"
    if move_into_registry:
        if dump.resolve() != dest_dump.resolve():
            dest_dump.write_bytes(dump.read_bytes())
    else:
        dest_dump = dump

    if dump_sha == disk_sha:
        val = val if val.get("ok") else validate_dump_pe(dest_dump)
        manifest = {
            "schema": SCHEMA,
            "utc": _utc_now(),
            "status": "rejected",
            "score": 0,
            "rejection_reason": "dump_identical_to_disk",
            "disk_sys_path": str(disk),
            "disk_sha256": disk_sha,
            "dump_path": str(dest_dump.resolve()),
            "dump_sha256": dump_sha,
            "note": "Not a post-load image — same bytes as on-disk file.",
        }
        _atomic_write_json(inbox_dir / "manifest.json", manifest)
        return {"status": "rejected", "score": 0, "manifest": manifest, "inbox_dir": str(inbox_dir)}

    compare = pe_compare_disk_dump(disk, dest_dump)
    fixup = pe_fixup_mapped_for_ida(dest_dump)
    fix_path = fixup.get("dest") if fixup.get("dest") and not fixup.get("error") else ""
    score = _score_dump(val, compare, fixup, disk_sha, dump_sha)

    status = "validated" if val.get("ok") and score >= 50 else "rejected"
    manifest: dict[str, Any] = {
        "schema": SCHEMA,
        "utc": _utc_now(),
        "status": status,
        "score": score,
        "disk_sys_path": str(disk),
        "disk_sha256": disk_sha,
        "disk_name": disk.name,
        "dump_path": str(dest_dump.resolve()),
        "dump_sha256": dump_sha,
        "dump_validation": val,
        "pe_compare": compare,
        "pe_fixup": {k: fixup.get(k) for k in ("dest", "sha256", "error", "sections_updated") if k in fixup},
        "ida_fixup_path": fix_path,
        "capture_method": capture_method,
        "kernel_base": kernel_base,
        "size_of_image": size_of_image,
        "note": note,
        "host": "main_pc",
        "runtime_decrypt_likely": compare.get("runtime_decrypt_likely"),
    }
    if status == "rejected":
        manifest["rejection_reason"] = val.get("error") or "low_score"
    _atomic_write_json(inbox_dir / "manifest.json", manifest)
    return {"status": status, "score": score, "manifest": manifest, "inbox_dir": str(inbox_dir)}


def _score_dump(val: dict, compare: dict, fixup: dict, disk_sha: str, dump_sha: str) -> int:
    score = 0
    if val.get("ok"):
        score += 40
    if dump_sha != disk_sha:
        score += 15
    if compare.get("runtime_decrypt_likely"):
        score += 25
    ent = val.get("text_entropy")
    if ent is not None and ent < 7.0:
        score += 10
    if fixup.get("dest") and not fixup.get("error"):
        score += 10
    if compare.get("error"):
        score -= 30
    return max(0, min(100, score))


def pick_best_dump(disk_sys_path: str | Path) -> dict:
    """Choose highest-scoring validated dump for this on-disk driver."""
    disk = resolve_path(str(disk_sys_path)) if not isinstance(disk_sys_path, Path) else disk_sys_path
    if not disk.is_file():
        return {"error": f"not found: {disk_sys_path}"}
    sha = sha256_file(disk)
    candidates = [m for m in _list_manifests(sha[:16]) if m.get("disk_sha256") == sha]
    validated = [m for m in candidates if m.get("status") == "validated"]
    if not validated:
        return {
            "error": "no_validated_dump",
            "disk_sha256": sha,
            "candidates": len(candidates),
            "hint": f"Register via dump_registry register or drop under {HOST_DROP}",
        }
    best = max(validated, key=lambda m: (m.get("score") or 0, m.get("utc") or ""))
    return {"disk_sha256": sha, "best": best, "dump_path": best.get("dump_path"), "ida_fixup_path": best.get("ida_fixup_path")}


def resolve_disk_sys(disk_path: str, disk_sha256: str) -> Path | None:
    """Guest paths fail on host — fall back to card / staging."""
    p = resolve_path(disk_path)
    if p.is_file():
        return p
    sha = disk_sha256.lower()
    card = CRACK_ROOT / sha[:16] / "card.json"
    if card.is_file():
        try:
            data = json.loads(card.read_text(encoding="utf-8"))
            alt = resolve_path(str(data.get("path") or ""))
            if alt.is_file() and sha256_file(alt) == sha:
                return alt
        except (json.JSONDecodeError, OSError):
            pass
    stage = IDA_STAGING / sha[:16]
    if stage.is_dir():
        for sys in stage.glob("*.sys"):
            try:
                if sha256_file(sys) == sha:
                    return sys
            except OSError:
                continue
    inbox = AUDIT / "inbox" / "oem-sys" / "x64"
    if inbox.is_dir():
        for sys in inbox.rglob("*.sys"):
            try:
                if sha256_file(sys) == sha:
                    return sys
            except OSError:
                continue
    return None


def finalize_pending_manifest(manifest_path: Path) -> dict:
    """Upgrade guest-registered pending manifests to validated/rejected on the host."""
    try:
        data = json.loads(manifest_path.read_text(encoding="utf-8"))
    except (json.JSONDecodeError, OSError) as exc:
        return {"error": str(exc), "path": str(manifest_path)}
    if data.get("status") not in ("pending_host_validate", "pending"):
        return {"skipped": True, "status": data.get("status")}
    disk_sha = str(data.get("disk_sha256") or "")
    disk = resolve_disk_sys(str(data.get("disk_sys_path") or ""), disk_sha)
    dump = Path(str(data.get("dump_path") or ""))
    if disk is None:
        data["status"] = "rejected"
        data["rejection_reason"] = "disk_sys_not_on_host"
        data["host_validate_utc"] = _utc_now()
        _atomic_write_json(manifest_path, data)
        return {"status": "rejected", "reason": "disk_sys_not_on_host", "manifest": data}
    if not dump.is_file():
        data["status"] = "rejected"
        data["rejection_reason"] = "dump_missing"
        _atomic_write_json(manifest_path, data)
        return {"status": "rejected", "reason": "dump_missing"}
    dump_sha = sha256_file(dump)
    if dump_sha == sha256_file(disk):
        data["status"] = "rejected"
        data["rejection_reason"] = "dump_identical_to_disk"
        data["host_validate_utc"] = _utc_now()
        _atomic_write_json(manifest_path, data)
        return {"status": "rejected", "reason": "dump_identical_to_disk"}
    val = validate_dump_pe(dump)
    compare = pe_compare_disk_dump(disk, dump)
    fixup = pe_fixup_mapped_for_ida(dump)
    dump_sha = sha256_file(dump)
    score = _score_dump(val, compare, fixup, sha256_file(disk), dump_sha)
    status = "validated" if val.get("ok") and score >= 50 else "rejected"
    data.update(
        {
            "status": status,
            "score": score,
            "disk_sys_path": str(disk),
            "host_validate_utc": _utc_now(),
            "dump_validation": val,
            "pe_compare": compare,
            "pe_fixup": {k: fixup.get(k) for k in ("dest", "sha256", "error") if k in fixup},
            "ida_fixup_path": fixup.get("dest"),
            "runtime_decrypt_likely": compare.get("runtime_decrypt_likely"),
        }
    )
    if status == "rejected":
        data["rejection_reason"] = val.get("error") or "low_score"
    _atomic_write_json(manifest_path, data)
    return {"status": status, "score": score, "manifest": data}


def scan_inbox() -> dict:
    """Finalize all pending guest manifests under audit/dumps/inbox."""
    INBOX.mkdir(parents=True, exist_ok=True)
    rows: list[dict] = []
    for manifest in sorted(INBOX.rglob("manifest.json")):
        rows.append(finalize_pending_manifest(manifest))
    validated = sum(1 for r in rows if r.get("status") == "validated")
    rejected = sum(1 for r in rows if r.get("status") == "rejected")
    return {"scanned": len(rows), "validated": validated, "rejected": rejected, "results": rows}


def scan_all() -> dict:
    """Host-drop sidecars + pending inbox — one call before auto reversal."""
    drop = scan_host_drop()
    inbox = scan_inbox()
    return {"host_drop": drop, "inbox": inbox}


def scan_host_drop() -> dict:
    """
    Process driver-lab/audit/dumps/host-drop/*.json sidecars:
    { "disk_sys_path": "...", "dump_path": "...", "capture_method": "writemem", ... }
    """
    HOST_DROP.mkdir(parents=True, exist_ok=True)
    processed: list[dict] = []
    errors: list[dict] = []
    for sidecar in sorted(HOST_DROP.glob("*.json")):
        try:
            spec = json.loads(sidecar.read_text(encoding="utf-8"))
        except json.JSONDecodeError as exc:
            errors.append({"file": str(sidecar), "error": str(exc)})
            continue
        disk = spec.get("disk_sys_path") or spec.get("disk_path")
        dump = spec.get("dump_path") or spec.get("memory_dump")
        if not disk or not dump:
            errors.append({"file": str(sidecar), "error": "missing disk_sys_path or dump_path"})
            continue
        reg = register_memory_dump(
            disk_sys_path=disk,
            dump_path=dump,
            capture_method=str(spec.get("capture_method") or "host_drop"),
            kernel_base=str(spec.get("kernel_base") or ""),
            size_of_image=int(spec.get("size_of_image") or 0),
            note=str(spec.get("note") or f"from {sidecar.name}"),
        )
        processed.append({"sidecar": str(sidecar), "register": reg})
        if reg.get("status") in ("validated", "duplicate"):
            archive = PROCESSED / "host-drop" / sidecar.name
            archive.parent.mkdir(parents=True, exist_ok=True)
            sidecar.replace(archive)
    return {"processed": processed, "errors": errors, "drop_dir": str(HOST_DROP)}


def link_manifest_to_ticket(disk_sha256: str, ticket_path: Path, manifest: dict) -> None:
    ticket_path = Path(ticket_path)
    if not ticket_path.is_file():
        return
    try:
        ticket = json.loads(ticket_path.read_text(encoding="utf-8"))
    except json.JSONDecodeError:
        return
    ticket["dump_manifest"] = {
        "schema": manifest.get("schema"),
        "utc": manifest.get("utc"),
        "dump_sha256": manifest.get("dump_sha256"),
        "score": manifest.get("score"),
        "runtime_decrypt_likely": manifest.get("runtime_decrypt_likely"),
        "ida_fixup_path": manifest.get("ida_fixup_path"),
    }
    _atomic_write_json(ticket_path, ticket)


def dump_registry_cli_main(argv: list[str] | None = None) -> int:
    import argparse

    p = argparse.ArgumentParser(description="DriverLab memory dump registry")
    sub = p.add_subparsers(dest="cmd", required=True)

    reg = sub.add_parser("register", help="Validate and register a memory dump")
    reg.add_argument("disk_sys")
    reg.add_argument("dump")
    reg.add_argument("--method", default="writemem")
    reg.add_argument("--kernel-base", default="")
    reg.add_argument("--note", default="")

    pick = sub.add_parser("pick", help="Best validated dump for a .sys")
    pick.add_argument("disk_sys")

    scan = sub.add_parser("scan-drop", help="Ingest host-drop/*.json sidecars")
    sub.add_parser("scan-inbox", help="Validate pending guest manifests")
    sub.add_parser("scan-all", help="scan-drop then scan-inbox")

    args = p.parse_args(argv)
    if args.cmd == "register":
        out = register_memory_dump(
            disk_sys_path=args.disk_sys,
            dump_path=args.dump,
            capture_method=args.method,
            kernel_base=args.kernel_base,
            note=args.note,
        )
    elif args.cmd == "pick":
        out = pick_best_dump(args.disk_sys)
    elif args.cmd == "scan-inbox":
        out = scan_inbox()
    elif args.cmd == "scan-all":
        out = scan_all()
    else:
        out = scan_host_drop()
    print(json.dumps(out, indent=2))
    return 0 if "error" not in out else 1


if __name__ == "__main__":
    raise SystemExit(dump_registry_cli_main())