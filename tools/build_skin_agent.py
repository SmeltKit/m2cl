# Copyright (C) 2026 ppm203; GPL-3.0-only with ADDITIONAL_TERMS.md.
"""Build the small Java-8-compatible skin bootstrap; no third-party binary bundled."""
import argparse, pathlib, subprocess, tempfile, zipfile
root=pathlib.Path(__file__).resolve().parent.parent
parser=argparse.ArgumentParser();parser.add_argument('--javac',default='javac');args=parser.parse_args()
with tempfile.TemporaryDirectory() as output:
    subprocess.run([args.javac,'--release','8','-encoding','UTF-8','-d',output,str(root/'tools/skin-agent/SkinAgent.java')],check=True)
    dest=root/'resources/skin-agent/m2cl-skin-agent.jar';dest.parent.mkdir(parents=True,exist_ok=True)
    with zipfile.ZipFile(dest,'w',compression=zipfile.ZIP_DEFLATED) as jar:
        files={'META-INF/MANIFEST.MF':(root/'tools/skin-agent/MANIFEST.MF').read_bytes(),'META-INF/M2CL-LICENSE.txt':(root/'LICENSE').read_bytes(),'META-INF/M2CL-ADDITIONAL-TERMS.md':(root/'ADDITIONAL_TERMS.md').read_bytes(),'META-INF/SkinAgent.java':(root/'tools/skin-agent/SkinAgent.java').read_bytes()}
        files.update({p.relative_to(output).as_posix():p.read_bytes() for p in pathlib.Path(output).rglob('*.class')})
        for name,data in sorted(files.items()):
            info=zipfile.ZipInfo(name,(2026,1,1,0,0,0));info.compress_type=zipfile.ZIP_DEFLATED;jar.writestr(info,data)
print(dest)
