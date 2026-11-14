"""
DriverLab crack pipeline — qualify kernel primitives from *your* binaries only.

Does not bypass anti-cheat. ``lane_assess`` reports CI/LOLDrivers/name heuristics so you
route work to isolated VM research vs paid live lanes (see driver3 lane-gate).
"""
from __future__ import annotations

import csv
import hashlib
import json
import math
import os
import re
import shutil
import struct
import subprocess
from datetime import datetime, timezone
from pathlib import Path

DEV = Path(__file__).resolve().parents[2]
LAB = DEV / "driver-lab"
DATA = LAB / "data"
AUDIT = LAB / "audit"
CRACK_ROOT = AUDIT / "crack"
IDA_STAGING = AUDIT / "inbox" / "ida-staging"
LOL_CSV = DATA / "loldrivers-drivers.csv"
AC_POLICY = DATA / "ac_eac_be_vehicle_policy.json"
SEVEN = LAB / "tools" / "7z-portable" / "7z.exe"
UNSHIELD = LAB / "tools" / "unshield" / "unshield.exe"
POST_LOAD_DUMP_DOC = "driver-lab/tools/drivers/lab_post_load_dump.md"
SCN_CNT_UNINITIALIZED_DATA = 0x00000080
SCN_MEM_EXECUTE = 0x20000000
SCN_MEM_WRITE = 0x80000000
IS_CAB_MAGIC = b"ISc("
INNO_MAGIC = b"Inno Setup Setup Data"

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


def _shannon_entropy(data: bytes) -> float:
    if not data:
        return 0.0
    counts = [0] * 256
    for b in data:
        counts[b] += 1
    n = len(data)
    ent = 0.0
    for c in counts:
        if c:
            p = c / n
            ent -= p * math.log2(p)
    return ent


def pe_unpack_assess(path: str | Path) -> dict:
    """
    Detect packers / runtime-decrypt drivers on disk (entropy + PE sanity).
    Does not brute-force crypto keys — use VM load + dump when static RE is blind.
    """
    p = resolve_path(str(path)) if not isinstance(path, Path) else path
    if not p.is_file():
        return {"error": f"not a file: {path}"}
    blob = p.read_bytes()
    out: dict = {
        "path": str(p),
        "size": len(blob),
        "is_pe": blob[:2] == b"MZ",
    }
    if not out["is_pe"]:
        out["verdict"] = "not_pe"
        out["next_steps"] = ["Use crack_extract on the parent installer/archive."]
        return out

    e_lfanew = struct.unpack_from("<I", blob, 0x3C)[0] if len(blob) > 0x40 else 0
    if e_lfanew + 0xF8 > len(blob):
        out["verdict"] = "pe_truncated"
        return out
    if blob[e_lfanew : e_lfanew + 4] != b"PE\x00\x00":
        out["verdict"] = "bad_pe_signature"
        return out

    machine = struct.unpack_from("<H", blob, e_lfanew + 4)[0]
    num_sections = struct.unpack_from("<H", blob, e_lfanew + 6)[0]
    opt_hdr_size = struct.unpack_from("<H", blob, e_lfanew + 20)[0]
    sec_table = e_lfanew + 24 + opt_hdr_size
    sections: list[dict] = []
    pack_hints: list[str] = []
    low = blob.decode("latin-1", errors="ignore").lower()
    for needle in ("upx", "themida", "vmprotect", "enigma", "aspack", "petite", "mpress", ".vmps"):
        if needle in low:
            pack_hints.append(needle)

    high_entropy_sections: list[str] = []
    zero_raw_executable: list[str] = []
    rwx_sections: list[str] = []
    expanded_executable: list[str] = []
    for i in range(min(num_sections, 32)):
        off = sec_table + i * 40
        if off + 40 > len(blob):
            break
        name = blob[off : off + 8].split(b"\x00")[0].decode("ascii", errors="replace")
        vsize, _vaddr, raw_size, raw_ptr = struct.unpack_from("<IIII", blob, off + 8)
        chars = struct.unpack_from("<I", blob, off + 36)[0]
        executable = bool(chars & SCN_MEM_EXECUTE)
        writable = bool(chars & SCN_MEM_WRITE)
        uninitialized = bool(chars & SCN_CNT_UNINITIALIZED_DATA)
        sec_data = blob[raw_ptr : raw_ptr + raw_size] if raw_size and raw_ptr < len(blob) else b""
        ent = _shannon_entropy(sec_data[: min(len(sec_data), 256_000)])
        zero_raw_code = raw_size == 0 and vsize >= 0x1000 and executable and not uninitialized
        rwx = executable and writable
        expanded = (
            executable
            and raw_size >= 0x200
            and vsize >= 0x2000
            and vsize >= raw_size * 3
        )
        row = {
            "name": name,
            "virtual_size": vsize,
            "raw_size": raw_size,
            "entropy": round(ent, 3),
            "executable": executable,
            "writable": writable,
            "rwx": rwx,
            "zero_raw_executable": zero_raw_code,
        }
        sections.append(row)
        if raw_size > 0x200 and ent >= 7.2 and name not in (".rsrc",):
            high_entropy_sections.append(name)
        if zero_raw_code:
            zero_raw_executable.append(name)
        if rwx:
            rwx_sections.append(name)
        if expanded:
            expanded_executable.append(name)
        lname = name.lower()
        if lname in {"upx0", "upx1", "upx2", ".vmp0", ".vmp1", ".themida"} and lname not in pack_hints:
            pack_hints.append(lname)

    imports = dumpbin_imports(p)
    stripped = len(imports) < 8
    text_sec = next((s for s in sections if s["name"] in (".text", "PAGE", "INIT")), None)
    text_entropy = text_sec["entropy"] if text_sec else None
    packed_layout = bool(zero_raw_executable) or (bool(expanded_executable) and stripped)

    if not sections:
        verdict = "pe_parse_failed"
    elif any(h in pack_hints for h in ("upx", "themida", "vmprotect", "enigma", "aspack", "upx0", "upx1", ".vmp0", ".vmp1")):
        verdict = "likely_packer_named"
    elif packed_layout or (high_entropy_sections and stripped):
        verdict = "likely_encrypted_or_packed"
    elif high_entropy_sections or expanded_executable:
        verdict = "high_entropy_sections"
    elif stripped and text_entropy is not None and text_entropy > 7.0:
        verdict = "stripped_dense_text"
    else:
        verdict = "static_re_ok"

    dump_steps = [
        f"On-disk bytes stay packed. Follow {POST_LOAD_DUMP_DOC} on the lab VM.",
        "guest-export-stripped-driver.ps1 copies the DriverStore file. That is not the unpacked image.",
        "Dump after the stub restores code, fix raw offsets to virtual addresses, then open that file in IDA.",
    ]
    routes = {
        "static_re_ok": [
            "IDA/Ghidra on this file; run crack_profile + qualify.",
        ],
        "likely_encrypted_or_packed": dump_steps,
        "likely_packer_named": dump_steps
        + [
            "Try crack_extract on the parent installer. OEM drivers are rarely packed on disk.",
        ],
        "high_entropy_sections": [
            "Inspect high-entropy or expanded sections in IDA. They may be compressed blobs, not a packed driver.",
        ],
        "stripped_dense_text": [
            "Normal for retail .sys — use strings/UTF-16 scan (crack_profile) + VM IOCTL probes.",
        ],
    }
    next_steps = list(routes.get(verdict, ["Manual review."]))
    rwx_unexpected = [name for name in rwx_sections if name.lower() not in {"init", "initdata"}]
    if rwx_unexpected and verdict == "static_re_ok":
        next_steps.insert(
            0,
            "RWX sections other than INIT are flagged. Confirm they are not a packer stub before treating the file as plain code.",
        )
    out.update(
        {
            "machine": hex(machine),
            "num_sections": num_sections,
            "sections": sections[:24],
            "high_entropy_sections": high_entropy_sections,
            "zero_raw_executable": zero_raw_executable,
            "rwx_sections": rwx_sections,
            "rwx_unexpected": rwx_unexpected,
            "expanded_executable": expanded_executable,
            "pack_string_hints": pack_hints,
            "imports_count": len(imports),
            "stripped_imports": stripped,
            "verdict": verdict,
            "unpack_note": (
                "Entropy, raw-size versus virtual-size, and RWX triage. Not decryption. "
                "A runtime decrypt needs a post-load memory image. This lab has no kernel debugger wired, "
                "so the dump stays a manual VM step."
            ),
            "next_steps": next_steps,
        }
    )
    return out


def _align_up(value: int, alignment: int) -> int:
    if alignment <= 0:
        return value
    return (value + alignment - 1) // alignment * alignment


def pe_parse_layout(path: Path) -> dict:
    """Minimal PE layout for fixup and compare."""
    blob = path.read_bytes()
    out: dict = {"path": str(path), "size": len(blob), "sections": []}
    if blob[:2] != b"MZ":
        out["error"] = "not_pe"
        return out
    e_lfanew = struct.unpack_from("<I", blob, 0x3C)[0]
    if e_lfanew + 0xF8 > len(blob) or blob[e_lfanew : e_lfanew + 4] != b"PE\x00\x00":
        out["error"] = "bad_pe"
        return out
    opt = e_lfanew + 24
    magic = struct.unpack_from("<H", blob, opt)[0]
    num_sections = struct.unpack_from("<H", blob, e_lfanew + 6)[0]
    opt_hdr_size = struct.unpack_from("<H", blob, e_lfanew + 20)[0]
    if magic != 0x20B:
        out["error"] = "not_pe32_plus"
        return out
    file_align = struct.unpack_from("<I", blob, opt + 32)[0] or 0x200
    sec_table = e_lfanew + 24 + opt_hdr_size
    for i in range(min(num_sections, 64)):
        off = sec_table + i * 40
        if off + 40 > len(blob):
            break
        name = blob[off : off + 8].split(b"\x00")[0].decode("ascii", errors="replace")
        vsize, vaddr, raw_size, raw_ptr = struct.unpack_from("<IIII", blob, off + 8)
        raw = blob[raw_ptr : raw_ptr + raw_size] if raw_size and raw_ptr < len(blob) else b""
        out["sections"].append(
            {
                "index": i,
                "name": name,
                "virtual_size": vsize,
                "virtual_address": vaddr,
                "raw_size": raw_size,
                "raw_pointer": raw_ptr,
                "entropy": round(_shannon_entropy(raw[: min(len(raw), 256_000)]), 3),
            }
        )
    out["e_lfanew"] = e_lfanew
    out["sec_table"] = sec_table
    out["num_sections"] = num_sections
    out["file_alignment"] = file_align
    return out


def pe_fixup_mapped_for_ida(src: str | Path, dest: str | Path | None = None) -> dict:
    """
    Synchronize section raw file offsets with virtual addresses (PE-bear style).
    Use on kernel memory dumps (.writemem) before IDA.
    """
    sp = resolve_path(str(src)) if not isinstance(src, Path) else src
    if not sp.is_file():
        return {"error": f"not found: {src}"}
    layout = pe_parse_layout(sp)
    if layout.get("error"):
        return {"error": layout["error"], "path": str(sp)}
    blob = bytearray(sp.read_bytes())
    e_lfanew = layout["e_lfanew"]
    file_align = layout["file_alignment"]
    sec_table = layout["sec_table"]
    max_end = len(blob)
    changes: list[dict] = []
    for row in layout["sections"]:
        i = row["index"]
        off = sec_table + i * 40
        vsize = row["virtual_size"]
        vaddr = row["virtual_address"]
        new_raw_size = _align_up(vsize, file_align) if vsize else 0
        new_raw_ptr = vaddr if new_raw_size else 0
        struct.pack_into("<II", blob, off + 16, new_raw_size, new_raw_ptr)
        end = new_raw_ptr + new_raw_size
        if end > max_end:
            max_end = end
        changes.append(
            {
                "name": row["name"],
                "was_raw_ptr": row["raw_pointer"],
                "was_raw_size": row["raw_size"],
                "new_raw_ptr": new_raw_ptr,
                "new_raw_size": new_raw_size,
            }
        )
    if max_end > len(blob):
        blob.extend(b"\x00" * (max_end - len(blob)))
    if dest is None:
        parent = sp.parent
        dp = parent / f"{sp.stem}.ida-fixed{sp.suffix}"
    else:
        dp = Path(dest)
    dp.parent.mkdir(parents=True, exist_ok=True)
    dp.write_bytes(blob)
    return {
        "source": str(sp),
        "dest": str(dp.resolve()),
        "sha256": sha256_file(dp),
        "file_alignment": file_align,
        "sections_updated": changes,
        "ida_note": "Open dest in IDA (user-ida-pro-mcp idb_open). Source hash differs from dest after layout fix.",
    }


def pe_compare_disk_dump(disk: str | Path, dump: str | Path) -> dict:
    """Compare per-section entropy between on-disk and post-load images."""
    d1 = pe_parse_layout(resolve_path(str(disk)) if not isinstance(disk, Path) else disk)
    d2 = pe_parse_layout(resolve_path(str(dump)) if not isinstance(dump, Path) else dump)
    if d1.get("error") or d2.get("error"):
        return {"error": "pe_parse_failed", "disk": d1.get("error"), "dump": d2.get("error")}
    by_name = {s["name"]: s for s in d2["sections"]}
    rows: list[dict] = []
    decrypted_hint = False
    for s in d1["sections"]:
        other = by_name.get(s["name"])
        if not other:
            rows.append({"name": s["name"], "disk_entropy": s["entropy"], "dump_entropy": None, "delta": None})
            continue
        delta = round(other["entropy"] - s["entropy"], 3)
        if s["entropy"] >= 7.0 and other["entropy"] < 6.5:
            decrypted_hint = True
        rows.append(
            {
                "name": s["name"],
                "disk_entropy": s["entropy"],
                "dump_entropy": other["entropy"],
                "delta": delta,
                "disk_raw_size": s["raw_size"],
                "dump_raw_size": other["raw_size"],
            }
        )
    return {
        "disk": d1["path"],
        "dump": d2["path"],
        "sections": rows,
        "runtime_decrypt_likely": decrypted_hint,
        "note": (
            "Large entropy drop on .text/PAGE after load/fixup usually means runtime decrypt. "
            "If entropy is unchanged, you may still have the DriverStore file, not a memory dump."
        ),
    }


def stage_binary_for_ida(path: str | Path) -> dict:
    """Copy into audit/inbox/ida-staging so the host IDA workbench owns the file (no Program Files locks)."""
    sp = resolve_path(str(path)) if not isinstance(path, Path) else path
    if not sp.is_file():
        return {"error": f"not found: {path}"}
    sha = sha256_file(sp)
    dest_dir = IDA_STAGING / sha[:16]
    dest_dir.mkdir(parents=True, exist_ok=True)
    dest = dest_dir / sp.name
    shutil.copy2(sp, dest)
    manifest = dest_dir / "manifest.json"
    manifest.write_text(
        json.dumps(
            {
                "sha256": sha,
                "source": str(sp),
                "staged": str(dest),
                "utc": datetime.now(timezone.utc).isoformat(),
            },
            indent=2,
        ),
        encoding="utf-8",
    )
    return {"sha256": sha, "source": str(sp), "staged_path": str(dest.resolve()), "manifest": str(manifest)}


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
    pe = pe_unpack_assess(p)

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
        "pe_unpack": {
            "verdict": pe.get("verdict"),
            "high_entropy_sections": pe.get("high_entropy_sections"),
            "zero_raw_executable": pe.get("zero_raw_executable"),
            "rwx_sections": pe.get("rwx_sections"),
            "rwx_unexpected": pe.get("rwx_unexpected"),
            "expanded_executable": pe.get("expanded_executable"),
            "pack_string_hints": pe.get("pack_string_hints"),
        },
        "next_steps": [
            "Run crack_qualify on guest (load + CreateFile on devices_try).",
            "Open IDA on this hash; ticket IRP_MJ_CREATE + DEVICE_CONTROL.",
            "If lane.paid_live_lane is avoid_mapper_vehicle, do not plan live mapper.",
        ],
    }
    if pe.get("verdict") not in (None, "static_re_ok"):
        out["next_steps"] = list(pe.get("next_steps") or []) + out["next_steps"][:2]

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


def _file_contains(path: Path, needle: bytes, max_bytes: int = 32_000_000) -> bool:
    try:
        with path.open("rb") as handle:
            prev = b""
            remaining = max_bytes
            while remaining > 0:
                chunk = handle.read(min(1_048_576, remaining))
                if not chunk:
                    break
                remaining -= len(chunk)
                window = prev + chunk
                if needle in window:
                    return True
                prev = chunk[-(len(needle) - 1) :] if len(needle) > 1 else b""
    except OSError:
        return False
    return False


def _is_iscab(path: Path) -> bool:
    try:
        with path.open("rb") as handle:
            return handle.read(4) == IS_CAB_MAGIC
    except OSError:
        return False


def find_unshield() -> Path | None:
    if UNSHIELD.is_file():
        return UNSHIELD
    found = shutil.which("unshield")
    return Path(found) if found else None


def find_innounp() -> Path | None:
    found = shutil.which("innounp")
    if found:
        return Path(found)
    local = os.environ.get("LOCALAPPDATA")
    if not local:
        return None
    root = Path(local) / "Microsoft" / "WinGet" / "Packages"
    if not root.is_dir():
        return None
    hits = sorted(root.glob("JurgenRathlev.innounp*/innounp.exe"))
    return hits[0] if hits else None


def _tail(text: str | None, limit: int = 400) -> str:
    return (text or "")[-limit:]


def unshield_extract(cab: Path, dest: Path) -> dict:
    """Extract an InstallShield cabinet (ISc( / data1.cab). Not a driver decryptor."""
    exe = find_unshield()
    row: dict = {"cab": str(cab), "dest": str(dest)}
    if exe is None:
        row["ok"] = False
        row["error"] = f"unshield missing: {UNSHIELD}"
        return row
    dest.mkdir(parents=True, exist_ok=True)
    try:
        proc = subprocess.run(
            [str(exe), "-d", str(dest), "x", str(cab)],
            capture_output=True,
            text=True,
            timeout=300,
        )
    except (subprocess.TimeoutExpired, OSError) as exc:
        row["ok"] = False
        row["error"] = str(exc)
        return row
    row["ok"] = proc.returncode == 0
    row["exit"] = proc.returncode
    row["stderr_tail"] = _tail(proc.stderr)
    if proc.returncode != 0:
        row["stdout_tail"] = _tail(proc.stdout)
    return row


def innounp_extract(installer: Path, dest: Path) -> dict:
    exe = find_innounp()
    row: dict = {"installer": str(installer), "dest": str(dest)}
    if exe is None:
        row["ok"] = False
        row["error"] = "innounp missing"
        return row
    dest.mkdir(parents=True, exist_ok=True)
    try:
        proc = subprocess.run(
            [str(exe), "-x", f"-d{dest}", "-y", str(installer)],
            capture_output=True,
            text=True,
            timeout=300,
        )
    except (subprocess.TimeoutExpired, OSError) as exc:
        row["ok"] = False
        row["error"] = str(exc)
        return row
    row["ok"] = proc.returncode == 0
    row["exit"] = proc.returncode
    row["stderr_tail"] = _tail(proc.stderr)
    return row


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
    meta: dict = {"installshield": [], "inno": [], "notes": []}
    seen: set[str] = set()

    def mark(src: Path) -> bool:
        try:
            key = str(src.resolve()).lower()
        except OSError:
            key = str(src).lower()
        if key in seen:
            return False
        seen.add(key)
        return True

    def drain_iscabs(root: Path) -> None:
        pulled = 0
        while pulled < 8:
            nxt = None
            for cab in root.rglob("*"):
                if not cab.is_file() or not _is_iscab(cab):
                    continue
                try:
                    key = str(cab.resolve()).lower()
                except OSError:
                    key = str(cab).lower()
                if key not in seen:
                    nxt = cab
                    break
            if nxt is None:
                return
            mark(nxt)
            pulled += 1
            dest = root / "_unshield" / f"{nxt.stem}-{pulled}"
            meta["installshield"].append(unshield_extract(nxt, dest))

    def extract_one(src: Path, dest: Path, depth: int) -> list[str]:
        if depth > max_depth or not mark(src):
            return []
        dest.mkdir(parents=True, exist_ok=True)
        if _is_iscab(src):
            meta["installshield"].append(unshield_extract(src, dest))
        else:
            subprocess.run(
                [str(SEVEN), "x", "-y", f"-o{dest}", str(src)],
                capture_output=True,
                timeout=300,
            )
            if src.suffix.lower() == ".exe" and _file_contains(src, INNO_MAGIC):
                meta["inno"].append(innounp_extract(src, dest / "_inno"))
        drain_iscabs(dest)
        found = [str(sys) for sys in dest.rglob("*.sys")]
        if depth < max_depth:
            for nested in list(dest.rglob("*")):
                if not nested.is_file():
                    continue
                if nested.suffix.lower() not in {".exe", ".msi", ".7z", ".zip", ".cab"}:
                    continue
                try:
                    if nested.stat().st_size < 200_000:
                        continue
                except OSError:
                    continue
                sub = dest / "_nested" / nested.stem
                found.extend(extract_one(nested, sub, depth + 1))
        return found

    kind = "plain"
    if _is_iscab(p) or _file_contains(p, b"InstallShield"):
        kind = "installshield"
    elif p.suffix.lower() == ".exe" and _file_contains(p, INNO_MAGIC):
        kind = "inno"
    elif p.suffix.lower() == ".msi":
        kind = "msi"

    sys_paths = extract_one(p, out, 0)
    unique = sorted(set(sys_paths))
    if meta["installshield"] and kind == "plain":
        kind = "installshield"
    if meta["inno"] and kind == "plain":
        kind = "inno"
    if kind == "installshield" and not meta["installshield"]:
        meta["notes"].append("InstallShield marker found, but no ISc( cabinet was available for unshield.")
    elif kind == "installshield" and not any(row.get("ok") for row in meta["installshield"]):
        meta["notes"].append("unshield ran on an InstallShield cabinet and did not extract it. See installshield[].stderr_tail.")
    if any(row.get("error", "").startswith("unshield missing") for row in meta["installshield"]):
        meta["notes"].append(f"Place unshield.exe at {UNSHIELD}.")
    profiles = []
    seen_sha: set[str] = set()
    for sp in unique:
        sha = sha256_file(Path(sp))
        if sha in seen_sha:
            continue
        seen_sha.add(sha)
        profiles.append({"path": sp, "name": Path(sp).name, "sha256": sha})
        if len(profiles) >= 25:
            break
    return {
        "installer": str(p),
        "installer_kind": kind,
        "extract_root": str(out),
        "sys_count": len(unique),
        "sys_files": profiles,
        "installshield": meta["installshield"],
        "inno": meta["inno"],
        "notes": meta["notes"],
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


_IOCTL_LINE = re.compile(
    r"IOCTL 0x([0-9A-Fa-f]+) pa=0x([0-9A-Fa-f]+) ok=(True|False) gle=(\d+) got=(\d+) bytes=(.*)"
)


def parse_siv_phys_stdout(guest_stdout: str) -> dict:
    """Parse guest-siv-phys-smoke.ps1 lines. Read-only record; never a write or mapper result."""
    reads = []
    for match in _IOCTL_LINE.finditer(guest_stdout or ""):
        reads.append(
            {
                "code": int(match.group(1), 16),
                "pa": int(match.group(2), 16),
                "ok": match.group(3) == "True",
                "gle": int(match.group(4)),
                "got": int(match.group(5)),
                "bytes": match.group(6).strip(),
            }
        )
    hit = next((row for row in reads if row["code"] == 0x10 and row["pa"] == 0x1000 and row["ok"]), None)
    return {
        "guard": "read_only_ioctl_0x10_pa_0x1000",
        "mapper": False,
        "write_attempted": False,
        "reads": reads,
        "phys_read_ok": hit is not None,
        "sample_bytes": hit["bytes"] if hit else "",
    }


def merge_phys_smoke_result(card_sha_prefix: str, guest_stdout: str, guest_exit: int) -> dict | None:
    CRACK_ROOT.mkdir(parents=True, exist_ok=True)
    card = None
    for folder in CRACK_ROOT.iterdir():
        if folder.is_dir() and folder.name.lower().startswith(card_sha_prefix.lower()):
            candidate = folder / "card.json"
            if candidate.is_file():
                card = candidate
                break
    if not card:
        return None
    try:
        data = json.loads(card.read_text(encoding="utf-8"))
    except json.JSONDecodeError:
        return None
    parsed = parse_siv_phys_stdout(guest_stdout)
    data["vm_phys"] = {
        "utc": datetime.now(timezone.utc).isoformat(),
        "exit": guest_exit,
        "stdout_tail": guest_stdout.splitlines()[-20:],
        **parsed,
    }
    card.write_text(json.dumps(data, indent=2), encoding="utf-8")
    return data
