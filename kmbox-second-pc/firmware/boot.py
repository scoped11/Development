# RobRunDMA USB KMBox B / B+ boot — AC-aligned (EAC / BE / ACE dumps).
# ROBRUN_ONBOARD_REV=2026-11-13-identity-v3
# ac_profile.txt + kmbox_identity.txt (VID/PID + optional strings). Not an AC bypass.

import gc

_AC_ONBOARD = {
    "eac": {"freq": 1000, "delay": 120},
    "be": {"freq": 1000, "delay": 150},
    "ace": {"freq": 1000, "delay": 100},
    "robrun": {"freq": 1000, "delay": 120},
}


def _read_profile():
    try:
        with open("ac_profile.txt", "r") as f:
            return f.read().strip().lower()
    except OSError:
        return "robrun"


def _parse_id_token(raw):
    s = raw.strip().lower()
    if not s or s in ("-", "0", "none", "stock"):
        return None
    if s.startswith("0x"):
        s = s[2:]
    try:
        return int(s, 16)
    except ValueError:
        return None


def _read_identity():
    data = {
        "preset": None,
        "vid": None,
        "pid": None,
        "manufacturer": None,
        "product": None,
        "serial": None,
        "bcd_device": None,
    }
    try:
        with open("kmbox_identity.txt", "r") as f:
            for line in f:
                line = line.strip()
                if not line or line[0] == "#":
                    continue
                if "=" not in line:
                    continue
                key, val = line.split("=", 1)
                key = key.strip().lower()
                val = val.strip()
                if key == "preset":
                    if val:
                        data["preset"] = val.lower()
                    continue
                if not val:
                    continue
                if key == "vid":
                    data["vid"] = _parse_id_token(val)
                elif key == "pid":
                    data["pid"] = _parse_id_token(val)
                elif key == "manufacturer":
                    data["manufacturer"] = val
                elif key == "product":
                    data["product"] = val
                elif key == "serial":
                    data["serial"] = val
                elif key in ("bcd_device", "bcddevice", "bcd"):
                    data["bcd_device"] = _parse_id_token(val)
    except OSError:
        pass
    return data


def _device_call(name, *args):
    if device is None:
        return False
    fn = getattr(device, name, None)
    if not callable(fn):
        return False
    try:
        fn(*args)
        return True
    except Exception:
        return False


def _apply_identity(ident):
    if device is None:
        return
    vid, pid = ident.get("vid"), ident.get("pid")
    touched = False
    if vid is not None and pid is not None:
        if _device_call("VID", vid) or _device_call("vid", vid):
            touched = True
        if _device_call("PID", pid) or _device_call("pid", pid):
            touched = True
    bcd = ident.get("bcd_device")
    if bcd is not None:
        for api in ("bcdDevice", "BCDDevice", "bcd_device", "device_bcd"):
            if _device_call(api, bcd):
                touched = True
                break
    mfg = ident.get("manufacturer")
    if mfg:
        for api in ("manufacturer", "Manufacturer", "mfg", "iManufacturer"):
            if _device_call(api, mfg):
                touched = True
                break
    prod = ident.get("product")
    if prod:
        for api in ("product", "Product", "name", "iProduct"):
            if _device_call(api, prod):
                touched = True
                break
    sn = ident.get("serial")
    if sn:
        for api in ("serial", "Serial", "serial_number", "iSerialNumber"):
            if _device_call(api, sn):
                touched = True
                break
    # VID/PID only. Do not call device.enable() here — that re-enumerates the
    # HID USB every boot and sounds like unplug/replug if the MCU is reset.


try:
    import km
except ImportError:
    km = None

try:
    import device
except ImportError:
    device = None

try:
    import bt
except ImportError:
    bt = None

prof = _read_profile()
if prof == "robrun":
    tun_key = "eac"
else:
    tun_key = prof if prof in _AC_ONBOARD else "eac"
cfg = _AC_ONBOARD.get(tun_key, _AC_ONBOARD["eac"])

if km is not None:
    try:
        km.mode(1)
    except Exception:
        pass
    try:
        km.freq(cfg["freq"])
    except Exception:
        pass
    try:
        km.delay(cfg["delay"])
    except Exception:
        pass
    try:
        km.echo(1)
    except Exception:
        pass

# USB VID/PID apply is disabled. device.enable/VID on boot re-enumerates HID
# and can click-loop if the MCU resets. Serial km.* still works.
# _apply_identity(_read_identity())

if bt is not None:
    try:
        bt.enable(0)
    except Exception:
        pass

gc.collect()
