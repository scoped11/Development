"""Shared helpers for driverlab-vm OEM / BYOVD lab MCP tools."""
from __future__ import annotations

import hashlib
import json
import re
import subprocess
from pathlib import Path

DEV = Path(__file__).resolve().parents[3]  # .../Development
LAB = DEV / "driver-lab"
DATA = LAB / "data"
AUDIT = LAB / "audit"
BG_OUT = LAB / "vm" / "bg" / "out"
STRIP = AUDIT / "inbox" / "oem-sys" / "from-vm"
SEVEN = LAB / "tools" / "7z-portable" / "7z.exe"


def resolve_path(path: str) -> Path:
    p = Path(path)
    if p.is_file():
        return p.resolve()
    alt = DEV / path.replace("/", "\\").lstrip("\\")
    if alt.is_file():
        return alt.resolve()
    return p


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def file_version(path: Path) -> str | None:
    if path.suffix.lower() != ".sys":
        return None
    try:
        ps = (
            f"(Get-Item -LiteralPath '{path}').VersionInfo | "
            "Select-Object FileVersion,ProductVersion,CompanyName,OriginalFilename | ConvertTo-Json -Compress"
        )
        proc = subprocess.run(
            ["powershell", "-NoProfile", "-Command", ps],
            capture_output=True,
            text=True,
            timeout=30,
        )
        if proc.returncode == 0 and proc.stdout.strip():
            return proc.stdout.strip()
    except (subprocess.TimeoutExpired, OSError):
        pass
    return None


def lookup_loldrivers(sha256: str, head_limit: int = 5) -> list[dict]:
    path = DATA / "loldrivers-drivers.json"
    if not path.is_file():
        return []
    sha = sha256.lower()
    hits: list[dict] = []
    try:
        data = json.loads(path.read_text(encoding="utf-8", errors="replace"))
    except json.JSONDecodeError:
        return []
    items = data if isinstance(data, list) else data.get("KnownVulnerableDrivers") or data.get("drivers") or []
    if not isinstance(items, list):
        return []
    for row in items:
        if not isinstance(row, dict):
            continue
        matched = False
        sample_name = None
        for sample in row.get("KnownVulnerableSamples") or []:
            if not isinstance(sample, dict):
                continue
            s256 = str(sample.get("SHA256") or "").lower()
            if s256 == sha:
                matched = True
                sample_name = sample.get("Filename") or sample.get("OriginalFilename")
                break
        tags = json.dumps(row).lower()
        if not matched and sha not in tags:
            continue
        desc = ""
        cmd = row.get("Commands") or {}
        if isinstance(cmd, dict):
            desc = str(cmd.get("Description") or cmd.get("Command") or "")
        hits.append(
            {
                "Name": row.get("Tags") or sample_name or row.get("Id"),
                "Author": row.get("Author"),
                "Description": desc[:500],
                "MitreID": row.get("MitreID"),
                "Category": row.get("Category"),
            }
        )
        if len(hits) >= head_limit:
            break
    return hits


def search_loldrivers_name(pattern: str, head_limit: int = 5) -> list[dict]:
    """Match LOLDrivers entries by .sys filename (e.g. SIVX64.sys)."""
    path = DATA / "loldrivers-drivers.json"
    if not path.is_file():
        return []
    pat = pattern.lower()
    try:
        data = json.loads(path.read_text(encoding="utf-8", errors="replace"))
    except json.JSONDecodeError:
        return []
    items = data if isinstance(data, list) else []
    hits: list[dict] = []
    for row in items:
        if not isinstance(row, dict):
            continue
        for sample in row.get("KnownVulnerableSamples") or []:
            if not isinstance(sample, dict):
                continue
            re = sample.get("ResearchEvidence") if isinstance(sample.get("ResearchEvidence"), dict) else {}
            fn = str(
                sample.get("Filename")
                or sample.get("OriginalFilename")
                or re.get("Filename")
                or re.get("OriginalFilename")
                or ""
            ).lower()
            if pat not in fn and pat not in json.dumps(row).lower():
                continue
            cmd = row.get("Commands") or {}
            hits.append(
                {
                    "Filename": re.get("Filename") or sample.get("Filename") or sample.get("OriginalFilename"),
                    "SHA256": sample.get("SHA256"),
                    "FileVersion": re.get("FileVersion") or sample.get("FileVersion"),
                    "Description": (cmd.get("Description") or "")[:400] if isinstance(cmd, dict) else "",
                    "Resources": row.get("Resources"),
                }
            )
            break
        if len(hits) >= head_limit:
            break
    return hits


def lookup_policy(sha256: str) -> dict | None:
    path = DATA / "ac_eac_be_vehicle_policy.json"
    if not path.is_file():
        return None
    try:
        data = json.loads(path.read_text(encoding="utf-8", errors="replace"))
    except json.JSONDecodeError:
        return None
    sha = sha256.lower()
    for section in data.values():
        if not isinstance(section, dict):
            continue
        for name, row in section.items():
            if not isinstance(row, dict):
                continue
            blob = json.dumps(row).lower()
            if sha in blob or str(name).lower() in blob:
                if sha in blob:
                    return {"key": name, **row}
    return None


def installer_fingerprint(path: Path) -> dict:
    info: dict = {"path": str(path), "size": path.stat().st_size}
    head = path.read_bytes()[:256]
    if head.startswith(b"MZ"):
        info["kind"] = "pe"
    elif head.lstrip().startswith(b"<!") or head.lstrip().startswith(b"<html"):
        info["kind"] = "html_not_installer"
        info["warning"] = "File looks like HTML (e.g. HWiNFO 403 page saved as zip)."
        return info
    elif head[:2] == b"PK":
        info["kind"] = "zip"
    else:
        info["kind"] = "unknown"
    if SEVEN.is_file():
        for arc in ("pe", "nsis", "zip", "cab"):
            proc = subprocess.run(
                [str(SEVEN), "l", f"-t{arc}", str(path)],
                capture_output=True,
                text=True,
                timeout=120,
            )
            if proc.returncode == 0 and "Type =" in (proc.stdout or ""):
                info["archive_type"] = arc
                break
        proc = subprocess.run([str(SEVEN), "l", str(path)], capture_output=True, text=True, timeout=120)
        if proc.returncode == 0:
            lines = (proc.stdout or "").splitlines()
            info["seven_zip_list_head"] = lines[:25]
            if "InstallShield" in proc.stdout or "Nsis" in proc.stdout:
                if "Type = Nsis" in proc.stdout:
                    info["installer"] = "nsis"
                if "InstallShield" in proc.stdout:
                    info["installer"] = "installshield"
    low = path.read_bytes()[: min(2_000_000, path.stat().st_size)].decode("latin-1", errors="ignore").lower()
    for needle in ("dat.cab", "gdrv", ".sys", "issetupstream"):
        if needle in low:
            info.setdefault("string_hits", []).append(needle)
    return info


def driver_fingerprint(path: str, include_loldrivers: bool = False) -> dict:
    p = resolve_path(path)
    if not p.is_file():
        return {"error": f"not a file: {path}"}
    out: dict = {
        "path": str(p),
        "name": p.name,
        "size": p.stat().st_size,
    }
    if p.suffix.lower() == ".sys":
        out["sha256"] = sha256_file(p)
        out["version_info"] = file_version(p)
        if include_loldrivers:
            out["loldrivers_sha"] = lookup_loldrivers(out["sha256"])
            out["loldrivers_name"] = search_loldrivers_name(p.name)
            if not out["loldrivers_sha"] and out["loldrivers_name"]:
                out["loldrivers_note"] = "SHA not in catalog; name match may be a different file version."
        else:
            out["loldrivers_skipped"] = "use crack_lane_matrix / driverlab-crack for vehicle qualification"
        pol = lookup_policy(out["sha256"])
        if pol:
            out["vehicle_policy"] = pol
        x64 = AUDIT / "inbox" / "oem-sys" / "x64" / p.name
        if x64.is_file():
            out["inbox_x64_sha256"] = sha256_file(x64)
            out["inbox_x64_match"] = out["inbox_x64_sha256"].lower() == out["sha256"].lower()
    else:
        out.update(installer_fingerprint(p))
        if p.suffix.lower() == ".sys":
            out["sha256"] = sha256_file(p)
    return out


OEM_PRESETS: dict[str, dict] = {
    "siv_open": {
        "script": r"Z:\driver-lab\vm\bg\guest-siv-open-privilege.ps1",
        "timeout_sec": 200,
        "stall_sec": 90,
        "blurb": "RtlAdjustPrivilege + CreateFile on Global\\\\SIVDRIVER",
    },
    "siv_phys": {
        "script": r"Z:\driver-lab\vm\bg\guest-siv-phys-smoke.ps1",
        "timeout_sec": 120,
        "stall_sec": 75,
        "blurb": "16-byte phys read IOCTL 0x10 at PA 0x1000",
    },
    "load_open_smoke": {
        "script": r"Z:\driver-lab\vm\bg\guest-vm-load-open-smoke.ps1",
        "timeout_sec": 300,
        "stall_sec": 120,
        "blurb": "SONiX + iocbios2 + SIV load/open smoke",
    },
    "driverstore": {
        "script": r"Z:\driver-lab\vm\bg\guest-driverstore-snapshot.ps1",
        "timeout_sec": 90,
        "stall_sec": 60,
        "blurb": "pnputil + driverquery (SIV/OEM filter)",
    },
    "greenfield": {
        "script": r"Z:\driver-lab\vm\bg\guest-greenfield-research-order.ps1",
        "timeout_sec": 2400,
        "stall_sec": 300,
        "blurb": "Full vault harvest (long; no mapper)",
    },
    "giga_extract": {
        "script": r"Z:\driver-lab\vm\bg\guest-giga-extract.ps1",
        "timeout_sec": 180,
        "stall_sec": 100,
        "blurb": "Gigabyte InstallShield /extract_all attempt",
    },
    "crack_qualify_siv": {
        "script": r"Z:\driver-lab\vm\bg\guest-driver-crack-qualify.ps1",
        "timeout_sec": 180,
        "stall_sec": 90,
        "blurb": "SIV v5.88 load/open via -Preset siv (priv + Global symlink)",
        "powershell_args": "-Preset siv",
    },
}


def latest_greenfield_report() -> dict:
    candidates = [
        STRIP / "greenfield_research_vm.json",
        AUDIT / "inbox" / "oem-sys" / "from-vm" / "greenfield_research_vm.json",
        LAB / "vm" / "bg" / "greenfield_research_vm.json",
    ]
    for c in candidates:
        if c.is_file():
            try:
                return {"path": str(c), "report": json.loads(c.read_text(encoding="utf-8-sig"))}
            except json.JSONDecodeError as e:
                return {"path": str(c), "error": str(e)}
    log = STRIP / "greenfield_research_vm.log"
    if log.is_file():
        lines = log.read_text(encoding="utf-8", errors="replace").splitlines()
        return {"path": str(log), "log_tail": lines[-40:]}
    return {"error": "no greenfield report on share"}
