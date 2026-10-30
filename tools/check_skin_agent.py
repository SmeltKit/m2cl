# Copyright (C) 2026 ppm203; GPL-3.0-only with ADDITIONAL_TERMS.md.
"""Verify the checked-in bootstrap matches its source, notices and Java target."""
from pathlib import Path
import struct
import zipfile

root = Path(__file__).resolve().parent.parent
with zipfile.ZipFile(root / 'resources/skin-agent/m2cl-skin-agent.jar') as archive:
    for embedded, source in {
        'META-INF/SkinAgent.java': 'tools/skin-agent/SkinAgent.java',
        'META-INF/M2CL-LICENSE.txt': 'LICENSE',
        'META-INF/M2CL-ADDITIONAL-TERMS.md': 'ADDITIONAL_TERMS.md',
        'META-INF/MANIFEST.MF': 'tools/skin-agent/MANIFEST.MF',
    }.items():
        assert archive.read(embedded) == (root / source).read_bytes(), f'Rebuild skin agent after changing {source}'
    classes = [name for name in archive.namelist() if name.endswith('.class')]
    assert classes and all(name.startswith('app/m2cl/skin/') for name in classes)
    for name in classes:
        header = archive.read(name)[:8]
        assert header[:4] == b'\xca\xfe\xba\xbe' and struct.unpack('>H', header[6:8])[0] == 52
print('Skin bootstrap source, license notices and Java 8 bytecode verified')
