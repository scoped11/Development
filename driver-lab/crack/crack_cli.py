#!/usr/bin/env python3
"""Host CLI for crack_lib."""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from crack_lib import crack_driver_profile, extract_installer, find_sys_files, pe_unpack_assess  # noqa: E402


def main() -> int:
    p = argparse.ArgumentParser(description="DriverLab crack pipeline")
    sub = p.add_subparsers(dest="cmd", required=True)

    sp = sub.add_parser("profile", help="Static card for a .sys")
    sp.add_argument("path")
    sp.add_argument("--no-write", action="store_true")

    ex = sub.add_parser("extract", help="Extract installer (7z, unshield, innounp)")
    ex.add_argument("path")
    ex.add_argument("--depth", type=int, default=4)

    fs = sub.add_parser("find-sys", help="Find .sys under directory")
    fs.add_argument("path")
    fs.add_argument("--limit", type=int, default=50)

    pe = sub.add_parser("pe-unpack", help="PE entropy / packer assess")
    pe.add_argument("path")

    args = p.parse_args()
    if args.cmd == "profile":
        out = crack_driver_profile(args.path, write_card=not args.no_write)
    elif args.cmd == "extract":
        out = extract_installer(args.path, max_depth=args.depth)
    elif args.cmd == "pe-unpack":
        out = pe_unpack_assess(args.path)
    else:
        out = {"sys": find_sys_files(Path(args.path), limit=args.limit)}
    print(json.dumps(out, indent=2))
    return 0 if "error" not in out else 1


if __name__ == "__main__":
    raise SystemExit(main())
