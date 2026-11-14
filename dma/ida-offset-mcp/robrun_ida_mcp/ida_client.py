from __future__ import annotations

import json
import os
import subprocess
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path

DEFAULT_URL = os.environ.get("ROBRUN_IDA_URL", "http://127.0.0.1:13338")
TIMEOUT = 8.0
WORKER_WAIT_SEC = 120.0
PKG_ROOT = Path(__file__).resolve().parent.parent
PID_FILE = Path(os.environ.get("TEMP", str(PKG_ROOT))) / "robrun-ida-worker.pid"
LOG_FILE = Path(r"C:\Users\baile\OneDrive\Desktop\Development\debug\ida-offset-worker.log")


class IdaNotAttached(RuntimeError):
    pass


def _health() -> dict | None:
    try:
        with urllib.request.urlopen(DEFAULT_URL + "/health", timeout=2.0) as resp:
            payload = json.loads(resp.read().decode("utf-8"))
        if payload.get("ok"):
            return payload.get("result") or payload
    except Exception:
        return None
    return None


def call(method: str, params: dict | None = None) -> dict:
    ensure_worker(wait=True)
    body = json.dumps({"jsonrpc": "2.0", "id": 1, "method": method, "params": params or {}}).encode(
        "utf-8"
    )
    req = urllib.request.Request(
        DEFAULT_URL + "/rpc",
        data=body,
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    try:
        with urllib.request.urlopen(req, timeout=TIMEOUT) as resp:
            payload = json.loads(resp.read().decode("utf-8"))
    except urllib.error.URLError as exc:
        raise IdaNotAttached(f"headless IDA worker not reachable at {DEFAULT_URL}: {exc}") from exc
    if payload.get("error"):
        err = payload["error"]
        raise RuntimeError(err.get("message") if isinstance(err, dict) else str(err))
    return payload.get("result") or {}


def _pid_alive(pid: int) -> bool:
    if pid <= 0:
        return False
    if sys.platform == "win32":
        try:
            import ctypes

            handle = ctypes.windll.kernel32.OpenProcess(0x1000, False, pid)
            if handle:
                ctypes.windll.kernel32.CloseHandle(handle)
                return True
        except Exception:
            return False
        return False
    try:
        os.kill(pid, 0)
        return True
    except OSError:
        return False


def _spawn() -> int:
    python = sys.executable
    LOG_FILE.parent.mkdir(parents=True, exist_ok=True)
    log = open(LOG_FILE, "w", encoding="utf-8")
    kwargs: dict = {
        "cwd": str(PKG_ROOT),
        "stdout": log,
        "stderr": subprocess.STDOUT,
        "stdin": subprocess.DEVNULL,
        "env": {**os.environ, "PYTHONUNBUFFERED": "1"},
    }
    if sys.platform == "win32":
        kwargs["creationflags"] = subprocess.CREATE_NO_WINDOW | getattr(
            subprocess, "CREATE_NEW_PROCESS_GROUP", 0
        )
    proc = subprocess.Popen(
        [python, "-m", "robrun_ida_mcp.idalib_worker"],
        **kwargs,
    )
    PID_FILE.write_text(str(proc.pid), encoding="utf-8")
    return proc.pid


def ensure_worker(wait: bool = True) -> dict:
    info = _health()
    if info:
        info["attached"] = True
        return info
    pid = 0
    if PID_FILE.is_file():
        try:
            pid = int(PID_FILE.read_text(encoding="utf-8").strip())
        except ValueError:
            pid = 0
    if not _pid_alive(pid):
        pid = _spawn()
    if not wait:
        return {"attached": False, "starting": True, "pid": pid, "log": str(LOG_FILE)}
    deadline = time.time() + WORKER_WAIT_SEC
    last = None
    while time.time() < deadline:
        last = _health()
        if last:
            last["attached"] = True
            last["pid"] = pid
            return last
        time.sleep(0.4)
    raise IdaNotAttached(
        f"headless IDA worker pid {pid} did not open {DEFAULT_URL} within {int(WORKER_WAIT_SEC)}s; "
        f"see {LOG_FILE}"
    )


def attached() -> dict:
    try:
        return ensure_worker(wait=False)
    except Exception as exc:
        return {"attached": False, "error": str(exc)}
