from pathlib import Path
import re

dump = Path(r"C:\Development\dumps\2026-11-14\dump.cs")
text = dump.read_text(encoding="utf-8", errors="replace")

want = [
    "InventoryItemType",
    "ContainerTypeDescription",
    "ContainerModel",
    "ContainerEntityInventoryModel",
    "PlotModel",
    "HideObjectModel",
    "PlayerObserverModel",
    "SecurityAlarmModel",
    "SafeDoorModel",
    "CargoModel",
    "GrenadeModel",
    "GrenadeDescription",
    "LocationCashWithdrawalDescription",
    "InventoryItemViewDescription",
    "PlotContainerModel",
    "ShelvingModel",
    "StorageBoxModel",
    "LootNodeModel",
    "FurnitureLootModel",
    "AccessEntityModel",
    "BreachEntityModel",
    "DefenseBlockPublicModel",
    "UnlockEntityModel",
    "DoorModel",
    "SecurityPanelModel",
    "LocationSecurityAlarmDescription",
    "PlayerObserverDescription",
    "BattleCharacterConfig",
    "GenderDescription",
    "OutfitDescription",
    "CharacterAppearanceModel",
    "AppearanceModel",
    "Gender",
    "PlayerRoleDescription",
    "CharacterBagDescription",
    "LootableObjectGroupModel",
    "CumulativeChargerModel",
    "PointOfInterestModel",
    "Decoder",
    "Fuse",
    "RaidLock",
    "EnumItem",
    "MyEnumItem",
]

# Find class/enum blocks by name
pat = re.compile(
    r'^(public |private |internal |protected )?(abstract |sealed |static )?(class|enum|struct) ([A-Za-z0-9_.<>, ]+)',
    re.M,
)

hits = {}
for m in pat.finditer(text):
    kind = m.group(3)
    name = m.group(4).split(":")[0].split("<")[0].strip()
    if name in want and name not in hits:
        hits[name] = (kind, m.start())

out = []
for name in want:
    if name not in hits:
        out.append(f"\n===== MISSING {name} =====\n")
        continue
    kind, start = hits[name]
    chunk = text[start:start + 4500]
    # clip at next top-level type after a blank-ish end
    out.append(f"\n===== {kind} {name} @ {start} =====\n")
    out.append(chunk.split("\n// Namespace:")[0][:4000])

# extra: search gender/outfit/police/decoder/fuse in field names
keys = ["Gender", "Outfit", "Police", "Uniform", "Decoder", "Fuse", "RaidLock", "IsHidden", "HideObject"]
for key in keys:
    found = []
    for m in re.finditer(rf'.{{0,80}}{key}.{{0,80}}', text):
        line = m.group(0).replace("\n", " ")
        if "0x" in line or "enum" in line.lower() or "class" in line.lower():
            found.append(line[:160])
        if len(found) >= 8:
            break
    out.append(f"\n===== SEARCH {key} =====\n")
    out.extend(found)
    out.append("\n")

Path(r"C:\Development\dma\scripts\_types_extract.txt").write_text(
    "\n".join(out), encoding="utf-8"
)
print("wrote", len(hits), "types")
