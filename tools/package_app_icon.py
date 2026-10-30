# Copyright (C) 2026 ppm203
# GNU GPL version 3 only, with section 7 additional terms.
# See LICENSE and ADDITIONAL_TERMS.md in the repository root.

"""Export committed platform icons from the 1024px PNG rendered by app_icon_renderer.

Requires Pillow; normal launcher builds use the committed files, not this script.
"""
from pathlib import Path
from PIL import Image
import subprocess
import xml.etree.ElementTree as ET

folder = Path(__file__).resolve().parents[1] / "assets" / "branding"
with Image.open(folder / "m2cl.png") as source:
    image = source.convert("RGBA")
    if image.size != (1024, 1024):
        raise ValueError("Expected the 1024px render of m2cl.svg")
    # macOS gets a platform-specific tile; Windows/Linux keep the silhouette.
    source_svg = ET.parse(folder / "m2cl.svg").getroot()
    layers = "".join(ET.tostring(child, encoding="unicode") for child in source_svg)
    mac_svg = f'''<svg xmlns="http://www.w3.org/2000/svg" width="1024" height="1024" viewBox="0 0 1024 1024">
      <defs><linearGradient id="macTile" x2="0.7" y2="1"><stop stop-color="#ffffff"/><stop offset="1" stop-color="#ffffff"/></linearGradient></defs>
      <rect x="100" y="110" width="824" height="824" rx="184" fill="#000" opacity=".12"/>
      <rect x="100" y="100" width="824" height="824" rx="184" fill="url(#macTile)"/>
      <rect x="101" y="101" width="822" height="822" rx="183" fill="none" stroke="#dce2dc" stroke-opacity=".7" stroke-width="2"/>
      <g transform="translate(145 145) scale(.565)">{layers}</g>
    </svg>'''
    (folder / "m2cl-macos.svg").write_text(mac_svg)
    subprocess.run([str(folder.parents[1] / "build/app_icon_renderer"), str(folder / "m2cl-macos.svg"), str(folder / "m2cl-macos.png")], check=True)
    with Image.open(folder / "m2cl-macos.png") as mac_image:
        mac_image.save(folder / "m2cl.icns", format="ICNS")
    image.save(folder / "m2cl.ico", format="ICO", sizes=[(n, n) for n in (16, 20, 24, 32, 40, 48, 64, 128, 256)])
    for size in (16, 32, 48, 64, 128, 256, 512):
        image.resize((size, size), Image.Resampling.LANCZOS).save(folder / f"m2cl-{size}.png")
