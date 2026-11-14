"""
Host kernel dump via WinDbg kd.exe over the VirtualBox COM pipe.

Flow: lm (or break on load) -> .writemem to audit/dumps/raw -> register_memory_dump.
Does not power off the VM. Does not bypass anti-cheat.
"""
from __future__ import annotations

import json
import re
import subprocess
import uuid
from datetime import datetime, timezone
from pathlib import Path

import sys

_REVERSAL = Path(__file__).resolve().parent
_CRACK = _REVERSAL.parent / "crack"
for _p in (str(_REVERSAL), str(_CRACK)):
    if _p not in sys.path:
        sys.path.insert(0, _p)

from dump_registry import HOST_DROP, scan_host_drop  # noqa: E402
from crack_lib import AUDIT, resolve_path, sha256_file  # noqa: E402

LAB = AUDIT.parent
RAW = AUDIT / "dumps" / "raw"
SESSIONS = AUDIT / "dumps" / "kd-sessions"
GENERATED = Path(__file__).resolve().parent / "windbg" / "generated"
VBOX = Path(r"C:\Program Files\Oracle\VirtualBox\VBoxManage.exe")
VM_NAME = "DriverLab-E-26200"
PIPE = r"\\.\pipe\DriverLabKD"
ADDR_RE = re.compile(r"[0-9a-f]{8,16}(?:`[0-9a-f]{8})?", re.I)


def _utc() -> str:
    return datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")


def find_kd() -> Path | None:
    roots = [
        Path(r"C:\Program Files\WindowsApps"),
        Path(r"C:\Program Files (x86)\Windows Kits\10\Debuggers\x64"),
        Path(r"C:\Program Files\Windows Kits\10\Debuggers\x64"),
    ]
    found: list[Path] = []
    apps = roots[0]
    if apps.is_dir():
        found.extend(apps.glob("Microsoft.WinDbg_*_x64__8wekyb3d8bbwe/amd64/kd.exe"))
    for root in roots[1:]:
        candidate = root / "kd.exe"
        if candidate.is_file():
            found.append(candidate)
    found = [p for p in found if p.is_file() and p.stat().st_size > 1000]
    return sorted(found, key=lambda p: p.stat().st_mtime, reverse=True)[0] if found else None


def connection_string(pipe: str = PIPE) -> str:
    return f"com:pipe,port={pipe},resets=0,reconnect"


def vm_uart_state(vm_name: str = VM_NAME) -> dict:
    if not VBOX.is_file():
        return {"error": "VBoxManage missing", "vbox": str(VBOX)}
    proc = subprocess.run(
        [str(VBOX), "showvminfo", vm_name, "--machinereadable"],
        capture_output=True,
        text=True,
        timeout=30,
    )
    info: dict[str, str] = {}
    for line in (proc.stdout or "").splitlines():
        if "=" not in line:
            continue
        key, _, val = line.partition("=")
        info[key.strip()] = val.strip().strip('"')
    return {
        "vm": vm_name,
        "state": info.get("VMState", ""),
        "uart1": info.get("uart1", ""),
        "uartmode1": info.get("uartmode1", ""),
        "pipe_ready": info.get("uart1", "") not in ("", "off")
        and (
            "pipe" in (info.get("uartmode1", "") + info.get("uart1", "")).lower()
            or "DriverLabKD" in (proc.stdout or "")
        ),
        "raw_uartmode1": info.get("uartmode1", ""),
    }


def parse_module_range(text: str, module: str) -> dict | None:
    """Parse `lm m` start/end for a module. Returns base and size in bytes."""
    stem = Path(module).stem.lower()
    for line in text.splitlines():
        low = line.lower()
        if stem not in low:
            continue
        addrs = ADDR_RE.findall(line)
        if len(addrs) < 2:
            continue
        start = int(addrs[0].replace("`", ""), 16)
        end = int(addrs[1].replace("`", ""), 16)
        if end > start:
            return {"base": start, "end": end, "size": end - start, "line": line.strip()}
    return None


def fmt_kd_addr(value: int) -> str:
    return f"{value >> 32:08x}`{value & 0xFFFFFFFF:08x}"


def write_script(name: str, body: str) -> Path:
    GENERATED.mkdir(parents=True, exist_ok=True)
    path = GENERATED / name
    path.write_text(body.replace("\r\n", "\n").replace("\n", "\r\n"), encoding="utf-8")
    return path


def run_kd_script(script: Path, timeout: int, pipe: str = PIPE) -> dict:
    kd = find_kd()
    if kd is None:
        return {"ok": False, "error": "kd.exe not found (WinDbg)"}
    # $<file runs the script inside kd.
    cmd = [str(kd), "-k", connection_string(pipe), "-c", f"$<{script}"]
    try:
        proc = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
    except subprocess.TimeoutExpired as exc:
        return {
            "ok": False,
            "error": "kd_timeout",
            "timeout": timeout,
            "stdout": (exc.stdout or "")[-2000:] if isinstance(exc.stdout, str) else "",
            "hint": "Pipe not connected, or guest is not waiting on a kernel break. UART must be configured before boot.",
        }
    except OSError as exc:
        return {"ok": False, "error": str(exc)}
    text = (proc.stdout or "") + "\n" + (proc.stderr or "")
    return {"ok": proc.returncode == 0, "exit": proc.returncode, "output": text[-8000:], "kd": str(kd)}


def _phase1_script(module_stem: str, wait_load: bool) -> str:
    lines = ["$$ driverlab phase 1 — locate mapped image", ".echo DRIVERLAB_PHASE1"]
    if wait_load:
        lines += [f"sxe ld:{module_stem}.sys", "g"]
    lines += [f"lm m {module_stem}", ".echo DRIVERLAB_PHASE1_END", "q", ""]
    return "\n".join(lines)


def _phase2_script(dump_path: Path, base: int, size: int) -> str:
    # .writemem length is a byte count with the L prefix.
    return "\n".join(
        [
            "$$ driverlab phase 2 — write mapped image",
            ".echo DRIVERLAB_PHASE2",
            f'.writemem "{dump_path}" {fmt_kd_addr(base)} L{size:x}',
            ".echo DRIVERLAB_WRITEMEM_DONE",
            "q",
            "",
        ]
    )


def emit_sidecar(disk: Path, dump: Path, base: int, size: int, note: str) -> Path:
    HOST_DROP.mkdir(parents=True, exist_ok=True)
    side = HOST_DROP / f"kd-{_utc()}-{disk.stem}.json"
    side.write_text(
        json.dumps(
            {
                "disk_sys_path": str(disk),
                "dump_path": str(dump),
                "capture_method": "kd_writemem",
                "kernel_base": hex(base),
                "size_of_image": size,
                "note": note,
            },
            indent=2,
        ),
        encoding="utf-8",
    )
    return side


def kd_dump_module(
    disk_sys: str,
    *,
    module: str = "",
    wait_load: bool = False,
    timeout: int = 90,
    dry_run: bool = False,
    register: bool = True,
) -> dict:
    disk = resolve_path(disk_sys)
    if not disk.is_file():
        return {"error": f"disk sys not found: {disk_sys}"}
    stem = Path(module).stem if module else disk.stem
    uart = vm_uart_state()
    kd = find_kd()
    stamp = _utc()
    RAW.mkdir(parents=True, exist_ok=True)
    SESSIONS.mkdir(parents=True, exist_ok=True)
    dump_path = RAW / f"{stamp}-{stem}.bin"
    phase1 = write_script(f"{stamp}-{stem}-phase1.txt", _phase1_script(stem, wait_load))
    receipt: dict = {
        "utc": datetime.now(timezone.utc).isoformat(),
        "disk_sys": str(disk),
        "disk_sha256": sha256_file(disk),
        "module": stem,
        "kd": str(kd) if kd else None,
        "pipe": PIPE,
        "connection": connection_string(),
        "uart": uart,
        "phase1_script": str(phase1),
        "dump_path": str(dump_path),
        "wait_load": wait_load,
    }
    if dry_run:
        receipt["status"] = "dry_run"
        _save_receipt(receipt)
        return receipt
    if kd is None:
        receipt["status"] = "blocked"
        receipt["error"] = "kd.exe not found"
        _save_receipt(receipt)
        return receipt
    pipe_ready = bool(uart.get("pipe_ready"))
    if not pipe_ready:
        receipt["status"] = "blocked"
        receipt["error"] = "uart_not_ready"
        receipt["fix"] = [
            "Power off DriverLab-E-26200 (do not save state).",
            "powershell -File driver-lab\\vm\\host-enable-kd-uart.ps1",
            "In the guest (elevated): driver-lab\\vm\\bg\\guest-enable-kd-serial.ps1",
            "Reboot the guest, then rerun kd_dump without --dry-run.",
        ]
        _save_receipt(receipt)
        return receipt

    probe_timeout = timeout if wait_load else min(timeout, 45)
    phase1_run = run_kd_script(phase1, probe_timeout)
    receipt["phase1"] = {k: phase1_run.get(k) for k in ("ok", "exit", "error", "hint")}
    output = phase1_run.get("output") or ""
    located = parse_module_range(output, stem)
    if not located:
        receipt["status"] = "module_not_mapped"
        receipt["error"] = "lm did not show the module"
        receipt["phase1_output_tail"] = output[-1500:]
        if not wait_load:
            receipt["hint"] = "Load the driver in the guest, or rerun with wait_load=true before sc start."
        _save_receipt(receipt)
        return receipt

    phase2 = write_script(
        f"{stamp}-{stem}-phase2.txt",
        _phase2_script(dump_path, located["base"], located["size"]),
    )
    receipt["phase2_script"] = str(phase2)
    receipt["mapped"] = {"base": hex(located["base"]), "size": located["size"]}
    phase2_run = run_kd_script(phase2, timeout)
    receipt["phase2"] = {k: phase2_run.get(k) for k in ("ok", "exit", "error")}
    if not dump_path.is_file() or dump_path.stat().st_size < 0x400:
        receipt["status"] = "writemem_missing"
        receipt["error"] = "kd did not write a dump file"
        receipt["phase2_output_tail"] = (phase2_run.get("output") or "")[-1500:]
        _save_receipt(receipt)
        return receipt

    side = emit_sidecar(disk, dump_path, located["base"], located["size"], f"kd {stem} {stamp}")
    receipt["sidecar"] = str(side)
    if register:
        ingested = scan_host_drop()
        receipt["register"] = ingested
        receipt["status"] = "registered" if any(
            (row.get("register") or {}).get("status") in ("validated", "duplicate") for row in ingested.get("processed") or []
        ) else "sidecar_written"
    else:
        receipt["status"] = "sidecar_written"
    _save_receipt(receipt)
    return receipt


def _save_receipt(body: dict) -> None:
    SESSIONS.mkdir(parents=True, exist_ok=True)
    name = f"{_utc()}-{uuid.uuid4().hex[:8]}.json"
    (SESSIONS / name).write_text(json.dumps(body, indent=2), encoding="utf-8")
    body["receipt"] = str(SESSIONS / name)


def main() -> int:
    import argparse

    p = argparse.ArgumentParser(description="KD writemem + dump registry sidecar")
    sub = p.add_subparsers(dest="cmd", required=True)
    d = sub.add_parser("dump", help="Dump a loaded module and register it")
    d.add_argument("disk_sys")
    d.add_argument("--module", default="")
    d.add_argument("--wait-load", action="store_true")
    d.add_argument("--timeout", type=int, default=90)
    d.add_argument("--dry-run", action="store_true")
    d.add_argument("--no-register", action="store_true")
    sub.add_parser("uart-status")
    args = p.parse_args()
    if args.cmd == "uart-status":
        out = {"kd": str(find_kd()) if find_kd() else None, "uart": vm_uart_state()}
    else:
        out = kd_dump_module(
            args.disk_sys,
            module=args.module,
            wait_load=args.wait_load,
            timeout=args.timeout,
            dry_run=args.dry_run,
            register=not args.no_register,
        )
    print(json.dumps(out, indent=2))
    return 0 if out.get("status") in ("dry_run", "registered", "sidecar_written") or "uart" in out else 1


if __name__ == "__main__":
    raise SystemExit(main())
