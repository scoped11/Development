#!/usr/bin/env python3
"""Extract TypeInfo RVAs and field offsets from a fresh Il2CppDumper dump."""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

DUMP_DIR = Path(r"C:\Users\baile\OneDrive\Desktop\Development\dumps\2026-11-12")
DUMP_CS = DUMP_DIR / "dump.cs"
SCRIPT = DUMP_DIR / "script.json"

TYPEINFO_NAMES = [
    "Game.Battle.BattleEntitiesModel",
    "Game.Battle.BattleEntitiesModelUpdater",
    "Game.Battle.BattleEntitiesUpdater",
    "Cinemachine.CinemachineBrain",
    "Game.Battle.UiUpdater.CameraBrainUpdater",
    "Cinemachine.Cinemachine3rdPersonFollow",
    "Game.Game",
    "Game.GameModel",
    "Game.Battle.BattleModel",
    "Game.Battle.Entities.Player.Camera.BattleCameraModel",
    "Game.Battle.Entities.Player.Camera.BattleCameraModelUpdater",
    "Models.Models.InventoryItemDescriptions",
    "Models.Models.InventorySlot.InventorySlot",
]

METHOD_NAMES = [
    "ContentContext$$get_Instance",
    "Game.ContentContext$$get_Instance",
    "Kefir.ContentContext$$get_Instance",
    "BasePrimitiveData<float>$$get_Value",
    "BasePrimitiveData<int>$$get_Value",
    "BasePrimitiveData<object>$$get_Value",
    "BasePrimitiveData<bool>$$get_Value",
    "BasePrimitiveData<UnityEngine.Vector3>$$get_Value",
]

# Exact class names as they appear after "public class " / "public sealed class "
CLASSES = [
    "BattleEntitiesModel",
    "BattleEntitiesModelUpdater",
    "BattleEntitiesViewUpdater",
    "BattleCharacterEntityView",
    "CharacterMovementView",
    "CarEntityView",
    "Game",
    "GameModel",
    "BattleModel",
    "BattleCameraModel",
    "WorldEntitiesModel",
    "BattleCharacterModel",
    "KnockoutModel",
    "CrouchModel",
    "InVehicleModel",
    "IsHiddenModel",
    "SkeletonModel",
    "SkeletonConfig",
    "TransformModel",
    "CharacterReplicatedTransformModel",
    "LookDirectionModel",
    "BattleCharacterPrivateModel",
    "WeaponSpreadArgsModel",
    "HealthModel",
    "PlayerEntityModel",
    "PlayerClientModel",
    "RecoilModel",
    "PlayerFakeSpreadModel",
    "PlayerCameraModel",
    "CameraBrainUpdater",
    "CinemachineBrain",
    "Cinemachine3rdPersonFollow",
    "SetMovementModel",
    "CharacterRotationModel",
    "ClientCharacterSlotsModel",
    "BattleCharacterSlotModel",
    "BattleCharacterSlotData",
    "VehicleModel",
    "LocationCashLootNodeModel",
    "VehicleTransformModel",
    "CarModel",
    "HelicopterModel",
    "LootModel",
    "LootEntityInventoryModel",
    "RandomLootObjectModel",
    "StaticLootObjectModel",
    "ContainerModel",
    "GrenadeModel",
    "GrenadeTransformModel",
    "ContentContext",
]


def extract_typeinfo(script_path: Path) -> dict[str, int]:
    wanted = {f"{n}_TypeInfo" for n in TYPEINFO_NAMES}
    found: dict[str, int] = {}
    # Stream by regex; script.json is ~250MB.
    name_re = re.compile(r'"Name"\s*:\s*"([^"]+_TypeInfo)"')
    addr_re = re.compile(r'"Address"\s*:\s*(\d+)')
    with script_path.open("r", encoding="utf-8", errors="ignore") as fh:
        pending_addr: int | None = None
        pending_name: str | None = None
        for line in fh:
            if '"ScriptMetadata"' in line and found and len(found) >= len(wanted):
                # keep scanning until we have all; ScriptMetadata is after ScriptMethod
                pass
            mname = name_re.search(line)
            if mname:
                pending_name = mname.group(1)
            madd = addr_re.search(line)
            if madd:
                pending_addr = int(madd.group(1))
            if pending_name and pending_addr is not None and pending_name in wanted:
                found[pending_name] = pending_addr
                pending_name = None
                pending_addr = None
            elif pending_name and pending_addr is not None:
                pending_name = None
                pending_addr = None
            if len(found) >= len(wanted) and '"ScriptMetadata"' in line:
                # still may need later entries
                pass
    return found


def extract_methods(script_path: Path) -> dict[str, int]:
    needles = {n.lower() for n in METHOD_NAMES}
    extra = (
        "contentcontext$$get_instance",
        "baseprimitivedata",
    )
    found: dict[str, list[tuple[str, int]]] = {}
    name_re = re.compile(r'"Name"\s*:\s*"([^"]+)"')
    addr_re = re.compile(r'"Address"\s*:\s*(\d+)')
    pending_addr: int | None = None
    pending_name: str | None = None
    with script_path.open("r", encoding="utf-8", errors="ignore") as fh:
        in_methods = False
        for line in fh:
            if '"ScriptMethod"' in line:
                in_methods = True
            if '"ScriptMetadata"' in line:
                break
            if not in_methods:
                continue
            mname = name_re.search(line)
            if mname:
                pending_name = mname.group(1)
            madd = addr_re.search(line)
            if madd:
                pending_addr = int(madd.group(1))
            if pending_name and pending_addr is not None:
                low = pending_name.lower()
                keep = False
                if low in needles:
                    keep = True
                elif "contentcontext" in low and "get_instance" in low:
                    keep = True
                elif "baseprimitivedata" in low and "get_value" in low:
                    keep = True
                if keep:
                    found.setdefault(pending_name, []).append((pending_name, pending_addr))
                pending_name = None
                pending_addr = None
    # collapse
    out: dict[str, int] = {}
    for name, hits in found.items():
        out[name] = hits[0][1]
    return out


CLASS_START = re.compile(
    r"^// Namespace: (?P<ns>\S+)\s*$"
)
CLASS_LINE = re.compile(
    r"^(?:public |internal |protected |private )?(?:abstract |sealed |static )*(?:class|struct) (?P<name>\S+)"
)
FIELD_LINE = re.compile(
    r"^\s+(?:public |private |protected |internal )(?:static )?(?:readonly )?(?P<ty>.+?) (?P<field>\S+); // (?P<off>0x[0-9A-Fa-f]+)"
)


def parse_dump_cs(path: Path) -> dict[str, dict]:
    wanted = set(CLASSES)
    current_ns = ""
    current = None
    current_name = None
    brace = 0
    out: dict[str, dict] = {}
    with path.open("r", encoding="utf-8", errors="ignore") as fh:
        for line in fh:
            mns = CLASS_START.match(line)
            if mns:
                current_ns = mns.group("ns")
                continue
            if current is None:
                mcl = CLASS_LINE.match(line)
                if not mcl:
                    continue
                name = mcl.group("name").split("<")[0]
                if name not in wanted:
                    continue
                # skip compiler-generated nested
                if "`" in mcl.group("name") and name not in wanted:
                    continue
                fq = f"{current_ns}.{name}" if current_ns else name
                current_name = fq
                current = {
                    "ns": current_ns,
                    "name": name,
                    "fq": fq,
                    "fields": [],
                }
                brace = line.count("{") - line.count("}")
                continue
            brace += line.count("{") - line.count("}")
            mf = FIELD_LINE.match(line)
            if mf:
                current["fields"].append(
                    {
                        "name": mf.group("field"),
                        "type": mf.group("ty").strip(),
                        "offset": int(mf.group("off"), 16),
                    }
                )
            if brace <= 0 and current is not None:
                # keep first non-nested match; later nested <>c classes have same short name rarely
                if current_name not in out:
                    out[current_name] = current
                elif len(current["fields"]) > len(out[current_name]["fields"]):
                    out[current_name] = current
                current = None
                current_name = None
    return out


def main() -> int:
    print("Parsing dump.cs ...", flush=True)
    types = parse_dump_cs(DUMP_CS)
    print(f"Parsed {len(types)} types", flush=True)
    print("Streaming script.json TypeInfo + methods ...", flush=True)
    methods = extract_methods(SCRIPT)
    typeinfo = extract_typeinfo(SCRIPT)

    result = {
        "game_assembly_size": 147842560,
        "game_assembly_size_hex": hex(147842560),
        "typeinfo": {k: hex(v) for k, v in sorted(typeinfo.items())},
        "typeinfo_raw": typeinfo,
        "methods": {k: hex(v) for k, v in sorted(methods.items())},
        "types": {},
    }
    for fq, td in sorted(types.items()):
        result["types"][fq] = {
            "ns": td["ns"],
            "name": td["name"],
            "fields": [
                {"name": f["name"], "type": f["type"], "offset": hex(f["offset"])}
                for f in td["fields"]
            ],
        }

    out = Path(__file__).resolve().parent / "fresh_offsets.json"
    out.write_text(json.dumps(result, indent=2), encoding="utf-8")
    print(f"Wrote {out}")
    print("\n=== TypeInfo ===")
    for k, v in sorted(typeinfo.items()):
        print(f"  {k} = 0x{v:X}")
    print("\n=== Methods (subset) ===")
    for k, v in sorted(methods.items()):
        if "ContentContext" in k or "BasePrimitiveData" in k:
            print(f"  {k} = 0x{v:X}")
    print("\n=== Types captured ===")
    for fq, td in sorted(types.items()):
        print(f"  {fq}  fields={len(td['fields'])}")
    missing = [c for c in CLASSES if not any(td["name"] == c for td in types.values())]
    print("\n=== Missing class names ===")
    for m in missing:
        print(f"  {m}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
