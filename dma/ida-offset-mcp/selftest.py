from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT))

from robrun_ida_mcp import dump_index, pe  # noqa: E402


def expect(cond: bool, msg: str) -> None:
    if not cond:
        raise SystemExit(f"FAIL: {msg}")
    print("ok", msg)


def main() -> None:
    g = pe.confirm_getter("0x904DA0")
    expect(g.get("ok") is True, f"Game.get_GameModel {g}")
    expect(g.get("field_offset") == "0xd0", f"GameModel offset {g.get('field_offset')}")

    ge = pe.confirm_getter("0x7A9000")
    expect(ge.get("ok") is True, f"UnityEntry.get_Game {ge}")
    expect(ge.get("field_offset") == "0x28", f"_game offset {ge.get('field_offset')}")

    s = pe.confirm_singleton("0xF052F0")
    expect(s.get("ok") is True, f"ContentContext.get_Instance {s}")
    expect(s.get("typeinfo_rva") == "0x81a4d88", f"ContentContext TypeInfo {s.get('typeinfo_rva')}")

    ti = dump_index.lookup_typeinfo("Game.Game")
    names = [h["name"] for h in ti.get("hits") or []]
    expect("Game.Game" in names, f"typeinfo Game.Game {ti}")

    m = dump_index.lookup_method("Content.ContentContext$$get_Instance")
    expect((m.get("hits") or [{}])[0].get("rva") == "0xf052f0", f"get_Instance {m}")

    f = dump_index.field_from_dump("Game.Game", "GameModel")
    expect((f.get("match") or {}).get("offset") == "0xD0", f"dump GameModel {f}")

    f2 = dump_index.field_from_dump("Start.UnityEntry", "_game")
    expect((f2.get("match") or {}).get("offset") == "0x28", f"dump UnityEntry._game {f2}")

    print("all selftests passed")


if __name__ == "__main__":
    main()
