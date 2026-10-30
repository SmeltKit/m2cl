# Copyright (C) 2026 ppm203
# GNU GPL version 3 only, with section 7 additional terms.
# See LICENSE and ADDITIONAL_TERMS.md in the repository root.

"""Create isolated, metadata-only instances for UI screenshots. Never used by the app at startup."""
import json
from pathlib import Path
import argparse

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("destination", type=Path)
args = parser.parse_args()
destination = args.destination.resolve()
if destination.exists():
    parser.error("Destination already exists; choose a new preview directory")
destination.mkdir(parents=True)
root = destination / "test-minecraft"
examples = [
    ("01-survival", "1.20.1", "樱谷生存", "net.minecraftforge:forge:1.20.1-47.2.0", 17),
    ("02-caves", "1.18.2", "雪山探险", "net.fabricmc:fabric-loader:0.16.0", 17),
    ("03-classic", "1.7.10", "经典工业", "net.minecraftforge:forge:1.7.10-10.13.4", 8),
    ("04-ocean", "1.8.9", "海底世界", "", 8),
]
names = {}
paths = []
for identifier, version, name, library, java in examples:
    folder = root / "versions" / identifier
    (folder / "mods").mkdir(parents=True)
    (folder / "logs").mkdir()
    data = {"id": version, "javaVersion": {"majorVersion": java}, "libraries": [{"name": library}] if library else []}
    (folder / f"{identifier}.json").write_text(json.dumps(data), encoding="utf-8")
    paths.append(str(folder))
    names[str(folder)] = name
settings = {"schemaVersion": 1, "gameRoots": [str(root)], "selectedInstance": paths[0], "appearance": "light", "theme": "forest", "favoriteInstances": [paths[0]], "instanceNames": names}
(destination / "settings.json").write_text(json.dumps(settings, ensure_ascii=False, indent=2), encoding="utf-8")
print(destination)
