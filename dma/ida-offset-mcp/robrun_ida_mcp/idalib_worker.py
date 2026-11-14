"""Headless IDA worker. No window. import idapro must stay first."""

from __future__ import annotations

import os
import sys
import traceback
from pathlib import Path

import idapro

idapro.enable_console_messages(False)

DEFAULT_IDB = Path(r"D:\SteamLibrary\steamapps\common\Rob Run\GameAssembly.dll.i64")
HOST = os.environ.get("ROBRUN_IDA_HOST", "127.0.0.1")
PORT = int(os.environ.get("ROBRUN_IDA_PORT", "13338"))


def _idb_path() -> Path:
    env = os.environ.get("ROBRUN_IDB")
    if env:
        return Path(env)
    game = os.environ.get("ROBRUN_GAME")
    if game:
        cand = Path(game) / "GameAssembly.dll.i64"
        if cand.is_file():
            return cand
        dll = Path(game) / "GameAssembly.dll"
        if dll.is_file():
            return dll
    return DEFAULT_IDB


def main() -> int:
    idb = _idb_path()
    if not idb.is_file():
        print(f"robrun-ida worker: IDB not found: {idb}", file=sys.stderr)
        return 2
    rc = idapro.open_database(str(idb), run_auto_analysis=False)
    if rc != 0:
        print(f"robrun-ida worker: open_database failed rc={rc} path={idb}", file=sys.stderr)
        return 3
    try:
        try:
            import ida_hexrays

            ida_hexrays.init_hexrays_plugin()
        except Exception:
            pass
        from robrun_ida_mcp.ida_rpc import run_server

        print(f"robrun-ida worker: {idb.name} on {HOST}:{PORT}", file=sys.stderr)
        run_server(HOST, PORT, threaded=False)
        return 0
    except Exception:
        traceback.print_exc()
        return 4
    finally:
        try:
            idapro.close_database(False)
        except Exception:
            pass


if __name__ == "__main__":
    sys.exit(main())
