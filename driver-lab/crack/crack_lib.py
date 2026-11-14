"""
DriverLab crack pipeline — qualify kernel primitives from *your* binaries only.

Does not bypass anti-cheat. ``lane_assess`` reports CI/LOLDrivers/name heuristics so you
route work to isolated VM research vs paid live lanes (see driver3 lane-gate).
"""
from __future__ import annotations

import csv
import hashlib
import json
import re
import subprocess
from datetime import datetime, timezone
from pathlib import Path

DEV = Path(__file__).resolve().parents[2]
LAB = DEV / "driver-lab"
DATA = LAB / "data"
AUDIT = LAB / "audit"
CRACK_ROOT = AUDIT / "crack"
LOL_CSV = DATA / "loldrivers-drivers.csv"
AC_POLICY = DATA / "ac_eac_be_vehicle_policy.json"
SEVEN = LAB / "tools" / "7z-portable" / "7z.exe"

PHYS_IMPORTS = (
    "MmMapIoSpace",
    "MmMapIoSpaceEx",
    "MmUnmapIoSpace",
    "MmGetSystemRoutineAddress",
    "MmMapLockedPages",
    "MmProbeAndLockPages",
    "MmCopyMemory",
    "MmCopyVirtualMemory",
    "ZwMapViewOfSection",
    "ZwOpenSection",
    "HalGetBusData",
    "HalSetBusData",
    "HalGetBusDataByOffset",
    "HalSetBusDataByOffset",
    "__readmsr",
    "__writemsr",
    "MapIoSpace",
)

DEVICE_PATTERNS = (
    re.compile(rb"\\\\\.\\[^\x00\r\n\\]{2,64}", re.I),
    re.compile(rb"\\Device\\[^\x00\r\n\\]{2,64}", re.I),
    re.compile(rb"\\DosDevices\\[^\x00\r\n\\]{2,64}", re.I),
    re.compile(rb"\\GLOBAL??\\[^\x00\r\n\\]{2,64}", re.I),
)

_lol_hash_index: dict[str, dict] | None = None
_lol_tag_index: dict[str, dict] | None = None
_ac_policy: dict | None = None


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


def find_dumpbin() -> Path | None:
    roots = [
        Path(r"C:\Program Files\Microsoft Visual Studio\2022"),
        Path(r"C:\Program Files (x86)\Microsoft Visual Studio\2022"),
        Path(r"C:\Program Files\Microsoft Visual Studio"),
    ]
    for root in roots:
        if not root.is_dir():
            continue
        for candidate in root.rglob("dumpbin.exe"):
            if "Hostx64" in str(candidate) and "x64" in str(candidate):
                return candidate
    return None


def dumpbin_imports(path: Path) -> list[str]:
    db = find_dumpbin()
    if not db:
        return []
    try:
        proc = subprocess.run(
            [str(db), "/imports", str(path)],
            capture_output=True,
            text=True,
            timeout=120,
        )
    except (subprocess.TimeoutExpired, OSError):
        return []
    if proc.returncode != 0:
        return []
    symbols: list[str] = []
    # dumpbin import rows: "    180012345  HalGetBusDataByOffset" or "         5A HalGetBusDataByOffset"
    row_re = re.compile(r"^\s*[0-9A-Fa-f]+\s+([A-Za-z_][\w@$?]*)$")
    for line in (proc.stdout or "").splitlines():
        m = row_re.match(line)
        if not m:
            continue
        tok = m.group(1)
        if tok.lower() in {"hint", "name", "table", "summary", "ordinal"}:
            continue
        symbols.append(tok)
    return sorted(set(symbols))


def scan_binary_strings(path: Path, max_bytes: int = 3_000_000) -> bytes:
    size = min(path.stat().st_size, max_bytes)
    with path.open("rb") as f:
        return f.read(size)


def _utf16le_strings(blob: bytes, min_chars: int = 6) -> list[str]:
    found: list[str] = []
    for align in (0, 1):
        chars: list[str] = []
        i = align
        while i + 1 < len(blob):
            lo, hi = blob[i], blob[i + 1]
            if hi == 0 and 32 <= lo < 127:
                chars.append(chr(lo))
            else:
                if len(chars) >= min_chars:
                    found.append("".join(chars))
                chars = []
            i += 2
        if len(chars) >= min_chars:
            found.append("".join(chars))
    return found


def device_candidates(blob: bytes) -> list[str]:
    found: set[str] = set()
    for pat in DEVICE_PATTERNS:
        for m in pat.finditer(blob):
            s = m.group(0).decode("ascii", errors="ignore").strip("\x00 ")
            if len(s) > 3:
                found.add(s)
    interesting = ("\\Device\\", "\\DosDevices\\", "\\\\.\\", "\\GLOBAL??\\", "SIVDRIVER", "Global\\")
    for s in _utf16le_strings(blob):
        if any(tok in s for tok in interesting):
            found.add(s.strip())
    return sorted(found, key=lambda x: (0 if x.startswith("\\\\.\\") else 1, x.lower()))


def byovd_tier(imports: list[str], strings_blob: bytes) -> dict:
    hit_imports = [s for s in PHYS_IMPORTS if s in imports]
    blob_s = strings_blob.decode("latin-1", errors="ignore")
    hit_dyn = "MmGetSystemRoutineAddress" in hit_imports or "MmGetSystemRoutineAddress" in blob_s
    if hit_dyn and ("MmMapIoSpace" in blob_s or "MmMapIoSpaceEx" in blob_s):
        hit_imports.append("(dynamic MmMapIoSpace*)")

    score = len(hit_imports)
    if score >= 3:
        tier = 1
    elif score >= 1:
        tier = 2
    else:
        tier = 3

    return {
        "tier": tier,
        "tier_note": "1=strong phys/map surface, 2=partial, 3=weak/none from imports+strings",
        "danger_imports": hit_imports,
    }


def _load_lol_index() -> tuple[dict[str, dict], dict[str, dict]]:
    global _lol_hash_index, _lol_tag_index
    if _lol_hash_index is not None and _lol_tag_index is not None:
        return _lol_hash_index, _lol_tag_index
    hidx: dict[str, dict] = {}
    tidx: dict[str, dict] = {}
    if LOL_CSV.is_file():
        with LOL_CSV.open(encoding="utf-8", errors="replace", newline="") as f:
            for row in csv.DictReader(f):
                sha_field = row.get("KnownVulnerableSamples_SHA256") or ""
                for h in sha_field.split(","):
                    h = h.strip().lower()
                    if h:
                        hidx[h] = row
                tags = row.get("Tags") or ""
                for t in tags.split(","):
                    t = t.strip().lower()
                    if t:
                        tidx[t] = row
    _lol_hash_index, _lol_tag_index = hidx, tidx
    return hidx, tidx


def _load_ac_policy() -> dict:
    global _ac_policy
    if _ac_policy is not None:
        return _ac_policy
    if AC_POLICY.is_file():
        try:
            _ac_policy = json.loads(AC_POLICY.read_text(encoding="utf-8"))
        except json.JSONDecodeError:
            _ac_policy = {}
    else:
        _ac_policy = {}
    return _ac_policy


def _tier_match(filename: str, tier: str) -> str:
    policy = _load_ac_policy()
    tiers = policy.get("tiers") or {}
    block = tiers.get(tier) or {}
    patterns = block.get("block_filename_patterns") or []
    for p in patterns:
        try:
            if re.match(p, filename):
                return "name_match"
        except re.error:
            continue
    mapper = (policy.get("mapper_vehicles") or {}).get(filename)
    if isinstance(mapper, dict):
        key = {
            "paid_eac": "eac_paid",
            "paid_battleye": "be_paid",
            "paid_ace": "ace_paid",
        }.get(tier, "")
        if key and key in mapper:
            return str(mapper[key])
    return "no_match"


def lane_assess(sha256: str, filename: str) -> dict:
    """Paid-tier / CI visibility — not authorization to cheat on live games."""
    hidx, tidx = _load_lol_index()
    sha = sha256.lower()
    lol = hidx.get(sha) or tidx.get(filename.lower())

    eac = _tier_match(filename, "paid_eac")
    be = _tier_match(filename, "paid_battleye")
    ace = _tier_match(filename, "paid_ace")

    ci_block = "likely_when_blocklist_on" if lol else "no_loldrivers_hash_row"

    public_lol = bool(lol)
    name_blocked = any(x in ("name_match", "block_expected") for x in (eac, be, ace))

    if public_lol or name_blocked:
        paid_lane = "avoid_mapper_vehicle"
        vm_lane = "qualify_ioctl_only_in_lab"
    else:
        paid_lane = "unknown_hash_research_not_live"
        vm_lane = "greenfield_qualify_then_ida"

    return {
        "anti_cheat_bypass": False,
        "anti_cheat_note": (
            "No tool bypasses EAC/BE/ACE/Vanguard. This block records local heuristics "
            "only; live titles add kernel/user/server/behavior checks."
        ),
        "loldrivers_hash_match": public_lol,
        "win11_vuln_blocklist_hint": ci_block,
        "paid_tier_heuristic": {
            "eac_paid": eac,
            "battleye_paid": be,
            "ace_paid": ace,
        },
        "routing": {
            "paid_live_lane": paid_lane,
            "driverlab_vm_lane": vm_lane,
        },
        "honest_summary": (
            "Public LOL / classic vehicle names are dead for paid AC + CI."
            if (public_lol or name_blocked)
            else "Hash not in local LOL CSV — still requires VM qualification; not live-ready."
        ),
    }


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


def crack_driver_profile(path: str, write_card: bool = False) -> dict:
    p = resolve_path(path)
    if not p.is_file():
        return {"error": f"not a file: {path}"}
    if p.suffix.lower() != ".sys":
        return {"error": "crack_driver_profile expects a .sys kernel driver"}

    blob = scan_binary_strings(p)
    imports = dumpbin_imports(p)
    sha = sha256_file(p)
    devices = device_candidates(blob)
    tier = byovd_tier(imports, blob)
    lane = lane_assess(sha, p.name)

    out: dict = {
        "schema": "driverlab_crack_card_v1",
        "utc": datetime.now(timezone.utc).isoformat(),
        "path": str(p),
        "name": p.name,
        "size": p.stat().st_size,
        "sha256": sha,
        "capability_id": f"{sha.lower()}@{p.name}",
        "version_info": file_version(p),
        "imports_count": len(imports),
        "imports_sample": imports[:80],
        "byovd": tier,
        "device_string_candidates": devices[:40],
        "vm_probe_suggest": {
            "devices_try": [d for d in devices if d.startswith("\\\\.\\")][:12],
            "se_load_driver_privilege": "\\\\.\\Global\\" in " ".join(devices)
            or "SIVDRIVER" in blob.decode("latin-1", errors="ignore").upper(),
        },
        "lane": lane,
        "next_steps": [
            "Run crack_qualify on guest (load + CreateFile on devices_try).",
            "Open IDA on this hash; ticket IRP_MJ_CREATE + DEVICE_CONTROL.",
            "If lane.paid_live_lane is avoid_mapper_vehicle, do not plan live mapper.",
        ],
    }

    if write_card:
        card_path = write_crack_card(out)
        out["card_path"] = str(card_path)
    return out


def write_crack_card(profile: dict) -> Path:
    CRACK_ROOT.mkdir(parents=True, exist_ok=True)
    sha = str(profile.get("sha256", "unknown"))
    dest = CRACK_ROOT / sha[:16]
    dest.mkdir(parents=True, exist_ok=True)
    card = dest / "card.json"
    card.write_text(json.dumps(profile, indent=2), encoding="utf-8")
    return card


def find_sys_files(root: Path, limit: int = 200) -> list[dict]:
    if not root.is_dir():
        return []
    rows: list[dict] = []
    for p in root.rglob("*.sys"):
        try:
            rows.append(
                {
                    "path": str(p),
                    "name": p.name,
                    "size": p.stat().st_size,
                    "sha256": sha256_file(p),
                }
            )
        except OSError:
            continue
        if len(rows) >= limit:
            break
    return rows


def extract_installer(path: str, out_subdir: str | None = None, max_depth: int = 4) -> dict:
    p = resolve_path(path)
    if not p.is_file():
        return {"error": f"not found: {path}"}
    if not SEVEN.is_file():
        return {"error": f"7z missing: {SEVEN}"}
    base = AUDIT / "inbox" / "oem-extract" / "crack"
    leaf = out_subdir or p.stem
    out = base / leaf
    out.mkdir(parents=True, exist_ok=True)

    def extract_one(src: Path, dest: Path, depth: int) -> list[str]:
        if depth > max_depth:
            return []
        dest.mkdir(parents=True, exist_ok=True)
        subprocess.run([str(SEVEN), "x", "-y", f"-o{dest}", str(src)], capture_output=True, timeout=300)
        found: list[str] = []
        for sys in dest.rglob("*.sys"):
            found.append(str(sys))
        if depth < max_depth:
            for nested in dest.rglob("*"):
                if not nested.is_file():
                    continue
                if nested.suffix.lower() not in {".exe", ".msi", ".7z", ".zip", ".cab"}:
                    continue
                if nested.stat().st_size < 200_000:
                    continue
                sub = dest / "_nested" / nested.stem
                found.extend(extract_one(nested, sub, depth + 1))
        return found

    sys_paths = extract_one(p, out, 0)
    unique = sorted(set(sys_paths))
    profiles = []
    for sp in unique[:25]:
        profiles.append({"path": sp, "name": Path(sp).name, "sha256": sha256_file(Path(sp))})
    return {
        "installer": str(p),
        "extract_root": str(out),
        "sys_count": len(unique),
        "sys_files": profiles,
    }


def merge_qualify_result(card_sha_prefix: str, guest_stdout: str, guest_exit: int) -> dict | None:
    CRACK_ROOT.mkdir(parents=True, exist_ok=True)
    card = None
    for d in CRACK_ROOT.iterdir():
        if d.is_dir() and d.name.lower().startswith(card_sha_prefix.lower()):
            c = d / "card.json"
            if c.is_file():
                card = c
                break
    if not card:
        return None
    try:
        data = json.loads(card.read_text(encoding="utf-8"))
    except json.JSONDecodeError:
        return None
    data["vm_qualify"] = {
        "utc": datetime.now(timezone.utc).isoformat(),
        "exit": guest_exit,
        "stdout_tail": guest_stdout.splitlines()[-30:],
        "load_open_ok": "CREATEFILE_OK" in guest_stdout,
    }
    card.write_text(json.dumps(data, indent=2), encoding="utf-8")
    return data
