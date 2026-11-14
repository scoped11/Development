from __future__ import annotations

import os
from pathlib import Path

DEFAULT_GAME = Path(r"D:\SteamLibrary\steamapps\common\Rob Run")
DEFAULT_DEV = Path(r"C:\Users\baile\OneDrive\Desktop\Development")


def game_dir() -> Path:
    return Path(os.environ.get("ROBRUN_GAME", DEFAULT_GAME))


def dump_dir() -> Path:
    env = os.environ.get("ROBRUN_DUMP")
    if env:
        return Path(env)
    dumps = DEFAULT_DEV / "dumps"
    if dumps.is_dir():
        dated = sorted(
            (p for p in dumps.iterdir() if p.is_dir() and (p / "script.json").exists()),
            key=lambda p: p.name,
            reverse=True,
        )
        if dated:
            return dated[0]
    return DEFAULT_DEV / "dumps" / "2026-11-12"


def module_path(name: str) -> Path:
    key = name.lower().replace(".dll", "")
    game = game_dir()
    if key in ("gameassembly", "ga"):
        return game / "GameAssembly.dll"
    if key in ("unityplayer", "unity"):
        return game / "UnityPlayer.dll"
    path = Path(name)
    if path.is_file():
        return path
    cand = game / name
    if cand.is_file():
        return cand
    raise FileNotFoundError(f"module not found: {name}")
