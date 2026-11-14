#!/usr/bin/env python3
"""KM box setup: list COM ports, probe km.*, push onboard scripts.

Onboard USB B+ scripts live in dma/kmbox-usb/ (boot.py + main.py).
`push` writes those over the MicroPython REPL. It is not a vendor .bin burn.
`flash` stays opt-in and is not invoked unless you pass that subcommand.
"""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
import time
from pathlib import Path

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    print("Missing pyserial. Run: python -m pip install pyserial", file=sys.stderr)
    raise SystemExit(1)

BAUDS = (115200, 4_000_000, 921600, 2_000_000)
KM_MARKERS = (
    "KMBOX",
    "K-BOX",
    "WINCHIPHEAD",
    "CH340",
    "CH341",
    "FERRUM",
    "MAKCU",
    "CH343",
    "CH9102",
    "USB-SERIAL",
    "USB SERIAL",
    "1A86",
    "CP210",
    "FTDI",
)


def port_sort_key(desc: str) -> tuple[int, str]:
    upper = desc.upper()
    if "BLUETOOTH" in upper:
        return (2, desc)
    if any(m in upper for m in KM_MARKERS):
        return (0, desc)
    return (1, desc)


def list_com_ports() -> None:
    rows = []
    for p in list_ports.comports():
        rows.append((p.device, p.description or "", p.hwid or ""))
    rows.sort(key=lambda r: port_sort_key(f"{r[1]} {r[2]}"))
    if not rows:
        print("No COM ports found.")
        return
    for device, desc, hwid in rows:
        extra = f"  [{hwid}]" if hwid else ""
        print(f"{device}\t{desc}{extra}")


def alive_reply(text: str) -> bool:
    t = text.lower()
    return (
        "km." in text
        or "kmbox" in t
        or "ferrum" in t
        or ">>>" in text
    )


def probe_port(device: str, baud: int, timeout: float = 0.35) -> tuple[bool, str]:
    try:
        ser = serial.Serial(
            device,
            baud,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            timeout=0.05,
            write_timeout=0.5,
        )
    except serial.SerialException as exc:
        return False, str(exc)
    try:
        ser.dtr = True
        ser.rts = True
        ser.reset_input_buffer()
        ser.reset_output_buffer()
        time.sleep(0.04)
        last = ""
        for prefix in ("km.", "."):
            ser.reset_input_buffer()
            cmd = f"{prefix}version()\r\n".encode()
            ser.write(cmd)
            ser.flush()
            deadline = time.monotonic() + timeout
            buf = bytearray()
            while time.monotonic() < deadline:
                chunk = ser.read(256)
                if chunk:
                    buf.extend(chunk)
                    text = buf.decode("utf-8", errors="replace")
                    if alive_reply(text) or ">>>" in text:
                        ser.close()
                        return True, text.strip()
                else:
                    time.sleep(0.02)
            if buf:
                last = buf.decode("utf-8", errors="replace").strip()
        ser.close()
        return False, last or "no reply"
    except Exception as exc:
        try:
            ser.close()
        except Exception:
            pass
        return False, str(exc)


def com_number(device: str) -> int | None:
    m = re.search(r"COM(\d+)$", device.upper())
    return int(m.group(1)) if m else None


def is_legacy_phantom_com(port) -> bool:
    hw = (port.hwid or "").upper()
    desc = (port.description or "").upper()
    if "PNP0501" in hw or "COMMUNICATIONS PORT" in desc:
        return True
    return False


def cmd_probe(explicit_com: int | None) -> int:
    candidates: list[str] = []
    if explicit_com:
        candidates.append(f"COM{explicit_com}")
    for p in sorted(list_ports.comports(), key=lambda x: port_sort_key(x.description or "")):
        if explicit_com is None and is_legacy_phantom_com(p):
            continue
        if p.device not in candidates:
            candidates.append(p.device)

    if not candidates:
        print("[!] No serial ports.")
        return 1

    for device in candidates:
        for baud in BAUDS:
            print(f"[*] Trying {device} @ {baud} ...")
            ok, reply = probe_port(device, baud)
            if ok:
                print(f"[+] KM device on {device} @ {baud}")
                print(reply[:500])
                return 0
            if reply and reply != "no reply":
                print(f"    ({reply[:120]})")
    print("[!] No km.version reply. Check control USB on this PC, driver, or boot mode.")
    return 1


def cmd_test_move(device: str, baud: int) -> int:
    pattern = [(36, 0), (0, 36), (-36, 0), (0, -36)]
    try:
        ser = serial.Serial(device, baud, timeout=0.1, write_timeout=0.5)
    except serial.SerialException as exc:
        print(f"[!] Open failed: {exc}")
        return 1
    try:
        ser.dtr = ser.rts = True
        for dx, dy in pattern:
            ser.write(f"km.move({dx},{dy})\r\n".encode())
            ser.flush()
            time.sleep(0.05)
        print("[+] Sent test moves — watch cursor on the GAME PC.")
        return 0
    finally:
        ser.close()


FIRMWARE_DIR = Path(__file__).resolve().parents[1] / "kmbox-usb"


def _read_until(ser, token: bytes, timeout: float) -> bytes:
    deadline = time.monotonic() + timeout
    buf = bytearray()
    while time.monotonic() < deadline:
        chunk = ser.read(256)
        if chunk:
            buf.extend(chunk)
            if token in buf:
                return bytes(buf)
        else:
            time.sleep(0.02)
    return bytes(buf)


def enter_raw_repl(ser) -> bool:
    ser.reset_input_buffer()
    ser.write(b"\r\x03\x03")
    time.sleep(0.15)
    ser.reset_input_buffer()
    ser.write(b"\x01")
    reply = _read_until(ser, b"raw REPL", 1.5)
    return b"raw REPL" in reply or b">" in reply


def exit_raw_repl(ser) -> None:
    ser.write(b"\x02")
    time.sleep(0.05)


def raw_exec(ser, source: str, timeout: float = 4.0) -> str:
    payload = source.encode("utf-8")
    ser.write(b"\x05A\x01")
    ser.write(payload)
    ser.write(b"\x04")
    reply = _read_until(ser, b"\x04", timeout)
    return reply.decode("utf-8", errors="replace")


def write_board_file(ser, remote_name: str, text: str) -> None:
    encoded = text.encode("utf-8")
    raw_exec(ser, "import uos as os")
    chunks = [encoded[i : i + 64] for i in range(0, len(encoded), 64)] or [b""]
    raw_exec(ser, f"f=open({remote_name!r},'wb')")
    for chunk in chunks:
        raw_exec(ser, "f.write(" + repr(chunk) + ")")
    raw_exec(ser, "f.close()")


AC_CHOICES = ("eac", "be", "ace", "robrun")


def sync_host_ac_profile(ac: str) -> None:
    ac = ac.lower().strip()
    if ac not in AC_CHOICES:
        ac = "robrun"
    repo_root = Path(__file__).resolve().parents[2]
    for rel in ("debug/kmbox_ac_profile.txt", "kmbox_ac_profile.txt"):
        path = repo_root / rel
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(ac + "\n", encoding="utf-8")
    print(f"[+] Host profile file: {ac} (KmboxUsb reads on Connect)")


def cmd_push(device: str, baud: int, ac: str) -> int:
    boot = FIRMWARE_DIR / "boot.py"
    main = FIRMWARE_DIR / "main.py"
    ac_line = ac.lower().strip()
    if ac_line not in AC_CHOICES:
        print(f"[!] Unknown --ac {ac!r}; use one of {AC_CHOICES}")
        return 1
    if not boot.is_file() or not main.is_file():
        print(f"[!] Missing {boot} or {main}")
        return 1
    try:
        ser = serial.Serial(device, baud, timeout=0.1, write_timeout=1.0)
    except serial.SerialException as exc:
        print(f"[!] Open failed: {exc}")
        return 1
    try:
        ser.dtr = ser.rts = True
        time.sleep(0.2)
        if not enter_raw_repl(ser):
            print("[!] Could not enter raw REPL. Box must be USB B+ with km MicroPython.")
            return 1
        write_board_file(ser, "boot.py", boot.read_text(encoding="utf-8"))
        write_board_file(ser, "main.py", main.read_text(encoding="utf-8"))
        write_board_file(ser, "ac_profile.txt", ac_line + "\n")
        identity = FIRMWARE_DIR / "kmbox_identity.txt"
        if identity.is_file():
            write_board_file(ser, "kmbox_identity.txt", identity.read_text(encoding="utf-8"))
            print(f"[+] Pushed {identity.name}")
        exit_raw_repl(ser)
        sync_host_ac_profile(ac_line)
        print(f"[+] Pushed {boot.name}, {main.name}, ac_profile.txt={ac_line}")
        print("[*] Power-cycle the box (unplug both USBs) so boot.py runs.")
        return 0
    finally:
        try:
            exit_raw_repl(ser)
        except Exception:
            pass
        ser.close()


def cmd_flash_esptool(
    com: str,
    firmware: Path,
    offset: str,
    baud: int,
    extra_args: list[str],
) -> int:
    if not firmware.is_file():
        print(f"[!] Firmware not found: {firmware}")
        return 1
    esptool = [sys.executable, "-m", "esptool"]
    try:
        subprocess.run([*esptool, "version"], check=True, capture_output=True)
    except (subprocess.CalledProcessError, FileNotFoundError):
        print("[!] esptool not installed. Run: python -m pip install esptool")
        return 1

    flash_cmd = [
        *esptool,
        "--chip",
        "auto",
        "--port",
        com,
        "--baud",
        str(baud),
        "write_flash",
        offset,
        str(firmware),
        *extra_args,
    ]
    print("[*] Flashing (unplug game PC USB first; use BOOT if flash fails):")
    print("    " + " ".join(flash_cmd))
    return subprocess.call(flash_cmd)


def main() -> int:
    parser = argparse.ArgumentParser(description="KM box probe and ESP flash helper")
    sub = parser.add_subparsers(dest="cmd", required=True)

    p_list = sub.add_parser("list", help="List COM ports")
    p_list.set_defaults(func=lambda _a: (list_com_ports(), 0)[1])

    p_probe = sub.add_parser("probe", help="Find KM device (km.version)")
    p_probe.add_argument("--com", type=int, default=0, help="Try this COM first")
    p_probe.set_defaults(
        func=lambda a: cmd_probe(a.com if a.com > 0 else None)
    )

    p_move = sub.add_parser("test-move", help="Send square move pattern")
    p_move.add_argument("--com", required=True, help="e.g. COM5")
    p_move.add_argument("--baud", type=int, default=115200)
    p_move.set_defaults(func=lambda a: cmd_test_move(a.com.upper(), a.baud))

    p_push = sub.add_parser(
        "push",
        help="Write dma/kmbox-usb boot.py+main.py over MicroPython REPL",
    )
    p_push.add_argument("--com", required=True, help="e.g. COM5 (control USB)")
    p_push.add_argument("--baud", type=int, default=115200)
    p_push.add_argument(
        "--ac",
        default="robrun",
        choices=AC_CHOICES,
        help="EAC/BE/ACE-aligned onboard+host tuning (not kernel IOCTL)",
    )
    p_push.set_defaults(
        func=lambda a: cmd_push(a.com.upper(), a.baud, a.ac)
    )

    p_flash = sub.add_parser("flash", help="ESP write_flash via esptool")
    p_flash.add_argument("--com", required=True)
    p_flash.add_argument("--firmware", type=Path, required=True)
    p_flash.add_argument(
        "--offset",
        default="0x0",
        help="Flash offset from custom FW readme (default 0x0)",
    )
    p_flash.add_argument("--baud", type=int, default=921600)
    p_flash.add_argument("esptool_extra", nargs="*", help="Extra esptool args")
    p_flash.set_defaults(
        func=lambda a: cmd_flash_esptool(
            a.com.upper(), a.firmware, a.offset, a.baud, a.esptool_extra
        )
    )

    args = parser.parse_args()
    return int(args.func(args))


if __name__ == "__main__":
    raise SystemExit(main())
