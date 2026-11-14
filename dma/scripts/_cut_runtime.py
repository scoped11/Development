from pathlib import Path

p = Path(r"C:\Development\dma\src\game\runtime.cpp")
text = p.read_text(encoding="utf-8")
lines = text.splitlines(keepends=True)

drop = {
    "FindKlassByName",
    "FindNamedKlassNearGame",
    "ScanTypeInfoRegionForKlass",
    "FindGameViaHeap",
    "FindGameViaUnityEntry",
    "FindBattleViaHeap",
    "FindBattleViaBattleModel",
    "FindBattleViaGameModel",
    "FindCameraBrain",
    "FindCameraRig",
    "FindViewUpdater",
}

keep = []
i = 0
while i < len(lines):
    line = lines[i]
    drop_this = False
    for name in drop:
        if f"RobRunSession::{name}" in line:
            drop_this = True
            break
    if drop_this:
        i += 1
        depth = 0
        started = False
        while i < len(lines):
            depth += lines[i].count("{") - lines[i].count("}")
            if "{" in lines[i]:
                started = True
            i += 1
            if started and depth <= 0:
                break
        if i < len(lines) and lines[i].strip() == "":
            i += 1
        continue
    keep.append(line)
    i += 1

text = "".join(keep)

# Drop reconstructed-rig helpers (ESP used these instead of the shoulder lens).
start = text.find("float Clamp01(float v)")
end = text.find("} // namespace")
if start != -1 and end != -1 and start < end:
    text = text[:start] + text[end:]

p.write_text(text, encoding="utf-8")
print("cut ok", len(text.splitlines()), "lines")
