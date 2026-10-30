# Copyright (C) 2026 ppm203
# GNU GPL version 3 only, with section 7 additional terms.
# See LICENSE and ADDITIONAL_TERMS.md in the repository root.

"""Render the real Qt UI in isolated offscreen processes for visual review."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--binary", type=Path, default=Path("build/m2cl.app/Contents/MacOS/m2cl"))
parser.add_argument("--fixture", type=Path, default=Path("artifacts/ui-preview-v2"))
parser.add_argument("--output", type=Path, default=Path("artifacts/ui-v2"))
args = parser.parse_args()
binary = args.binary.resolve()
fixture = args.fixture.resolve()
output = args.output.resolve()
output.mkdir(parents=True, exist_ok=True)
if not binary.is_file() or not (fixture / "settings.json").is_file():
    parser.error("Build the app and create a preview fixture first")

variants = [
    ("home-light", ["--appearance", "light"], "1"),
    ("home-dark", ["--appearance", "dark"], "1"),
    ("hero-detail", ["--appearance", "light", "--screenshot-widget", "heroCover"], "1"),
    ("home-compact", ["--appearance", "light", "--window-size", "840x620"], "1"),
    ("home-fractional", ["--appearance", "light"], "1.25"),
    ("home-wide", ["--appearance", "light", "--window-size", "1440x900"], "2"),
    ("home-hidpi", ["--appearance", "light"], "2"),
    ("library", ["--appearance", "light", "--page", "library"], "1"),
    ("settings", ["--appearance", "light", "--page", "settings"], "1"),
    ("instance", ["--appearance", "light", "--instance-settings"], "1"),
    ("downloads", ["--appearance", "light", "--downloads"], "1"),
]
for name, extra, scale in variants:
    environment = dict(os.environ, QT_QPA_PLATFORM="offscreen", QT_SCALE_FACTOR=scale)
    subprocess.run([str(binary), "--smoke-test", "--data-dir", str(fixture), "--theme", "forest", *extra,
                    "--screenshot", str(output / f"{name}.png")], env=environment, check=True, timeout=20)
    print(f"Rendered {name}", flush=True)

with tempfile.TemporaryDirectory(prefix="empty-preview-", dir=output) as temporary:
    directory = Path(temporary)
    root = directory / "empty-minecraft"
    (root / "versions").mkdir(parents=True)
    (directory / "settings.json").write_text(json.dumps({"gameRoots": [str(root)]}), encoding="utf-8")
    subprocess.run([str(binary), "--smoke-test", "--data-dir", str(directory), "--appearance", "light",
                    "--screenshot", str(output / "home-empty.png")],
                   env=dict(os.environ, QT_QPA_PLATFORM="offscreen", QT_SCALE_FACTOR="1"), check=True, timeout=20)
    print("Rendered home-empty", flush=True)
