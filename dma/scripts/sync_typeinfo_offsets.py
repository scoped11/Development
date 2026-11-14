#!/usr/bin/env python3
"""Refresh GameAssembly TypeInfo RVAs in src/game/offsets.h from Il2CppDumper script.json."""

from __future__ import annotations

import argparse
import json
import re
from datetime import datetime, timezone
from pathlib import Path

TYPEINFO_MAP = {
    "BattleEntitiesModel": "Game.Battle.BattleEntitiesModel",
    "BattleEntitiesModelUpdater": "Game.Battle.BattleEntitiesModelUpdater",
    "BattleEntitiesUpdater": "Game.Battle.BattleEntitiesUpdater",
    "CinemachineBrain": "Cinemachine.CinemachineBrain",
    "CameraBrainUpdater": "Game.Battle.UiUpdater.CameraBrainUpdater",
    "Cinemachine3rdPersonFollow": "Cinemachine.Cinemachine3rdPersonFollow",
    "Game": "Game.Game",
    "GameModel": "Game.GameModel",
    "BattleModel": "Game.Battle.BattleModel",
    "BattleCameraModel": "Game.Battle.Entities.Player.Camera.BattleCameraModel",
    "BattleCameraModelUpdater": "Game.Battle.Entities.Player.Camera.BattleCameraModelUpdater",
}

REPO = Path(__file__).resolve().parents[1]
OFFSETS_H = REPO / "src" / "game" / "offsets.h"


def pe_size_of_image(path: Path) -> int:
    data = path.read_bytes()
    if len(data) < 0x40:
        return 0
    e_lfanew = int.from_bytes(data[0x3C:0x40], "little")
    if e_lfanew + 0x54 > len(data):
        return 0
    return int.from_bytes(data[e_lfanew + 0x50 : e_lfanew + 0x54], "little")


def load_metadata(script_path: Path) -> dict[str, int]:
    data = json.loads(script_path.read_text(encoding="utf-8"))
    out: dict[str, int] = {}
    for entry in data.get("ScriptMetadata", []):
        name = entry.get("Name") or ""
        if not name.endswith("_TypeInfo"):
            continue
        addr = entry.get("Address")
        if isinstance(addr, int):
            out[name[: -len("_TypeInfo")]] = addr
    return out


def patch_offsets_h(meta: dict[str, int], ga_size: int, dump_dir: Path) -> list[str]:
    text = OFFSETS_H.read_text(encoding="utf-8")
    lines: list[str] = []
    missing: list[str] = []

    stamp = datetime.now(timezone.utc).strftime("%Y-%m-%d")
    text = re.sub(
        r"// Dump: dumps/[^\n]+",
        f"// Dump: {dump_dir.name} (Il2CppDumper, metadata v39). "
        f"Synced {stamp} UTC. GameAssembly SizeOfImage {ga_size:#x}.",
        text,
        count=1,
    )
    for pat in (
        r"(constexpr size_t kDumpGameAssemblySize = )0[xX][0-9A-Fa-f]+ULL;",
        r"(constexpr size_t kLiveGameAssemblySize = )0[xX][0-9A-Fa-f]+ULL;",
    ):
        text, nsize = re.subn(pat, rf"\g<1>{ga_size:#X}ULL;", text, count=1)
        if nsize != 1:
            missing.append(pat)
    for key, fq in TYPEINFO_MAP.items():
        rva = meta.get(fq)
        if rva is None:
            missing.append(fq)
            continue
        pattern = rf"(constexpr uintptr_t {key} = )0x[0-9A-Fa-f]+;"
        repl = rf"\g<1>0x{rva:X};"
        new_text, n = re.subn(pattern, repl, text, count=1)
        if n != 1:
            missing.append(f"{key} (pattern)")
        else:
            text = new_text
            lines.append(f"{key} = 0x{rva:X}  ({fq})")

    OFFSETS_H.write_text(text, encoding="utf-8")
    report = dump_dir / "OFFSET_REPORT.md"
    missing_lines = [f"- {m}" for m in missing] if missing else ["- (none)"]
    body = [
        "# Offset sync",
        "",
        f"- Generated: {stamp} UTC",
        f"- GameAssembly size: `{ga_size:#x}`",
        "",
        "## TypeInfo updates",
        "",
        *[f"- `{line}`" for line in lines],
        "",
        "## Missing",
        "",
        *missing_lines,
        "",
        "**Manual:** field offsets in `offsets.h` — diff `dump.cs` for layout changes.",
        "Skeleton (`BattleCharacterModel+0x138`, `SkeletonConfig` chain) unchanged in this build.",
    ]
    report.write_text("\n".join(body), encoding="utf-8")
    return missing


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--dump-dir", type=Path, required=True)
    ap.add_argument("--game-assembly", type=Path, default=Path(r"D:\SteamLibrary\steamapps\common\Rob Run\GameAssembly.dll"))
    args = ap.parse_args()
    script = args.dump_dir / "script.json"
    if not script.is_file():
        raise SystemExit(f"script.json not found in {args.dump_dir}")
    meta = load_metadata(script)
    ga_size = (
        pe_size_of_image(args.game_assembly)
        if args.game_assembly.is_file()
        else 0
    )
    missing = patch_offsets_h(meta, ga_size, args.dump_dir)
    if missing:
        print("WARN missing:", ", ".join(missing))
    print(f"Patched {OFFSETS_H}")
    print(f"Wrote {args.dump_dir / 'OFFSET_REPORT.md'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
